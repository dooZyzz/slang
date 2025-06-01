#include "utils/memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

// Freelist node
typedef struct FreeNode {
    struct FreeNode* next;
} FreeNode;

// Memory block header
typedef struct BlockHeader {
    size_t size;
    struct BlockHeader* next;
} BlockHeader;

// Freelist allocator data
typedef struct {
    size_t block_size;
    size_t blocks_per_chunk;
    FreeNode* free_list;
    BlockHeader* chunks;
    AllocatorStats stats;
} FreelistAllocatorData;

// Forward declarations
static void freelist_free(Allocator* allocator, void* ptr, size_t size, const char* file, int line);

// Allocate new chunk of blocks
static bool allocate_chunk(FreelistAllocatorData* data) {
    size_t chunk_size = sizeof(BlockHeader) + (data->block_size * data->blocks_per_chunk);
    BlockHeader* chunk = malloc(chunk_size);
    if (!chunk) return false;
    
    // Initialize chunk header
    chunk->size = chunk_size;
    chunk->next = data->chunks;
    data->chunks = chunk;
    
    // Add blocks to free list
    char* block_ptr = (char*)(chunk + 1);
    for (size_t i = 0; i < data->blocks_per_chunk; i++) {
        FreeNode* node = (FreeNode*)block_ptr;
        node->next = data->free_list;
        data->free_list = node;
        block_ptr += data->block_size;
    }
    
    // Update stats
    data->stats.total_allocated += chunk_size;
    
    return true;
}

// Freelist allocator functions
static void* freelist_alloc(Allocator* allocator, size_t size, AllocFlags flags, const char* file, int line, const char* tag) {
    (void)file; (void)line; (void)tag; // Unused in freelist allocator
    
    FreelistAllocatorData* data = (FreelistAllocatorData*)allocator->data;
    
    // Freelist only allocates fixed-size blocks
    if (size > data->block_size) {
        return NULL;
    }
    
    // Get block from free list
    if (!data->free_list) {
        if (!allocate_chunk(data)) {
            return NULL;
        }
    }
    
    FreeNode* node = data->free_list;
    data->free_list = node->next;
    
    // Zero memory if requested
    if (flags & ALLOC_FLAG_ZERO) {
        memset(node, 0, data->block_size);
    }
    
    // Update stats
    data->stats.current_usage += data->block_size;
    data->stats.allocation_count++;
    
    if (data->stats.current_usage > data->stats.peak_usage) {
        data->stats.peak_usage = data->stats.current_usage;
    }
    
    return node;
}

static void* freelist_realloc(Allocator* allocator, void* ptr, size_t old_size, size_t new_size, const char* file, int line, const char* tag) {
    FreelistAllocatorData* data = (FreelistAllocatorData*)allocator->data;
    
    // Freelist doesn't support resize
    if (new_size > data->block_size) {
        return NULL;
    }
    
    if (!ptr) {
        return freelist_alloc(allocator, new_size, ALLOC_FLAG_NONE, file, line, tag);
    }
    
    if (new_size == 0) {
        freelist_free(allocator, ptr, old_size, file, line);
        return NULL;
    }
    
    // Block is already the right size
    return ptr;
}

static void freelist_free(Allocator* allocator, void* ptr, size_t size, const char* file, int line) {
    (void)size; (void)file; (void)line; // Unused
    
    if (!ptr) return;
    
    FreelistAllocatorData* data = (FreelistAllocatorData*)allocator->data;
    
    // Add block back to free list
    FreeNode* node = (FreeNode*)ptr;
    node->next = data->free_list;
    data->free_list = node;
    
    // Update stats
    data->stats.total_freed += data->block_size;
    data->stats.current_usage -= data->block_size;
    data->stats.free_count++;
}

