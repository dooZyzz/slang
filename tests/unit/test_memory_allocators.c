#include "utils/test_framework.h"
#include "utils/memory.h"
#include "utils/alloc.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Test platform allocator
static void test_platform_allocator(TestSuite* suite) {
    Allocator* alloc = mem_create_platform_allocator();
    TEST_ASSERT(suite, alloc != NULL, "Platform allocator creation");
    
    // Test basic allocation
    void* ptr1 = MEM_ALLOC(alloc, 100);
    TEST_ASSERT(suite, ptr1 != NULL, "Basic allocation");
    
    // Test zero allocation
    void* ptr2 = MEM_ALLOC_ZERO(alloc, 50);
    TEST_ASSERT(suite, ptr2 != NULL, "Zero allocation");
    
    // Verify zero initialization
    unsigned char* bytes = (unsigned char*)ptr2;
    bool all_zero = true;
    for (int i = 0; i < 50; i++) {
        if (bytes[i] != 0) {
            all_zero = false;
            break;
        }
    }
    TEST_ASSERT(suite, all_zero, "Memory zeroing");
    
    // Test reallocation
    void* ptr3 = MEM_REALLOC(alloc, ptr1, 100, 200);
    TEST_ASSERT(suite, ptr3 != NULL, "Reallocation");
    
    // Test stats
    AllocatorStats stats = mem_get_stats(alloc);
    TEST_ASSERT_EQUAL_INT(suite, 250, stats.current_usage, "Current usage");
    TEST_ASSERT_EQUAL_INT(suite, 2, stats.allocation_count, "Allocation count");
    
    // Free memory
    MEM_FREE(alloc, ptr3, 200);
    MEM_FREE(alloc, ptr2, 50);
    
    stats = mem_get_stats(alloc);
    TEST_ASSERT_EQUAL_INT(suite, 0, stats.current_usage, "Memory freed");
    TEST_ASSERT_EQUAL_INT(suite, 2, stats.free_count, "Free count");
    
    mem_destroy(alloc);
}

// Test arena allocator
static void test_arena_allocator(TestSuite* suite) {
    Allocator* arena = mem_create_arena_allocator(1024);
    TEST_ASSERT(suite, arena != NULL, "Arena allocator creation");
    
    // Allocate multiple objects
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = MEM_ALLOC(arena, 50);
        TEST_ASSERT(suite, ptrs[i] != NULL, "Arena allocation");
    }
    
    // Arena should not free individual allocations
    MEM_FREE(arena, ptrs[0], 50);
    
    AllocatorStats stats = mem_get_stats(arena);
    TEST_ASSERT_EQUAL_INT(suite, 500, stats.current_usage, "Arena no individual free");
    TEST_ASSERT_EQUAL_INT(suite, 10, stats.allocation_count, "Arena allocation count");
    
    // Test reset
    mem_reset(arena);
    stats = mem_get_stats(arena);
    TEST_ASSERT_EQUAL_INT(suite, 0, stats.current_usage, "Arena reset");
    
    // Allocate after reset
    void* ptr = MEM_ALLOC(arena, 100);
    TEST_ASSERT(suite, ptr != NULL, "Allocation after reset");
    
    mem_destroy(arena);
}

// Test freelist allocator
static void test_freelist_allocator(TestSuite* suite) {
    Allocator* freelist = mem_create_freelist_allocator(64, 10);
    TEST_ASSERT(suite, freelist != NULL, "Freelist allocator creation");
    
    // Allocate fixed-size blocks
    void* ptrs[5];
    for (int i = 0; i < 5; i++) {
        ptrs[i] = MEM_ALLOC(freelist, 32);
        TEST_ASSERT(suite, ptrs[i] != NULL, "Freelist allocation");
    }
    
    // Free some blocks
    MEM_FREE(freelist, ptrs[1], 64);
    MEM_FREE(freelist, ptrs[3], 64);
    
    // Allocate again - should reuse freed blocks
    void* ptr1 = MEM_ALLOC(freelist, 48);
    void* ptr2 = MEM_ALLOC(freelist, 64);
    TEST_ASSERT(suite, ptr1 != NULL && ptr2 != NULL, "Reuse freed blocks");
    
    // Test allocation larger than block size - should fail
    void* large_ptr = MEM_ALLOC(freelist, 128);
    TEST_ASSERT(suite, large_ptr == NULL, "Oversized block rejection");
    
    AllocatorStats stats = mem_get_stats(freelist);
    TEST_ASSERT_EQUAL_INT(suite, 7, stats.allocation_count, "Freelist alloc count");
    TEST_ASSERT_EQUAL_INT(suite, 2, stats.free_count, "Freelist free count");
    
    mem_destroy(freelist);
}

