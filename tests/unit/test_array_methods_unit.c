#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/analyzer.h"
#include "codegen/compiler.h"
#include "runtime/core/vm.h"
#include "utils/error.h"

DEFINE_TEST(array_length) {
    const char* source = "let arr = [1, 2, 3]; arr.length;";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "array_length");
    
    ErrorReporter* errors = error_reporter_create();
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    bool semantic_ok = semantic_analyze(analyzer, program);
    TEST_ASSERT(suite, semantic_ok, "array_length");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_length");
    
    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_length");
    
    // Check result
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_length");
    TEST_ASSERT(suite, AS_NUMBER(top) == 3.0, "array_length");
    
    vm_free(&vm);
    chunk_free(&chunk);
    semantic_analyzer_destroy(analyzer);
    error_reporter_destroy(errors);
    parser_destroy(parser);
    program_destroy(program);
}

DEFINE_TEST(array_push) {
    const char* source = 
        "let arr = [1, 2];"
        "arr.push(3);"
        "arr.length;";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "array_push");
    
    ErrorReporter* errors = error_reporter_create();
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    bool semantic_ok = semantic_analyze(analyzer, program);
    TEST_ASSERT(suite, semantic_ok, "array_push");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_push");
    
    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_push");
    
    // Check result - length should be 3
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_push");
    TEST_ASSERT(suite, AS_NUMBER(top) == 3.0, "array_push");
    
    vm_free(&vm);
    chunk_free(&chunk);
    semantic_analyzer_destroy(analyzer);
    error_reporter_destroy(errors);
    parser_destroy(parser);
    program_destroy(program);
}

DEFINE_TEST(array_pop) {
    const char* source = 
        "let arr = [1, 2, 3];"
        "let popped = arr.pop();"
        "popped;";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "array_pop");
    
    ErrorReporter* errors = error_reporter_create();
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    bool semantic_ok = semantic_analyze(analyzer, program);
    TEST_ASSERT(suite, semantic_ok, "array_pop");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_pop");
    
    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_pop");
    
    // Check result - should be 3
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_pop");
    TEST_ASSERT(suite, AS_NUMBER(top) == 3.0, "array_pop");
    
    vm_free(&vm);
    chunk_free(&chunk);
    semantic_analyzer_destroy(analyzer);
    error_reporter_destroy(errors);
    parser_destroy(parser);
    program_destroy(program);
}

DEFINE_TEST(array_access_and_push) {
    const char* source = 
        "let arr = [];"
        "arr.push(10);"
        "arr.push(20);"
        "arr[0] + arr[1];";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "array_access_and_push");
    
    ErrorReporter* errors = error_reporter_create();
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    bool semantic_ok = semantic_analyze(analyzer, program);
    TEST_ASSERT(suite, semantic_ok, "array_access_and_push");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_access_and_push");
    
    VM vm;
    vm_init(&vm);
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_access_and_push");
    
    // Check result - should be 30
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_access_and_push");
    TEST_ASSERT(suite, AS_NUMBER(top) == 30.0, "array_access_and_push");
    
    vm_free(&vm);
    chunk_free(&chunk);
    semantic_analyzer_destroy(analyzer);
    error_reporter_destroy(errors);
    parser_destroy(parser);
    program_destroy(program);
}

// Native test callbacks for array methods
// Commented out until vm_define_global is implemented
/*
static TaggedValue double_value(int arg_count, TaggedValue* args) {
    if (arg_count >= 1 && IS_NUMBER(args[0])) {
        return NUMBER_VAL(AS_NUMBER(args[0]) * 2);
    }
    return NIL_VAL;
}

static TaggedValue is_even(int arg_count, TaggedValue* args) {
    if (arg_count >= 1 && IS_NUMBER(args[0])) {
        int num = (int)AS_NUMBER(args[0]);
        return BOOL_VAL(num % 2 == 0);
    }
    return BOOL_VAL(false);
}

static TaggedValue sum_values(int arg_count, TaggedValue* args) {
    if (arg_count >= 2 && IS_NUMBER(args[0]) && IS_NUMBER(args[1])) {
        return NUMBER_VAL(AS_NUMBER(args[0]) + AS_NUMBER(args[1]));
    }
    return NIL_VAL;
}
*/

