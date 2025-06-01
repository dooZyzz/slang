#ifndef STDLIB_UTILS_H
#define STDLIB_UTILS_H

#include "runtime/core/vm.h"

// System functions
TaggedValue sys_exit(int arg_count, TaggedValue* args);
TaggedValue sys_getenv(int arg_count, TaggedValue* args);
TaggedValue sys_setenv(int arg_count, TaggedValue* args);
TaggedValue sys_time(int arg_count, TaggedValue* args);
TaggedValue sys_sleep(int arg_count, TaggedValue* args);

// Type checking functions
TaggedValue type_of(int arg_count, TaggedValue* args);
TaggedValue is_string(int arg_count, TaggedValue* args);
TaggedValue is_number(int arg_count, TaggedValue* args);
TaggedValue is_bool(int arg_count, TaggedValue* args);
TaggedValue is_nil(int arg_count, TaggedValue* args);
TaggedValue is_array(int arg_count, TaggedValue* args);
TaggedValue is_object(int arg_count, TaggedValue* args);
TaggedValue is_function(int arg_count, TaggedValue* args);

// Conversion functions
TaggedValue to_string(int arg_count, TaggedValue* args);
TaggedValue to_number(int arg_count, TaggedValue* args);
TaggedValue to_int(int arg_count, TaggedValue* args);

// Math extensions
TaggedValue math_random(int arg_count, TaggedValue* args);
TaggedValue math_randomInt(int arg_count, TaggedValue* args);

#endif // STDLIB_UTILS_H