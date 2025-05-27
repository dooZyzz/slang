#ifndef LANG_TEST_MACROS_H
#define LANG_TEST_MACROS_H

#include "utils/test_framework.h"
#include <stdio.h>

// Export annotations for dynamic loading
#ifdef _WIN32
    #define TEST_EXPORT __declspec(dllexport)
    #define TEST_IMPORT __declspec(dllimport)
#else
    #define TEST_EXPORT __attribute__((visibility("default")))
    #define TEST_IMPORT
#endif

#ifdef BUILDING_TEST_LIB
    #define TEST_API TEST_EXPORT
#else
    #define TEST_API TEST_IMPORT
#endif

// Test suite definition macro
#define TEST_SUITE(suite_name) \
    static TestCase suite_name##_test_cases[]; \
    static size_t suite_name##_test_count; \
    static const char* suite_name##_suite_name = #suite_name; \
    \
    TEST_API TestSuite* run_##suite_name##_tests(void) { \
        TestSuite* suite = test_suite_create(suite_name##_suite_name); \
        test_suite_run(suite, suite_name##_test_cases, suite_name##_test_count); \
        return suite; \
    } \
    \
    static TestCase suite_name##_test_cases[] = {

// Test case definition macro
#define TEST_CASE(test_name, display_name) \
    {display_name, test_##test_name},

// End test suite definition
#define END_TEST_SUITE(suite_name) \
    }; \
    static size_t suite_name##_test_count = \
        sizeof(suite_name##_test_cases) / sizeof(suite_name##_test_cases[0]); \
    \
    /* Automatically add main function when building as standalone */ \
    TEST_STANDALONE_MAIN_IF_ENABLED(suite_name)

// Define test function with proper signature
#define DEFINE_TEST(name) static void test_##name(TestSuite* suite)

// Standalone test runner macro
#define STANDALONE_TEST_RUNNER(suite_name) \
    int main(void) { \
        TestSuite* suite = run_##suite_name##_tests(); \
        test_suite_print_results(suite); \
        \
        TestSuite* suites[] = {suite}; \
        test_suite_print_summary(suites, 1); \
        \
        int failed = suite->failed; \
        test_suite_destroy(suite); \
        \
        return failed > 0 ? 1 : 0; \
    }

// Conditional macro that adds main only when TEST_STANDALONE is defined
#ifdef TEST_STANDALONE
    #define TEST_STANDALONE_MAIN_IF_ENABLED(suite_name) STANDALONE_TEST_RUNNER(suite_name)
#else
    #define TEST_STANDALONE_MAIN_IF_ENABLED(suite_name) /* No standalone runner */
#endif

// Keep old macros for backward compatibility
#ifdef BUILD_STANDALONE
    #define OPTIONAL_STANDALONE_RUNNER(suite_name) /* Already handled by END_TEST_SUITE */
#else
    #define OPTIONAL_STANDALONE_RUNNER(suite_name)
#endif

// Main test runner registration macro
#define TEST_SUITE_REGISTRATION(suite_name) \
    extern TestSuite* run_##suite_name##_tests(void);

#define REGISTER_TEST_SUITE(suite_name) \
    { #suite_name, run_##suite_name##_tests },

// Test runner structure
typedef struct {
    const char* name;
    TestSuite* (*runner)(void);
} TestSuiteEntry;

// Dynamic test loader helper
#ifdef __linux__
    #include <dlfcn.h>
    #define LOAD_LIBRARY(path) dlopen(path, RTLD_LAZY)
    #define GET_FUNCTION(lib, name) dlsym(lib, name)
    #define CLOSE_LIBRARY(lib) dlclose(lib)
    typedef void* LibraryHandle;
#elif defined(_WIN32)
    #include <windows.h>
    #define LOAD_LIBRARY(path) LoadLibrary(path)
    #define GET_FUNCTION(lib, name) GetProcAddress(lib, name)
    #define CLOSE_LIBRARY(lib) FreeLibrary(lib)
    typedef HMODULE LibraryHandle;
#elif defined(__APPLE__)
    #include <dlfcn.h>
    #define LOAD_LIBRARY(path) dlopen(path, RTLD_LAZY)
    #define GET_FUNCTION(lib, name) dlsym(lib, name)
    #define CLOSE_LIBRARY(lib) dlclose(lib)
    typedef void* LibraryHandle;
#endif

// Dynamic test loader
static inline TestSuite* load_test_suite_dynamic(const char* library_path, const char* suite_name) {
    LibraryHandle lib = LOAD_LIBRARY(library_path);
    if (!lib) return NULL;
    
    char function_name[256];
    snprintf(function_name, sizeof(function_name), "run_%s_tests", suite_name);
    
    typedef TestSuite* (*TestRunner)(void);
    TestRunner runner = (TestRunner)GET_FUNCTION(lib, function_name);
    
    if (!runner) {
        CLOSE_LIBRARY(lib);
        return NULL;
    }
    
    return runner();
}

#endif // LANG_TEST_MACROS_H
