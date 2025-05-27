#ifndef MODULE_H
#define MODULE_H

#include <stdbool.h>
#include "vm/vm.h"
#include "ast/ast.h"
#include "runtime/package.h"

typedef enum {
    MODULE_STATE_UNLOADED,
    MODULE_STATE_LOADING,
    MODULE_STATE_LOADED,
    MODULE_STATE_ERROR
} ModuleState;

// Module scope entry with visibility flag
typedef struct ModuleScopeEntry {
    const char* name;
    TaggedValue value;
    bool is_exported;  // true if exported, false if module-private
} ModuleScopeEntry;

// Module scope hashtable for fast lookup
typedef struct ModuleScope {
    ModuleScopeEntry* entries;
    size_t count;
    size_t capacity;
} ModuleScope;

typedef struct Module {
    const char* path;
    const char* absolute_path;
    ModuleState state;
    
    // Module scope (all definitions with export flags)
    ModuleScope* scope;
    
    // Module exports (public interface)
    struct {
        char** names;
        TaggedValue* values;
        uint8_t* visibility;  // 0 = private, 1 = public
        size_t count;
        size_t capacity;
    } exports;
    
    // Module object for storing exports
    Object* module_object;
    
    // Module globals (preserved after module execution)
    struct {
        char** names;
        TaggedValue* values;
        size_t count;
        size_t capacity;
    } globals;
    
    // For native modules
    bool is_native;
    void* native_handle;  // dlopen handle
    char* temp_native_path; // Temporary extracted native library path
    
    // Module initialization function for native modules
    bool (*init_fn)(struct Module* module);
} Module;

// Module loader types for hierarchy
typedef enum {
    MODULE_LOADER_BOOTSTRAP,    // Root loader for built-ins
    MODULE_LOADER_SYSTEM,       // System/stdlib modules  
    MODULE_LOADER_APPLICATION,  // User application modules
    MODULE_LOADER_CHILD        // Dynamic child loaders
} ModuleLoaderType;

typedef struct ModuleLoader {
    // Loader hierarchy
    ModuleLoaderType type;
    const char* name;
    struct ModuleLoader* parent;  // Parent loader for delegation
    
    // Loaded modules cache
    struct {
        Module** modules;
        size_t count;
        size_t capacity;
    } cache;
    
    // Module search paths
    struct {
        char** paths;
        size_t count;
        size_t capacity;
    } search_paths;
    
    VM* vm;
    
    // Package system integration
    PackageSystem* package_system;
} ModuleLoader;

// Module loader functions
ModuleLoader* module_loader_create(VM* vm);
ModuleLoader* module_loader_create_with_hierarchy(ModuleLoaderType type, const char* name, ModuleLoader* parent, VM* vm);
void module_loader_destroy(ModuleLoader* loader);
void module_loader_add_search_path(ModuleLoader* loader, const char* path);

// Module loading
Module* module_load(ModuleLoader* loader, const char* path, bool is_native);
Module* module_load_relative(ModuleLoader* loader, const char* path, bool is_native, const char* relative_to);
Module* module_get_cached(ModuleLoader* loader, const char* path);

// Module scope functions
ModuleScope* module_scope_create(void);
void module_scope_destroy(ModuleScope* scope);
void module_scope_define(ModuleScope* scope, const char* name, TaggedValue value, bool is_exported);
TaggedValue module_scope_get(ModuleScope* scope, const char* name);
bool module_scope_has(ModuleScope* scope, const char* name);
bool module_scope_is_exported(ModuleScope* scope, const char* name);

// Module exports
void module_export(Module* module, const char* name, TaggedValue value);
void module_export_with_visibility(Module* module, const char* name, TaggedValue value, uint8_t visibility);
TaggedValue module_get_export(Module* module, const char* name);
bool module_has_export(Module* module, const char* name);

// Module scope lookup (for functions within the module)
TaggedValue module_get_from_scope(Module* module, const char* name);
bool module_has_in_scope(Module* module, const char* name);

// Native module support
Module* module_load_native(ModuleLoader* loader, const char* path);
void module_register_native_function(Module* module, const char* name, NativeFn fn);

// Standard library modules
void module_loader_init_stdlib(ModuleLoader* loader);

#endif
