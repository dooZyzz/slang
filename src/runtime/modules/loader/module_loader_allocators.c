/**
 * SwiftLang Module System Implementation
 * 
 * This file implements the core module loading and management system for SwiftLang.
 * The module system supports multiple module types and loading mechanisms.
 * 
 * This version uses the custom memory allocator system for better memory management.
 */

#include "runtime/modules/loader/module_loader.h"
#include "utils/allocators.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <glob.h>
#include <sys/time.h>
#include <time.h>
#include <libgen.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "runtime/core/vm.h"
#include "ast/ast.h"
#include "runtime/modules/lifecycle/builtin_modules.h"
#include "debug/debug.h"
#include "runtime/modules/formats/module_archive.h"
#include "runtime/modules/formats/module_format.h"
#include "runtime/packages/package.h"
#include "utils/bytecode_format.h"
#include "utils/logger.h"
#include "utils/version.h"
#include "runtime/modules/extensions/module_hooks.h"
#include "runtime/modules/extensions/module_inspect.h"
#include "runtime/modules/loader/module_cache.h"

// Constants
#define MODULE_NAME_BUFFER_SIZE 512
#define MODULE_PATH_BUFFER_SIZE 1024

// Forward declarations
bool ensure_module_initialized(Module* module, VM* vm);
static bool check_module_version_compatibility(const char* required_version, const char* module_version);

// Hash function for module scope
static uint32_t hash_string(const char* key) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; key[i] != '\0'; i++) {
        hash ^= (uint8_t)key[i];
        hash *= 16777619u;
    }
    return hash;
}

// Module scope functions
ModuleScope* module_scope_create(void) {
    ModuleScope* scope = MODULE_NEW(ModuleScope, "module-scope");
    if (!scope) return NULL;
    
    scope->capacity = 16;
    scope->entries = MODULE_ALLOC_ZERO(sizeof(ModuleScopeEntry) * scope->capacity, "scope-entries");
    if (!scope->entries) {
        MODULE_FREE(scope, sizeof(ModuleScope));
        return NULL;
    }
    
    scope->count = 0;
    return scope;
}

void module_scope_destroy(ModuleScope* scope) {
    if (!scope) return;
    
    for (size_t i = 0; i < scope->capacity; i++) {
        if (scope->entries[i].name) {
            MODULE_FREE((void*)scope->entries[i].name, strlen(scope->entries[i].name) + 1);
        }
    }
    MODULE_FREE(scope->entries, sizeof(ModuleScopeEntry) * scope->capacity);
    MODULE_FREE(scope, sizeof(ModuleScope));
}

static void module_scope_grow(ModuleScope* scope) {
    size_t old_capacity = scope->capacity;
    scope->capacity *= 2;
    
    ModuleScopeEntry* new_entries = MODULE_ALLOC_ZERO(sizeof(ModuleScopeEntry) * scope->capacity, "scope-grow");
    if (!new_entries) return; // Failed to grow
    
    // Rehash existing entries
    for (size_t i = 0; i < old_capacity; i++) {
        if (scope->entries[i].name) {
            uint32_t hash = hash_string(scope->entries[i].name);
            size_t index = hash % scope->capacity;
            
            while (new_entries[index].name != NULL) {
                index = (index + 1) % scope->capacity;
            }
            
            new_entries[index] = scope->entries[i];
        }
    }
    
    MODULE_FREE(scope->entries, sizeof(ModuleScopeEntry) * old_capacity);
    scope->entries = new_entries;
}