// Test trace allocator
static void test_trace_allocator(TestSuite* suite) {
    // Create trace allocator with platform backing
    Allocator* platform = mem_create_platform_allocator();
    Allocator* trace = mem_create_trace_allocator(platform);
    TEST_ASSERT(suite, trace != NULL, "Trace allocator creation");
    
    // Make tagged allocations
    void* ptr1 = MEM_ALLOC_TAGGED(trace, 100, "test-array");
    void* ptr2 = MEM_ALLOC_TAGGED(trace, 200, "test-object");
    void* ptr3 = MEM_ALLOC_TAGGED(trace, 50, "test-array");
    void* ptr4 = MEM_ALLOC(trace, 150);  // Untagged
    
    TEST_ASSERT(suite, ptr1 && ptr2 && ptr3 && ptr4, "Trace allocations");
    
    // Free some allocations
    MEM_FREE(trace, ptr1, 100);
    MEM_FREE(trace, ptr3, 50);
    
    // Get stats
    AllocatorStats stats = mem_get_stats(trace);
    TEST_ASSERT_EQUAL_INT(suite, 350, stats.current_usage, "Trace current usage");
    TEST_ASSERT_EQUAL_INT(suite, 4, stats.allocation_count, "Trace alloc count");
    TEST_ASSERT_EQUAL_INT(suite, 2, stats.free_count, "Trace free count");
    
    // Print detailed stats (for visual inspection)
    char* formatted = mem_format_stats(trace);
    if (formatted) {
        printf("\n%s\n", formatted);
        free(formatted);
    }
    
    // Test memory leak detection
    mem_check_leaks(trace);
    
    // Clean up remaining allocations
    MEM_FREE(trace, ptr2, 200);
    MEM_FREE(trace, ptr4, 150);
    
    mem_destroy(trace);
    mem_destroy(platform);
}

// Test memory migration helpers
static void test_migration_helpers(TestSuite* suite) {
    // Set up trace allocator as default
    Allocator* platform = mem_create_platform_allocator();
    Allocator* trace = mem_create_trace_allocator(platform);
    set_allocator(trace);
    
    // Test migration macros
    void* ptr1 = MALLOC(100);
    TEST_ASSERT(suite, ptr1 != NULL, "MALLOC");
    
    void* ptr2 = CALLOC(10, 20);
    TEST_ASSERT(suite, ptr2 != NULL, "CALLOC");
    
    char* str = STRDUP("Hello, World!");
    TEST_ASSERT(suite, str != NULL && strcmp(str, "Hello, World!") == 0, "STRDUP");
    
    // Test type-safe allocation
    typedef struct {
        int x, y, z;
    } Point3D;
    
    Point3D* point = NEW(Point3D);
    TEST_ASSERT(suite, point != NULL, "NEW");
    TEST_ASSERT(suite, point->x == 0 && point->y == 0 && point->z == 0, "NEW zeroing");
    
    int* array = NEW_ARRAY(int, 10);
    TEST_ASSERT(suite, array != NULL, "NEW_ARRAY");
    
    // Free allocations
    FREE_SIMPLE(ptr1);
    FREE_SIMPLE(ptr2);
    FREE(str, strlen("Hello, World!") + 1);
    FREE(point, sizeof(Point3D));
    FREE(array, sizeof(int) * 10);
    
    // Check for leaks
    AllocatorStats stats = mem_get_stats(trace);
    TEST_ASSERT_EQUAL_INT(suite, 0, stats.current_usage, "No memory leaks");
    
    // Clean up
    set_allocator(NULL);
    mem_destroy(trace);
    mem_destroy(platform);
}

// Test arena scope helper
static void test_arena_scope(TestSuite* suite) {
    // Track allocations before arena
    Allocator* trace = mem_create_trace_allocator(mem_create_platform_allocator());
    set_allocator(trace);
    
    void* persistent = MALLOC(100);
    TEST_ASSERT(suite, persistent != NULL, "Persistent allocation");
    
    // Use arena scope
    WITH_ARENA(arena, 4096, {
        // Inside arena scope
        void* temp1 = MALLOC(200);
        void* temp2 = MALLOC(300);
        char* temp_str = STRDUP("Temporary string");
        
        TEST_ASSERT(suite, temp1 && temp2 && temp_str, "Arena scope allocations");
        
        // These allocations will be automatically freed when scope ends
    });
    
    // Check that only persistent allocation remains
    AllocatorStats stats = mem_get_stats(trace);
    TEST_ASSERT_EQUAL_INT(suite, 100, stats.current_usage, "Arena cleanup");
    
    FREE(persistent, 100);
    
    // Clean up
    set_allocator(NULL);
    mem_destroy(trace);
}

// Export test functions for test framework
void memory_allocators_register_tests(TestSuite* suite) {
    RUN_TEST(suite, test_platform_allocator);
    RUN_TEST(suite, test_arena_allocator);
    RUN_TEST(suite, test_freelist_allocator);
    RUN_TEST(suite, test_trace_allocator);
    RUN_TEST(suite, test_migration_helpers);
    RUN_TEST(suite, test_arena_scope);
}

// Standalone test runner
int main(void) {
    TestSuite* suite = test_suite_create("Memory Allocators");
    memory_allocators_register_tests(suite);
    test_suite_print_results(suite);
    
    bool all_passed = (suite->failed == 0);
    test_suite_destroy(suite);
    
    return all_passed ? 0 : 1;
}