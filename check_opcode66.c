#include "include/runtime/core/vm.h"
#include <stdio.h>

int main() {
    printf("Checking opcodes around 66:\n");
    printf("OP_JUMP = %d\n", OP_JUMP);
    printf("OP_JUMP_IF_FALSE = %d\n", OP_JUMP_IF_FALSE);
    printf("OP_JUMP_IF_TRUE = %d\n", OP_JUMP_IF_TRUE);
    printf("OP_LOOP = %d\n", OP_LOOP);
    printf("OP_FUNCTION = %d\n", OP_FUNCTION);
    printf("OP_CLOSURE = %d\n", OP_CLOSURE);
    printf("OP_CALL = %d\n", OP_CALL);
    printf("OP_METHOD_CALL = %d\n", OP_METHOD_CALL);
    printf("OP_RETURN = %d\n", OP_RETURN);
    printf("OP_LOAD_BUILTIN = %d\n", OP_LOAD_BUILTIN);
    printf("OP_ARRAY = %d\n", OP_ARRAY);
    printf("OP_BUILD_ARRAY = %d\n", OP_BUILD_ARRAY);
    printf("OP_GET_SUBSCRIPT = %d\n", OP_GET_SUBSCRIPT);
    printf("OP_SET_SUBSCRIPT = %d\n", OP_SET_SUBSCRIPT);
    printf("OP_CREATE_OBJECT = %d\n", OP_CREATE_OBJECT);
    printf("OP_GET_PROPERTY = %d\n", OP_GET_PROPERTY);
    printf("OP_SET_PROPERTY = %d\n", OP_SET_PROPERTY);
    printf("OP_SET_PROTOTYPE = %d\n", OP_SET_PROTOTYPE);
    printf("OP_DEFINE_STRUCT = %d\n", OP_DEFINE_STRUCT);
    printf("OP_CREATE_STRUCT = %d\n", OP_CREATE_STRUCT);
    printf("OP_GET_FIELD = %d\n", OP_GET_FIELD);
    printf("OP_SET_FIELD = %d\n", OP_SET_FIELD);
    printf("OP_GET_OBJECT_PROTO = %d\n", OP_GET_OBJECT_PROTO);
    printf("OP_GET_STRUCT_PROTO = %d\n", OP_GET_STRUCT_PROTO);
    printf("OP_OPTIONAL_CHAIN = %d\n", OP_OPTIONAL_CHAIN);
    printf("OP_FORCE_UNWRAP = %d\n", OP_FORCE_UNWRAP);
    printf("OP_GET_ITER = %d\n", OP_GET_ITER);
    printf("OP_FOR_ITER = %d\n", OP_FOR_ITER);
    printf("OP_DEFINE_LOCAL = %d\n", OP_DEFINE_LOCAL);
    printf("OP_AWAIT = %d\n", OP_AWAIT);
    printf("OP_LOAD_MODULE = %d\n", OP_LOAD_MODULE);
    printf("OP_LOAD_NATIVE_MODULE = %d\n", OP_LOAD_NATIVE_MODULE);
    return 0;
}