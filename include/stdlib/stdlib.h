#ifndef STDLIB_H
#define STDLIB_H

#include "runtime/core/vm.h"
#include "runtime/core/object.h"

// Stdlib modules would be included here when added to build
// #include "stdlib/file.h"
// #include "stdlib/json.h" 
// #include "stdlib/utils.h"

// Stdlib initialization - sets up all prototype methods and global functions
void stdlib_init(VM* vm);

// Initialize individual type prototypes
void stdlib_init_object_prototype(Object* proto);
void stdlib_init_array_prototype(Object* proto);
void stdlib_init_string_prototype(Object* proto);
void stdlib_init_function_prototype(Object* proto);

// Array methods
TaggedValue array_push_method(int arg_count, TaggedValue* args);
TaggedValue array_pop_method(int arg_count, TaggedValue* args);
TaggedValue array_length_method(int arg_count, TaggedValue* args);
TaggedValue array_map_method(int arg_count, TaggedValue* args);
TaggedValue array_filter_method(int arg_count, TaggedValue* args);
TaggedValue array_reduce_method(int arg_count, TaggedValue* args);
TaggedValue array_count_method(int arg_count, TaggedValue* args);
TaggedValue array_isEmpty_method(int arg_count, TaggedValue* args);

// Set the current VM for stdlib functions that need it
void stdlib_set_vm(VM* vm);

// String methods
TaggedValue string_length_method(int arg_count, TaggedValue* args);
TaggedValue string_charAt_method(int arg_count, TaggedValue* args);
TaggedValue string_indexOf_method(int arg_count, TaggedValue* args);
TaggedValue string_substring_method(int arg_count, TaggedValue* args);
TaggedValue string_toUpperCase_method(int arg_count, TaggedValue* args);
TaggedValue string_toLowerCase_method(int arg_count, TaggedValue* args);
TaggedValue string_split_method(int arg_count, TaggedValue* args);
TaggedValue string_trim_method(int arg_count, TaggedValue* args);

#endif // STDLIB_H