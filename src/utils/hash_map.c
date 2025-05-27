#include "utils/hash_map.h"
#include <stdlib.h>
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
};

static unsigned int hash(const char* key) {
    unsigned int hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

HashMap* hash_map_create(void) {
    HashMap* map = calloc(1, sizeof(HashMap));
    if (!map) return NULL;
    
    map->capacity = INITIAL_CAPACITY;
    map->buckets = calloc(map->capacity, sizeof(HashMapEntry*));
    
    return map;
}

void hash_map_destroy(HashMap* map) {
    if (!map) return;
    
    // Free all entries
    for (size_t i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
    }
    
    free(map->buckets);
    free(map);
}

static void resize(HashMap* map) {
    size_t new_capacity = map->capacity * 2;
    HashMapEntry** new_buckets = calloc(new_capacity, sizeof(HashMapEntry*));
    
    // Rehash all entries
    for (size_t i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            unsigned int index = hash(entry->key) % new_capacity;
            entry->next = new_buckets[index];
            new_buckets[index] = entry;
            entry = next;
        }
    }
    
    free(map->buckets);
    map->buckets = new_buckets;
    map->capacity = new_capacity;
}

void hash_map_put(HashMap* map, const char* key, void* value) {
    if (!map || !key) return;
    
    // Check if we need to resize
    if (map->size >= map->capacity * LOAD_FACTOR) {
        resize(map);
    }
    
    unsigned int index = hash(key) % map->capacity;
    HashMapEntry* entry = map->buckets[index];
    
    // Check if key already exists
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            entry->value = value;
            return;
        }
        entry = entry->next;
    }
    
    // Create new entry
    entry = malloc(sizeof(HashMapEntry));
    entry->key = strdup(key);
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

bool hash_map_contains(HashMap* map, const char* key) {
    return hash_map_get(map, key) != NULL;
}

void hash_map_remove(HashMap* map, const char* key) {
    if (!map || !key) return;
    
    unsigned int index = hash(key) % map->capacity;
    HashMapEntry* entry = map->buckets[index];
    HashMapEntry* prev = NULL;
    
    while (entry) {
        if (strcmp(entry->key, key) == 0) {
            if (prev) {
                prev->next = entry->next;
            } else {
                map->buckets[index] = entry->next;
            }
            free(entry->key);
            free(entry);
            map->size--;
            return;
        }
        prev = entry;
        entry = entry->next;
    }
}

void hash_map_clear(HashMap* map) {
    if (!map) return;
    
    for (size_t i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            free(entry->key);
            free(entry);
            entry = next;
        }
        map->buckets[i] = NULL;
    }
    map->size = 0;
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

bool hash_map_is_empty(HashMap* map) {
    return !map || map->size == 0;
}