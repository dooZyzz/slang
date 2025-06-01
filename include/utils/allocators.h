#ifndef ALLOCATORS_H
#define ALLOCATORS_H

#include "utils/memory.h"

// Allocator subsystems
typedef enum {
    ALLOC_SYSTEM_VM,          // VM core structures
    ALLOC_SYSTEM_OBJECTS,     // Runtime objects
    ALLOC_SYSTEM_STRINGS,     // String pool
    ALLOC_SYSTEM_BYTECODE,    // Bytecode chunks
    ALLOC_SYSTEM_COMPILER,    // Compiler structures
    ALLOC_SYSTEM_AST,         // AST nodes
    ALLOC_SYSTEM_PARSER,      // Parser temporary data
    ALLOC_SYSTEM_SYMBOLS,     // Symbol tables
    ALLOC_SYSTEM_MODULES,     // Module system
    ALLOC_SYSTEM_STDLIB,      // Standard library
    ALLOC_SYSTEM_TEMP,        // Temporary allocations
    ALLOC_SYSTEM_COUNT
} AllocatorSystem;

// Allocator configuration
typedef struct {
    bool enable_trace;        // Enable trace allocator for debugging
    bool enable_stats;        // Enable statistics collection
    size_t arena_size;        // Default arena size
    size_t object_pool_size;  // Object pool block size
} AllocatorConfig;

// Global allocator management
void allocators_init(const AllocatorConfig* config);
void allocators_shutdown(void);
Allocator* allocators_get(AllocatorSystem system);
void allocators_print_stats(void);
void allocators_check_leaks(void);

// Convenience macros for each subsystem
#define VM_ALLOC(size) MEM_ALLOC_TAGGED(allocators_get(ALLOC_SYSTEM_VM), size, "vm-core")
#define VM_ALLOC_ZERO(size) MEM_ALLOC_ZERO_TAGGED(allocators_get(ALLOC_SYSTEM_VM), size, "vm-core")
#define VM_REALLOC(ptr, old_size, new_size) MEM_REALLOC(allocators_get(ALLOC_SYSTEM_VM), ptr, old_size, new_size)
#define VM_FREE(ptr, size) MEM_FREE(allocators_get(ALLOC_SYSTEM_VM), ptr, size)
#define VM_NEW(type) MEM_NEW(allocators_get(ALLOC_SYSTEM_VM), type)
#define VM_NEW_ARRAY(type, count) MEM_NEW_ARRAY(allocators_get(ALLOC_SYSTEM_VM), type, count)

#define OBJ_ALLOC(size, tag) MEM_ALLOC_TAGGED(allocators_get(ALLOC_SYSTEM_OBJECTS), size, tag)
#define OBJ_ALLOC_ZERO(size, tag) MEM_ALLOC_ZERO_TAGGED(allocators_get(ALLOC_SYSTEM_OBJECTS), size, tag)
#define OBJ_FREE(ptr, size) MEM_FREE(allocators_get(ALLOC_SYSTEM_OBJECTS), ptr, size)
#define OBJ_NEW(type, tag) MEM_NEW_TAGGED(allocators_get(ALLOC_SYSTEM_OBJECTS), type, tag)

#define STR_ALLOC(size) MEM_ALLOC_TAGGED(allocators_get(ALLOC_SYSTEM_STRINGS), size, "string")
#define STR_DUP(str) MEM_STRDUP(allocators_get(ALLOC_SYSTEM_STRINGS), str)
#define STR_FREE(ptr, size) MEM_FREE(allocators_get(ALLOC_SYSTEM_STRINGS), ptr, size)

#define AST_ALLOC(size) MEM_ALLOC_TAGGED(allocators_get(ALLOC_SYSTEM_AST), size, "ast")
#define AST_ALLOC_ZERO(size) MEM_ALLOC_ZERO_TAGGED(allocators_get(ALLOC_SYSTEM_AST), size, "ast")
#define AST_NEW(type) MEM_NEW(allocators_get(ALLOC_SYSTEM_AST), type)
#define AST_NEW_ARRAY(type, count) MEM_NEW_ARRAY(allocators_get(ALLOC_SYSTEM_AST), type, count)
#define AST_DUP(str) MEM_STRDUP(allocators_get(ALLOC_SYSTEM_AST), str)

#define COMPILER_ALLOC(size) MEM_ALLOC(allocators_get(ALLOC_SYSTEM_COMPILER), size)
#define COMPILER_ALLOC_ZERO(size) MEM_ALLOC_ZERO(allocators_get(ALLOC_SYSTEM_COMPILER), size)
#define COMPILER_REALLOC(ptr, old_size, new_size) MEM_REALLOC(allocators_get(ALLOC_SYSTEM_COMPILER), ptr, old_size, new_size)
#define COMPILER_FREE(ptr, size) MEM_FREE(allocators_get(ALLOC_SYSTEM_COMPILER), ptr, size)
#define COMPILER_NEW(type) MEM_NEW(allocators_get(ALLOC_SYSTEM_COMPILER), type)

#define MODULE_ALLOC(size, tag) MEM_ALLOC_TAGGED(allocators_get(ALLOC_SYSTEM_MODULES), size, tag)
#define MODULE_ALLOC_ZERO(size, tag) MEM_ALLOC_ZERO_TAGGED(allocators_get(ALLOC_SYSTEM_MODULES), size, tag)
#define MODULE_FREE(ptr, size) MEM_FREE(allocators_get(ALLOC_SYSTEM_MODULES), ptr, size)
#define MODULE_DUP(str) MEM_STRDUP(allocators_get(ALLOC_SYSTEM_MODULES), str)
#define MODULE_NEW(type, tag) MEM_NEW_TAGGED(allocators_get(ALLOC_SYSTEM_MODULES), type, tag)

// Reset arena allocators (for AST, parser temp data, etc.)
void allocators_reset_ast(void);
void allocators_reset_temp(void);
void allocators_reset_compiler(void);

#endif // ALLOCATORS_H