#ifndef LANG_TEST_FRAMEWORK_H
#define LANG_TEST_FRAMEWORK_H

#include <stdbool.h>
#include <stddef.h>
#include <time.h>

// ANSI color codes for terminal output
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

// Test result structure
typedef struct {
    const char* name;
    bool passed;
    const char* error_message;
    const char* file;
    int line;
    double duration_ms;
} TestResult;

// Test suite structure
typedef struct {
    const char* name;
    TestResult* results;
    size_t result_count;
    size_t result_capacity;
    size_t passed;
    size_t failed;
    clock_t start_time;
} TestSuite;

// Test function signature
typedef void (*TestFunc)(TestSuite* suite);

// Test case structure
typedef struct {
    const char* name;
    TestFunc func;
} TestCase;

// Test framework API
TestSuite* test_suite_create(const char* name);
void test_suite_destroy(TestSuite* suite);
void test_suite_run(TestSuite* suite, TestCase* cases, size_t count);
void test_suite_print_results(TestSuite* suite);
void test_suite_print_summary(TestSuite** suites, size_t count);

// Test assertions
void test_assert(TestSuite* suite, bool condition, const char* test_name, 
                const char* message, const char* file, int line);
void test_assert_equal_int(TestSuite* suite, int expected, int actual, 
                          const char* test_name, const char* file, int line);
void test_assert_equal_str(TestSuite* suite, const char* expected, const char* actual,
                          const char* test_name, const char* file, int line);
void test_assert_null(TestSuite* suite, void* ptr, const char* test_name,
                     const char* file, int line);
void test_assert_not_null(TestSuite* suite, void* ptr, const char* test_name,
                         const char* file, int line);
void test_assert_equal_double(TestSuite* suite, double expected, double actual, double epsilon,
                             const char* test_name, const char* file, int line);

// Convenience macros
#define TEST_ASSERT(suite, condition, test_name) \
    test_assert(suite, condition, test_name, #condition, __FILE__, __LINE__)

#define TEST_ASSERT_MSG(suite, condition, test_name, msg) \
    test_assert(suite, condition, test_name, msg, __FILE__, __LINE__)

#define TEST_ASSERT_EQUAL_INT(suite, expected, actual, test_name) \
    test_assert_equal_int(suite, expected, actual, test_name, __FILE__, __LINE__)

#define TEST_ASSERT_EQUAL_STR(suite, expected, actual, test_name) \
    test_assert_equal_str(suite, expected, actual, test_name, __FILE__, __LINE__)

#define TEST_ASSERT_NULL(suite, ptr, test_name) \
    test_assert_null(suite, ptr, test_name, __FILE__, __LINE__)

#define TEST_ASSERT_NOT_NULL(suite, ptr, test_name) \
    test_assert_not_null(suite, ptr, test_name, __FILE__, __LINE__)

#define TEST_ASSERT_EQUAL_STRING(suite, expected, actual, test_name) \
    test_assert_equal_str(suite, expected, actual, test_name, __FILE__, __LINE__)

#define TEST_ASSERT_STRING_EQUAL(suite, expected, actual, test_name) \
    test_assert_equal_str(suite, expected, actual, test_name, __FILE__, __LINE__)

#define TEST_ASSERT_TRUE(suite, condition, test_name) \
    test_assert(suite, (condition), test_name, #condition " is not true", __FILE__, __LINE__)

#define TEST_ASSERT_FALSE(suite, condition, test_name) \
    test_assert(suite, !(condition), test_name, #condition " is not false", __FILE__, __LINE__)

#define TEST_ASSERT_EQUAL_DOUBLE(suite, expected, actual, epsilon, test_name) \
    test_assert_equal_double(suite, expected, actual, epsilon, test_name, __FILE__, __LINE__)

// Test running macros
#define RUN_TEST(suite, test_func) do { \
    clock_t start = clock(); \
    test_func(suite); \
    clock_t end = clock(); \
    double duration = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0; \
    if (suite->result_count > 0) { \
        suite->results[suite->result_count - 1].duration_ms = duration; \
    } \
} while(0)

#define DEFINE_TEST(name) static void test_##name(TestSuite* suite)

#endif // LANG_TEST_FRAMEWORK_H
