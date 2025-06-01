#include "runtime/core/vm.h"
#include "debug/debug.h"
#include "runtime/modules/loader/module_loader.h"
#include "runtime/modules/lifecycle/builtin_modules.h"
#include "runtime/core/bootstrap.h"
#include "stdlib/stdlib.h"
#include "utils/logger.h"
#include "utils/allocators.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "runtime/core/array.h"  // Still need macros, but using object system for arrays

// Simple object tracking hash table for GC purposes
typedef struct
{
    Object** objects;
    size_t count;
    size_t capacity;
} ObjectHash;

// Extend the VM struct inline
static ObjectHash g_vm_objects;
static bool g_vm_has_error;
static char g_vm_error_message[512];

// Helper macros to access extended fields
#define vm_objects(vm) g_vm_objects
#define vm_has_error(vm) g_vm_has_error
#define vm_error_message(vm) g_vm_error_message

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity) * 2)

// Bytecode allocator macros
#define BYTECODE_ALLOC(size) MEM_ALLOC_TAGGED(allocators_get(ALLOC_SYSTEM_BYTECODE), size, "bytecode")
#define BYTECODE_ALLOC_ZERO(size) MEM_ALLOC_ZERO_TAGGED(allocators_get(ALLOC_SYSTEM_BYTECODE), size, "bytecode")
#define BYTECODE_REALLOC(ptr, old_size, new_size) MEM_REALLOC(allocators_get(ALLOC_SYSTEM_BYTECODE), ptr, old_size, new_size)
#define BYTECODE_FREE(ptr, size) MEM_FREE(allocators_get(ALLOC_SYSTEM_BYTECODE), ptr, size)
#define BYTECODE_NEW(type) MEM_NEW(allocators_get(ALLOC_SYSTEM_BYTECODE), type)
#define BYTECODE_NEW_ARRAY(type, count) MEM_NEW_ARRAY(allocators_get(ALLOC_SYSTEM_BYTECODE), type, count)

// Global print hook for output redirection
static PrintHook g_print_hook = NULL;

// Forward declarations
static bool is_falsey(TaggedValue value);
static InterpretResult vm_interpret_function(VM* vm, Function* function);
// define_global is declared in vm.h

// Object tracking functions
static void object_hash_init(ObjectHash* hash)
{
    hash->objects = NULL;
    hash->count = 0;
    hash->capacity = 0;
}

static void object_hash_free(ObjectHash* hash)
{
    if (hash->objects)
    {
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
        MEM_FREE(alloc, hash->objects, hash->capacity * sizeof(Object*));
    }
    hash->objects = NULL;
    hash->count = 0;
    hash->capacity = 0;
}

// String creation helper
static TaggedValue create_string_value(const char* str)
{
    return STRING_VAL(STR_DUP(str));
}

// Built-in native functions
static TaggedValue native_print(int arg_count, TaggedValue* args)
{
    for (int i = 0; i < arg_count; i++)
    {
        if (i > 0) printf(" ");
        print_value(args[i]);
    }
    printf("\n");
    return NIL_VAL;
}

static TaggedValue native_typeof(int arg_count, TaggedValue* args)
{
    if (arg_count != 1)
    {
        return create_string_value("error: typeof expects 1 argument");
    }

    switch (args[0].type)
    {
    case VAL_BOOL: return create_string_value("bool");
    case VAL_NIL: return create_string_value("nil");
    case VAL_NUMBER: return create_string_value("number");
    case VAL_STRING: return create_string_value("string");
    case VAL_FUNCTION: return create_string_value("function");
    case VAL_NATIVE: return create_string_value("native");
    case VAL_CLOSURE: return create_string_value("closure");
    case VAL_OBJECT: return create_string_value("object");
    case VAL_STRUCT: return create_string_value("struct");
    default: return create_string_value("unknown");
    }
}

static TaggedValue native_assert(int arg_count, TaggedValue* args)
{
    if (arg_count < 1)
    {
        fprintf(stderr, "Assertion failed: assert() requires at least 1 argument\n");
        exit(1);
    }

    bool condition = !IS_NIL(args[0]) && (!IS_BOOL(args[0]) || AS_BOOL(args[0]));
    if (!condition)
    {
        if (arg_count > 1 && IS_STRING(args[1]))
        {
            fprintf(stderr, "Assertion failed: %s\n", AS_STRING(args[1]));
        }
        else
        {
            fprintf(stderr, "Assertion failed\n");
        }
        exit(1);
    }

    return NIL_VAL;
}

static void bootstrap_init(VM* vm)
{
    // Register built-in functions as globals
    define_global(vm, "print", NATIVE_VAL(native_print));
    define_global(vm, "typeof", NATIVE_VAL(native_typeof));
    define_global(vm, "assert", NATIVE_VAL(native_assert));
}

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
    // Free bytecode arrays
    if (chunk->code)
    {
        BYTECODE_FREE(chunk->code, chunk->capacity);
    }
    if (chunk->lines)
    {
        BYTECODE_FREE(chunk->lines, chunk->capacity * sizeof(int));
    }

    for (size_t i = 0; i < chunk->constants.count; i++)
    {
        // Free strings in constants
        // TODO: Proper memory management for different constant types
    }
    if (chunk->constants.values)
    {
        BYTECODE_FREE(chunk->constants.values, chunk->constants.capacity * sizeof(TaggedValue));
    }

    chunk_init(chunk);
}

void chunk_write(Chunk* chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1)
    {
        size_t old_capacity = chunk->capacity;
        chunk->capacity = old_capacity < 8 ? 8 : old_capacity * 2;

        // Realloc pattern: allocate-copy-free
        uint8_t* new_code = BYTECODE_ALLOC(chunk->capacity);
        if (chunk->code)
        {
            memcpy(new_code, chunk->code, old_capacity);
            BYTECODE_FREE(chunk->code, old_capacity);
        }
        chunk->code = new_code;

        int* new_lines = BYTECODE_ALLOC(chunk->capacity * sizeof(int));
        if (chunk->lines)
        {
            memcpy(new_lines, chunk->lines, old_capacity * sizeof(int));
            BYTECODE_FREE(chunk->lines, old_capacity * sizeof(int));
        }
        chunk->lines = new_lines;
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

        // Realloc pattern: allocate-copy-free
        TaggedValue* new_values = BYTECODE_ALLOC(sizeof(TaggedValue) * chunk->constants.capacity);
        if (chunk->constants.values)
        {
            memcpy(new_values, chunk->constants.values, sizeof(TaggedValue) * old_capacity);
            BYTECODE_FREE(chunk->constants.values, sizeof(TaggedValue) * old_capacity);
        }
        chunk->constants.values = new_values;
    }

    chunk->constants.values[chunk->constants.count] = value;
    return chunk->constants.count++;
}

// Array methods are now implemented via prototypes in object.c

// TODO: Implement array methods (map, filter, reduce) with object system
// These will be added to Array.prototype when closure support is complete

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
    vm->current_module = NULL;
    string_pool_init(&vm->strings);
    object_hash_init(&vm_objects(vm));
    bootstrap_init(vm); // Initialize built-in prototypes

    vm_has_error(vm) = false;
    vm_error_message(vm)[0] = '\0';
}

void set_print_hook(PrintHook hook)
{
    g_print_hook = hook;
}

// Compatibility wrapper for existing code
void vm_set_print_hook(PrintHook hook)
{
    set_print_hook(hook);
}

static void vm_print_internal(const char* message, const char* end, bool newline)
{
    if (g_print_hook)
    {
        g_print_hook(message);
    }
    else
    {
        if (end && end[0])
        {
            printf("%s%s", message, end);
        }
        else
        {
            printf("%s", message);
        }
        if (newline)
        {
            printf("\n");
        }
    }
}

