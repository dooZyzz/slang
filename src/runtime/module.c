#include "runtime/module.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dlfcn.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <glob.h>
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "vm/vm.h"
#include "ast/ast.h"
#include "runtime/builtin_modules.h"
#include "debug/debug.h"
#include "runtime/module_archive.h"
#include "runtime/module_format.h"
#include "runtime/package.h"
#include "utils/bytecode_format.h"
#include "utils/logger.h"

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
    ModuleScope* scope = calloc(1, sizeof(ModuleScope));
    scope->capacity = 16;
    scope->entries = calloc(scope->capacity, sizeof(ModuleScopeEntry));
    return scope;
}

void module_scope_destroy(ModuleScope* scope) {
    if (!scope) return;
    
    for (size_t i = 0; i < scope->capacity; i++) {
        if (scope->entries[i].name) {
            free((void*)scope->entries[i].name);
        }
    }
    free(scope->entries);
    free(scope);
}

static void module_scope_grow(ModuleScope* scope) {
    size_t old_capacity = scope->capacity;
    ModuleScopeEntry* old_entries = scope->entries;
    
    scope->capacity *= 2;
    scope->entries = calloc(scope->capacity, sizeof(ModuleScopeEntry));
    scope->count = 0;
    
    // Rehash existing entries
    for (size_t i = 0; i < old_capacity; i++) {
        if (old_entries[i].name) {
            module_scope_define(scope, old_entries[i].name, old_entries[i].value, old_entries[i].is_exported);
            free((void*)old_entries[i].name);
        }
    }
    
    free(old_entries);
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
    scope->entries[index].name = strdup(name);
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

// Load a module from a .swiftmodule archive
static Module* module_load_from_archive(ModuleLoader* loader, const char* archive_path, const char* module_name) {
    // Create module
    Module* module = calloc(1, sizeof(Module));
    module->path = strdup(module_name);
    module->absolute_path = strdup(archive_path);
    module->state = MODULE_STATE_LOADING;
    module->is_native = false;
    
    // Initialize exports
    module->exports.capacity = 16;
    module->exports.names = calloc(module->exports.capacity, sizeof(char*));
    module->exports.values = calloc(module->exports.capacity, sizeof(TaggedValue));
    module->exports.visibility = calloc(module->exports.capacity, sizeof(uint8_t));
    
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
    
    // We don't need to parse the full metadata for now
    free(json_content);
    
    // Find the main module bytecode (usually named after the package)
    uint8_t* bytecode = NULL;
    size_t bytecode_size = 0;
    
    // Try with swift. prefix first
    char bytecode_name[256];
    snprintf(bytecode_name, sizeof(bytecode_name), "swift.%s", module_name);
    // fprintf(stderr, "[DEBUG] Looking for bytecode: %s\n", bytecode_name);
    
    if (!module_archive_extract_bytecode(archive, bytecode_name, &bytecode, &bytecode_size)) {
        // fprintf(stderr, "[DEBUG] Failed with swift prefix, trying without\n");
        
        // Try without the swift. prefix
        if (!module_archive_extract_bytecode(archive, module_name, &bytecode, &bytecode_size)) {
            fprintf(stderr, "Failed to extract module bytecode: %s\n", module_name);
            module_archive_destroy(archive);
            module->state = MODULE_STATE_ERROR;
            return module;
        }
    }
    
    // fprintf(stderr, "[DEBUG] Successfully extracted bytecode, size: %zu\n", bytecode_size);
    
    // Deserialize and execute the bytecode
    Chunk* chunk = malloc(sizeof(Chunk));
    chunk_init(chunk);
    
    // Use the proper bytecode deserialization
    // fprintf(stderr, "[DEBUG] About to deserialize bytecode of size %zu\n", bytecode_size);
    if (!bytecode_deserialize(bytecode, bytecode_size, chunk)) {
        fprintf(stderr, "Failed to deserialize module bytecode\n");
        free(bytecode);
        chunk_free(chunk);
        free(chunk);
        module_archive_destroy(archive);
        module->state = MODULE_STATE_ERROR;
        return module;
    }
    // fprintf(stderr, "[DEBUG] Bytecode deserialized successfully\n");
    
    free(bytecode);
    
    // Execute module in VM context
    VM* vm = loader->vm;
    
    // Save VM state
    Chunk* saved_chunk = vm->chunk;
    const char* saved_module_path = vm->current_module_path;
    
    // Set current module path
    vm->current_module_path = module->path;
    
    // Define __module_exports__
    define_global(vm, "__module_exports__", OBJECT_VAL(module->module_object));
    
    // Execute bytecode
    // fprintf(stderr, "[DEBUG] Executing module bytecode for: %s (chunk has %zu bytes)\n", module_name, chunk->count);
    
    // Debug: Print raw bytecode
    // fprintf(stderr, "[DEBUG] Module bytecode (%zu bytes): ", chunk->count);
    // for (size_t i = 0; i < chunk->count; i++) {
    //     fprintf(stderr, "%02x ", chunk->code[i]);
    // }
    // fprintf(stderr, "\n");
    
    // Debug: Print constants
    // fprintf(stderr, "[DEBUG] Constants:\n");
    // for (size_t i = 0; i < chunk->constants.count; i++) {
    //     fprintf(stderr, "[DEBUG] Constant %zu: ", i);
    //     TaggedValue* val = &chunk->constants.values[i];
    //     if (val->type == VAL_STRING) {
    //         fprintf(stderr, "STRING \"%s\"\n", val->as.string);
    //     } else if (val->type == VAL_NUMBER) {
    //         fprintf(stderr, "NUMBER %f\n", val->as.number);
    //     } else if (val->type == VAL_NIL) {
    //         fprintf(stderr, "NIL\n");
    //     } else if (val->type == VAL_BOOL) {
    //         fprintf(stderr, "BOOL %s\n", val->as.boolean ? "true" : "false");
    //     } else {
    //         fprintf(stderr, "OTHER type=%d\n", val->type);
    //     }
    // }
    
    // Execute module by creating a separate VM instance
    // fprintf(stderr, "[DEBUG] Creating separate VM for module execution\n");
    
    VM module_vm;
    vm_init_with_loader(&module_vm, loader);
    
    // Copy necessary state
    module_vm.current_module_path = module->path;
    module_vm.module_loader = loader;
    module_vm.current_module = module; // Set the module context
    
    // Define __module_exports__ for export statements
    define_global(&module_vm, "__module_exports__", OBJECT_VAL(module->module_object));
    
    // fprintf(stderr, "[DEBUG] About to call vm_interpret in separate VM\n");
    InterpretResult result = vm_interpret(&module_vm, chunk);
    // fprintf(stderr, "[DEBUG] vm_interpret returned: %d\n", result);
    
    if (result == INTERPRET_OK) {
        // Copy module globals before destroying the VM
        module->globals.count = module_vm.globals.count;
        module->globals.capacity = module_vm.globals.capacity;
        module->globals.names = malloc(sizeof(char*) * module->globals.capacity);
        module->globals.values = malloc(sizeof(TaggedValue) * module->globals.capacity);
        
        for (size_t i = 0; i < module->globals.count; i++) {
            module->globals.names[i] = strdup(module_vm.globals.names[i]);
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
        
        // Debug: Check module exports
        // fprintf(stderr, "[DEBUG] Module object properties:\n");
        if (module->module_object) {
            // This would need a proper object property iteration
            // fprintf(stderr, "[DEBUG] Module object exists\n");
        }
    }
    
    // Restore VM state
    vm->chunk = saved_chunk;
    vm->current_module_path = saved_module_path;
    undefine_global(vm, "__module_exports__");
    
    // fprintf(stderr, "[DEBUG] Module execution completed, cleaning up\n");
    
    // Clean up
    chunk_free(chunk);
    free(chunk);
    module_archive_destroy(archive);
    
    return module;
}

static char* resolve_module_path(ModuleLoader* loader, const char* path, const char* relative_to) {
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
                return strdup(buffer);
            }
            
            // Try as .swiftmodule archive
            snprintf(buffer, sizeof(buffer), "%s/%s.swiftmodule", loader->search_paths.paths[i], module_name);
            if (access(buffer, F_OK) == 0) {
                if (getenv("SWIFTLANG_DEBUG")) {
                    printf("DEBUG: Found .swiftmodule at %s\n", buffer);
                }
                return strdup(buffer);
            }
            
            // Try in modules subdirectory
            snprintf(buffer, sizeof(buffer), "%s/modules/%s.swiftmodule", loader->search_paths.paths[i], module_name);
            if (access(buffer, F_OK) == 0) {
                if (getenv("SWIFTLANG_DEBUG")) {
                    printf("DEBUG: Found .swiftmodule in modules/ at %s\n", buffer);
                }
                return strdup(buffer);
            }
            
            // Try as .swift file
            snprintf(buffer, sizeof(buffer), "%s/%s.swift", loader->search_paths.paths[i], module_name);
            if (access(buffer, F_OK) == 0) {
                if (getenv("SWIFTLANG_DEBUG")) {
                    printf("DEBUG: Found .swift file at %s\n", buffer);
                }
                return strdup(buffer);
            }
        }
        
        // Also check current directory
        char buffer[PATH_MAX];
        snprintf(buffer, sizeof(buffer), "%s.swiftmodule", module_name);
        if (access(buffer, F_OK) == 0) {
            if (getenv("SWIFTLANG_DEBUG")) {
                printf("DEBUG: Found .swiftmodule in current dir at %s\n", buffer);
            }
            return strdup(buffer);
        }
        
        // Check modules subdirectory in current directory
        snprintf(buffer, sizeof(buffer), "modules/%s.swiftmodule", module_name);
        if (access(buffer, F_OK) == 0) {
            if (getenv("SWIFTLANG_DEBUG")) {
                printf("DEBUG: Found .swiftmodule in modules/ subdir at %s\n", buffer);
            }
            return strdup(buffer);
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
        return strdup(path + 1);  // Skip $ and return just the module name
    }
    
    // If path is absolute, use it directly
    if (path[0] == '/') {
        return strdup(path);
    }
    
    char buffer[PATH_MAX];
    char converted_path[PATH_MAX];
    
    // Convert dotted path to file path (e.g., sys.native.io -> sys/native/io)
    size_t j = 0;
    for (size_t i = 0; path[i] != '\0' && j < PATH_MAX - 1; i++) {
        if (path[i] == '.') {
            converted_path[j++] = '/';
        } else {
            converted_path[j++] = path[i];
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
                    return strdup(buffer);
                }
                
                // Try with .swift extension
                snprintf(buffer, sizeof(buffer), "%s/%s.swift", dir, path);
                if (access(buffer, F_OK) == 0) {
                    return strdup(buffer);
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
                return strdup(buffer);
            }
        }
        
        // Try as regular Swift module with converted path
        snprintf(buffer, sizeof(buffer), "%s/%s", loader->search_paths.paths[i], converted_path);
        if (access(buffer, F_OK) == 0) {
            return strdup(buffer);
        }
        
        // Try with .swift extension
        snprintf(buffer, sizeof(buffer), "%s/%s.swift", loader->search_paths.paths[i], converted_path);
        if (access(buffer, F_OK) == 0) {
            return strdup(buffer);
        }
        
        // Also try with original dotted path for backward compatibility
        snprintf(buffer, sizeof(buffer), "%s/%s.swift", loader->search_paths.paths[i], path);
        if (access(buffer, F_OK) == 0) {
            return strdup(buffer);
        }
        
        // Try as .swiftmodule archive
        snprintf(buffer, sizeof(buffer), "%s/%s.swiftmodule", loader->search_paths.paths[i], path);
        if (access(buffer, F_OK) == 0) {
            return strdup(buffer);
        }
    }
    
    // Also check current directory for .swiftmodule
    snprintf(buffer, sizeof(buffer), "%s.swiftmodule", path);
    if (access(buffer, F_OK) == 0) {
        return strdup(buffer);
    }
    
    return NULL;
}

