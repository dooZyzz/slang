#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "runtime/core/string_pool.h"

// Test string pool initialization and cleanup
DEFINE_TEST(init_and_free)
{
    StringPool pool;
    string_pool_init(&pool);
    
    TEST_ASSERT(suite, pool.buckets != NULL, "init_and_free");
    TEST_ASSERT(suite, pool.bucket_count == 32, "init_and_free");  // INITIAL_BUCKET_COUNT
    TEST_ASSERT(suite, pool.entry_count == 0, "init_and_free");
    TEST_ASSERT(suite, pool.all_strings == NULL, "init_and_free");
    
    string_pool_free(&pool);
    
    TEST_ASSERT(suite, pool.buckets == NULL, "init_and_free");
    TEST_ASSERT(suite, pool.bucket_count == 0, "init_and_free");
    TEST_ASSERT(suite, pool.entry_count == 0, "init_and_free");
    TEST_ASSERT(suite, pool.all_strings == NULL, "init_and_free");
}

// Test string interning
DEFINE_TEST(intern_string)
{
    StringPool pool;
    string_pool_init(&pool);
    
    const char* test_str = "Hello, World!";
    size_t len = strlen(test_str);
    
    char* interned1 = string_pool_intern(&pool, test_str, len);
    TEST_ASSERT(suite, interned1 != NULL, "intern_string");
    TEST_ASSERT(suite, strcmp(interned1, test_str) == 0, "intern_string");
    TEST_ASSERT(suite, pool.entry_count == 1, "intern_string");
    
    // Interning the same string should return the same pointer
    char* interned2 = string_pool_intern(&pool, test_str, len);
    TEST_ASSERT(suite, interned2 == interned1, "intern_string");
    TEST_ASSERT(suite, pool.entry_count == 1, "intern_string");  // Count shouldn't increase
    
    string_pool_free(&pool);
}

// Test interning multiple different strings
DEFINE_TEST(intern_multiple)
{
    StringPool pool;
    string_pool_init(&pool);
    
    char* str1 = string_pool_intern(&pool, "first", 5);
    char* str2 = string_pool_intern(&pool, "second", 6);
    char* str3 = string_pool_intern(&pool, "third", 5);
    
    TEST_ASSERT(suite, str1 != NULL, "intern_multiple");
    TEST_ASSERT(suite, str2 != NULL, "intern_multiple");
    TEST_ASSERT(suite, str3 != NULL, "intern_multiple");
    TEST_ASSERT(suite, str1 != str2, "intern_multiple");
    TEST_ASSERT(suite, str2 != str3, "intern_multiple");
    TEST_ASSERT(suite, str1 != str3, "intern_multiple");
    TEST_ASSERT(suite, pool.entry_count == 3, "intern_multiple");
    
    // Test that they contain correct values
    TEST_ASSERT(suite, strcmp(str1, "first") == 0, "intern_multiple");
    TEST_ASSERT(suite, strcmp(str2, "second") == 0, "intern_multiple");
    TEST_ASSERT(suite, strcmp(str3, "third") == 0, "intern_multiple");
    
    string_pool_free(&pool);
}

// Test empty string interning
DEFINE_TEST(intern_empty)
{
    StringPool pool;
    string_pool_init(&pool);
    
    char* empty = string_pool_intern(&pool, "", 0);
    TEST_ASSERT(suite, empty != NULL, "intern_empty");
    TEST_ASSERT(suite, strlen(empty) == 0, "intern_empty");
    TEST_ASSERT(suite, pool.entry_count == 1, "intern_empty");
    
    // Interning another empty string should return same pointer
    char* empty2 = string_pool_intern(&pool, "", 0);
    TEST_ASSERT(suite, empty2 == empty, "intern_empty");
    
    string_pool_free(&pool);
}

// Test strings with same content but different lengths
DEFINE_TEST(intern_prefix)
{
    StringPool pool;
    string_pool_init(&pool);
    
    char* str1 = string_pool_intern(&pool, "hello", 5);
    char* str2 = string_pool_intern(&pool, "hell", 4);
    char* str3 = string_pool_intern(&pool, "hello world", 11);
    
    // These should all be different strings
    TEST_ASSERT(suite, str1 != str2, "intern_prefix");
    TEST_ASSERT(suite, str1 != str3, "intern_prefix");
    TEST_ASSERT(suite, str2 != str3, "intern_prefix");
    TEST_ASSERT(suite, pool.entry_count == 3, "intern_prefix");
    
    string_pool_free(&pool);
}

// Test string_pool_create (currently same as intern)
DEFINE_TEST(create_string)
{
    StringPool pool;
    string_pool_init(&pool);
    
    const char* test_str = "Created String";
    size_t len = strlen(test_str);
    
    char* created = string_pool_create(&pool, test_str, len);
    TEST_ASSERT(suite, created != NULL, "create_string");
    TEST_ASSERT(suite, strcmp(created, test_str) == 0, "create_string");
    TEST_ASSERT(suite, pool.entry_count == 1, "create_string");
    
    string_pool_free(&pool);
}

