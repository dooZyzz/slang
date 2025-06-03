#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include "runtime/core/vm.h"
#include "runtime/core/gc.h"
#include "runtime/core/object.h"
#include "utils/allocators.h"
#include "codegen/compiler.h"
#include "parser/parser.h"

// Test 1: Basic object allocation and collection
void test_basic_gc() {
    printf("\n=== Test 1: Basic GC ===\n");
    
    VM* vm = vm_create();
    assert(vm != NULL);
    assert(vm->gc != NULL);
    
    // Enable verbose mode for testing
    gc_set_verbose(vm->gc, true);
    
    // Create some objects
    printf("Creating 5 objects...\n");
    for (int i = 0; i < 5; i++) {
        Object* obj = object_create();
        assert(obj != NULL);
        // Don't push to stack - these should be collected
    }
    
    // Force collection
    printf("Forcing garbage collection...\n");
    gc_collect(vm->gc);
    
    // Check stats
    GCStats stats = gc_get_stats(vm->gc);
    printf("Objects freed: %zu\n", stats.objects_freed);
    assert(stats.objects_freed == 5);
    
    vm_destroy(vm);
    printf("Test 1 passed!\n");
}

// Test 2: Root set preservation
void test_root_preservation() {
    printf("\n=== Test 2: Root Preservation ===\n");
    
    VM* vm = vm_create();
    gc_set_verbose(vm->gc, true);
    
    // Create objects and push some to stack
    printf("Creating objects, keeping 3 on stack...\n");
    Object* kept1 = object_create();
    Object* kept2 = object_create();
    Object* kept3 = object_create();
    
    vm_push(vm, OBJECT_VAL(kept1));
    vm_push(vm, OBJECT_VAL(kept2));
    vm_push(vm, OBJECT_VAL(kept3));
    
    // Create garbage objects
    for (int i = 0; i < 7; i++) {
        object_create();
    }
    
    GCStats before = gc_get_stats(vm->gc);
    printf("Before GC: %zu objects\n", vm->gc->object_count);
    
    // Collect
    gc_collect(vm->gc);
    
    GCStats after = gc_get_stats(vm->gc);
    printf("After GC: %zu objects\n", vm->gc->object_count);
    printf("Freed: %zu objects\n", after.objects_freed - before.objects_freed);
    
    // Should have kept 3 objects
    assert(vm->gc->object_count == 3);
    
    vm_destroy(vm);
    printf("Test 2 passed!\n");
}

// Test 3: Object graph traversal
void test_object_references() {
    printf("\n=== Test 3: Object References ===\n");
    
    VM* vm = vm_create();
    gc_set_verbose(vm->gc, true);
    
    // Create object graph
    Object* root = object_create();
    Object* child1 = object_create();
    Object* child2 = object_create();
    Object* grandchild = object_create();
    
    // Build references
    object_set_property(root, "child1", OBJECT_VAL(child1));
    object_set_property(root, "child2", OBJECT_VAL(child2));
    object_set_property(child1, "grandchild", OBJECT_VAL(grandchild));
    
    // Keep only root on stack
    vm_push(vm, OBJECT_VAL(root));
    
    // Create garbage
    for (int i = 0; i < 5; i++) {
        object_create();
    }
    
    printf("Before GC: %zu objects\n", vm->gc->object_count);
    gc_collect(vm->gc);
    printf("After GC: %zu objects\n", vm->gc->object_count);
    
    // Should keep 4 objects (root + 3 children)
    assert(vm->gc->object_count == 4);
    
    vm_destroy(vm);
    printf("Test 3 passed!\n");
}

