#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>
#include "parser/parser.h"
#include "ast/ast.h"
#include "utils/allocators.h"
#include "utils/memory.h"

// Global flag for timeout
static volatile int timed_out = 0;

void timeout_handler(int sig) {
    (void)sig;
    timed_out = 1;
    printf("\n=== TIMEOUT: Stopping test to prevent WSL crash ===\n");
}

// Test cases that should cause parser failures
const char* invalid_programs[] = {
    // Unclosed string interpolation
    "let x = \"Hello ${name",
    "let x = \"Hello ${ unclosed expression",
    "let x = \"${",
    
    // Unclosed blocks
    "func test() { let x = 5",
    "if (true) { print(\"test\")",
    "for (let i = 0; i < 10; i++) {",
    
    // Invalid syntax
    "let 123invalid = 5",
    "func () { }",  // Missing function name
    "let x = 5 +",  // Incomplete expression
    "import",       // Missing module path
    "struct { }",   // Missing struct name
    
    // Deeply nested unclosed structures
    "func a() { func b() { func c() { func d() {",
    "if (true) { if (false) { if (maybe) { if (sure) {",
    
    // Complex string interpolation errors
    "let x = \"Hello ${name} and ${age} and ${",
    "let x = \"Test ${ func() { return 5; }",
    
    // Array/object literal errors
    "let arr = [1, 2, 3,",
    "let obj = {x: 5, y:",
    "let obj = {x: 5, y: 10,",
    
    // Mixed errors
    "func test() { let x = \"Hello ${world",
    "struct Person { let name = \"${",
    
    NULL
};

void test_parser_with_trace(const char* source, const char* test_name) {
    printf("\n--- Testing: %s ---\n", test_name);
    printf("Source: %.50s%s\n", source, strlen(source) > 50 ? "..." : "");
    
    // Create parser with trace allocator
    Parser* parser = parser_create(source);
    if (!parser) {
        printf("Failed to create parser\n");
        return;
    }
    
    // Try to parse
    ProgramNode* ast = parser_parse_program(parser);
    
    if (parser->had_error) {
        printf("Parser error detected (expected)\n");
    } else {
        printf("No parser error (unexpected!)\n");
    }
    
    // Clean up
    if (ast) {
        // AST is cleaned up when allocators are reset/destroyed
        // since it uses arena allocation
    }
    parser_destroy(parser);
}

int main(int argc, char** argv) {
    // Set up timeout handler (default 30 seconds, can be overridden)
    int timeout_seconds = 30;
    if (argc > 1) {
        timeout_seconds = atoi(argv[1]);
    }
    
    signal(SIGALRM, timeout_handler);
    alarm(timeout_seconds);
    
    printf("=== Parser Memory Leak Test ===\n");
    printf("Timeout: %d seconds\n", timeout_seconds);
    printf("Using trace allocator to detect leaks\n\n");
    
    // Initialize allocators with trace enabled
    AllocatorConfig config = {
        .enable_trace = true,
        .enable_stats = true,
        .arena_size = 64 * 1024,
        .object_pool_size = 256
    };
    allocators_init(&config);
    printf("Allocators initialized with trace enabled\n");
    
    // Run tests
    int test_count = 0;
    for (int i = 0; invalid_programs[i] != NULL && !timed_out; i++) {
        test_parser_with_trace(invalid_programs[i], invalid_programs[i]);
        test_count++;
    }
    
    printf("\n=== Test Summary ===\n");
    printf("Tests run: %d\n", test_count);
    
    // Get allocator stats for all systems
    printf("\n=== Allocator Statistics ===\n");
    allocators_print_stats();
    
    // Cleanup allocators
    allocators_shutdown();
    
    return 0;
}