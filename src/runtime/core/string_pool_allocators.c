#include "runtime/core/string_pool.h"
#include "utils/allocators.h"
#include <stdlib.h>
#include <string.h>

#define INITIAL_BUCKET_COUNT 32
#define MAX_LOAD_FACTOR 0.75

static uint32_t hash_string(const char* string, size_t length) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint8_t)string[i];
        hash *= 16777619;
    }
    return hash;
}

void string_pool_init(StringPool* pool) {
    pool->bucket_count = INITIAL_BUCKET_COUNT;
    pool->buckets = STR_ALLOC(pool->bucket_count * sizeof(StringEntry*));
    if (pool->buckets) {
        memset(pool->buckets, 0, pool->bucket_count * sizeof(StringEntry*));
    }
    pool->entry_count = 0;
    pool->all_strings = NULL;
}

void string_pool_free(StringPool* pool) {
    // Note: With arena allocator for strings, we don't need to free individual strings
    // The arena will be reset/destroyed which frees all strings at once
    
    // Free buckets array
    if (pool->buckets) {
        STR_FREE(pool->buckets, pool->bucket_count * sizeof(StringEntry*));
    }
    
    // Reset
    pool->buckets = NULL;
    pool->bucket_count = 0;
    pool->entry_count = 0;
    pool->all_strings = NULL;
}

static StringEntry* find_entry(StringPool* pool, const char* string, size_t length, uint32_t hash) {
    if (!pool->buckets) return NULL;
    
    uint32_t index = hash % pool->bucket_count;
    StringEntry* entry = pool->buckets[index];
    
    while (entry) {
        if (entry->length == length && memcmp(entry->string, string, length) == 0) {
            return entry;
        }
        entry = entry->next;
    }
    
    return NULL;
}

static void resize_pool(StringPool* pool) {
    size_t new_bucket_count = pool->bucket_count * 2;
    StringEntry** new_buckets = STR_ALLOC(new_bucket_count * sizeof(StringEntry*));
    if (!new_buckets) return; // Failed to allocate
    
    memset(new_buckets, 0, new_bucket_count * sizeof(StringEntry*));
    
    // Rehash all entries
    for (size_t i = 0; i < pool->bucket_count; i++) {
        StringEntry* entry = pool->buckets[i];
        while (entry) {
            StringEntry* next = entry->next;
            uint32_t hash = hash_string(entry->string, entry->length);
            uint32_t new_index = hash % new_bucket_count;
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            entry = next;
        }
    }
    
    STR_FREE(pool->buckets, pool->bucket_count * sizeof(StringEntry*));
    pool->buckets = new_buckets;
    pool->bucket_count = new_bucket_count;
}

char* string_pool_intern(StringPool* pool, const char* string, size_t length) {
    if (!pool->buckets) return NULL;
    
    uint32_t hash = hash_string(string, length);
    StringEntry* existing = find_entry(pool, string, length, hash);
    
    if (existing) {
        return existing->string;
    }
    
    // Check if we need to resize
    if (pool->entry_count + 1 > pool->bucket_count * MAX_LOAD_FACTOR) {
        resize_pool(pool);
    }
    
    // Create new entry - using string allocator (arena)
    StringEntry* entry = STR_ALLOC(sizeof(StringEntry));
    if (!entry) return NULL;
    
    entry->string = STR_ALLOC(length + 1);
    if (!entry->string) {
        // Note: With arena allocator, we can't free the entry
        // but the arena will clean up everything eventually
        return NULL;
    }
    
    memcpy(entry->string, string, length);
    entry->string[length] = '\0';
    entry->length = length;
    entry->marked = false;
    
    // Add to bucket
    uint32_t index = hash % pool->bucket_count;
    entry->next = pool->buckets[index];
    pool->buckets[index] = entry;
    
    // Add to all strings list
    entry->all_next = pool->all_strings;
    pool->all_strings = entry;
    
    pool->entry_count++;
    
    return entry->string;
}

char* string_pool_intern_cstring(StringPool* pool, const char* string) {
    return string_pool_intern(pool, string, strlen(string));
}

bool string_pool_contains(StringPool* pool, const char* string, size_t length) {
    if (!pool->buckets) return false;
    
    uint32_t hash = hash_string(string, length);
    return find_entry(pool, string, length, hash) != NULL;
}

void string_pool_mark_all(StringPool* pool) {
    StringEntry* entry = pool->all_strings;
    while (entry) {
        entry->marked = false;
        entry = entry->all_next;
    }
}

void string_pool_mark_string(StringPool* pool, const char* string) {
    if (!string || !pool->buckets) return;
    
    size_t length = strlen(string);
    uint32_t hash = hash_string(string, length);
    StringEntry* entry = find_entry(pool, string, length, hash);
    
    if (entry) {
        entry->marked = true;
    }
}

// Note: sweep_unmarked is not applicable with arena allocator
// Strings live until the arena is reset
size_t string_pool_sweep_unmarked(StringPool* pool) {
    // With arena allocator, we don't sweep individual strings
    // Return 0 to indicate no strings were freed
    return 0;
}

size_t string_pool_count(StringPool* pool) {
    return pool->entry_count;
}

size_t string_pool_memory_usage(StringPool* pool) {
    size_t total = pool->bucket_count * sizeof(StringEntry*);
    
    StringEntry* entry = pool->all_strings;
    while (entry) {
        total += sizeof(StringEntry);
        total += entry->length + 1;
        entry = entry->all_next;
    }
    
    return total;
}