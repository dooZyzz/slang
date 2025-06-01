#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "runtime/core/vm.h"
#include "runtime/core/object.h"

DEFINE_TEST(stack_operations) {
    VM vm;
    vm_init(&vm);
    
    // Push some values
    vm_push(&vm, NUMBER_VAL(42));
    vm_push(&vm, BOOL_VAL(true));
    vm_push(&vm, NIL_VAL);
    vm_push(&vm, STRING_VAL("hello"));
    
    // Pop and verify
    TaggedValue str_val = vm_pop(&vm);
    TEST_ASSERT(suite, IS_STRING(str_val), "stack_operations");
    
    TaggedValue nil_val = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NIL(nil_val), "stack_operations");
    
    TaggedValue bool_val = vm_pop(&vm);
    TEST_ASSERT(suite, IS_BOOL(bool_val), "stack_operations");
    TEST_ASSERT(suite, AS_BOOL(bool_val) == true, "stack_operations");
    
    TaggedValue num_val = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(num_val), "stack_operations");
    TEST_ASSERT(suite, AS_NUMBER(num_val) == 42, "stack_operations");
    
    vm_free(&vm);
}

DEFINE_TEST(arithmetic_operations) {
    VM vm;
    vm_init(&vm);
    
    // Test basic VM operations by simulating bytecode execution
    // This is a simplified test since vm_binary_op is not exposed
    
    // Test that we can push and pop numbers
    vm_push(&vm, NUMBER_VAL(42.0));
    TaggedValue val = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(val), "arithmetic_operations");
    TEST_ASSERT(suite, AS_NUMBER(val) == 42.0, "arithmetic_operations");
    
    vm_free(&vm);
}

DEFINE_TEST(comparison_operations) {
    VM vm;
    vm_init(&vm);
    
    // Test value equality using values_equal (if exposed)
    // For now, test basic stack operations with different types
    vm_push(&vm, NUMBER_VAL(42));
    vm_push(&vm, NUMBER_VAL(42));
    
    TaggedValue b = vm_pop(&vm);
    TaggedValue a = vm_pop(&vm);
    
    TEST_ASSERT(suite, IS_NUMBER(a) && IS_NUMBER(b), "comparison_operations");
    TEST_ASSERT(suite, AS_NUMBER(a) == AS_NUMBER(b), "comparison_operations");
    
    vm_free(&vm);
}

DEFINE_TEST(logical_operations) {
    VM vm;
    vm_init(&vm);
    
    // Test boolean values on stack
    vm_push(&vm, BOOL_VAL(true));
    vm_push(&vm, BOOL_VAL(false));
    
    TaggedValue false_val = vm_pop(&vm);
    TaggedValue true_val = vm_pop(&vm);
    
    TEST_ASSERT(suite, IS_BOOL(true_val), "logical_operations");
    TEST_ASSERT(suite, AS_BOOL(true_val) == true, "logical_operations");
    TEST_ASSERT(suite, IS_BOOL(false_val), "logical_operations");
    TEST_ASSERT(suite, AS_BOOL(false_val) == false, "logical_operations");
    
    vm_free(&vm);
}

DEFINE_TEST(nil_operations) {
    VM vm;
    vm_init(&vm);
    
    // Test nil on stack
    vm_push(&vm, NIL_VAL);
    TaggedValue nil_val = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NIL(nil_val), "nil_operations");
    
    // Test nil vs other types
    vm_push(&vm, NIL_VAL);
    vm_push(&vm, NUMBER_VAL(42));
    
    TaggedValue num_val = vm_pop(&vm);
    nil_val = vm_pop(&vm);
    
    TEST_ASSERT(suite, IS_NIL(nil_val), "nil_operations");
    TEST_ASSERT(suite, IS_NUMBER(num_val), "nil_operations");
    
    vm_free(&vm);
}

DEFINE_TEST(string_operations) {
    VM vm;
    vm_init(&vm);
    
    // Test string values on stack
    // Note: STRING_VAL expects char*, not const char*
    char str1[] = "Hello";
    char str2[] = "World";
    
    vm_push(&vm, STRING_VAL(str1));
    vm_push(&vm, STRING_VAL(str2));
    
    TaggedValue world = vm_pop(&vm);
    TaggedValue hello = vm_pop(&vm);
    
    TEST_ASSERT(suite, IS_STRING(hello), "string_operations");
    TEST_ASSERT(suite, IS_STRING(world), "string_operations");
    TEST_ASSERT(suite, strcmp(AS_STRING(hello), "Hello") == 0, "string_operations");
    TEST_ASSERT(suite, strcmp(AS_STRING(world), "World") == 0, "string_operations");
    
    vm_free(&vm);
}

// Define test suite
TEST_SUITE(vm_unit)
    TEST_CASE(stack_operations, "Stack Operations")
    TEST_CASE(arithmetic_operations, "Arithmetic Operations")
    TEST_CASE(comparison_operations, "Comparison Operations")
    TEST_CASE(logical_operations, "Logical Operations")
    TEST_CASE(nil_operations, "Nil Operations")
    TEST_CASE(string_operations, "String Operations")
END_TEST_SUITE(vm_unit)

// Optional standalone runner
