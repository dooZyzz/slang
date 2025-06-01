#include "utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

#define DEFAULT_ARENA_SIZE (64 * 1024) // 64KB default
#define ARENA_ALIGNMENT 16

// Arena block
typedef struct ArenaBlock {
    struct ArenaBlock* next;
    size_t size;
    size_t used;
    char data[];
} ArenaBlock;

// Arena allocator data
typedef struct {
    ArenaBlock* current;
    ArenaBlock* first;
    size_t default_block_size;
    AllocatorStats stats;
} ArenaAllocatorData;

// Align size to boundary
static size_t align_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

// Create new arena block
static ArenaBlock* create_arena_block(size_t size) {
    ArenaBlock* block = malloc(sizeof(ArenaBlock) + size);
    if (!block) return NULL;
    
    block->next = NULL;
    block->size = size;
    block->used = 0;
    
    return block;
}

// Arena allocator functions
static void* arena_alloc(Allocator* allocator, size_t size, AllocFlags flags, const char* file, int line, const char* tag) {
    (void)file; (void)line; (void)tag; // Unused in arena allocator
    
    if (size == 0) return NULL;
    
    ArenaAllocatorData* data = (ArenaAllocatorData*)allocator->data;
    size_t aligned_size = align_size(size, ARENA_ALIGNMENT);
    
    // Check if current block has enough space
    if (!data->current || data->current->used + aligned_size > data->current->size) {
        // Need new block
        size_t block_size = (aligned_size > data->default_block_size) ? 
                            aligned_size : data->default_block_size;
        
        ArenaBlock* new_block = create_arena_block(block_size);
        if (!new_block) return NULL;
        
        // Link new block
        if (!data->first) {
            data->first = new_block;
        } else {
            new_block->next = data->current;
        }
        data->current = new_block;
        
        // Update stats for block allocation
        data->stats.total_allocated += sizeof(ArenaBlock) + block_size;
    }
    
    // Allocate from current block
    void* ptr = data->current->data + data->current->used;
    data->current->used += aligned_size;
    
    // Zero memory if requested
    if (flags & ALLOC_FLAG_ZERO) {
        memset(ptr, 0, size);
    }
    
    // Update stats
    data->stats.current_usage += size;
    data->stats.allocation_count++;
    
    if (data->stats.current_usage > data->stats.peak_usage) {
        data->stats.peak_usage = data->stats.current_usage;
    }
    
    return ptr;
}

static void* arena_realloc(Allocator* allocator, void* ptr, size_t old_size, size_t new_size, const char* file, int line, const char* tag) {
    // Arena doesn't support true realloc, allocate new and copy
    if (!ptr) {
        return arena_alloc(allocator, new_size, ALLOC_FLAG_NONE, file, line, tag);
    }
    
    if (new_size == 0) {
        // Arena doesn't free individual allocations
        return NULL;
    }
    
    void* new_ptr = arena_alloc(allocator, new_size, ALLOC_FLAG_NONE, file, line, tag);
    if (new_ptr && old_size > 0) {
        memcpy(new_ptr, ptr, (old_size < new_size) ? old_size : new_size);
    }
    
    return new_ptr;
}

static void arena_free(Allocator* allocator, void* ptr, size_t size, const char* file, int line) {
    (void)allocator; (void)ptr; (void)size; (void)file; (void)line;
    // Arena doesn't free individual allocations
}

static void arena_reset(Allocator* allocator) {
    ArenaAllocatorData* data = (ArenaAllocatorData*)allocator->data;
    
    // Reset all blocks
    ArenaBlock* block = data->first;
    while (block) {
        block->used = 0;
        block = block->next;
    }
    
    // Reset current to first block
    data->current = data->first;
    
    // Update stats
    data->stats.current_usage = 0;
    data->stats.free_count = data->stats.allocation_count;
}

static void arena_destroy(Allocator* allocator) {
    if (!allocator) return;
    
    ArenaAllocatorData* data = (ArenaAllocatorData*)allocator->data;
    
    // Free all blocks
    ArenaBlock* block = data->first;
    while (block) {
        ArenaBlock* next = block->next;
        free(block);
        block = next;
    }
    
    free(data);
    free(allocator);
}

static AllocatorStats arena_get_stats(Allocator* allocator) {
    ArenaAllocatorData* data = (ArenaAllocatorData*)allocator->data;
    return data->stats;
}

static char* arena_format_stats(Allocator* allocator) {
    ArenaAllocatorData* data = (ArenaAllocatorData*)allocator->data;
    AllocatorStats* stats = &data->stats;
    
    // Count blocks
    size_t block_count = 0;
    size_t total_block_size = 0;
    size_t total_used = 0;
    
    ArenaBlock* block = data->first;
    while (block) {
        block_count++;
        total_block_size += block->size;
        total_used += block->used;
        block = block->next;
    }
    
    char* buffer = malloc(1024);
    if (!buffer) return NULL;
    
    snprintf(buffer, 1024,
        "=== Arena Allocator Stats ===\n"
        "Blocks:           %zu\n"
        "Total Block Size: %zu bytes\n"
        "Total Used:       %zu bytes\n"
        "Utilization:      %.1f%%\n"
        "Current Usage:    %zu bytes\n"
        "Peak Usage:       %zu bytes\n"
        "Allocations:      %zu\n"
        "Default Block:    %zu bytes\n"
        "=============================",
        block_count,
        total_block_size,
        total_used,
        total_block_size > 0 ? (100.0 * total_used / total_block_size) : 0.0,
        stats->current_usage,
        stats->peak_usage,
        stats->allocation_count,
        data->default_block_size
    );
    
    return buffer;
}

// Create arena allocator
Allocator* mem_create_arena_allocator(size_t initial_size) {
    Allocator* allocator = calloc(1, sizeof(Allocator));
    if (!allocator) return NULL;
    
    ArenaAllocatorData* data = calloc(1, sizeof(ArenaAllocatorData));
    if (!data) {
        free(allocator);
        return NULL;
    }
    
    data->default_block_size = (initial_size > 0) ? initial_size : DEFAULT_ARENA_SIZE;
    
    allocator->type = ALLOCATOR_ARENA;
    allocator->alloc = arena_alloc;
    allocator->realloc = arena_realloc;
    allocator->free = arena_free;
    allocator->reset = arena_reset;
    allocator->destroy = arena_destroy;
    allocator->get_stats = arena_get_stats;
    allocator->format_stats = arena_format_stats;
    allocator->data = data;
    
    return allocator;
}