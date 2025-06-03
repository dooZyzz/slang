#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "parser/parser.h"
#include "ast/ast.h"
#include "utils/allocators.h"
#include "utils/memory.h"
#include "lexer/lexer.h"
#include "runtime/core/vm.h"
#include "codegen/compiler.h"

// Global flag for timeout
static volatile int timed_out = 0;

void timeout_handler(int sig) {
    (void)sig;
    timed_out = 1;
    printf("\n=== TIMEOUT: Stopping test to prevent WSL crash ===\n");
}

// Test lexer memory leaks
void test_lexer_leaks(const char* source) {
    printf("\n--- Testing Lexer: %.30s... ---\n", source);
    
    Lexer* lexer = lexer_create(source);
    if (!lexer) {
        printf("Failed to create lexer\n");
        return;
    }
    
    // Scan all tokens
    Token token;
    int count = 0;
    do {
        token = lexer_next_token(lexer);
        count++;
    } while (token.type != TOKEN_EOF && count < 1000);
    
    printf("Scanned %d tokens\n", count);
    lexer_destroy(lexer);
}

// Test parser memory leaks with valid programs
void test_parser_valid(const char* source) {
    printf("\n--- Testing Parser (valid): %.30s... ---\n", source);
    
    Parser* parser = parser_create(source);
    if (!parser) {
        printf("Failed to create parser\n");
        return;
    }
    
    ProgramNode* ast = parser_parse_program(parser);
    printf("Parser %s\n", parser->had_error ? "failed" : "succeeded");
    
    parser_destroy(parser);
}

// Test VM creation/destruction
void test_vm_leaks() {
    printf("\n--- Testing VM creation/destruction ---\n");
    
    VM* vm = vm_create();
    if (!vm) {
        printf("Failed to create VM\n");
        return;
    }
    
    // Just create and destroy
    vm_destroy(vm);
    printf("VM test completed\n");
}

const char* test_programs[] = {
    // Simple valid programs
    "let x = 42",
    "func test() { return 5; }",
    "let arr = [1, 2, 3]",
    "let obj = {x: 5, y: 10}",
    
    // String operations
    "let s = \"Hello, World!\"",
    "let s = \"Hello \" + \"World\"",
    
    // Complex expressions
    "let x = 5 + 3 * 2 - 1",
    "let y = (10 + 20) * 30 / 2",
    
    // Control flow
    "if (true) { print(\"yes\"); }",
    "for (let i = 0; i < 10; i++) { print(i); }",
    
    // Invalid programs that should fail
    "let x = \"unclosed string",
    "func test() { return",
    "let arr = [1, 2,",
    
    NULL
};

int main(int argc, char** argv) {
    // Set up timeout handler
    int timeout_seconds = 15;
    if (argc > 1) {
        timeout_seconds = atoi(argv[1]);
    }
    
    signal(SIGALRM, timeout_handler);
    alarm(timeout_seconds);
    
    printf("=== Comprehensive Memory Leak Test ===\n");
    printf("Timeout: %d seconds\n", timeout_seconds);
    
    // Initialize allocators with trace enabled
    AllocatorConfig config = {
        .enable_trace = true,
        .enable_stats = true,
        .arena_size = 64 * 1024,
        .object_pool_size = 256
    };
    allocators_init(&config);
    printf("Allocators initialized with trace enabled\n\n");
    
    // Run various tests
    for (int i = 0; test_programs[i] != NULL && !timed_out; i++) {
        test_lexer_leaks(test_programs[i]);
        if (timed_out) break;
        
        test_parser_valid(test_programs[i]);
        if (timed_out) break;
    }
    
    // Test VM separately
    if (!timed_out) {
        test_vm_leaks();
    }
    
    printf("\n=== Final Allocator Statistics ===\n");
    allocators_print_stats();
    
    // Check for leaks
    printf("\n=== Leak Check ===\n");
    allocators_check_leaks();
    
    // Cleanup
    allocators_shutdown();
    
    return 0;
}