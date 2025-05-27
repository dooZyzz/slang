// array.c
#include <stdlib.h>
#include <string.h>
#include "vm/array.h"
#include "vm/vm.h"

Array* old_array_create(size_t initial_capacity) {
    Array* array = malloc(sizeof(Array));
    array->count = 0;
    array->capacity = initial_capacity;
    array->elements = malloc(sizeof(TaggedValue) * array->capacity);
    return array;
}

void old_array_free(Array* array) {
    if (array == NULL) return;
    free(array->elements);
    free(array);
}

void old_array_push(Array* array, TaggedValue value) {
    if (array->count >= array->capacity) {
        array->capacity = array->capacity < 8 ? 8 : array->capacity * 2;
        array->elements = realloc(array->elements, sizeof(TaggedValue) * array->capacity);
    }
    array->elements[array->count++] = value;
}

TaggedValue old_array_get(Array* array, size_t index) {
    if (index >= array->count) {
        return (TaggedValue){.type = VAL_NIL};
    }
    return array->elements[index];
}

void old_array_set(Array* array, size_t index, TaggedValue value) {
    if (index >= array->count) {
        // Grow array if needed
        if (index >= array->capacity) {
            size_t new_capacity = index + 1;
            if (new_capacity < array->capacity * 2) {
                new_capacity = array->capacity * 2;
            }
            array->elements = realloc(array->elements, sizeof(TaggedValue) * new_capacity);
            array->capacity = new_capacity;
        }
        // Fill with nil
        for (size_t i = array->count; i < index; i++) {
            array->elements[i] = (TaggedValue){.type = VAL_NIL};
        }
        array->count = index + 1;
    }
    array->elements[index] = value;
}
