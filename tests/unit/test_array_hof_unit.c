#include <stdio.h>
#include <string.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "parser/parser.h"
#include "vm/vm.h"
#include "codegen/compiler.h"

DEFINE_TEST(array_map_lambda) {
    const char* source = 
        "let nums = [1, 2, 3, 4, 5]\n"
        "let doubled = nums.map({ x in x * 2 })\n"
        "doubled.length";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "array_map_lambda - parsing");
    TEST_ASSERT(suite, !parser->had_error, "array_map_lambda - no parse errors");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_map_lambda - compilation");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_map_lambda - execution");
    
    // Check result - length should be 5
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_map_lambda - result is number");
    TEST_ASSERT(suite, AS_NUMBER(top) == 5.0, "array_map_lambda - correct length");
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(array_map_values) {
    const char* source = 
        "let nums = [1, 2, 3]\n"
        "let doubled = nums.map({ x in x * 2 })\n"
        "let sum = doubled.reduce({ acc, x in acc + x }, 0)\n"
        "sum";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "array_map_values - parsing");
    TEST_ASSERT(suite, !parser->had_error, "array_map_values - no parse errors");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_map_values - compilation");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_map_values - execution");
    
    // Check result - sum should be 12 (2+4+6)
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_map_values - result is number");
    TEST_ASSERT(suite, AS_NUMBER(top) == 12.0, "array_map_values - correct sum");
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(array_filter_lambda) {
    const char* source = 
        "let nums = [1, 2, 3, 4, 5, 6]\n"
        "let evens = nums.filter({ x in x % 2 == 0 })\n"
        "evens.length";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "array_filter_lambda - parsing");
    TEST_ASSERT(suite, !parser->had_error, "array_filter_lambda - no parse errors");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_filter_lambda - compilation");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_filter_lambda - execution");
    
    // Check result - should have 3 even numbers (2, 4, 6)
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_filter_lambda - result is number");
    TEST_ASSERT(suite, AS_NUMBER(top) == 3.0, "array_filter_lambda - correct count");
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(array_reduce_lambda) {
    const char* source = 
        "let nums = [1, 2, 3, 4, 5]\n"
        "let sum = nums.reduce({ acc, x in acc + x }, 0)\n"
        "sum";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "array_reduce_lambda - parsing");
    TEST_ASSERT(suite, !parser->had_error, "array_reduce_lambda - no parse errors");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_reduce_lambda - compilation");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_reduce_lambda - execution");
    
    // Check result - sum should be 15
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_reduce_lambda - result is number");
    TEST_ASSERT(suite, AS_NUMBER(top) == 15.0, "array_reduce_lambda - correct sum");
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(array_nested_hof) {
    const char* source = 
        "let nums = [1, 2, 3, 4, 5]\n"
        "let result = nums\n"
        "    .map({ x in x * 2 })\n"
        "    .filter({ x in x > 5 })\n"
        "    .reduce({ acc, x in acc + x }, 0)\n"
        "result";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "array_nested_hof - parsing");
    TEST_ASSERT(suite, !parser->had_error, "array_nested_hof - no parse errors");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_nested_hof - compilation");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_nested_hof - execution");
    
    // Check result - should be 24 (6+8+10)
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_nested_hof - result is number");
    TEST_ASSERT(suite, AS_NUMBER(top) == 24.0, "array_nested_hof - correct sum");
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

// Define test suite
TEST_SUITE(array_hof_unit)
    TEST_CASE(array_map_lambda, "Array Map with Lambda")
    TEST_CASE(array_map_values, "Array Map Values")
    TEST_CASE(array_filter_lambda, "Array Filter with Lambda")
    TEST_CASE(array_reduce_lambda, "Array Reduce with Lambda")
    TEST_CASE(array_nested_hof, "Nested Array HOF Operations")
END_TEST_SUITE(array_hof_unit)