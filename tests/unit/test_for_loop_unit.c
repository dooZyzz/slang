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
#include "ast/ast.h"
#include "utils/error.h"

DEFINE_TEST(for_in_array) {
    const char* source = 
        "var sum = 0;"
        "for i in [1, 2, 3, 4, 5] {"
        "    sum = sum + i;"
        "}"
        "sum;";  // Should be 1+2+3+4+5 = 15
    
    ErrorReporter* errors = error_reporter_create();
    
    Parser* parser = parser_create(source);
    ProgramNode* prog = parser_parse_program(parser);
    
    TEST_ASSERT_NOT_NULL(suite, prog, "for_in_array");
    TEST_ASSERT(suite, !parser->had_error, "for_in_array");
    
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    semantic_analyze(analyzer, prog);
    
    TEST_ASSERT(suite, !error_has_errors(errors), "for_in_array");
    
    Chunk chunk;
    chunk_init(&chunk);
    
    bool compiled = compile(prog, &chunk);
    TEST_ASSERT(suite, compiled, "for_in_array");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "for_in_array");
    
    // Check result - should be 15
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "for_in_array");
    TEST_ASSERT(suite, AS_NUMBER(top) == 15.0, "for_in_array");
    
    // Cleanup
    vm_free(&vm);
    chunk_free(&chunk);
    semantic_analyzer_destroy(analyzer);
    error_reporter_destroy(errors);
    parser_destroy(parser);
    program_destroy(prog);
}

DEFINE_TEST(for_in_empty_array) {
    const char* source = 
        "var count = 0;"
        "for i in [] {"
        "    count = count + 1;"
        "}"
        "count;";  // Should be 0
    
    ErrorReporter* errors = error_reporter_create();
    
    Parser* parser = parser_create(source);
    ProgramNode* prog = parser_parse_program(parser);
    
    TEST_ASSERT_NOT_NULL(suite, prog, "for_in_empty_array");
    TEST_ASSERT(suite, !parser->had_error, "for_in_empty_array");
    
    SemanticAnalyzer* analyzer = semantic_analyzer_create(errors);
    semantic_analyze(analyzer, prog);
    
    TEST_ASSERT(suite, !error_has_errors(errors), "for_in_empty_array");
    
    Chunk chunk;
    chunk_init(&chunk);
    
    bool compiled = compile(prog, &chunk);
    TEST_ASSERT(suite, compiled, "for_in_empty_array");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "for_in_empty_array");
    
    // Check result - should be 0
    TaggedValue top = vm_pop(&vm);
    TEST_ASSERT(suite, IS_NUMBER(top), "for_in_empty_array");
    TEST_ASSERT(suite, AS_NUMBER(top) == 0.0, "for_in_empty_array");
    
    // Cleanup
    vm_free(&vm);
    chunk_free(&chunk);
    semantic_analyzer_destroy(analyzer);
    error_reporter_destroy(errors);
    parser_destroy(parser);
    program_destroy(prog);
}

// Define test suite
TEST_SUITE(for_loop_unit)
    TEST_CASE(for_in_array, "For-In Loop with Array")
    TEST_CASE(for_in_empty_array, "For-In Loop with Empty Array")
END_TEST_SUITE(for_loop_unit)