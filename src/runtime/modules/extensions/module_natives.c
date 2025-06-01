#include "runtime/core/vm.h"
#include "runtime/core/object.h"
#include "runtime/modules/loader/module_loader.h"
#include "runtime/modules/extensions/module_inspect.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * Native functions for module system introspection.
 * These are available as built-in functions in SwiftLang.
 */

// Global VM pointer for native functions
static VM* g_vm = NULL;

// __current_module__: Get the current module object
static TaggedValue native_current_module(int arg_count, TaggedValue* args) {
    if (!g_vm || !g_vm->current_module) {
        return NIL_VAL;
    }
    
    // Return module as an object with metadata
    Object* mod_obj = object_create();
    object_set_property(mod_obj, "path", STRING_VAL(strdup(g_vm->current_module->path)));
    object_set_property(mod_obj, "version", 
        g_vm->current_module->version ? STRING_VAL(strdup(g_vm->current_module->version)) : NIL_VAL);
    object_set_property(mod_obj, "is_native", BOOL_VAL(g_vm->current_module->is_native));
    object_set_property(mod_obj, "is_lazy", BOOL_VAL(g_vm->current_module->chunk != NULL));
    // Store module pointer as a number (cast to uintptr_t)
    object_set_property(mod_obj, "_internal", NUMBER_VAL((double)(uintptr_t)g_vm->current_module));
    return OBJECT_VAL(mod_obj);
}

// __loaded_modules__(): Get array of all loaded modules
static TaggedValue native_loaded_modules(int arg_count, TaggedValue* args) {
    if (!g_vm || !g_vm->module_loader) {
        return OBJECT_VAL(array_create());
    }
    
    size_t count;
    Module** modules = module_get_all_loaded(g_vm->module_loader, &count);
    
    Object* array = array_create_with_capacity(count);
    
    for (size_t i = 0; i < count; i++) {
        Module* mod = modules[i];
        Object* mod_obj = object_create();
        object_set_property(mod_obj, "path", STRING_VAL(strdup(mod->path)));
        object_set_property(mod_obj, "version", 
            mod->version ? STRING_VAL(strdup(mod->version)) : NIL_VAL);
        object_set_property(mod_obj, "is_native", BOOL_VAL(mod->is_native));
        object_set_property(mod_obj, "is_lazy", BOOL_VAL(mod->chunk != NULL));
        object_set_property(mod_obj, "_internal", NUMBER_VAL((double)(uintptr_t)mod));
        array_push(array, OBJECT_VAL(mod_obj));
    }
    
    free(modules);
    return OBJECT_VAL(array);
}

// __module_exports__(module): Get module exports
static TaggedValue native_module_exports(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_OBJECT(args[0])) {
        return OBJECT_VAL(array_create());
    }
    
    Object* mod_obj = AS_OBJECT(args[0]);
    TaggedValue* internal = object_get_property(mod_obj, "_internal");
    if (!internal || !IS_NUMBER(*internal)) {
        return OBJECT_VAL(array_create());
    }
    
    Module* module = (Module*)(uintptr_t)AS_NUMBER(*internal);
    size_t count;
    ExportInfo* exports = module_get_exports(module, &count);
    
    Object* array = array_create_with_capacity(count);
    
    for (size_t i = 0; i < count; i++) {
        Object* exp_obj = object_create();
        object_set_property(exp_obj, "name", STRING_VAL(strdup(exports[i].name)));
        object_set_property(exp_obj, "type", STRING_VAL(strdup(exports[i].type_name)));
        object_set_property(exp_obj, "is_function", BOOL_VAL(exports[i].is_function));
        object_set_property(exp_obj, "is_constant", BOOL_VAL(exports[i].is_constant));
        array_push(array, OBJECT_VAL(exp_obj));
    }
    
    module_exports_free(exports, count);
    return OBJECT_VAL(array);
}

// __module_state__(module): Get module state as string
static TaggedValue native_module_state(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_OBJECT(args[0])) {
        return STRING_VAL(strdup("unknown"));
    }
    
    Object* mod_obj = AS_OBJECT(args[0]);
    TaggedValue* internal = object_get_property(mod_obj, "_internal");
    if (!internal || !IS_NUMBER(*internal)) {
        return STRING_VAL(strdup("unknown"));
    }
    
    Module* module = (Module*)(uintptr_t)AS_NUMBER(*internal);
    return STRING_VAL(strdup(module_state_to_string(module->state)));
}

// __module_stats__(module): Get module statistics
static TaggedValue native_module_stats(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_OBJECT(args[0])) {
        return NIL_VAL;
    }
    
    Object* mod_obj = AS_OBJECT(args[0]);
    TaggedValue* internal = object_get_property(mod_obj, "_internal");
    if (!internal || !IS_NUMBER(*internal)) {
        return NIL_VAL;
    }
    
    Module* module = (Module*)(uintptr_t)AS_NUMBER(*internal);
    ModuleStats* stats = module_get_stats(module);
    
    Object* stats_obj = object_create();
    object_set_property(stats_obj, "load_time_ms", NUMBER_VAL(stats->load_time_ms));
    object_set_property(stats_obj, "init_time_ms", NUMBER_VAL(stats->init_time_ms));
    object_set_property(stats_obj, "access_count", NUMBER_VAL(stats->access_count));
    object_set_property(stats_obj, "export_lookups", NUMBER_VAL(stats->export_lookups));
    object_set_property(stats_obj, "cache_hits", NUMBER_VAL(stats->cache_hits));
    object_set_property(stats_obj, "cache_misses", NUMBER_VAL(stats->cache_misses));
    
    module_stats_free(stats);
    return OBJECT_VAL(stats_obj);
}

// __module_info__: Get detailed module information as JSON
static TaggedValue native_module_info(int arg_count, TaggedValue* args) {
    bool include_exports = true;
    bool include_stats = true;
    
    if (arg_count > 0 && IS_OBJECT(args[0])) {
        Object* mod_obj = AS_OBJECT(args[0]);
        TaggedValue* internal = object_get_property(mod_obj, "_internal");
        if (internal && IS_NUMBER(*internal)) {
            Module* module = (Module*)(uintptr_t)AS_NUMBER(*internal);
            
            if (arg_count > 1 && IS_BOOL(args[1])) {
                include_exports = AS_BOOL(args[1]);
            }
            if (arg_count > 2 && IS_BOOL(args[2])) {
                include_stats = AS_BOOL(args[2]);
            }
            
            char* json = module_to_json(module, include_exports, include_stats);
            TaggedValue result = STRING_VAL(json);
            return result;
        }
    }
    
    return STRING_VAL(strdup("{}"));
}

// Register module introspection natives
void register_module_natives(VM* vm) {
    // Store VM pointer for native functions
    g_vm = vm;
    
    
    // Core introspection functions
    define_global(vm, "__current_module__", NATIVE_VAL(native_current_module));
    define_global(vm, "__loaded_modules__", NATIVE_VAL(native_loaded_modules));
    define_global(vm, "__module_exports__", NATIVE_VAL(native_module_exports));
    define_global(vm, "__module_state__", NATIVE_VAL(native_module_state));
    define_global(vm, "__module_stats__", NATIVE_VAL(native_module_stats));
    define_global(vm, "__module_info__", NATIVE_VAL(native_module_info));
}