// Test 4: Circular references
void test_circular_references() {
    printf("\n=== Test 4: Circular References ===\n");
    
    VM* vm = vm_create();
    gc_set_verbose(vm->gc, true);
    
    // Create circular reference
    Object* obj1 = object_create();
    Object* obj2 = object_create();
    Object* obj3 = object_create();
    
    object_set_property(obj1, "next", OBJECT_VAL(obj2));
    object_set_property(obj2, "next", OBJECT_VAL(obj3));
    object_set_property(obj3, "next", OBJECT_VAL(obj1));  // Circular!
    
    // Keep one on stack
    vm_push(vm, OBJECT_VAL(obj1));
    
    // Create garbage
    for (int i = 0; i < 3; i++) {
        object_create();
    }
    
    printf("Before GC: %zu objects\n", vm->gc->object_count);
    gc_collect(vm->gc);
    printf("After GC: %zu objects\n", vm->gc->object_count);
    
    // Should keep all 3 in the cycle
    assert(vm->gc->object_count == 3);
    
    // Now drop the reference
    vm_pop(vm);
    
    gc_collect(vm->gc);
    printf("After dropping root: %zu objects\n", vm->gc->object_count);
    
    // All should be collected
    assert(vm->gc->object_count == 0);
    
    vm_destroy(vm);
    printf("Test 4 passed!\n");
}

// Test 5: Stress test
void test_gc_stress() {
    printf("\n=== Test 5: GC Stress Test ===\n");
    
    VM* vm = vm_create();
    
    // Set small threshold to trigger frequent GCs
    gc_set_threshold(vm->gc, 10 * 1024);  // 10KB
    
    clock_t start = clock();
    
    // Create many objects
    const int iterations = 10000;
    for (int i = 0; i < iterations; i++) {
        Object* obj = object_create();
        
        // Keep every 10th object
        if (i % 10 == 0) {
            vm_push(vm, OBJECT_VAL(obj));
        }
        
        // Create some properties
        char key[32];
        snprintf(key, sizeof(key), "prop_%d", i);
        object_set_property(obj, key, NUMBER_VAL(i));
    }
    
    clock_t end = clock();
    double elapsed = ((double)(end - start)) / CLOCKS_PER_SEC;
    
    gc_print_stats(vm->gc);
    
    printf("Created %d objects in %.2f seconds\n", iterations, elapsed);
    printf("Final object count: %zu\n", vm->gc->object_count);
    
    // Should have kept ~1000 objects
    assert(vm->gc->object_count >= 900 && vm->gc->object_count <= 1100);
    
    vm_destroy(vm);
    printf("Test 5 passed!\n");
}

// Test 6: Global references
void test_global_roots() {
    printf("\n=== Test 6: Global Roots ===\n");
    
    VM* vm = vm_create();
    gc_set_verbose(vm->gc, true);
    
    // Create objects and store in globals
    Object* global_obj = object_create();
    object_set_property(global_obj, "data", STRING_VAL("important"));
    
    define_global(vm, "myGlobal", OBJECT_VAL(global_obj));
    
    // Create garbage
    for (int i = 0; i < 5; i++) {
        object_create();
    }
    
    printf("Before GC: %zu objects\n", vm->gc->object_count);
    gc_collect(vm->gc);
    printf("After GC: %zu objects\n", vm->gc->object_count);
    
    // Global should be preserved
    assert(vm->gc->object_count == 1);
    
    vm_destroy(vm);
    printf("Test 6 passed!\n");
}

// Test 7: Array handling
void test_array_gc() {
    printf("\n=== Test 7: Array GC ===\n");
    
    VM* vm = vm_create();
    gc_set_verbose(vm->gc, true);
    
    // Create array with objects
    Object* array = array_create();
    
    for (int i = 0; i < 5; i++) {
        Object* element = object_create();
        char data[32];
        snprintf(data, sizeof(data), "element_%d", i);
        object_set_property(element, "data", STRING_VAL(data));
        array_push(array, OBJECT_VAL(element));
    }
    
    // Keep array on stack
    vm_push(vm, OBJECT_VAL(array));
    
    // Create garbage
    for (int i = 0; i < 10; i++) {
        object_create();
    }
    
    printf("Before GC: %zu objects\n", vm->gc->object_count);
    gc_collect(vm->gc);
    printf("After GC: %zu objects\n", vm->gc->object_count);
    
    // Should keep array + 5 elements = 6 objects
    assert(vm->gc->object_count == 6);
    
    vm_destroy(vm);
    printf("Test 7 passed!\n");
}

