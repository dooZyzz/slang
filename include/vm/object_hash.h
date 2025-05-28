#ifndef OBJECT_HASH_H
#define OBJECT_HASH_H

#include "vm/object.h"
#include "vm/vm.h"

/**
 * Hash table-based object implementation for better performance.
 * These functions provide O(1) average case property access compared
 * to the O(n) linked list implementation.
 */

// Create an optimized object with hash table storage
Object* object_create_optimized(void);

// Optimized property access
TaggedValue* object_get_property_optimized(Object* obj, const char* key);
void object_set_property_optimized(Object* obj, const char* key, TaggedValue value);

// Module-specific optimizations

/**
 * Create a module export object with pre-sized hash table.
 * This avoids resizing during module loading.
 * 
 * @param expected_exports Number of exports the module will have
 * @return Optimized object for module exports
 */
Object* create_module_export_object(size_t expected_exports);

/**
 * Batch set multiple properties at once.
 * More efficient than setting properties one by one.
 * 
 * @param obj The object to set properties on
 * @param keys Array of property names
 * @param values Array of property values
 * @param count Number of properties to set
 */
void object_set_properties_batch(Object* obj, const char** keys, TaggedValue* values, size_t count);

/**
 * Iterator callback for property iteration.
 * 
 * @param key Property name
 * @param value Property value
 * @param user_data User-provided context
 */
typedef void (*PropertyIterator)(const char* key, TaggedValue* value, void* user_data);

/**
 * Iterate over all properties of an object.
 * Useful for import * operations.
 * 
 * @param obj The object to iterate
 * @param iterator Callback function for each property
 * @param user_data Context passed to the iterator
 */
void object_iterate_properties(Object* obj, PropertyIterator iterator, void* user_data);

#endif // OBJECT_HASH_H