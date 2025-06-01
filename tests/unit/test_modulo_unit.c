#include <stdio.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "runtime/core/vm.h"

DEFINE_TEST(basic_modulo) {
    const char* source = "5 % 2;";
    
    Parser* parser = parser_create(source);
    ProgramNode* ast = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "basic_modulo");
    TEST_ASSERT_NOT_NULL(suite, ast, "basic_modulo");
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(ast, &chunk), "basic_modulo");
    
    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);
    
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "basic_modulo");
    TEST_ASSERT(suite, vm.stack_top > vm.stack, "basic_modulo");
    
    TaggedValue value = *(vm.stack_top - 1);
    TEST_ASSERT(suite, IS_NUMBER(value), "basic_modulo");
    TEST_ASSERT(suite, AS_NUMBER(value) == 1.0, "basic_modulo");
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(ast);
    parser_destroy(parser);
}

DEFINE_TEST(modulo_zero_remainder) {
    const char* source = "10 % 5;";
    
    Parser* parser = parser_create(source);
    ProgramNode* ast = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "modulo_zero_remainder");
    TEST_ASSERT_NOT_NULL(suite, ast, "modulo_zero_remainder");
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(ast, &chunk), "modulo_zero_remainder");
    
    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);
    
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "modulo_zero_remainder");
    TEST_ASSERT(suite, vm.stack_top > vm.stack, "modulo_zero_remainder");
    
    TaggedValue value = *(vm.stack_top - 1);
    TEST_ASSERT(suite, IS_NUMBER(value), "modulo_zero_remainder");
    TEST_ASSERT(suite, AS_NUMBER(value) == 0.0, "modulo_zero_remainder");
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(ast);
    parser_destroy(parser);
}

DEFINE_TEST(modulo_negative) {
    const char* source = "-7 % 3;";
    
    Parser* parser = parser_create(source);
    ProgramNode* ast = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "modulo_negative");
    TEST_ASSERT_NOT_NULL(suite, ast, "modulo_negative");
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(ast, &chunk), "modulo_negative");
    
    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);
    
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "modulo_negative");
    TEST_ASSERT(suite, vm.stack_top > vm.stack, "modulo_negative");
    
    TaggedValue value = *(vm.stack_top - 1);
    TEST_ASSERT(suite, IS_NUMBER(value), "modulo_negative");
    TEST_ASSERT(suite, AS_NUMBER(value) == -1.0, "modulo_negative");
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(ast);
    parser_destroy(parser);
}

DEFINE_TEST(modulo_expression) {
    const char* source = "(10 + 5) % (2 + 2);";
    
    Parser* parser = parser_create(source);
    ProgramNode* ast = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "modulo_expression");
    TEST_ASSERT_NOT_NULL(suite, ast, "modulo_expression");
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(ast, &chunk), "modulo_expression");
    
    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);
    
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "modulo_expression");
    TEST_ASSERT(suite, vm.stack_top > vm.stack, "modulo_expression");
    
    TaggedValue value = *(vm.stack_top - 1);
    TEST_ASSERT(suite, IS_NUMBER(value), "modulo_expression");
    TEST_ASSERT(suite, AS_NUMBER(value) == 3.0, "modulo_expression"); // 15 % 4 = 3
    
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(ast);
    parser_destroy(parser);
}

// Define test suite
TEST_SUITE(modulo_unit)
    TEST_CASE(basic_modulo, "Basic Modulo Operation")
    TEST_CASE(modulo_zero_remainder, "Modulo with Zero Remainder")
    TEST_CASE(modulo_negative, "Modulo with Negative Number")
    TEST_CASE(modulo_expression, "Modulo with Expressions")
END_TEST_SUITE(modulo_unit)