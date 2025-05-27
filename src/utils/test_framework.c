#include "utils/test_framework.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void add_result(TestSuite* suite, const char* name, bool passed,
                       const char* error_message, const char* file, int line)
{
    if (suite->result_count >= suite->result_capacity)
    {
        suite->result_capacity = suite->result_capacity ? suite->result_capacity * 2 : 16;
        suite->results = realloc(suite->results, sizeof(TestResult) * suite->result_capacity);
    }

    TestResult* result = &suite->results[suite->result_count++];
    result->name = name;
    result->passed = passed;
    result->error_message = error_message;
    result->file = file;
    result->line = line;
    result->duration_ms = 0.0;

    if (passed)
    {
        suite->passed++;
    }
    else
    {
        suite->failed++;
    }
}

TestSuite* test_suite_create(const char* name)
{
    TestSuite* suite = malloc(sizeof(TestSuite));
    suite->name = name;
    suite->results = NULL;
    suite->result_count = 0;
    suite->result_capacity = 0;
    suite->passed = 0;
    suite->failed = 0;
    suite->start_time = clock();
    return suite;
}

void test_suite_destroy(TestSuite* suite)
{
    if (suite)
    {
        free(suite->results);
        free(suite);
    }
}

void test_suite_run(TestSuite* suite, TestCase* cases, size_t count)
{
printf("\n%s%s╔══ %s%s%sRunning Test Suite: %s%s%s %s══╗%s\n\n",
       COLOR_BOLD, COLOR_CYAN, COLOR_RESET, COLOR_WHITE, COLOR_BOLD, COLOR_RESET, COLOR_BOLD, suite->name, COLOR_CYAN,
       COLOR_RESET);
for (size_t i = 0; i < count; i++)
    {
        printf("  %s%s%s%-50s%s ", COLOR_WHITE, COLOR_DIM, COLOR_DIM, cases[i].name, COLOR_RESET);
        fflush(stdout);

        size_t results_before = suite->result_count;
        clock_t start = clock();

        cases[i].func(suite);

        clock_t end = clock();
        double duration = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;

        // Check if test added any results
        if (suite->result_count > results_before)
        {
            // Test added its own results
            bool all_passed = true;
            for (size_t j = results_before; j < suite->result_count; j++)
            {
                if (!suite->results[j].passed)
                {
                    all_passed = false;
                    break;
                }
            }

            if (all_passed)
            {
                printf("%s✓%s %s(%.2f ms)%s\n",
                       COLOR_GREEN, COLOR_RESET, COLOR_DIM, duration, COLOR_RESET);
            }
            else
            {
                printf("%s✗%s %s(%.2f ms)%s\n",
                       COLOR_RED, COLOR_RESET, COLOR_DIM, duration, COLOR_RESET);
            }
        }
        else
        {
            // Test didn't add results, assume it passed
            add_result(suite, cases[i].name, true, NULL, __FILE__, __LINE__);
            printf("%s✓%s %s(%.2f ms)%s\n",
                   COLOR_GREEN, COLOR_RESET, COLOR_DIM, duration, COLOR_RESET);
        }
    }
}

void test_suite_print_results(TestSuite* suite)
{
    if (suite->failed > 0)
    {
        printf("\n%s%sFailed Tests:%s\n", COLOR_BOLD, COLOR_RED, COLOR_RESET);
        for (size_t i = 0; i < suite->result_count; i++)
        {
            if (!suite->results[i].passed)
            {
                printf("  %s✗ %s%s\n", COLOR_RED, suite->results[i].name, COLOR_RESET);
                if (suite->results[i].error_message)
                {
                    printf("    %sError: %s%s\n", COLOR_DIM,
                           suite->results[i].error_message, COLOR_RESET);
                }
                printf("    %sLocation: %s:%d%s\n", COLOR_DIM,
                       suite->results[i].file, suite->results[i].line, COLOR_RESET);
            }
        }
    }

    clock_t end_time = clock();
    double total_duration = ((double)(end_time - suite->start_time) / CLOCKS_PER_SEC) * 1000.0;

    printf("\n%s%sSummary for %s:%s\n", COLOR_BOLD, COLOR_WHITE, suite->name, COLOR_RESET);
    printf("  Total:  %zu\n", suite->passed + suite->failed);
    printf("  %sPassed: %zu%s\n", COLOR_GREEN, suite->passed, COLOR_RESET);
    printf("  %sFailed: %zu%s\n", COLOR_RED, suite->failed, COLOR_RESET);
    printf("  Time:   %.2f ms\n", total_duration);
}

