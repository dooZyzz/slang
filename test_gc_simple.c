#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "runtime/core/vm.h"
#include "runtime/core/gc.h"
#include "runtime/core/object.h"
#include "utils/allocators.h"

int main() {
    printf("=== Simple GC Test ===\n");
    
    // Initialize allocators
    printf("1. Initializing allocators...\n");
    AllocatorConfig config = {
        .enable_trace = false,
        .enable_stats = true,
        .arena_size = 64 * 1024,
        .object_pool_size = 256
    };
    allocators_init(&config);
    
    // Create VM
    printf("2. Creating VM...\n");
    VM* vm = vm_create();
    if (!vm) {
        printf("Failed to create VM\n");
        return 1;
    }
    printf("VM created successfully\n");
    
    // Check GC
    printf("3. Checking GC...\n");
    if (!vm->gc) {
        printf("GC is NULL!\n");
        return 1;
    }
    printf("GC created successfully\n");
    
    // Create a simple object
    printf("4. Creating object...\n");
    Object* obj = object_create();
    if (!obj) {
        printf("Failed to create object\n");
        return 1;
    }
    printf("Object created successfully\n");
    
    // Force collection
    printf("5. Forcing GC...\n");
    gc_collect(vm->gc);
    printf("GC completed\n");
    
    // Cleanup
    printf("6. Cleaning up...\n");
    vm_destroy(vm);
    allocators_shutdown();
    
    printf("Test completed successfully!\n");
    return 0;
}