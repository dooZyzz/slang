#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "runtime/core/vm.h"
#include "debug/debug.h"

DEFINE_TEST(simple_arithmetic) {
    const char* source = "1 + 2 * 3;"; // Should evaluate to 7
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "simple_arithmetic");
    TEST_ASSERT(suite, !parser->had_error, "simple_arithmetic");
    
    Chunk chunk;
    chunk_init(&chunk);
    
    bool compiled = compile(program, &chunk);
    TEST_ASSERT(suite, compiled, "simple_arithmetic");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "simple_arithmetic");
    
    // Clean up
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(variable_declaration) {
    const char* source = "var x = 42; x;";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "variable_declaration");
    TEST_ASSERT(suite, !parser->had_error, "variable_declaration");
    
    Chunk chunk;
    chunk_init(&chunk);
    
    bool compiled = compile(program, &chunk);
    TEST_ASSERT(suite, compiled, "variable_declaration");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "variable_declaration");
    
    // Clean up
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(if_statement) {
    const char* source = "var x = 10; if (x > 5) { x = 20; } x;";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "if_statement");
    TEST_ASSERT(suite, !parser->had_error, "if_statement");
    
    Chunk chunk;
    chunk_init(&chunk);
    
    bool compiled = compile(program, &chunk);
    TEST_ASSERT(suite, compiled, "if_statement");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "if_statement");
    
    // Clean up
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(while_loop) {
    const char* source = 
        "var sum = 0;"
        "var i = 0;"
        "while (i < 5) {"
        "    sum = sum + i;"
        "    i = i + 1;"
        "}"
        "sum;"; // Should be 0+1+2+3+4 = 10
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "while_loop");
    TEST_ASSERT(suite, !parser->had_error, "while_loop");
    
    Chunk chunk;
    chunk_init(&chunk);
    
    bool compiled = compile(program, &chunk);
    TEST_ASSERT(suite, compiled, "while_loop");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "while_loop");
    
    // Clean up
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(logical_operators) {
    const char* source = "true && false || true;"; // Should evaluate to true
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "logical_operators");
    TEST_ASSERT(suite, !parser->had_error, "logical_operators");
    
    Chunk chunk;
    chunk_init(&chunk);
    
    bool compiled = compile(program, &chunk);
    TEST_ASSERT(suite, compiled, "logical_operators");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "logical_operators");
    
    // Clean up
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(string_literals) {
    const char* source = "\"Hello, World!\";";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "string_literals");
    TEST_ASSERT(suite, !parser->had_error, "string_literals");
    
    Chunk chunk;
    chunk_init(&chunk);
    
    bool compiled = compile(program, &chunk);
    TEST_ASSERT(suite, compiled, "string_literals");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "string_literals");
    
    // Clean up
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(multiline_strings) {
    const char* source = 
        "let poem = \"Roses are red\n"
        "Violets are blue\n"
        "Sugar is sweet\n"
        "And so are you\";\n"
        "poem;";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "multiline_strings");
    TEST_ASSERT(suite, !parser->had_error, "multiline_strings");
    
    Chunk chunk;
    chunk_init(&chunk);
    
    bool compiled = compile(program, &chunk);
    TEST_ASSERT(suite, compiled, "multiline_strings");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "multiline_strings");
    
    // Clean up
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(multiline_string_with_interpolation) {
    const char* source = 
        "let name = \"Alice\";\n"
        "let age = 25;\n"
        "let message = \"User Info:\n"
        "Name: $name\n"
        "Age: $age\n"
        "Status: Active\";\n"
        "message;";
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "multiline_string_with_interpolation");
    TEST_ASSERT(suite, !parser->had_error, "multiline_string_with_interpolation");
    
    Chunk chunk;
    chunk_init(&chunk);
    
    bool compiled = compile(program, &chunk);
    TEST_ASSERT(suite, compiled, "multiline_string_with_interpolation");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "multiline_string_with_interpolation");
    
    // Clean up
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

DEFINE_TEST(scoped_variables) {
    const char* source = 
        "var x = 10;"
        "{"
        "    var x = 20;"  // Shadow outer x
        "    x = 30;"       // Modify inner x
        "}"
        "x;";              // Should still be 10
    
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    TEST_ASSERT_NOT_NULL(suite, program, "scoped_variables");
    TEST_ASSERT(suite, !parser->had_error, "scoped_variables");
    
    Chunk chunk;
    chunk_init(&chunk);
    
    bool compiled = compile(program, &chunk);
    TEST_ASSERT(suite, compiled, "scoped_variables");
    
    VM vm;
    vm_init(&vm);
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    TEST_ASSERT_EQUAL_INT(suite, INTERPRET_OK, result, "scoped_variables");
    
    // Clean up
    vm_free(&vm);
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
}

// Define test suite
TEST_SUITE(integration)
    TEST_CASE(simple_arithmetic, "Simple Arithmetic")
    TEST_CASE(variable_declaration, "Variable Declaration")
    TEST_CASE(if_statement, "If Statement")
    TEST_CASE(while_loop, "While Loop")
    TEST_CASE(logical_operators, "Logical Operators")
    TEST_CASE(string_literals, "String Literals")
    TEST_CASE(multiline_strings, "Multi-line Strings")
    TEST_CASE(multiline_string_with_interpolation, "Multi-line String with Interpolation")
    TEST_CASE(scoped_variables, "Scoped Variables")
END_TEST_SUITE(integration)

// Optional standalone runner
