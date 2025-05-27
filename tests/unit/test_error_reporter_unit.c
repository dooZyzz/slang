#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "utils/error.h"

// Helper to capture stderr output
static char captured_error[1024];
static FILE* original_stderr;

static void capture_stderr_start(void)
{
    memset(captured_error, 0, sizeof(captured_error));
    original_stderr = stderr;
    
    // Create a temporary file for capturing stderr
    FILE* temp = tmpfile();
    if (temp) {
        stderr = temp;
    }
}

static void capture_stderr_end(void)
{
    if (stderr != original_stderr) {
        fflush(stderr);
        rewind(stderr);
        fread(captured_error, 1, sizeof(captured_error) - 1, stderr);
        fclose(stderr);
    }
    stderr = original_stderr;
}

DEFINE_TEST(simple_error)
{
    capture_stderr_start();

    ErrorReporter* reporter = error_reporter_create();
    error_set_source(reporter, "test.swift", "var x = 42;");

    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "test.swift", 1, 4, "Test error message");

    capture_stderr_end();

    // Check that error was reported
    TEST_ASSERT(suite, strstr(captured_error, "error") != NULL || strstr(captured_error, "Error") != NULL,
                "simple_error");
    TEST_ASSERT(suite, strstr(captured_error, "Test error message") != NULL, "simple_error");
    TEST_ASSERT(suite, strstr(captured_error, "test.swift") != NULL, "simple_error");

    error_reporter_destroy(reporter);
}

DEFINE_TEST(error_with_context)
{
    capture_stderr_start();

    ErrorReporter* reporter = error_reporter_create();
    const char* source = "var x = 42;\nvar y = \"hello\";";
    error_set_source(reporter, "test.swift", source);

    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                        "test.swift", 2, 8, "Variable 'y' already defined");

    capture_stderr_end();

    // Check error message components - just check if the message was captured
    TEST_ASSERT(suite, strlen(captured_error) > 0, "error_with_context");
    TEST_ASSERT(suite, strstr(captured_error, "Variable 'y' already defined") != NULL, "error_with_context");

    error_reporter_destroy(reporter);
}

DEFINE_TEST(warning_message)
{
    capture_stderr_start();

    ErrorReporter* reporter = error_reporter_create();
    error_set_source(reporter, "test.swift", "var unused = 10;");

    error_report_simple(reporter, ERROR_LEVEL_WARNING, ERROR_PHASE_SEMANTIC,
                        "test.swift", 1, 4, "Variable 'unused' is never used");

    capture_stderr_end();

    // Check warning format - just check if the message was captured
    TEST_ASSERT(suite, strlen(captured_error) > 0, "warning_message");
    TEST_ASSERT(suite, strstr(captured_error, "Variable 'unused' is never used") != NULL, "warning_message");

    error_reporter_destroy(reporter);
}

DEFINE_TEST(multiline_error)
{
    capture_stderr_start();

    ErrorReporter* reporter = error_reporter_create();
    const char* source = "func foo() {\n    var x = \n    42\n}";
    error_set_source(reporter, "test.swift", source);

    // Error on line 2
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "test.swift", 2, 24, "Unexpected newline in expression");

    capture_stderr_end();

    // Check multiline handling
    TEST_ASSERT(suite, strstr(captured_error, "Unexpected newline") != NULL, "multiline_error");

    error_reporter_destroy(reporter);
}

DEFINE_TEST(error_location)
{
    capture_stderr_start();

    ErrorReporter* reporter = error_reporter_create();
    const char* source = "let x = y + z;";
    error_set_source(reporter, "math.swift", source);

    // Error at 'y' (position 8)
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_SEMANTIC,
                        "math.swift", 1, 8, "Undefined variable 'y'");

    capture_stderr_end();

    // Check location marking
    TEST_ASSERT(suite, strstr(captured_error, "math.swift") != NULL, "error_location");
    TEST_ASSERT(suite, strstr(captured_error, "Undefined variable") != NULL, "error_location");

    error_reporter_destroy(reporter);
}

DEFINE_TEST(multiple_errors)
{
    capture_stderr_start();

    ErrorReporter* reporter = error_reporter_create();
    const char* source = "var x = 1;\nvar y = 2;\nvar z = 3;";
    error_set_source(reporter, "test.swift", source);

    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "test.swift", 1, 4, "First error");
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "test.swift", 2, 15, "Second error");
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "test.swift", 3, 26, "Third error");

    capture_stderr_end();

    // All errors should be present
    TEST_ASSERT(suite, strstr(captured_error, "First error") != NULL, "multiple_errors");
    TEST_ASSERT(suite, strstr(captured_error, "Second error") != NULL, "multiple_errors");
    TEST_ASSERT(suite, strstr(captured_error, "Third error") != NULL, "multiple_errors");

    error_reporter_destroy(reporter);
}

// Define test suite
TEST_SUITE(error_reporter_unit)
    TEST_CASE(simple_error, "Simple Error")
    TEST_CASE(error_with_context, "Error with Context")
    TEST_CASE(warning_message, "Warning Message")
    TEST_CASE(multiline_error, "Multiline Error")
    TEST_CASE(error_location, "Error Location")
    TEST_CASE(multiple_errors, "Multiple Errors")
END_TEST_SUITE(error_reporter_unit)
