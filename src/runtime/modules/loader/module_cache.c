#include "runtime/modules/loader/module_cache.h"
#include "runtime/modules/loader/module_loader.h"
#include "utils/hash_map.h"
#include "utils/allocators.h"
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "runtime/modules/module_allocator_macros.h"

/**
 * Thread-safe module cache with optimized lookup.
 * Uses a hash map for O(1) module lookups and implements
 * read-write locking for concurrent access.
 * This is an internal implementation used by ModuleLoader.
 */

typedef struct ModuleCache
{
    HashMap* modules; // Path -> Module mapping
    pthread_rwlock_t lock; // Read-write lock for thread safety
    size_t hit_count; // Cache statistics
    size_t miss_count;
    size_t eviction_count;
} ModuleCache;

// Create a new module cache
ModuleCache* module_cache_create(void)
{
    ModuleCache* cache = MODULES_NEW(ModuleCache);
    if (!cache) return NULL;

    // Create hash map with module allocator
    if (!cache->modules)
    {
        MODULES_FREE(cache, sizeof(ModuleCache));
        return NULL;
    }

    pthread_rwlock_init(&cache->lock, NULL);
    cache->hit_count = 0;
    cache->miss_count = 0;
    cache->eviction_count = 0;
    return cache;
}

// Destroy the module cache
void module_cache_destroy(ModuleCache* cache)
{
    if (!cache) return;

    pthread_rwlock_wrlock(&cache->lock);

    // Note: modules themselves are owned by module loader
    hash_map_destroy(cache->modules);

    pthread_rwlock_unlock(&cache->lock);
    pthread_rwlock_destroy(&cache->lock);

    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    MEM_FREE(alloc, cache, sizeof(ModuleCache));
}

// Cache a module
void module_cache_put(ModuleCache* cache, const char* path, Module* module)
{
    if (!cache || !path || !module) return;

    pthread_rwlock_wrlock(&cache->lock);
    hash_map_put(cache->modules, path, module);
    pthread_rwlock_unlock(&cache->lock);
}

// Get a module from cache
Module* module_cache_get(ModuleCache* cache, const char* path)
{
    if (!cache || !path) return NULL;

    pthread_rwlock_rdlock(&cache->lock);
    Module* module = (Module*)hash_map_get(cache->modules, path);

    if (module)
    {
        cache->hit_count++;
        // Update access time for LRU tracking
        module->last_access_time = time(NULL);
    }
    else
    {
        cache->miss_count++;
    }

    pthread_rwlock_unlock(&cache->lock);
    return module;
}

// Remove a module from cache
void module_cache_remove(ModuleCache* cache, const char* path)
{
    if (!cache || !path) return;

    pthread_rwlock_wrlock(&cache->lock);
    hash_map_remove(cache->modules, path);
    cache->eviction_count++;
    pthread_rwlock_unlock(&cache->lock);
}

// Clear all modules from cache
void module_cache_clear(ModuleCache* cache)
{
    if (!cache) return;

    pthread_rwlock_wrlock(&cache->lock);
    hash_map_clear(cache->modules);
    pthread_rwlock_unlock(&cache->lock);
}

// Get cache statistics
void module_cache_get_stats(ModuleCache* cache, ModuleCacheStats* stats)
{
    if (!cache || !stats) return;

    pthread_rwlock_rdlock(&cache->lock);
    stats->hit_count = cache->hit_count;
    stats->miss_count = cache->miss_count;
    stats->eviction_count = cache->eviction_count;
    stats->size = hash_map_size(cache->modules);
    pthread_rwlock_unlock(&cache->lock);
}

// Preload modules into cache
void module_cache_preload(ModuleCache* cache, Module** modules, size_t count)
{
    if (!cache || !modules || count == 0) return;

    pthread_rwlock_wrlock(&cache->lock);
    for (size_t i = 0; i < count; i++)
    {
        if (modules[i] && modules[i]->path)
        {
            hash_map_put(cache->modules, modules[i]->path, modules[i]);
        }
    }
    pthread_rwlock_unlock(&cache->lock);
}

