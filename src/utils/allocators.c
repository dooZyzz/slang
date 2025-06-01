#include "utils/allocators.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Allocator instances
static struct {
    Allocator* allocators[ALLOC_SYSTEM_COUNT];
    AllocatorConfig config;
    bool initialized;
} g_allocators = {0};

// System names for debugging
static const char* system_names[] = {
    "VM Core",
    "Objects",
    "Strings",
    "Bytecode",
    "Compiler",
    "AST",
    "Parser",
    "Symbols",
    "Modules",
    "Stdlib",
    "Temp"
};

void allocators_init(const AllocatorConfig* config) {
    if (g_allocators.initialized) {
        return;
    }
    
    // Copy config or use defaults
    if (config) {
        g_allocators.config = *config;
    } else {
        g_allocators.config.enable_trace = false;
        g_allocators.config.enable_stats = true;
        g_allocators.config.arena_size = 64 * 1024;  // 64KB
        g_allocators.config.object_pool_size = 256;
    }
    
    // Initialize memory system
    mem_init();
    
    // Create allocators for each subsystem
    for (int i = 0; i < ALLOC_SYSTEM_COUNT; i++) {
        Allocator* base = NULL;
        
        switch (i) {
            case ALLOC_SYSTEM_VM:
            case ALLOC_SYSTEM_MODULES:
            case ALLOC_SYSTEM_STDLIB:
                // Long-lived data - use platform allocator
                base = mem_create_platform_allocator();
                break;
                
            case ALLOC_SYSTEM_OBJECTS:
                // Objects of various sizes - use platform for now
                // TODO: Could use freelist for common object sizes
                base = mem_create_platform_allocator();
                break;
                
            case ALLOC_SYSTEM_STRINGS:
                // Strings are immutable - arena is perfect
                base = mem_create_arena_allocator(g_allocators.config.arena_size * 4);
                break;
                
            case ALLOC_SYSTEM_BYTECODE:
            case ALLOC_SYSTEM_COMPILER:
            case ALLOC_SYSTEM_AST:
            case ALLOC_SYSTEM_PARSER:
            case ALLOC_SYSTEM_SYMBOLS:
            case ALLOC_SYSTEM_TEMP:
                // Temporary data - use arena allocators
                base = mem_create_arena_allocator(g_allocators.config.arena_size);
                break;
        }
        
        // Wrap with trace allocator if debugging
        if (g_allocators.config.enable_trace) {
            g_allocators.allocators[i] = mem_create_trace_allocator(base);
        } else {
            g_allocators.allocators[i] = base;
        }
    }
    
    g_allocators.initialized = true;
}

void allocators_shutdown(void) {
    if (!g_allocators.initialized) {
        return;
    }
    
    // Check for leaks if tracing
    if (g_allocators.config.enable_trace) {
        allocators_check_leaks();
    }
    
    // Print final stats
    if (g_allocators.config.enable_stats) {
        allocators_print_stats();
    }
    
    // Destroy allocators
    for (int i = 0; i < ALLOC_SYSTEM_COUNT; i++) {
        if (g_allocators.allocators[i]) {
            mem_destroy(g_allocators.allocators[i]);
            g_allocators.allocators[i] = NULL;
        }
    }
    
    // Shutdown memory system
    mem_shutdown();
    
    g_allocators.initialized = false;
}

Allocator* allocators_get(AllocatorSystem system) {
    if (!g_allocators.initialized) {
        allocators_init(NULL);
    }
    
    if (system >= 0 && system < ALLOC_SYSTEM_COUNT) {
        return g_allocators.allocators[system];
    }
    
    return mem_get_default_allocator();
}

void allocators_print_stats(void) {
    printf("\n=== Memory Allocator Statistics ===\n");
    
    for (int i = 0; i < ALLOC_SYSTEM_COUNT; i++) {
        if (g_allocators.allocators[i]) {
            printf("\n--- %s ---\n", system_names[i]);
            char* stats = mem_format_stats(g_allocators.allocators[i]);
            if (stats) {
                printf("%s\n", stats);
                free(stats);
            }
        }
    }
    
    printf("\n=================================\n");
}

void allocators_check_leaks(void) {
    bool has_leaks = false;
    
    for (int i = 0; i < ALLOC_SYSTEM_COUNT; i++) {
        if (g_allocators.allocators[i]) {
            AllocatorStats stats = mem_get_stats(g_allocators.allocators[i]);
            if (stats.current_usage > 0) {
                if (!has_leaks) {
                    printf("\n=== Memory Leaks by Subsystem ===\n");
                    has_leaks = true;
                }
                printf("%s: %zu bytes in %zu allocations\n",
                       system_names[i], stats.current_usage,
                       stats.allocation_count - stats.free_count);
            }
        }
    }
    
    if (has_leaks) {
        printf("=================================\n");
    }
}

void allocators_reset_ast(void) {
    if (g_allocators.initialized && g_allocators.allocators[ALLOC_SYSTEM_AST]) {
        mem_reset(g_allocators.allocators[ALLOC_SYSTEM_AST]);
    }
}

void allocators_reset_temp(void) {
    if (g_allocators.initialized && g_allocators.allocators[ALLOC_SYSTEM_TEMP]) {
        mem_reset(g_allocators.allocators[ALLOC_SYSTEM_TEMP]);
    }
}

void allocators_reset_compiler(void) {
    if (g_allocators.initialized) {
        // Reset compiler-related arenas
        if (g_allocators.allocators[ALLOC_SYSTEM_COMPILER]) {
            mem_reset(g_allocators.allocators[ALLOC_SYSTEM_COMPILER]);
        }
        if (g_allocators.allocators[ALLOC_SYSTEM_BYTECODE]) {
            mem_reset(g_allocators.allocators[ALLOC_SYSTEM_BYTECODE]);
        }
    }
}