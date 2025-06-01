#include "runtime/core/vm.h"
#include "utils/allocators.h"
#include "debug/debug.h"
#include "runtime/modules/loader/module_loader.h"
#include "runtime/modules/lifecycle/builtin_modules.h"
#include "runtime/core/bootstrap.h"
#include "stdlib/stdlib.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "runtime/core/array.h"

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

// Global print hook for output redirection
static PrintHook g_print_hook = NULL;

// Forward declarations
static bool is_falsey(TaggedValue value);

void chunk_init(Chunk* chunk)
{
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->constants.count = 0;
    chunk->constants.capacity = 0;
    chunk->constants.values = NULL;
}

void chunk_free(Chunk* chunk)
{
    if (chunk->code) {
        COMPILER_FREE(chunk->code, chunk->capacity);
    }
    if (chunk->lines) {
        COMPILER_FREE(chunk->lines, chunk->capacity * sizeof(int));
    }

    // Free strings in constants
    for (size_t i = 0; i < chunk->constants.count; i++) {
        // Strings are managed by string pool, no need to free
    }
    
    if (chunk->constants.values) {
        COMPILER_FREE(chunk->constants.values, chunk->constants.capacity * sizeof(TaggedValue));
    }

    chunk_init(chunk);
}

void chunk_write(Chunk* chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1)
    {
        size_t old_capacity = chunk->capacity;
        chunk->capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        
        chunk->code = COMPILER_REALLOC(chunk->code, old_capacity, chunk->capacity);
        chunk->lines = COMPILER_REALLOC(chunk->lines, 
                                       old_capacity * sizeof(int), 
                                       chunk->capacity * sizeof(int));
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int chunk_add_constant(Chunk* chunk, TaggedValue value)
{
    // Grow constants array if needed
    if (chunk->constants.count >= chunk->constants.capacity)
    {
        size_t old_capacity = chunk->constants.capacity;
        chunk->constants.capacity = GROW_CAPACITY(old_capacity);
        
        chunk->constants.values = COMPILER_REALLOC(chunk->constants.values,
                                                  old_capacity * sizeof(TaggedValue),
                                                  chunk->constants.capacity * sizeof(TaggedValue));
    }

    chunk->constants.values[chunk->constants.count] = value;
    return chunk->constants.count++;
}

// Forward declaration
void define_global(VM* vm, const char* name, TaggedValue value);

void vm_init(VM* vm)
{
    vm->stack_top = vm->stack;
    vm->frame_count = 0;
    vm->globals.count = 0;
    vm->globals.capacity = 0;
    vm->globals.names = NULL;
    vm->globals.values = NULL;
    vm->struct_types.count = 0;
    vm->struct_types.capacity = 0;
    vm->struct_types.names = NULL;
    vm->struct_types.types = NULL;
    vm->open_upvalues = NULL;
    string_pool_init(&vm->strings);
    vm->module_loader = NULL;
    vm->current_module_path = NULL;
    vm->current_module = NULL;
    
    // Initialize standard library
    stdlib_init(vm);
}

void vm_init_with_loader(VM* vm, ModuleLoader* loader) {
    vm_init(vm);
    vm->module_loader = loader;
}

void vm_free(VM* vm)
{
    // Free globals
    for (size_t i = 0; i < vm->globals.count; i++) {
        if (vm->globals.names[i]) {
            VM_FREE(vm->globals.names[i], strlen(vm->globals.names[i]) + 1);
        }
    }
    if (vm->globals.names) {
        VM_FREE(vm->globals.names, vm->globals.capacity * sizeof(char*));
    }
    if (vm->globals.values) {
        VM_FREE(vm->globals.values, vm->globals.capacity * sizeof(TaggedValue));
    }
    
    // Free struct types
    for (size_t i = 0; i < vm->struct_types.count; i++) {
        if (vm->struct_types.names[i]) {
            VM_FREE(vm->struct_types.names[i], strlen(vm->struct_types.names[i]) + 1);
        }
        if (vm->struct_types.types[i]) {
            // Free struct type fields
            StructType* type = vm->struct_types.types[i];
            for (size_t j = 0; j < type->field_count; j++) {
                if (type->fields[j]) {
                    VM_FREE(type->fields[j], strlen(type->fields[j]) + 1);
                }
            }
            if (type->fields) {
                VM_FREE(type->fields, type->field_count * sizeof(char*));
            }
            VM_FREE(type, sizeof(StructType));
        }
    }
    if (vm->struct_types.names) {
        VM_FREE(vm->struct_types.names, vm->struct_types.capacity * sizeof(char*));
    }
    if (vm->struct_types.types) {
        VM_FREE(vm->struct_types.types, vm->struct_types.capacity * sizeof(StructType*));
    }
    
    // Free open upvalues
    Upvalue* upvalue = vm->open_upvalues;
    while (upvalue != NULL) {
        Upvalue* next = upvalue->next;
        OBJ_FREE(upvalue, sizeof(Upvalue));
        upvalue = next;
    }
    
    string_pool_free(&vm->strings);
}

VM* vm_create(void)
{
    VM* vm = VM_NEW(VM);
    if (vm) {
        vm_init(vm);
    }
    return vm;
}

void vm_destroy(VM* vm)
{
    if (vm) {
        vm_free(vm);
        VM_FREE(vm, sizeof(VM));
    }
}

static void grow_globals(VM* vm)
{
    size_t old_capacity = vm->globals.capacity;
    vm->globals.capacity = GROW_CAPACITY(old_capacity);
    
    vm->globals.names = VM_REALLOC(vm->globals.names,
                                   old_capacity * sizeof(char*),
                                   vm->globals.capacity * sizeof(char*));
    
    vm->globals.values = VM_REALLOC(vm->globals.values,
                                    old_capacity * sizeof(TaggedValue),
                                    vm->globals.capacity * sizeof(TaggedValue));
    
    // Initialize new entries
    for (size_t i = old_capacity; i < vm->globals.capacity; i++) {
        vm->globals.names[i] = NULL;
        vm->globals.values[i] = NIL_VAL;
    }
}

static void grow_struct_types(VM* vm)
{
    size_t old_capacity = vm->struct_types.capacity;
    vm->struct_types.capacity = GROW_CAPACITY(old_capacity);
    
    vm->struct_types.names = VM_REALLOC(vm->struct_types.names,
                                        old_capacity * sizeof(char*),
                                        vm->struct_types.capacity * sizeof(char*));
    
    vm->struct_types.types = VM_REALLOC(vm->struct_types.types,
                                        old_capacity * sizeof(StructType*),
                                        vm->struct_types.capacity * sizeof(StructType*));
}

// Note: This is the refactored beginning of vm.c using allocators
// The rest of the vm.c file needs to be updated similarly