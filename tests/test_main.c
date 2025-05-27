#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"

// Test suite registrations
TEST_SUITE_REGISTRATION(lexer_unit)
TEST_SUITE_REGISTRATION(vm_unit)
TEST_SUITE_REGISTRATION(symbol_table_unit)
TEST_SUITE_REGISTRATION(error_reporter_unit)
TEST_SUITE_REGISTRATION(integration)
TEST_SUITE_REGISTRATION(array_methods_unit)
TEST_SUITE_REGISTRATION(string_interp_unit)
TEST_SUITE_REGISTRATION(for_loop_unit)
TEST_SUITE_REGISTRATION(modulo_unit)
TEST_SUITE_REGISTRATION(array_assign_unit)
TEST_SUITE_REGISTRATION(ast_unit)
TEST_SUITE_REGISTRATION(string_pool_unit)
TEST_SUITE_REGISTRATION(object_unit)
TEST_SUITE_REGISTRATION(error_advanced_unit)
TEST_SUITE_REGISTRATION(syntax_unit)

// Static test suite registry
static TestSuiteEntry test_suites[] = {
    REGISTER_TEST_SUITE(lexer_unit)
    REGISTER_TEST_SUITE(vm_unit)
    REGISTER_TEST_SUITE(symbol_table_unit)
    REGISTER_TEST_SUITE(error_reporter_unit)
    REGISTER_TEST_SUITE(integration)
    REGISTER_TEST_SUITE(array_methods_unit)
    REGISTER_TEST_SUITE(string_interp_unit)
    REGISTER_TEST_SUITE(for_loop_unit)
    REGISTER_TEST_SUITE(modulo_unit)
    REGISTER_TEST_SUITE(array_assign_unit)
    REGISTER_TEST_SUITE(ast_unit)
    REGISTER_TEST_SUITE(string_pool_unit)
    REGISTER_TEST_SUITE(object_unit)
    REGISTER_TEST_SUITE(error_advanced_unit)
    REGISTER_TEST_SUITE(syntax_unit)
};

static const size_t test_suite_count = sizeof(test_suites) / sizeof(test_suites[0]);

// Run specific test suite by name
static int run_specific_suite(const char* suite_name) {
    for (size_t i = 0; i < test_suite_count; i++) {
        if (strcmp(test_suites[i].name, suite_name) == 0) {
            TestSuite* suite = test_suites[i].runner();
            test_suite_print_results(suite);
            
            TestSuite* suites[] = {suite};
            test_suite_print_summary(suites, 1);
            
            int failed = suite->failed;
            test_suite_destroy(suite);
            
            return failed > 0 ? 1 : 0;
        }
    }
    
    fprintf(stderr, "Error: Test suite '%s' not found\n", suite_name);
    fprintf(stderr, "Available test suites:\n");
    for (size_t i = 0; i < test_suite_count; i++) {
        fprintf(stderr, "  - %s\n", test_suites[i].name);
    }
    return 1;
}

// Run all test suites
static int run_all_suites(bool verbose) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║               LANG COMPREHENSIVE TEST SUITE                  ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    TestSuite** all_suites = malloc(test_suite_count * sizeof(TestSuite*));
    
    // Run all test suites
    for (size_t i = 0; i < test_suite_count; i++) {
        all_suites[i] = test_suites[i].runner();
        if (verbose && all_suites[i]) {
            test_suite_print_results(all_suites[i]);
        }
    }
    
    // Always print summary
    test_suite_print_summary(all_suites, test_suite_count);
    
    // Calculate total failures
    int total_failed = 0;
    for (size_t i = 0; i < test_suite_count; i++) {
        if (all_suites[i]) {
            total_failed += all_suites[i]->failed;
        }
    }
    
    // Cleanup
    for (size_t i = 0; i < test_suite_count; i++) {
        if (all_suites[i]) {
            test_suite_destroy(all_suites[i]);
        }
    }
    free(all_suites);
    
    return total_failed > 0 ? 1 : 0;
}

// Print usage information
static void print_usage(const char* program_name) {
    printf("Usage: %s [options] [suite_name]\n", program_name);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help      Show this help message\n");
    printf("  -v, --verbose   Show detailed test results\n");
    printf("  -l, --list      List available test suites\n");
    printf("\n");
    printf("If no suite name is provided, all test suites will be run.\n");
    printf("\n");
    printf("Available test suites:\n");
    for (size_t i = 0; i < test_suite_count; i++) {
        printf("  - %s\n", test_suites[i].name);
    }
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    const char* suite_name = NULL;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            printf("Available test suites:\n");
            for (size_t j = 0; j < test_suite_count; j++) {
                printf("  - %s\n", test_suites[j].name);
            }
            return 0;
        } else if (argv[i][0] != '-') {
            suite_name = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Run tests
    if (suite_name) {
        return run_specific_suite(suite_name);
    } else {
        return run_all_suites(verbose);
    }
}