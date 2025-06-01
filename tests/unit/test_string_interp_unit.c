#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "semantic/analyzer.h"
#include "codegen/compiler.h"
#include "runtime/core/vm.h"
#include "utils/error.h"
#include "ast/ast.h"
#include "runtime/modules/lifecycle/builtin_modules.h"

DEFINE_TEST(simple_interpolation) {
    const char* source = 
        "let name = \"World\"\n"
        "let greeting = \"Hello, $name!\"\n"
        "greeting\n";
    
    ErrorReporter* errors = error_reporter_create();
    Parser* parser = parser_create(source);
    TEST_ASSERT_NOT_NULL(suite, parser, "simple_interpolation");
    
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "simple_interpolation");
    TEST_ASSERT(suite, !parser->had_error, "simple_interpolation");
    
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    TEST_ASSERT_NOT_NULL(suite, analyzer, "simple_interpolation");
    TEST_ASSERT(suite, semantic_analyze(analyzer, program), "simple_interpolation");
    semantic_analyzer_destroy(analyzer);
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(program, &chunk), "simple_interpolation");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "simple_interpolation");
    
    // Check result
    TEST_ASSERT(suite, vm.stack_top > vm.stack, "simple_interpolation");
    TaggedValue result_val = vm.stack_top[-1];
    TEST_ASSERT(suite, IS_STRING(result_val), "simple_interpolation");
    TEST_ASSERT_EQUAL_STRING(suite, "Hello, World!", AS_STRING(result_val), "simple_interpolation");
    
    vm_free(&vm);
    chunk_free(&chunk);
    parser_destroy(parser);
    program_destroy(program);
    error_reporter_destroy(errors);
}

DEFINE_TEST(expression_interpolation) {
    const char* source = 
        "let x = 10\n"
        "let y = 20\n"
        "let result = \"The sum of $x and $y is ${x + y}\"\n"
        "result\n";
    
    ErrorReporter* errors = error_reporter_create();
    Parser* parser = parser_create(source);
    TEST_ASSERT_NOT_NULL(suite, parser, "expression_interpolation");
    
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "expression_interpolation");
    TEST_ASSERT(suite, !parser->had_error, "expression_interpolation");
    
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    TEST_ASSERT_NOT_NULL(suite, analyzer, "expression_interpolation");
    TEST_ASSERT(suite, semantic_analyze(analyzer, program), "expression_interpolation");
    semantic_analyzer_destroy(analyzer);
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(program, &chunk), "expression_interpolation");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "expression_interpolation");
    
    // Check result
    TEST_ASSERT(suite, vm.stack_top > vm.stack, "expression_interpolation");
    TaggedValue result_val = vm.stack_top[-1];
    TEST_ASSERT(suite, IS_STRING(result_val), "expression_interpolation");
    TEST_ASSERT_EQUAL_STRING(suite, "The sum of 10 and 20 is 30", AS_STRING(result_val), "expression_interpolation");
    
    vm_free(&vm);
    chunk_free(&chunk);
    parser_destroy(parser);
    program_destroy(program);
    error_reporter_destroy(errors);
}

DEFINE_TEST(mixed_interpolation) {
    const char* source = 
        "let name = \"Alice\"\n"
        "let age = 25\n"
        "let msg = \"$name is $age years old and will be ${age + 1} next year\"\n"
        "msg\n";
    
    ErrorReporter* errors = error_reporter_create();
    Parser* parser = parser_create(source);
    TEST_ASSERT_NOT_NULL(suite, parser, "mixed_interpolation");
    
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "mixed_interpolation");
    TEST_ASSERT(suite, !parser->had_error, "mixed_interpolation");
    
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    TEST_ASSERT_NOT_NULL(suite, analyzer, "mixed_interpolation");
    TEST_ASSERT(suite, semantic_analyze(analyzer, program), "mixed_interpolation");
    semantic_analyzer_destroy(analyzer);
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(program, &chunk), "mixed_interpolation");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "mixed_interpolation");
    
    // Check result
    TEST_ASSERT(suite, vm.stack_top > vm.stack, "mixed_interpolation");
    TaggedValue result_val = vm.stack_top[-1];
    TEST_ASSERT(suite, IS_STRING(result_val), "mixed_interpolation");
    TEST_ASSERT_EQUAL_STRING(suite, "Alice is 25 years old and will be 26 next year", AS_STRING(result_val), "mixed_interpolation");
    
    vm_free(&vm);
    chunk_free(&chunk);
    parser_destroy(parser);
    program_destroy(program);
    error_reporter_destroy(errors);
}

