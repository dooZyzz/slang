#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "utils/error.h"

// Helper to capture stderr output
static char captured_error[4096];
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

// Test error counting functions
DEFINE_TEST(error_counting)
{
    ErrorReporter* reporter = error_reporter_create();
    
    // Initially no errors
    TEST_ASSERT(suite, error_count(reporter) == 0, "error_counting");
    TEST_ASSERT(suite, warning_count(reporter) == 0, "error_counting");
    TEST_ASSERT(suite, !error_has_errors(reporter), "error_counting");
    TEST_ASSERT(suite, !error_has_fatal(reporter), "error_counting");
    
    // Add some errors
    capture_stderr_start();
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "test.swift", 1, 1, "Error 1");
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "test.swift", 2, 1, "Error 2");
    capture_stderr_end();
    
    TEST_ASSERT(suite, error_count(reporter) == 2, "error_counting");
    TEST_ASSERT(suite, error_has_errors(reporter), "error_counting");
    
    // Add warnings
    capture_stderr_start();
    error_report_simple(reporter, ERROR_LEVEL_WARNING, ERROR_PHASE_SEMANTIC,
                        "test.swift", 3, 1, "Warning 1");
    error_report_simple(reporter, ERROR_LEVEL_WARNING, ERROR_PHASE_SEMANTIC,
                        "test.swift", 4, 1, "Warning 2");
    error_report_simple(reporter, ERROR_LEVEL_WARNING, ERROR_PHASE_SEMANTIC,
                        "test.swift", 5, 1, "Warning 3");
    capture_stderr_end();
    
    TEST_ASSERT(suite, warning_count(reporter) == 3, "error_counting");
    TEST_ASSERT(suite, error_count(reporter) == 2, "error_counting"); // Errors unchanged
    
    error_reporter_destroy(reporter);
}

// Test fatal error handling
DEFINE_TEST(fatal_error)
{
    ErrorReporter* reporter = error_reporter_create();
    
    TEST_ASSERT(suite, !error_has_fatal(reporter), "fatal_error");
    
    capture_stderr_start();
    error_report_simple(reporter, ERROR_LEVEL_FATAL, ERROR_PHASE_RUNTIME,
                        "test.swift", 1, 1, "Fatal error occurred");
    capture_stderr_end();
    
    TEST_ASSERT(suite, error_has_fatal(reporter), "fatal_error");
    TEST_ASSERT(suite, error_count(reporter) == 1, "fatal_error");
    
    // Further errors should be ignored after fatal
    capture_stderr_start();
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_RUNTIME,
                        "test.swift", 2, 1, "This should be ignored");
    capture_stderr_end();
    
    TEST_ASSERT(suite, error_count(reporter) == 1, "fatal_error"); // Count unchanged
    
    error_reporter_destroy(reporter);
}

// Test error clearing
DEFINE_TEST(clear_errors)
{
    ErrorReporter* reporter = error_reporter_create();
    
    // Add various errors and warnings
    capture_stderr_start();
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "test.swift", 1, 1, "Error 1");
    error_report_simple(reporter, ERROR_LEVEL_WARNING, ERROR_PHASE_SEMANTIC,
                        "test.swift", 2, 1, "Warning 1");
    error_report_simple(reporter, ERROR_LEVEL_FATAL, ERROR_PHASE_RUNTIME,
                        "test.swift", 3, 1, "Fatal error");
    capture_stderr_end();
    
    TEST_ASSERT(suite, error_count(reporter) == 2, "clear_errors"); // 1 error + 1 fatal
    TEST_ASSERT(suite, warning_count(reporter) == 1, "clear_errors");
    TEST_ASSERT(suite, error_has_fatal(reporter), "clear_errors");
    
    // Clear all errors
    error_clear(reporter);
    
    TEST_ASSERT(suite, error_count(reporter) == 0, "clear_errors");
    TEST_ASSERT(suite, warning_count(reporter) == 0, "clear_errors");
    TEST_ASSERT(suite, !error_has_fatal(reporter), "clear_errors");
    TEST_ASSERT(suite, !error_has_errors(reporter), "clear_errors");
    
    error_reporter_destroy(reporter);
}

// Test color enable/disable
DEFINE_TEST(color_control)
{
    ErrorReporter* reporter = error_reporter_create();
    error_set_source(reporter, "test.swift", "var x = 42;");
    
    // Test with color enabled (default)
    capture_stderr_start();
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "test.swift", 1, 4, "Color test");
    capture_stderr_end();
    
    // Should contain ANSI escape codes
    TEST_ASSERT(suite, strstr(captured_error, "\033[") != NULL, "color_control");
    
    // Disable color
    error_enable_color(reporter, false);
    
    capture_stderr_start();
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "test.swift", 1, 4, "No color test");
    capture_stderr_end();
    
    // Should NOT contain ANSI escape codes
    TEST_ASSERT(suite, strstr(captured_error, "\033[") == NULL, "color_control");
    
    error_reporter_destroy(reporter);
}

// Test max errors limit
DEFINE_TEST(max_errors_limit)
{
    ErrorReporter* reporter = error_reporter_create();
    error_set_max_errors(reporter, 3);
    
    // Add errors up to the limit
    capture_stderr_start();
    for (int i = 1; i <= 5; i++) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Error %d", i);
        error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                            "test.swift", i, 1, msg);
    }
    capture_stderr_end();
    
    // Should only have 3 errors (the limit)
    TEST_ASSERT(suite, error_count(reporter) == 3, "max_errors_limit");
    
    // Check for "too many errors" message
    TEST_ASSERT(suite, strstr(captured_error, "Too many errors") != NULL, "max_errors_limit");
    
    error_reporter_destroy(reporter);
}

