#include <stdio.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "vm/vm.h"

DEFINE_TEST(array_index_assignment) {
    const char* source = 
        "var array = [1, 6, 4];\n"
        "array[0] = array[1];\n"
        "array[0];\n";
    
    Parser* parser = parser_create(source);
    ProgramNode* ast = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "array_index_assignment");
    TEST_ASSERT_NOT_NULL(suite, ast, "array_index_assignment");
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(ast, &chunk), "array_index_assignment");
    
    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_index_assignment");
    
    // Check that array[0] now equals 6
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_index_assignment");
    TEST_ASSERT(suite, AS_NUMBER(top) == 6.0, "array_index_assignment");
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(ast);
    parser_destroy(parser);
}

DEFINE_TEST(array_multi_assignment) {
    const char* source = 
        "var arr = [1, 2, 3, 4, 5];\n"
        "arr[1] = 10;\n"
        "arr[3] = 20;\n"
        "arr[1] + arr[3];\n";
    
    Parser* parser = parser_create(source);
    ProgramNode* ast = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "array_multi_assignment");
    TEST_ASSERT_NOT_NULL(suite, ast, "array_multi_assignment");
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(ast, &chunk), "array_multi_assignment");
    
    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_multi_assignment");
    
    // Check result - should be 30
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_multi_assignment");
    TEST_ASSERT(suite, AS_NUMBER(top) == 30.0, "array_multi_assignment");
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(ast);
    parser_destroy(parser);
}

DEFINE_TEST(array_assign_from_expression) {
    const char* source = 
        "var arr = [0, 0, 0];\n"
        "var x = 5;\n"
        "var y = 10;\n"
        "arr[1] = x + y;\n"
        "arr[1];\n";
    
    Parser* parser = parser_create(source);
    ProgramNode* ast = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "array_assign_from_expression");
    TEST_ASSERT_NOT_NULL(suite, ast, "array_assign_from_expression");
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(ast, &chunk), "array_assign_from_expression");
    
    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_assign_from_expression");
    
    // Check result - should be 15
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_assign_from_expression");
    TEST_ASSERT(suite, AS_NUMBER(top) == 15.0, "array_assign_from_expression");
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(ast);
    parser_destroy(parser);
}

// Define test suite
TEST_SUITE(array_assign_unit)
    TEST_CASE(array_index_assignment, "Array Index Assignment")
    TEST_CASE(array_multi_assignment, "Multiple Array Assignments")
    TEST_CASE(array_assign_from_expression, "Array Assignment from Expression")
END_TEST_SUITE(array_assign_unit)