void vm_free(VM* vm)
{
    // Free global names and values
    for (size_t i = 0; i < vm->globals.count; i++)
    {
        if (vm->globals.names[i])
        {
            STR_FREE(vm->globals.names[i], strlen(vm->globals.names[i]) + 1);
        }
    }
    if (vm->globals.names)
    {
        VM_FREE(vm->globals.names, vm->globals.capacity * sizeof(char*));
    }
    if (vm->globals.values)
    {
        VM_FREE(vm->globals.values, vm->globals.capacity * sizeof(TaggedValue));
    }

    // Free struct types
    for (size_t i = 0; i < vm->struct_types.count; i++)
    {
        if (vm->struct_types.names[i])
        {
            STR_FREE(vm->struct_types.names[i], strlen(vm->struct_types.names[i]) + 1);
        }
    }
    if (vm->struct_types.names)
    {
        VM_FREE(vm->struct_types.names, vm->struct_types.capacity * sizeof(char*));
    }
    if (vm->struct_types.types)
    {
        VM_FREE(vm->struct_types.types, vm->struct_types.capacity * sizeof(StructType*));
    }

    string_pool_free(&vm->strings);
    object_hash_free(&vm_objects(vm));
}

// Initialize a new VM instance
VM* vm_create()
{
    VM* vm = VM_NEW(VM);
    if (!vm)
    {
        return NULL;
    }
    vm_init(vm);
    // module_loader_init(vm); // TODO: Initialize module loader
    builtin_modules_init();
    return vm;
}

void vm_init_with_loader(VM* vm, ModuleLoader* loader)
{
    // Initialize basic VM state
    vm_init(vm);

    // Set the module loader
    vm->module_loader = loader;
}

// Destroy a VM instance
void vm_destroy(VM* vm)
{
    if (vm)
    {
        // module_loader_cleanup(vm); // TODO: Cleanup module loader
        vm_free(vm);
        VM_FREE(vm, sizeof(VM));
    }
}

static Upvalue* capture_upvalue(VM* vm, TaggedValue* local)
{
    Upvalue* prev_upvalue = NULL;
    Upvalue* upvalue = vm->open_upvalues;

    // Walk the list to find an existing upvalue or the insertion point
    while (upvalue != NULL && upvalue->location > local)
    {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    // Found existing upvalue
    if (upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }

    // Create new upvalue
    Upvalue* created_upvalue = VM_NEW(Upvalue);
    created_upvalue->location = local;
    created_upvalue->next = upvalue;
    created_upvalue->closed = NIL_VAL;

    if (prev_upvalue == NULL)
    {
        vm->open_upvalues = created_upvalue;
    }
    else
    {
        prev_upvalue->next = created_upvalue;
    }

    return created_upvalue;
}

static void close_upvalues(VM* vm, TaggedValue* last)
{
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last)
    {
        Upvalue* upvalue = vm->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->open_upvalues = upvalue->next;
    }
}

static void vm_runtime_error(VM* vm, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    // Print stack trace
    fprintf(stderr, "\n[Stack trace]\n");
    for (int i = vm->frame_count - 1; i >= 0; i--)
    {
        CallFrame* frame = &vm->frames[i];
        size_t instruction = frame->ip - frame->closure->function->chunk.code - 1;
        fprintf(stderr, "  at %s:%d\n", frame->closure->function->name,
                frame->closure->function->chunk.lines[instruction]);
    }

    vm->stack_top = vm->stack; // Reset stack
    vm->frame_count = 0;
}

void vm_push(VM* vm, TaggedValue value)
{
    *vm->stack_top = value;
    vm->stack_top++;
}

TaggedValue vm_pop(VM* vm)
{
    vm->stack_top--;
    return *vm->stack_top;
}

static TaggedValue vm_peek(VM* vm, int distance)
{
    return vm->stack_top[-1 - distance];
}

static void call_frame_push(VM* vm, CallFrame* frame)
{
    vm->frames[vm->frame_count++] = *frame;
}

static InterpretResult call_closure(VM* vm, Closure* closure, int arg_count)
{
    if (arg_count != closure->function->arity)
    {
        vm_runtime_error(vm, "Expected %d arguments but got %d.", closure->function->arity, arg_count);
        return INTERPRET_RUNTIME_ERROR;
    }

    if (vm->frame_count >= FRAMES_MAX)
    {
        vm_runtime_error(vm, "Stack overflow.");
        return INTERPRET_RUNTIME_ERROR;
    }

    CallFrame frame = {0};
    frame.closure = closure;
    frame.ip = closure->function->chunk.code;
    frame.slots = vm->stack_top - arg_count - 1;
    call_frame_push(vm, &frame);
    return INTERPRET_OK;
}

// Define a global variable
void define_global(VM* vm, const char* name, TaggedValue value)
{
    // Check if we need to grow the global arrays
    if (vm->globals.count + 1 > vm->globals.capacity)
    {
        size_t old_capacity = vm->globals.capacity;
        vm->globals.capacity = old_capacity < 8 ? 8 : old_capacity * 2;

        // Realloc pattern for names array
        char** new_names = VM_NEW_ARRAY(char*, vm->globals.capacity);
        if (vm->globals.names)
        {
            memcpy(new_names, vm->globals.names, old_capacity * sizeof(char*));
            VM_FREE(vm->globals.names, old_capacity * sizeof(char*));
        }
        vm->globals.names = new_names;

        // Realloc pattern for values array
        TaggedValue* new_values = VM_NEW_ARRAY(TaggedValue, vm->globals.capacity);
        if (vm->globals.values)
        {
            memcpy(new_values, vm->globals.values, old_capacity * sizeof(TaggedValue));
            VM_FREE(vm->globals.values, old_capacity * sizeof(TaggedValue));
        }
        vm->globals.values = new_values;
    }

    vm->globals.names[vm->globals.count] = STR_DUP(name);
    vm->globals.values[vm->globals.count] = value;
    vm->globals.count++;
}

// Undefine a global variable
void undefine_global(VM* vm, const char* name)
{
    for (size_t i = 0; i < vm->globals.count; i++)
    {
        if (strcmp(vm->globals.names[i], name) == 0)
        {
            STR_FREE(vm->globals.names[i], strlen(vm->globals.names[i]) + 1);
            // Shift remaining globals down
            for (size_t j = i; j < vm->globals.count - 1; j++)
            {
                vm->globals.names[j] = vm->globals.names[j + 1];
                vm->globals.values[j] = vm->globals.values[j + 1];
            }
            vm->globals.count--;
            return;
        }
    }
}

// New helper function to convert a value to string
static const char* value_to_string(VM* vm, TaggedValue value);

// Forward declare call_native
static InterpretResult call_native(VM* vm, NativeFn native, int arg_count);

// Value equality comparison
bool values_equal(TaggedValue a, TaggedValue b)
{
    if (a.type != b.type) return false;

    switch (a.type)
    {
    case VAL_BOOL: return AS_BOOL(a) == AS_BOOL(b);
    case VAL_NIL: return true;
    case VAL_NUMBER: return AS_NUMBER(a) == AS_NUMBER(b);
    case VAL_STRING: return strcmp(AS_STRING(a), AS_STRING(b)) == 0;
    case VAL_FUNCTION: return AS_FUNCTION(a) == AS_FUNCTION(b);
    case VAL_NATIVE: return AS_NATIVE(a) == AS_NATIVE(b);
    case VAL_OBJECT: return a.as.object == b.as.object;
    default: return false;
    }
}

// Original vm_interpret for compatibility
InterpretResult vm_interpret(VM* vm, Chunk* chunk)
{
    // Create a dummy function to wrap the chunk
    Function main_func = {
        .chunk = *chunk,
        .name = "<script>",
        .arity = 0,
        .upvalue_count = 0,
        .module = NULL
    };
    return vm_interpret_function(vm, &main_func);
}