// Helper struct for LRU tracking
typedef struct
{
    const char* path;
    Module* module;
    time_t access_time;
} ModuleAccessInfo;

// Comparison function for qsort (oldest first)
static int compare_access_times(const void* a, const void* b)
{
    const ModuleAccessInfo* ma = (const ModuleAccessInfo*)a;
    const ModuleAccessInfo* mb = (const ModuleAccessInfo*)b;
    return (ma->access_time > mb->access_time) - (ma->access_time < mb->access_time);
}

// Collector context for LRU eviction
typedef struct
{
    ModuleAccessInfo* modules;
    size_t count;
    size_t capacity;
} LRUCollectorContext;

// Callback to collect modules for LRU analysis
static void collect_for_lru(const char* key, void* value, void* user_data)
{
    LRUCollectorContext* ctx = (LRUCollectorContext*)user_data;
    Module* module = (Module*)value;

    if (ctx->count >= ctx->capacity)
    {
        // Realloc with allocator
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
        size_t old_size = ctx->capacity * sizeof(ModuleAccessInfo);
        ctx->capacity *= 2;
        size_t new_size = ctx->capacity * sizeof(ModuleAccessInfo);

        ModuleAccessInfo* new_modules = MEM_ALLOC(alloc, new_size);
        if (new_modules)
        {
            memcpy(new_modules, ctx->modules, old_size);
            MEM_FREE(alloc, ctx->modules, old_size);
            ctx->modules = new_modules;
        }
    }

    ctx->modules[ctx->count].path = key;
    ctx->modules[ctx->count].module = module;
    ctx->modules[ctx->count].access_time = module->last_access_time;
    ctx->count++;
}

// Memory pressure handling with LRU eviction
void module_cache_trim(ModuleCache* cache, size_t max_size)
{
    if (!cache) return;

    pthread_rwlock_wrlock(&cache->lock);

    size_t current_size = hash_map_size(cache->modules);
    if (current_size <= max_size)
    {
        pthread_rwlock_unlock(&cache->lock);
        return;
    }

    // Collect all modules with their access times
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    LRUCollectorContext ctx = {
        .modules = MEM_ALLOC(alloc, current_size * sizeof(ModuleAccessInfo)),
        .count = 0,
        .capacity = current_size
    };

    if (!ctx.modules)
    {
        pthread_rwlock_unlock(&cache->lock);
        return;
    }

    hash_map_iterate(cache->modules, collect_for_lru, &ctx);

    // Sort modules by access time (oldest first)
    qsort(ctx.modules, ctx.count, sizeof(ModuleAccessInfo), compare_access_times);

    // Evict oldest modules until we're under the limit
    size_t modules_to_evict = current_size - max_size;
    for (size_t i = 0; i < modules_to_evict && i < ctx.count; i++)
    {
        Module* module = ctx.modules[i].module;

        // Only evict if no active references
        if (module_get_ref_count(module) == 0)
        {
            hash_map_remove(cache->modules, ctx.modules[i].path);
            cache->eviction_count++;
            module_unload(module, NULL);
        }
    }

    MEM_FREE(alloc, ctx.modules, ctx.capacity * sizeof(ModuleAccessInfo));
    pthread_rwlock_unlock(&cache->lock);
}

// Iterator callback context
typedef struct
{
    ModuleCacheIterator iterator;
    void* user_data;
} IteratorContext;

// Hash map iterator callback wrapper
static void hash_map_iterator_callback(const char* key, void* value, void* user_data)
{
    IteratorContext* ctx = (IteratorContext*)user_data;
    ctx->iterator(key, (Module*)value, ctx->user_data);
}

// Iterate over all cached modules
void module_cache_iterate(ModuleCache* cache, ModuleCacheIterator iterator, void* user_data)
{
    if (!cache || !iterator) return;

    pthread_rwlock_rdlock(&cache->lock);

    IteratorContext ctx = {
        .iterator = iterator,
        .user_data = user_data
    };

    hash_map_iterate(cache->modules, hash_map_iterator_callback, &ctx);

    pthread_rwlock_unlock(&cache->lock);
}
