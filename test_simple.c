#include <stdio.h>
#include "utils/allocators.h"
#include "runtime/core/string_pool.h"
#include "runtime/core/object.h"

int main() {
    // Initialize allocators
    AllocatorConfig config = {
        .trace_enabled = true,
        .arena_size = 1024 * 1024,  // 1MB
        .freelist_chunk_size = 64
    };
    allocators_init(&config);
    
    printf("Testing refactored memory system...\n");
    
    // Test string pool
    StringPool pool;
    string_pool_init(&pool);
    
    char* str1 = string_pool_intern(&pool, "Hello", 5);
    char* str2 = string_pool_intern(&pool, "World", 5);
    char* str3 = string_pool_intern(&pool, "Hello", 5);  // Should return same as str1
    
    printf("str1: %s\n", str1);
    printf("str2: %s\n", str2);
    printf("str3: %s (should be same address as str1: %s)\n", str3, str1 == str3 ? "YES" : "NO");
    
    // Test object creation
    Object* obj = object_create();
    if (obj) {
        object_set_property(obj, "name", STRING_VAL("Test"));
        object_set_property(obj, "value", NUMBER_VAL(42));
        
        TaggedValue* name = object_get_property(obj, "name");
        TaggedValue* value = object_get_property(obj, "value");
        
        if (name && IS_STRING(*name)) {
            printf("obj.name = %s\n", AS_STRING(*name));
        }
        if (value && IS_NUMBER(*value)) {
            printf("obj.value = %g\n", AS_NUMBER(*value));
        }
        
        object_free(obj);
    }
    
    // Clean up
    string_pool_free(&pool);
    
    // Check for memory leaks
    allocators_check_leaks();
    allocators_print_stats();
    
    allocators_shutdown();
    
    return 0;
}