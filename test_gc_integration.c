#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "runtime/core/vm.h"
#include "runtime/core/gc.h"
#include "runtime/core/object.h"
#include "utils/allocators.h"
#include "codegen/compiler.h"
#include "parser/parser.h"
#include "semantic/analyzer.h"

// Helper to compile and run a program
static bool run_program(const char* name, const char* source, bool verbose_gc) {
    printf("\n=== Running Program: %s ===\n", name);
    printf("Source:\n%s\n", source);
    printf("---\n");
    
    // Create VM
    VM* vm = vm_create();
    if (!vm) {
        printf("Failed to create VM\n");
        return false;
    }
    
    // Enable GC verbosity if requested
    if (verbose_gc) {
        gc_set_verbose(vm->gc, true);
    }
    
    // Get initial GC stats
    GCStats before = gc_get_stats(vm->gc);
    
    // Parse
    Parser* parser = parser_create(source);
    ProgramNode* ast = parser_parse_program(parser);
    
    if (parser->had_error) {
        printf("Parse error!\n");
        parser_destroy(parser);
        vm_destroy(vm);
        return false;
    }
    
    // Skip semantic analysis for now
    
    // Compile
    Chunk chunk;
    chunk_init(&chunk);
    
    if (!compile(ast, &chunk)) {
        printf("Compilation failed!\n");
        chunk_free(&chunk);
        parser_destroy(parser);
        vm_destroy(vm);
        return false;
    }
    
    // Clean up parser/AST (uses arena allocator)
    parser_destroy(parser);
    
    // Run
    clock_t start = clock();
    InterpretResult result = vm_interpret(vm, &chunk);
    clock_t end = clock();
    
    double runtime = ((double)(end - start)) / CLOCKS_PER_SEC * 1000.0;
    
    if (result != INTERPRET_OK) {
        printf("Runtime error!\n");
        chunk_free(&chunk);
        vm_destroy(vm);
        return false;
    }
    
    // Get final GC stats
    GCStats after = gc_get_stats(vm->gc);
    
    // Print GC summary
    printf("\n--- GC Summary ---\n");
    printf("Collections: %zu\n", after.collections - before.collections);
    printf("Objects allocated: %zu\n", after.total_allocated - before.total_allocated);
    printf("Objects freed: %zu\n", after.objects_freed - before.objects_freed);
    printf("Peak memory: %zu bytes\n", after.peak_allocated);
    printf("Final memory: %zu bytes\n", after.current_allocated);
    printf("Total GC time: %.2f ms\n", after.total_gc_time - before.total_gc_time);
    printf("Runtime: %.2f ms\n", runtime);
    
    // Clean up
    chunk_free(&chunk);
    vm_destroy(vm);
    
    return true;
}

