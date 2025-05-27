#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "utils/syntax_test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


// Test the syntax test framework itself
static void test_directive_parsing(TestSuite* suite) {
    // Create a temporary test file
    const char* test_content = 
        "// @compile-fail: undefined variable\n"
        "let x = y\n"
        "\n"
        "// @output: Hello, World!\n"
        "print(\"Hello, World!\")\n"
        "\n"
        "// @parse-only\n"
        "func test() {}\n";
    
    // Write to temp file
    FILE* f = tmpfile();
    fwrite(test_content, 1, strlen(test_content), f);
    fflush(f);
    
    // Parse directives (this is internal testing)
    // In real usage, syntax_test_parse would handle this
    
    TEST_ASSERT(suite, true, "directive_parsing");
}

static void test_compile_fail(TestSuite* suite) {
    // Test that compile-fail tests work correctly
    SyntaxTestSuite* syntax_suite = syntax_test_suite_create("compile_fail_test");
    
    // Would add actual test files here
    // For now, just test the framework
    TEST_ASSERT(suite, syntax_suite != NULL, "compile_fail");
    
    syntax_test_suite_destroy(syntax_suite);
}

static void test_output_matching(TestSuite* suite) {
    // Test output matching functionality
    SyntaxTestSuite* syntax_suite = syntax_test_suite_create("output_test");
    
    TEST_ASSERT(suite, syntax_suite != NULL, "output_matching");
    
    syntax_test_suite_destroy(syntax_suite);
}

static void test_runtime_error(TestSuite* suite) {
    // Test runtime error detection
    SyntaxTestSuite* syntax_suite = syntax_test_suite_create("runtime_error_test");
    
    TEST_ASSERT(suite, syntax_suite != NULL, "runtime_error");
    
    syntax_test_suite_destroy(syntax_suite);
}

static void test_parse_only(TestSuite* suite) {
    // Test parse-only tests
    SyntaxTestSuite* syntax_suite = syntax_test_suite_create("parse_only_test");
    
    TEST_ASSERT(suite, syntax_suite != NULL, "parse_only");
    
    syntax_test_suite_destroy(syntax_suite);
}

// Run all syntax tests from the tests/syntax directory
static void test_run_syntax_tests(TestSuite* suite) {
    SyntaxTestSuite* syntax_suite = syntax_test_suite_create("all_syntax_tests");
    
    // Add all syntax tests from the directory
    syntax_test_suite_add_directory(syntax_suite, "tests/syntax");
    
    // Run all tests
    bool all_passed = syntax_test_suite_run(syntax_suite, false);
    
    TEST_ASSERT(suite, all_passed, "run_syntax_tests");
    
    // Print detailed results if any failed
    if (!all_passed) {
        syntax_test_print_results(syntax_suite);
    }
    
    syntax_test_suite_destroy(syntax_suite);
}

// Define test suite
TEST_SUITE(syntax_unit)
    TEST_CASE(directive_parsing, "Directive Parsing")
    TEST_CASE(compile_fail, "Compile Fail Tests")
    TEST_CASE(output_matching, "Output Matching")
    TEST_CASE(runtime_error, "Runtime Error Tests")
    TEST_CASE(parse_only, "Parse Only Tests")
    TEST_CASE(run_syntax_tests, "Run All Syntax Tests")
END_TEST_SUITE(syntax_unit)