bool module_scope_set(ModuleScope* scope, const char* name, TaggedValue value) {
    if (!scope || !name) return false;
    
    // Check if we need to grow
    if (scope->count >= scope->capacity * 0.75) {
        module_scope_grow(scope);
    }
    
    uint32_t hash = hash_string(name);
    size_t index = hash % scope->capacity;
    
    // Linear probing
    while (scope->entries[index].name != NULL) {
        if (strcmp(scope->entries[index].name, name) == 0) {
            // Update existing entry
            scope->entries[index].value = value;
            return true;
        }
        index = (index + 1) % scope->capacity;
    }
    
    // Insert new entry
    scope->entries[index].name = MODULE_DUP(name);
    if (!scope->entries[index].name) return false;
    
    scope->entries[index].value = value;
    scope->count++;
    
    return true;
}

TaggedValue* module_scope_get(ModuleScope* scope, const char* name) {
    if (!scope || !name) return NULL;
    
    uint32_t hash = hash_string(name);
    size_t index = hash % scope->capacity;
    
    // Linear probing
    while (scope->entries[index].name != NULL) {
        if (strcmp(scope->entries[index].name, name) == 0) {
            return &scope->entries[index].value;
        }
        index = (index + 1) % scope->capacity;
    }
    
    return NULL;
}

// Module creation
Module* module_create(const char* name, const char* path) {
    Module* module = MODULE_NEW(Module, "module");
    if (!module) return NULL;
    
    module->name = MODULE_DUP(name);
    module->path = path ? MODULE_DUP(path) : NULL;
    module->state = MODULE_STATE_CREATED;
    module->scope = module_scope_create();
    module->exports.count = 0;
    module->exports.capacity = 0;
    module->exports.names = NULL;
    module->dependencies = NULL;
    module->dependency_count = 0;
    module->native_handle = NULL;
    module->chunk.count = 0;
    module->chunk.capacity = 0;
    module->chunk.code = NULL;
    module->chunk.lines = NULL;
    module->chunk.constants.count = 0;
    module->chunk.constants.capacity = 0;
    module->chunk.constants.values = NULL;
    module->version = MODULE_DUP(SWIFTLANG_VERSION);
    
    if (!module->name || !module->scope) {
        module_destroy(module);
        return NULL;
    }
    
    return module;
}

void module_destroy(Module* module) {
    if (!module) return;
    
    if (module->name) {
        MODULE_FREE((void*)module->name, strlen(module->name) + 1);
    }
    
    if (module->path) {
        MODULE_FREE((void*)module->path, strlen(module->path) + 1);
    }
    
    if (module->version) {
        MODULE_FREE((void*)module->version, strlen(module->version) + 1);
    }
    
    // Free exports
    for (size_t i = 0; i < module->exports.count; i++) {
        if (module->exports.names[i]) {
            MODULE_FREE(module->exports.names[i], strlen(module->exports.names[i]) + 1);
        }
    }
    if (module->exports.names) {
        MODULE_FREE(module->exports.names, module->exports.capacity * sizeof(char*));
    }
    
    // Free dependencies
    for (size_t i = 0; i < module->dependency_count; i++) {
        if (module->dependencies[i].name) {
            MODULE_FREE((void*)module->dependencies[i].name, strlen(module->dependencies[i].name) + 1);
        }
        if (module->dependencies[i].version) {
            MODULE_FREE((void*)module->dependencies[i].version, strlen(module->dependencies[i].version) + 1);
        }
    }
    if (module->dependencies) {
        MODULE_FREE(module->dependencies, module->dependency_count * sizeof(ModuleDependency));
    }
    
    // Free chunk
    chunk_free(&module->chunk);
    
    // Free scope
    module_scope_destroy(module->scope);
    
    // Close native handle if any
    if (module->native_handle) {
        dlclose(module->native_handle);
    }
    
    MODULE_FREE(module, sizeof(Module));
}

