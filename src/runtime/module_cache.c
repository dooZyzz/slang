#include "runtime/module_cache.h"
#include "runtime/module.h"
#include "utils/hash_map.h"
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

/**
 * Thread-safe module cache with optimized lookup.
 * Uses a hash map for O(1) module lookups and implements
 * read-write locking for concurrent access.
 * This is an internal implementation used by ModuleLoader.
 */

typedef struct ModuleCache {
    HashMap* modules;           // Path -> Module mapping
    pthread_rwlock_t lock;      // Read-write lock for thread safety
    size_t hit_count;          // Cache statistics
    size_t miss_count;
    size_t eviction_count;
} ModuleCache;

// Create a new module cache
ModuleCache* module_cache_create(void) {
    ModuleCache* cache = malloc(sizeof(ModuleCache));
    cache->modules = hash_map_create();
    pthread_rwlock_init(&cache->lock, NULL);
    cache->hit_count = 0;
    cache->miss_count = 0;
    cache->eviction_count = 0;
    return cache;
}

// Destroy the module cache
void module_cache_destroy(ModuleCache* cache) {
    if (!cache) return;
    
    pthread_rwlock_wrlock(&cache->lock);
    
    // Note: modules themselves are owned by module loader
    hash_map_destroy(cache->modules);
    
    pthread_rwlock_unlock(&cache->lock);
    pthread_rwlock_destroy(&cache->lock);
    
    free(cache);
}

// Cache a module
void module_cache_put(ModuleCache* cache, const char* path, Module* module) {
    if (!cache || !path || !module) return;
    
    pthread_rwlock_wrlock(&cache->lock);
    hash_map_put(cache->modules, path, module);
    pthread_rwlock_unlock(&cache->lock);
}

// Get a module from cache
Module* module_cache_get(ModuleCache* cache, const char* path) {
    if (!cache || !path) return NULL;
    
    pthread_rwlock_rdlock(&cache->lock);
    Module* module = (Module*)hash_map_get(cache->modules, path);
    
    if (module) {
        cache->hit_count++;
    } else {
        cache->miss_count++;
    }
    
    pthread_rwlock_unlock(&cache->lock);
    return module;
}

// Remove a module from cache
void module_cache_remove(ModuleCache* cache, const char* path) {
    if (!cache || !path) return;
    
    pthread_rwlock_wrlock(&cache->lock);
    hash_map_remove(cache->modules, path);
    cache->eviction_count++;
    pthread_rwlock_unlock(&cache->lock);
}

// Clear all modules from cache
void module_cache_clear(ModuleCache* cache) {
    if (!cache) return;
    
    pthread_rwlock_wrlock(&cache->lock);
    hash_map_clear(cache->modules);
    pthread_rwlock_unlock(&cache->lock);
}

// Get cache statistics
void module_cache_get_stats(ModuleCache* cache, ModuleCacheStats* stats) {
    if (!cache || !stats) return;
    
    pthread_rwlock_rdlock(&cache->lock);
    stats->hit_count = cache->hit_count;
    stats->miss_count = cache->miss_count;
    stats->eviction_count = cache->eviction_count;
    stats->size = hash_map_size(cache->modules);
    pthread_rwlock_unlock(&cache->lock);
}

// Preload modules into cache
void module_cache_preload(ModuleCache* cache, Module** modules, size_t count) {
    if (!cache || !modules || count == 0) return;
    
    pthread_rwlock_wrlock(&cache->lock);
    for (size_t i = 0; i < count; i++) {
        if (modules[i] && modules[i]->path) {
            hash_map_put(cache->modules, modules[i]->path, modules[i]);
        }
    }
    pthread_rwlock_unlock(&cache->lock);
}

// Memory pressure handling
void module_cache_trim(ModuleCache* cache, size_t max_size) {
    if (!cache) return;
    
    pthread_rwlock_wrlock(&cache->lock);
    
    size_t current_size = hash_map_size(cache->modules);
    if (current_size <= max_size) {
        pthread_rwlock_unlock(&cache->lock);
        return;
    }
    
    // TODO: Implement LRU eviction
    // For now, this is a placeholder
    size_t to_remove = current_size - max_size;
    cache->eviction_count += to_remove;
    
    pthread_rwlock_unlock(&cache->lock);
}