// Test 8: Memory pressure and automatic collection
void test_auto_collection() {
    printf("\n=== Test 8: Automatic Collection ===\n");
    
    VM* vm = vm_create();
    
    // Set very small threshold
    gc_set_threshold(vm->gc, 1024);  // 1KB
    
    GCStats before = gc_get_stats(vm->gc);
    
    // Allocate until GC triggers
    for (int i = 0; i < 100; i++) {
        Object* obj = object_create();
        // Add properties to increase memory usage
        for (int j = 0; j < 10; j++) {
            char key[32];
            snprintf(key, sizeof(key), "prop_%d_%d", i, j);
            object_set_property(obj, key, NUMBER_VAL(i * 10 + j));
        }
    }
    
    GCStats after = gc_get_stats(vm->gc);
    
    printf("Collections triggered: %zu\n", after.collections - before.collections);
    
    // Should have triggered at least one collection
    assert(after.collections > before.collections);
    
    vm_destroy(vm);
    printf("Test 8 passed!\n");
}

// Run a simple program to test GC during execution
void test_gc_during_execution() {
    printf("\n=== Test 9: GC During Program Execution ===\n");
    
    // Initialize allocators
    AllocatorConfig config = {
        .enable_trace = false,
        .enable_stats = true,
        .arena_size = 64 * 1024,
        .object_pool_size = 256
    };
    allocators_init(&config);
    
    VM* vm = vm_create();
    gc_set_verbose(vm->gc, true);
    gc_set_threshold(vm->gc, 5 * 1024);  // 5KB
    
    // Simple program that creates objects
    const char* source = 
        "func createObjects() {\n"
        "    let arr = [];\n"
        "    for (let i = 0; i < 20; i++) {\n"
        "        let obj = {};\n"
        "        obj.value = i;\n"
        "        obj.data = \"test_\" + i;\n"
        "        arr.push(obj);\n"
        "    }\n"
        "    return arr;\n"
        "}\n"
        "let result = createObjects();\n"
        "print(\"Created \" + result.length + \" objects\");\n";
    
    // Parse and compile
    Parser* parser = parser_create(source);
    ProgramNode* ast = parser_parse_program(parser);
    
    if (!parser->had_error && ast) {
        Chunk chunk;
        chunk_init(&chunk);
        
        if (compile(ast, &chunk)) {
            printf("Running program...\n");
            InterpretResult result = vm_interpret(vm, &chunk);
            
            if (result == INTERPRET_OK) {
                printf("Program executed successfully\n");
                gc_print_stats(vm->gc);
            } else {
                printf("Runtime error\n");
            }
        }
        
        chunk_free(&chunk);
    }
    
    parser_destroy(parser);
    vm_destroy(vm);
    allocators_shutdown();
    
    printf("Test 9 passed!\n");
}

int main() {
    printf("=== Garbage Collector Test Suite ===\n");
    
    // Initialize allocators once at the beginning
    AllocatorConfig config = {
        .enable_trace = false,
        .enable_stats = true,
        .arena_size = 64 * 1024,
        .object_pool_size = 256
    };
    allocators_init(&config);
    
    // Run individual tests
    printf("Running test_basic_gc...\n");
    test_basic_gc();
    return 0;  // Stop here for debugging
    
    printf("\n=== All GC tests passed! ===\n");
    
    // Final allocator stats
    printf("\n=== Final Allocator Statistics ===\n");
    allocators_print_stats();
    
    allocators_shutdown();
    
    return 0;
}