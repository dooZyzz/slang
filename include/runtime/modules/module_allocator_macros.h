#ifndef MODULE_ALLOCATOR_MACROS_H
#define MODULE_ALLOCATOR_MACROS_H

#include "utils/allocators.h"

// Module allocator macros
#define MODULES_ALLOC(size) MODULE_ALLOC(size, "module")
#define MODULES_FREE(ptr, size) MODULE_FREE(ptr, size)
#define MODULES_NEW(type) MODULE_NEW(type, "module")
#define MODULES_NEW_ZERO(type) ((type*)MODULE_ALLOC_ZERO(sizeof(type), "module"))
#define MODULES_NEW_ARRAY(type, count) ((type*)MODULE_ALLOC(sizeof(type) * (count), "module-array"))
#define MODULES_NEW_ARRAY_ZERO(type, count) ((type*)MODULE_ALLOC_ZERO(sizeof(type) * (count), "module-array"))
#define STRINGS_STRDUP(str) MODULE_DUP(str)
#define STRINGS_ALLOC(size) STR_ALLOC(size)
#define STRINGS_FREE(ptr, size) STR_FREE(ptr, size)

// Bytecode allocator macros
#define BYTECODE_NEW(type) MEM_NEW(allocators_get(ALLOC_SYSTEM_BYTECODE), type)
#define BYTECODE_FREE(ptr, size) SLANG_MEM_FREE(allocators_get(ALLOC_SYSTEM_BYTECODE), ptr, size)

#endif // MODULE_ALLOCATOR_MACROS_H