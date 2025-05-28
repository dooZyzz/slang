#ifndef LANG_MODULE_CACHE_H
#define LANG_MODULE_CACHE_H

#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct Module Module;
typedef struct ModuleCache ModuleCache;

// Module cache for runtime module loading
// This is an internal API used by ModuleLoader

// Create and destroy module cache
ModuleCache* module_cache_create(void);
void module_cache_destroy(ModuleCache* cache);

// Cache operations
void module_cache_put(ModuleCache* cache, const char* path, Module* module);
Module* module_cache_get(ModuleCache* cache, const char* path);
void module_cache_remove(ModuleCache* cache, const char* path);
void module_cache_clear(ModuleCache* cache);

// Cache statistics
typedef struct {
    size_t hit_count;
    size_t miss_count;
    size_t eviction_count;
    size_t size;
} ModuleCacheStats;

void module_cache_get_stats(ModuleCache* cache, ModuleCacheStats* stats);

// Memory management
void module_cache_trim(ModuleCache* cache, size_t max_size);

// Batch operations
void module_cache_preload(ModuleCache* cache, Module** modules, size_t count);

#endif // LANG_MODULE_CACHE_H