void test_suite_print_summary(TestSuite** suites, size_t count)
{
    size_t total_passed = 0;
    size_t total_failed = 0;
    double total_time = 0.0;

    printf("\n%s%s╔═══════════════════════════════════════════════════════════════╗%s\n",
           COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    printf("%s%s║                      TEST SUMMARY TABLE                       ║%s\n",
           COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    printf("%s%s╠═══════════════════════════════════════════════════════════════╣%s\n",
           COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    printf("%s║%s%s %-30s │ %6s │ %6s │ %8s   %s║%s\n",
           COLOR_CYAN, COLOR_BOLD, COLOR_WHITE, "Suite Name", "Passed", "Failed", "Time(ms)", COLOR_CYAN, COLOR_RESET);
    printf("%s%s╠═══════════════════════════════════════════════════════════════╣%s\n",
           COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

    for (size_t i = 0; i < count; i++)
    {
        TestSuite* suite = suites[i];
        double suite_time = ((double)(clock() - suite->start_time) / CLOCKS_PER_SEC) * 1000.0;

        // const char* status_color = suite->failed > 0 ? COLOR_RED : COLOR_GREEN;
        printf("%s║%s %-30s │ %s%6zu%s │ %s%6zu%s │ %8.2f   %s║%s\n",
               COLOR_CYAN, COLOR_RESET,
               suite->name,
               COLOR_GREEN, suite->passed, COLOR_RESET,
               suite->failed > 0 ? COLOR_RED : COLOR_DIM, suite->failed, COLOR_RESET,
               suite_time,
               COLOR_CYAN, COLOR_RESET);

        total_passed += suite->passed;
        total_failed += suite->failed;
        total_time += suite_time;
    }

    printf("%s%s╠═══════════════════════════════════════════════════════════════╣%s\n",
           COLOR_BOLD, COLOR_CYAN, COLOR_RESET);
    printf("%s║%s%s %-30s │ %s%6zu%s │ %s%6zu%s │ %8.2f   %s║%s\n",
           COLOR_CYAN, COLOR_BOLD, COLOR_WHITE, "TOTAL",
           COLOR_GREEN, total_passed, COLOR_RESET,
           total_failed > 0 ? COLOR_RED : COLOR_DIM, total_failed, COLOR_RESET,
           total_time, COLOR_CYAN, COLOR_RESET);
    printf("%s%s╚═══════════════════════════════════════════════════════════════╝%s\n\n",
           COLOR_BOLD, COLOR_CYAN, COLOR_RESET);

    if (total_failed == 0)
    {
        printf("%s%s✓ All tests passed!%s\n\n", COLOR_BOLD, COLOR_GREEN, COLOR_RESET);
    }
    else
    {
        printf("%s%s✗ %zu tests failed%s\n\n", COLOR_BOLD, COLOR_RED, total_failed, COLOR_RESET);
    }
}

void test_assert(TestSuite* suite, bool condition, const char* test_name,
                 const char* message, const char* file, int line)
{
    if (!condition)
    {
        add_result(suite, test_name, false, message, file, line);
    }
}

void test_assert_equal_int(TestSuite* suite, int expected, int actual,
                           const char* test_name, const char* file, int line)
{
    if (expected != actual)
    {
        static char buffer[256]; // Make static to persist beyond function scope
        snprintf(buffer, sizeof(buffer), "Expected %d, but got %d", expected, actual);
        add_result(suite, test_name, false, buffer, file, line);
    }
}

void test_assert_equal_str(TestSuite* suite, const char* expected, const char* actual,
                           const char* test_name, const char* file, int line)
{
    if (expected == NULL && actual == NULL) return;

    if (expected == NULL || actual == NULL || strcmp(expected, actual) != 0)
    {
        static char buffer[512]; // Make static to persist beyond function scope
        snprintf(buffer, sizeof(buffer), "Expected \"%s\", but got \"%s\"",
                 expected ? expected : "NULL", actual ? actual : "NULL");
        add_result(suite, test_name, false, buffer, file, line);
    }
}

void test_assert_null(TestSuite* suite, void* ptr, const char* test_name,
                      const char* file, int line)
{
    if (ptr != NULL)
    {
        add_result(suite, test_name, false, "Expected NULL, but got non-NULL", file, line);
    }
}

void test_assert_not_null(TestSuite* suite, void* ptr, const char* test_name,
                          const char* file, int line)
{
    if (ptr == NULL)
    {
        add_result(suite, test_name, false, "Expected non-NULL, but got NULL", file, line);
    }
}

void test_assert_equal_double(TestSuite* suite, double expected, double actual, double epsilon,
                             const char* test_name, const char* file, int line)
{
    double diff = expected - actual;
    if (diff < 0) diff = -diff;
    
    if (diff > epsilon)
    {
        static char buffer[256];
        snprintf(buffer, sizeof(buffer), "Expected %f, but got %f (diff: %f > epsilon: %f)", 
                 expected, actual, diff, epsilon);
        add_result(suite, test_name, false, buffer, file, line);
    }
}
