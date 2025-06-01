/**
 * SwiftLang Module System Implementation
 * 
 * This file implements the core module loading and management system for SwiftLang.
 * The module system supports multiple module types and loading mechanisms:
 * 
 * 1. Compiled Modules (.swiftmodule archives)
 *    - ZIP archives containing bytecode and metadata
 *    - Pre-compiled for fast loading
 *    - Support for module exports and dependencies
 * 
 * 2. Source Modules (.swift files)
 *    - Compiled on-demand from source
 *    - Cached as bytecode for subsequent loads
 *    - Support for directory-based modules with module.json
 * 
 * 3. Native Modules (.dylib/.so)
 *    - Dynamic libraries with C API
 *    - Seamless integration with SwiftLang functions
 *    - Used for system interfaces and performance-critical code
 * 
 * Key Features:
 * - Module caching to avoid recompilation
 * - Path resolution with multiple search paths
 * - Package system integration
 * - Export/import mechanism for controlled visibility
 * - Module-scoped globals and execution context
 * 
 * The module loader maintains a cache of loaded modules and handles
 * circular dependencies through proper state management.
 */

#include "runtime/modules/loader/module_loader.h"
#include "utils/allocators.h"
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
#include <stdlib.h>
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

// Include common module macros
#include "runtime/modules/module_allocator_macros.h"

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
    ModuleScope* scope = MODULES_NEW_ZERO(ModuleScope);
    scope->capacity = 16;
    scope->entries = MODULES_NEW_ARRAY_ZERO(ModuleScopeEntry, scope->capacity);
    return scope;
}

void module_scope_destroy(ModuleScope* scope) {
    if (!scope) return;
    
    for (size_t i = 0; i < scope->capacity; i++) {
        if (scope->entries[i].name) {
            size_t name_len = strlen(scope->entries[i].name) + 1;
            STRINGS_FREE((void*)scope->entries[i].name, name_len);
        }
    }
    MODULES_FREE(scope->entries, scope->capacity * sizeof(ModuleScopeEntry));
    MODULES_FREE(scope, sizeof(ModuleScope));
}

static void module_scope_grow(ModuleScope* scope) {
    size_t old_capacity = scope->capacity;
    ModuleScopeEntry* old_entries = scope->entries;
    
    scope->capacity *= 2;
    scope->entries = MODULES_NEW_ARRAY_ZERO(ModuleScopeEntry, scope->capacity);
    scope->count = 0;
    
    // Rehash existing entries
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_entries[i].name) {
            module_scope_define(scope, old_entries[i].name, old_entries[i].value, old_entries[i].is_exported);
            // Free the name from old entries after copying
            size_t name_len = strlen(old_entries[i].name) + 1;
            STRINGS_FREE((void*)old_entries[i].name, name_len);
        }
    }
    
    MODULES_FREE(old_entries, old_capacity * sizeof(ModuleScopeEntry));
}

void module_scope_define(ModuleScope* scope, const char* name, TaggedValue value, bool is_exported) {
    if (!scope) return;
    
    if (scope->count + 1 > scope->capacity * 0.75) {
        module_scope_grow(scope);
    }
    
    uint32_t hash = hash_string(name);
    size_t index = hash & (scope->capacity - 1);
    
    while (scope->entries[index].name != NULL) {
        if (strcmp(scope->entries[index].name, name) == 0) {
            // Update existing entry
            scope->entries[index].value = value;
            scope->entries[index].is_exported = is_exported;
            return;
        }
        index = (index + 1) & (scope->capacity - 1);
    }
    
    // New entry
    scope->entries[index].name = STRINGS_STRDUP(name);
    scope->entries[index].value = value;
    scope->entries[index].is_exported = is_exported;
    scope->count++;
}

TaggedValue module_scope_get(ModuleScope* scope, const char* name) {
    if (!scope) return NIL_VAL;
    
    uint32_t hash = hash_string(name);
    size_t index = hash & (scope->capacity - 1);
    
    while (scope->entries[index].name != NULL) {
        if (strcmp(scope->entries[index].name, name) == 0) {
            return scope->entries[index].value;
        }
        index = (index + 1) & (scope->capacity - 1);
    }
    
    return NIL_VAL;
}

bool module_scope_has(ModuleScope* scope, const char* name) {
    if (!scope) return false;
    
    uint32_t hash = hash_string(name);
    size_t index = hash & (scope->capacity - 1);
    
    while (scope->entries[index].name != NULL) {
        if (strcmp(scope->entries[index].name, name) == 0) {
            return true;
        }
        index = (index + 1) & (scope->capacity - 1);
    }
    
    return false;
}

bool module_scope_is_exported(ModuleScope* scope, const char* name) {
    if (!scope) return false;
    
    uint32_t hash = hash_string(name);
    size_t index = hash & (scope->capacity - 1);
    
    while (scope->entries[index].name != NULL) {
        if (strcmp(scope->entries[index].name, name) == 0) {
            return scope->entries[index].is_exported;
        }
        index = (index + 1) & (scope->capacity - 1);
    }
    
    return false;
}

// Module scope lookup functions
TaggedValue module_get_from_scope(Module* module, const char* name) {
    if (!module || !module->scope) return NIL_VAL;
    return module_scope_get(module->scope, name);
}

bool module_has_in_scope(Module* module, const char* name) {
    if (!module || !module->scope) return false;
    return module_scope_has(module->scope, name);
}

/**
 * Load a module from a compiled .swiftmodule archive.
 * 
 * This function handles the complete process of loading a module from a ZIP archive:
 * 1. Opens the archive and extracts metadata (module.json)
 * 2. Deserializes the bytecode for the module
 * 3. Executes the module bytecode in a separate VM instance
 * 4. Extracts exported symbols from the module object
 * 5. Returns a fully initialized Module structure
 * 
 * @param loader The module loader instance
 * @param archive_path Path to the .swiftmodule archive file
 * @param module_name Name of the module being loaded
 * @return Initialized Module* on success, or Module with error state on failure
 */