DEFINE_TEST(nested_interpolation) {
    const char* source = 
        "let x = 5\n"
        "let y = 10\n"
        "let expr = \"x + y\"\n"
        "let result = \"The expression '$expr' evaluates to ${x + y}\"\n"
        "result\n";
    
    ErrorReporter* errors = error_reporter_create();
    Parser* parser = parser_create(source);
    TEST_ASSERT_NOT_NULL(suite, parser, "nested_interpolation");
    
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "nested_interpolation");
    TEST_ASSERT(suite, !parser->had_error, "nested_interpolation");
    
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    TEST_ASSERT_NOT_NULL(suite, analyzer, "nested_interpolation");
    TEST_ASSERT(suite, semantic_analyze(analyzer, program), "nested_interpolation");
    semantic_analyzer_destroy(analyzer);
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(program, &chunk), "nested_interpolation");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "nested_interpolation");
    
    // Check result
    TEST_ASSERT(suite, vm.stack_top > vm.stack, "nested_interpolation");
    TaggedValue result_val = vm.stack_top[-1];
    TEST_ASSERT(suite, IS_STRING(result_val), "nested_interpolation");
    TEST_ASSERT_EQUAL_STRING(suite, "The expression 'x + y' evaluates to 15", AS_STRING(result_val), "nested_interpolation");
    
    vm_free(&vm);
    chunk_free(&chunk);
    parser_destroy(parser);
    program_destroy(program);
    error_reporter_destroy(errors);
}

DEFINE_TEST(type_conversion) {
    const char* source = 
        "let n = 42\n"
        "let b = true\n"
        "let s = \"Value: $n, Bool: $b\"\n"
        "s\n";
    
    ErrorReporter* errors = error_reporter_create();
    Parser* parser = parser_create(source);
    TEST_ASSERT_NOT_NULL(suite, parser, "type_conversion");
    
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "type_conversion");
    TEST_ASSERT(suite, !parser->had_error, "type_conversion");
    
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    TEST_ASSERT_NOT_NULL(suite, analyzer, "type_conversion");
    TEST_ASSERT(suite, semantic_analyze(analyzer, program), "type_conversion");
    semantic_analyzer_destroy(analyzer);
    
    Chunk chunk;
    chunk_init(&chunk);
    TEST_ASSERT(suite, compile(program, &chunk), "type_conversion");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "type_conversion");
    
    // Check result
    TEST_ASSERT(suite, vm.stack_top > vm.stack, "type_conversion");
    TaggedValue result_val = vm.stack_top[-1];
    TEST_ASSERT(suite, IS_STRING(result_val), "type_conversion");
    TEST_ASSERT_EQUAL_STRING(suite, "Value: 42, Bool: true", AS_STRING(result_val), "type_conversion");
    
    vm_free(&vm);
    chunk_free(&chunk);
    parser_destroy(parser);
    program_destroy(program);
    error_reporter_destroy(errors);
}

// Define test suite
TEST_SUITE(string_interp_unit)
    TEST_CASE(simple_interpolation, "Simple Variable Interpolation")
    TEST_CASE(expression_interpolation, "Expression Interpolation")
    TEST_CASE(mixed_interpolation, "Mixed Interpolation")
    TEST_CASE(nested_interpolation, "Nested Interpolation")
    TEST_CASE(type_conversion, "Type Conversion in Interpolation")
END_TEST_SUITE(string_interp_unit)