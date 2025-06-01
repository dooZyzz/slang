#include "utils/memory.h"
#include "utils/hash_map.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <inttypes.h>

// Allocation record
typedef struct AllocRecord {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    char* tag;
    struct AllocRecord* next;
    struct AllocRecord* prev;
} AllocRecord;

// Tag statistics
typedef struct TagStats {
    size_t count;
    size_t total_size;
    size_t peak_size;
} TagStats;

// Trace allocator data
typedef struct {
    Allocator* backing;
    HashMap* allocations;  // ptr -> AllocRecord
    HashMap* tag_stats;    // tag -> TagStats
    AllocRecord* alloc_list;  // Linked list of all allocations
    AllocatorStats stats;
} TraceAllocatorData;

// Forward declarations
static void trace_free(Allocator* allocator, void* ptr, size_t size, const char* file, int line);

// Convert pointer to string key for hashmap
static void ptr_to_key(void* ptr, char* key, size_t key_size) {
    snprintf(key, key_size, "%p", ptr);
}

// Update tag statistics
static void update_tag_stats(TraceAllocatorData* data, const char* tag, size_t size, bool is_alloc) {
    if (!tag) tag = "<untagged>";
    
    TagStats* stats = (TagStats*)hash_map_get(data->tag_stats, tag);
    if (!stats) {
        stats = calloc(1, sizeof(TagStats));
        if (!stats) return;
        hash_map_put(data->tag_stats, tag, stats);
    }
    
    if (is_alloc) {
        stats->count++;
        stats->total_size += size;
        if (stats->total_size > stats->peak_size) {
            stats->peak_size = stats->total_size;
        }
    } else {
        if (stats->count > 0) stats->count--;
        if (stats->total_size >= size) stats->total_size -= size;
    }
}

// Trace allocator functions
static void* trace_alloc(Allocator* allocator, size_t size, AllocFlags flags, const char* file, int line, const char* tag) {
    TraceAllocatorData* data = (TraceAllocatorData*)allocator->data;
    
    // Allocate through backing allocator
    void* ptr = data->backing->alloc(data->backing, size, flags, file, line, tag);
    if (!ptr) return NULL;
    
    // Create allocation record
    AllocRecord* record = calloc(1, sizeof(AllocRecord));
    if (!record) {
        data->backing->free(data->backing, ptr, size, file, line);
        return NULL;
    }
    
    record->ptr = ptr;
    record->size = size;
    record->file = file;
    record->line = line;
    if (tag) {
        record->tag = strdup(tag);
    }
    
    // Add to linked list
    record->next = data->alloc_list;
    if (data->alloc_list) {
        data->alloc_list->prev = record;
    }
    data->alloc_list = record;
    
    // Add to hashmap
    char key[32];
    ptr_to_key(ptr, key, sizeof(key));
    hash_map_put(data->allocations, key, record);
    
    // Update stats
    data->stats.total_allocated += size;
    data->stats.current_usage += size;
    data->stats.allocation_count++;
    
    if (data->stats.current_usage > data->stats.peak_usage) {
        data->stats.peak_usage = data->stats.current_usage;
    }
    
    update_tag_stats(data, tag, size, true);
    
    return ptr;
}

static void* trace_realloc(Allocator* allocator, void* ptr, size_t old_size, size_t new_size, const char* file, int line, const char* tag) {
    TraceAllocatorData* data = (TraceAllocatorData*)allocator->data;
    
    if (!ptr) {
        return trace_alloc(allocator, new_size, ALLOC_FLAG_NONE, file, line, tag);
    }
    
    if (new_size == 0) {
        trace_free(allocator, ptr, old_size, file, line);
        return NULL;
    }
    
    // Find existing record
    char key[32];
    ptr_to_key(ptr, key, sizeof(key));
    AllocRecord* record = (AllocRecord*)hash_map_get(data->allocations, key);
    
    if (!record) {
        fprintf(stderr, "WARNING: Realloc of untracked pointer %p at %s:%d\n", ptr, file, line);
        return NULL;
    }
    
    // Reallocate through backing
    void* new_ptr = data->backing->realloc(data->backing, ptr, old_size, new_size, file, line, tag);
    if (!new_ptr) return NULL;
    
    // Update record
    if (new_ptr != ptr) {
        // Remove old key
        hash_map_remove(data->allocations, key);
        
        // Add new key
        ptr_to_key(new_ptr, key, sizeof(key));
        hash_map_put(data->allocations, key, record);
        record->ptr = new_ptr;
    }
    
    // Update stats
    update_tag_stats(data, record->tag, record->size, false);
    update_tag_stats(data, tag ? tag : record->tag, new_size, true);
    
    data->stats.current_usage = data->stats.current_usage - record->size + new_size;
    data->stats.total_allocated += (new_size > record->size) ? (new_size - record->size) : 0;
    
    if (data->stats.current_usage > data->stats.peak_usage) {
        data->stats.peak_usage = data->stats.current_usage;
    }
    
    record->size = new_size;
    record->file = file;
    record->line = line;
    if (tag && tag != record->tag) {
        free(record->tag);
        record->tag = strdup(tag);
    }
    
    return new_ptr;
}