static void freelist_reset(Allocator* allocator) {
    FreelistAllocatorData* data = (FreelistAllocatorData*)allocator->data;
    
    // Rebuild free list from all chunks
    data->free_list = NULL;
    
    BlockHeader* chunk = data->chunks;
    while (chunk) {
        char* block_ptr = (char*)(chunk + 1);
        size_t num_blocks = (chunk->size - sizeof(BlockHeader)) / data->block_size;
        
        for (size_t i = 0; i < num_blocks; i++) {
            FreeNode* node = (FreeNode*)block_ptr;
            node->next = data->free_list;
            data->free_list = node;
            block_ptr += data->block_size;
        }
        
        chunk = chunk->next;
    }
    
    // Reset stats
    data->stats.current_usage = 0;
    data->stats.free_count = data->stats.allocation_count;
}

static void freelist_destroy(Allocator* allocator) {
    if (!allocator) return;
    
    FreelistAllocatorData* data = (FreelistAllocatorData*)allocator->data;
    
    // Free all chunks
    BlockHeader* chunk = data->chunks;
    while (chunk) {
        BlockHeader* next = chunk->next;
        free(chunk);
        chunk = next;
    }
    
    free(data);
    free(allocator);
}

static AllocatorStats freelist_get_stats(Allocator* allocator) {
    FreelistAllocatorData* data = (FreelistAllocatorData*)allocator->data;
    return data->stats;
}

static char* freelist_format_stats(Allocator* allocator) {
    FreelistAllocatorData* data = (FreelistAllocatorData*)allocator->data;
    AllocatorStats* stats = &data->stats;
    
    // Count free blocks
    size_t free_blocks = 0;
    FreeNode* node = data->free_list;
    while (node) {
        free_blocks++;
        node = node->next;
    }
    
    // Count total blocks
    size_t total_blocks = 0;
    size_t chunk_count = 0;
    BlockHeader* chunk = data->chunks;
    while (chunk) {
        chunk_count++;
        total_blocks += (chunk->size - sizeof(BlockHeader)) / data->block_size;
        chunk = chunk->next;
    }
    
    size_t used_blocks = total_blocks - free_blocks;
    
    char* buffer = malloc(1024);
    if (!buffer) return NULL;
    
    snprintf(buffer, 1024,
        "=== Freelist Allocator Stats ===\n"
        "Block Size:       %zu bytes\n"
        "Blocks/Chunk:     %zu\n"
        "Chunks:           %zu\n"
        "Total Blocks:     %zu\n"
        "Used Blocks:      %zu\n"
        "Free Blocks:      %zu\n"
        "Utilization:      %.1f%%\n"
        "Current Usage:    %zu bytes\n"
        "Peak Usage:       %zu bytes\n"
        "Allocations:      %zu\n"
        "Frees:            %zu\n"
        "================================",
        data->block_size,
        data->blocks_per_chunk,
        chunk_count,
        total_blocks,
        used_blocks,
        free_blocks,
        total_blocks > 0 ? (100.0 * used_blocks / total_blocks) : 0.0,
        stats->current_usage,
        stats->peak_usage,
        stats->allocation_count,
        stats->free_count
    );
    
    return buffer;
}

// Create freelist allocator
Allocator* mem_create_freelist_allocator(size_t block_size, size_t initial_blocks) {
    if (block_size < sizeof(FreeNode)) {
        block_size = sizeof(FreeNode);
    }
    
    Allocator* allocator = calloc(1, sizeof(Allocator));
    if (!allocator) return NULL;
    
    FreelistAllocatorData* data = calloc(1, sizeof(FreelistAllocatorData));
    if (!data) {
        free(allocator);
        return NULL;
    }
    
    data->block_size = block_size;
    data->blocks_per_chunk = (initial_blocks > 0) ? initial_blocks : 64;
    
    allocator->type = ALLOCATOR_FREELIST;
    allocator->alloc = freelist_alloc;
    allocator->realloc = freelist_realloc;
    allocator->free = freelist_free;
    allocator->reset = freelist_reset;
    allocator->destroy = freelist_destroy;
    allocator->get_stats = freelist_get_stats;
    allocator->format_stats = freelist_format_stats;
    allocator->data = data;
    
    return allocator;
}