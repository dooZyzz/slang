#ifndef STRING_POOL_H
#define STRING_POOL_H

#include <stddef.h>
#include <stdbool.h>

typedef struct StringEntry {
    char* string;
    size_t length;
    struct StringEntry* next;      // For hash table chaining
    struct StringEntry* all_next;  // For all strings list
    bool marked;  // For future GC
} StringEntry;

typedef struct {
    StringEntry** buckets;
    size_t bucket_count;
    size_t entry_count;
    StringEntry* all_strings;  // Linked list of all strings for cleanup
} StringPool;

// Initialize the string pool
void string_pool_init(StringPool* pool);

// Free all strings in the pool
void string_pool_free(StringPool* pool);

// Intern a string (returns a pointer that the pool owns)
char* string_pool_intern(StringPool* pool, const char* string, size_t length);

// Create a new string in the pool
char* string_pool_create(StringPool* pool, const char* string, size_t length);

// Mark all strings as unreachable (for GC)
void string_pool_mark_sweep_begin(StringPool* pool);

// Mark a string as reachable
void string_pool_mark(StringPool* pool, char* string);

// Sweep unmarked strings
void string_pool_sweep(StringPool* pool);

#endif