static Module* module_load_from_archive(ModuleLoader* loader, const char* archive_path, const char* module_name) {
    MODULE_DEBUG("module_load_from_archive called: archive=%s, module=%s\n", archive_path, module_name);
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    // Create module
    Module* module = MODULES_NEW_ZERO(Module);
    module->path = STRINGS_STRDUP(module_name);
    module->absolute_path = STRINGS_STRDUP(archive_path);
    module->state = MODULE_STATE_LOADING;
    module->is_native = false;
    module->ref_count = 0;
    pthread_mutex_init(&module->ref_mutex, NULL);
    module->last_access_time = time(NULL);
    
    // Track load start
    module_track_load_start(module);
    
    // Initialize exports
    module->exports.capacity = 16;
    module->exports.names = MODULES_NEW_ARRAY_ZERO(char*, module->exports.capacity);
    module->exports.values = MODULES_NEW_ARRAY_ZERO(TaggedValue, module->exports.capacity);
    module->exports.visibility = MODULES_NEW_ARRAY_ZERO(uint8_t, module->exports.capacity);
    
    // Create module object
    module->module_object = object_create();
    
    // Open the archive
    ModuleArchive* archive = module_archive_open(archive_path);
    if (!archive) {
        fprintf(stderr, "Failed to open module archive: %s\n", archive_path);
        module->state = MODULE_STATE_ERROR;
        return module;
    }
    
    // Read module metadata from JSON
    char* json_content = NULL;
    size_t json_size = 0;
    
    if (!module_archive_extract_json(archive, &json_content, &json_size)) {
        fprintf(stderr, "Failed to extract module.json from archive\n");
        module_archive_destroy(archive);
        module->state = MODULE_STATE_ERROR;
        return module;
    }
    
    // Parse version from JSON
    const char* version_start = strstr(json_content, "\"version\"");
    if (version_start) {
        version_start = strchr(version_start, ':');
        if (version_start) {
            version_start++; // Skip :
            // Skip whitespace
            while (*version_start && (*version_start == ' ' || *version_start == '\t')) {
                version_start++;
            }
            if (*version_start == '"') {
                version_start++; // Skip opening quote
                const char* version_end = strchr(version_start, '"');
                if (version_end) {
                    size_t version_len = version_end - version_start;
                    char* version = STRINGS_ALLOC(version_len + 1);
                    strncpy(version, version_start, version_len);
                    version[version_len] = '\0';
                    module->version = version;
                    MODULE_DEBUG("Module version: %s\n", module->version);
                }
            }
        }
    }
    
    // Free json_content using string allocator
    STRINGS_FREE(json_content, json_size + 1);
    
    // Find the main module bytecode (usually named after the package)
    uint8_t* bytecode = NULL;
    size_t bytecode_size = 0;
    
    // Try with swift. prefix first
    char bytecode_name[MODULE_NAME_BUFFER_SIZE];
    snprintf(bytecode_name, sizeof(bytecode_name), "swift.%s", module_name);
    MODULE_DEBUG("Looking for bytecode: %s\n", bytecode_name);
    
    if (!module_archive_extract_bytecode(archive, bytecode_name, &bytecode, &bytecode_size)) {
        MODULE_DEBUG("Failed with swift prefix, trying without\n");
        
        // Try without the swift. prefix
        if (!module_archive_extract_bytecode(archive, module_name, &bytecode, &bytecode_size)) {
            fprintf(stderr, "Failed to extract module bytecode: %s\n", module_name);
            module_archive_destroy(archive);
            module->state = MODULE_STATE_ERROR;
            return module;
        }
    }
    
    MODULE_DEBUG("Successfully extracted bytecode, size: %zu\n", bytecode_size);
    
    // Deserialize and execute the bytecode
    Allocator* bc_alloc = allocators_get(ALLOC_SYSTEM_BYTECODE);
    Chunk* chunk = BYTECODE_NEW(Chunk);
    chunk_init(chunk);
    
    // Use the proper bytecode deserialization
    MODULE_DEBUG("About to deserialize bytecode of size %zu\n", bytecode_size);
    if (!bytecode_deserialize(bytecode, bytecode_size, chunk)) {
        fprintf(stderr, "Failed to deserialize module bytecode\n");
        MODULES_FREE(bytecode, bytecode_size);
        chunk_free(chunk);
        BYTECODE_FREE(chunk, sizeof(Chunk));
        module_archive_destroy(archive);
        module->state = MODULE_STATE_ERROR;
        return module;
    }
    MODULE_DEBUG("Bytecode deserialized successfully\n");
    
    MODULES_FREE(bytecode, bytecode_size);
    
    // Execute module in VM context
    VM* vm = loader->vm;
    
    // Save VM state
    Chunk* saved_chunk = vm->chunk;
    const char* saved_module_path = vm->current_module_path;
    Module* saved_module = vm->current_module;
    
    // Set current module context
    vm->current_module_path = module->path;
    vm->current_module = module;
    
    // Define __module_exports__
    define_global(vm, "__module_exports__", OBJECT_VAL(module->module_object));
    
    // Execute bytecode
    MODULE_DEBUG("Executing module bytecode for: %s (chunk has %zu bytes)\n", module_name, chunk->count);
    MODULE_DEBUG("Module pointer: %p, current_module set to: %p\n", (void*)module, (void*)vm->current_module);
    
    // Store the chunk for potential lazy execution
    module->chunk = chunk;
    
    // Check if lazy loading is enabled
    bool lazy_loading = getenv("SWIFTLANG_LAZY_MODULES") != NULL;
    if (lazy_loading) {
        MODULE_DEBUG("Lazy loading enabled for module: %s\n", module_name);
        module->state = MODULE_STATE_UNLOADED;
        module_archive_destroy(archive);
        return module;
    }
    
    // Execute module immediately if not lazy loading
    MODULE_DEBUG("Creating separate VM for module execution\n");
    
    VM module_vm;
    vm_init_with_loader(&module_vm, loader);
    
    // Copy necessary state
    module_vm.current_module_path = module->path;
    module_vm.module_loader = loader;
    module_vm.current_module = module; // Set the module context
    
    // Define __module_exports__ for export statements
    define_global(&module_vm, "__module_exports__", OBJECT_VAL(module->module_object));
    
    MODULE_DEBUG("About to call vm_interpret in separate VM\n");
    InterpretResult result = vm_interpret(&module_vm, chunk);
    MODULE_DEBUG("vm_interpret returned: %d\n", result);
    
    if (result == INTERPRET_OK) {
        // Copy module globals before destroying the VM
        module->globals.count = module_vm.globals.count;
        module->globals.capacity = module_vm.globals.capacity;
        module->globals.names = MODULES_NEW_ARRAY(char*, module->globals.capacity);
        module->globals.values = MODULES_NEW_ARRAY(TaggedValue, module->globals.capacity);
        
        for (size_t i = 0; i < module->globals.count; i++) {
            module->globals.names[i] = STRINGS_STRDUP(module_vm.globals.names[i]);
            module->globals.values[i] = module_vm.globals.values[i];
        }
        
        // fprintf(stderr, "[DEBUG] Preserved %zu module globals\n", module->globals.count);
    }
    
    // Clean up
    module_vm.module_loader = NULL; // Don't free the shared loader
    vm_free(&module_vm);
    
    if (result != INTERPRET_OK) {
        fprintf(stderr, "Failed to execute module: %s (result: %d)\n", module_name, result);
        module->state = MODULE_STATE_ERROR;
    } else {
        // fprintf(stderr, "[DEBUG] Module executed successfully: %s\n", module_name);
        module->state = MODULE_STATE_LOADED;
        
        // Execute module initialization hooks (before VM is freed)
        bool hooks_ok = module_execute_init_hooks(module, &module_vm);
        
        // Extract exports from module object
        if (module->module_object) {
            MODULE_DEBUG("Extracting exports from module object\n");
            ObjectProperty* prop = module->module_object->properties;
            int prop_count = 0;
            while (prop) {
                prop_count++;
                MODULE_DEBUG("Found property: %s\n", prop->key);
                // If the property is a function, ensure it has a reference to this module
                if (prop->value && IS_FUNCTION(*prop->value)) {
                    AS_FUNCTION(*prop->value)->module = module;
                }
                // Add to exports array
                if (module->exports.count >= module->exports.capacity) {
                    size_t old_capacity = module->exports.capacity;
                    module->exports.capacity *= 2;
                    
                    // Realloc names array
                    char** new_names = MODULES_NEW_ARRAY(char*, module->exports.capacity);
                    memcpy(new_names, module->exports.names, old_capacity * sizeof(char*));
                    MODULES_FREE(module->exports.names, old_capacity * sizeof(char*));
                    module->exports.names = new_names;
                    
                    // Realloc values array
                    TaggedValue* new_values = MODULES_NEW_ARRAY(TaggedValue, module->exports.capacity);
                    memcpy(new_values, module->exports.values, old_capacity * sizeof(TaggedValue));
                    MODULES_FREE(module->exports.values, old_capacity * sizeof(TaggedValue));
                    module->exports.values = new_values;
                    
                    // Realloc visibility array
                    uint8_t* new_visibility = MODULES_NEW_ARRAY(uint8_t, module->exports.capacity);
                    memcpy(new_visibility, module->exports.visibility, old_capacity * sizeof(uint8_t));
                    MODULES_FREE(module->exports.visibility, old_capacity * sizeof(uint8_t));
                    module->exports.visibility = new_visibility;
                }
                module->exports.names[module->exports.count] = STRINGS_STRDUP(prop->key);
                module->exports.values[module->exports.count] = *prop->value;
                module->exports.visibility[module->exports.count] = 1; // Public
                module->exports.count++;
                
                prop = prop->next;
            }
            MODULE_DEBUG("Total properties extracted: %d, exports count: %zu\n", prop_count, module->exports.count);
        }
        
        // Check hooks result after exports are processed
        if (!hooks_ok) {
            fprintf(stderr, "Module init hooks failed for: %s\n", module_name);
            module->state = MODULE_STATE_ERROR;
        }
    }
    
    // Restore VM state
    vm->chunk = saved_chunk;
    vm->current_module_path = saved_module_path;
    vm->current_module = saved_module;
    undefine_global(vm, "__module_exports__");
    
    // fprintf(stderr, "[DEBUG] Module execution completed, cleaning up\n");
    
    // Clean up
    chunk_free(chunk);
    BYTECODE_FREE(chunk, sizeof(Chunk));
    module_archive_destroy(archive);
    
    return module;
}

/**
 * Ensure a module is initialized (for lazy loading).
 * This function executes the module's bytecode if it hasn't been executed yet.
 */
