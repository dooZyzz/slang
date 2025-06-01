#include "utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Platform allocator data
typedef struct {
    AllocatorStats stats;
} PlatformAllocatorData;

// Forward declarations
static void platform_free(Allocator* allocator, void* ptr, size_t size, const char* file, int line);

// Platform allocator functions
static void* platform_alloc(Allocator* allocator, size_t size, AllocFlags flags, const char* file, int line, const char* tag) {
    (void)file; (void)line; (void)tag; // Unused in platform allocator
    
    if (size == 0) return NULL;
    
    void* ptr = (flags & ALLOC_FLAG_ZERO) ? calloc(1, size) : malloc(size);
    
    if (ptr) {
        PlatformAllocatorData* data = (PlatformAllocatorData*)allocator->data;
        data->stats.total_allocated += size;
        data->stats.current_usage += size;
        data->stats.allocation_count++;
        
        if (data->stats.current_usage > data->stats.peak_usage) {
            data->stats.peak_usage = data->stats.current_usage;
        }
    }
    
    return ptr;
}

static void* platform_realloc(Allocator* allocator, void* ptr, size_t old_size, size_t new_size, const char* file, int line, const char* tag) {
    (void)file; (void)line; (void)tag; // Unused in platform allocator
    
    if (new_size == 0) {
        platform_free(allocator, ptr, old_size, file, line);
        return NULL;
    }
    
    void* new_ptr = realloc(ptr, new_size);
    
    if (new_ptr) {
        PlatformAllocatorData* data = (PlatformAllocatorData*)allocator->data;
        
        // Update stats
        data->stats.current_usage = data->stats.current_usage - old_size + new_size;
        data->stats.total_allocated += (new_size > old_size) ? (new_size - old_size) : 0;
        
        if (data->stats.current_usage > data->stats.peak_usage) {
            data->stats.peak_usage = data->stats.current_usage;
        }
    }
    
    return new_ptr;
}

static void platform_free(Allocator* allocator, void* ptr, size_t size, const char* file, int line) {
    (void)file; (void)line; // Unused in platform allocator
    
    if (!ptr) return;
    
    free(ptr);
    
    PlatformAllocatorData* data = (PlatformAllocatorData*)allocator->data;
    data->stats.total_freed += size;
    data->stats.current_usage -= size;
    data->stats.free_count++;
}

static void platform_reset(Allocator* allocator) {
    (void)allocator; // Nothing to reset for platform allocator
}

static void platform_destroy(Allocator* allocator) {
    if (allocator) {
        free(allocator->data);
        free(allocator);
    }
}

static AllocatorStats platform_get_stats(Allocator* allocator) {
    PlatformAllocatorData* data = (PlatformAllocatorData*)allocator->data;
    return data->stats;
}

static char* platform_format_stats(Allocator* allocator) {
    PlatformAllocatorData* data = (PlatformAllocatorData*)allocator->data;
    AllocatorStats* stats = &data->stats;
    
    char* buffer = malloc(512);
    if (!buffer) return NULL;
    
    snprintf(buffer, 512,
        "=== Platform Allocator Stats ===\n"
        "Total Allocated:  %zu bytes\n"
        "Total Freed:      %zu bytes\n"
        "Current Usage:    %zu bytes\n"
        "Peak Usage:       %zu bytes\n"
        "Allocations:      %zu\n"
        "Frees:            %zu\n"
        "Active:           %zu\n"
        "===============================",
        stats->total_allocated,
        stats->total_freed,
        stats->current_usage,
        stats->peak_usage,
        stats->allocation_count,
        stats->free_count,
        stats->allocation_count - stats->free_count
    );
    
    return buffer;
}

// Create platform allocator
Allocator* mem_create_platform_allocator(void) {
    Allocator* allocator = calloc(1, sizeof(Allocator));
    if (!allocator) return NULL;
    
    PlatformAllocatorData* data = calloc(1, sizeof(PlatformAllocatorData));
    if (!data) {
        free(allocator);
        return NULL;
    }
    
    allocator->type = ALLOCATOR_PLATFORM;
    allocator->alloc = platform_alloc;
    allocator->realloc = platform_realloc;
    allocator->free = platform_free;
    allocator->reset = platform_reset;
    allocator->destroy = platform_destroy;
    allocator->get_stats = platform_get_stats;
    allocator->format_stats = platform_format_stats;
    allocator->data = data;
    
    return allocator;
}