// Module loader creation with hierarchy support
ModuleLoader* module_loader_create_with_hierarchy(ModuleLoaderType type, const char* name, ModuleLoader* parent, VM* vm) {
    LOG_DEBUG(LOG_MODULE_MODULE_LOADER, "Creating module loader: type=%d, name=%s, parent=%p", 
              type, name ? name : "(null)", (void*)parent);
    // fprintf(stderr, "[DEBUG] module_loader_create_with_hierarchy called: type=%d, name=%s, parent=%p, vm=%p\n",
    //         type, name ? name : "(null)", parent, vm);
    
    ModuleLoader* loader = calloc(1, sizeof(ModuleLoader));
    loader->type = type;
    loader->name = name ? strdup(name) : NULL;
    loader->parent = parent;
    loader->vm = vm;
    
    // fprintf(stderr, "[DEBUG] Created loader=%p with type=%d, name=%s\n", 
    //         loader, loader->type, loader->name ? loader->name : "(null)");
    
    // Initialize cache
    loader->cache.capacity = 16;
    loader->cache.modules = calloc(loader->cache.capacity, sizeof(Module*));
    // fprintf(stderr, "[DEBUG] Initialized cache with capacity=%zu\n", loader->cache.capacity);
    
    // Initialize search paths
    loader->search_paths.capacity = 8;
    loader->search_paths.paths = calloc(loader->search_paths.capacity, sizeof(char*));
    // fprintf(stderr, "[DEBUG] Initialized search paths with capacity=%zu\n", loader->search_paths.capacity);
    
    // Only create package system for application loaders
    if (type == MODULE_LOADER_APPLICATION) {
        // fprintf(stderr, "[DEBUG] Creating package system for APPLICATION loader\n");
        loader->package_system = package_system_create(vm);
        // fprintf(stderr, "[DEBUG] Package system created: %p\n", loader->package_system);
        
        // fprintf(stderr, "[DEBUG] Loading root module.json\n");
        package_system_load_root(loader->package_system, "module.json");
        
        // fprintf(stderr, "[DEBUG] Initializing stdlib namespace\n");
        package_init_stdlib_namespace(loader->package_system);
    } else {
        // fprintf(stderr, "[DEBUG] Skipping package system creation for loader type %d\n", type);
    }
    
    // fprintf(stderr, "[DEBUG] module_loader_create_with_hierarchy returning loader=%p\n", loader);
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
    
    // Don't destroy parent loaders
    
    // Free name
    if (loader->name) {
        free((char*)loader->name);
    }
    
    // Free cache
    for (size_t i = 0; i < loader->cache.count; i++) {
        // TODO: Properly free modules
    }
    free(loader->cache.modules);
    
    // Free search paths
    for (size_t i = 0; i < loader->search_paths.count; i++) {
        free(loader->search_paths.paths[i]);
    }
    free(loader->search_paths.paths);
    
    // Free package system if present
    if (loader->package_system) {
        package_system_destroy(loader->package_system);
    }
    
    free(loader);
}