bool ensure_module_initialized(Module* module, VM* vm) {
    if (!module || module->state == MODULE_STATE_LOADED || module->state == MODULE_STATE_ERROR) {
        return module && module->state == MODULE_STATE_LOADED;
    }
    
    if (module->state == MODULE_STATE_LOADING) {
        // Already being loaded (circular dependency)
        return false;
    }
    
    if (module->state == MODULE_STATE_UNLOADED && module->chunk) {
        fprintf(stderr, "[DEBUG] Initializing lazy-loaded module: %s\n", module->path);
        
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
        Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
        
        // Mark as loading to prevent circular initialization
        module->state = MODULE_STATE_LOADING;
        
        // Create a separate VM for module execution
        VM module_vm;
        vm_init_with_loader(&module_vm, vm->module_loader);
        
        // Copy necessary state
        module_vm.current_module_path = module->path;
        module_vm.module_loader = vm->module_loader;
        module_vm.current_module = module;
        
        // Define __module_exports__ for export statements
        define_global(&module_vm, "__module_exports__", OBJECT_VAL(module->module_object));
        
        // Execute the module
        InterpretResult result = vm_interpret(&module_vm, module->chunk);
        
        if (result == INTERPRET_OK) {
            // Copy module globals
            module->globals.count = module_vm.globals.count;
            module->globals.capacity = module_vm.globals.capacity;
            module->globals.names = MODULES_NEW_ARRAY(char*, module->globals.capacity);
            module->globals.values = MODULES_NEW_ARRAY(TaggedValue, module->globals.capacity);
            
            for (size_t i = 0; i < module->globals.count; i++) {
                module->globals.names[i] = STRINGS_STRDUP(module_vm.globals.names[i]);
                module->globals.values[i] = module_vm.globals.values[i];
            }
            
            // Extract exports from module object
            if (module->module_object) {
                ObjectProperty* prop = module->module_object->properties;
                while (prop) {
                    if (prop->value && IS_FUNCTION(*prop->value)) {
                        AS_FUNCTION(*prop->value)->module = module;
                    }
                    if (module->exports.count >= module->exports.capacity) {
                        size_t old_capacity = module->exports.capacity;
                        module->exports.capacity *= 2;
                        
                        // Realloc arrays
                        char** new_names = MODULES_NEW_ARRAY(char*, module->exports.capacity);
                        memcpy(new_names, module->exports.names, old_capacity * sizeof(char*));
                        MODULES_FREE(module->exports.names, old_capacity * sizeof(char*));
                        module->exports.names = new_names;
                        
                        TaggedValue* new_values = MODULES_NEW_ARRAY(TaggedValue, module->exports.capacity);
                        memcpy(new_values, module->exports.values, old_capacity * sizeof(TaggedValue));
                        MODULES_FREE(module->exports.values, old_capacity * sizeof(TaggedValue));
                        module->exports.values = new_values;
                        
                        uint8_t* new_visibility = MODULES_NEW_ARRAY(uint8_t, module->exports.capacity);
                        memcpy(new_visibility, module->exports.visibility, old_capacity * sizeof(uint8_t));
                        MODULES_FREE(module->exports.visibility, old_capacity * sizeof(uint8_t));
                        module->exports.visibility = new_visibility;
                    }
                    module->exports.names[module->exports.count] = STRINGS_STRDUP(prop->key);
                    module->exports.values[module->exports.count] = *prop->value;
                    module->exports.visibility[module->exports.count] = 1;
                    module->exports.count++;
                    prop = prop->next;
                }
            }
            
            module->state = MODULE_STATE_LOADED;
            
            // Execute module initialization hooks
            if (!module_execute_init_hooks(module, vm)) {
                fprintf(stderr, "Module init hooks failed for: %s\n", module->path);
                module->state = MODULE_STATE_ERROR;
                return false;
            }
            
            // Execute first-use hooks for lazy-loaded modules
            module_execute_first_use_hooks(module, vm);
        } else {
            module->state = MODULE_STATE_ERROR;
        }
        
        // Clean up
        module_vm.module_loader = NULL;
        vm_free(&module_vm);
        
        // Don't need the chunk anymore
        Allocator* bc_alloc = allocators_get(ALLOC_SYSTEM_BYTECODE);
        chunk_free(module->chunk);
        BYTECODE_FREE(module->chunk, sizeof(Chunk));
        module->chunk = NULL;
        
        return module->state == MODULE_STATE_LOADED;
    }
    
    return false;
}

static char* resolve_module_path(ModuleLoader* loader, const char* path, const char* relative_to) {
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    // First, try to resolve through package system
    if (loader->package_system) {
        char* resolved = package_resolve_module_path(loader->package_system, path);
        if (resolved) {
            return resolved;
        }
    }
    
    // Handle local imports with @ prefix
    if (path[0] == '@') {
        // Local import - skip @ and let normal resolution handle it
        // This allows @module to find module.swiftmodule in search paths
        const char* module_name = path + 1;  // Skip @
        
        // Debug output
        if (getenv("SWIFTLANG_DEBUG")) {
            printf("DEBUG: Resolving @%s\n", module_name);
            printf("DEBUG: Search paths:\n");
            for (size_t i = 0; i < loader->search_paths.count; i++) {
                printf("  [%zu] %s\n", i, loader->search_paths.paths[i]);
            }
        }
        
        // Try each search path for .swiftmodule files
        for (size_t i = 0; i < loader->search_paths.count; i++) {
            char buffer[PATH_MAX];
            
            // Try as directory with module.json first (for source modules)
            snprintf(buffer, sizeof(buffer), "%s/%s/module.json", loader->search_paths.paths[i], module_name);
            if (access(buffer, F_OK) == 0) {
                // Return the directory path
                snprintf(buffer, sizeof(buffer), "%s/%s", loader->search_paths.paths[i], module_name);
                if (getenv("SWIFTLANG_DEBUG")) {
                    printf("DEBUG: Found module directory at %s\n", buffer);
                }
                return STRINGS_STRDUP(buffer);
            }
            
            // Try as .swiftmodule archive
            snprintf(buffer, sizeof(buffer), "%s/%s.swiftmodule", loader->search_paths.paths[i], module_name);
            if (access(buffer, F_OK) == 0) {
                if (getenv("SWIFTLANG_DEBUG")) {
                    printf("DEBUG: Found .swiftmodule at %s\n", buffer);
                }
                return STRINGS_STRDUP(buffer);
            }
            
            // Try in modules subdirectory
            snprintf(buffer, sizeof(buffer), "%s/modules/%s.swiftmodule", loader->search_paths.paths[i], module_name);
            if (access(buffer, F_OK) == 0) {
                if (getenv("SWIFTLANG_DEBUG")) {
                    printf("DEBUG: Found .swiftmodule in modules/ at %s\n", buffer);
                }
                return STRINGS_STRDUP(buffer);
            }
            
            // Try as .swift file
            snprintf(buffer, sizeof(buffer), "%s/%s.swift", loader->search_paths.paths[i], module_name);
            if (access(buffer, F_OK) == 0) {
                if (getenv("SWIFTLANG_DEBUG")) {
                    printf("DEBUG: Found .swift file at %s\n", buffer);
                }
                return STRINGS_STRDUP(buffer);
            }
        }
        
        // Also check current directory
        char buffer[PATH_MAX];
        snprintf(buffer, sizeof(buffer), "%s.swiftmodule", module_name);
        if (access(buffer, F_OK) == 0) {
            if (getenv("SWIFTLANG_DEBUG")) {
                printf("DEBUG: Found .swiftmodule in current dir at %s\n", buffer);
            }
            return STRINGS_STRDUP(buffer);
        }
        
        // Check modules subdirectory in current directory
        snprintf(buffer, sizeof(buffer), "modules/%s.swiftmodule", module_name);
        if (access(buffer, F_OK) == 0) {
            if (getenv("SWIFTLANG_DEBUG")) {
                printf("DEBUG: Found .swiftmodule in modules/ subdir at %s\n", buffer);
            }
            return STRINGS_STRDUP(buffer);
        }
        
        if (getenv("SWIFTLANG_DEBUG")) {
            printf("DEBUG: Module @%s not found\n", module_name);
        }
        
        return NULL;
    }
    
    // Handle native imports with $ prefix
    if (path[0] == '$') {
        // Native import - return the module name without $
        // The module loading system will handle finding the native module
        return STRINGS_STRDUP(path + 1);  // Skip $ and return just the module name
    }
    
    // If path is absolute, use it directly
    if (path[0] == '/') {
        return STRINGS_STRDUP(path);
    }
    
    char buffer[PATH_MAX];
    char converted_path[PATH_MAX];
    
    // Handle @module syntax for local modules
    const char* search_path = path;
    if (path[0] == '@') {
        search_path = path + 1; // Skip the @ prefix
    }
    
    // Convert dotted path to file path (e.g., sys.native.io -> sys/native/io)
    size_t j = 0;
    for (size_t i = 0; search_path[i] != '\0' && j < PATH_MAX - 1; i++) {
        if (search_path[i] == '.') {
            converted_path[j++] = '/';
        } else {
            converted_path[j++] = search_path[i];
        }
    }
    converted_path[j] = '\0';
    
    // Native modules are determined by the is_native flag, not the path
    bool is_native = false;
    
    // If path starts with './' or '../', resolve relative to the importing file
    if ((path[0] == '.' && path[1] == '/') || 
        (path[0] == '.' && path[1] == '.' && path[2] == '/')) {
        if (relative_to) {
            // Get directory of the importing file
            char dir[PATH_MAX];
            strncpy(dir, relative_to, PATH_MAX - 1);
            dir[PATH_MAX - 1] = '\0';
            
            // Find last slash to get directory
            char* last_slash = strrchr(dir, '/');
            if (last_slash) {
                *last_slash = '\0';
                
                // Resolve relative path
                snprintf(buffer, sizeof(buffer), "%s/%s", dir, path);
                if (access(buffer, F_OK) == 0) {
                    return STRINGS_STRDUP(buffer);
                }
                
                // Try with .swift extension
                snprintf(buffer, sizeof(buffer), "%s/%s.swift", dir, path);
                if (access(buffer, F_OK) == 0) {
                    return STRINGS_STRDUP(buffer);
                }
            }
        }
    }
    
    // Try each search path
    for (size_t i = 0; i < loader->search_paths.count; i++) {
        // For native modules, look in the native subdirectory
        if (is_native) {
            // Extract module name after sys.native.
            const char* native_name = path + 11;
            
            #ifdef __APPLE__
            snprintf(buffer, sizeof(buffer), "%s/native/%s.dylib", loader->search_paths.paths[i], native_name);
            #else
            snprintf(buffer, sizeof(buffer), "%s/native/%s.so", loader->search_paths.paths[i], native_name);
            #endif
            if (access(buffer, F_OK) == 0) {
                return STRINGS_STRDUP(buffer);
            }
        }
        
        // Try as directory with module.json
        snprintf(buffer, sizeof(buffer), "%s/%s/module.json", loader->search_paths.paths[i], converted_path);
        if (access(buffer, F_OK) == 0) {
            snprintf(buffer, sizeof(buffer), "%s/%s", loader->search_paths.paths[i], converted_path);
            return STRINGS_STRDUP(buffer);
        }
        
        // Try as regular Swift module with converted path
        snprintf(buffer, sizeof(buffer), "%s/%s", loader->search_paths.paths[i], converted_path);
        if (access(buffer, F_OK) == 0) {
            return STRINGS_STRDUP(buffer);
        }
        
        // Try with .swift extension
        snprintf(buffer, sizeof(buffer), "%s/%s.swift", loader->search_paths.paths[i], converted_path);
        if (access(buffer, F_OK) == 0) {
            return STRINGS_STRDUP(buffer);
        }
        
        // Also try with original dotted path for backward compatibility
        snprintf(buffer, sizeof(buffer), "%s/%s.swift", loader->search_paths.paths[i], search_path);
        if (access(buffer, F_OK) == 0) {
            return STRINGS_STRDUP(buffer);
        }
        
        // Try as .swiftmodule archive
        snprintf(buffer, sizeof(buffer), "%s/%s.swiftmodule", loader->search_paths.paths[i], search_path);
        if (access(buffer, F_OK) == 0) {
            return STRINGS_STRDUP(buffer);
        }
    }
    
    // Also check current directory for .swiftmodule
    snprintf(buffer, sizeof(buffer), "%s.swiftmodule", path);
    if (access(buffer, F_OK) == 0) {
        return STRINGS_STRDUP(buffer);
    }
    
    return NULL;
}

