# Memory Allocator Integration Example

This document shows a complete example of refactoring existing code to use the SwiftLang memory allocator system.

## Original Code (hash_map.c)

```c
// Original implementation using standard malloc/free
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

HashMap* hash_map_create(void) {
    HashMap* map = calloc(1, sizeof(HashMap));
    if (!map) return NULL;
    
    map->capacity = INITIAL_CAPACITY;
    map->buckets = calloc(map->capacity, sizeof(HashMapEntry*));
    
    return map;
}

void hash_map_put(HashMap* map, const char* key, void* value) {
    // ... 
    HashMapEntry* entry = malloc(sizeof(HashMapEntry));
    entry->key = strdup(key);
    entry->value = value;
    // ...
}

void hash_map_destroy(HashMap* map) {
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
```

## Refactored Code (hash_map_v2.c)

```c
// Refactored implementation using allocators
#include "utils/memory.h"
#include "utils/alloc.h"

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

// Create with specific allocator
HashMap* hash_map_create_with_allocator(Allocator* allocator) {
    if (!allocator) {
        allocator = get_allocator();  // Use default if none provided
    }
    
    HashMap* map = MEM_NEW(allocator, HashMap);
    if (!map) return NULL;
    
    map->allocator = allocator;
    map->capacity = INITIAL_CAPACITY;
    map->buckets = MEM_NEW_ARRAY(allocator, HashMapEntry*, map->capacity);
    if (!map->buckets) {
        MEM_FREE(allocator, map, sizeof(HashMap));
        return NULL;
    }
    
    return map;
}

// Convenience function using default allocator
HashMap* hash_map_create(void) {
    return hash_map_create_with_allocator(get_allocator());
}

void hash_map_put(HashMap* map, const char* key, void* value) {
    // ... 
    HashMapEntry* entry = MEM_NEW(map->allocator, HashMapEntry);
    if (!entry) return;
    
    entry->key = MEM_STRDUP(map->allocator, key);
    if (!entry->key) {
        MEM_FREE(map->allocator, entry, sizeof(HashMapEntry));
        return;
    }
    
    entry->value = value;
    // ...
}

void hash_map_destroy(HashMap* map) {
    if (!map) return;
    
    Allocator* alloc = map->allocator;
    
    // Free all entries
    for (size_t i = 0; i < map->capacity; i++) {
        HashMapEntry* entry = map->buckets[i];
        while (entry) {
            HashMapEntry* next = entry->next;
            MEM_FREE(alloc, entry->key, strlen(entry->key) + 1);
            MEM_FREE(alloc, entry, sizeof(HashMapEntry));
            entry = next;
        }
    }
    
    MEM_FREE(alloc, map->buckets, sizeof(HashMapEntry*) * map->capacity);
    MEM_FREE(alloc, map, sizeof(HashMap));
}
```

## Key Changes

### 1. Store Allocator Reference
```c
struct HashMap {
    // ... existing fields ...
    Allocator* allocator;  // Added
};
```

### 2. Accept Allocator in Constructor
```c
HashMap* hash_map_create_with_allocator(Allocator* allocator) {
    // Use provided allocator or default
}
```

### 3. Replace Allocation Calls
| Original | Refactored |
|----------|------------|
| `malloc(size)` | `MEM_ALLOC(allocator, size)` |
| `calloc(1, size)` | `MEM_ALLOC_ZERO(allocator, size)` |
| `calloc(n, size)` | `MEM_NEW_ARRAY(allocator, type, n)` |
| `realloc(ptr, new_size)` | `MEM_REALLOC(allocator, ptr, old_size, new_size)` |
| `strdup(str)` | `MEM_STRDUP(allocator, str)` |
| `free(ptr)` | `MEM_FREE(allocator, ptr, size)` |

### 4. Track Sizes for Deallocation
```c
// Must track size for proper deallocation
MEM_FREE(alloc, entry->key, strlen(entry->key) + 1);  // String size
MEM_FREE(alloc, entry, sizeof(HashMapEntry));         // Struct size
MEM_FREE(alloc, map->buckets, sizeof(HashMapEntry*) * map->capacity);  // Array size
```

