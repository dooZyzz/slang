#ifndef SYNTAX_TEST_H
#define SYNTAX_TEST_H

#include <stdbool.h>
#include <stddef.h>

// Test directive types that can appear in Swift source files
typedef enum {
    TEST_DIRECTIVE_NONE,
    TEST_DIRECTIVE_COMPILE_FAIL,    // @compile-fail: <error message pattern>
    TEST_DIRECTIVE_RUNTIME_ERROR,   // @runtime-error: <error message pattern>
    TEST_DIRECTIVE_OUTPUT,          // @output: <expected output>
    TEST_DIRECTIVE_OUTPUT_REGEX,    // @output-regex: <regex pattern>
    TEST_DIRECTIVE_PARSE_ONLY,      // @parse-only
    TEST_DIRECTIVE_NO_RUN,          // @no-run
    TEST_DIRECTIVE_IGNORE,          // @ignore
} TestDirectiveType;

typedef struct {
    TestDirectiveType type;
    char* pattern;  // Expected error message or output pattern
    int line;       // Line number where directive appears
} TestDirective;

typedef struct {
    char* filename;
    char* content;
    TestDirective* directives;
    size_t directive_count;
    size_t directive_capacity;
} SyntaxTest;

typedef struct {
    SyntaxTest* tests;
    size_t test_count;
    size_t test_capacity;
    
    // Statistics
    size_t passed;
    size_t failed;
    size_t ignored;
} SyntaxTestSuite;

// Test suite management
SyntaxTestSuite* syntax_test_suite_create(const char* name);
void syntax_test_suite_destroy(SyntaxTestSuite* suite);
void syntax_test_suite_add_file(SyntaxTestSuite* suite, const char* filepath);
void syntax_test_suite_add_directory(SyntaxTestSuite* suite, const char* dirpath);

// Test execution
bool syntax_test_run(SyntaxTest* test, bool verbose);
bool syntax_test_suite_run(SyntaxTestSuite* suite, bool verbose);

// Test parsing
SyntaxTest* syntax_test_parse(const char* filepath);
void syntax_test_destroy(SyntaxTest* test);

// Result reporting
void syntax_test_print_results(SyntaxTestSuite* suite);

#endif // SYNTAX_TEST_H