// Test error with suggestion
DEFINE_TEST(error_with_suggestion)
{
    ErrorReporter* reporter = error_reporter_create();
    error_set_source(reporter, "test.swift", "var x = 42;");
    
    capture_stderr_start();
    
    ErrorInfo info = {
        .level = ERROR_LEVEL_ERROR,
        .phase = ERROR_PHASE_SEMANTIC,
        .message = "Variable 'y' is not defined",
        .location = {
            .filename = "test.swift",
            .line = 1,
            .column = 4,
            .length = 1
        },
        .suggestion = "Did you mean 'x'?"
    };
    
    error_report(reporter, &info);
    capture_stderr_end();
    
    // Check that suggestion appears
    TEST_ASSERT(suite, strstr(captured_error, "suggestion:") != NULL, "error_with_suggestion");
    TEST_ASSERT(suite, strstr(captured_error, "Did you mean 'x'?") != NULL, "error_with_suggestion");
    
    error_reporter_destroy(reporter);
}

// Test multiple source files
DEFINE_TEST(multiple_source_files)
{
    ErrorReporter* reporter = error_reporter_create();
    
    // Set sources for multiple files
    error_set_source(reporter, "file1.swift", "let a = 1;");
    error_set_source(reporter, "file2.swift", "let b = 2;");
    error_set_source(reporter, "file3.swift", "let c = 3;");
    
    // Report errors in different files
    capture_stderr_start();
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "file1.swift", 1, 4, "Error in file1");
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "file2.swift", 1, 4, "Error in file2");
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "file3.swift", 1, 4, "Error in file3");
    capture_stderr_end();
    
    // Verify all files are referenced
    TEST_ASSERT(suite, strstr(captured_error, "file1.swift") != NULL, "multiple_source_files");
    TEST_ASSERT(suite, strstr(captured_error, "file2.swift") != NULL, "multiple_source_files");
    TEST_ASSERT(suite, strstr(captured_error, "file3.swift") != NULL, "multiple_source_files");
    
    // Update existing source
    error_set_source(reporter, "file1.swift", "let a = 100;");
    
    capture_stderr_start();
    error_report_simple(reporter, ERROR_LEVEL_ERROR, ERROR_PHASE_PARSER,
                        "file1.swift", 1, 8, "Updated source error");
    capture_stderr_end();
    
    // Should show updated source
    TEST_ASSERT(suite, strstr(captured_error, "100") != NULL, "multiple_source_files");
    
    error_reporter_destroy(reporter);
}

// Test error location with different lengths
DEFINE_TEST(error_location_lengths)
{
    ErrorReporter* reporter = error_reporter_create();
    error_set_source(reporter, "test.swift", "let longVariableName = 42;");
    
    capture_stderr_start();
    
    ErrorInfo info = {
        .level = ERROR_LEVEL_ERROR,
        .phase = ERROR_PHASE_SEMANTIC,
        .message = "Variable name too long",
        .location = {
            .filename = "test.swift",
            .line = 1,
            .column = 4,
            .length = 16  // Length of "longVariableName"
        },
        .suggestion = NULL
    };
    
    error_report(reporter, &info);
    capture_stderr_end();
    
    // Should have underline for the full length
    TEST_ASSERT(suite, strstr(captured_error, "^~~~~~~~~~~~~~~~") != NULL, "error_location_lengths");
    
    error_reporter_destroy(reporter);
}

// Test null safety
DEFINE_TEST(null_safety)
{
    // Test null reporter
    TEST_ASSERT(suite, error_count(NULL) == 0, "null_safety");
    TEST_ASSERT(suite, warning_count(NULL) == 0, "null_safety");
    TEST_ASSERT(suite, !error_has_errors(NULL), "null_safety");
    TEST_ASSERT(suite, !error_has_fatal(NULL), "null_safety");
    
    // These should not crash
    error_clear(NULL);
    error_enable_color(NULL, false);
    error_set_max_errors(NULL, 10);
    error_set_source(NULL, "test.swift", "code");
    
    ErrorReporter* reporter = error_reporter_create();
    
    // Test null parameters
    error_set_source(reporter, NULL, "code");
    error_set_source(reporter, "test.swift", NULL);
    error_report(reporter, NULL);
    
    ErrorInfo info = {.level = ERROR_LEVEL_ERROR};
    error_report(NULL, &info);
    
    error_reporter_destroy(reporter);
}

// Define test suite
TEST_SUITE(error_advanced_unit)
    TEST_CASE(error_counting, "Error Counting")
    TEST_CASE(fatal_error, "Fatal Error Handling")
    TEST_CASE(clear_errors, "Clear Errors")
    TEST_CASE(color_control, "Color Control")
    TEST_CASE(max_errors_limit, "Max Errors Limit")
    TEST_CASE(error_with_suggestion, "Error with Suggestion")
    TEST_CASE(multiple_source_files, "Multiple Source Files")
    TEST_CASE(error_location_lengths, "Error Location Lengths")
    TEST_CASE(null_safety, "Null Safety")
END_TEST_SUITE(error_advanced_unit)