#ifndef ALLOC_H
#define ALLOC_H

#include "utils/memory.h"
#include <string.h>

// Global allocator selection
Allocator* get_allocator(void);
void set_allocator(Allocator* allocator);

// Drop-in replacements for standard allocation functions
// These use the global allocator by default

#define ALLOC(size) \
    MEM_ALLOC(get_allocator(), size)

#define ALLOC_ZERO(size) \
    MEM_ALLOC_ZERO(get_allocator(), size)

#define ALLOC_TAG(size, tag) \
    MEM_ALLOC_TAGGED(get_allocator(), size, tag)

#define REALLOC(ptr, old_size, new_size) \
    MEM_REALLOC(get_allocator(), ptr, old_size, new_size)

#define FREE(ptr, size) \
    SLANG_MEM_FREE(get_allocator(), ptr, size)

#define STRDUP(str) \
    MEM_STRDUP(get_allocator(), str)

// Type-safe allocation
#define NEW(type) \
    MEM_NEW(get_allocator(), type)

#define NEW_ARRAY(type, count) \
    MEM_NEW_ARRAY(get_allocator(), type, count)

// Migration helpers - these make it easy to convert existing code
// Just replace malloc -> MALLOC, calloc -> CALLOC, etc.

#define MALLOC(size) ALLOC(size)
#define CALLOC(nmemb, size) ALLOC_ZERO((nmemb) * (size))
#define REALLOC_SIMPLE(ptr, new_size) REALLOC(ptr, 0, new_size)  // Note: loses old_size info
#define FREE_SIMPLE(ptr) FREE(ptr, 0)  // Note: loses size info

// For gradual migration - these track file/line but use simplified interface
static inline void* alloc_malloc(size_t size, const char* file, int line) {
    return mem_alloc(get_allocator(), size, ALLOC_FLAG_NONE, file, line, NULL);
}

static inline void* alloc_calloc(size_t nmemb, size_t size, const char* file, int line) {
    return mem_alloc(get_allocator(), nmemb * size, ALLOC_FLAG_ZERO, file, line, NULL);
}

static inline void alloc_free(void* ptr, const char* file, int line) {
    mem_free(get_allocator(), ptr, 0, file, line);
}

static inline char* alloc_strdup(const char* str, const char* file, int line) {
    return mem_strdup(get_allocator(), str, file, line, NULL);
}

// Tracked versions for migration
#define MALLOC_TRACKED(size) alloc_malloc(size, __FILE__, __LINE__)
#define CALLOC_TRACKED(nmemb, size) alloc_calloc(nmemb, size, __FILE__, __LINE__)
#define FREE_TRACKED(ptr) alloc_free(ptr, __FILE__, __LINE__)
#define STRDUP_TRACKED(str) alloc_strdup(str, __FILE__, __LINE__)

// Arena allocator helpers
#define WITH_ARENA(arena_var, size, body) \
    do { \
        Allocator* arena_var = mem_create_arena_allocator(size); \
        Allocator* _saved = get_allocator(); \
        set_allocator(arena_var); \
        body \
        set_allocator(_saved); \
        mem_destroy(arena_var); \
    } while(0)

// Temporary allocation helper using arena
#define WITH_TEMP_ALLOC(size, body) \
    WITH_ARENA(_temp_arena, size, body)

#endif // ALLOC_H