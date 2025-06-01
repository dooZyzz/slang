#ifndef STDLIB_JSON_H
#define STDLIB_JSON_H

#include "runtime/core/vm.h"

// JSON functions
TaggedValue json_parse(int arg_count, TaggedValue* args);
TaggedValue json_stringify(int arg_count, TaggedValue* args);

#endif // STDLIB_JSON_H