static void trace_free(Allocator* allocator, void* ptr, size_t size, const char* file, int line) {
    if (!ptr) return;
    
    TraceAllocatorData* data = (TraceAllocatorData*)allocator->data;
    
    // Find record
    char key[32];
    ptr_to_key(ptr, key, sizeof(key));
    AllocRecord* record = (AllocRecord*)hash_map_get(data->allocations, key);
    
    if (!record) {
        fprintf(stderr, "WARNING: Free of untracked pointer %p at %s:%d\n", ptr, file, line);
        return;
    }
    
    if (size != 0 && record->size != size) {
        fprintf(stderr, "WARNING: Size mismatch in free: allocated %zu, freed %zu at %s:%d\n",
                record->size, size, file, line);
    }
    
    // Free through backing
    data->backing->free(data->backing, ptr, record->size, file, line);
    
    // Update stats
    data->stats.total_freed += record->size;
    data->stats.current_usage -= record->size;
    data->stats.free_count++;
    
    update_tag_stats(data, record->tag, record->size, false);
    
    // Remove from linked list
    if (record->prev) {
        record->prev->next = record->next;
    } else {
        data->alloc_list = record->next;
    }
    if (record->next) {
        record->next->prev = record->prev;
    }
    
    // Remove from hashmap
    hash_map_remove(data->allocations, key);
    
    // Free record
    free(record->tag);
    free(record);
}

static void trace_reset(Allocator* allocator) {
    // Trace allocator doesn't support reset
    (void)allocator;
}

// Helper to free tag stats
static void free_tag_stat_helper(const char* key, void* value, void* user_data) {
    (void)key; (void)user_data;
    free(value);
}

static void trace_destroy(Allocator* allocator) {
    if (!allocator) return;
    
    TraceAllocatorData* data = (TraceAllocatorData*)allocator->data;
    
    // Report leaks only if there are any (ignore backing allocator's own allocations)
    if (data->alloc_list && data->stats.current_usage > 0) {
        fprintf(stderr, "\n=== MEMORY LEAKS DETECTED ===\n");
        fprintf(stderr, "%zu bytes leaked in %zu allocations\n",
                data->stats.current_usage,
                data->stats.allocation_count - data->stats.free_count);
        
        // Show first few leaks
        AllocRecord* record = data->alloc_list;
        int count = 0;
        while (record && count < 10) {
            const char* file = record->file ? record->file : "<unknown>";
            const char* tag = record->tag ? record->tag : "<untagged>";
            
            // Extract filename from path
            const char* filename = strrchr(file, '/');
            if (filename) filename++;
            else filename = file;
            
            fprintf(stderr, "  %p: %zu bytes at %s:%d [%s]\n",
                    record->ptr, record->size, filename, record->line, tag);
            
            record = record->next;
            count++;
        }
        
        if (record) {
            fprintf(stderr, "  ... and %zu more allocations\n",
                    (data->stats.allocation_count - data->stats.free_count) - count);
        }
    }
    
    // Free all records
    AllocRecord* record = data->alloc_list;
    while (record) {
        AllocRecord* next = record->next;
        free(record->tag);
        free(record);
        record = next;
    }
    
    // Free tag stats
    if (data->tag_stats) {
        // Free all TagStats structures
        hash_map_iterate(data->tag_stats, free_tag_stat_helper, NULL);
        hash_map_destroy(data->tag_stats);
    }
    
    // Destroy allocations map
    if (data->allocations) {
        hash_map_destroy(data->allocations);
    }
    
    free(data);
    free(allocator);
}

static AllocatorStats trace_get_stats(Allocator* allocator) {
    TraceAllocatorData* data = (TraceAllocatorData*)allocator->data;
    return data->stats;
}

// Format stats helper for tag statistics
typedef struct {
    char* buffer;
    size_t offset;
    size_t size;
} FormatContext;

