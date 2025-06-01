#include "runtime/core/string_pool.h"
#include "utils/allocators.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

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
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    pool->buckets = MEM_ALLOC_ZERO(alloc, pool->bucket_count * sizeof(StringEntry*));
    pool->entry_count = 0;
    pool->all_strings = NULL;
}

void string_pool_free(StringPool* pool) {
    // Free all string entries
    StringEntry* current = pool->all_strings;
    while (current) {
        StringEntry* next = current->all_next;
        // Strings use ALLOC_SYSTEM_STRINGS allocator
        STR_FREE(current->string, strlen(current->string) + 1);
        STR_FREE(current, sizeof(StringEntry));
        current = next;
    }
    
    // Free bucket array
    STR_FREE(pool->buckets, pool->bucket_count * sizeof(StringEntry*));
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
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    StringEntry** new_buckets = MEM_ALLOC_ZERO(alloc, new_bucket_count * sizeof(StringEntry*));
    
    if (!new_buckets) return; // Allocation failed, keep current size
    
    // Rehash all entries
    for (size_t i = 0; i < pool->bucket_count; i++) {
        StringEntry* entry = pool->buckets[i];
        while (entry) {
            StringEntry* next = entry->next;
            // Recompute hash since StringEntry doesn't store it
            uint32_t hash = hash_string(entry->string, entry->length);
            uint32_t new_index = hash % new_bucket_count;
            entry->next = new_buckets[new_index];
            new_buckets[new_index] = entry;
            entry = next;
        }
    }
    
    // Free old bucket array and update
    STR_FREE(pool->buckets, pool->bucket_count * sizeof(StringEntry*));
    pool->buckets = new_buckets;
    pool->bucket_count = new_bucket_count;
}

char* string_pool_intern(StringPool* pool, const char* string, size_t length) {
    if (!string) return NULL;
    
    uint32_t hash = hash_string(string, length);
    
    // Check if string already exists
    StringEntry* existing = find_entry(pool, string, length, hash);
    if (existing) {
        return existing->string;
    }
    
    // Check if we need to resize
    if (pool->entry_count >= pool->bucket_count * MAX_LOAD_FACTOR) {
        resize_pool(pool);
    }
    
    // Create new entry
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    StringEntry* entry = MEM_ALLOC(alloc, sizeof(StringEntry));
    if (!entry) return NULL;
    
    entry->string = MEM_ALLOC(alloc, length + 1);
    if (!entry->string) {
        STR_FREE(entry, sizeof(StringEntry));
        return NULL;
    }
    
    memcpy(entry->string, string, length);
    entry->string[length] = '\0';
    entry->length = length;
    entry->marked = false;
    
    // Add to hash table
    uint32_t index = hash % pool->bucket_count;
    entry->next = pool->buckets[index];
    pool->buckets[index] = entry;
    
    // Add to all strings list
    entry->all_next = pool->all_strings;
    pool->all_strings = entry;
    
    pool->entry_count++;
    
    return entry->string;
}

char* string_pool_intern_cstr(StringPool* pool, const char* string) {
    return string_pool_intern(pool, string, strlen(string));
}

char* string_pool_create(StringPool* pool, const char* string, size_t length) {
    // string_pool_create is the same as intern in this implementation
    return string_pool_intern(pool, string, length);
}

void string_pool_mark_sweep_begin(StringPool* pool) {
    // Mark all strings as unreachable
    StringEntry* entry = pool->all_strings;
    while (entry) {
        entry->marked = false;
        entry = entry->all_next;
    }
}

bool string_pool_contains(StringPool* pool, const char* string) {
    if (!string) return false;
    
    size_t length = strlen(string);
    uint32_t hash = hash_string(string, length);
    
    return find_entry(pool, string, length, hash) != NULL;
}

void string_pool_mark(StringPool* pool, char* string) {
    if (!string) return;
    
    // Mark all strings that match this pointer
    StringEntry* entry = pool->all_strings;
    while (entry) {
        if (entry->string == string) {
            entry->marked = true;
            return;
        }
        entry = entry->all_next;
    }
}

void string_pool_sweep(StringPool* pool) {
    StringEntry** current = &pool->all_strings;
    
    while (*current) {
        StringEntry* entry = *current;
        
        if (!entry->marked) {
            // Remove from bucket chain
            uint32_t hash = hash_string(entry->string, entry->length);
            uint32_t index = hash % pool->bucket_count;
            StringEntry** bucket_ptr = &pool->buckets[index];
            while (*bucket_ptr) {
                if (*bucket_ptr == entry) {
                    *bucket_ptr = entry->next;
                    break;
                }
                bucket_ptr = &(*bucket_ptr)->next;
            }
            
            // Remove from all strings list
            *current = entry->all_next;
            
            // Free the entry
            StringEntry* unreached = entry;
            STR_FREE(unreached->string, strlen(unreached->string) + 1);
            STR_FREE(unreached, sizeof(StringEntry));
            
            pool->entry_count--;
        } else {
            // Reset mark for next cycle
            entry->marked = false;
            current = &entry->all_next;
        }
    }
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