int main() {
    printf("=== GC Integration Tests ===\n");
    printf("Testing garbage collection with real programs\n");
    
    // Initialize allocators
    AllocatorConfig config = {
        .enable_trace = false,
        .enable_stats = true,
        .arena_size = 64 * 1024,
        .object_pool_size = 256
    };
    allocators_init(&config);
    
    // Test 1: Simple object creation
    const char* test1 = 
        "// Test 1: Simple object creation\n"
        "let obj = {};\n"
        "obj.name = \"test\";\n"
        "obj.value = 42;\n"
        "print(\"Object created with name: \" + obj.name);\n";
    
    run_program("Simple Object Creation", test1, false);
    
    // Test 2: Object creation in loop (should trigger GC)
    const char* test2 = 
        "// Test 2: Many objects in loop\n"
        "for (let i = 0; i < 100; i++) {\n"
        "    let obj = {};\n"
        "    obj.index = i;\n"
        "    obj.data = \"Item \" + i;\n"
        "    // Objects go out of scope and should be collected\n"
        "}\n"
        "print(\"Created 100 temporary objects\");\n";
    
    run_program("Loop Object Creation", test2, true);
    
    // Test 3: Array of objects
    const char* test3 = 
        "// Test 3: Array of objects\n"
        "let arr = [];\n"
        "for (let i = 0; i < 20; i++) {\n"
        "    let obj = {};\n"
        "    obj.id = i;\n"
        "    obj.name = \"Object_\" + i;\n"
        "    arr.push(obj);\n"
        "}\n"
        "print(\"Array length: \" + arr.length);\n"
        "// Array keeps objects alive\n";
    
    run_program("Array of Objects", test3, false);
    
    // Test 4: Nested objects
    const char* test4 = 
        "// Test 4: Nested object structures\n"
        "let root = {};\n"
        "root.child = {};\n"
        "root.child.grandchild = {};\n"
        "root.child.grandchild.data = \"Deep value\";\n"
        "root.sibling = {};\n"
        "root.sibling.data = \"Sibling value\";\n"
        "print(\"Nested structure created\");\n"
        "print(\"Deep value: \" + root.child.grandchild.data);\n";
    
    run_program("Nested Objects", test4, false);
    
    // Test 5: Function closures creating objects
    const char* test5 = 
        "// Test 5: Closures and objects\n"
        "func createCounter() {\n"
        "    let state = {};\n"
        "    state.count = 0;\n"
        "    \n"
        "    func increment() {\n"
        "        state.count = state.count + 1;\n"
        "        return state.count;\n"
        "    }\n"
        "    \n"
        "    return increment;\n"
        "}\n"
        "\n"
        "let counter1 = createCounter();\n"
        "let counter2 = createCounter();\n"
        "print(\"Counter 1: \" + counter1());\n"
        "print(\"Counter 1: \" + counter1());\n"
        "print(\"Counter 2: \" + counter2());\n";
    
    run_program("Closures with Objects", test5, false);
    
    // Test 6: Circular references
    const char* test6 = 
        "// Test 6: Circular references\n"
        "let obj1 = {};\n"
        "let obj2 = {};\n"
        "obj1.ref = obj2;\n"
        "obj2.ref = obj1;\n"
        "obj1.data = \"First\";\n"
        "obj2.data = \"Second\";\n"
        "print(\"Circular reference created\");\n"
        "// Both should be collected when they go out of scope\n";
    
    run_program("Circular References", test6, true);
    
    // Test 7: String concatenation stress test
    const char* test7 = 
        "// Test 7: String operations\n"
        "let result = \"\";\n"
        "for (let i = 0; i < 50; i++) {\n"
        "    result = result + \"x\";\n"
        "    // Each concatenation might create temporary objects\n"
        "}\n"
        "print(\"String length: \" + result.length);\n";
    
    run_program("String Stress Test", test7, false);
    
    // Test 8: Complex program with multiple features
    const char* test8 = 
        "// Test 8: Complex program\n"
        "struct Person {\n"
        "    let name;\n"
        "    let age;\n"
        "    let friends;\n"
        "}\n"
        "\n"
        "func createPerson(name, age) {\n"
        "    let p = Person();\n"
        "    p.name = name;\n"
        "    p.age = age;\n"
        "    p.friends = [];\n"
        "    return p;\n"
        "}\n"
        "\n"
        "func addFriend(person, friend) {\n"
        "    person.friends.push(friend);\n"
        "}\n"
        "\n"
        "// Create a social network\n"
        "let people = [];\n"
        "for (let i = 0; i < 10; i++) {\n"
        "    people.push(createPerson(\"Person_\" + i, 20 + i));\n"
        "}\n"
        "\n"
        "// Add some friendships\n"
        "addFriend(people[0], people[1]);\n"
        "addFriend(people[0], people[2]);\n"
        "addFriend(people[1], people[0]);\n"
        "\n"
        "print(\"Created \" + people.length + \" people\");\n"
        "print(people[0].name + \" has \" + people[0].friends.length + \" friends\");\n";
    
    run_program("Complex Program", test8, true);
    
    // Test 9: Memory pressure test
    const char* test9 = 
        "// Test 9: Memory pressure\n"
        "// Create and discard many objects to trigger multiple GCs\n"
        "let kept = [];\n"
        "for (let i = 0; i < 1000; i++) {\n"
        "    let temp = {};\n"
        "    temp.index = i;\n"
        "    temp.data = \"Temporary data for object \" + i;\n"
        "    \n"
        "    // Keep every 10th object\n"
        "    if (i % 10 == 0) {\n"
        "        kept.push(temp);\n"
        "    }\n"
        "    // Others should be collected\n"
        "}\n"
        "print(\"Kept \" + kept.length + \" objects out of 1000\");\n";
    
    // Set smaller GC threshold for this test
    VM* vm = vm_create();
    gc_set_threshold(vm->gc, 10 * 1024); // 10KB
    vm_destroy(vm);
    
    run_program("Memory Pressure Test", test9, true);
    
    // Test 10: Module with exports (if modules are implemented)
    const char* test10 = 
        "// Test 10: Module-like pattern\n"
        "let module = {};\n"
        "module.exports = {};\n"
        "\n"
        "module.exports.helper = func(x) {\n"
        "    return x * 2;\n"
        "};\n"
        "\n"
        "module.exports.data = {\n"
        "    version: \"1.0\",\n"
        "    author: \"test\"\n"
        "};\n"
        "\n"
        "print(\"Module version: \" + module.exports.data.version);\n"
        "print(\"Helper result: \" + module.exports.helper(21));\n";
    
    run_program("Module Pattern", test10, false);
    
    // Final summary
    printf("\n\n=== Final Allocator Statistics ===\n");
    allocators_print_stats();
    
    // Check for leaks
    printf("\n=== Checking for memory leaks ===\n");
    allocators_check_leaks();
    
    allocators_shutdown();
    
    return 0;
}