#ifndef OBJECT_SIZES_H
#define OBJECT_SIZES_H

#include <stddef.h>

// Common object sizes for memory allocation

// Core runtime objects
#define SIZE_FUNCTION           sizeof(Function)
#define SIZE_CLOSURE            sizeof(Closure)
#define SIZE_UPVALUE            sizeof(Upvalue)
#define SIZE_STRUCT_TYPE        sizeof(StructType)
#define SIZE_STRUCT_INSTANCE    sizeof(StructInstance)

// Object system
#define SIZE_OBJECT             sizeof(Object)
#define SIZE_OBJECT_PROPERTY    sizeof(ObjectProperty)
#define SIZE_ARRAY              sizeof(Array)

// String sizes
#define SIZE_STRING(len)        ((len) + 1)  // Include null terminator

// Dynamic array sizes
#define SIZE_PTR_ARRAY(count)   ((count) * sizeof(void*))
#define SIZE_STR_ARRAY(count)   ((count) * sizeof(char*))
#define SIZE_VAL_ARRAY(count)   ((count) * sizeof(TaggedValue))

// Chunk components
#define SIZE_BYTECODE(count)    ((count) * sizeof(uint8_t))
#define SIZE_LINES(count)       ((count) * sizeof(int))

// Helper for field arrays
#define SIZE_FIELD_ARRAY(count) ((count) * sizeof(char*))

#endif // OBJECT_SIZES_H