// Add search path to module loader
void module_loader_add_search_path(ModuleLoader* loader, const char* path) {
    if (loader->search_paths.count >= loader->search_paths.capacity) {
        loader->search_paths.capacity *= 2;
        loader->search_paths.paths = realloc(loader->search_paths.paths, 
                                            loader->search_paths.capacity * sizeof(char*));
    }
    loader->search_paths.paths[loader->search_paths.count++] = strdup(path);
}


Module* module_get_cached(ModuleLoader* loader, const char* path) {
    // Check current loader's cache
    for (size_t i = 0; i < loader->cache.count; i++) {
        if (strcmp(loader->cache.modules[i]->path, path) == 0) {
            return loader->cache.modules[i];
        }
    }
    
    // Not found, check parent loader if exists
    if (loader->parent) {
        return module_get_cached(loader->parent, path);
    }
    
    return NULL;
}

static void cache_module(ModuleLoader* loader, Module* module) {
    if (loader->cache.count >= loader->cache.capacity) {
        loader->cache.capacity *= 2;
        loader->cache.modules = realloc(loader->cache.modules,
                                       loader->cache.capacity * sizeof(Module*));
    }
    
    loader->cache.modules[loader->cache.count++] = module;
}

Module* module_load(ModuleLoader* loader, const char* path, bool is_native) {
    return module_load_relative(loader, path, is_native, NULL);
}

