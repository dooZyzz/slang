#include "runtime/modules/loader/module_loader.h"
#include "runtime/modules/extensions/module_hooks.h"
#include "runtime/modules/loader/module_cache.h"
#include "runtime/core/vm.h"
#include "utils/platform_threads.h"
#include "utils/platform_dynlib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef _WIN32
#include <unistd.h>
#endif

/**
 * Module unloading support with proper cleanup and hook execution.
 */

// Unload a module and free its resources
void module_unload(Module* module, VM* vm) {
    if (!module) return;
    
    // Execute unload hooks before cleanup
    if (module->state == MODULE_STATE_LOADED) {
        module_execute_unload_hooks(module, vm);
    }
    
    // Mark as unloading to prevent concurrent access
    module->state = MODULE_STATE_ERROR;
    
    // Free module resources
    if (module->path) {
        free((char*)module->path);
        module->path = NULL;
    }
    
    if (module->absolute_path) {
        free((char*)module->absolute_path);
        module->absolute_path = NULL;
    }
    
    if (module->version) {
        free((char*)module->version);
        module->version = NULL;
    }
    
    // Free exports
    if (module->exports.names) {
        for (size_t i = 0; i < module->exports.count; i++) {
            free(module->exports.names[i]);
        }
        free(module->exports.names);
        free(module->exports.values);
        free(module->exports.visibility);
    }
    
    // Free globals
    if (module->globals.names) {
        for (size_t i = 0; i < module->globals.count; i++) {
            free(module->globals.names[i]);
        }
        free(module->globals.names);
        free(module->globals.values);
    }
    
    // Free module scope
    if (module->scope) {
        module_scope_destroy(module->scope);
        module->scope = NULL;
    }
    
    // Free lazy loading chunk
    if (module->chunk) {
        chunk_free(module->chunk);
        free(module->chunk);
        module->chunk = NULL;
    }
    
    // Handle native module cleanup
    if (module->is_native && module->native_handle) {
        platform_dynlib_close(module->native_handle);
        module->native_handle = NULL;
        
        // Remove temporary native library file
        if (module->temp_native_path) {
            unlink(module->temp_native_path);
            free(module->temp_native_path);
            module->temp_native_path = NULL;
        }
    }
    
    // Note: module_object is managed by the GC
    module->module_object = NULL;
    
    // Destroy the reference counting mutex
    platform_mutex_destroy(&module->ref_mutex);
    
    // Finally free the module itself
    free(module);
}

// Unload a module by name from the loader
bool module_loader_unload(ModuleLoader* loader, const char* module_name) {
    if (!loader || !module_name) return false;
    
    // Get the module from cache
    Module* module = module_cache_get(loader->cache, module_name);
    if (!module) {
        return false;
    }
    
    // Remove from cache
    module_cache_remove(loader->cache, module_name);
    
    // Unload the module
    module_unload(module, loader->vm);
    return true;
}

// Helper struct for unload_all operation
typedef struct {
    ModuleLoader* loader;
    Module** modules_to_unload;
    size_t count;
    size_t capacity;
} UnloadContext;

// Callback for collecting modules to unload
static void collect_modules_callback(const char* name, Module* module, void* user_data) {
    UnloadContext* ctx = (UnloadContext*)user_data;
    
    // Grow array if needed
    if (ctx->count >= ctx->capacity) {
        ctx->capacity = ctx->capacity == 0 ? 16 : ctx->capacity * 2;
        ctx->modules_to_unload = realloc(ctx->modules_to_unload, 
                                       ctx->capacity * sizeof(Module*));
    }
    
    // Store module reference
    ctx->modules_to_unload[ctx->count++] = module;
}

// Unload all modules from a loader
void module_loader_unload_all(ModuleLoader* loader) {
    if (!loader) return;
    
    // Collect all modules first to avoid modifying cache during iteration
    UnloadContext ctx = {
        .loader = loader,
        .modules_to_unload = NULL,
        .count = 0,
        .capacity = 0
    };
    
    // Iterate and collect modules
    module_cache_iterate(loader->cache, collect_modules_callback, &ctx);
    
    // Now unload each module
    for (size_t i = 0; i < ctx.count; i++) {
        Module* module = ctx.modules_to_unload[i];
        if (module && module->path) {
            // Remove from cache first
            module_cache_remove(loader->cache, module->path);
            // Then unload
            module_unload(module, loader->vm);
        }
    }
    
    // Clean up
    if (ctx.modules_to_unload) {
        free(ctx.modules_to_unload);
    }
}

// Check if a module can be safely unloaded
bool module_can_unload(Module* module) {
    if (!module) return false;
    
    // Can't unload if in error state or currently loading
    if (module->state == MODULE_STATE_ERROR || 
        module->state == MODULE_STATE_LOADING) {
        return false;
    }
    
    // Check reference count
    platform_mutex_lock(&module->ref_mutex);
    bool can_unload = (module->ref_count == 0);
    platform_mutex_unlock(&module->ref_mutex);
    
    return can_unload;
}

// Force unload a module (even with active references)
void module_force_unload(Module* module, VM* vm) {
    if (!module) return;
    
    fprintf(stderr, "Warning: Force unloading module '%s'\n", module->path);
    module_unload(module, vm);
}

// Reference counting implementation
void module_ref(Module* module) {
    if (!module) return;
    
    platform_mutex_lock(&module->ref_mutex);
    module->ref_count++;
    platform_mutex_unlock(&module->ref_mutex);
}

void module_unref(Module* module) {
    if (!module) return;
    
    platform_mutex_lock(&module->ref_mutex);
    if (module->ref_count > 0) {
        module->ref_count--;
    }
    platform_mutex_unlock(&module->ref_mutex);
}

int module_get_ref_count(Module* module) {
    if (!module) return 0;
    
    platform_mutex_lock(&module->ref_mutex);
    int count = module->ref_count;
    platform_mutex_unlock(&module->ref_mutex);
    
    return count;
}