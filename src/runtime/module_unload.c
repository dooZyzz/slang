#include "runtime/module.h"
#include "runtime/module_hooks.h"
#include "vm/vm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>

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
        dlclose(module->native_handle);
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
    
    // Finally free the module itself
    free(module);
}

// Unload a module by name from the loader
bool module_loader_unload(ModuleLoader* loader, const char* module_name) {
    if (!loader || !module_name) return false;
    
    // Find the module in cache
    Module* module = NULL;
    for (size_t i = 0; i < loader->cache.count; i++) {
        if (strcmp(loader->cache.modules[i]->path, module_name) == 0) {
            module = loader->cache.modules[i];
            
            // Remove from cache
            if (i < loader->cache.count - 1) {
                memmove(&loader->cache.modules[i], 
                       &loader->cache.modules[i + 1],
                       (loader->cache.count - i - 1) * sizeof(Module*));
            }
            loader->cache.count--;
            break;
        }
    }
    
    if (!module) {
        return false;
    }
    
    // Unload the module
    module_unload(module, loader->vm);
    return true;
}

// Unload all modules from a loader
void module_loader_unload_all(ModuleLoader* loader) {
    if (!loader) return;
    
    // Unload in reverse order (dependencies first)
    while (loader->cache.count > 0) {
        Module* module = loader->cache.modules[loader->cache.count - 1];
        loader->cache.count--;
        module_unload(module, loader->vm);
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
    
    // TODO: Check for active references
    // This would require reference counting or GC integration
    
    return true;
}

// Force unload a module (even with active references)
void module_force_unload(Module* module, VM* vm) {
    if (!module) return;
    
    fprintf(stderr, "Warning: Force unloading module '%s'\n", module->path);
    module_unload(module, vm);
}