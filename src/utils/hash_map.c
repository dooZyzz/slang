#include "utils/hash_map.h"
#include "utils/allocators.h"
#include <string.h>

#define INITIAL_CAPACITY 16
#define LOAD_FACTOR 0.75

typedef struct HashMapEntry {
    char* key;
    void* value;
    struct HashMapEntry* next;
} HashMapEntry;

struct HashMap {
    HashMapEntry** buckets;
    size_t capacity;
    size_t size;
    Allocator* allocator;  // Store allocator reference
};

static unsigned int hash(const char* key) {
    unsigned int hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

HashMap* hash_map_create_with_allocator(Allocator* allocator) {
    if (!allocator) {
        // Use default allocator if none provided
        allocator = allocators_get(ALLOC_SYSTEM_VM);
    }
    
    HashMap* map = MEM_NEW(allocator, HashMap);
    if (!map) return NULL;
    
    map->allocator = allocator;
    map->capacity = INITIAL_CAPACITY;
    map->size = 0;
    map->buckets = MEM_NEW_ARRAY(allocator, HashMapEntry*, map->capacity);
    if (!map->buckets) {
        SLANG_MEM_FREE(allocator, map, sizeof(HashMap));
        return NULL;
    }
    
    // Initialize buckets to NULL
    memset(map->buckets, 0, sizeof(HashMapEntry*) * map->capacity);
    
    return map;
}

HashMap* hash_map_create(void) {
    return hash_map_create_with_allocator(NULL);
}

void hash_map_destroy(HashMap* map) {
    if (!map) return;
    
    Allocator* alloc = map->allocator;
    
    // Free all entries
    for (size_t i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            SLANG_MEM_FREE(alloc, entry->key, strlen(entry->key) + 1);
            SLANG_MEM_FREE(alloc, entry, sizeof(HashMapEntry));
            entry = next;
        }
    }
    
    SLANG_MEM_FREE(alloc, map->buckets, sizeof(HashMapEntry*) * map->capacity);
    SLANG_MEM_FREE(alloc, map, sizeof(HashMap));
}

static void resize(HashMap* map) {
    size_t old_capacity = map->capacity;
    HashMapEntry** old_buckets = map->buckets;
    
    map->capacity *= 2;
    map->buckets = MEM_NEW_ARRAY(map->allocator, HashMapEntry*, map->capacity);
    if (!map->buckets) {
        // Restore old state on failure
        map->buckets = old_buckets;
        map->capacity = old_capacity;
        return;
    }
    
    // Initialize new buckets
    memset(map->buckets, 0, sizeof(HashMapEntry*) * map->capacity);
    
    // Rehash all entries
    for (size_t i = 0; i < old_capacity; i++) {
        HashMapEntry* entry = old_buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            
            // Insert into new bucket
            unsigned int index = hash(entry->key) % map->capacity;
            entry->next = map->buckets[index];
            map->buckets[index] = entry;
            
            entry = next;
        }
    }
    
    // Free old bucket array
    SLANG_MEM_FREE(map->allocator, old_buckets, sizeof(HashMapEntry*) * old_capacity);
}

void hash_map_put(HashMap* map, const char* key, void* value) {
    if (!map || !key) return;
    
    // Check if we need to resize
    if (map->size >= map->capacity * LOAD_FACTOR) {
        resize(map);
    }
    
    unsigned int index = hash(key) % map->capacity;
    
    // Check if key already exists
    HashMapEntry* entry = map->buckets[index];
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            // Update existing entry
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    // Create new entry
    entry = MEM_NEW(map->allocator, HashMapEntry);
    if (!entry) return;
    
    entry->key = MEM_STRDUP(map->allocator, key);
    if (!entry->key) {
        SLANG_MEM_FREE(map->allocator, entry, sizeof(HashMapEntry));
        return;
    }
    
    entry->value = value;
    entry->next = map->buckets[index];
    map->buckets[index] = entry;
    
    map->size++;
}

void* hash_map_get(HashMap* map, const char* key) {
    if (!map || !key) return NULL;
    
    unsigned int index = hash(key) % map->capacity;
    HashMapEntry* entry = map->buckets[index];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            return entry->value;
        }
        entry = entry->next;
    }
    
    return NULL;
}

void hash_map_remove(HashMap* map, const char* key) {
    if (!map || !key) return;
    
    unsigned int index = hash(key) % map->capacity;
    HashMapEntry** prev = &map->buckets[index];
    HashMapEntry* entry = map->buckets[index];
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            *prev = entry->next;
            SLANG_MEM_FREE(map->allocator, entry->key, strlen(entry->key) + 1);
            SLANG_MEM_FREE(map->allocator, entry, sizeof(HashMapEntry));
            map->size--;
            return;
        }
        prev = &entry->next;
        entry = entry->next;
    }
}

void hash_map_iterate(HashMap* map, HashMapIterator iterator, void* user_data) {
    if (!map || !iterator) return;
    
    for (size_t i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            iterator(entry->key, entry->value, user_data);
            entry = entry->next;
        }
    }
}

size_t hash_map_size(HashMap* map) {
    return map ? map->size : 0;
}

bool hash_map_contains(HashMap* map, const char* key) {
    return hash_map_get(map, key) != NULL;
}

void hash_map_clear(HashMap* map) {
    if (!map) return;
    
    // Free all entries
    for (size_t i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            SLANG_MEM_FREE(map->allocator, entry->key, strlen(entry->key) + 1);
            SLANG_MEM_FREE(map->allocator, entry, sizeof(HashMapEntry));
            entry = next;
        }
        map->buckets[i] = NULL;
    }
    
    map->size = 0;
}