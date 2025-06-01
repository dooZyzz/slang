// array.h
#ifndef ARRAY_H
#define ARRAY_H

#include "runtime/core/vm.h"

typedef struct {
    TaggedValue* elements;
    size_t count;
    size_t capacity;
} Array;

// Create a new array with the specified initial capacity
Array* old_array_create(size_t initial_capacity);

// Free an array and its elements
void old_array_free(Array* array);

// Add an element to the end of the array
void old_array_push(Array* array, TaggedValue value);

// Get an element at the specified index (returns NIL if out of bounds)
TaggedValue old_array_get(Array* array, size_t index);

// Set an element at the specified index (grows array if needed)
void old_array_set(Array* array, size_t index, TaggedValue value);

// Helper macro to check if a TaggedValue is an array
#define IS_ARRAY(value) ((value).type == VAL_OBJECT && (value).as.object != NULL && ((Object*)(value).as.object)->is_array)

// Helper macro to cast object to array
#define AS_ARRAY(value) ((Object*)(value).as.object)

#endif // ARRAY_H