static bool load_compiled_module(ModuleLoader* loader, Module* module, ModuleMetadata* metadata) {
    // fprintf(stderr, "[DEBUG] Loading compiled module from: %s\n", metadata->compiled_path);
    
    ModuleArchive* archive = module_archive_open(metadata->compiled_path);
    if (!archive) {
        // fprintf(stderr, "[DEBUG] Failed to open module archive: %s\n", metadata->compiled_path);
        return false;
    }
    
    // fprintf(stderr, "[DEBUG] Successfully opened module archive\n");
    
    // Extract native library if needed
    if (metadata->native.library) {
        // fprintf(stderr, "[DEBUG] Module has native library: %s\n", metadata->native.library);
        
        char temp_lib_path[PATH_MAX];
        snprintf(temp_lib_path, sizeof(temp_lib_path), "/tmp/swiftlang_%s_%d.dylib",
                metadata->name, getpid());
        
        const char* platform = module_archive_get_platform();
        // fprintf(stderr, "[DEBUG] Extracting native library for platform: %s to %s\n", platform, temp_lib_path);
        
        if (module_archive_extract_native_lib(archive, platform, temp_lib_path)) {
            // fprintf(stderr, "[DEBUG] Native library extracted successfully\n");
            module->native_handle = dlopen(temp_lib_path, RTLD_LAZY | RTLD_LOCAL);
            if (!module->native_handle) {
                // fprintf(stderr, "[ERROR] Failed to load extracted native library: %s\n", dlerror());
            } else {
                // fprintf(stderr, "[DEBUG] Native library loaded successfully\n");
            }
            // Mark for cleanup
            module->temp_native_path = strdup(temp_lib_path);
        } else {
            // fprintf(stderr, "[ERROR] Failed to extract native library\n");
        }
    }
    
    // Load bytecode modules
    size_t entry_count = module_archive_get_entry_count(archive);
    // fprintf(stderr, "[DEBUG] Module archive has %zu entries\n", entry_count);
    
    for (size_t i = 0; i < entry_count; i++) {
        const char* entry_name = module_archive_get_entry_name(archive, i);
        // fprintf(stderr, "[DEBUG] Entry %zu: %s\n", i, entry_name);
        
        if (strstr(entry_name, "bytecode/") == entry_name && strstr(entry_name, ".swiftbc")) {
            // Extract module name
            char module_name[256];
            const char* start = entry_name + strlen("bytecode/");
            const char* end = strstr(start, ".swiftbc");
            size_t len = end - start;
            strncpy(module_name, start, len);
            module_name[len] = '\0';
            
            // fprintf(stderr, "[DEBUG] Found bytecode for module: %s\n", module_name);
            
            // Load bytecode
            uint8_t* bytecode;
            size_t bytecode_size;
            if (module_archive_extract_bytecode(archive, module_name, &bytecode, &bytecode_size)) {
                // fprintf(stderr, "[DEBUG] Loading bytecode for %s, size: %zu\n", module_name, bytecode_size);
                
                // Deserialize bytecode
                Chunk* chunk = malloc(sizeof(Chunk));
                chunk_init(chunk);
                
                // Read our simple format
                size_t offset = 0;
                uint32_t magic;
                memcpy(&magic, bytecode + offset, sizeof(uint32_t));
                offset += sizeof(uint32_t);
                
                if (magic == 0x42434453) { // "BCDS"
                    // fprintf(stderr, "[DEBUG] Valid bytecode magic found\n");
                    
                    // Read constants
                    uint32_t const_count;
                    memcpy(&const_count, bytecode + offset, sizeof(uint32_t));
                    offset += sizeof(uint32_t);
                    
                    // fprintf(stderr, "[DEBUG] Constant count: %u\n", const_count);
                    
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
                                    char* str = malloc(string_len + 1);
                                    memcpy(str, bytecode + offset, string_len);
                                    str[string_len] = '\0';
                                    offset += string_len;
                                    
                                    // Intern the string
                                    value.as.string = string_pool_intern(&loader->vm->strings, str, string_len);
                                    free(str);
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
                        
                        // Debug print the constant
                        // fprintf(stderr, "[DEBUG] Constant %zu: type=%d, ", i, value.type);
                        if (value.type == VAL_STRING) {
                            // fprintf(stderr, "string=%s\n", value.as.string ? value.as.string : "(null)");
                        } else if (value.type == VAL_NIL) {
                            // fprintf(stderr, "nil\n");
                        } else if (value.type == VAL_NUMBER) {
                            // fprintf(stderr, "number=%f\n", value.as.number);
                        } else {
                            // fprintf(stderr, "other\n");
                        }
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
                    
                    // fprintf(stderr, "[DEBUG] About to execute module bytecode for %s\n", module_name);
                    
                    // Save VM state
                    Chunk* saved_chunk = vm->chunk;
                    
                    // Define __module_exports__ for this submodule
                    if (!module->module_object) {
                        // fprintf(stderr, "[ERROR] module->module_object is NULL!\n");
                    } else {
                        // // fprintf(stderr, "[DEBUG] module->module_object is at %p\n", module->module_object);
                    }
                    define_global(vm, "__module_exports__", OBJECT_VAL(module->module_object));
                    
                    // // fprintf(stderr, "[DEBUG] Calling vm_interpret...\n");
                    // fprintf(stderr, "[DEBUG] Chunk has %zu bytes of code\n", chunk->count);
                    // fprintf(stderr, "[DEBUG] VM is at %p\n", (void*)vm);
                    // fprintf(stderr, "[DEBUG] Chunk is at %p\n", (void*)chunk);
                    
                    // Print first few opcodes for debugging
                    // fprintf(stderr, "[DEBUG] First opcodes:");
                    for (size_t i = 0; i < chunk->count && i < 20; i++) {
                        // fprintf(stderr, " %02x", chunk->code[i]);
                    }
                    // fprintf(stderr, "\n");
                    
                    // Validate opcodes reference valid constants
                    for (size_t i = 0; i < chunk->count; i++) {
                        uint8_t op = chunk->code[i];
                        if (op == 0x00 || op == 0x20 || op == 0x1e) { // OP_CONSTANT, OP_GET_GLOBAL, OP_DEFINE_LOCAL
                            if (i + 1 < chunk->count) {
                                uint8_t idx = chunk->code[i + 1];
                                if (idx >= chunk->constants.count) {
                                    // fprintf(stderr, "[ERROR] Opcode at %zu references invalid constant %d (max %zu)\n", 
                                    //         i, idx, chunk->constants.count);
                                }
                            }
                        }
                    }
                    
                    // Try to catch the crash
                    if (!vm) {
                        // fprintf(stderr, "[ERROR] VM is NULL!\n");
                        return false;
                    }
                    
                    if (!chunk || !chunk->code) {
                        // fprintf(stderr, "[ERROR] Chunk or chunk->code is NULL!\n");
                        return false;
                    }
                    
                    // Execute bytecode
                    InterpretResult result;
                    // Skip empty chunks (just return)
                    if (chunk->count <= 2) {
                        // fprintf(stderr, "[DEBUG] Skipping empty module bytecode\n");
                        result = INTERPRET_OK;
                    } else {
                        result = vm_interpret(vm, chunk);
                    }
                    
                    // fprintf(stderr, "[DEBUG] vm_interpret returned: %d\n", result);
                    
                    if (result != INTERPRET_OK) {
                        // fprintf(stderr, "[ERROR] Failed to execute module bytecode: %s (result=%d)\n", module_name, result);
                    } else {
                        // fprintf(stderr, "[DEBUG] Module bytecode executed successfully\n");
                    }
                    
                    // Restore VM state
                    vm->chunk = saved_chunk;
                    undefine_global(vm, "__module_exports__");
                }
                
                // Don't free the chunk yet - VM might still need it
                // TODO: proper chunk lifecycle management
                // chunk_free(chunk);
                // free(chunk);
                free(bytecode);
            }
        }
    }
    
    module_archive_destroy(archive);
    return true;
}