// Module loader creation with hierarchy support
ModuleLoader* module_loader_create_with_hierarchy(ModuleLoaderType type, const char* name, ModuleLoader* parent, VM* vm) {
    LOG_DEBUG(LOG_MODULE_MODULE_LOADER, "Creating module loader: type=%d, name=%s, parent=%p", 
              type, name ? name : "(null)", (void*)parent);
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    ModuleLoader* loader = MODULES_NEW_ZERO(ModuleLoader);
    loader->type = type;
    loader->name = name ? STRINGS_STRDUP(name) : NULL;
    loader->parent = parent;
    loader->vm = vm;
    
    // Initialize module cache
    loader->cache = module_cache_create();
    MODULE_DEBUG("Initialized module cache\n");
    
    // Initialize search paths
    loader->search_paths.capacity = 8;
    loader->search_paths.paths = MODULES_NEW_ARRAY_ZERO(char*, loader->search_paths.capacity);
    
    // Only create package system for application loaders
    if (type == MODULE_LOADER_APPLICATION) {
        loader->package_system = package_system_create(vm);
        package_system_load_root(loader->package_system, "module.json");
        package_init_stdlib_namespace(loader->package_system);
    }
    
    return loader;
}

// Standard module loader creation
ModuleLoader* module_loader_create(VM* vm) {
    ModuleLoader* loader = module_loader_create_with_hierarchy(MODULE_LOADER_APPLICATION, "main", NULL, vm);
    
    // Initialize builtin modules
    builtin_modules_init();
    
    // Add current directory
    module_loader_add_search_path(loader, ".");
    
    // Add modules directory relative to current directory
    module_loader_add_search_path(loader, "./modules");
    module_loader_add_search_path(loader, "../modules");
    
    // Add src directory
    module_loader_add_search_path(loader, "./src");
    module_loader_add_search_path(loader, "src");
    
    // Add system module path
    module_loader_add_search_path(loader, "/usr/local/lib/swiftlang/modules");
    
    // Add user module path
    char* home = getenv("HOME");
    if (home) {
        char user_path[PATH_MAX];
        snprintf(user_path, sizeof(user_path), "%s/.swiftlang/modules", home);
        module_loader_add_search_path(loader, user_path);
    }
    
    return loader;
}

// Module loader destroy
void module_loader_destroy(ModuleLoader* loader) {
    if (!loader) return;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    // Don't destroy parent loaders
    
    // Free name
    if (loader->name) {
        size_t name_len = strlen(loader->name) + 1;
        STRINGS_FREE((char*)loader->name, name_len);
    }
    
    // Free cache
    // Note: modules are freed separately, cache just holds references
    module_cache_destroy(loader->cache);
    
    // Free search paths
    for (size_t i = 0; i < loader->search_paths.count; i++) {
        size_t path_len = strlen(loader->search_paths.paths[i]) + 1;
        STRINGS_FREE(loader->search_paths.paths[i], path_len);
    }
    MODULES_FREE(loader->search_paths.paths, loader->search_paths.capacity * sizeof(char*));
    
    // Free package system if present
    if (loader->package_system) {
        package_system_destroy(loader->package_system);
    }
    
    MODULES_FREE(loader, sizeof(ModuleLoader));
}


// Add search path to module loader
void module_loader_add_search_path(ModuleLoader* loader, const char* path) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    if (loader->search_paths.count >= loader->search_paths.capacity) {
        size_t old_capacity = loader->search_paths.capacity;
        loader->search_paths.capacity *= 2;
        
        char** new_paths = MODULES_NEW_ARRAY(char*, loader->search_paths.capacity);
        memcpy(new_paths, loader->search_paths.paths, old_capacity * sizeof(char*));
        MODULES_FREE(loader->search_paths.paths, old_capacity * sizeof(char*));
        loader->search_paths.paths = new_paths;
    }
    loader->search_paths.paths[loader->search_paths.count++] = STRINGS_STRDUP(path);
}


Module* module_get_cached(ModuleLoader* loader, const char* path) {
    // Check current loader's cache
    Module* module = module_cache_get(loader->cache, path);
    if (module) {
        return module;
    }
    
    // Not found, check parent loader if exists
    if (loader->parent) {
        return module_get_cached(loader->parent, path);
    }
    
    return NULL;
}

static void cache_module(ModuleLoader* loader, Module* module) {
    module_cache_put(loader->cache, module->path, module);
}

Module* module_load(ModuleLoader* loader, const char* path, bool is_native) {
    return module_load_relative(loader, path, is_native, NULL);
}