// Module loader creation
ModuleLoader* module_loader_create(VM* vm) {
    ModuleLoader* loader = MODULE_NEW(ModuleLoader, "module-loader");
    if (!loader) return NULL;
    
    loader->vm = vm;
    loader->modules = NULL;
    loader->module_count = 0;
    loader->module_capacity = 0;
    loader->search_paths = NULL;
    loader->search_path_count = 0;
    loader->search_path_capacity = 0;
    loader->loading_stack = NULL;
    loader->loading_stack_count = 0;
    loader->loading_stack_capacity = 0;
    loader->package_system = package_system_create();
    
    // Initialize with default search paths
    const char* default_paths[] = {
        ".",
        "./modules",
        "./lib/swift",
        "/usr/local/lib/swift",
        "/usr/lib/swift"
    };
    
    for (size_t i = 0; i < sizeof(default_paths) / sizeof(default_paths[0]); i++) {
        module_loader_add_search_path(loader, default_paths[i]);
    }
    
    // Get environment search paths
    const char* env_path = getenv("SWIFTLANG_MODULE_PATH");
    if (env_path) {
        char* paths = MODULE_DUP(env_path);
        char* path = strtok(paths, ":");
        while (path) {
            module_loader_add_search_path(loader, path);
            path = strtok(NULL, ":");
        }
        MODULE_FREE(paths, strlen(env_path) + 1);
    }
    
    // Initialize hook manager
    hook_manager_init();
    
    // Initialize module inspection
    module_inspect_init();
    
    return loader;
}

void module_loader_destroy(ModuleLoader* loader) {
    if (!loader) return;
    
    // Cleanup hook manager
    hook_manager_cleanup();
    
    // Cleanup module inspection
    module_inspect_cleanup();
    
    // Destroy all modules
    for (size_t i = 0; i < loader->module_count; i++) {
        module_destroy(loader->modules[i]);
    }
    if (loader->modules) {
        MODULE_FREE(loader->modules, loader->module_capacity * sizeof(Module*));
    }
    
    // Free search paths
    for (size_t i = 0; i < loader->search_path_count; i++) {
        MODULE_FREE(loader->search_paths[i], strlen(loader->search_paths[i]) + 1);
    }
    if (loader->search_paths) {
        MODULE_FREE(loader->search_paths, loader->search_path_capacity * sizeof(char*));
    }
    
    // Free loading stack
    if (loader->loading_stack) {
        MODULE_FREE(loader->loading_stack, loader->loading_stack_capacity * sizeof(const char*));
    }
    
    // Destroy package system
    if (loader->package_system) {
        package_system_destroy(loader->package_system);
    }
    
    MODULE_FREE(loader, sizeof(ModuleLoader));
}

// Add search path
bool module_loader_add_search_path(ModuleLoader* loader, const char* path) {
    if (!loader || !path) return false;
    
    // Check if we need to grow the array
    if (loader->search_path_count >= loader->search_path_capacity) {
        size_t new_capacity = loader->search_path_capacity == 0 ? 8 : loader->search_path_capacity * 2;
        char** new_paths = MODULE_ALLOC(sizeof(char*) * new_capacity, "search-paths");
        if (!new_paths) return false;
        
        if (loader->search_paths) {
            memcpy(new_paths, loader->search_paths, loader->search_path_count * sizeof(char*));
            MODULE_FREE(loader->search_paths, loader->search_path_capacity * sizeof(char*));
        }
        
        loader->search_paths = new_paths;
        loader->search_path_capacity = new_capacity;
    }
    
    // Resolve to absolute path
    char* absolute_path = realpath(path, NULL);
    if (!absolute_path) {
        // Path doesn't exist yet, store as-is
        loader->search_paths[loader->search_path_count] = MODULE_DUP(path);
    } else {
        loader->search_paths[loader->search_path_count] = MODULE_DUP(absolute_path);
        free(absolute_path);
    }
    
    if (!loader->search_paths[loader->search_path_count]) {
        return false;
    }
    
    loader->search_path_count++;
    return true;
}

// Note: This is a partial refactoring of module_loader.c
// The key changes are:
// 1. All allocations use MODULE_* macros for module subsystem
// 2. Size tracking for proper deallocation
// 3. Tagged allocations for debugging