// Commented out - requires vm_define_global which is not implemented
// Use lambda tests in test_array_hof_unit.c instead
/*
DEFINE_TEST(array_map_native) {
    const char* source = 
        "let arr = [1, 2, 3];"
        "let doubled = arr.map(test_double);"
        "doubled.length;";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "array_map_native");
    
    ErrorReporter* errors = error_reporter_create();
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    bool semantic_ok = semantic_analyze(analyzer, program);
    TEST_ASSERT(suite, semantic_ok, "array_map_native");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_map_native");
    
    VM vm;
    vm_init(&vm);
    
    // Register test callback
    vm_push(&vm, NATIVE_VAL(double_value));
    vm_define_global(&vm, "test_double", vm_pop(&vm));
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_map_native");
    
    // Check result - length should be 3
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_map_native");
    TEST_ASSERT(suite, AS_NUMBER(top) == 3.0, "array_map_native");
    
    vm_free(&vm);
    chunk_free(&chunk);
    semantic_analyzer_destroy(analyzer);
    error_reporter_destroy(errors);
    parser_destroy(parser);
    program_destroy(program);
}
*/

/*
DEFINE_TEST(array_filter_native) {
    const char* source = 
        "let arr = [1, 2, 3, 4, 5];"
        "let evens = arr.filter(test_is_even);"
        "evens.length;";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "array_filter_native");
    
    ErrorReporter* errors = error_reporter_create();
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    bool semantic_ok = semantic_analyze(analyzer, program);
    TEST_ASSERT(suite, semantic_ok, "array_filter_native");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_filter_native");
    
    VM vm;
    vm_init(&vm);
    
    // Register test callback
    vm_push(&vm, NATIVE_VAL(is_even));
    vm_define_global(&vm, "test_is_even", vm_pop(&vm));
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_filter_native");
    
    // Check result - length should be 2 (2 and 4 are even)
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_filter_native");
    TEST_ASSERT(suite, AS_NUMBER(top) == 2.0, "array_filter_native");
    
    vm_free(&vm);
    chunk_free(&chunk);
    semantic_analyzer_destroy(analyzer);
    error_reporter_destroy(errors);
    parser_destroy(parser);
    program_destroy(program);
}
*/

/*
DEFINE_TEST(array_reduce_native) {
    const char* source = 
        "let arr = [1, 2, 3, 4];"
        "let sum = arr.reduce(test_sum, 0);"
        "sum;";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT(suite, !parser->had_error, "array_reduce_native");
    
    ErrorReporter* errors = error_reporter_create();
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    bool semantic_ok = semantic_analyze(analyzer, program);
    TEST_ASSERT(suite, semantic_ok, "array_reduce_native");
    
    Chunk chunk;
    chunk_init(&chunk);
    bool compile_ok = compile(program, &chunk);
    TEST_ASSERT(suite, compile_ok, "array_reduce_native");
    
    VM vm;
    vm_init(&vm);
    
    // Register test callback
    vm_push(&vm, NATIVE_VAL(sum_values));
    vm_define_global(&vm, "test_sum", vm_pop(&vm));
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "array_reduce_native");
    
    // Check result - sum should be 10
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "array_reduce_native");
    TEST_ASSERT(suite, AS_NUMBER(top) == 10.0, "array_reduce_native");
    
    vm_free(&vm);
    chunk_free(&chunk);
    semantic_analyzer_destroy(analyzer);
    error_reporter_destroy(errors);
    parser_destroy(parser);
    program_destroy(program);
}
*/

// Define test suite
TEST_SUITE(array_methods_unit)
    TEST_CASE(array_length, "Array Length Property")
    TEST_CASE(array_push, "Array Push Method")
    TEST_CASE(array_pop, "Array Pop Method")
    TEST_CASE(array_access_and_push, "Array Access and Push")
    // Commented out - these require vm_define_global
    // TEST_CASE(array_map_native, "Array Map Method with Native Callback")
    // TEST_CASE(array_filter_native, "Array Filter Method with Native Callback")
    // TEST_CASE(array_reduce_native, "Array Reduce Method with Native Callback")
END_TEST_SUITE(array_methods_unit)