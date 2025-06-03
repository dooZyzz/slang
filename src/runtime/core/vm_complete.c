#include "runtime/core/vm.h"
#include "debug/debug.h"
#include "runtime/modules/loader/module_loader.h"
#include "runtime/modules/lifecycle/builtin_modules.h"
#include "runtime/core/bootstrap.h"
#include "runtime/core/gc.h"
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
typedef struct {
    Object **objects;
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
#define BYTECODE_FREE(ptr, size) SLANG_MEM_FREE(allocators_get(ALLOC_SYSTEM_BYTECODE), ptr, size)
#define BYTECODE_NEW(type) MEM_NEW(allocators_get(ALLOC_SYSTEM_BYTECODE), type)
#define BYTECODE_NEW_ARRAY(type, count) MEM_NEW_ARRAY(allocators_get(ALLOC_SYSTEM_BYTECODE), type, count)

// Global print hook for output redirection
static PrintHook g_print_hook = NULL;

// Forward declarations
static bool is_falsey(TaggedValue value);

// define_global is declared in vm.h

// Object tracking functions
static void object_hash_init(ObjectHash *hash) {
    hash->objects = NULL;
    hash->count = 0;
    hash->capacity = 0;
}

static void object_hash_free(ObjectHash *hash) {
    if (hash->objects) {
        Allocator *alloc = allocators_get(ALLOC_SYSTEM_VM);
        SLANG_MEM_FREE(alloc, hash->objects, hash->capacity * sizeof(Object*));
    }
    hash->objects = NULL;
    hash->count = 0;
    hash->capacity = 0;
}

// String creation helper
static TaggedValue create_string_value(const char *str) {
    return STRING_VAL(STR_DUP(str));
}

// Built-in native functions
static TaggedValue native_print(int arg_count, TaggedValue *args) {
    for (int i = 0; i < arg_count; i++) {
        if (i > 0) printf(" ");
        print_value(args[i]);
    }
    printf("\n");
    return NIL_VAL;
}

static TaggedValue native_typeof(int arg_count, TaggedValue *args) {
    if (arg_count != 1) {
        return create_string_value("error: typeof expects 1 argument");
    }

    switch (args[0].type) {
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

static TaggedValue native_assert(int arg_count, TaggedValue *args) {
    if (arg_count < 1) {
        fprintf(stderr, "Assertion failed: assert() requires at least 1 argument\n");
        exit(1);
    }

    bool condition = !IS_NIL(args[0]) && (!IS_BOOL(args[0]) || AS_BOOL(args[0]));
    if (!condition) {
        if (arg_count > 1 && IS_STRING(args[1])) {
            fprintf(stderr, "Assertion failed: %s\n", AS_STRING(args[1]));
        } else {
            fprintf(stderr, "Assertion failed\n");
        }
        exit(1);
    }

    return NIL_VAL;
}

static void bootstrap_init(VM *vm) {
    // Register built-in functions as globals
    define_global(vm, "print", NATIVE_VAL(native_print));
    define_global(vm, "typeof", NATIVE_VAL(native_typeof));
    define_global(vm, "assert", NATIVE_VAL(native_assert));
}

void chunk_init(Chunk *chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    chunk->constants.count = 0;
    chunk->constants.capacity = 0;
    chunk->constants.values = NULL;
}

void chunk_free(Chunk *chunk) {
    // Free bytecode arrays
    if (chunk->code) {
        BYTECODE_FREE(chunk->code, chunk->capacity);
    }
    if (chunk->lines) {
        BYTECODE_FREE(chunk->lines, chunk->capacity * sizeof(int));
    }

    for (size_t i = 0; i < chunk->constants.count; i++) {
        // Free strings in constants
        // TODO: Proper memory management for different constant types
    }
    if (chunk->constants.values) {
        BYTECODE_FREE(chunk->constants.values, chunk->constants.capacity * sizeof(TaggedValue));
    }

    chunk_init(chunk);
}

void chunk_write(Chunk *chunk, uint8_t byte, int line) {
    // fprintf(stderr, "chunk_write: chunk=%p, byte=%d, count=%zu->%zu\n", 
    //         (void*)chunk, byte, chunk->count, chunk->count + 1);
    if (chunk->capacity < chunk->count + 1) {
        size_t old_capacity = chunk->capacity;
        chunk->capacity = old_capacity < 8 ? 8 : old_capacity * 2;

        // Realloc pattern: allocate-copy-free
        uint8_t *new_code = BYTECODE_ALLOC(chunk->capacity);
        if (chunk->code) {
            memcpy(new_code, chunk->code, old_capacity);
            BYTECODE_FREE(chunk->code, old_capacity);
        }
        chunk->code = new_code;

        int *new_lines = BYTECODE_ALLOC(chunk->capacity * sizeof(int));
        if (chunk->lines) {
            memcpy(new_lines, chunk->lines, old_capacity * sizeof(int));
            BYTECODE_FREE(chunk->lines, old_capacity * sizeof(int));
        }
        chunk->lines = new_lines;
    }

    chunk->code[chunk->count] = byte;
    chunk->lines[chunk->count] = line;
    chunk->count++;
}

int chunk_add_constant(Chunk *chunk, TaggedValue value) {
    // Grow constants array if needed
    if (chunk->constants.count >= chunk->constants.capacity) {
        size_t old_capacity = chunk->constants.capacity;
        chunk->constants.capacity = GROW_CAPACITY(old_capacity);

        // Realloc pattern: allocate-copy-free
        TaggedValue *new_values = BYTECODE_ALLOC(sizeof(TaggedValue) * chunk->constants.capacity);
        if (chunk->constants.values) {
            memcpy(new_values, chunk->constants.values, sizeof(TaggedValue) * old_capacity);
            BYTECODE_FREE(chunk->constants.values, sizeof(TaggedValue) * old_capacity);
        }
        chunk->constants.values = new_values;
    }

    chunk->constants.values[chunk->constants.count] = value;
    return chunk->constants.count++;
}


// Forward declaration
void define_global(VM *vm, const char *name, TaggedValue value);

void vm_init(VM *vm) {
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
    vm->debug_trace = false;
    string_pool_init(&vm->strings);
    object_hash_init(&vm_objects(vm));
    
    // Initialize garbage collector
    GCConfig gc_config = {
        .heap_grow_factor = 2,
        .min_heap_size = 1024 * 1024,  // 1MB
        .max_heap_size = 0,             // Unlimited
        .gc_threshold = 1024 * 1024,    // 1MB
        .incremental = false,
        .incremental_step_size = 1024,
        .stress_test = false,
        .verbose = false
    };
    vm->gc = gc_create(vm, &gc_config);
    
    // Set current VM for object allocation
    object_set_current_vm(vm);
    
    // Initialize built-in prototypes early so extensions can use them
    init_builtin_prototypes();
    
    bootstrap_init(vm); // Initialize built-in functions
    
    // Initialize built-in prototypes and stdlib after VM is fully set up
    // Delay stdlib_init until after VM is fully created
    // stdlib_init(vm);

    vm_has_error(vm) = false;
    vm_error_message(vm)[0] = '\0';
}

void set_print_hook(PrintHook hook) {
    g_print_hook = hook;
}

// Compatibility wrapper for existing code
void vm_set_print_hook(PrintHook hook) {
    set_print_hook(hook);
}

static void vm_print_internal(const char *message, const char *end, bool newline) {
    if (g_print_hook) {
        g_print_hook(message);
    } else {
        if (end && end[0]) {
            printf("%s%s", message, end);
        } else {
            printf("%s", message);
        }
        if (newline) {
            printf("\n");
        }
    }
}

void vm_free(VM *vm) {
    // Destroy garbage collector (will collect all remaining objects)
    if (vm->gc) {
        gc_destroy(vm->gc);
        vm->gc = NULL;
    }
    
    // Free global names and values
    for (size_t i = 0; i < vm->globals.count; i++) {
        if (vm->globals.names[i]) {
            STR_FREE(vm->globals.names[i], strlen(vm->globals.names[i]) + 1);
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
            STR_FREE(vm->struct_types.names[i], strlen(vm->struct_types.names[i]) + 1);
        }
    }
    if (vm->struct_types.names) {
        VM_FREE(vm->struct_types.names, vm->struct_types.capacity * sizeof(char*));
    }
    if (vm->struct_types.types) {
        VM_FREE(vm->struct_types.types, vm->struct_types.capacity * sizeof(StructType*));
    }

    string_pool_free(&vm->strings);
    object_hash_free(&vm_objects(vm));
}

// Initialize a new VM instance
VM *vm_create() {
    VM *vm = VM_NEW(VM);
    if (!vm) {
        return NULL;
    }
    vm_init(vm);
    // module_loader_init(vm); // TODO: Initialize module loader
    builtin_modules_init();
    
    // Initialize stdlib after VM is created
    // Note: stdlib_init initializes prototypes and adds built-in methods
    stdlib_init(vm);
    
    return vm;
}

void vm_init_with_loader(VM *vm, ModuleLoader *loader) {
    // Initialize basic VM state
    vm_init(vm);

    // Set the module loader
    vm->module_loader = loader;
}

// Destroy a VM instance
void vm_destroy(VM *vm) {
    if (vm) {
        // module_loader_cleanup(vm); // TODO: Cleanup module loader
        vm_free(vm);
        VM_FREE(vm, sizeof(VM));
    }
}

static Upvalue *capture_upvalue(VM *vm, TaggedValue *local) {
    Upvalue *prev_upvalue = NULL;
    Upvalue *upvalue = vm->open_upvalues;

    // Walk the list to find an existing upvalue or the insertion point
    while (upvalue != NULL && upvalue->location > local) {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }

    // Found existing upvalue
    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }

    // Create new upvalue
    Upvalue *created_upvalue = VM_NEW(Upvalue);
    created_upvalue->location = local;
    created_upvalue->next = upvalue;
    created_upvalue->closed = NIL_VAL;

    if (prev_upvalue == NULL) {
        vm->open_upvalues = created_upvalue;
    } else {
        prev_upvalue->next = created_upvalue;
    }

    return created_upvalue;
}

static void close_upvalues(VM *vm, TaggedValue *last) {
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last) {
        Upvalue *upvalue = vm->open_upvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm->open_upvalues = upvalue->next;
    }
}

static void vm_runtime_error(VM *vm, const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    // Print stack trace
    fprintf(stderr, "\n[Stack trace]\n");
    for (int i = vm->frame_count - 1; i >= 0; i--) {
        CallFrame *frame = &vm->frames[i];
        size_t instruction = frame->ip - frame->closure->function->chunk.code - 1;
        fprintf(stderr, "  at %s:%d\n", frame->closure->function->name,
                frame->closure->function->chunk.lines[instruction]);
    }

    vm->stack_top = vm->stack; // Reset stack
    vm->frame_count = 0;
}

void vm_push(VM *vm, TaggedValue value) {
    *vm->stack_top = value;
    vm->stack_top++;
}

TaggedValue vm_pop(VM *vm) {
    vm->stack_top--;
    return *vm->stack_top;
}

static TaggedValue vm_peek(VM *vm, int distance) {
    return vm->stack_top[-1 - distance];
}

static void call_frame_push(VM *vm, CallFrame *frame) {
    vm->frames[vm->frame_count++] = *frame;
}

static InterpretResult call_closure(VM *vm, Closure *closure, int arg_count) {
    if (arg_count != closure->function->arity) {
        vm_runtime_error(vm, "Expected %d arguments but got %d.", closure->function->arity, arg_count);
        return INTERPRET_RUNTIME_ERROR;
    }

    if (vm->frame_count >= FRAMES_MAX) {
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
void define_global(VM *vm, const char *name, TaggedValue value) {
    // Check if we need to grow the global arrays
    if (vm->globals.count + 1 > vm->globals.capacity) {
        size_t old_capacity = vm->globals.capacity;
        vm->globals.capacity = old_capacity < 8 ? 8 : old_capacity * 2;

        // Realloc pattern for names array
        char **new_names = VM_NEW_ARRAY(char*, vm->globals.capacity);
        if (vm->globals.names) {
            memcpy(new_names, vm->globals.names, old_capacity * sizeof(char *));
            VM_FREE(vm->globals.names, old_capacity * sizeof(char*));
        }
        vm->globals.names = new_names;

        // Realloc pattern for values array
        TaggedValue *new_values = VM_NEW_ARRAY(TaggedValue, vm->globals.capacity);
        if (vm->globals.values) {
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
void undefine_global(VM *vm, const char *name) {
    for (size_t i = 0; i < vm->globals.count; i++) {
        if (strcmp(vm->globals.names[i], name) == 0) {
            STR_FREE(vm->globals.names[i], strlen(vm->globals.names[i]) + 1);
            // Shift remaining globals down
            for (size_t j = i; j < vm->globals.count - 1; j++) {
                vm->globals.names[j] = vm->globals.names[j + 1];
                vm->globals.values[j] = vm->globals.values[j + 1];
            }
            vm->globals.count--;
            return;
        }
    }
}

// New helper function to convert a value to string
static const char *value_to_string(VM *vm, TaggedValue value);

// Forward declare call_native
static InterpretResult call_native(VM *vm, NativeFn native, int arg_count);

// Forward declare the unified interpreter
static InterpretResult vm_run_frame(VM *vm);

// Value equality comparison
bool values_equal(TaggedValue a, TaggedValue b) {
    if (a.type != b.type) return false;

    switch (a.type) {
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

// Unified interpreter loop - runs the current frame until it returns or errors
static InterpretResult vm_run_frame(VM *vm) {
    CallFrame *frame = &vm->frames[vm->frame_count - 1];

    for (;;) {
        if (vm->debug_trace) {
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
        }

        uint8_t instruction = *frame->ip++;
        switch (instruction) {
            case OP_CONSTANT: {
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

            case OP_DUP: {
                TaggedValue value = vm_peek(vm, 0);
                vm_push(vm, value);
                break;
            }

            case OP_SWAP: {
                TaggedValue a = vm_pop(vm);
                TaggedValue b = vm_pop(vm);
                vm_push(vm, a);
                vm_push(vm, b);
                break;
            }

            case OP_STRING_CONCAT: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);

                // Convert both values to strings using our existing value_to_string helper
                const char* str_a = IS_STRING(a) ? AS_STRING(a) : value_to_string(vm, a);
                const char* str_b = IS_STRING(b) ? AS_STRING(b) : value_to_string(vm, b);
                
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
                break;
            }

            case OP_STRING_INTERP: {
                uint8_t part_count = *frame->ip++;
                size_t total_length = 0;
                size_t* lengths = VM_ALLOC(sizeof(size_t) * part_count);
                const char** parts = VM_ALLOC(sizeof(char*) * part_count);

                // Calculate total length and convert all parts to strings
                for (int i = part_count - 1; i >= 0; i--) {
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
                for (int i = 0; i < part_count; i++) {
                    strcpy(ptr, parts[i]);
                    ptr += lengths[i];
                }

                // Pop all parts
                for (int i = 0; i < part_count; i++) {
                    vm_pop(vm);
                }

                // Intern and push result
                const char* interned = string_pool_intern(&vm->strings, buffer, strlen(buffer));
                STR_FREE(buffer, total_length + 1);
                VM_FREE(lengths, sizeof(size_t) * part_count);
                VM_FREE(parts, sizeof(char*) * part_count);

                vm_push(vm, STRING_VAL(interned));
                break;
            }

            case OP_INTERN_STRING: {
                TaggedValue string_val = vm_pop(vm);
                if (IS_STRING(string_val)) {
                    const char* str = AS_STRING(string_val);
                    const char* interned = string_pool_intern(&vm->strings, str, strlen(str));
                    vm_push(vm, STRING_VAL(interned));
                } else {
                    vm_runtime_error(vm, "Can only intern strings");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_CONSTANT_LONG: {
                uint16_t index = (uint16_t) (*frame->ip++) << 8;
                index |= *frame->ip++;
                TaggedValue constant = frame->closure->function->chunk.constants.values[index];
                vm_push(vm, constant);
                break;
            }

            case OP_EQUAL: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                vm_push(vm, BOOL_VAL(values_equal(a, b)));
                break;
            }

            case OP_NOT_EQUAL: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                vm_push(vm, BOOL_VAL(!values_equal(a, b)));
                break;
            }

            case OP_GREATER: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) > AS_NUMBER(b)));
                } else {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_GREATER_EQUAL: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) >= AS_NUMBER(b)));
                } else {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_LESS: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) < AS_NUMBER(b)));
                } else {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_LESS_EQUAL: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) <= AS_NUMBER(b)));
                } else {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_ADD: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) + AS_NUMBER(b)));
                } else if (IS_STRING(a) && IS_STRING(b)) {
                    const char *str_a = AS_STRING(a);
                    const char *str_b = AS_STRING(b);
                    size_t len_a = strlen(str_a);
                    size_t len_b = strlen(str_b);
                    size_t total_len = len_a + len_b + 1;

                    char *buffer = STR_ALLOC(total_len);
                    strcpy(buffer, str_a);
                    strcat(buffer, str_b);

                    const char *interned = string_pool_intern(&vm->strings, buffer, strlen(buffer));
                    STR_FREE(buffer, total_len);

                    vm_push(vm, STRING_VAL(interned));
                } else {
                    vm_runtime_error(vm, "Operands must be two numbers or two strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_SUBTRACT: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) - AS_NUMBER(b)));
                } else {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_MULTIPLY: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) * AS_NUMBER(b)));
                } else {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_DIVIDE: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    if (AS_NUMBER(b) == 0) {
                        vm_runtime_error(vm, "Division by zero.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) / AS_NUMBER(b)));
                } else {
                    vm_runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_MODULO: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    double divisor = AS_NUMBER(b);
                    if (divisor == 0) {
                        vm_runtime_error(vm, "Division by zero in modulo operation.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    vm_push(vm, NUMBER_VAL(fmod(AS_NUMBER(a), divisor)));
                    break;
                }
                vm_runtime_error(vm, "Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }

            case OP_POWER: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, NUMBER_VAL(pow(AS_NUMBER(a), AS_NUMBER(b))));
                    break;
                }
                vm_runtime_error(vm, "Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }

            case OP_NEGATE: {
                if (!IS_NUMBER(vm_peek(vm, 0))) {
                    vm_runtime_error(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                TaggedValue value = vm_pop(vm);
                vm_push(vm, NUMBER_VAL(-AS_NUMBER(value)));
                break;
            }

            case OP_NOT: {
                TaggedValue value = vm_pop(vm);
                vm_push(vm, BOOL_VAL(is_falsey(value)));
                break;
            }

            case OP_AND: {
                TaggedValue right = vm_pop(vm);
                TaggedValue left = vm_pop(vm);

                // Short-circuit evaluation: if left is falsey, return left, otherwise return right
                if (is_falsey(left)) {
                    vm_push(vm, left);
                } else {
                    vm_push(vm, right);
                }
                break;
            }

            case OP_OR: {
                TaggedValue right = vm_pop(vm);
                TaggedValue left = vm_pop(vm);

                // Short-circuit evaluation: if left is truthy, return left, otherwise return right
                if (!is_falsey(left)) {
                    vm_push(vm, left);
                } else {
                    vm_push(vm, right);
                }
                break;
            }

            case OP_TO_STRING: {
                TaggedValue val = vm_pop(vm);
                if (IS_NIL(val)) {
                    vm_push(vm, STRING_VAL(STR_DUP("nil")));
                } else if (IS_BOOL(val)) {
                    vm_push(vm, STRING_VAL(STR_DUP(AS_BOOL(val) ? "true" : "false")));
                } else if (IS_NUMBER(val)) {
                    char buffer[32];
                    double num = AS_NUMBER(val);
                    if (num == (int64_t) num) {
                        snprintf(buffer, sizeof(buffer), "%ld", (long) num);
                    } else {
                        snprintf(buffer, sizeof(buffer), "%.6g", num);
                    }
                    vm_push(vm, STRING_VAL(STR_DUP(buffer)));
                } else if (IS_STRING(val)) {
                    vm_push(vm, val);
                } else if (IS_NATIVE(val)) {
                    vm_push(vm, STRING_VAL(STR_DUP("<native function>")));
                } else if (IS_OBJECT(val)) {
                    Object *obj = AS_OBJECT(val);
                    Object *proto = object_get_prototype(obj);
                    if (proto != NULL) {
                        TaggedValue *name_val_ptr = object_get_property(proto, "__name__");
                        if (name_val_ptr != NULL && IS_STRING(*name_val_ptr)) {
                            size_t result_size = 1024;
                            char *result = STR_ALLOC(result_size); // Initial buffer
                            snprintf(result, result_size, "<%s instance>", AS_STRING(*name_val_ptr));
                            const char *interned = string_pool_intern(&vm->strings, result, strlen(result));
                            STR_FREE(result, result_size);
                            vm_push(vm, STRING_VAL(interned));
                            break;
                        }
                    }

                    // Check for __struct_type__ property
                    TaggedValue *type_val_ptr = object_get_property(obj, "__struct_type__");
                    if (type_val_ptr != NULL && IS_STRING(*type_val_ptr)) {
                        char buffer[256];
                        snprintf(buffer, sizeof(buffer), "<%s instance>", AS_STRING(*type_val_ptr));
                        vm_push(vm, STRING_VAL(STR_DUP(buffer)));
                    } else {
                        vm_push(vm, STRING_VAL(STR_DUP("<object>")));
                    }
                } else {
                    vm_push(vm, STRING_VAL(STR_DUP("<unknown>")));
                }
                break;
            }

            // OP_PRINT removed - handled by native print function

            // OP_PRINT_EXPR and OP_DEBUG not needed - removed from design

            case OP_JUMP: {
                uint16_t offset = (uint16_t) (*frame->ip++) << 8;
                offset |= *frame->ip++;
                frame->ip += offset;
                break;
            }

            case OP_JUMP_IF_FALSE: {
                uint16_t offset = (uint16_t) (*frame->ip++) << 8;
                offset |= *frame->ip++;
                if (is_falsey(vm_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_JUMP_IF_TRUE: {
                uint16_t offset = (uint16_t) (*frame->ip++) << 8;
                offset |= *frame->ip++;
                if (!is_falsey(vm_peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }

            case OP_LOOP: {
                uint16_t offset = (uint16_t) (*frame->ip++) << 8;
                offset |= *frame->ip++;
                frame->ip -= offset;
                break;
            }

            case OP_GET_LOCAL: {
                uint8_t slot = *frame->ip++;
                vm_push(vm, frame->slots[slot]);
                break;
            }

            case OP_SET_LOCAL: {
                uint8_t slot = *frame->ip++;
                frame->slots[slot] = vm_peek(vm, 0);
                break;
            }

            case OP_GET_GLOBAL: {
                uint8_t name_index = *frame->ip++;
                const char *name = AS_STRING(frame->closure->function->chunk.constants.values[name_index]);

                // Look in current module first
                if (vm->current_module) {
                    for (size_t i = 0; i < vm->current_module->globals.count; i++) {
                        if (strcmp(vm->current_module->globals.names[i], name) == 0) {
                            vm_push(vm, vm->current_module->globals.values[i]);
                            goto found_global;
                        }
                    }
                }

                // Then look in VM globals
                for (size_t i = 0; i < vm->globals.count; i++) {
                    if (strcmp(vm->globals.names[i], name) == 0) {
                        vm_push(vm, vm->globals.values[i]);
                        goto found_global;
                    }
                }

                vm_runtime_error(vm, "Undefined global variable '%s'.", name);
                return INTERPRET_RUNTIME_ERROR;

            found_global:
                break;
            }

            case OP_SET_GLOBAL: {
                uint8_t name_index = *frame->ip++;
                const char *name = AS_STRING(frame->closure->function->chunk.constants.values[name_index]);
                TaggedValue value = vm_peek(vm, 0);

                // Check if we're setting a module-level global
                if (vm->current_module) {
                    // Check if it already exists in module
                    for (size_t i = 0; i < vm->current_module->globals.count; i++) {
                        if (strcmp(vm->current_module->globals.names[i], name) == 0) {
                            vm->current_module->globals.values[i] = value;
                            goto global_set;
                        }
                    }

                    // Add to module globals
                    if (vm->current_module->globals.count + 1 > vm->current_module->globals.capacity) {
                        size_t old_capacity = vm->current_module->globals.capacity;
                        size_t new_capacity = old_capacity < 8 ? 8 : old_capacity * 2;

                        // Realloc pattern for names
                        char **new_names = VM_ALLOC(new_capacity * sizeof(char*));
                        if (vm->current_module->globals.names) {
                            memcpy(new_names, vm->current_module->globals.names, old_capacity * sizeof(char *));
                            MODULE_FREE(vm->current_module->globals.names, old_capacity * sizeof(char*));
                        }
                        vm->current_module->globals.names = new_names;

                        // Realloc pattern for values
                        TaggedValue *new_values = VM_ALLOC(new_capacity * sizeof(TaggedValue));
                        if (vm->current_module->globals.values) {
                            memcpy(new_values, vm->current_module->globals.values, old_capacity * sizeof(TaggedValue));
                            MODULE_FREE(vm->current_module->globals.values, old_capacity * sizeof(TaggedValue));
                        }
                        vm->current_module->globals.values = new_values;

                        vm->current_module->globals.capacity = new_capacity;
                    }

                    vm->current_module->globals.names[vm->current_module->globals.count] = MODULE_DUP(name);
                    vm->current_module->globals.values[vm->current_module->globals.count] = value;
                    vm->current_module->globals.count++;
                } else {
                    // Setting global in VM
                    for (size_t i = 0; i < vm->globals.count; i++) {
                        if (strcmp(vm->globals.names[i], name) == 0) {
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

            case OP_DEFINE_GLOBAL: {
                uint8_t name_index = *frame->ip++;
                const char *name = AS_STRING(frame->closure->function->chunk.constants.values[name_index]);
                TaggedValue value = vm_pop(vm);

                if (vm->current_module) {
                    // Module-level global
                    if (vm->current_module->globals.count + 1 > vm->current_module->globals.capacity) {
                        size_t old_capacity = vm->current_module->globals.capacity;
                        size_t new_capacity = old_capacity < 8 ? 8 : old_capacity * 2;

                        // Realloc pattern for names
                        char **new_names = VM_ALLOC(new_capacity * sizeof(char*));
                        if (vm->current_module->globals.names) {
                            memcpy(new_names, vm->current_module->globals.names, old_capacity * sizeof(char *));
                            MODULE_FREE(vm->current_module->globals.names, old_capacity * sizeof(char*));
                        }
                        vm->current_module->globals.names = new_names;

                        // Realloc pattern for values
                        TaggedValue *new_values = VM_ALLOC(new_capacity * sizeof(TaggedValue));
                        if (vm->current_module->globals.values) {
                            memcpy(new_values, vm->current_module->globals.values, old_capacity * sizeof(TaggedValue));
                            MODULE_FREE(vm->current_module->globals.values, old_capacity * sizeof(TaggedValue));
                        }
                        vm->current_module->globals.values = new_values;

                        vm->current_module->globals.capacity = new_capacity;
                    }

                    vm->current_module->globals.names[vm->current_module->globals.count] = MODULE_DUP(name);
                    vm->current_module->globals.values[vm->current_module->globals.count] = value;
                    vm->current_module->globals.count++;
                } else {
                    // VM-level global
                    define_global(vm, name, value);
                }
                break;
            }

            case OP_ARRAY:
            case OP_BUILD_ARRAY: {
                uint8_t count = *frame->ip++;
                Object *array = array_create();  // Use array_create to get proper prototype

                // Pop values in reverse order and set as indexed properties
                for (int i = count - 1; i >= 0; i--) {
                    TaggedValue value = vm_pop(vm);
                    char index_str[32];
                    snprintf(index_str, sizeof(index_str), "%d", i);
                    object_set_property(array, index_str, value);
                }
                
                // Update length after adding elements
                object_set_property(array, "length", NUMBER_VAL((double)count));

                vm_push(vm, OBJECT_VAL(array));
                break;
            }

            case OP_GET_SUBSCRIPT: {
                TaggedValue index = vm_pop(vm);
                TaggedValue collection = vm_pop(vm);

                if (IS_OBJECT(collection)) {
                    Object *obj = AS_OBJECT(collection);
                    
                    if (IS_NUMBER(index)) {
                        // Array-like access using numeric index
                        char index_str[32];
                        snprintf(index_str, sizeof(index_str), "%d", (int)AS_NUMBER(index));
                        TaggedValue *value_ptr = object_get_property(obj, index_str);
                        vm_push(vm, value_ptr ? *value_ptr : NIL_VAL);
                    } else if (IS_STRING(index)) {
                        // Object property access
                        const char *property_name = AS_STRING(index);
                        TaggedValue *value_ptr = object_get_property(obj, property_name);
                        vm_push(vm, value_ptr ? *value_ptr : NIL_VAL);
                    } else {
                        vm_runtime_error(vm, "Index must be number or string.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else if (IS_STRING(collection)) {
                    if (!IS_NUMBER(index)) {
                        vm_runtime_error(vm, "String index must be a number.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    const char *str = AS_STRING(collection);
                    int i = (int) AS_NUMBER(index);
                    size_t len = strlen(str);

                    if (i < 0 || i >= (int) len) {
                        vm_runtime_error(vm, "String index %d out of bounds (length: %zu).", i, len);
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    char char_str[2] = {str[i], '\0'};
                    const char *interned = string_pool_intern(&vm->strings, char_str, 2);
                    vm_push(vm, STRING_VAL(interned));
                } else {
                    vm_runtime_error(vm, "Cannot index into non-collection type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_SET_SUBSCRIPT: {
                TaggedValue value = vm_pop(vm);
                TaggedValue index = vm_pop(vm);
                TaggedValue collection = vm_pop(vm);

                if (IS_OBJECT(collection)) {
                    Object *obj = AS_OBJECT(collection);
                    
                    if (IS_NUMBER(index)) {
                        // Array-like access using numeric index
                        char index_str[32];
                        int idx = (int)AS_NUMBER(index);
                        snprintf(index_str, sizeof(index_str), "%d", idx);
                        object_set_property(obj, index_str, value);
                        
                        // Update length if necessary
                        TaggedValue *length_ptr = object_get_property(obj, "length");
                        if (length_ptr && IS_NUMBER(*length_ptr)) {
                            int current_length = (int)AS_NUMBER(*length_ptr);
                            if (idx >= current_length) {
                                object_set_property(obj, "length", NUMBER_VAL((double)(idx + 1)));
                            }
                        }
                        
                        vm_push(vm, value);
                    } else if (IS_STRING(index)) {
                        // Object property access
                        const char *property_name = AS_STRING(index);
                        object_set_property(obj, property_name, value);
                        vm_push(vm, value);
                    } else {
                        vm_runtime_error(vm, "Index must be number or string.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    vm_runtime_error(vm, "Cannot set element on non-object type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_LENGTH: {
                TaggedValue value = vm_pop(vm);
                if (IS_STRING(value)) {
                    vm_push(vm, NUMBER_VAL((double)strlen(AS_STRING(value))));
                } else if (IS_OBJECT(value)) {
                    // Check for length property (arrays store their length as a property)
                    Object* obj = AS_OBJECT(value);
                    TaggedValue* len_ptr = object_get_property(obj, "length");
                    if (len_ptr != NULL) {
                        vm_push(vm, *len_ptr);
                    } else {
                        vm_runtime_error(vm, "Object has no length property.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    vm_runtime_error(vm, "Cannot get length of non-collection type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            // OP_ARRAY_PUSH removed - handled as a method on array prototype

            case OP_METHOD_CALL: {
                uint8_t arg_count = *frame->ip++;
                uint8_t method_name_index = *frame->ip++;
                const char* method_name = AS_STRING(frame->closure->function->chunk.constants.values[method_name_index]);
                
                // Get the receiver object (it's at position arg_count from top)
                TaggedValue receiver = vm_peek(vm, arg_count);
                
                // Look up the method on the object or its prototype chain
                TaggedValue method = NIL_VAL;
                
                if (IS_OBJECT(receiver)) {
                    Object* obj = AS_OBJECT(receiver);
                    TaggedValue* method_ptr = object_get_property(obj, method_name);
                    if (method_ptr) {
                        method = *method_ptr;
                    }
                } else if (IS_STRING(receiver)) {
                    // String methods from string prototype
                    Object* string_proto = get_string_prototype();
                    if (string_proto) {
                        TaggedValue* method_ptr = object_get_property(string_proto, method_name);
                        if (method_ptr) {
                            method = *method_ptr;
                        }
                    }
                } else if (IS_NUMBER(receiver)) {
                    // Number methods from number prototype
                    Object* number_proto = get_number_prototype();
                    if (number_proto) {
                        TaggedValue* method_ptr = object_get_property(number_proto, method_name);
                        if (method_ptr) {
                            method = *method_ptr;
                        }
                    }
                }
                
                if (IS_NIL(method)) {
                    vm_runtime_error(vm, "Undefined method '%s'.", method_name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Replace receiver with method on stack
                vm->stack_top[-arg_count - 1] = method;
                
                // Call the method
                if (IS_CLOSURE(method)) {
                    Closure *closure = AS_CLOSURE(method);
                    InterpretResult result = call_closure(vm, closure, arg_count);
                    if (result != INTERPRET_OK) {
                        return result;
                    }
                    frame = &vm->frames[vm->frame_count - 1];
                } else if (IS_NATIVE(method)) {
                    NativeFn native = AS_NATIVE(method);
                    InterpretResult result = call_native(vm, native, arg_count);
                    if (result != INTERPRET_OK) {
                        return result;
                    }
                } else {
                    vm_runtime_error(vm, "Invalid method type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_CALL: {
                uint8_t arg_count = *frame->ip++;
                TaggedValue callee = vm_peek(vm, arg_count);

                if (IS_CLOSURE(callee)) {
                    Closure *closure = AS_CLOSURE(callee);
                    InterpretResult result = call_closure(vm, closure, arg_count);
                    if (result != INTERPRET_OK) {
                        return result;
                    }
                    frame = &vm->frames[vm->frame_count - 1];
                } else if (IS_NATIVE(callee)) {
                    NativeFn native = AS_NATIVE(callee);
                    InterpretResult result = call_native(vm, native, arg_count);
                    if (result != INTERPRET_OK) {
                        return result;
                    }
                } else {
                    vm_runtime_error(vm, "Can only call functions.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_RETURN: {
                TaggedValue result = vm_pop(vm);
                close_upvalues(vm, frame->slots);
                vm->frame_count--;
                if (vm->frame_count == 0) {
                    vm_pop(vm); // Pop the script function
                    return INTERPRET_OK;
                }

                vm->stack_top = frame->slots;
                vm_push(vm, result);
                frame = &vm->frames[vm->frame_count - 1];
                break;
            }

            case OP_CLOSURE: {
                uint8_t function_index = *frame->ip++;
                Function *function = AS_FUNCTION(frame->closure->function->chunk.constants.values[function_index]);
                Closure *closure = BYTECODE_NEW(Closure);
                closure->function = function;
                closure->upvalue_count = function->upvalue_count;

                if (closure->upvalue_count > 0) {
                    closure->upvalues = BYTECODE_NEW_ARRAY(Upvalue*, closure->upvalue_count);
                    for (int i = 0; i < closure->upvalue_count; i++) {
                        uint8_t is_local = *frame->ip++;
                        uint8_t index = *frame->ip++;
                        if (is_local) {
                            closure->upvalues[i] = capture_upvalue(vm, frame->slots + index);
                        } else {
                            closure->upvalues[i] = frame->closure->upvalues[index];
                        }

                        // Check for NULL upvalue (shouldn't happen in well-formed bytecode)
                        if (closure->upvalues[i] == NULL) {
                            vm_runtime_error(vm, "Failed to capture upvalue %d for closure.", i);
                            // Free what we've allocated so far
                            for (int j = 0; j < i; j++) {
                                // Upvalues themselves are managed by the VM
                            }
                            BYTECODE_FREE(closure->upvalues, sizeof(Upvalue*) * closure->upvalue_count);
                            BYTECODE_FREE(closure, sizeof(Closure));
                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                } else {
                    closure->upvalues = NULL;
                }

                vm_push(vm, (TaggedValue){VAL_CLOSURE, {.closure = closure}});
                break;
            }

            case OP_CLOSURE_LONG: {
                // Read 24-bit constant index
                uint32_t function_index = (*frame->ip++) << 16;
                function_index |= (*frame->ip++) << 8;
                function_index |= *frame->ip++;
                
                Function *function = AS_FUNCTION(frame->closure->function->chunk.constants.values[function_index]);
                Closure *closure = BYTECODE_NEW(Closure);
                closure->function = function;
                closure->upvalue_count = function->upvalue_count;

                if (closure->upvalue_count > 0) {
                    closure->upvalues = BYTECODE_NEW_ARRAY(Upvalue*, closure->upvalue_count);
                    for (int i = 0; i < closure->upvalue_count; i++) {
                        uint8_t is_local = *frame->ip++;
                        uint8_t index = *frame->ip++;
                        if (is_local) {
                            closure->upvalues[i] = capture_upvalue(vm, frame->slots + index);
                        } else {
                            closure->upvalues[i] = frame->closure->upvalues[index];
                        }

                        // Check for NULL upvalue (shouldn't happen in well-formed bytecode)
                        if (closure->upvalues[i] == NULL) {
                            vm_runtime_error(vm, "Failed to capture upvalue %d for closure.", i);
                            // Free what we've allocated so far
                            for (int j = 0; j < i; j++) {
                                // Upvalues themselves are managed by the VM
                            }
                            BYTECODE_FREE(closure->upvalues, sizeof(Upvalue*) * closure->upvalue_count);
                            BYTECODE_FREE(closure, sizeof(Closure));
                            return INTERPRET_RUNTIME_ERROR;
                        }
                    }
                } else {
                    closure->upvalues = NULL;
                }

                vm_push(vm, (TaggedValue){VAL_CLOSURE, {.closure = closure}});
                break;
            }

            case OP_GET_UPVALUE: {
                uint8_t slot = *frame->ip++;
                if (slot >= frame->closure->upvalue_count) {
                    vm_runtime_error(vm, "Invalid upvalue index %d (closure has %d upvalues).",
                                     slot, frame->closure->upvalue_count);
                    return INTERPRET_RUNTIME_ERROR;
                }
                Upvalue *upvalue = frame->closure->upvalues[slot];
                if (upvalue == NULL) {
                    vm_runtime_error(vm, "Upvalue %d is NULL.", slot);
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm_push(vm, *upvalue->location);
                break;
            }

            case OP_SET_UPVALUE: {
                uint8_t slot = *frame->ip++;
                if (slot >= frame->closure->upvalue_count) {
                    vm_runtime_error(vm, "Invalid upvalue index %d (closure has %d upvalues).",
                                     slot, frame->closure->upvalue_count);
                    return INTERPRET_RUNTIME_ERROR;
                }
                Upvalue *upvalue = frame->closure->upvalues[slot];
                if (upvalue == NULL) {
                    vm_runtime_error(vm, "Upvalue %d is NULL.", slot);
                    return INTERPRET_RUNTIME_ERROR;
                }
                *upvalue->location = vm_peek(vm, 0);
                break;
            }

            case OP_CLOSE_UPVALUE: {
                close_upvalues(vm, vm->stack_top - 1);
                vm_pop(vm);
                break;
            }

            // OP_STRUCT and OP_CONSTRUCT removed - use OP_DEFINE_STRUCT and OP_CREATE_STRUCT instead

            case OP_CREATE_OBJECT: {
                // Create an empty object
                Object* obj = object_create();
                vm_push(vm, OBJECT_VAL(obj));
                break;
            }

            case OP_GET_PROPERTY: {
                TaggedValue name_val = vm_pop(vm);
                TaggedValue object_val = vm_pop(vm);

                if (!IS_STRING(name_val)) {
                    vm_runtime_error(vm, "Property name must be a string.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                const char *property_name = AS_STRING(name_val);

                if (IS_OBJECT(object_val)) {
                    Object *obj = AS_OBJECT(object_val);
                    TaggedValue *value_ptr = object_get_property(obj, property_name);
                    if (value_ptr) {
                        vm_push(vm, *value_ptr);
                    } else {
                        vm_push(vm, NIL_VAL);
                    }
                } else if (IS_STRING(object_val)) {
                    // Handle string properties
                    const char *str = AS_STRING(object_val);
                    if (strcmp(property_name, "length") == 0) {
                        vm_push(vm, NUMBER_VAL((double)strlen(str)));
                    } else if (strcmp(property_name, "substring") == 0) {
                        vm_push(vm, NIL_VAL); // TODO: native_string_substring not implemented
                    } else if (strcmp(property_name, "indexOf") == 0) {
                        vm_push(vm, NIL_VAL); // TODO: native_string_index_of not implemented
                    } else if (strcmp(property_name, "split") == 0) {
                        vm_push(vm, NIL_VAL); // TODO: native_string_split not implemented
                    } else if (strcmp(property_name, "replace") == 0) {
                        vm_push(vm, NIL_VAL); // TODO: native_string_replace not implemented
                    } else if (strcmp(property_name, "toLowerCase") == 0) {
                        vm_push(vm, NIL_VAL); // TODO: native_string_to_lower_case not implemented
                    } else if (strcmp(property_name, "toUpperCase") == 0) {
                        vm_push(vm, NIL_VAL); // TODO: native_string_to_upper_case not implemented
                    } else if (strcmp(property_name, "trim") == 0) {
                        vm_push(vm, NIL_VAL); // TODO: native_string_trim not implemented
                    } else if (strcmp(property_name, "startsWith") == 0) {
                        vm_push(vm, NIL_VAL); // TODO: native_string_starts_with not implemented
                    } else if (strcmp(property_name, "endsWith") == 0) {
                        vm_push(vm, NIL_VAL); // TODO: native_string_ends_with not implemented
                    } else if (strcmp(property_name, "repeat") == 0) {
                        vm_push(vm, NIL_VAL); // TODO: native_string_repeat not implemented
                    } else {
                        vm_push(vm, NIL_VAL);
                    }
                } else if (IS_NUMBER(object_val)) {
                    // Handle number properties by looking them up on Number.prototype
                    Object* number_proto = get_number_prototype();
                    if (number_proto) {
                        TaggedValue* method = object_get_property(number_proto, property_name);
                        if (method) {
                            vm_push(vm, *method);
                        } else {
                            vm_push(vm, NIL_VAL);
                        }
                    } else {
                        vm_push(vm, NIL_VAL);
                    }
                } else {
                    vm_runtime_error(vm, "Only objects have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_SET_PROPERTY: {
                TaggedValue value = vm_pop(vm);
                TaggedValue name_val = vm_pop(vm);
                TaggedValue object_val = vm_pop(vm);

                if (!IS_OBJECT(object_val)) {
                    vm_runtime_error(vm, "Only objects have properties.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!IS_STRING(name_val)) {
                    vm_runtime_error(vm, "Property name must be a string.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                Object *obj = AS_OBJECT(object_val);
                const char *property_name = AS_STRING(name_val);
                object_set_property(obj, property_name, value);
                vm_push(vm, value);
                break;
            }

            case OP_OBJECT_LITERAL: {
                uint8_t property_count = *frame->ip++;
                Object* obj = object_create();

                // Properties are on the stack as: value, key, value, key, ...
                for (int i = 0; i < property_count; i++) {
                    TaggedValue value = vm_pop(vm);
                    TaggedValue key = vm_pop(vm);

                    if (!IS_STRING(key)) {
                        vm_runtime_error(vm, "Object property key must be a string.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    object_set_property(obj, AS_STRING(key), value);
                }

                vm_push(vm, OBJECT_VAL(obj));
                break;
            }

            // OP_IMPORT and OP_EXPORT removed - use OP_LOAD_MODULE, OP_IMPORT_FROM, OP_MODULE_EXPORT instead

            case OP_LOAD_MODULE: {
                uint8_t path_index = *frame->ip++;
                const char* module_path = AS_STRING(frame->closure->function->chunk.constants.values[path_index]);
                
                // Load the module through the module loader
                if (vm->module_loader) {
                    // Detect native modules by $ prefix
                    bool is_native = (module_path[0] == '$');
                    Module* module = module_load(vm->module_loader, module_path, is_native);
                    if (module) {
                        // Ensure module is initialized
                        if (ensure_module_initialized(module, vm)) {
                            // Push module object onto stack
                            if (module->module_object) {
                                vm_push(vm, OBJECT_VAL(module->module_object));
                            } else {
                                // Module has no exports object - push empty object
                                Object* empty_obj = object_create();
                                vm_push(vm, OBJECT_VAL(empty_obj));
                            }
                        } else {
                            vm_runtime_error(vm, "Failed to initialize module: %s", module_path);
                            return INTERPRET_RUNTIME_ERROR;
                        }
                    } else {
                        vm_runtime_error(vm, "Failed to load module: %s", module_path);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    vm_runtime_error(vm, "No module loader available");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_IMPORT_FROM: {
                // Stack: [module_object]
                // Bytecode: OP_IMPORT_FROM <name_index>
                TaggedValue module_val = vm_pop(vm);
                uint8_t name_index = *frame->ip++;
                const char* import_name = AS_STRING(frame->closure->function->chunk.constants.values[name_index]);
                
                if (!IS_OBJECT(module_val)) {
                    vm_runtime_error(vm, "Cannot import from non-object");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                Object* module_obj = AS_OBJECT(module_val);
                TaggedValue* exported_value = object_get_property(module_obj, import_name);
                
                if (exported_value) {
                    vm_push(vm, *exported_value);
                } else {
                    vm_runtime_error(vm, "Module does not export '%s'", import_name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

            case OP_MODULE_EXPORT: {
                // Stack: [value]
                // Bytecode: OP_MODULE_EXPORT <name_index>
                TaggedValue value = vm_peek(vm, 0); // Don't pop, leave on stack
                uint8_t name_index = *frame->ip++;
                const char* export_name = AS_STRING(frame->closure->function->chunk.constants.values[name_index]);
                
                if (vm->current_module) {
                    // Add to module exports
                    module_export(vm->current_module, export_name, value);
                }
                break;
            }

            case OP_GET_OBJECT_PROTO: {
                // Get the prototype for a built-in type to register extension methods
                // Stack: [] -> [prototype]
                uint8_t type_id = *frame->ip++;
                Object* prototype = NULL;
                
                switch (type_id) {
                    case 0: // Object
                        prototype = get_object_prototype();
                        break;
                    case 1: // Array
                        prototype = get_array_prototype();
                        break;
                    case 2: // String
                        prototype = get_string_prototype();
                        break;
                    case 3: // Number
                        prototype = get_number_prototype();
                        break;
                    case 4: // Function
                        prototype = get_function_prototype();
                        break;
                    default:
                        vm_runtime_error(vm, "Unknown built-in type ID: %d", type_id);
                        return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!prototype) {
                    // Debug: check if prototypes are initialized
                    fprintf(stderr, "[DEBUG] Prototype is NULL for type_id=%d\n", type_id);
                    fprintf(stderr, "[DEBUG] object_prototype=%p, array_prototype=%p, string_prototype=%p, number_prototype=%p\n",
                            get_object_prototype(), get_array_prototype(), get_string_prototype(), get_number_prototype());
                    vm_runtime_error(vm, "Failed to get prototype for type ID: %d", type_id);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                vm_push(vm, OBJECT_VAL(prototype));
                break;
            }
            
            case OP_GET_STRUCT_PROTO: {
                // Get the prototype for a struct type to register extension methods
                // Stack: [] -> [prototype]
                uint8_t name_index = *frame->ip++;
                const char* struct_name = AS_STRING(frame->closure->function->chunk.constants.values[name_index]);
                
                // Get or create the struct prototype
                Object* prototype = get_struct_prototype(struct_name);
                if (!prototype) {
                    vm_runtime_error(vm, "Failed to get prototype for struct: %s", struct_name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                vm_push(vm, OBJECT_VAL(prototype));
                break;
            }

            default:
                vm_runtime_error(vm, "Unknown opcode %d.", instruction);
                return INTERPRET_RUNTIME_ERROR;
        }
    }
}

// Original vm_interpret for compatibility
InterpretResult vm_interpret(VM *vm, Chunk *chunk) {
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

InterpretResult vm_interpret_function(VM *vm, Function *function) {
    Closure closure;
    closure.function = function;
    closure.upvalues = NULL;
    closure.upvalue_count = 0;

    vm_push(vm, (TaggedValue){VAL_CLOSURE, {.closure = &closure}});

    CallFrame *frame = &vm->frames[vm->frame_count];
    frame->closure = &closure;
    frame->ip = function->chunk.code;
    frame->slots = vm->stack;
    vm->frame_count = 1;

    return vm_run_frame(vm);
}

Function *function_new(const char *name) {
    Function *function = BYTECODE_NEW(Function);
    function->name = STR_DUP(name);
    function->arity = 0;
    function->upvalue_count = 0;
    chunk_init(&function->chunk);
    return function;
}

void function_free(Function *function) {
    if (function) {
        chunk_free(&function->chunk);
        STR_FREE((void*)function->name, strlen(function->name) + 1);
        BYTECODE_FREE(function, sizeof(Function));
    }
}

static bool is_falsey(TaggedValue value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// Enhanced call_native with better array method support
static InterpretResult call_native(VM *vm, NativeFn native, int arg_count) {
    // For array methods, we need to include the array itself as an argument
    TaggedValue *args = vm->stack_top - arg_count;
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
static const char *value_to_string(VM *vm, TaggedValue value) {
    static char buffer[256]; // Static buffer for simple conversions

    if (IS_STRING(value)) {
        return AS_STRING(value);
    } else if (IS_NUMBER(value)) {
        double num = AS_NUMBER(value);
        if (num == (int64_t) num) {
            snprintf(buffer, sizeof(buffer), "%ld", (long) num);
        } else {
            snprintf(buffer, sizeof(buffer), "%.6g", num);
        }
        return buffer;
    } else if (IS_BOOL(value)) {
        return AS_BOOL(value) ? "true" : "false";
    } else if (IS_NIL(value)) {
        return "nil";
    } else if (IS_OBJECT(value)) {
        // Check for __struct_type__
        Object *obj = AS_OBJECT(value);
        TaggedValue *type_val_ptr = object_get_property(obj, "__struct_type__");
        if (type_val_ptr != NULL && IS_STRING(*type_val_ptr)) {
            snprintf(buffer, sizeof(buffer), "<%s instance>", AS_STRING(*type_val_ptr));
            return buffer;
        }
        return "<object>";
    } else {
        return "<unknown>";
    }
}

// CLI helper functions
void vm_push_string(VM *vm, const char *str) {
    const char *interned = string_pool_intern(&vm->strings, str, strlen(str));
    vm_push(vm, STRING_VAL(interned));
}

void vm_define_native(VM *vm, const char *name, NativeFn function) {
    vm_push_string(vm, name);
    vm_push(vm, NATIVE_VAL(function));
    define_global(vm, name, vm->stack_top[-1]);
    vm_pop(vm);
    vm_pop(vm);
}

// Basic introspection
void vm_list_globals(VM *vm) {
    printf("Global variables:\n");
    for (size_t i = 0; i < vm->globals.count; i++) {
        printf("  %s: ", vm->globals.names[i]);
        print_value(vm->globals.values[i]);
        printf("\n");
    }
}

TaggedValue vm_get_global(VM *vm, const char *name) {
    for (size_t i = 0; i < vm->globals.count; i++) {
        if (strcmp(vm->globals.names[i], name) == 0) {
            return vm->globals.values[i];
        }
    }
    return NIL_VAL;
}

// Function creation and destruction
Function *function_create(const char *name, int arity) {
    Allocator *alloc = allocators_get(ALLOC_SYSTEM_VM);
    Allocator *str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);

    Function *function = VM_NEW(Function);
    function->name = STR_DUP(name);
    function->arity = arity;
    function->upvalue_count = 0;
    function->module = NULL; // Will be set when function is defined in a module
    chunk_init(&function->chunk);
    return function;
}

// Forward declarations for vm_call_value (already declared in header)

// Call a value (function, closure, or native)
TaggedValue vm_call_value(VM *vm, TaggedValue callee, int arg_count, TaggedValue *args) {
    if (IS_FUNCTION(callee)) {
        return vm_call_function(vm, AS_FUNCTION(callee), arg_count, args);
    } else if (IS_CLOSURE(callee)) {
        return vm_call_closure(vm, AS_CLOSURE(callee), arg_count, args);
    } else if (IS_NATIVE(callee)) {
        // Direct native call
        NativeFn native = AS_NATIVE(callee);
        return native(arg_count, args);
    } else {
        // Not callable
        return NIL_VAL;
    }
}

// Helper to call a function
TaggedValue vm_call_function(VM *vm, Function *function, int arg_count, TaggedValue *args) {
    // Push the function as a closure onto the stack
    Closure closure;
    closure.function = function;
    closure.upvalue_count = 0;
    closure.upvalues = NULL;
    vm_push(vm, (TaggedValue){VAL_CLOSURE, {.closure = &closure}});

    // Push arguments onto stack
    for (int i = 0; i < arg_count; i++) {
        vm_push(vm, args[i]);
    }

    // Call the closure
    if (call_closure(vm, &closure, arg_count) != INTERPRET_OK) {
        return NIL_VAL;
    }

    // Run the interpreter until this function returns
    int initial_frame_count = vm->frame_count - 1;
    InterpretResult result = vm_run_frame(vm);
    
    // The result should be on top of the stack
    if (result == INTERPRET_OK && vm->frame_count == initial_frame_count) {
        return vm_pop(vm);
    }
    
    return NIL_VAL;
}

// Helper to call a closure
TaggedValue vm_call_closure(VM *vm, Closure *closure, int arg_count, TaggedValue *args) {
    // Push the closure onto the stack
    vm_push(vm, (TaggedValue){VAL_CLOSURE, {.closure = closure}});

    // Push arguments onto stack
    for (int i = 0; i < arg_count; i++) {
        vm_push(vm, args[i]);
    }

    // Call the closure
    if (call_closure(vm, closure, arg_count) != INTERPRET_OK) {
        return NIL_VAL;
    }

    // Run the interpreter until this function returns
    int initial_frame_count = vm->frame_count - 1;
    InterpretResult result = vm_run_frame(vm);
    
    // The result should be on top of the stack
    if (result == INTERPRET_OK && vm->frame_count == initial_frame_count) {
        return vm_pop(vm);
    }
    
    return NIL_VAL;
}

// Print functions that handle the print hook
static void print_object(TaggedValue value) {
    char buffer[1024];
    if (IS_OBJECT(value)) {
        Object *obj = AS_OBJECT(value);
        
        // Check if this is an array-like object (has length property)
        TaggedValue *length_ptr = object_get_property(obj, "length");
        if (length_ptr && IS_NUMBER(*length_ptr)) {
            snprintf(buffer, sizeof(buffer), "[");
            vm_print_internal(buffer, "", false);

            // TODO: Iterate through array object properties
            vm_print_internal("...", "", false);
            vm_print_internal("]", "", false);
        } else {
            // Regular object
            vm_print_internal("{", "", false);

            bool first = true;
            size_t count = 0;
            for (size_t i = 0; i < obj->property_count; i++) {
                if (obj->properties[i].value->type != VAL_NIL) {
                    count++;
                }
            }
            // TODO: Implement object_get_keys
            const char **keys = NULL; // object_get_keys(obj, &count);
            if (keys == NULL) {
                vm_print_internal("<object>", "", false);
                vm_print_internal("}", "", false);
                return;
            }

            for (size_t i = 0; i < count; i++) {
                if (!first) {
                    vm_print_internal(",", " ", false);
                }
                first = false;

                snprintf(buffer, sizeof(buffer), "\"%s\": ", keys[i]);
                vm_print_internal(buffer, "", false);

                TaggedValue *prop_value_ptr = object_get_property(obj, keys[i]);
                TaggedValue prop_value = prop_value_ptr ? *prop_value_ptr : NIL_VAL;
                print_value(prop_value);
            }

            vm_print_internal("}", "", false);
        }
    } else if (IS_CLOSURE(value)) {
        Closure *closure = AS_CLOSURE(value);
        snprintf(buffer, sizeof(buffer), "<fn %s>", closure->function->name);
        vm_print_internal(buffer, "", false);
    } else if (IS_FUNCTION(value)) {
        Function *function = AS_FUNCTION(value);
        snprintf(buffer, sizeof(buffer), "<fn %s>", function->name);
        vm_print_internal(buffer, "", false);
    } else if (IS_NATIVE(value)) {
        vm_print_internal("<native fn>", "", false);
    } else if (false) // IS_STRUCT_TYPE not implemented
    {
        // StructType* type = AS_STRUCT_TYPE(value);
        snprintf(buffer, sizeof(buffer), "<struct>");
        vm_print_internal(buffer, "", false);
    } else {
        vm_print_internal("<unknown object>", "", false);
    }
}

void print_value(TaggedValue value) {
    char buffer[32];
    if (IS_NUMBER(value)) {
        double num = AS_NUMBER(value);
        if (num == (int64_t) num) {
            snprintf(buffer, sizeof(buffer), "%ld", (long) num);
        } else {
            snprintf(buffer, sizeof(buffer), "%.6g", num);
        }
        vm_print_internal(buffer, "", false);
    } else if (IS_BOOL(value)) {
        vm_print_internal(AS_BOOL(value) ? "true" : "false", "", false);
    } else if (IS_NIL(value)) {
        vm_print_internal("nil", "", false);
    } else if (IS_STRING(value)) {
        vm_print_internal(AS_STRING(value), "", false);
    } else {
        print_object(value);
    }
}
