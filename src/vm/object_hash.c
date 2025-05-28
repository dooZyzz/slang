#include "vm/object.h"
#include "vm/vm.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/**
 * Hash table-based object implementation for better performance.
 * This implementation uses open addressing with linear probing.
 */

#define INITIAL_CAPACITY 8
#define MAX_LOAD_FACTOR 0.75

typedef struct {
    char* key;
    TaggedValue value;
    bool occupied;
    bool deleted;
} HashEntry;

typedef struct {
    HashEntry* entries;
    size_t capacity;
    size_t count;
    size_t tombstones;
} HashTable;

// FNV-1a hash function
static uint32_t hash_string(const char* str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    return hash;
}

static HashTable* hash_table_create(size_t initial_capacity) {
    HashTable* table = malloc(sizeof(HashTable));
    table->capacity = initial_capacity;
    table->count = 0;
    table->tombstones = 0;
    table->entries = calloc(initial_capacity, sizeof(HashEntry));
    return table;
}

static void hash_table_destroy(HashTable* table) {
    if (!table) return;
    
    for (size_t i = 0; i < table->capacity; i++) {
        if (table->entries[i].occupied && !table->entries[i].deleted) {
            free(table->entries[i].key);
        }
    }
    free(table->entries);
    free(table);
}

static HashEntry* find_entry(HashEntry* entries, size_t capacity, const char* key) {
    uint32_t hash = hash_string(key);
    size_t index = hash % capacity;
    HashEntry* tombstone = NULL;
    
    for (;;) {
        HashEntry* entry = &entries[index];
        
        if (!entry->occupied) {
            // Empty entry - key not found
            return tombstone != NULL ? tombstone : entry;
        }
        
        if (entry->deleted) {
            // Remember first tombstone
            if (tombstone == NULL) tombstone = entry;
        } else if (strcmp(entry->key, key) == 0) {
            // Found the key
            return entry;
        }
        
        // Linear probing
        index = (index + 1) % capacity;
    }
}

static void hash_table_resize(HashTable* table) {
    size_t new_capacity = table->capacity * 2;
    HashEntry* new_entries = calloc(new_capacity, sizeof(HashEntry));
    
    // Rehash all entries
    table->count = 0;
    table->tombstones = 0;
    
    for (size_t i = 0; i < table->capacity; i++) {
        HashEntry* entry = &table->entries[i];
        if (entry->occupied && !entry->deleted) {
            HashEntry* dest = find_entry(new_entries, new_capacity, entry->key);
            dest->key = entry->key;
            dest->value = entry->value;
            dest->occupied = true;
            dest->deleted = false;
            table->count++;
        }
    }
    
    free(table->entries);
    table->entries = new_entries;
    table->capacity = new_capacity;
}

static bool hash_table_set(HashTable* table, const char* key, TaggedValue value) {
    // Check if we need to resize
    if (table->count + table->tombstones + 1 > table->capacity * MAX_LOAD_FACTOR) {
        hash_table_resize(table);
    }
    
    HashEntry* entry = find_entry(table->entries, table->capacity, key);
    bool is_new_key = !entry->occupied || entry->deleted;
    
    if (is_new_key) {
        if (entry->deleted) {
            table->tombstones--;
        }
        table->count++;
        entry->key = strdup(key);
    }
    
    entry->value = value;
    entry->occupied = true;
    entry->deleted = false;
    
    return is_new_key;
}

static TaggedValue* hash_table_get(HashTable* table, const char* key) {
    if (table->count == 0) return NULL;
    
    HashEntry* entry = find_entry(table->entries, table->capacity, key);
    if (!entry->occupied || entry->deleted) {
        return NULL;
    }
    
    return &entry->value;
}

static bool hash_table_delete(HashTable* table, const char* key) {
    if (table->count == 0) return false;
    
    HashEntry* entry = find_entry(table->entries, table->capacity, key);
    if (!entry->occupied || entry->deleted) {
        return false;
    }
    
    // Mark as deleted (tombstone)
    free(entry->key);
    entry->deleted = true;
    table->tombstones++;
    table->count--;
    
    return true;
}

// Enhanced object structure with hash table
typedef struct ObjectHash {
    HashTable* properties;
    Object* prototype;
    bool is_array;
} ObjectHash;

// Create optimized object
Object* object_create_optimized(void) {
    ObjectHash* obj = malloc(sizeof(ObjectHash));
    obj->properties = hash_table_create(INITIAL_CAPACITY);
    obj->prototype = NULL;
    obj->is_array = false;
    return (Object*)obj;
}

// Get property with hash table
TaggedValue* object_get_property_optimized(Object* obj, const char* key) {
    if (!obj || !key) return NULL;
    
    ObjectHash* hash_obj = (ObjectHash*)obj;
    TaggedValue* value = hash_table_get(hash_obj->properties, key);
    
    if (value) {
        return value;
    }
    
    // Check prototype chain
    if (hash_obj->prototype) {
        return object_get_property_optimized(hash_obj->prototype, key);
    }
    
    return NULL;
}

// Set property with hash table
void object_set_property_optimized(Object* obj, const char* key, TaggedValue value) {
    if (!obj || !key) return;
    
    ObjectHash* hash_obj = (ObjectHash*)obj;
    hash_table_set(hash_obj->properties, key, value);
}

// Module export optimization: pre-sized hash table
Object* create_module_export_object(size_t expected_exports) {
    ObjectHash* obj = malloc(sizeof(ObjectHash));
    // Pre-size the hash table to avoid resizing
    size_t initial_size = 8;
    while (initial_size < expected_exports / MAX_LOAD_FACTOR) {
        initial_size *= 2;
    }
    obj->properties = hash_table_create(initial_size);
    obj->prototype = NULL;
    obj->is_array = false;
    return (Object*)obj;
}

// Batch export optimization
void object_set_properties_batch(Object* obj, const char** keys, TaggedValue* values, size_t count) {
    if (!obj || !keys || !values || count == 0) return;
    
    ObjectHash* hash_obj = (ObjectHash*)obj;
    
    // Pre-resize if necessary
    size_t required_capacity = (hash_obj->properties->count + count) / MAX_LOAD_FACTOR;
    while (hash_obj->properties->capacity < required_capacity) {
        hash_table_resize(hash_obj->properties);
    }
    
    // Batch insert
    for (size_t i = 0; i < count; i++) {
        hash_table_set(hash_obj->properties, keys[i], values[i]);
    }
}

// Property iteration for export/import all
typedef void (*PropertyIterator)(const char* key, TaggedValue* value, void* user_data);

void object_iterate_properties(Object* obj, PropertyIterator iterator, void* user_data) {
    if (!obj || !iterator) return;
    
    ObjectHash* hash_obj = (ObjectHash*)obj;
    HashTable* table = hash_obj->properties;
    
    for (size_t i = 0; i < table->capacity; i++) {
        HashEntry* entry = &table->entries[i];
        if (entry->occupied && !entry->deleted) {
            iterator(entry->key, &entry->value, user_data);
        }
    }
}