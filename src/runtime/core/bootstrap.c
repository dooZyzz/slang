#include "runtime/core/bootstrap.h"
#include "runtime/modules/loader/module_loader.h"
#include "runtime/modules/loader/module_cache.h"
#include "runtime/core/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Built-in native functions
static TaggedValue native_print(int arg_count, TaggedValue* args) {
    for (int i = 0; i < arg_count; i++) {
        if (i > 0) printf(" ");
        print_value(args[i]);
    }
    printf("\n");
    return NIL_VAL;
}

static TaggedValue native_typeof(int arg_count, TaggedValue* args) {
    if (arg_count != 1) {
        return STRING_VAL("error: typeof expects 1 argument");
    }
    
    switch (args[0].type) {
        case VAL_BOOL:     return STRING_VAL("bool");
        case VAL_NIL:      return STRING_VAL("nil");
        case VAL_NUMBER:   return STRING_VAL("number");
        case VAL_STRING:   return STRING_VAL("string");
        case VAL_FUNCTION: return STRING_VAL("function");
        case VAL_NATIVE:   return STRING_VAL("native");
        case VAL_CLOSURE:  return STRING_VAL("closure");
        case VAL_OBJECT:   return STRING_VAL("object");
        case VAL_STRUCT:   return STRING_VAL("struct");
        default:           return STRING_VAL("unknown");
    }
}

static TaggedValue native_assert(int arg_count, TaggedValue* args) {
    if (arg_count < 1) {
        fprintf(stderr, "Assertion failed: assert() requires at least 1 argument\n");
        exit(1);
    }
    
    bool condition = !IS_NIL(args[0]) && (!IS_BOOL(args[0]) || AS_BOOL(args[0]));
    if (!condition) {
        if (arg_count > 1 && IS_STRING(args[1])) {
            fprintf(stderr, "Assertion failed: %s\n", AS_STRING(args[1]));
        } else {
            fprintf(stderr, "Assertion failed\n");
        }
        exit(1);
    }
    
    return NIL_VAL;
}

// Create the built-ins module
Module* bootstrap_create_builtins_module(VM* vm) {
    Module* module = calloc(1, sizeof(Module));
    module->path = strdup("__builtins__");
    module->absolute_path = strdup("<built-in>");
    module->state = MODULE_STATE_LOADED;
    module->is_native = false;
    module->scope = module_scope_create();
    
    // Initialize exports with visibility
    module->exports.capacity = 16;
    module->exports.names = calloc(module->exports.capacity, sizeof(char*));
    module->exports.values = calloc(module->exports.capacity, sizeof(TaggedValue));
    module->exports.visibility = calloc(module->exports.capacity, sizeof(uint8_t));
    
    // Create module object
    module->module_object = object_create();
    
    // Add built-in functions (all public)
    module->exports.names[0] = strdup("print");
    module->exports.values[0] = NATIVE_VAL(native_print);
    module->exports.visibility[0] = 1; // public
    
    module->exports.names[1] = strdup("typeof");
    module->exports.values[1] = NATIVE_VAL(native_typeof);
    module->exports.visibility[1] = 1; // public
    
    module->exports.names[2] = strdup("assert");
    module->exports.values[2] = NATIVE_VAL(native_assert);
    module->exports.visibility[2] = 1; // public
    
    module->exports.count = 3;
    
    // Also add to module object for property access
    object_set_property(module->module_object, "print", NATIVE_VAL(native_print));
    object_set_property(module->module_object, "typeof", NATIVE_VAL(native_typeof));
    object_set_property(module->module_object, "assert", NATIVE_VAL(native_assert));
    
    // Add to module scope as exported
    module_scope_define(module->scope, "print", NATIVE_VAL(native_print), true);
    module_scope_define(module->scope, "typeof", NATIVE_VAL(native_typeof), true);
    module_scope_define(module->scope, "assert", NATIVE_VAL(native_assert), true);
    
    return module;
}

// Create the bootstrap loader
ModuleLoader* bootstrap_loader_create(VM* vm) {
    ModuleLoader* loader = module_loader_create_with_hierarchy(MODULE_LOADER_BOOTSTRAP, "bootstrap", NULL, vm);
    
    // Create and cache the built-ins module
    Module* builtins = bootstrap_create_builtins_module(vm);
    
    // Add to cache using the cache API
    module_cache_put(loader->cache, "__builtins__", builtins);
    
    return loader;
}