static InterpretResult vm_interpret_function(VM* vm, Function* function)
{
    Closure closure;
    closure.function = function;
    closure.upvalues = NULL;
    closure.upvalue_count = 0;

    vm_push(vm, (TaggedValue){VAL_CLOSURE, {.closure = &closure}});

    CallFrame* frame = &vm->frames[vm->frame_count];
    frame->closure = &closure;
    frame->ip = function->chunk.code;
    frame->slots = vm->stack;
    vm->frame_count = 1;

    for (;;)
    {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (TaggedValue* slot = vm->stack; slot < vm->stack_top; slot++)
        {
            printf("[ ");
            print_value(*slot);
            printf(" ]");
        }
        printf("\n");
        disassemble_instruction(&frame->closure->function->chunk,
                                (int)(frame->ip - frame->closure->function->chunk.code));
#endif

        uint8_t instruction = *frame->ip++;
        switch (instruction)
        {
        case OP_CONSTANT:
            {
                uint8_t index = *frame->ip++;
                TaggedValue constant = frame->closure->function->chunk.constants.values[index];
                vm_push(vm, constant);
                break;
            }

        case OP_TRUE:
            vm_push(vm, BOOL_VAL(true));
            break;

        case OP_FALSE:
            vm_push(vm, BOOL_VAL(false));
            break;

        case OP_NIL:
            vm_push(vm, NIL_VAL);
            break;

        case OP_POP:
            vm_pop(vm);
            break;

        case OP_DUP:
            {
                TaggedValue value = vm_peek(vm, 0);
                vm_push(vm, value);
                break;
            }

        case OP_SWAP:
            {
                TaggedValue a = vm_pop(vm);
                TaggedValue b = vm_pop(vm);
                vm_push(vm, a);
                vm_push(vm, b);
                break;
            }

        /* OP_STRING_CONCAT not yet implemented
        case OP_STRING_CONCAT:
        {
            TaggedValue b = vm_pop(vm);
            TaggedValue a = vm_pop(vm);

            if (IS_STRING(a) && IS_STRING(b))
            {
                const char* str_a = AS_STRING(a);
                const char* str_b = AS_STRING(b);
                size_t len_a = strlen(str_a);
                size_t len_b = strlen(str_b);
                size_t total_len = len_a + len_b + 1;

                char* buffer = STR_ALLOC(total_len);
                strcpy(buffer, str_a);
                strcat(buffer, str_b);

                // Intern the string
                const char* interned = string_pool_intern(&vm->strings, buffer, strlen(buffer));
                STR_FREE(buffer, total_len);

                vm_push(vm, STRING_VAL(interned));
            }
            else
            {
                vm_runtime_error(vm, "Operands must be strings.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        } */

        /* OP_STRING_INTERP not yet implemented
        case OP_STRING_INTERP:
        {
            uint8_t part_count = *frame->ip++;
            size_t total_length = 0;
            size_t* lengths = VM_ALLOC(sizeof(size_t) * part_count);
            const char** parts = VM_ALLOC(sizeof(char*) * part_count);

            // Calculate total length
            for (int i = part_count - 1; i >= 0; i--)
            {
                TaggedValue val = vm_peek(vm, i);
                const char* str = value_to_string(vm, val);
                parts[part_count - 1 - i] = str;
                lengths[part_count - 1 - i] = strlen(str);
                total_length += lengths[part_count - 1 - i];
            }

            // Allocate result buffer
            char* buffer = STR_ALLOC(total_length + 1);
            char* ptr = buffer;

            // Concatenate all parts
            for (int i = 0; i < part_count; i++)
            {
                strcpy(ptr, parts[i]);
                ptr += lengths[i];
            }

            // Pop all parts
            for (int i = 0; i < part_count; i++)
            {
                vm_pop(vm);
            }

            // Intern and push result
            const char* interned = string_pool_intern(&vm->strings, buffer, total_length + 1);
            STR_FREE(buffer, total_length + 1);
            VM_FREE(lengths, sizeof(size_t) * part_count);
            VM_FREE(parts, sizeof(char*) * part_count);

            vm_push(vm, STRING_VAL(interned));
            break;
        } */

        /* OP_INTERN_STRING not yet implemented
        case OP_INTERN_STRING:
        {
            TaggedValue string_val = vm_pop(vm);
            if (IS_STRING(string_val))
            {
                const char* str = AS_STRING(string_val);
                const char* interned = string_pool_intern(&vm->strings, str, strlen(str));
                vm_push(vm, STRING_VAL(interned));
            }
            else
            {
                vm_runtime_error(vm, "Can only intern strings");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        } */

        case OP_CONSTANT_LONG:
            {
                uint16_t index = (uint16_t)(*frame->ip++) << 8;
                index |= *frame->ip++;
                TaggedValue constant = frame->closure->function->chunk.constants.values[index];
                vm_push(vm, constant);
                break;
            }

        case OP_EQUAL:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                vm_push(vm, BOOL_VAL(values_equal(a, b)));
                break;
            }

        case OP_NOT_EQUAL:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                vm_push(vm, BOOL_VAL(!values_equal(a, b)));
                break;
            }

        case OP_GREATER:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b))
                {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) > AS_NUMBER(b)));
                }
                else
                {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_GREATER_EQUAL:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b))
                {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) >= AS_NUMBER(b)));
                }
                else
                {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_LESS:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b))
                {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) < AS_NUMBER(b)));
                }
                else
                {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_LESS_EQUAL:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b))
                {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) <= AS_NUMBER(b)));
                }
                else
                {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_ADD:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b))
                {
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) + AS_NUMBER(b)));
                }
                else if (IS_STRING(a) && IS_STRING(b))
                {
                    const char* str_a = AS_STRING(a);
                    const char* str_b = AS_STRING(b);
                    size_t len_a = strlen(str_a);
                    size_t len_b = strlen(str_b);
                    size_t total_len = len_a + len_b + 1;

                    char* buffer = STR_ALLOC(total_len);
                    strcpy(buffer, str_a);
                    strcat(buffer, str_b);

                    const char* interned = string_pool_intern(&vm->strings, buffer, strlen(buffer));
                    STR_FREE(buffer, total_len);

                    vm_push(vm, STRING_VAL(interned));
                }
                else
                {
                    vm_runtime_error(vm, "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_SUBTRACT:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b))
                {
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) - AS_NUMBER(b)));
                }
                else
                {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_MULTIPLY:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b))
                {
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) * AS_NUMBER(b)));
                }
                else
                {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_DIVIDE:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b))
                {
                    if (AS_NUMBER(b) == 0)
                    {
                        vm_runtime_error(vm, "Division by zero.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) / AS_NUMBER(b)));
                }
                else
                {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_MODULO:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b))
                {
                    double divisor = AS_NUMBER(b);
                    if (divisor == 0)
                    {
                        vm_runtime_error(vm, "Division by zero in modulo operation.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    vm_push(vm, NUMBER_VAL(fmod(AS_NUMBER(a), divisor)));
                }
                else
                {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        /* OP_POWER not yet implemented
        case OP_POWER:
        {
            TaggedValue b = vm_pop(vm);
            TaggedValue a = vm_pop(vm);
            if (IS_NUMBER(a) && IS_NUMBER(b))
            {
                vm_push(vm, NUMBER_VAL(pow(AS_NUMBER(a), AS_NUMBER(b))));
            }
            else
            {
                vm_runtime_error(vm, "Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        } */

        case OP_NEGATE:
            {
                if (!IS_NUMBER(vm_peek(vm, 0)))
                {
                    vm_runtime_error(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                TaggedValue value = vm_pop(vm);
                vm_push(vm, NUMBER_VAL(-AS_NUMBER(value)));
                break;
            }

        case OP_NOT:
            {
                TaggedValue value = vm_pop(vm);
                vm_push(vm, BOOL_VAL(is_falsey(value)));
                break;
            }

        case OP_TO_STRING:
            {
                TaggedValue val = vm_pop(vm);
                if (IS_NIL(val))
                {
                    vm_push(vm, STRING_VAL(STR_DUP("nil")));
                }
                else if (IS_BOOL(val))
                {
                    vm_push(vm, STRING_VAL(STR_DUP(AS_BOOL(val) ? "true" : "false")));
                }
                else if (IS_NUMBER(val))
                {
                    char buffer[32];
                    double num = AS_NUMBER(val);
                    if (num == (int64_t)num)
                    {
                        snprintf(buffer, sizeof(buffer), "%lld", (int64_t)num);
                    }
                    else
                    {
                        snprintf(buffer, sizeof(buffer), "%.6g", num);
                    }
                    vm_push(vm, STRING_VAL(STR_DUP(buffer)));
                }
                else if (IS_STRING(val))
                {
                    vm_push(vm, val);
                }
                else if (IS_ARRAY(val))
                {
                    char buffer[256];
                    snprintf(buffer, sizeof(buffer), "<array>");
                    vm_push(vm, STRING_VAL(STR_DUP(buffer)));
                }
                else if (IS_NATIVE(val))
                {
                    vm_push(vm, STRING_VAL(STR_DUP("<native function>")));
                }
                else if (IS_OBJECT(val))
                {
                    Object* obj = AS_OBJECT(val);
                    Object* proto = object_get_prototype(obj);
                    if (proto != NULL)
                    {
                        TaggedValue* name_val_ptr = object_get_property(proto, "__name__");
                        if (name_val_ptr != NULL && IS_STRING(*name_val_ptr))
                        {
                            size_t result_size = 1024;
                            char* result = STR_ALLOC(result_size); // Initial buffer
                            snprintf(result, result_size, "<%s instance>", AS_STRING(*name_val_ptr));
                            const char* interned = string_pool_intern(&vm->strings, result, strlen(result));
                            STR_FREE(result, result_size);
                            vm_push(vm, STRING_VAL(interned));
                            break;
                        }
                    }

                    // Check for __struct_type__ property
                    TaggedValue* type_val_ptr = object_get_property(obj, "__struct_type__");
                    if (type_val_ptr != NULL && IS_STRING(*type_val_ptr))
                    {
                        char buffer[256];
                        snprintf(buffer, sizeof(buffer), "<%s instance>", AS_STRING(*type_val_ptr));
                        vm_push(vm, STRING_VAL(STR_DUP(buffer)));
                    }
                    else
                    {
                        vm_push(vm, STRING_VAL(STR_DUP("<object>")));
                    }
                }
                else
                {
                    vm_push(vm, STRING_VAL(STR_DUP("<unknown>")));
                }
                break;
            }

        case OP_PRINT:
            {
                TaggedValue value = vm_pop(vm);
                const char* str = value_to_string(vm, value);
                vm_print_internal(str, "", true);
                break;
            }

        /* OP_PRINT_EXPR not yet implemented
        case OP_PRINT_EXPR:
        {
            // Used for the CLI's expression printing
            TaggedValue value = vm_peek(vm, 0);
            const char* str = value_to_string(vm, value);
            vm_print_internal(str, "", true);
            break;
        } */

        /* OP_DEBUG not yet implemented
        case OP_DEBUG:
        {
            uint8_t count = *frame->ip++;
            printf("Debug: ");
            for (int i = count - 1; i >= 0; i--)
            {
                print_value(vm_peek(vm, i));
                if (i > 0) printf(", ");
            }
            printf("\n");
            // Pop all values
            for (int i = 0; i < count; i++)
            {
                vm_pop(vm);
            }
            break;
        } */

        case OP_JUMP:
            {
                uint16_t offset = (uint16_t)(*frame->ip++) << 8;
                offset |= *frame->ip++;
                frame->ip += offset;
                break;
            }

        case OP_JUMP_IF_FALSE:
            {
                uint16_t offset = (uint16_t)(*frame->ip++) << 8;
                offset |= *frame->ip++;
                if (is_falsey(vm_peek(vm, 0)))
                {
                    frame->ip += offset;
                }
                break;
            }

        case OP_JUMP_IF_TRUE:
            {
                uint16_t offset = (uint16_t)(*frame->ip++) << 8;
                offset |= *frame->ip++;
                if (!is_falsey(vm_peek(vm, 0)))
                {
                    frame->ip += offset;
                }
                break;
            }

        case OP_LOOP:
            {
                uint16_t offset = (uint16_t)(*frame->ip++) << 8;
                offset |= *frame->ip++;
                frame->ip -= offset;
                break;
            }

        case OP_GET_LOCAL:
            {
                uint8_t slot = *frame->ip++;
                vm_push(vm, frame->slots[slot]);
                break;
            }

        case OP_SET_LOCAL:
            {
                uint8_t slot = *frame->ip++;
                frame->slots[slot] = vm_peek(vm, 0);
                break;
            }

        case OP_GET_GLOBAL:
            {
                uint8_t name_index = *frame->ip++;
                const char* name = AS_STRING(frame->closure->function->chunk.constants.values[name_index]);

                // Look in current module first
                if (vm->current_module)
                {
                    for (size_t i = 0; i < vm->current_module->globals.count; i++)
                    {
                        if (strcmp(vm->current_module->globals.names[i], name) == 0)
                        {
                            vm_push(vm, vm->current_module->globals.values[i]);
                            goto found_global;
                        }
                    }
                }

                // Then look in VM globals
                for (size_t i = 0; i < vm->globals.count; i++)
                {
                    if (strcmp(vm->globals.names[i], name) == 0)
                    {
                        vm_push(vm, vm->globals.values[i]);
                        goto found_global;
                    }
                }

                vm_runtime_error(vm, "Undefined global variable '%s'.", name);
                return INTERPRET_RUNTIME_ERROR;

            found_global:
                break;
            }

        case OP_SET_GLOBAL:
            {
                uint8_t name_index = *frame->ip++;
                const char* name = AS_STRING(frame->closure->function->chunk.constants.values[name_index]);
                TaggedValue value = vm_peek(vm, 0);

                // Check if we're setting a module-level global
                if (vm->current_module)
                {
                    // Check if it already exists in module
                    for (size_t i = 0; i < vm->current_module->globals.count; i++)
                    {
                        if (strcmp(vm->current_module->globals.names[i], name) == 0)
                        {
                            vm->current_module->globals.values[i] = value;
                            goto global_set;
                        }
                    }

                    // Add to module globals
                    if (vm->current_module->globals.count + 1 > vm->current_module->globals.capacity)
                    {
                        size_t old_capacity = vm->current_module->globals.capacity;
                        size_t new_capacity = old_capacity < 8 ? 8 : old_capacity * 2;

                        // Realloc pattern for names
                        char** new_names = VM_ALLOC(new_capacity * sizeof(char*));
                        if (vm->current_module->globals.names)
                        {
                            memcpy(new_names, vm->current_module->globals.names, old_capacity * sizeof(char*));
                            MODULE_FREE(vm->current_module->globals.names, old_capacity * sizeof(char*));
                        }
                        vm->current_module->globals.names = new_names;

                        // Realloc pattern for values
                        TaggedValue* new_values = VM_ALLOC(new_capacity * sizeof(TaggedValue));
                        if (vm->current_module->globals.values)
                        {
                            memcpy(new_values, vm->current_module->globals.values, old_capacity * sizeof(TaggedValue));
                            MODULE_FREE(vm->current_module->globals.values, old_capacity * sizeof(TaggedValue));
                        }
                        vm->current_module->globals.values = new_values;

                        vm->current_module->globals.capacity = new_capacity;
                    }

                    vm->current_module->globals.names[vm->current_module->globals.count] = MODULE_DUP(name);
                    vm->current_module->globals.values[vm->current_module->globals.count] = value;
                    vm->current_module->globals.count++;
                }
                else
                {
                    // Setting global in VM
                    for (size_t i = 0; i < vm->globals.count; i++)
                    {
                        if (strcmp(vm->globals.names[i], name) == 0)
                        {
                            vm->globals.values[i] = value;
                            goto global_set;
                        }
                    }

                    // Not found - define new global
                    define_global(vm, name, value);
                }

            global_set:
                break;
            }

        case OP_DEFINE_GLOBAL:
            {
                uint8_t name_index = *frame->ip++;
                const char* name = AS_STRING(frame->closure->function->chunk.constants.values[name_index]);
                TaggedValue value = vm_pop(vm);

                if (vm->current_module)
                {
                    // Module-level global
                    if (vm->current_module->globals.count + 1 > vm->current_module->globals.capacity)
                    {
                        size_t old_capacity = vm->current_module->globals.capacity;
                        size_t new_capacity = old_capacity < 8 ? 8 : old_capacity * 2;

                        // Realloc pattern for names
                        char** new_names = VM_ALLOC(new_capacity * sizeof(char*));
                        if (vm->current_module->globals.names)
                        {
                            memcpy(new_names, vm->current_module->globals.names, old_capacity * sizeof(char*));
                            MODULE_FREE(vm->current_module->globals.names, old_capacity * sizeof(char*));
                        }
                        vm->current_module->globals.names = new_names;

                        // Realloc pattern for values
                        TaggedValue* new_values = VM_ALLOC(new_capacity * sizeof(TaggedValue));
                        if (vm->current_module->globals.values)
                        {
                            memcpy(new_values, vm->current_module->globals.values, old_capacity * sizeof(TaggedValue));
                            MODULE_FREE(vm->current_module->globals.values, old_capacity * sizeof(TaggedValue));
                        }
                        vm->current_module->globals.values = new_values;

                        vm->current_module->globals.capacity = new_capacity;
                    }

                    vm->current_module->globals.names[vm->current_module->globals.count] = MODULE_DUP(name);
                    vm->current_module->globals.values[vm->current_module->globals.count] = value;
                    vm->current_module->globals.count++;
                }
                else
                {
                    // VM-level global
                    define_global(vm, name, value);
                }
                break;
            }

        /* OP_ARRAY_LITERAL not yet implemented
        case OP_ARRAY_LITERAL:
        {
            uint8_t count = *frame->ip++;
            Array* array = array_new(vm);

            // Pre-allocate array capacity
            if (count > 0) {
                array_ensure_capacity(array, count);
            }

            // Allocate temporary storage for values
            TaggedValue* temp = VM_ALLOC(sizeof(TaggedValue) * count);

            // Pop values in reverse order
            for (int i = count - 1; i >= 0; i--) {
                temp[i] = vm_pop(vm);
            }

            // Add to array in correct order
            for (int i = 0; i < count; i++) {
                array_push(array, temp[i]);
            }

            VM_FREE(temp, sizeof(TaggedValue) * count);
            vm_push(vm, ARRAY_VAL(array));
            break;
        } */

        /* OP_GET_ELEMENT not yet implemented
        case OP_GET_ELEMENT:
        {
            TaggedValue index = vm_pop(vm);
            TaggedValue collection = vm_pop(vm);

            if (IS_ARRAY(collection))
            {
                if (!IS_NUMBER(index))
                {
                    vm_runtime_error(vm, "Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                Array* array = AS_ARRAY(collection);
                int i = (int)AS_NUMBER(index);

                if (i < 0 || i >= (int)array->count)
                {
                    vm_runtime_error(vm, "Array index %d out of bounds (size: %zu).", i, array->count);
                    return INTERPRET_RUNTIME_ERROR;
                }

                vm_push(vm, array->elements[i]);
            }
            else if (IS_STRING(collection))
            {
                if (!IS_NUMBER(index))
                {
                    vm_runtime_error(vm, "String index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                const char* str = AS_STRING(collection);
                int i = (int)AS_NUMBER(index);
                size_t len = strlen(str);

                if (i < 0 || i >= (int)len)
                {
                    vm_runtime_error(vm, "String index %d out of bounds (length: %zu).", i, len);
                    return INTERPRET_RUNTIME_ERROR;
                }

                char char_str[2] = {str[i], '\0'};
                const char* interned = string_pool_intern(&vm->strings, char_str, 2);
                vm_push(vm, STRING_VAL(interned));
            }
            else if (IS_OBJECT(collection))
            {
                if (IS_STRING(index))
                {
                    Object* obj = AS_OBJECT(collection);
                    const char* property_name = AS_STRING(index);
                    TaggedValue value = object_get_property(obj, property_name);
                    vm_push(vm, value);
                }
                else
                {
                    vm_runtime_error(vm, "Object property access requires string key.");
                    return INTERPRET_RUNTIME_ERROR;
                }
            }
            else
            {
                vm_runtime_error(vm, "Cannot index into non-collection type.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        } */

        /* OP_SET_ELEMENT not yet implemented
        case OP_SET_ELEMENT:
        {
            TaggedValue value = vm_pop(vm);
            TaggedValue index = vm_pop(vm);
            TaggedValue collection = vm_pop(vm);

            if (IS_ARRAY(collection))
            {
                if (!IS_NUMBER(index))
                {
                    vm_runtime_error(vm, "Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                Array* array = AS_ARRAY(collection);
                int i = (int)AS_NUMBER(index);

                if (i < 0 || i >= (int)array->count)
                {
                    vm_runtime_error(vm, "Array index %d out of bounds (size: %zu).", i, array->count);
                    return INTERPRET_RUNTIME_ERROR;
                }

                array->elements[i] = value;
                vm_push(vm, value);
            }
            else if (IS_OBJECT(collection))
            {
                if (!IS_STRING(index))
                {
                    vm_runtime_error(vm, "Object property name must be a string.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                Object* obj = AS_OBJECT(collection);
                const char* property_name = AS_STRING(index);
                object_set_property(obj, property_name, value);
                vm_push(vm, value);
            }
            else
            {
                vm_runtime_error(vm, "Cannot set element on non-collection type.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        } */

        /* OP_LENGTH not yet implemented
        case OP_LENGTH:
        {
            TaggedValue value = vm_pop(vm);
            if (IS_ARRAY(value))
            {
                // TODO: Get array count from object property
                vm_push(vm, NUMBER_VAL(0.0));
            }
            else if (IS_STRING(value))
            {
                vm_push(vm, NUMBER_VAL((double)strlen(AS_STRING(value))));
            }
            else if (IS_OBJECT(value))
            {
                // Check for length property
                Object* obj = AS_OBJECT(value);
                TaggedValue* len_ptr = object_get_property(obj, "length");
                if (len_ptr != NULL)
                {
                    vm_push(vm, *len_ptr);
                }
                else
                {
                    vm_runtime_error(vm, "Object has no length property.");
                    return INTERPRET_RUNTIME_ERROR;
                }
            }
            else
            {
                vm_runtime_error(vm, "Cannot get length of non-collection type.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        } */

        /* OP_ARRAY_PUSH not yet implemented
        case OP_ARRAY_PUSH:
        {
            TaggedValue element = vm_pop(vm);
            TaggedValue array_val = vm_pop(vm);

            if (!IS_ARRAY(array_val))
            {
                vm_runtime_error(vm, "Can only push to arrays.");
                return INTERPRET_RUNTIME_ERROR;
            }

            Array* array = AS_ARRAY(array_val);
            array_push(array, element);
            vm_push(vm, NIL_VAL);  // Push returns nothing
            break;
        } */

        case OP_CALL:
            {
                uint8_t arg_count = *frame->ip++;
                TaggedValue callee = vm_peek(vm, arg_count);

                if (IS_CLOSURE(callee))
                {
                    Closure* closure = AS_CLOSURE(callee);
                    InterpretResult result = call_closure(vm, closure, arg_count);
                    if (result != INTERPRET_OK)
                    {
                        return result;
                    }
                    frame = &vm->frames[vm->frame_count - 1];
                }
                else if (IS_NATIVE(callee))
                {
                    NativeFn native = AS_NATIVE(callee);
                    InterpretResult result = call_native(vm, native, arg_count);
                    if (result != INTERPRET_OK)
                    {
                        return result;
                    }
                }
                else
                {
                    vm_runtime_error(vm, "Can only call functions.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_RETURN:
            {
                TaggedValue result = vm_pop(vm);
                close_upvalues(vm, frame->slots);
                vm->frame_count--;
                if (vm->frame_count == 0)
                {
                    vm_pop(vm); // Pop the script function
                    return INTERPRET_OK;
                }

                vm->stack_top = frame->slots;
                vm_push(vm, result);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

        case OP_CLOSURE:
            {
                uint8_t function_index = *frame->ip++;
                Function* function = AS_FUNCTION(frame->closure->function->chunk.constants.values[function_index]);
                Closure* closure = BYTECODE_NEW(Closure);
                closure->function = function;
                closure->upvalue_count = function->upvalue_count;

                if (closure->upvalue_count > 0)
                {
                    closure->upvalues = BYTECODE_NEW_ARRAY(Upvalue*, closure->upvalue_count);
                    for (int i = 0; i < closure->upvalue_count; i++)
                    {
                        uint8_t is_local = *frame->ip++;
                        uint8_t index = *frame->ip++;
                        if (is_local)
                        {
                            closure->upvalues[i] = capture_upvalue(vm, frame->slots + index);
                        }
                        else
                        {
                            closure->upvalues[i] = frame->closure->upvalues[index];
                        }

                        // Check for NULL upvalue (shouldn't happen in well-formed bytecode)
                        if (closure->upvalues[i] == NULL)
                        {
                            vm_runtime_error(vm, "Failed to capture upvalue %d for closure.", i);
                            // Free what we've allocated so far
                            for (int j = 0; j < i; j++)
                            {
                                // Upvalues themselves are managed by the VM
                            }
                            BYTECODE_FREE(closure->upvalues, sizeof(Upvalue*) * closure->upvalue_count);
                            BYTECODE_FREE(closure, sizeof(Closure));
                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                }
                else
                {
                    closure->upvalues = NULL;
                }

                vm_push(vm, (TaggedValue){VAL_CLOSURE, {.closure = closure}});
                break;
            }

        case OP_GET_UPVALUE:
            {
                uint8_t slot = *frame->ip++;
                if (slot >= frame->closure->upvalue_count)
                {
                    vm_runtime_error(vm, "Invalid upvalue index %d (closure has %d upvalues).",
                                     slot, frame->closure->upvalue_count);
                    return INTERPRET_RUNTIME_ERROR;
                }
                Upvalue* upvalue = frame->closure->upvalues[slot];
                if (upvalue == NULL)
                {
                    vm_runtime_error(vm, "Upvalue %d is NULL.", slot);
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm_push(vm, *upvalue->location);
                break;
            }

        case OP_SET_UPVALUE:
            {
                uint8_t slot = *frame->ip++;
                if (slot >= frame->closure->upvalue_count)
                {
                    vm_runtime_error(vm, "Invalid upvalue index %d (closure has %d upvalues).",
                                     slot, frame->closure->upvalue_count);
                    return INTERPRET_RUNTIME_ERROR;
                }
                Upvalue* upvalue = frame->closure->upvalues[slot];
                if (upvalue == NULL)
                {
                    vm_runtime_error(vm, "Upvalue %d is NULL.", slot);
                    return INTERPRET_RUNTIME_ERROR;
                }
                *upvalue->location = vm_peek(vm, 0);
                break;
            }

        case OP_CLOSE_UPVALUE:
            {
                close_upvalues(vm, vm->stack_top - 1);
                vm_pop(vm);
                break;
            }

        /* OP_STRUCT not yet implemented
        case OP_STRUCT:
        {
            uint8_t field_count = *frame->ip++;

            // Allocate arrays for field names
            char** field_names = VM_ALLOC(sizeof(char*) * field_count);
            bool allocation_failed = false;

            // Pop field names from stack (they were pushed in reverse order)
            for (int i = field_count - 1; i >= 0; i--)
            {
                TaggedValue field_val = vm_pop(vm);
                if (!IS_STRING(field_val))
                {
                    for (int j = i + 1; j < field_count; j++) {
                        STR_FREE(field_names[j], strlen(field_names[j]) + 1);
                    }
                    VM_FREE(field_names, sizeof(char*) * field_count);
                    vm_runtime_error(vm, "Struct field name must be a string.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                const char* field_str = AS_STRING(field_val);
                if (strchr(field_str, ' ') != NULL)
                {
                    for (int j = i + 1; j < field_count; j++) {
                        STR_FREE(field_names[j], strlen(field_names[j]) + 1);
                    }
                    VM_FREE(field_names, sizeof(char*) * field_count);
                    vm_runtime_error(vm, "Struct field name cannot contain spaces: '%s'.", field_str);
                    return INTERPRET_RUNTIME_ERROR;
                }
                field_names[i] = STR_DUP(field_str);
            }

            // Get struct name
            TaggedValue name_val = vm_pop(vm);
            if (!IS_STRING(name_val))
            {
                for (int i = 0; i < field_count; i++) {
                    STR_FREE(field_names[i], strlen(field_names[i]) + 1);
                }
                VM_FREE(field_names, sizeof(char*) * field_count);
                vm_runtime_error(vm, "Struct name must be a string.");
                return INTERPRET_RUNTIME_ERROR;
            }
            const char* struct_name = AS_STRING(name_val);

            // Create the struct type
            // StructType* type = create_struct_type(struct_name, field_names, field_count); // TODO: Implement
            StructType* type = NULL; // Placeholder

            // Store in VM's struct types
            if (vm->struct_types.count + 1 > vm->struct_types.capacity)
            {
                size_t old_capacity = vm->struct_types.capacity;
                size_t new_capacity = old_capacity < 8 ? 8 : old_capacity * 2;

                // Realloc pattern for names
                char** new_names = VM_NEW_ARRAY(char*, new_capacity);
                if (vm->struct_types.names) {
                    memcpy(new_names, vm->struct_types.names, old_capacity * sizeof(char*));
                    VM_FREE(vm->struct_types.names, old_capacity * sizeof(char*));
                }
                vm->struct_types.names = new_names;

                // Realloc pattern for types
                StructType** new_types = VM_NEW_ARRAY(StructType*, new_capacity);
                if (vm->struct_types.types) {
                    memcpy(new_types, vm->struct_types.types, old_capacity * sizeof(StructType*));
                    VM_FREE(vm->struct_types.types, old_capacity * sizeof(StructType*));
                }
                vm->struct_types.types = new_types;

                vm->struct_types.capacity = new_capacity;
            }
            vm->struct_types.names[vm->struct_types.count] = STR_DUP(struct_name);
            vm->struct_types.types[vm->struct_types.count] = type;
            vm->struct_types.count++;

            // Push the struct type as a value
            vm_push(vm, STRUCT_TYPE_VAL(type));

            // Clean up field names array (strings are owned by struct type now)
            VM_FREE(field_names, sizeof(char*) * field_count);
            break;
        } */

        /* OP_CONSTRUCT not yet implemented
        case OP_CONSTRUCT:
        {
            uint8_t field_count = *frame->ip++;
            TaggedValue type_val = vm_peek(vm, field_count);

            if (!IS_STRUCT_TYPE(type_val))
            {
                vm_runtime_error(vm, "Can only construct from struct types.");
                return INTERPRET_RUNTIME_ERROR;
            }

            StructType* type = AS_STRUCT_TYPE(type_val);
            if (field_count != type->field_count)
            {
                vm_runtime_error(vm, "Expected %d arguments for struct %s, got %d.",
                                 type->field_count, type->name, field_count);
                return INTERPRET_RUNTIME_ERROR;
            }

            // Create new object and set prototype
            Object* obj = object_create();

            // Pop field values and set properties
            for (int i = field_count - 1; i >= 0; i--)
            {
                TaggedValue value = vm_pop(vm);
                object_set_property(obj, type->field_names[i], value);
            }

            // Pop the struct type
            vm_pop(vm);

            // Get the struct name for the type property
            const char* type_name = NULL;
            for (size_t i = 0; i < vm->struct_types.count; i++)
            {
                if (vm->struct_types.types[i] == type)
                {
                    type_name = vm->struct_types.names[i];
                    break;
                }
            }

            if (type_name)
            {
                object_set_property(obj, "__struct_type__", STRING_VAL(STR_DUP(type_name)));
            }

            vm_push(vm, OBJECT_VAL(obj));
            break;
        } */

        case OP_GET_PROPERTY:
            {
                TaggedValue name_val = vm_pop(vm);
                TaggedValue object_val = vm_pop(vm);

                if (!IS_STRING(name_val))
                {
                    vm_runtime_error(vm, "Property name must be a string.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                const char* property_name = AS_STRING(name_val);

                if (IS_OBJECT(object_val))
                {
                    Object* obj = AS_OBJECT(object_val);
                    TaggedValue* value_ptr = object_get_property(obj, property_name);
                    if (value_ptr)
                    {
                        vm_push(vm, *value_ptr);
                    }
                    else
                    {
                        vm_push(vm, NIL_VAL);
                    }
                }
                else if (IS_ARRAY(object_val))
                {
                    // Handle array properties
                    Array* array = AS_ARRAY(object_val);
                    if (strcmp(property_name, "length") == 0)
                    {
                        vm_push(vm, NUMBER_VAL((double)array->count));
                    }
                    else if (strcmp(property_name, "push") == 0)
                    {
                        // Return the array push method
                        vm_push(vm, NIL_VAL); // TODO: native_array_push not implemented
                    }
                    else if (strcmp(property_name, "pop") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_array_pop not implemented
                    }
                    else if (strcmp(property_name, "join") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_array_join not implemented
                    }
                    else if (strcmp(property_name, "slice") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_array_slice not implemented
                    }
                    else if (strcmp(property_name, "indexOf") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_array_index_of not implemented
                    }
                    else if (strcmp(property_name, "includes") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_array_includes not implemented
                    }
                    else if (strcmp(property_name, "reverse") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_array_reverse not implemented
                    }
                    else if (strcmp(property_name, "concat") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_array_concat not implemented
                    }
                    else
                    {
                        vm_push(vm, NIL_VAL);
                    }
                }
                else if (IS_STRING(object_val))
                {
                    // Handle string properties
                    const char* str = AS_STRING(object_val);
                    if (strcmp(property_name, "length") == 0)
                    {
                        vm_push(vm, NUMBER_VAL((double)strlen(str)));
                    }
                    else if (strcmp(property_name, "substring") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_string_substring not implemented
                    }
                    else if (strcmp(property_name, "indexOf") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_string_index_of not implemented
                    }
                    else if (strcmp(property_name, "split") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_string_split not implemented
                    }
                    else if (strcmp(property_name, "replace") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_string_replace not implemented
                    }
                    else if (strcmp(property_name, "toLowerCase") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_string_to_lower_case not implemented
                    }
                    else if (strcmp(property_name, "toUpperCase") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_string_to_upper_case not implemented
                    }
                    else if (strcmp(property_name, "trim") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_string_trim not implemented
                    }
                    else if (strcmp(property_name, "startsWith") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_string_starts_with not implemented
                    }
                    else if (strcmp(property_name, "endsWith") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_string_ends_with not implemented
                    }
                    else if (strcmp(property_name, "repeat") == 0)
                    {
                        vm_push(vm, NIL_VAL); // TODO: native_string_repeat not implemented
                    }
                    else
                    {
                        vm_push(vm, NIL_VAL);
                    }
                }
                else
                {
                    vm_runtime_error(vm, "Only objects have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_SET_PROPERTY:
            {
                TaggedValue value = vm_pop(vm);
                TaggedValue name_val = vm_pop(vm);
                TaggedValue object_val = vm_pop(vm);

                if (!IS_OBJECT(object_val))
                {
                    vm_runtime_error(vm, "Only objects have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!IS_STRING(name_val))
                {
                    vm_runtime_error(vm, "Property name must be a string.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                Object* obj = AS_OBJECT(object_val);
                const char* property_name = AS_STRING(name_val);
                object_set_property(obj, property_name, value);
                vm_push(vm, value);
                break;
            }

        /* OP_OBJECT_LITERAL not yet implemented
        case OP_OBJECT_LITERAL:
        {
            uint8_t property_count = *frame->ip++;
            Object* obj = object_create();

            // Properties are on the stack as: value, key, value, key, ...
            for (int i = 0; i < property_count; i++)
            {
                TaggedValue value = vm_pop(vm);
                TaggedValue key = vm_pop(vm);

                if (!IS_STRING(key))
                {
                    vm_runtime_error(vm, "Object property key must be a string.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                object_set_property(obj, AS_STRING(key), value);
            }

            vm_push(vm, OBJECT_VAL(obj));
            break;
        } */

        /* OP_IMPORT not yet implemented
        case OP_IMPORT:
        {
            uint8_t path_index = *frame->ip++;
            const char* module_path = AS_STRING(frame->closure->function->chunk.constants.values[path_index]);

            // TaggedValue module_val = load_module(vm, module_path); // TODO: implement
            TaggedValue module_val = NIL_VAL;
            if (IS_NIL(module_val))
            {
                vm_runtime_error(vm, "Failed to load module: %s", module_path);
                return INTERPRET_RUNTIME_ERROR;
            }

            vm_push(vm, module_val);
            break;
        } */

        /* OP_EXPORT not yet implemented
        case OP_EXPORT:
        {
            TaggedValue value = vm_pop(vm);
            uint8_t name_index = *frame->ip++;
            const char* export_name = AS_STRING(frame->closure->function->chunk.constants.values[name_index]);

            if (vm->current_module)
            {
                module_add_export(vm->current_module, export_name, value);
            }
            break;
        } */

        default:
            vm_runtime_error(vm, "Unknown opcode %d.", instruction);
            return INTERPRET_RUNTIME_ERROR;
        }
    }
}

Function* function_new(const char* name)
{
    Function* function = BYTECODE_NEW(Function);
    function->name = STR_DUP(name);
    function->arity = 0;
    function->upvalue_count = 0;
    chunk_init(&function->chunk);
    return function;
}

void function_free(Function* function)
{
    if (function)
    {
        chunk_free(&function->chunk);
        STR_FREE((void*)function->name, strlen(function->name) + 1);
        BYTECODE_FREE(function, sizeof(Function));
    }
}

static bool is_falsey(TaggedValue value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// Enhanced call_native with better array method support
static InterpretResult call_native(VM* vm, NativeFn native, int arg_count)
{
    // For array methods, we need to include the array itself as an argument
    TaggedValue* args = vm->stack_top - arg_count;
    TaggedValue result = NIL_VAL;

    // Call the native function
    result = native(arg_count, args);

    // Pop arguments and function
    vm->stack_top -= arg_count + 1;

    // Push result
    vm_push(vm, result);
    return INTERPRET_OK;
}

// Helper function to convert value to string
static const char* value_to_string(VM* vm, TaggedValue value)
{
    static char buffer[256]; // Static buffer for simple conversions

    if (IS_STRING(value))
    {
        return AS_STRING(value);
    }
    else if (IS_NUMBER(value))
    {
        double num = AS_NUMBER(value);
        if (num == (int64_t)num)
        {
            snprintf(buffer, sizeof(buffer), "%lld", (int64_t)num);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "%.6g", num);
        }
        return buffer;
    }
    else if (IS_BOOL(value))
    {
        return AS_BOOL(value) ? "true" : "false";
    }
    else if (IS_NIL(value))
    {
        return "nil";
    }
    else if (IS_ARRAY(value))
    {
        // TODO: Implement array to string conversion using object system
        return "[...]";
    }
    else if (IS_OBJECT(value))
    {
        // Check for __struct_type__
        Object* obj = AS_OBJECT(value);
        TaggedValue* type_val_ptr = object_get_property(obj, "__struct_type__");
        if (type_val_ptr != NULL && IS_STRING(*type_val_ptr))
        {
            snprintf(buffer, sizeof(buffer), "<%s instance>", AS_STRING(*type_val_ptr));
            return buffer;
        }
        return "<object>";
    }
    else
    {
        return "<unknown>";
    }
}

// CLI helper functions
void vm_push_string(VM* vm, const char* str)
{
    const char* interned = string_pool_intern(&vm->strings, str, strlen(str));
    vm_push(vm, STRING_VAL(interned));
}

void vm_define_native(VM* vm, const char* name, NativeFn function)
{
    vm_push_string(vm, name);
    vm_push(vm, NATIVE_VAL(function));
    define_global(vm, name, vm->stack_top[-1]);
    vm_pop(vm);
    vm_pop(vm);
}

// Basic introspection
void vm_list_globals(VM* vm)
{
    printf("Global variables:\n");
    for (size_t i = 0; i < vm->globals.count; i++)
    {
        printf("  %s: ", vm->globals.names[i]);
        print_value(vm->globals.values[i]);
        printf("\n");
    }
}

TaggedValue vm_get_global(VM* vm, const char* name)
{
    for (size_t i = 0; i < vm->globals.count; i++)
    {
        if (strcmp(vm->globals.names[i], name) == 0)
        {
            return vm->globals.values[i];
        }
    }
    return NIL_VAL;
}

// Function creation and destruction
Function* function_create(const char* name, int arity)
{
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);

    Function* function = VM_NEW(Function);
    function->name = STR_DUP(name);
    function->arity = arity;
    function->upvalue_count = 0;
    function->module = NULL; // Will be set when function is defined in a module
    chunk_init(&function->chunk);
    return function;
}

// Forward declarations for vm_call_value (already declared in header)

// Call a value (function, closure, or native)
TaggedValue vm_call_value(VM* vm, TaggedValue callee, int arg_count, TaggedValue* args)
{
    if (IS_FUNCTION(callee))
    {
        return vm_call_function(vm, AS_FUNCTION(callee), arg_count, args);
    }
    else if (IS_CLOSURE(callee))
    {
        return vm_call_closure(vm, AS_CLOSURE(callee), arg_count, args);
    }
    else if (IS_NATIVE(callee))
    {
        // Direct native call
        NativeFn native = AS_NATIVE(callee);
        return native(arg_count, args);
    }
    else
    {
        // Not callable
        return NIL_VAL;
    }
}

// Helper to call a function
TaggedValue vm_call_function(VM* vm, Function* function, int arg_count, TaggedValue* args)
{
    // Push arguments onto stack
    for (int i = 0; i < arg_count; i++)
    {
        vm_push(vm, args[i]);
    }

    // Create closure from function
    Closure closure;
    closure.function = function;
    closure.upvalue_count = 0;
    closure.upvalues = NULL;

    // Call the closure
    if (call_closure(vm, &closure, arg_count) != INTERPRET_OK)
    {
        return NIL_VAL;
    }

    // Run the interpreter until this function returns
    int initial_frame_count = vm->frame_count - 1;
    while (vm->frame_count > initial_frame_count)
    {
        // Simple execution loop - this is a simplified version
        CallFrame* frame = &vm->frames[vm->frame_count - 1];
        uint8_t instruction = *frame->ip++;

        // Handle RETURN specially
        if (instruction == OP_RETURN)
        {
            TaggedValue result = vm_pop(vm);
            close_upvalues(vm, frame->slots);
            vm->frame_count--;
            if (vm->frame_count == initial_frame_count)
            {
                return result;
            }
            vm->stack_top = frame->slots;
            vm_push(vm, result);
        }
        else
        {
            // For now, just return NIL for other instructions
            // A full implementation would handle all opcodes
            break;
        }
    }

    return NIL_VAL;
}

// Helper to call a closure
TaggedValue vm_call_closure(VM* vm, Closure* closure, int arg_count, TaggedValue* args)
{
    // Push arguments onto stack
    for (int i = 0; i < arg_count; i++)
    {
        vm_push(vm, args[i]);
    }

    // Call the closure
    if (call_closure(vm, closure, arg_count) != INTERPRET_OK)
    {
        return NIL_VAL;
    }

    // Run the interpreter until this function returns
    int initial_frame_count = vm->frame_count - 1;
    while (vm->frame_count > initial_frame_count)
    {
        // Simple execution loop - this is a simplified version
        CallFrame* frame = &vm->frames[vm->frame_count - 1];
        uint8_t instruction = *frame->ip++;

        // Handle RETURN specially
        if (instruction == OP_RETURN)
        {
            TaggedValue result = vm_pop(vm);
            close_upvalues(vm, frame->slots);
            vm->frame_count--;
            if (vm->frame_count == initial_frame_count)
            {
                return result;
            }
            vm->stack_top = frame->slots;
            vm_push(vm, result);
        }
        else
        {
            // For now, just return NIL for other instructions
            // A full implementation would handle all opcodes
            break;
        }
    }

    return NIL_VAL;
}

// Print functions that handle the print hook
static void print_object(TaggedValue value)
{
    char buffer[1024];
    if (IS_ARRAY(value))
    {
        Object* array = AS_ARRAY(value);
        snprintf(buffer, sizeof(buffer), "[");
        vm_print_internal(buffer, "", false);

        // TODO: Iterate through array object properties
        vm_print_internal("...", "", false);
        vm_print_internal("]", "", false);
    }
    else if (IS_CLOSURE(value))
    {
        Closure* closure = AS_CLOSURE(value);
        snprintf(buffer, sizeof(buffer), "<fn %s>", closure->function->name);
        vm_print_internal(buffer, "", false);
    }
    else if (IS_FUNCTION(value))
    {
        Function* function = AS_FUNCTION(value);
        snprintf(buffer, sizeof(buffer), "<fn %s>", function->name);
        vm_print_internal(buffer, "", false);
    }
    else if (IS_NATIVE(value))
    {
        vm_print_internal("<native fn>", "", false);
    }
    else if (IS_OBJECT(value))
    {
        Object* obj = AS_OBJECT(value);
        vm_print_internal("{", "", false);

        bool first = true;
        size_t count = 0;
        for (size_t i = 0; i < obj->property_count; i++)
        {
            if (obj->properties[i].value->type != VAL_NIL)
            {
                count++;
            }
        }
        // TODO: Implement object_get_keys
        const char** keys = NULL; // object_get_keys(obj, &count);
        if (keys == NULL)
        {
            vm_print_internal("<object>", "", false);
            return;
        }

        for (size_t i = 0; i < count; i++)
        {
            if (!first)
            {
                vm_print_internal(",", " ", false);
            }
            first = false;

            snprintf(buffer, sizeof(buffer), "\"%s\": ", keys[i]);
            vm_print_internal(buffer, "", false);

            TaggedValue* prop_value_ptr = object_get_property(obj, keys[i]);
            TaggedValue prop_value = prop_value_ptr ? *prop_value_ptr : NIL_VAL;
            print_value(prop_value);
        }

        vm_print_internal("}", "", false);
    }
    else if (false) // IS_STRUCT_TYPE not implemented
    {
        // StructType* type = AS_STRUCT_TYPE(value);
        snprintf(buffer, sizeof(buffer), "<struct>");
        vm_print_internal(buffer, "", false);
    }
    else
    {
        vm_print_internal("<unknown object>", "", false);
    }
}

void print_value(TaggedValue value)
{
    char buffer[32];
    if (IS_NUMBER(value))
    {
        double num = AS_NUMBER(value);
        if (num == (int64_t)num)
        {
            snprintf(buffer, sizeof(buffer), "%lld", (int64_t)num);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "%.6g", num);
        }
        vm_print_internal(buffer, "", false);
    }
    else if (IS_BOOL(value))
    {
        vm_print_internal(AS_BOOL(value) ? "true" : "false", "", false);
    }
    else if (IS_NIL(value))
    {
        vm_print_internal("nil", "", false);
    }
    else if (IS_STRING(value))
    {
        vm_print_internal(AS_STRING(value), "", false);
    }
    else
    {
        print_object(value);
    }
}
