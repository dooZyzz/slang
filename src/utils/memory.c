#include "utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

// Global default allocator
static Allocator* default_allocator = NULL;

// Initialize memory system
void mem_init(void) {
    if (!default_allocator) {
        default_allocator = mem_create_platform_allocator();
    }
}

// Shutdown memory system
void mem_shutdown(void) {
    if (default_allocator) {
        mem_destroy(default_allocator);
        default_allocator = NULL;
    }
}

// Set default allocator
void mem_set_default_allocator(Allocator* allocator) {
    default_allocator = allocator;
}

// Get default allocator
Allocator* mem_get_default_allocator(void) {
    if (!default_allocator) {
        mem_init();
    }
    return default_allocator;
}

// Core allocation functions
void* mem_alloc(Allocator* allocator, size_t size, AllocFlags flags, const char* file, int line, const char* tag) {
    if (!allocator) {
        allocator = mem_get_default_allocator();
    }
    if (!allocator || !allocator->alloc) {
        return NULL;
    }
    return allocator->alloc(allocator, size, flags, file, line, tag);
}

void* mem_realloc(Allocator* allocator, void* ptr, size_t old_size, size_t new_size, const char* file, int line, const char* tag) {
    if (!allocator) {
        allocator = mem_get_default_allocator();
    }
    if (!allocator || !allocator->realloc) {
        return NULL;
    }
    return allocator->realloc(allocator, ptr, old_size, new_size, file, line, tag);
}

void mem_free(Allocator* allocator, void* ptr, size_t size, const char* file, int line) {
    if (!ptr) return;
    if (!allocator) {
        allocator = mem_get_default_allocator();
    }
    if (!allocator || !allocator->free) {
        return;
    }
    allocator->free(allocator, ptr, size, file, line);
}

void* mem_dup(Allocator* allocator, const void* ptr, size_t size, const char* file, int line, const char* tag) {
    if (!ptr || size == 0) return NULL;
    
    void* new_ptr = mem_alloc(allocator, size, ALLOC_FLAG_NONE, file, line, tag);
    if (new_ptr) {
        memcpy(new_ptr, ptr, size);
    }
    return new_ptr;
}

char* mem_strdup(Allocator* allocator, const char* str, const char* file, int line, const char* tag) {
    if (!str) return NULL;
    
    size_t len = strlen(str) + 1;
    char* new_str = (char*)mem_alloc(allocator, len, ALLOC_FLAG_NONE, file, line, tag);
    if (new_str) {
        memcpy(new_str, str, len);
    }
    return new_str;
}

// Allocator operations
void mem_reset(Allocator* allocator) {
    if (allocator && allocator->reset) {
        allocator->reset(allocator);
    }
}

void mem_destroy(Allocator* allocator) {
    if (allocator && allocator->destroy) {
        allocator->destroy(allocator);
    }
}

AllocatorStats mem_get_stats(Allocator* allocator) {
    if (allocator && allocator->get_stats) {
        return allocator->get_stats(allocator);
    }
    AllocatorStats empty = {0};
    return empty;
}

char* mem_format_stats(Allocator* allocator) {
    if (allocator && allocator->format_stats) {
        return allocator->format_stats(allocator);
    }
    return NULL;
}

// Memory debugging stubs (implemented by trace allocator)
void mem_check_leaks(Allocator* allocator) {
    if (allocator && allocator->type == ALLOCATOR_TRACE) {
        // Trace allocator will implement this
        AllocatorStats stats = mem_get_stats(allocator);
        if (stats.current_usage > 0) {
            fprintf(stderr, "Memory leaks detected: %zu bytes in %zu allocations\n", 
                    stats.current_usage, stats.allocation_count - stats.free_count);
            mem_print_allocations(allocator);
        }
    }
}

void mem_print_allocations(Allocator* allocator) {
    if (allocator && allocator->type == ALLOCATOR_TRACE) {
        char* stats = mem_format_stats(allocator);
        if (stats) {
            printf("%s\n", stats);
            free(stats);
        }
    }
}