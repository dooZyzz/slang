#ifndef LANG_MODULE_CACHE_H
#define LANG_MODULE_CACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

// Global module cache for compiled modules
typedef struct ModuleCache ModuleCache;

typedef struct {
    const char* module_name;      // Module name
    const char* module_path;      // Source path  
    const char* compiled_path;    // Compiled module path
    time_t compile_time;          // When it was compiled
    bool is_native;              // Native vs Swift module
} CachedModule;

// Create and destroy the global module cache
ModuleCache* module_cache_create(const char* cache_dir);
void module_cache_destroy(ModuleCache* cache);

// Get the global module cache singleton
ModuleCache* module_cache_get_global(void);
void module_cache_set_global(ModuleCache* cache);

// Check if a module needs recompilation
bool module_cache_needs_rebuild(ModuleCache* cache, 
                               const char* module_name,
                               const char** source_files,
                               size_t source_count);

// Get compiled module path from cache
char* module_cache_get_compiled_path(ModuleCache* cache,
                                    const char* module_name,
                                    bool is_native);

// Register a compiled module
void module_cache_register(ModuleCache* cache,
                          const char* module_name,
                          const char* compiled_path,
                          bool is_native);

// Clean up old cached modules
void module_cache_cleanup(ModuleCache* cache, int max_age_days);

// Get cache directory
const char* module_cache_get_dir(ModuleCache* cache);

#endif // LANG_MODULE_CACHE_H