### 5. Use Type-Safe Macros
```c
// Instead of:
HashMap* map = calloc(1, sizeof(HashMap));

// Use:
HashMap* map = MEM_NEW(allocator, HashMap);
```

## Usage Examples

### Default Allocator
```c
// Uses global default allocator
HashMap* map = hash_map_create();
hash_map_put(map, "key", value);
hash_map_destroy(map);
```

### Custom Allocator
```c
// Use arena for temporary hash map
Allocator* arena = mem_create_arena_allocator(64 * 1024);
HashMap* temp_map = hash_map_create_with_allocator(arena);

// Use the map...
hash_map_put(temp_map, "temp", data);

// Destroy arena - frees everything at once
mem_destroy(arena);
```

### With Tracing
```c
// Enable tracing for debugging
Allocator* platform = mem_create_platform_allocator();
Allocator* trace = mem_create_trace_allocator(platform);

HashMap* map = hash_map_create_with_allocator(trace);
// Use map...

// Check for leaks
mem_check_leaks(trace);
char* stats = mem_format_stats(trace);
printf("%s\n", stats);
free(stats);

hash_map_destroy(map);
mem_destroy(trace);
mem_destroy(platform);
```

### In Different Contexts
```c
// VM context - long-lived
HashMap* globals = hash_map_create_with_allocator(
    allocators_get(ALLOC_SYSTEM_VM)
);

// Compiler context - temporary
HashMap* symbols = hash_map_create_with_allocator(
    allocators_get(ALLOC_SYSTEM_COMPILER)
);

// Module context - persistent
HashMap* exports = hash_map_create_with_allocator(
    allocators_get(ALLOC_SYSTEM_MODULES)
);
```

## Benefits

1. **Flexibility**: Choose allocator based on usage pattern
2. **Performance**: Arena allocators for temporary maps
3. **Debugging**: Trace allocator finds leaks
4. **Isolation**: Different subsystems use different allocators
5. **Profiling**: Track memory usage per subsystem

## Common Patterns

### Temporary Data Structure
```c
// Use arena for data that has clear lifetime
WITH_ARENA(temp, 8192, {
    HashMap* map = hash_map_create_with_allocator(temp);
    // Use map for computation
    // No need to call hash_map_destroy - arena cleanup handles it
});
```

### Long-Lived with Leak Detection
```c
#ifdef DEBUG
    Allocator* base = mem_create_platform_allocator();
    Allocator* alloc = mem_create_trace_allocator(base);
#else
    Allocator* alloc = mem_create_platform_allocator();
#endif

HashMap* persistent_map = hash_map_create_with_allocator(alloc);
```

### Pool of Same-Sized Objects
```c
// If hash map entries are similar size
Allocator* pool = mem_create_freelist_allocator(
    sizeof(HashMapEntry) + 32,  // Entry + average key size
    100  // Initial pool size
);
HashMap* pooled_map = hash_map_create_with_allocator(pool);
```

## Gotchas and Tips

1. **Always track sizes**: Required for MEM_FREE
2. **Consistent allocator**: Use same allocator for related data
3. **String lifetime**: Be careful with string ownership
4. **Arena limitations**: Can't free individual items
5. **Realloc patterns**: Need both old and new size

## Testing

```c
void test_hash_map_with_allocators(void) {
    // Test with different allocator types
    Allocator* allocators[] = {
        mem_create_platform_allocator(),
        mem_create_arena_allocator(8192),
        mem_create_trace_allocator(mem_create_platform_allocator())
    };
    
    for (int i = 0; i < 3; i++) {
        HashMap* map = hash_map_create_with_allocator(allocators[i]);
        
        // Run tests...
        hash_map_put(map, "test", "value");
        assert(hash_map_get(map, "test") != NULL);
        
        // Cleanup (except arena)
        if (allocators[i]->type != ALLOCATOR_ARENA) {
            hash_map_destroy(map);
        }
        
        mem_destroy(allocators[i]);
    }
}
```