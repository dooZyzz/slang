#ifndef HASH_MAP_H
#define HASH_MAP_H

#include <stdbool.h>
#include <stddef.h>

typedef struct HashMap HashMap;

// Create and destroy
HashMap* hash_map_create(void);
void hash_map_destroy(HashMap* map);

// Operations
void hash_map_put(HashMap* map, const char* key, void* value);
void* hash_map_get(HashMap* map, const char* key);
bool hash_map_contains(HashMap* map, const char* key);
void hash_map_remove(HashMap* map, const char* key);
void hash_map_clear(HashMap* map);

// Iteration
typedef void (*HashMapIterator)(const char* key, void* value, void* user_data);
void hash_map_iterate(HashMap* map, HashMapIterator iterator, void* user_data);

// Properties
size_t hash_map_size(HashMap* map);
bool hash_map_is_empty(HashMap* map);

#endif // HASH_MAP_H