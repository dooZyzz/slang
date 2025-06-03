#include <stdio.h>
#include "runtime/core/vm.h"
#include "runtime/core/gc.h"
#include "runtime/core/object.h"
#include "utils/allocators.h"

int main() {
    printf("=== Direct GC Test ===\n");
    
    // Initialize allocators
    AllocatorConfig config = {
        .enable_trace = false,
        .enable_stats = true,
        .arena_size = 64 * 1024,
        .object_pool_size = 256
    };
    allocators_init(&config);
    
    // Create VM (which creates GC)
    VM* vm = vm_create();
    if (!vm) {
        printf("Failed to create VM\n");
        return 1;
    }
    
    printf("VM created with GC\n");
    
    // Enable verbose GC
    gc_set_verbose(vm->gc, true);
    
    // Set small threshold to trigger GC quickly
    gc_set_threshold(vm->gc, 1024); // 1KB
    
    // Get initial stats
    GCStats before = gc_get_stats(vm->gc);
    printf("Initial stats - Allocated: %zu bytes\n", before.current_allocated);
    
    // Create many objects
    printf("\nCreating 1000 objects...\n");
    for (int i = 0; i < 1000; i++) {
        Object* obj = object_create();
        if (!obj) {
            printf("Failed to create object %d\n", i);
            break;
        }
        
        // Set some properties to increase memory usage
        char key[32];
        snprintf(key, sizeof(key), "prop%d", i);
        object_set(obj, key, NUMBER_VAL(i));
        
        if (i % 100 == 0) {
            printf("Created %d objects\n", i);
        }
    }
    
    // Get stats after allocation
    GCStats after = gc_get_stats(vm->gc);
    printf("\nAfter allocation - Collections: %zu, Allocated: %zu bytes\n", 
           after.collections, after.current_allocated);
    
    // Force a collection
    printf("\nForcing garbage collection...\n");
    gc_collect(vm->gc);
    
    // Final stats
    GCStats final = gc_get_stats(vm->gc);
    printf("\nFinal stats:\n");
    printf("  Total collections: %zu\n", final.collections);
    printf("  Objects allocated: %zu\n", final.total_allocated);
    printf("  Objects freed: %zu\n", final.objects_freed);
    printf("  Current memory: %zu bytes\n", final.current_allocated);
    printf("  Peak memory: %zu bytes\n", final.peak_allocated);
    printf("  Total GC time: %.2f ms\n", final.total_gc_time);
    
    // Clean up
    vm_destroy(vm);
    allocators_shutdown();
    
    return 0;
}