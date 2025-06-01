#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>
#include <stdbool.h>

// Forward declarations
typedef struct Allocator Allocator;
typedef struct AllocatorStats AllocatorStats;
typedef struct AllocationInfo AllocationInfo;

// Allocator types
typedef enum {
    ALLOCATOR_PLATFORM,  // Standard malloc/free
    ALLOCATOR_ARENA,     // Arena/linear allocator
    ALLOCATOR_FREELIST,  // Freelist allocator
    ALLOCATOR_TRACE      // Tracing allocator for debugging
} AllocatorType;

// Allocation flags
typedef enum {
    ALLOC_FLAG_NONE = 0,
    ALLOC_FLAG_ZERO = 1 << 0,  // Zero memory (like calloc)
} AllocFlags;

// Memory statistics
struct AllocatorStats {
    size_t total_allocated;
    size_t total_freed;
    size_t current_usage;
    size_t peak_usage;
    size_t allocation_count;
    size_t free_count;
};

// Allocation tracking info (for trace allocator)
struct AllocationInfo {
    void* ptr;
    size_t size;
    const char* file;
    int line;
    const char* tag;  // Optional debug string
    struct AllocationInfo* next;
};

// Allocator interface
struct Allocator {
    AllocatorType type;
    void* (*alloc)(Allocator* allocator, size_t size, AllocFlags flags, const char* file, int line, const char* tag);
    void* (*realloc)(Allocator* allocator, void* ptr, size_t old_size, size_t new_size, const char* file, int line, const char* tag);
    void (*free)(Allocator* allocator, void* ptr, size_t size, const char* file, int line);
    void (*reset)(Allocator* allocator);  // For arena allocator
    void (*destroy)(Allocator* allocator);
    AllocatorStats (*get_stats)(Allocator* allocator);
    char* (*format_stats)(Allocator* allocator);  // Returns formatted stats table
    void* data;  // Allocator-specific data
};

// Convenience macros for tracking file/line
#define MEM_ALLOC(allocator, size) \
    mem_alloc(allocator, size, ALLOC_FLAG_NONE, __FILE__, __LINE__, NULL)

#define MEM_ALLOC_ZERO(allocator, size) \
    mem_alloc(allocator, size, ALLOC_FLAG_ZERO, __FILE__, __LINE__, NULL)

#define MEM_ALLOC_TAGGED(allocator, size, tag) \
    mem_alloc(allocator, size, ALLOC_FLAG_NONE, __FILE__, __LINE__, tag)

#define MEM_ALLOC_ZERO_TAGGED(allocator, size, tag) \
    mem_alloc(allocator, size, ALLOC_FLAG_ZERO, __FILE__, __LINE__, tag)

#define MEM_REALLOC(allocator, ptr, old_size, new_size) \
    mem_realloc(allocator, ptr, old_size, new_size, __FILE__, __LINE__, NULL)

#define MEM_REALLOC_TAGGED(allocator, ptr, old_size, new_size, tag) \
    mem_realloc(allocator, ptr, old_size, new_size, __FILE__, __LINE__, tag)

#define MEM_FREE(allocator, ptr, size) \
    mem_free(allocator, ptr, size, __FILE__, __LINE__)

#define MEM_DUP(allocator, ptr, size) \
    mem_dup(allocator, ptr, size, __FILE__, __LINE__, NULL)

#define MEM_STRDUP(allocator, str) \
    mem_strdup(allocator, str, __FILE__, __LINE__, NULL)

// Type-safe allocation macros
#define MEM_NEW(allocator, type) \
    ((type*)MEM_ALLOC_ZERO(allocator, sizeof(type)))

#define MEM_NEW_ARRAY(allocator, type, count) \
    ((type*)MEM_ALLOC_ZERO(allocator, sizeof(type) * (count)))

#define MEM_NEW_TAGGED(allocator, type, tag) \
    ((type*)MEM_ALLOC_ZERO_TAGGED(allocator, sizeof(type), tag))

#define MEM_NEW_ARRAY_TAGGED(allocator, type, count, tag) \
    ((type*)MEM_ALLOC_ZERO_TAGGED(allocator, sizeof(type) * (count), tag))

#define MEM_STRDUP_TAGGED(allocator, str, tag) \
    mem_strdup(allocator, str, __FILE__, __LINE__, tag)

// Global allocator management
void mem_init(void);
void mem_shutdown(void);
void mem_set_default_allocator(Allocator* allocator);
Allocator* mem_get_default_allocator(void);

// Allocator creation
Allocator* mem_create_platform_allocator(void);
Allocator* mem_create_arena_allocator(size_t initial_size);
Allocator* mem_create_freelist_allocator(size_t block_size, size_t initial_blocks);
Allocator* mem_create_trace_allocator(Allocator* backing_allocator);

// Core allocation functions
void* mem_alloc(Allocator* allocator, size_t size, AllocFlags flags, const char* file, int line, const char* tag);
void* mem_realloc(Allocator* allocator, void* ptr, size_t old_size, size_t new_size, const char* file, int line, const char* tag);
void mem_free(Allocator* allocator, void* ptr, size_t size, const char* file, int line);
void* mem_dup(Allocator* allocator, const void* ptr, size_t size, const char* file, int line, const char* tag);
char* mem_strdup(Allocator* allocator, const char* str, const char* file, int line, const char* tag);

// Allocator operations
void mem_reset(Allocator* allocator);
void mem_destroy(Allocator* allocator);
AllocatorStats mem_get_stats(Allocator* allocator);
char* mem_format_stats(Allocator* allocator);

// Memory debugging
void mem_check_leaks(Allocator* allocator);
void mem_print_allocations(Allocator* allocator);

#endif // MEMORY_H