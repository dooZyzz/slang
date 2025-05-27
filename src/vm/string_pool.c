#include "vm/string_pool.h"
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
    pool->buckets = calloc(pool->bucket_count, sizeof(StringEntry*));
    pool->entry_count = 0;
    pool->all_strings = NULL;
}

void string_pool_free(StringPool* pool) {
    // Free all strings
    StringEntry* current = pool->all_strings;
    while (current) {
        StringEntry* next = current->all_next;
        free(current->string);
        free(current);
        current = next;
    }
    
    // Free buckets
    free(pool->buckets);
    
    // Reset
    pool->buckets = NULL;
    pool->bucket_count = 0;
    pool->entry_count = 0;
    pool->all_strings = NULL;
}

static StringEntry* find_entry(StringPool* pool, const char* string, size_t length, uint32_t hash) {
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
    StringEntry** new_buckets = calloc(new_bucket_count, sizeof(StringEntry*));
    
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
    
    free(pool->buckets);
    pool->buckets = new_buckets;
    pool->bucket_count = new_bucket_count;
}

char* string_pool_intern(StringPool* pool, const char* string, size_t length) {
    uint32_t hash = hash_string(string, length);
    StringEntry* existing = find_entry(pool, string, length, hash);
    
    if (existing) {
        return existing->string;
    }
    
    // Check if we need to resize
    if (pool->entry_count + 1 > pool->bucket_count * MAX_LOAD_FACTOR) {
        resize_pool(pool);
    }
    
    // Create new entry
    StringEntry* entry = malloc(sizeof(StringEntry));
    entry->string = malloc(length + 1);
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

char* string_pool_create(StringPool* pool, const char* string, size_t length) {
    // For now, just intern - we can optimize this later
    return string_pool_intern(pool, string, length);
}

void string_pool_mark_sweep_begin(StringPool* pool) {
    StringEntry* current = pool->all_strings;
    while (current) {
        current->marked = false;
        current = current->all_next;
    }
}

void string_pool_mark(StringPool* pool, char* string) {
    // Find the entry for this string
    for (size_t i = 0; i < pool->bucket_count; i++) {
        StringEntry* entry = pool->buckets[i];
        while (entry) {
            if (entry->string == string) {
                entry->marked = true;
                return;
            }
            entry = entry->next;
        }
    }
}

void string_pool_6sweep(StringPool* pool) {
    StringEntry** current = &pool->all_strings;
    
    while (*current) {
        if (!(*current)->marked) {
            // Remove from all strings list
            StringEntry* unreached = *current;
            *current = unreached->all_next;
            
            // Remove from hash table
            uint32_t hash = hash_string(unreached->string, unreached->length);
            uint32_t index = hash % pool->bucket_count;
            StringEntry** bucket = &pool->buckets[index];
            while (*bucket) {
                if (*bucket == unreached) {
                    *bucket = unreached->next;
                    break;
                }
                bucket = &(*bucket)->next;
            }
            
            // Free memory
            free(unreached->string);
            free(unreached);
            pool->entry_count--;
        } else {
            current = &(*current)->all_next;
        }
    }
}