// Test mark and sweep functionality
DEFINE_TEST(mark_sweep_basic)
{
    StringPool pool;
    string_pool_init(&pool);
    
    char* str1 = string_pool_intern(&pool, "keep me", 7);
    string_pool_intern(&pool, "delete me", 9);
    char* str3 = string_pool_intern(&pool, "keep me too", 11);
    
    TEST_ASSERT(suite, pool.entry_count == 3, "mark_sweep_basic");
    
    // Begin mark-sweep cycle
    string_pool_mark_sweep_begin(&pool);
    
    // Mark strings we want to keep
    string_pool_mark(&pool, str1);
    string_pool_mark(&pool, str3);
    
    // Sweep unmarked strings
    string_pool_sweep(&pool);
    
    // Only marked strings should remain
    TEST_ASSERT(suite, pool.entry_count == 2, "mark_sweep_basic");
    
    // The kept strings should still be valid
    TEST_ASSERT(suite, strcmp(str1, "keep me") == 0, "mark_sweep_basic");
    TEST_ASSERT(suite, strcmp(str3, "keep me too") == 0, "mark_sweep_basic");
    
    string_pool_free(&pool);
}

// Test mark-sweep with all strings marked
DEFINE_TEST(mark_sweep_all_marked)
{
    StringPool pool;
    string_pool_init(&pool);
    
    char* str1 = string_pool_intern(&pool, "string 1", 8);
    char* str2 = string_pool_intern(&pool, "string 2", 8);
    char* str3 = string_pool_intern(&pool, "string 3", 8);
    
    TEST_ASSERT(suite, pool.entry_count == 3, "mark_sweep_all_marked");
    
    string_pool_mark_sweep_begin(&pool);
    string_pool_mark(&pool, str1);
    string_pool_mark(&pool, str2);
    string_pool_mark(&pool, str3);
    string_pool_sweep(&pool);
    
    // All strings should remain
    TEST_ASSERT(suite, pool.entry_count == 3, "mark_sweep_all_marked");
    
    string_pool_free(&pool);
}

// Test mark-sweep with no strings marked
DEFINE_TEST(mark_sweep_none_marked)
{
    StringPool pool;
    string_pool_init(&pool);
    
    string_pool_intern(&pool, "string 1", 8);
    string_pool_intern(&pool, "string 2", 8);
    string_pool_intern(&pool, "string 3", 8);
    
    TEST_ASSERT(suite, pool.entry_count == 3, "mark_sweep_none_marked");
    
    string_pool_mark_sweep_begin(&pool);
    string_pool_sweep(&pool);
    
    // All strings should be removed
    TEST_ASSERT(suite, pool.entry_count == 0, "mark_sweep_none_marked");
    TEST_ASSERT(suite, pool.all_strings == NULL, "mark_sweep_none_marked");
    
    string_pool_free(&pool);
}

// Test pool resizing with many strings
DEFINE_TEST(pool_resize)
{
    StringPool pool;
    string_pool_init(&pool);
    
    // Add enough strings to trigger resize (more than 75% of 32 initial buckets)
    char buffer[32];
    for (int i = 0; i < 30; i++) {
        snprintf(buffer, sizeof(buffer), "string_%d", i);
        string_pool_intern(&pool, buffer, strlen(buffer));
    }
    
    TEST_ASSERT(suite, pool.entry_count == 30, "pool_resize");
    TEST_ASSERT(suite, pool.bucket_count > 32, "pool_resize");  // Should have resized
    
    // Verify all strings are still accessible
    for (int i = 0; i < 30; i++) {
        snprintf(buffer, sizeof(buffer), "string_%d", i);
        char* found = string_pool_intern(&pool, buffer, strlen(buffer));
        TEST_ASSERT(suite, found != NULL, "pool_resize");
        TEST_ASSERT(suite, strcmp(found, buffer) == 0, "pool_resize");
    }
    
    // Entry count shouldn't have increased
    TEST_ASSERT(suite, pool.entry_count == 30, "pool_resize");
    
    string_pool_free(&pool);
}

// Test with strings containing special characters
DEFINE_TEST(special_characters)
{
    StringPool pool;
    string_pool_init(&pool);
    
    char* str1 = string_pool_intern(&pool, "hello\nworld", 11);
    char* str2 = string_pool_intern(&pool, "tab\there", 8);
    char* str3 = string_pool_intern(&pool, "null\0byte", 9);  // Including null byte
    
    TEST_ASSERT(suite, str1 != NULL, "special_characters");
    TEST_ASSERT(suite, str2 != NULL, "special_characters");
    TEST_ASSERT(suite, str3 != NULL, "special_characters");
    TEST_ASSERT(suite, pool.entry_count == 3, "special_characters");
    
    // Verify content
    TEST_ASSERT(suite, memcmp(str1, "hello\nworld", 11) == 0, "special_characters");
    TEST_ASSERT(suite, memcmp(str2, "tab\there", 8) == 0, "special_characters");
    TEST_ASSERT(suite, memcmp(str3, "null\0byte", 9) == 0, "special_characters");
    
    string_pool_free(&pool);
}

// Define test suite
TEST_SUITE(string_pool_unit)
    TEST_CASE(init_and_free, "Init and Free")
    TEST_CASE(intern_string, "Intern String")
    TEST_CASE(intern_multiple, "Intern Multiple Strings")
    TEST_CASE(intern_empty, "Intern Empty String")
    TEST_CASE(intern_prefix, "Intern Prefix Strings")
    TEST_CASE(create_string, "Create String")
    TEST_CASE(mark_sweep_basic, "Mark Sweep Basic")
    TEST_CASE(mark_sweep_all_marked, "Mark Sweep All Marked")
    TEST_CASE(mark_sweep_none_marked, "Mark Sweep None Marked")
    TEST_CASE(pool_resize, "Pool Resize")
    TEST_CASE(special_characters, "Special Characters")
END_TEST_SUITE(string_pool_unit)