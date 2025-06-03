#include "runtime/core/vm.h"
#include "runtime/core/object.h"

// Simple native function
static TaggedValue native_add(int arg_count, TaggedValue* args) {
    if (arg_count != 2) {
        return NIL_VAL;
    }
    
    if (!IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
        return NIL_VAL;
    }
    
    double a = AS_NUMBER(args[0]);
    double b = AS_NUMBER(args[1]);
    return NUMBER_VAL(a + b);
}

// Module initialization
void native_test_init(VM* vm) {
    // Create module object
    Object* module = object_create();
    
    // Add native function
    object_set_property(module, "add", NATIVE_VAL(native_add));
    
    // Register the module
    define_global(vm, "native_test", OBJECT_VAL(module));
}