static bool load_compiled_module(ModuleLoader* loader, Module* module, ModuleMetadata* metadata) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    Allocator* bc_alloc = allocators_get(ALLOC_SYSTEM_BYTECODE);
    
    ModuleArchive* archive = module_archive_open(metadata->compiled_path);
    if (!archive) {
        return false;
    }
    
    // Extract native library if needed
    if (metadata->native.library) {
        char temp_lib_path[PATH_MAX];
        snprintf(temp_lib_path, sizeof(temp_lib_path), "/tmp/swiftlang_%s_%d.dylib",
                metadata->name, getpid());
        
        const char* platform = module_archive_get_platform();
        
        if (module_archive_extract_native_lib(archive, platform, temp_lib_path)) {
            module->native_handle = dlopen(temp_lib_path, RTLD_LAZY | RTLD_LOCAL);
            if (!module->native_handle) {
                // Failed to load extracted native library
            } else {
                // Native library loaded successfully
            }
            // Mark for cleanup
            module->temp_native_path = STRINGS_STRDUP(temp_lib_path);
        }
    }
    
    // Load bytecode modules
    size_t entry_count = module_archive_get_entry_count(archive);
    
    for (size_t i = 0; i < entry_count; i++) {
        const char* entry_name = module_archive_get_entry_name(archive, i);
        
        if (strstr(entry_name, "bytecode/") == entry_name && strstr(entry_name, ".swiftbc")) {
            // Extract module name
            char module_name[MODULE_NAME_BUFFER_SIZE];
            const char* start = entry_name + strlen("bytecode/");
            const char* end = strstr(start, ".swiftbc");
            size_t len = end - start;
            strncpy(module_name, start, len);
            module_name[len] = '\0';
            
            // Load bytecode
            uint8_t* bytecode;
            size_t bytecode_size;
            if (module_archive_extract_bytecode(archive, module_name, &bytecode, &bytecode_size)) {
                // Deserialize bytecode
                Chunk* chunk = BYTECODE_NEW(Chunk);
                chunk_init(chunk);
                
                // Read our simple format
                size_t offset = 0;
                uint32_t magic;
                memcpy(&magic, bytecode + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);
                
                if (magic == 0x42434453) { // "BCDS"
                    // Read constants
                    uint32_t const_count;
                    memcpy(&const_count, bytecode + offset, sizeof(uint32_t));
                    offset += sizeof(uint32_t);
                    
                    for (size_t i = 0; i < const_count; i++) {
                        TaggedValue value;
                        
                        // Read type
                        memcpy(&value.type, bytecode + offset, sizeof(ValueType));
                        offset += sizeof(ValueType);
                        
                        // Read value based on type
                        switch (value.type) {
                            case VAL_STRING: {
                                // Read string length
                                uint32_t string_len;
                                memcpy(&string_len, bytecode + offset, sizeof(uint32_t));
                                offset += sizeof(uint32_t);
                                
                                // Read string data
                                if (string_len > 0) {
                                    char* str = MODULES_ALLOC(string_len + 1);
                                    memcpy(str, bytecode + offset, string_len);
                                    str[string_len] = '\0';
                                    offset += string_len;
                                    
                                    // Intern the string
                                    value.as.string = string_pool_intern(&loader->vm->strings, str, string_len);
                                    MODULES_FREE(str, string_len + 1);
                                } else {
                                    value.as.string = string_pool_intern(&loader->vm->strings, "", 0);
                                }
                                break;
                            }
                            case VAL_NUMBER:
                                memcpy(&value.as.number, bytecode + offset, sizeof(double));
                                offset += sizeof(double);
                                break;
                            case VAL_BOOL:
                                memcpy(&value.as.boolean, bytecode + offset, sizeof(bool));
                                offset += sizeof(bool);
                                break;
                            case VAL_NIL:
                                // No data for nil
                                break;
                            default:
                                // For other types, read the raw value
                                memcpy(&value.as, bytecode + offset, sizeof(Value));
                                offset += sizeof(Value);
                                break;
                        }
                        
                        chunk_add_constant(chunk, value);
                    }
                    
                    // Read code
                    uint32_t code_size;
                    memcpy(&code_size, bytecode + offset, sizeof(uint32_t));
                    offset += sizeof(uint32_t);
                    
                    for (size_t i = 0; i < code_size; i++) {
                        chunk_write(chunk, bytecode[offset++], 0);
                    }
                    
                    // Execute the module code to populate exports
                    VM* vm = loader->vm;
                    
                    // Save VM state
                    Chunk* saved_chunk = vm->chunk;
                    
                    // Define __module_exports__ for this submodule
                    define_global(vm, "__module_exports__", OBJECT_VAL(module->module_object));
                    
                    // Execute bytecode
                    InterpretResult result;
                    // Skip empty chunks (just return)
                    if (chunk->count <= 2) {
                        result = INTERPRET_OK;
                    } else {
                        result = vm_interpret(vm, chunk);
                    }
                    
                    if (result != INTERPRET_OK) {
                        // Failed to execute module bytecode
                    }
                    
                    // Restore VM state
                    vm->chunk = saved_chunk;
                    undefine_global(vm, "__module_exports__");
                }
                
                // Free the temporary chunk - it was only used for immediate execution
                chunk_free(chunk);
                BYTECODE_FREE(chunk, sizeof(Chunk));
                MODULES_FREE(bytecode, bytecode_size);
            }
        }
    }
    
    module_archive_destroy(archive);
    return true;
}

Module* load_module_from_metadata(ModuleLoader* loader, ModuleMetadata* metadata) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    Module* module = MODULES_NEW_ZERO(Module);
    module->path = STRINGS_STRDUP(metadata->name);
    module->absolute_path = STRINGS_STRDUP(metadata->path);
    module->state = MODULE_STATE_LOADING;
    module->is_native = metadata->native.library != NULL;
    module->scope = module_scope_create();
    module->ref_count = 0;
    pthread_mutex_init(&module->ref_mutex, NULL);
    module->last_access_time = time(NULL);
    
    // Initialize exports
    module->exports.capacity = metadata->export_count + 8;
    module->exports.names = MODULES_NEW_ARRAY_ZERO(char*, module->exports.capacity);
    module->exports.values = MODULES_NEW_ARRAY_ZERO(TaggedValue, module->exports.capacity);
    module->exports.visibility = MODULES_NEW_ARRAY_ZERO(uint8_t, module->exports.capacity);
    
    // Create module object early
    module->module_object = object_create();
    
    // Load native library if needed
    void* native_handle = NULL;
    if (metadata->native.library) {
        native_handle = package_load_native_library(metadata);
        if (!native_handle) {
            fprintf(stderr, "Failed to load native library for module %s\n", metadata->name);
            module->state = MODULE_STATE_ERROR;
            return module;
        }
        module->native_handle = native_handle;
    }
    
    // Process exports from metadata
    for (size_t i = 0; i < metadata->export_count; i++) {
        ModuleExport* exp = &metadata->exports[i];
        
        switch (exp->type) {
            case MODULE_EXPORT_CONSTANT:
                module_export(module, exp->name, exp->constant_value);
                break;
                
            case MODULE_EXPORT_FUNCTION:
                if (exp->native_name && native_handle) {
                    // Look up native function
                    void* sym = dlsym(native_handle, exp->native_name);
                    if (sym) {
                        NativeFn fn = (NativeFn)sym;
                        module_export(module, exp->name, NATIVE_VAL(fn));
                    } else {
                        const char* error = dlerror();
                        fprintf(stderr, "Native function %s not found in library: %s\n", exp->native_name, error ? error : "unknown error");
                    }
                }
                break;
                
            default:
                // Other export types need bytecode support
                break;
        }
    }
    
    // Load from compiled .swiftmodule if available
    if (metadata->compiled_path) {
        if (!load_compiled_module(loader, module, metadata)) {
            fprintf(stderr, "Failed to load compiled module from %s\n", metadata->compiled_path);
        }
    }
    
    // Update module object with exports
    for (size_t i = 0; i < module->exports.count; i++) {
        object_set_property(module->module_object, module->exports.names[i], 
                          module->exports.values[i]);
    }
    
    module->state = MODULE_STATE_LOADED;
    return module;
}

/**
 * Load a module with path resolution relative to a base path.
 * 
 * This is the main entry point for module loading. It handles:
 * - Cache lookup to avoid reloading modules
 * - Path resolution (absolute, relative, package-based)
 * - Different module types (.swiftmodule archives, source files, native libraries)
 * - Module initialization and registration
 * 
 * The function follows this resolution order:
 * 1. Check module cache
 * 2. Try package system resolution
 * 3. Resolve relative/absolute paths
 * 4. Load based on file type (.swiftmodule, .swift, or native)
 * 
 * @param loader Module loader instance
 * @param path Module path or name to load
 * @param is_native True if loading a native module
 * @param relative_to Optional base path for relative imports
 * @return Loaded Module* or NULL on failure
 */
