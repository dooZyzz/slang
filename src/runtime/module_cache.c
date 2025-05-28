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
 */

typedef struct ModuleCache {
    HashMap* modules;           // Path -> Module mapping
    pthread_rwlock_t lock;      // Read-write lock for thread safety
    size_t hit_count;          // Cache statistics
    size_t miss_count;
    size_t eviction_count;
} ModuleCache;

// Global module cache
static ModuleCache* g_module_cache = NULL;

// Initialize the global module cache
void module_cache_init(void) {
    if (g_module_cache) return;
    
    g_module_cache = malloc(sizeof(ModuleCache));
    g_module_cache->modules = hash_map_create();
    pthread_rwlock_init(&g_module_cache->lock, NULL);
    g_module_cache->hit_count = 0;
    g_module_cache->miss_count = 0;
    g_module_cache->eviction_count = 0;
}

// Destroy the module cache
void module_cache_destroy(void) {
    if (!g_module_cache) return;
    
    pthread_rwlock_wrlock(&g_module_cache->lock);
    
    // Free all cached modules
    // Note: modules themselves are freed by module loader
    
    hash_map_destroy(g_module_cache->modules);
    pthread_rwlock_unlock(&g_module_cache->lock);
    pthread_rwlock_destroy(&g_module_cache->lock);
    
    free(g_module_cache);
    g_module_cache = NULL;
}

// Cache a module
void module_cache_put(const char* path, Module* module) {
    if (!g_module_cache || !path || !module) return;
    
    pthread_rwlock_wrlock(&g_module_cache->lock);
    hash_map_put(g_module_cache->modules, path, module);
    pthread_rwlock_unlock(&g_module_cache->lock);
}

// Get a module from cache
Module* module_cache_get(const char* path) {
    if (!g_module_cache || !path) return NULL;
    
    pthread_rwlock_rdlock(&g_module_cache->lock);
    Module* module = (Module*)hash_map_get(g_module_cache->modules, path);
    
    if (module) {
        g_module_cache->hit_count++;
    } else {
        g_module_cache->miss_count++;
    }
    
    pthread_rwlock_unlock(&g_module_cache->lock);
    return module;
}

// Remove a module from cache
void module_cache_remove(const char* path) {
    if (!g_module_cache || !path) return;
    
    pthread_rwlock_wrlock(&g_module_cache->lock);
    hash_map_remove(g_module_cache->modules, path);
    g_module_cache->eviction_count++;
    pthread_rwlock_unlock(&g_module_cache->lock);
}

// Get cache statistics
void module_cache_stats(size_t* hits, size_t* misses, size_t* evictions, size_t* size) {
    if (!g_module_cache) {
        if (hits) *hits = 0;
        if (misses) *misses = 0;
        if (evictions) *evictions = 0;
        if (size) *size = 0;
        return;
    }
    
    pthread_rwlock_rdlock(&g_module_cache->lock);
    if (hits) *hits = g_module_cache->hit_count;
    if (misses) *misses = g_module_cache->miss_count;
    if (evictions) *evictions = g_module_cache->eviction_count;
    if (size) *size = hash_map_size(g_module_cache->modules);
    pthread_rwlock_unlock(&g_module_cache->lock);
}

// Module path normalization for consistent caching
char* module_normalize_path(const char* path) {
    if (!path) return NULL;
    
    // Convert relative paths to absolute
    char* abs_path = realpath(path, NULL);
    if (abs_path) return abs_path;
    
    // If realpath fails, just duplicate the path
    return strdup(path);
}

// Batch module loading optimization
typedef struct {
    const char** paths;
    Module** modules;
    size_t count;
    ModuleLoader* loader;
} BatchLoadContext;

void module_load_batch(ModuleLoader* loader, const char** paths, Module** modules, size_t count) {
    if (!loader || !paths || !modules || count == 0) return;
    
    // First pass: check cache for all modules
    size_t uncached_count = 0;
    for (size_t i = 0; i < count; i++) {
        char* norm_path = module_normalize_path(paths[i]);
        modules[i] = module_cache_get(norm_path);
        if (!modules[i]) {
            uncached_count++;
        }
        free(norm_path);
    }
    
    // Second pass: load uncached modules
    if (uncached_count > 0) {
        // Could potentially parallelize this
        for (size_t i = 0; i < count; i++) {
            if (!modules[i]) {
                modules[i] = module_load(loader, paths[i], false);
            }
        }
    }
}

// Preload commonly used modules
void module_cache_preload(ModuleLoader* loader, const char** common_modules, size_t count) {
    if (!loader || !common_modules || count == 0) return;
    
    Module** modules = malloc(count * sizeof(Module*));
    module_load_batch(loader, common_modules, modules, count);
    free(modules);
}

// Memory pressure handling
void module_cache_trim(size_t max_size) {
    if (!g_module_cache) return;
    
    pthread_rwlock_wrlock(&g_module_cache->lock);
    
    size_t current_size = hash_map_size(g_module_cache->modules);
    if (current_size <= max_size) {
        pthread_rwlock_unlock(&g_module_cache->lock);
        return;
    }
    
    // Simple eviction - in real implementation would use LRU
    // For now, this is a placeholder
    size_t to_remove = current_size - max_size;
    // Note: proper implementation would track access times
    
    pthread_rwlock_unlock(&g_module_cache->lock);
}