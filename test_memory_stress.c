#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include "parser/parser.h"
#include "ast/ast.h"
#include "utils/allocators.h"
#include "utils/memory.h"
#include "lexer/lexer.h"
#include "runtime/core/vm.h"
#include "codegen/compiler.h"
#include "runtime/core/string_pool.h"

// Global flag for timeout
static volatile int timed_out = 0;
static volatile int should_stop = 0;

void timeout_handler(int sig) {
    (void)sig;
    timed_out = 1;
    printf("\n=== TIMEOUT: Stopping test ===\n");
}

void sigint_handler(int sig) {
    (void)sig;
    should_stop = 1;
    printf("\n=== SIGINT: Stopping test ===\n");
}

// Monitor memory usage
void print_memory_stats(const char* phase) {
    printf("\n=== Memory Stats: %s ===\n", phase);
    allocators_print_stats();
    
    // Also print system memory usage
    FILE* status = fopen("/proc/self/status", "r");
    if (status) {
        char line[256];
        while (fgets(line, sizeof(line), status)) {
            if (strncmp(line, "VmRSS:", 6) == 0 || 
                strncmp(line, "VmSize:", 7) == 0 ||
                strncmp(line, "VmPeak:", 7) == 0) {
                printf("%s", line);
            }
        }
        fclose(status);
    }
}

// Test 1: Repeated parsing without cleanup
void test_parser_leak_stress() {
    printf("\n=== Test 1: Parser Stress Test ===\n");
    
    const char* programs[] = {
        // String-heavy programs
        "let s = \"This is a very long string that will be allocated in the string pool and potentially leaked if not properly managed\"",
        "let arr = [\"str1\", \"str2\", \"str3\", \"str4\", \"str5\", \"str6\", \"str7\", \"str8\", \"str9\", \"str10\"]",
        "let obj = {a: \"value1\", b: \"value2\", c: \"value3\", d: \"value4\", e: \"value5\"}",
        
        // Complex programs
        "func test() { let x = 5; let y = 10; return x + y; } let result = test();",
        "for (let i = 0; i < 100; i++) { let s = \"iteration: \" + i; print(s); }",
        
        // String interpolation
        "let name = \"World\"; let msg = \"Hello, ${name}! Welcome to ${\"the test\"}!\"",
        
        NULL
    };
    
    print_memory_stats("Before Parser Stress Test");
    
    for (int iter = 0; iter < 10000 && !should_stop && !timed_out; iter++) {
        for (int i = 0; programs[i] != NULL; i++) {
            Parser* parser = parser_create(programs[i]);
            if (parser) {
                ProgramNode* ast = parser_parse_program(parser);
                // Note: AST uses arena allocator, should be freed with parser
                parser_destroy(parser);
            }
        }
        
        if (iter % 1000 == 0) {
            printf("Iteration %d...\n", iter);
            print_memory_stats("During Parser Test");
        }
    }
    
    print_memory_stats("After Parser Stress Test");
}

// Test 2: VM object creation without GC
void test_vm_object_leak() {
    printf("\n=== Test 2: VM Object Stress Test ===\n");
    
    VM* vm = vm_create();
    if (!vm) {
        printf("Failed to create VM\n");
        return;
    }
    
    print_memory_stats("Before VM Object Test");
    
    // Create many objects without triggering GC
    for (int i = 0; i < 10000 && !should_stop && !timed_out; i++) {
        // Push strings (which should be interned)
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "string_%d", i);
        vm_push(vm, STRING_VAL(buffer));
        
        // Create object values
        if (i % 2 == 0) {
            Object* obj = object_create();
            if (obj) {
                vm_push(vm, OBJECT_VAL(obj));
            }
        }
        
        if (i % 1000 == 0) {
            printf("Created %d objects...\n", i);
            print_memory_stats("During VM Test");
        }
    }
    
    print_memory_stats("Before VM Destroy");
    vm_destroy(vm);
    print_memory_stats("After VM Destroy");
}

// Test 3: Module loading/unloading
void test_module_leak() {
    printf("\n=== Test 3: Module Loading Stress Test ===\n");
    
    const char* module_code = 
        "export func add(a, b) { return a + b; }\n"
        "export func multiply(a, b) { return a * b; }\n"
        "export let constant = \"This is a module constant string\";";
    
    print_memory_stats("Before Module Test");
    
    for (int i = 0; i < 1000 && !should_stop && !timed_out; i++) {
        // Parse module code
        Parser* parser = parser_create(module_code);
        if (parser) {
            ProgramNode* ast = parser_parse_program(parser);
            parser_destroy(parser);
        }
        
        if (i % 100 == 0) {
            printf("Module iteration %d...\n", i);
            print_memory_stats("During Module Test");
        }
    }
    
    print_memory_stats("After Module Test");
}

// Test 4: String pool stress
void test_string_pool_leak() {
    printf("\n=== Test 4: String Pool Stress Test ===\n");
    
    StringPool pool;
    string_pool_init(&pool);
    
    print_memory_stats("Before String Pool Test");
    
    // Create many unique strings
    for (int i = 0; i < 50000 && !should_stop && !timed_out; i++) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "unique_string_%d_with_some_extra_text_to_make_it_longer", i);
        
        // Intern the string
        string_pool_intern(&pool, buffer, strlen(buffer));
        
        if (i % 5000 == 0) {
            printf("Interned %d strings...\n", i);
            print_memory_stats("During String Pool Test");
        }
    }
    
    print_memory_stats("Before String Pool Free");
    string_pool_free(&pool);
    print_memory_stats("After String Pool Free");
}

int main(int argc, char** argv) {
    // Set up signal handlers
    signal(SIGALRM, timeout_handler);
    signal(SIGINT, sigint_handler);
    
    int timeout_seconds = 60;
    if (argc > 1) {
        timeout_seconds = atoi(argv[1]);
    }
    alarm(timeout_seconds);
    
    printf("=== Comprehensive Memory Stress Test ===\n");
    printf("Timeout: %d seconds\n", timeout_seconds);
    printf("Press Ctrl+C to stop early\n\n");
    
    // Initialize allocators with trace enabled
    AllocatorConfig config = {
        .enable_trace = true,
        .enable_stats = true,
        .arena_size = 64 * 1024,
        .object_pool_size = 256
    };
    allocators_init(&config);
    
    // Run tests
    if (!should_stop && !timed_out) test_parser_leak_stress();
    if (!should_stop && !timed_out) test_vm_object_leak();
    if (!should_stop && !timed_out) test_module_leak();
    if (!should_stop && !timed_out) test_string_pool_leak();
    
    // Final stats
    printf("\n=== FINAL MEMORY STATISTICS ===\n");
    allocators_print_stats();
    
    // Check for leaks
    printf("\n=== LEAK CHECK ===\n");
    allocators_check_leaks();
    
    // Cleanup
    allocators_shutdown();
    
    return 0;
}