Module* load_module_from_metadata(ModuleLoader* loader, ModuleMetadata* metadata) {
    // Temporarily disable debug output
    // fprintf(stderr, "[DEBUG] load_module_from_metadata: %s\n", metadata->name);
    // fprintf(stderr, "[DEBUG]   Path: %s\n", metadata->path);
    // fprintf(stderr, "[DEBUG]   Compiled path: %s\n", metadata->compiled_path ? metadata->compiled_path : "(none)");
    // fprintf(stderr, "[DEBUG]   Native library: %s\n", metadata->native.library ? metadata->native.library : "(none)");
    // fprintf(stderr, "[DEBUG]   Export count: %zu\n", metadata->export_count);
    
    Module* module = calloc(1, sizeof(Module));
    module->path = strdup(metadata->name);
    module->absolute_path = strdup(metadata->path);
    module->state = MODULE_STATE_LOADING;
    module->is_native = metadata->native.library != NULL;
    module->scope = module_scope_create();
    
    // Initialize exports
    module->exports.capacity = metadata->export_count + 8;
    module->exports.names = calloc(module->exports.capacity, sizeof(char*));
    module->exports.values = calloc(module->exports.capacity, sizeof(TaggedValue));
    module->exports.visibility = calloc(module->exports.capacity, sizeof(uint8_t));
    
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
        
        // fprintf(stderr, "[DEBUG] Processing export %zu: %s (type=%d)\n", i, exp->name, exp->type);
        
        switch (exp->type) {
            case MODULE_EXPORT_CONSTANT:
                // fprintf(stderr, "[DEBUG] Exporting constant: %s\n", exp->name);
                module_export(module, exp->name, exp->constant_value);
                break;
                
            case MODULE_EXPORT_FUNCTION:
                if (exp->native_name && native_handle) {
                    // fprintf(stderr, "[DEBUG] Looking up native function: %s -> %s\n", exp->name, exp->native_name);
                    // Look up native function
                    void* sym = dlsym(native_handle, exp->native_name);
                    if (sym) {
                        // fprintf(stderr, "[DEBUG] Found native symbol '%s' at %p\n", exp->native_name, sym);
                        // dlsym returns the actual function pointer, not a GOT entry
                        // Use it directly without dereferencing
                        // fprintf(stderr, "[DEBUG] Final function pointer for '%s': %p\n", exp->name, sym);
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

Module* module_load_relative(ModuleLoader* loader, const char* path, bool is_native, const char* relative_to) {
    LOG_DEBUG(LOG_MODULE_MODULE_LOADER, "Loading module: path=%s, is_native=%d, relative_to=%s", 
              path, is_native, relative_to ? relative_to : "(null)");
    
    if (getenv("SWIFTLANG_DEBUG")) {
        // fprintf(stderr, "[DEBUG] module_load_relative called with path: %s, is_native: %d\n", path, is_native);
    }
    
    // Check cache first
    Module* cached = module_get_cached(loader, path);
    if (cached) {
        LOG_TRACE(LOG_MODULE_MODULE_LOADER, "Returning cached module %p for %s with state=%d", 
                  cached, path, cached->state);
        if (getenv("SWIFTLANG_DEBUG")) {
            // fprintf(stderr, "[DEBUG] Returning cached module %p for %s with state=%d\n", cached, path, cached->state);
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
            char* pkg_name = malloc(pkg_len + 1);
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
                    free(pkg_name);
                    return module;
                }
            }
            free(pkg_name);
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
            // fprintf(stderr, "[DEBUG] Found module in cache: %s\n", archive_path);
            
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
    // fprintf(stderr, "[DEBUG] Creating module for path: %s (absolute: %s, native: %s)\n", 
    //         path, absolute_path, is_native ? "yes" : "no");
    
    Module* module = calloc(1, sizeof(Module));
    module->path = strdup(path);
    module->absolute_path = absolute_path;
    module->state = MODULE_STATE_LOADING;
    module->is_native = is_native;
    module->scope = module_scope_create();
    // fprintf(stderr, "[DEBUG] Created module %p for %s\n", module, path);
    
    // Initialize exports
    module->exports.capacity = 16;
    module->exports.names = calloc(module->exports.capacity, sizeof(char*));
    module->exports.values = calloc(module->exports.capacity, sizeof(TaggedValue));
    module->exports.visibility = calloc(module->exports.capacity, sizeof(uint8_t));
    
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
        char init_fn_name[256];
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
        
        // fprintf(stderr, "[DEBUG] Native module loaded successfully: %s\n", path);
        
        // Mark module as loaded
        module->state = MODULE_STATE_LOADED;
    }
    
    if (!is_native) {
        // Check if this is a .swiftmodule archive
        size_t path_len = strlen(absolute_path);
        if (path_len > 12 && strcmp(absolute_path + path_len - 12, ".swiftmodule") == 0) {
            // Load from archive through package system
            ModuleMetadata* metadata = package_load_module_metadata(absolute_path);
            if (metadata) {
                Module* loaded = load_module_from_metadata(loader, metadata);
                if (loaded) {
                    // Update the cached module with loaded data
                    module->exports = loaded->exports;
                    module->module_object = loaded->module_object;
                    module->native_handle = loaded->native_handle;
                    module->state = loaded->state;
                    module->is_native = loaded->is_native;
                    free((void*)loaded->path);
                    free(loaded);
                    free(absolute_path);
                    return module;
                }
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
                module->exports.names = calloc(module->exports.capacity, sizeof(char*));
                module->exports.values = calloc(module->exports.capacity, sizeof(TaggedValue));
                module->exports.visibility = calloc(module->exports.capacity, sizeof(uint8_t));
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
            // TODO: implement package_metadata_free(dir_metadata);
            
            file_path_to_load = strdup(main_path);
            
            if (getenv("SWIFTLANG_DEBUG")) {
                // fprintf(stderr, "[DEBUG] Loading module main file: %s\n", file_path_to_load);
            }
        } else {
            // Not a directory, load as regular file
            file_path_to_load = strdup(absolute_path);
        }
        
        // Load the file (either main file from directory or direct file)
        FILE* file = fopen(file_path_to_load, "r");
        if (!file) {
            fprintf(stderr, "Failed to open module file: %s\n", file_path_to_load);
            free(file_path_to_load);
            module->state = MODULE_STATE_ERROR;
            return NULL;
        }
        
        // Read file contents
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        source = malloc(file_size + 1);
        if (!source) {
            fprintf(stderr, "Failed to allocate memory for module source\n");
            fclose(file);
            free(file_path_to_load);
            module->state = MODULE_STATE_ERROR;
            return NULL;
        }
        
        size_t read_size = fread(source, 1, file_size, file);
        source[read_size] = '\0';
        fclose(file);
        
        if (getenv("SWIFTLANG_DEBUG")) {
            // fprintf(stderr, "[DEBUG] Read %ld bytes of module source from %s\n", read_size, file_path_to_load);
        }
        
        free(file_path_to_load);
        
        // Parse the module source
        // fprintf(stderr, "[DEBUG] Parsing module source...\n");
        Lexer* lexer = lexer_create(source);
        Parser* parser = parser_create(source);
        
        ProgramNode* program = parser_parse_program(parser);
        if (parser->had_error) {
            fprintf(stderr, "Failed to parse module: %s\n", absolute_path);
            // TODO: ast_free_program(program);
            lexer_destroy(lexer);
            parser_destroy(parser);
            free(source);
            module->state = MODULE_STATE_ERROR;
            return NULL;
        }
        
        if (getenv("SWIFTLANG_DEBUG")) {
            // fprintf(stderr, "[DEBUG] Module parsed successfully with %zu statements\n", 
            //         program ? program->statement_count : 0);
        }
        
        // Create a chunk for the module
        Chunk* module_chunk = malloc(sizeof(Chunk));
        chunk_init(module_chunk);
        
        // Compile the module
        if (getenv("SWIFTLANG_DEBUG")) {
            // fprintf(stderr, "[DEBUG] Compiling module source...\n");
        }
        if (!compile(program, module_chunk)) {
            fprintf(stderr, "Failed to compile module: %s\n", absolute_path);
            // TODO: ast_free_program(program);
            chunk_free(module_chunk);
            free(module_chunk);
            lexer_destroy(lexer);
            parser_destroy(parser);
            free(source);
            module->state = MODULE_STATE_ERROR;
            return NULL;
        }
        // fprintf(stderr, "[DEBUG] Module compiled successfully\n");
        
        // Execute the module to populate exports
        VM* vm = loader->vm;
        
        // Create module exports object
        module->module_object = object_create();
        
        // Save VM state for module execution
        int saved_frame_count = vm->frame_count;
        TaggedValue* saved_stack_top = vm->stack_top;
        const char* saved_module_path = vm->current_module_path;
        Chunk* saved_chunk = vm->chunk;
        // int saved_global_count = vm->globals.count;  // Track globals before module execution
        
        
        // Set current module path for relative imports
        vm->current_module_path = module->absolute_path;
        
        // Create a new VM instance for module execution
        // // fprintf(stderr, "[DEBUG] Creating module VM...\n");
        VM module_vm;
        vm_init_with_loader(&module_vm, loader);
        // fprintf(stderr, "[DEBUG] module_vm at %p, module at %p\n", &module_vm, module);
        // fprintf(stderr, "[DEBUG] Before vm_init complete: module state=%d\n", module->state);
        
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
            
            // fprintf(stderr, "[DEBUG] Module executed successfully\n");
            
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
                        module->exports.capacity *= 2;
                        module->exports.names = realloc(module->exports.names, 
                                                       module->exports.capacity * sizeof(char*));
                        module->exports.values = realloc(module->exports.values,
                                                        module->exports.capacity * sizeof(TaggedValue));
                    }
                    module->exports.names[module->exports.count] = strdup(prop->key);
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
            // fprintf(stderr, "[DEBUG] Module loaded with %zu exports, state=%d\n", module->exports.count, module->state);
            
            // Copy module globals before destroying the VM
            // fprintf(stderr, "[DEBUG] Module VM has %zu globals total\n", module_vm.globals.count);
            for (size_t i = 0; i < module_vm.globals.count; i++) {
                // fprintf(stderr, "[DEBUG]   Global %zu: '%s'\n", i, module_vm.globals.names[i]);
            }
            
            module->globals.count = module_vm.globals.count;
            module->globals.capacity = module_vm.globals.capacity;
            module->globals.names = malloc(sizeof(char*) * module->globals.capacity);
            module->globals.values = malloc(sizeof(TaggedValue) * module->globals.capacity);
            
            for (size_t i = 0; i < module->globals.count; i++) {
                module->globals.names[i] = strdup(module_vm.globals.names[i]);
                module->globals.values[i] = module_vm.globals.values[i];
                // fprintf(stderr, "[DEBUG] Preserved global '%s'\n", module->globals.names[i]);
            }
            
            // fprintf(stderr, "[DEBUG] Preserved %zu module globals\n", module->globals.count);
            
            // fprintf(stderr, "[DEBUG] Before vm cleanup: module=%p state=%d\n", module, module->state);
            // Clean up module VM - but don't destroy the shared module loader!
            // Save the module loader before freeing
            // ModuleLoader* saved_loader = module_vm.module_loader;
            module_vm.module_loader = NULL;  // Prevent vm_free from destroying it
            vm_free(&module_vm);
            // Note: The module loader is still owned by the parent VM
            // fprintf(stderr, "[DEBUG] After vm cleanup: module=%p state=%d\n", module, module->state);
        }
        
        // Restore VM state
        vm->frame_count = saved_frame_count;
        vm->stack_top = saved_stack_top;
        vm->current_module_path = saved_module_path;
        vm->chunk = saved_chunk;
        
        // Clean up __module_exports__ global
        // undefine_global(vm, "__module_exports__");
        
        // Clean up
        // fprintf(stderr, "[DEBUG] Before chunk_free: module=%p state=%d\n", module, module->state);
        chunk_free(module_chunk);
        // fprintf(stderr, "[DEBUG] After chunk_free: module=%p state=%d\n", module, module->state);
        free(module_chunk);
        // fprintf(stderr, "[DEBUG] After free module_chunk: module=%p state=%d\n", module, module->state);
        // TODO: ast_free_program(program);
        lexer_destroy(lexer);
        parser_destroy(parser);
        free(source);
        // fprintf(stderr, "[DEBUG] After all cleanup: module=%p state=%d\n", module, module->state);
        
        // fprintf(stderr, "[DEBUG] Module cleanup complete\n");
    }
    
    // fprintf(stderr, "[DEBUG] About to return module: %p with state=%d\n", module, module ? module->state : -1);
    // fprintf(stderr, "[DEBUG] Returning module: %p with state=%d\n", module, module ? module->state : -1);
    return module;
}

Module* module_load_native(ModuleLoader* loader, const char* path) {
    return module_load(loader, path, true);
}

// Module export with visibility support
void module_export_with_visibility(Module* module, const char* name, TaggedValue value, uint8_t visibility) {
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
        module->exports.capacity *= 2;
        module->exports.names = realloc(module->exports.names,
                                       module->exports.capacity * sizeof(char*));
        module->exports.values = realloc(module->exports.values,
                                        module->exports.capacity * sizeof(TaggedValue));
        module->exports.visibility = realloc(module->exports.visibility,
                                           module->exports.capacity * sizeof(uint8_t));
    }
    
    module->exports.names[module->exports.count] = strdup(name);
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
    // Check if already exported
    for (size_t i = 0; i < module->exports.count; i++) {
        if (strcmp(module->exports.names[i], name) == 0) {
            module->exports.values[i] = value;
            return;
        }
    }
    
    // Add new export
    if (module->exports.count >= module->exports.capacity) {
        module->exports.capacity *= 2;
        module->exports.names = realloc(module->exports.names,
                                       module->exports.capacity * sizeof(char*));
        module->exports.values = realloc(module->exports.values,
                                        module->exports.capacity * sizeof(TaggedValue));
    }
    
    module->exports.names[module->exports.count] = strdup(name);
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