Module* module_load_relative(ModuleLoader* loader, const char* path, bool is_native, const char* relative_to) {
    LOG_DEBUG(LOG_MODULE_MODULE_LOADER, "Loading module: path=%s, is_native=%d, relative_to=%s", 
              path, is_native, relative_to ? relative_to : "(null)");
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    Allocator* bc_alloc = allocators_get(ALLOC_SYSTEM_BYTECODE);
    Allocator* ast_alloc = allocators_get(ALLOC_SYSTEM_AST);
    
    if (getenv("SWIFTLANG_DEBUG")) {
        // fprintf(stderr, "[DEBUG] module_load_relative called with path: %s, is_native: %d\n", path, is_native);
    }
    
    // Check cache first
    Module* cached = module_get_cached(loader, path);
    if (cached) {
        LOG_TRACE(LOG_MODULE_MODULE_LOADER, "Returning cached module %p for %s with state=%d", 
                  cached, path, cached->state);
        if (getenv("SWIFTLANG_DEBUG")) {
            fprintf(stderr, "[DEBUG] Returning cached module for %s with state=%d, exports=%zu\n", 
                    path, cached->state, cached->exports.count);
        }
        
        // Check for circular dependency
        if (cached->state == MODULE_STATE_LOADING) {
            fprintf(stderr, "Circular dependency detected: module '%s' is already being loaded\n", path);
            return NULL;
        }
        
        return cached;
    }
    
    // Try to load through package system
    if (loader->package_system) {
        // Check if this is a submodule path (e.g., "stb/math")
        const char* slash = strchr(path, '/');
        if (slash) {
            // Extract package name and module name
            size_t pkg_len = slash - path;
            char* pkg_name = MODULES_ALLOC(pkg_len + 1);
            strncpy(pkg_name, path, pkg_len);
            pkg_name[pkg_len] = '\0';
            
            const char* module_name = slash + 1;
            
            // Get package metadata
            ModuleMetadata* metadata = package_get_module_metadata(loader->package_system, pkg_name);
            if (metadata && metadata->module_count > 0) {
                // Load specific module from package
                Module* module = package_load_module_from_metadata(loader, metadata, module_name);
                if (module) {
                    cache_module(loader, module);
                    MODULES_FREE(pkg_name, pkg_len + 1);
                    return module;
                }
            }
            MODULES_FREE(pkg_name, pkg_len + 1);
        } else {
            // Try regular package loading
            ModuleMetadata* metadata = package_get_module_metadata(loader->package_system, path);
            if (metadata) {
                Module* module = load_module_from_metadata(loader, metadata);
                if (module) {
                    cache_module(loader, module);
                    return module;
                }
            }
        }
    }
    
    // Check for installed modules in cache before resolving path
    // Check local cache first
    char cache_pattern[PATH_MAX];
    snprintf(cache_pattern, sizeof(cache_pattern), ".cache/%s-*.swiftmodule", path);
    
    glob_t glob_result;
    if (glob(cache_pattern, GLOB_NOSORT, NULL, &glob_result) == 0) {
        if (glob_result.gl_pathc > 0) {
            // Found in local cache - load from archive
            const char* archive_path = glob_result.gl_pathv[0];
            
            // Create a module that will load from archive
            Module* module = module_load_from_archive(loader, archive_path, path);
            globfree(&glob_result);
            
            if (module) {
                cache_module(loader, module);
                return module;
            }
        }
        globfree(&glob_result);
    }
    
    // Check global cache
    const char* home = getenv("HOME");
    if (home) {
        snprintf(cache_pattern, sizeof(cache_pattern), "%s/.swiftlang/cache/%s-*.swiftmodule", home, path);
        
        if (glob(cache_pattern, GLOB_NOSORT, NULL, &glob_result) == 0) {
            if (glob_result.gl_pathc > 0) {
                // Found in global cache - load from archive
                const char* archive_path = glob_result.gl_pathv[0];
                
                Module* module = module_load_from_archive(loader, archive_path, path);
                globfree(&glob_result);
                
                if (module) {
                    cache_module(loader, module);
                    return module;
                }
            }
            globfree(&glob_result);
        }
    }
    
    // Resolve module path
    char* absolute_path = resolve_module_path(loader, path, relative_to);
    if (!absolute_path) {
        fprintf(stderr, "Module not found: %s\n", path);
        return NULL;
    }
    
    // Create module
    Module* module = MODULES_NEW_ZERO(Module);
    module->path = STRINGS_STRDUP(path);
    module->absolute_path = absolute_path;
    module->state = MODULE_STATE_LOADING;
    module->is_native = is_native;
    module->scope = module_scope_create();
    module->ref_count = 0;
    pthread_mutex_init(&module->ref_mutex, NULL);
    module->last_access_time = time(NULL);
    
    // Initialize exports
    module->exports.capacity = 16;
    module->exports.names = MODULES_NEW_ARRAY_ZERO(char*, module->exports.capacity);
    module->exports.values = MODULES_NEW_ARRAY_ZERO(TaggedValue, module->exports.capacity);
    module->exports.visibility = MODULES_NEW_ARRAY_ZERO(uint8_t, module->exports.capacity);
    
    // Cache the module early to handle circular dependencies
    cache_module(loader, module);
    
    if (is_native) {
        // Load native module directly
        module->native_handle = dlopen(absolute_path, RTLD_LAZY);
        if (!module->native_handle) {
            fprintf(stderr, "Failed to load native module %s: %s\n", absolute_path, dlerror());
            module->state = MODULE_STATE_ERROR;
            return NULL;
        }
        
        // Look for init function
        typedef bool (*ModuleInitFn)(Module*);
        
        // Try module-specific init function first (e.g., swiftlang_math_native_module_init)
        char init_fn_name[MODULE_NAME_BUFFER_SIZE];
        const char* module_name = path;
        
        // Skip prefixes
        if (module_name[0] == '$') module_name++;
        
        // Convert dots to underscores for function name
        snprintf(init_fn_name, sizeof(init_fn_name), "swiftlang_");
        size_t pos = strlen(init_fn_name);
        for (const char* p = module_name; *p && pos < sizeof(init_fn_name) - 20; p++) {
            if (*p == '.') {
                init_fn_name[pos++] = '_';
            } else {
                init_fn_name[pos++] = *p;
            }
        }
        strcpy(init_fn_name + pos, "_module_init");
        
        // Try module-specific init first
        ModuleInitFn init_fn = (ModuleInitFn)dlsym(module->native_handle, init_fn_name);
        
        // Fall back to generic init if not found
        if (!init_fn) {
            init_fn = (ModuleInitFn)dlsym(module->native_handle, "swiftlang_module_init");
        }
        
        if (!init_fn) {
            fprintf(stderr, "Native module %s missing init function (tried %s and swiftlang_module_init)\n", 
                    path, init_fn_name);
            module->state = MODULE_STATE_ERROR;
            return NULL;
        }
        
        // Initialize the module
        if (!init_fn(module)) {
            fprintf(stderr, "Native module %s initialization failed\n", path);
            module->state = MODULE_STATE_ERROR;
            return NULL;
        }
        
        // Mark module as loaded
        module->state = MODULE_STATE_LOADED;
    }
    
    if (!is_native) {
        // Check if this is a .swiftmodule archive
        size_t path_len = strlen(absolute_path);
        if (path_len > 12 && strcmp(absolute_path + path_len - 12, ".swiftmodule") == 0) {
            // Load from .swiftmodule archive
            if (getenv("SWIFTLANG_DEBUG")) {
                fprintf(stderr, "[DEBUG] Loading from .swiftmodule archive: %s\n", absolute_path);
            }
            Module* loaded = module_load_from_archive(loader, absolute_path, path);
            if (loaded) {
                // Update the cached module with loaded data
                module->exports = loaded->exports;
                module->module_object = loaded->module_object;
                module->native_handle = loaded->native_handle;
                module->state = loaded->state;
                module->is_native = loaded->is_native;
                module->globals = loaded->globals;
                size_t path_len = strlen(loaded->path) + 1;
                STRINGS_FREE((void*)loaded->path, path_len);
                size_t abs_path_len = strlen(loaded->absolute_path) + 1;
                STRINGS_FREE((void*)loaded->absolute_path, abs_path_len);
                MODULES_FREE(loaded, sizeof(Module));
                size_t abs_path_len2 = strlen(absolute_path) + 1;
                STRINGS_FREE(absolute_path, abs_path_len2);
                return module;
            }
            fprintf(stderr, "Failed to load module from archive: %s\n", absolute_path);
            module->state = MODULE_STATE_ERROR;
            return NULL;
        }
        
        // Variables for file loading
        char* source = NULL;
        char* file_path_to_load = NULL;
        
        // Check if absolute_path is a directory
        struct stat path_stat;
        if (stat(absolute_path, &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
            // It's a directory, load module.json to get main file
            char module_json_path[PATH_MAX];
            snprintf(module_json_path, sizeof(module_json_path), "%s/module.json", absolute_path);
            
            ModuleMetadata* dir_metadata = package_load_module_metadata(module_json_path);
            if (!dir_metadata) {
                fprintf(stderr, "Failed to load module.json from: %s\n", module_json_path);
                module->state = MODULE_STATE_ERROR;
                return NULL;
            }
            
            // Initialize exports if not already done
            if (!module->exports.names && dir_metadata->export_count > 0) {
                module->exports.capacity = dir_metadata->export_count + 8;
                module->exports.names = MODULES_NEW_ARRAY_ZERO(char*, module->exports.capacity);
                module->exports.values = MODULES_NEW_ARRAY_ZERO(TaggedValue, module->exports.capacity);
                module->exports.visibility = MODULES_NEW_ARRAY_ZERO(uint8_t, module->exports.capacity);
            }
            
            // Load native library if specified in metadata
            if (dir_metadata->native.library) {
                void* native_handle = package_load_native_library(dir_metadata);
                if (!native_handle) {
                    fprintf(stderr, "Failed to load native library for module %s\n", dir_metadata->name);
                    module->state = MODULE_STATE_ERROR;
                    return NULL;
                }
                module->native_handle = native_handle;
                
                // Process exports from metadata to define native functions
                for (size_t i = 0; i < dir_metadata->export_count; i++) {
                    ModuleExport* exp = &dir_metadata->exports[i];
                    
                    switch (exp->type) {
                        case MODULE_EXPORT_CONSTANT:
                            // Export constant value
                            module_export(module, exp->name, exp->constant_value);
                            break;
                            
                        case MODULE_EXPORT_FUNCTION:
                            if (exp->native_name) {
                                // Look up native function
                                void* sym = dlsym(native_handle, exp->native_name);
                                if (sym) {
                                    NativeFn fn = (NativeFn)sym;
                                    // Define the native function as a global in the module VM
                                    // This allows Swift code to reference it
                                    define_global(loader->vm, exp->name, NATIVE_VAL(fn));
                                    
                                    // Also add to module exports
                                    module_export(module, exp->name, NATIVE_VAL(fn));
                                    
                                    if (getenv("SWIFTLANG_DEBUG")) {
                                        // fprintf(stderr, "[DEBUG] Defined native function global: %s\n", exp->name);
                                    }
                                } else {
                                    const char* error = dlerror();
                                    fprintf(stderr, "Native function %s not found in library: %s\n", 
                                            exp->native_name, error ? error : "unknown error");
                                }
                            }
                            break;
                            
                        default:
                            // Other export types need bytecode support
                            break;
                    }
                }
            }
            
            // Build path to main file
            char main_path[PATH_MAX];
            const char* main_file = dir_metadata->main_file ? dir_metadata->main_file : "main.swift";
            snprintf(main_path, sizeof(main_path), "%s/%s", absolute_path, main_file);
            
            // Free metadata
            package_free_module_metadata(dir_metadata);
            
            file_path_to_load = STRINGS_STRDUP(main_path);
            
            if (getenv("SWIFTLANG_DEBUG")) {
                // fprintf(stderr, "[DEBUG] Loading module main file: %s\n", file_path_to_load);
            }
        } else {
            // Not a directory, load as regular file
            file_path_to_load = STRINGS_STRDUP(absolute_path);
        }
        
        // Load the file (either main file from directory or direct file)
        FILE* file = fopen(file_path_to_load, "r");
        if (!file) {
            fprintf(stderr, "Failed to open module file: %s\n", file_path_to_load);
            size_t file_path_len = strlen(file_path_to_load) + 1;
            STRINGS_FREE(file_path_to_load, file_path_len);
            module->state = MODULE_STATE_ERROR;
            return NULL;
        }
        
        // Read file contents
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        source = MODULES_ALLOC(file_size + 1);
        if (!source) {
            fprintf(stderr, "Failed to allocate memory for module source\n");
            fclose(file);
            size_t file_path_len = strlen(file_path_to_load) + 1;
            STRINGS_FREE(file_path_to_load, file_path_len);
            module->state = MODULE_STATE_ERROR;
            return NULL;
        }
        
        size_t read_size = fread(source, 1, file_size, file);
        source[read_size] = '\0';
        fclose(file);
        
        if (getenv("SWIFTLANG_DEBUG")) {
            // fprintf(stderr, "[DEBUG] Read %ld bytes of module source from %s\n", read_size, file_path_to_load);
        }
        
        size_t file_path_len = strlen(file_path_to_load) + 1;
        STRINGS_FREE(file_path_to_load, file_path_len);
        
        // Get file modification time for cache invalidation
        struct stat file_stat_cache;
        if (stat(absolute_path, &file_stat_cache) != 0) {
            file_stat_cache.st_mtime = 0;
        }
        
        // Check for cached bytecode
        char cache_path[PATH_MAX];
        const char* home = getenv("HOME");
        bool loaded_from_cache = false;
        Chunk* module_chunk = BYTECODE_NEW(Chunk);
        chunk_init(module_chunk);
        
        if (home) {
            // Create cache directory if it doesn't exist
            char cache_dir[PATH_MAX];
            snprintf(cache_dir, sizeof(cache_dir), "%s/.swiftlang/cache", home);
            mkdir(cache_dir, 0755);
            
            // Generate cache filename based on module path and mtime
            char* path_copy = STRINGS_STRDUP(absolute_path);
            char* module_name = basename(path_copy);
            snprintf(cache_path, sizeof(cache_path), "%s/%s-%ld.swiftbc", 
                     cache_dir, module_name, file_stat_cache.st_mtime);
            size_t path_copy_len = strlen(path_copy) + 1;
            STRINGS_FREE(path_copy, path_copy_len);
            
            // Try to load from cache
            FILE* cache_file = fopen(cache_path, "rb");
            if (cache_file) {
                fseek(cache_file, 0, SEEK_END);
                size_t cache_size = ftell(cache_file);
                fseek(cache_file, 0, SEEK_SET);
                
                uint8_t* cache_data = MODULES_ALLOC(cache_size);
                if (fread(cache_data, 1, cache_size, cache_file) == cache_size) {
                    if (bytecode_deserialize(cache_data, cache_size, module_chunk)) {
                        loaded_from_cache = true;
                        if (getenv("SWIFTLANG_DEBUG")) {
                            fprintf(stderr, "[DEBUG] Loaded module from cache: %s\n", cache_path);
                        }
                    }
                }
                MODULES_FREE(cache_data, cache_size);
                fclose(cache_file);
            }
        }
        
        // If not loaded from cache, parse and compile
        if (!loaded_from_cache) {
            // Parse the module source
            Lexer* lexer = lexer_create(source);
            Parser* parser = parser_create(source);
            
            ProgramNode* program = parser_parse_program(parser);
            if (parser->had_error) {
                fprintf(stderr, "Failed to parse module: %s\n", absolute_path);
                ast_free_program(program);
                lexer_destroy(lexer);
                parser_destroy(parser);
                MODULES_FREE(source, file_size + 1);
                chunk_free(module_chunk);
                BYTECODE_FREE(module_chunk, sizeof(Chunk));
                module->state = MODULE_STATE_ERROR;
                return NULL;
            }
            
            if (getenv("SWIFTLANG_DEBUG")) {
                // fprintf(stderr, "[DEBUG] Module parsed successfully with %zu statements\n", 
                //         program ? program->statement_count : 0);
            }
            
            // Compile the module
            if (getenv("SWIFTLANG_DEBUG")) {
                // fprintf(stderr, "[DEBUG] Compiling module source...\n");
            }
            if (!compile(program, module_chunk)) {
                fprintf(stderr, "Failed to compile module: %s\n", absolute_path);
                ast_free_program(program);
                chunk_free(module_chunk);
                BYTECODE_FREE(module_chunk, sizeof(Chunk));
                lexer_destroy(lexer);
                parser_destroy(parser);
                MODULES_FREE(source, file_size + 1);
                module->state = MODULE_STATE_ERROR;
                return NULL;
            }
            // fprintf(stderr, "[DEBUG] Module compiled successfully\n");
            
            // Save to cache if we have a cache path
            if (home) {
                uint8_t* bytecode_data;
                size_t bytecode_size;
                if (bytecode_serialize(module_chunk, &bytecode_data, &bytecode_size)) {
                    FILE* cache_file = fopen(cache_path, "wb");
                    if (cache_file) {
                        fwrite(bytecode_data, 1, bytecode_size, cache_file);
                        fclose(cache_file);
                        if (getenv("SWIFTLANG_DEBUG")) {
                            fprintf(stderr, "[DEBUG] Saved module to cache: %s\n", cache_path);
                        }
                    }
                    MODULES_FREE(bytecode_data, bytecode_size);
                }
            }
            
            // Clean up parser resources
            ast_free_program(program);
            lexer_destroy(lexer);
            parser_destroy(parser);
        }
        
        // Execute the module to populate exports
        VM* vm = loader->vm;
        
        // Create module exports object
        module->module_object = object_create();
        
        // Save VM state for module execution
        int saved_frame_count = vm->frame_count;
        TaggedValue* saved_stack_top = vm->stack_top;
        const char* saved_module_path = vm->current_module_path;
        Chunk* saved_chunk = vm->chunk;
        
        // Set current module path for relative imports
        vm->current_module_path = module->absolute_path;
        
        // Create a new VM instance for module execution
        VM module_vm;
        vm_init_with_loader(&module_vm, loader);
        
        // Copy necessary state
        module_vm.current_module_path = module->absolute_path;
        module_vm.module_loader = loader;  // Share the loader for now
        module_vm.current_module = module; // Set the module context
        
        // Define __module_exports__ in the module VM for export statements to use
        define_global(&module_vm, "__module_exports__", OBJECT_VAL(module->module_object));
        
        // Execute the module in its own VM
        if (getenv("SWIFTLANG_DEBUG")) {
            // fprintf(stderr, "[DEBUG] Executing module in separate VM: %s\n", module->absolute_path);
            // fprintf(stderr, "[DEBUG] Module chunk has %zu bytes of bytecode\n", module_chunk->count);
        }
        InterpretResult result = vm_interpret(&module_vm, module_chunk);
        
        if (getenv("SWIFTLANG_DEBUG")) {
            // fprintf(stderr, "[DEBUG] vm_interpret returned: %d for module: %s\n", result, module->absolute_path);
        }
        
        if (result != INTERPRET_OK) {
            fprintf(stderr, "Failed to execute module: %s (result=%d)\n", absolute_path, result);
            module->state = MODULE_STATE_ERROR;
            vm_free(&module_vm);
        } else {
            // Success - module executed successfully
            // The exports are now in module->module_object via OP_MODULE_EXPORT
            // Copy them to the exports array for compatibility
            
            // Iterate over module object properties and add to exports array
            if (module->module_object) {
                if (getenv("SWIFTLANG_DEBUG")) {
                    printf("DEBUG: Extracting exports from module object\n");
                }
                ObjectProperty* prop = module->module_object->properties;
                int prop_count = 0;
                while (prop) {
                    prop_count++;
                    if (getenv("SWIFTLANG_DEBUG")) {
                        printf("DEBUG: Found property: %s\n", prop->key);
                    }
                    // If the property is a function, ensure it has a reference to this module
                    if (prop->value && IS_FUNCTION(*prop->value)) {
                        AS_FUNCTION(*prop->value)->module = module;
                    }
                    // Add to exports array
                    if (module->exports.count >= module->exports.capacity) {
                        size_t old_capacity = module->exports.capacity;
                        module->exports.capacity *= 2;
                        
                        char** new_names = MODULES_NEW_ARRAY(char*, module->exports.capacity);
                        memcpy(new_names, module->exports.names, old_capacity * sizeof(char*));
                        MODULES_FREE(module->exports.names, old_capacity * sizeof(char*));
                        module->exports.names = new_names;
                        
                        TaggedValue* new_values = MODULES_NEW_ARRAY(TaggedValue, module->exports.capacity);
                        memcpy(new_values, module->exports.values, old_capacity * sizeof(TaggedValue));
                        MODULES_FREE(module->exports.values, old_capacity * sizeof(TaggedValue));
                        module->exports.values = new_values;
                    }
                    module->exports.names[module->exports.count] = STRINGS_STRDUP(prop->key);
                    module->exports.values[module->exports.count] = *prop->value;
                    
                    // If exporting a function, ensure it has a reference to this module
                    if (IS_FUNCTION(module->exports.values[module->exports.count])) {
                        AS_FUNCTION(module->exports.values[module->exports.count])->module = module;
                    }
                    
                    module->exports.count++;
                    
                    prop = prop->next;
                }
                if (getenv("SWIFTLANG_DEBUG")) {
                    printf("DEBUG: Total properties extracted: %d, exports count: %zu\n", prop_count, module->exports.count);
                }
            } else {
                if (getenv("SWIFTLANG_DEBUG")) {
                    printf("DEBUG: module->module_object is NULL!\n");
                }
            }
            
            module->state = MODULE_STATE_LOADED;
            
            // Copy module globals before destroying the VM
            module->globals.count = module_vm.globals.count;
            module->globals.capacity = module_vm.globals.capacity;
            module->globals.names = MODULES_NEW_ARRAY(char*, module->globals.capacity);
            module->globals.values = MODULES_NEW_ARRAY(TaggedValue, module->globals.capacity);
            
            for (size_t i = 0; i < module->globals.count; i++) {
                module->globals.names[i] = STRINGS_STRDUP(module_vm.globals.names[i]);
                module->globals.values[i] = module_vm.globals.values[i];
            }
            
            // Clean up module VM - but don't destroy the shared module loader!
            module_vm.module_loader = NULL;  // Prevent vm_free from destroying it
            vm_free(&module_vm);
        }
        
        // Restore VM state
        vm->frame_count = saved_frame_count;
        vm->stack_top = saved_stack_top;
        vm->current_module_path = saved_module_path;
        vm->chunk = saved_chunk;
        
        // Clean up
        chunk_free(module_chunk);
        BYTECODE_FREE(module_chunk, sizeof(Chunk));
        MODULES_FREE(source, file_size + 1);
    }
    
    return module;
}

Module* module_load_native(ModuleLoader* loader, const char* path) {
    return module_load(loader, path, true);
}

// Module export with visibility support
void module_export_with_visibility(Module* module, const char* name, TaggedValue value, uint8_t visibility) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    // Check if already exported
    for (size_t i = 0; i < module->exports.count; i++) {
        if (strcmp(module->exports.names[i], name) == 0) {
            module->exports.values[i] = value;
            module->exports.visibility[i] = visibility;
            // Also update module object
            if (visibility > 0) { // public
                object_set_property(module->module_object, name, value);
            }
            return;
        }
    }
    
    // Add new export
    if (module->exports.count >= module->exports.capacity) {
        size_t old_capacity = module->exports.capacity;
        module->exports.capacity *= 2;
        
        char** new_names = MODULES_NEW_ARRAY(char*, module->exports.capacity);
        memcpy(new_names, module->exports.names, old_capacity * sizeof(char*));
        MODULES_FREE(module->exports.names, old_capacity * sizeof(char*));
        module->exports.names = new_names;
        
        TaggedValue* new_values = MODULES_NEW_ARRAY(TaggedValue, module->exports.capacity);
        memcpy(new_values, module->exports.values, old_capacity * sizeof(TaggedValue));
        MODULES_FREE(module->exports.values, old_capacity * sizeof(TaggedValue));
        module->exports.values = new_values;
        
        uint8_t* new_visibility = MODULES_NEW_ARRAY(uint8_t, module->exports.capacity);
        memcpy(new_visibility, module->exports.visibility, old_capacity * sizeof(uint8_t));
        MODULES_FREE(module->exports.visibility, old_capacity * sizeof(uint8_t));
        module->exports.visibility = new_visibility;
    }
    
    module->exports.names[module->exports.count] = STRINGS_STRDUP(name);
    module->exports.values[module->exports.count] = value;
    module->exports.visibility[module->exports.count] = visibility;
    module->exports.count++;
    
    // Also add to module object if public
    if (visibility > 0) {
        object_set_property(module->module_object, name, value);
    }
}

