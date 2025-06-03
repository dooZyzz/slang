#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "utils/allocators.h"
#include "utils/cli.h"
#include "semantic/type.h"
#include "semantic/symbol_table.h"

// Test direct malloc usage in various subsystems
int main() {
    printf("=== Testing Direct Malloc Usage ===\n\n");
    
    // Initialize allocators with trace
    AllocatorConfig config = {
        .enable_trace = true,
        .enable_stats = true,
        .arena_size = 64 * 1024,
        .object_pool_size = 256
    };
    allocators_init(&config);
    
    printf("1. Testing type.c malloc usage:\n");
    // Type system uses direct calloc/free
    Type* int_type = type_create_basic(TYPE_INT);
    Type* array_type = type_create_array(int_type);
    printf("Created types (using direct calloc)\n");
    
    printf("\n2. Testing symbol_table.c malloc usage:\n");
    // Symbol table uses direct calloc
    SymbolTable* table = symbol_table_create();
    symbol_table_add(table, "test_var", SYMBOL_VARIABLE);
    printf("Created symbol table (using direct calloc)\n");
    
    printf("\n3. Allocator statistics (won't show direct mallocs):\n");
    allocators_print_stats();
    
    printf("\n4. Cleaning up...\n");
    // These use direct free()
    type_destroy(array_type);
    type_destroy(int_type);
    symbol_table_destroy(table);
    
    printf("\n5. Final allocator statistics:\n");
    allocators_print_stats();
    
    allocators_shutdown();
    
    printf("\n=== IMPORTANT FINDINGS ===\n");
    printf("1. Many subsystems use direct malloc/calloc/free\n");
    printf("2. These allocations are NOT tracked by the allocator system\n");
    printf("3. This is likely the source of memory leaks\n");
    printf("4. Key offenders:\n");
    printf("   - semantic/type.c\n");
    printf("   - semantic/symbol_table.c\n");
    printf("   - utils/cli.c\n");
    printf("   - runtime/packages/*.c\n");
    printf("   - runtime/core/bootstrap.c\n");
    
    return 0;
}