static void format_tag_stat(const char* key, void* value, void* user_data) {
    FormatContext* ctx = (FormatContext*)user_data;
    TagStats* stats = (TagStats*)value;
    
    if (stats->count > 0) {
        int written = snprintf(ctx->buffer + ctx->offset, ctx->size - ctx->offset,
                              "| %-30s | %8zu | %12zu | %12zu |\n",
                              key, stats->count, stats->total_size, stats->peak_size);
        if (written > 0) {
            ctx->offset += written;
        }
    }
}

static char* trace_format_stats(Allocator* allocator) {
    TraceAllocatorData* data = (TraceAllocatorData*)allocator->data;
    AllocatorStats* stats = &data->stats;
    
    size_t buffer_size = 4096;
    char* buffer = malloc(buffer_size);
    if (!buffer) return NULL;
    
    size_t offset = 0;
    
    // Overall stats
    offset += snprintf(buffer + offset, buffer_size - offset,
        "\n=== Trace Allocator Stats ===\n"
        "Total Allocated:  %zu bytes\n"
        "Total Freed:      %zu bytes\n"
        "Current Usage:    %zu bytes\n"
        "Peak Usage:       %zu bytes\n"
        "Allocations:      %zu\n"
        "Frees:            %zu\n"
        "Active:           %zu\n",
        stats->total_allocated,
        stats->total_freed,
        stats->current_usage,
        stats->peak_usage,
        stats->allocation_count,
        stats->free_count,
        stats->allocation_count - stats->free_count
    );
    
    // Tag statistics
    if (hash_map_size(data->tag_stats) > 0) {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "\n=== Allocation by Tag ===\n"
            "+--------------------------------+----------+--------------+--------------+\n"
            "| Tag                            | Count    | Current Size | Peak Size    |\n"
            "+--------------------------------+----------+--------------+--------------+\n"
        );
        
        FormatContext ctx = { buffer, offset, buffer_size };
        hash_map_iterate(data->tag_stats, format_tag_stat, &ctx);
        offset = ctx.offset;
        
        offset += snprintf(buffer + offset, buffer_size - offset,
            "+--------------------------------+----------+--------------+--------------+\n"
        );
    }
    
    // Active allocations
    if (data->alloc_list) {
        offset += snprintf(buffer + offset, buffer_size - offset,
            "\n=== Active Allocations ===\n"
            "Address          Size       Location                     Tag\n"
            "------------------------------------------------------------------------\n"
        );
        
        AllocRecord* record = data->alloc_list;
        int count = 0;
        while (record && count < 20) {  // Limit to first 20
            const char* file = record->file ? record->file : "<unknown>";
            const char* tag = record->tag ? record->tag : "<untagged>";
            
            // Extract filename from path
            const char* filename = strrchr(file, '/');
            if (filename) filename++;
            else filename = file;
            
            offset += snprintf(buffer + offset, buffer_size - offset,
                "%-16p %-10zu %-20s:%-6d %s\n",
                record->ptr, record->size, filename, record->line, tag
            );
            
            record = record->next;
            count++;
        }
        
        if (record) {
            offset += snprintf(buffer + offset, buffer_size - offset,
                "... and %zu more allocations\n",
                (stats->allocation_count - stats->free_count) - count
            );
        }
    }
    
    return buffer;
}

// Create trace allocator
Allocator* mem_create_trace_allocator(Allocator* backing_allocator) {
    if (!backing_allocator) return NULL;
    
    Allocator* allocator = calloc(1, sizeof(Allocator));
    if (!allocator) return NULL;
    
    TraceAllocatorData* data = calloc(1, sizeof(TraceAllocatorData));
    if (!data) {
        free(allocator);
        return NULL;
    }
    
    data->backing = backing_allocator;
    data->allocations = hash_map_create();
    data->tag_stats = hash_map_create();
    
    if (!data->allocations || !data->tag_stats) {
        if (data->allocations) hash_map_destroy(data->allocations);
        if (data->tag_stats) hash_map_destroy(data->tag_stats);
        free(data);
        free(allocator);
        return NULL;
    }
    
    allocator->type = ALLOCATOR_TRACE;
    allocator->alloc = trace_alloc;
    allocator->realloc = trace_realloc;
    allocator->free = trace_free;
    allocator->reset = trace_reset;
    allocator->destroy = trace_destroy;
    allocator->get_stats = trace_get_stats;
    allocator->format_stats = trace_format_stats;
    allocator->data = data;
    
    return allocator;
}