// Standard module export (defaults to public)
void module_export(Module* module, const char* name, TaggedValue value) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    // Check if already exported
    for (size_t i = 0; i < module->exports.count; i++) {
        if (strcmp(module->exports.names[i], name) == 0) {
            module->exports.values[i] = value;
            return;
        }
    }
    
    // Add new export
    if (module->exports.count >= module->exports.capacity) {
        size_t old_capacity = module->exports.capacity;
        module->exports.capacity *= 2;
        
        char** new_names = MODULES_NEW_ARRAY(char*, module->exports.capacity);
        memcpy(new_names, module->exports.names, old_capacity * sizeof(char*));
        MODULES_FREE(module->exports.names, old_capacity * sizeof(char*));
        module->exports.names = new_names;
        
        TaggedValue* new_values = MODULES_NEW_ARRAY(TaggedValue, module->exports.capacity);
        memcpy(new_values, module->exports.values, old_capacity * sizeof(TaggedValue));
        MODULES_FREE(module->exports.values, old_capacity * sizeof(TaggedValue));
        module->exports.values = new_values;
    }
    
    module->exports.names[module->exports.count] = STRINGS_STRDUP(name);
    module->exports.values[module->exports.count] = value;
    module->exports.count++;
}

TaggedValue module_get_export(Module* module, const char* name) {
    for (size_t i = 0; i < module->exports.count; i++) {
        if (strcmp(module->exports.names[i], name) == 0) {
            return module->exports.values[i];
        }
    }
    return NIL_VAL;
}

bool module_has_export(Module* module, const char* name) {
    for (size_t i = 0; i < module->exports.count; i++) {
        if (strcmp(module->exports.names[i], name) == 0) {
            return true;
        }
    }
    return false;
}

void module_register_native_function(Module* module, const char* name, NativeFn fn) {
    module_export(module, name, NATIVE_VAL(fn));
}

// Example standard library initialization
void module_loader_init_stdlib(ModuleLoader* loader) {
    (void)loader;  // Unused parameter
    // This would load built-in modules like:
    // - math
    // - string
    // - array
    // - file
    // - etc.
}

/**
 * Check if a module version satisfies a requirement.
 * @param required_version The version requirement (e.g., ">=1.0.0", "~>2.0")
 * @param module_version The actual module version
 * @return true if compatible, false otherwise
 */
static bool check_module_version_compatibility(const char* required_version, const char* module_version) {
    if (!required_version || !module_version) {
        // If no version specified, assume compatibility
        return true;
    }
    
    // Use version_satisfies from version.h
    return version_satisfies(module_version, required_version);
}

/**
 * Public API for checking module version compatibility.
 */
bool module_check_version_compatibility(const char* required_version, const char* module_version) {
    return check_module_version_compatibility(required_version, module_version);
}