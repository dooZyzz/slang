#include "vm/vm.h"
#include "debug/debug.h"
#include "runtime/module.h"
#include "runtime/builtin_modules.h"
#include "runtime/bootstrap.h"
#include "stdlib/stdlib.h"
#include "utils/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "vm/array.h"  // Still need macros, but using object system for arrays

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
    free(chunk->code);
    free(chunk->lines);

    for (size_t i = 0; i < chunk->constants.count; i++)
    {
        // Free strings in constants
        // TODO: Proper memory management for different constant types
    }
    free(chunk->constants.values);

    chunk_init(chunk);
}

void chunk_write(Chunk* chunk, uint8_t byte, int line)
{
    if (chunk->capacity < chunk->count + 1)
    {
        size_t old_capacity = chunk->capacity;
        chunk->capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        chunk->code = realloc(chunk->code, chunk->capacity);
        chunk->lines = realloc(chunk->lines, chunk->capacity * sizeof(int));
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
        chunk->constants.values = realloc(chunk->constants.values,
                                          sizeof(TaggedValue) * chunk->constants.capacity);
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
    string_pool_init(&vm->strings);
    vm->current_module_path = NULL;
    vm->current_module = NULL;
    
    // Initialize the object prototype system and stdlib
    stdlib_set_vm(vm);
    stdlib_init(vm);
    
    // Create bootstrap loader with built-ins
    ModuleLoader* bootstrap = bootstrap_loader_create(vm);
    
    // Create system loader with bootstrap as parent
    ModuleLoader* system = module_loader_create_with_hierarchy(MODULE_LOADER_SYSTEM, "system", bootstrap, vm);
    
    // Create application loader with system as parent
    vm->module_loader = module_loader_create_with_hierarchy(MODULE_LOADER_APPLICATION, "application", system, vm);
    
    // Initialize builtin modules
    builtin_modules_init();

    // No longer define natives directly - they come from bootstrap loader
    // Import built-ins into global namespace for backward compatibility
    Module* builtins = module_get_cached(bootstrap, "__builtins__");
    if (builtins) {
        for (size_t i = 0; i < builtins->exports.count; i++) {
            if (builtins->exports.visibility[i]) { // Only public exports
                define_global(vm, builtins->exports.names[i], builtins->exports.values[i]);
            }
        }
    }
}

void vm_init_with_loader(VM* vm, ModuleLoader* loader)
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
    vm->current_module_path = NULL;
    vm->current_module = NULL;
    
    // Initialize the object prototype system and stdlib
    stdlib_set_vm(vm);
    stdlib_init(vm);
    
    // Use provided loader
    vm->module_loader = loader;
    
    // Initialize builtin modules
    builtin_modules_init();

    // Import built-ins into global namespace from bootstrap loader
    ModuleLoader* bootstrap = loader;
    while (bootstrap && bootstrap->type != MODULE_LOADER_BOOTSTRAP) {
        bootstrap = bootstrap->parent;
    }
    
    if (bootstrap) {
        Module* builtins = module_get_cached(bootstrap, "__builtins__");
        if (builtins) {
            for (size_t i = 0; i < builtins->exports.count; i++) {
                if (builtins->exports.visibility[i]) { // Only public exports
                    define_global(vm, builtins->exports.names[i], builtins->exports.values[i]);
                }
            }
        }
    }
}

void vm_free(VM* vm)
{
    for (size_t i = 0; i < vm->globals.count; i++)
    {
        free(vm->globals.names[i]);
    }
    free(vm->globals.names);
    free(vm->globals.values);
    
    // Free struct types
    for (size_t i = 0; i < vm->struct_types.count; i++)
    {
        free(vm->struct_types.names[i]);
        struct_type_destroy(vm->struct_types.types[i]);
    }
    free(vm->struct_types.names);
    free(vm->struct_types.types);
    
    string_pool_free(&vm->strings);
    
    // Free module loader
    if (vm->module_loader) {
        module_loader_destroy(vm->module_loader);
    }
}

VM* vm_create(void)
{
    VM* vm = malloc(sizeof(VM));
    if (vm) {
        vm_init(vm);
    }
    return vm;
}

void vm_destroy(VM* vm)
{
    if (vm) {
        vm_free(vm);
        free(vm);
    }
}

static void reset_stack(VM* vm)
{
    vm->stack_top = vm->stack;
}

static Upvalue* capture_upvalue(VM* vm, TaggedValue* local) {
    // Upvalues allow closures to capture and share variables from outer scopes
    // This function ensures that multiple closures capturing the same variable
    // share a single Upvalue object, maintaining variable identity
    
    // Search for an existing upvalue pointing to this stack slot
    // The list is kept sorted by stack address (descending) for efficiency
    Upvalue* prev_upvalue = NULL;
    Upvalue* upvalue = vm->open_upvalues;
    
    while (upvalue != NULL && upvalue->location > local) {
        prev_upvalue = upvalue;
        upvalue = upvalue->next;
    }
    
    // If we found an existing upvalue for this location, return it
    // This ensures multiple closures share the same captured variable
    if (upvalue != NULL && upvalue->location == local) {
        return upvalue;
    }
    
    // Create a new upvalue pointing to the stack slot
    Upvalue* created_upvalue = malloc(sizeof(Upvalue));
    created_upvalue->location = local;          // Points to the variable on the stack
    created_upvalue->closed = NIL_VAL;          // Will store the value when closed
    created_upvalue->next = upvalue;            // Link into the sorted list
    
    // Insert into the linked list maintaining sort order
    if (prev_upvalue == NULL) {
        vm->open_upvalues = created_upvalue;
    } else {
        prev_upvalue->next = created_upvalue;
    }
    
    return created_upvalue;
}

static void close_upvalues(VM* vm, TaggedValue* last) {
    // Close upvalues when their variables go out of scope
    // This moves the value from the stack into the upvalue itself
    // ensuring captured variables survive after the stack frame is popped
    
    while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last) {
        Upvalue* upvalue = vm->open_upvalues;
        upvalue->closed = *upvalue->location;   // Copy value from stack
        upvalue->location = &upvalue->closed;   // Point to the closed value
        vm->open_upvalues = upvalue->next;      // Remove from open list
    }
}

static void runtime_error(VM* vm, const char* format, ...)
{
    fprintf(stderr, "Runtime error: ");

    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    // Print stack trace
    for (int i = vm->frame_count - 1; i >= 0; i--)
    {
        CallFrame* frame = &vm->frames[i];
        // Get the chunk for this frame
        Chunk* chunk = (i == 0) ? vm->chunk : &AS_FUNCTION(frame->slots[0])->chunk;
        size_t instruction = frame->ip - chunk->code - 1;
        int line = chunk->lines[instruction];
        fprintf(stderr, "[line %d] in ", line);
        if (i == 0)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", AS_FUNCTION(frame->slots[0])->name);
        }
    }

    reset_stack(vm);
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

static TaggedValue peek(VM* vm, int distance)
{
    return vm->stack_top[-1 - distance];
}

void vm_set_print_hook(PrintHook hook) {
    g_print_hook = hook;
}

void vm_print(const char* str) {
    if (g_print_hook) {
        g_print_hook(str);
    } else {
        printf("%s", str);
    }
}

void print_value(TaggedValue value)
{
    char buffer[1024];
    
    switch (value.type)
    {
    case VAL_BOOL:
        snprintf(buffer, sizeof(buffer), "%s", AS_BOOL(value) ? "true" : "false");
        vm_print(buffer);
        break;
    case VAL_NIL:
        snprintf(buffer, sizeof(buffer), "nil");
        vm_print(buffer);
        break;
    case VAL_NUMBER:
        snprintf(buffer, sizeof(buffer), "%g", AS_NUMBER(value));
        vm_print(buffer);
        break;
    case VAL_STRING:
        snprintf(buffer, sizeof(buffer), "%s", AS_STRING(value));
        vm_print(buffer);
        break;
    case VAL_FUNCTION:
        snprintf(buffer, sizeof(buffer), "<function %s>", AS_FUNCTION(value)->name);
        vm_print(buffer);
        break;
    case VAL_NATIVE:
        snprintf(buffer, sizeof(buffer), "<native fn>");
        vm_print(buffer);
        break;
    case VAL_CLOSURE:
        snprintf(buffer, sizeof(buffer), "<closure %s>", AS_CLOSURE(value)->function->name);
        vm_print(buffer);
        break;
    case VAL_OBJECT:
        if (IS_ARRAY(value)) {
            Object* array = AS_ARRAY(value);
            snprintf(buffer, sizeof(buffer), "[");
            vm_print(buffer);
            size_t length = array_length(array);
            for (size_t i = 0; i < length; i++) {
                if (i > 0) {
                    snprintf(buffer, sizeof(buffer), ", ");
                    vm_print(buffer);
                }
                print_value(array_get(array, i));
            }
            snprintf(buffer, sizeof(buffer), "]");
            vm_print(buffer);
        } else {
            snprintf(buffer, sizeof(buffer), "<object>");
            vm_print(buffer);
        }
        break;
    default:
        snprintf(buffer, sizeof(buffer), "<unknown>");
        vm_print(buffer);
    }
}

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

static bool is_falsey(TaggedValue value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static int find_global(VM* vm, const char* name)
{
    for (size_t i = 0; i < vm->globals.count; i++)
    {
        if (strcmp(vm->globals.names[i], name) == 0)
        {
            return i;
        }
    }
    return -1;
}

void define_global(VM* vm, const char* name, TaggedValue value)
{
    int existing = find_global(vm, name);
    if (existing != -1)
    {
        vm->globals.values[existing] = value;
        return;
    }

    if (vm->globals.capacity < vm->globals.count + 1)
    {
        size_t old_capacity = vm->globals.capacity;
        vm->globals.capacity = old_capacity < 8 ? 8 : old_capacity * 2;
        vm->globals.names = realloc(vm->globals.names,
                                    vm->globals.capacity * sizeof(char*));
        vm->globals.values = realloc(vm->globals.values,
                                     vm->globals.capacity * sizeof(TaggedValue));
    }

    vm->globals.names[vm->globals.count] = strdup(name);
    vm->globals.values[vm->globals.count] = value;
    vm->globals.count++;
}

void undefine_global(VM* vm, const char* name)
{
    int index = find_global(vm, name);
    if (index == -1) return;
    
    // Free the name
    free(vm->globals.names[index]);
    
    // Shift remaining globals down
    for (size_t i = index; i < vm->globals.count - 1; i++) {
        vm->globals.names[i] = vm->globals.names[i + 1];
        vm->globals.values[i] = vm->globals.values[i + 1];
    }
    
    vm->globals.count--;
}

static InterpretResult run(VM* vm)
{
    // Main VM execution loop - this is the heart of the interpreter
    // Each iteration fetches and executes one bytecode instruction
    CallFrame* frame = &vm->frames[vm->frame_count - 1];

    // Macro helpers for reading bytecode and performing common operations
    #define READ_BYTE() (*frame->ip++)  // Read next byte and advance instruction pointer
    #define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))  // Read 16-bit value (big-endian)

    // For constants, we need to read from the correct chunk
    // This complex macro handles different contexts: main script vs function vs closure
    #define READ_CONSTANT() ((vm->frame_count == 1) ? \
                            vm->chunk->constants.values[*(frame->ip++)] : \
                            (frame->closure ? frame->closure->function->chunk.constants.values[*(frame->ip++)] : \
                             AS_FUNCTION(frame->slots[0])->chunk.constants.values[*(frame->ip++)]))

    #define READ_STRING() AS_STRING(READ_CONSTANT())  // Read string constant

    #define BINARY_OP(valueType, op) \
        do { \
            /* Ensure both operands are numbers before performing arithmetic */ \
            if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
                runtime_error(vm, "Operands must be numbers."); \
                return INTERPRET_RUNTIME_ERROR; \
            } \
            /* Pop operands in reverse order (b then a) to maintain correct order */ \
            double b = AS_NUMBER(vm_pop(vm)); \
            double a = AS_NUMBER(vm_pop(vm)); \
            vm_push(vm, valueType(a op b)); \
        } while (false)

    // Infinite loop that runs until we hit a RETURN instruction or error
    for (;;)
    {
        // Update frame pointer each iteration in case of function calls
        // Function calls push new frames, so we need to refresh our pointer
        frame = &vm->frames[vm->frame_count - 1];

        // Get the current chunk from the current frame
        // Each function has its own chunk of bytecode
        Chunk* current_chunk;
        if (vm->frame_count == 1) {
            current_chunk = vm->chunk;
        } else if (frame->closure) {
            current_chunk = &frame->closure->function->chunk;
        } else {
            current_chunk = &AS_FUNCTION(frame->slots[0])->chunk;
        }

        if (debug_flags.trace_execution)
        {
            printf("          ");
            for (TaggedValue* slot = vm->stack; slot < vm->stack_top; slot++)
            {
                printf("[ ");
                print_value(*slot);
                printf(" ]");
            }
            printf("\n");
            disassemble_instruction(current_chunk, (int)(frame->ip - current_chunk->code));
        }

        // Fetch the next instruction opcode
        uint8_t instruction = READ_BYTE();
        // printf("DEBUG: Executing instruction %d\n", instruction);

        // Giant switch statement - each case handles one VM instruction
        // This is the core interpreter dispatch mechanism
        switch (instruction)
        {
        case OP_CONSTANT:
            {
                uint8_t constant_index = READ_BYTE();
                TaggedValue constant = current_chunk->constants.values[constant_index];
                vm_push(vm, constant);
                break;
            }
            
        case OP_CONSTANT_LONG:
            {
                uint8_t low = READ_BYTE();
                uint8_t high = READ_BYTE();
                uint16_t constant_index = (uint16_t)(low | (high << 8));
                TaggedValue constant = current_chunk->constants.values[constant_index];
                vm_push(vm, constant);
                break;
            }

        case OP_NIL:
            vm_push(vm, NIL_VAL);
            break;

        case OP_TRUE:
            vm_push(vm, BOOL_VAL(true));
            break;

        case OP_FALSE:
            vm_push(vm, BOOL_VAL(false));
            break;

        case OP_POP:
            vm_pop(vm);
            break;

        case OP_DUP:
            {
                TaggedValue value = peek(vm, 0);
                vm_push(vm, value);
                break;
            }

        case OP_SWAP:
            {
                TaggedValue top = vm_pop(vm);
                TaggedValue second = vm_pop(vm);
                vm_push(vm, top);
                vm_push(vm, second);
                break;
            }

        case OP_ADD:
            {
                if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1)))
                {
                    char* b = AS_STRING(vm_pop(vm));
                    char* a = AS_STRING(vm_pop(vm));
                    size_t a_len = strlen(a);
                    size_t b_len = strlen(b);
                    size_t length = a_len + b_len + 1;
                    char* buffer = malloc(length);
                    memcpy(buffer, a, a_len);
                    memcpy(buffer + a_len, b, b_len);
                    buffer[a_len + b_len] = '\0';
                    char* result = string_pool_create(&vm->strings, buffer, a_len + b_len);
                    free(buffer);
                    vm_push(vm, STRING_VAL(result));
                }
                else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1)))
                {
                    double b = AS_NUMBER(vm_pop(vm));
                    double a = AS_NUMBER(vm_pop(vm));
                    vm_push(vm, NUMBER_VAL(a + b));
                }
                else if (IS_STRING(peek(vm, 0)) && IS_NUMBER(peek(vm, 1)))
                {
                    char* b = AS_STRING(vm_pop(vm));
                    double a = AS_NUMBER(vm_pop(vm));
                    size_t a_len = strlen(b);
                    size_t length = a_len + 32; // Enough space for number conversion
                    char* buffer = malloc(length);
                    snprintf(buffer, length, "%g%s", a, b);
                    char* result = string_pool_create(&vm->strings, buffer, strlen(buffer));
                    free(buffer);
                    vm_push(vm, STRING_VAL(result));
                }
                else if (IS_NUMBER(peek(vm, 0)) && IS_STRING(peek(vm, 1)))
                {
                    double b = AS_NUMBER(vm_pop(vm));
                    char* a = AS_STRING(vm_pop(vm));
                    size_t a_len = strlen(a);
                    size_t length = a_len + 32; // Enough space for number conversion
                    char* buffer = malloc(length);
                    snprintf(buffer, length, "%s%g", a, b);
                    char* result = string_pool_create(&vm->strings, buffer, strlen(buffer));
                    free(buffer);
                    vm_push(vm, STRING_VAL(result));
                }
                else
                {

                    runtime_error(vm, "Operands must be between numbers and strings.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_SUBTRACT:
            BINARY_OP(NUMBER_VAL, -);
            break;

        case OP_MULTIPLY:
            BINARY_OP(NUMBER_VAL, *);
            break;

        case OP_DIVIDE:
            BINARY_OP(NUMBER_VAL, /);
            break;

        case OP_MODULO:
            {
                if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1)))
                {
                    runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                double b = AS_NUMBER(vm_pop(vm));
                double a = AS_NUMBER(vm_pop(vm));
                vm_push(vm, NUMBER_VAL(fmod(a, b)));
                break;
            }

        case OP_NEGATE:
            if (!IS_NUMBER(peek(vm, 0)))
            {
                runtime_error(vm, "Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            vm_push(vm, NUMBER_VAL(-AS_NUMBER(vm_pop(vm))));
            break;

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
            BINARY_OP(BOOL_VAL, >);
            break;

        case OP_GREATER_EQUAL:
            BINARY_OP(BOOL_VAL, >=);
            break;

        case OP_LESS:
            BINARY_OP(BOOL_VAL, <);
            break;

        case OP_LESS_EQUAL:
            BINARY_OP(BOOL_VAL, <=);
            break;

        case OP_NOT:
            vm_push(vm, BOOL_VAL(is_falsey(vm_pop(vm))));
            break;

        case OP_AND:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                vm_push(vm, BOOL_VAL(!is_falsey(a) && !is_falsey(b)));
                break;
            }

        case OP_OR:
            {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                vm_push(vm, BOOL_VAL(!is_falsey(a) || !is_falsey(b)));
                break;
            }

        case OP_BIT_AND:
            {
                if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1)))
                {
                    runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                long b = (long)AS_NUMBER(vm_pop(vm));
                long a = (long)AS_NUMBER(vm_pop(vm));
                vm_push(vm, NUMBER_VAL((double)(a & b)));
                break;
            }

        case OP_BIT_OR:
            {
                if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1)))
                {
                    runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                long b = (long)AS_NUMBER(vm_pop(vm));
                long a = (long)AS_NUMBER(vm_pop(vm));
                vm_push(vm, NUMBER_VAL((double)(a | b)));
                break;
            }

        case OP_BIT_XOR:
            {
                if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1)))
                {
                    runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                long b = (long)AS_NUMBER(vm_pop(vm));
                long a = (long)AS_NUMBER(vm_pop(vm));
                vm_push(vm, NUMBER_VAL((double)(a ^ b)));
                break;
            }

        case OP_BIT_NOT:
            {
                if (!IS_NUMBER(peek(vm, 0)))
                {
                    runtime_error(vm, "Operand must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                long value = (long)AS_NUMBER(vm_pop(vm));
                vm_push(vm, NUMBER_VAL((double)(~value)));
                break;
            }

        case OP_SHIFT_LEFT:
            {
                if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1)))
                {
                    runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                long b = (long)AS_NUMBER(vm_pop(vm));
                long a = (long)AS_NUMBER(vm_pop(vm));
                vm_push(vm, NUMBER_VAL((double)(a << b)));
                break;
            }

        case OP_SHIFT_RIGHT:
            {
                if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1)))
                {
                    runtime_error(vm, "Operands must be numbers.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                long b = (long)AS_NUMBER(vm_pop(vm));
                long a = (long)AS_NUMBER(vm_pop(vm));
                vm_push(vm, NUMBER_VAL((double)(a >> b)));
                break;
            }

        case OP_TO_STRING:
            {
                TaggedValue val = vm_pop(vm);
                
                // Convert value to string
                char buffer[256];
                switch (val.type) {
                    case VAL_NIL:
                        vm_push(vm, STRING_VAL(strdup("nil")));
                        break;
                    case VAL_BOOL:
                        vm_push(vm, STRING_VAL(strdup(AS_BOOL(val) ? "true" : "false")));
                        break;
                    case VAL_NUMBER:
                        {
                            // Format number as string
                            double num = AS_NUMBER(val);
                            if (num == (long long)num) {
                                snprintf(buffer, sizeof(buffer), "%lld", (long long)num);
                            } else {
                                snprintf(buffer, sizeof(buffer), "%g", num);
                            }
                            vm_push(vm, STRING_VAL(strdup(buffer)));
                        }
                        break;
                    case VAL_STRING:
                        // Already a string, push it back
                        vm_push(vm, val);
                        break;
                    case VAL_FUNCTION:
                        snprintf(buffer, sizeof(buffer), "<function %s>", AS_FUNCTION(val)->name);
                        vm_push(vm, STRING_VAL(strdup(buffer)));
                        break;
                    case VAL_CLOSURE:
                        snprintf(buffer, sizeof(buffer), "<closure %s>", AS_CLOSURE(val)->function->name);
                        vm_push(vm, STRING_VAL(strdup(buffer)));
                        break;
                    case VAL_NATIVE:
                        vm_push(vm, STRING_VAL(strdup("<native function>")));
                        break;
                    case VAL_OBJECT:
                        if (IS_ARRAY(val)) {
                            // Convert array to string representation
                            Object* array = AS_ARRAY(val);
                            size_t length = array_length(array);
                            
                            // Start with "["
                            char* result = malloc(1024); // Initial buffer
                            strcpy(result, "[");
                            
                            for (size_t i = 0; i < length; i++) {
                                if (i > 0) strcat(result, ", ");
                                
                                TaggedValue elem = array_get(array, i);
                                // Recursively convert elements
                                if (elem.type == VAL_NUMBER) {
                                    char num_buf[64];
                                    double num = AS_NUMBER(elem);
                                    if (num == (long long)num) {
                                        snprintf(num_buf, sizeof(num_buf), "%lld", (long long)num);
                                    } else {
                                        snprintf(num_buf, sizeof(num_buf), "%g", num);
                                    }
                                    strcat(result, num_buf);
                                } else if (elem.type == VAL_STRING) {
                                    strcat(result, AS_STRING(elem));
                                } else if (elem.type == VAL_BOOL) {
                                    strcat(result, AS_BOOL(elem) ? "true" : "false");
                                } else if (elem.type == VAL_NIL) {
                                    strcat(result, "nil");
                                } else {
                                    strcat(result, "<object>");
                                }
                            }
                            
                            strcat(result, "]");
                            vm_push(vm, STRING_VAL(result));
                        } else {
                            vm_push(vm, STRING_VAL(strdup("<object>")));
                        }
                        break;
                    default:
                        vm_push(vm, STRING_VAL(strdup("<unknown>")));
                        break;
                }
                break;
            }
            
        case OP_PRINT:
            {
                print_value(vm_pop(vm));
                char buffer[4] = "\n";
                vm_print(buffer);
                break;
            }

        case OP_JUMP:
            {
                uint16_t offset = READ_SHORT();
                frame->ip += offset;
                break;
            }

        case OP_JUMP_IF_FALSE:
            {
                uint16_t offset = READ_SHORT();
                if (is_falsey(peek(vm, 0)))
                {
                    frame->ip += offset;
                }
                break;
            }

        case OP_JUMP_IF_TRUE:
            {
                uint16_t offset = READ_SHORT();
                if (!is_falsey(peek(vm, 0)))
                {
                    frame->ip += offset;
                }
                break;
            }

        case OP_LOOP:
            {
                uint16_t offset = READ_SHORT();
                frame->ip -= offset;
                break;
            }

        case OP_GET_LOCAL:
            {
                uint8_t slot = READ_BYTE();
                vm_push(vm, frame->slots[slot]);
                break;
            }

        case OP_SET_LOCAL:
            {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(vm, 0);
                break;
            }
            
        case OP_GET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                Closure* closure = frame->closure;
                if (!closure) {
                    runtime_error(vm, "Cannot access upvalue outside of closure");
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (slot >= closure->upvalue_count) {
                    runtime_error(vm, "Invalid upvalue slot %d", slot);
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm_push(vm, *closure->upvalues[slot]->location);
                break;
            }
            
        case OP_SET_UPVALUE:
            {
                uint8_t slot = READ_BYTE();
                Closure* closure = frame->closure;
                if (!closure) {
                    runtime_error(vm, "Cannot set upvalue outside of closure");
                    return INTERPRET_RUNTIME_ERROR;
                }
                *closure->upvalues[slot]->location = peek(vm, 0);
                break;
            }

        case OP_GET_GLOBAL:
            {
                char* name = READ_STRING();
                
                // First check if we're in a module context
                if (vm->current_module) {
                    // Look for the variable in the module's scope
                    if (module_has_in_scope(vm->current_module, name)) {
                        vm_push(vm, module_get_from_scope(vm->current_module, name));
                        goto found_global;
                    }
                    
                    // Also check module globals for backward compatibility
                    for (size_t i = 0; i < vm->current_module->globals.count; i++) {
                        if (strcmp(vm->current_module->globals.names[i], name) == 0) {
                            vm_push(vm, vm->current_module->globals.values[i]);
                            goto found_global;
                        }
                    }
                }
                
                // Not found in module, check VM globals
                int global = find_global(vm, name);
                if (global == -1)
                {
                    runtime_error(vm, "Undefined variable '%s'.", name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm_push(vm, vm->globals.values[global]);
                
                found_global:
                break;
            }

        case OP_SET_GLOBAL:
            {
                char* name = READ_STRING();
                
                // First check if we're in a module context
                if (vm->current_module) {
                    // Look for the variable in the module's scope
                    if (module_has_in_scope(vm->current_module, name)) {
                        module_scope_define(vm->current_module->scope, name, peek(vm, 0), module_scope_is_exported(vm->current_module->scope, name));
                        goto set_global_done;
                    }
                    
                    // Also check module globals for backward compatibility
                    for (size_t i = 0; i < vm->current_module->globals.count; i++) {
                        if (strcmp(vm->current_module->globals.names[i], name) == 0) {
                            vm->current_module->globals.values[i] = peek(vm, 0);
                            goto set_global_done;
                        }
                    }
                }
                
                // Not found in module, check VM globals
                int global = find_global(vm, name);
                if (global == -1)
                {
                    runtime_error(vm, "Undefined variable '%s'.", name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                vm->globals.values[global] = peek(vm, 0);
                
                set_global_done:
                break;
            }

        case OP_DEFINE_GLOBAL:
            {
                char* name = READ_STRING();
                TaggedValue value = peek(vm, 0);
                
                // If defining a function in a module context, set its module reference
                // BUT only if it doesn't already have one (to avoid overwriting when re-exporting)
                if (vm->current_module && IS_FUNCTION(value) && AS_FUNCTION(value)->module == NULL) {
                    // fprintf(stderr, "[DEBUG] Setting module reference on function '%s' to module '%s'\n", 
                    //         AS_FUNCTION(value)->name, vm->current_module->path);
                    AS_FUNCTION(value)->module = vm->current_module;
                }
                
                // If we're in a module context, define in the module's scope
                if (vm->current_module) {
                    // Define in module scope (not exported by default)
                    module_scope_define(vm->current_module->scope, name, value, false);
                    
                    // Also add to module globals for backward compatibility
                    // Check if it already exists
                    for (size_t i = 0; i < vm->current_module->globals.count; i++) {
                        if (strcmp(vm->current_module->globals.names[i], name) == 0) {
                            vm->current_module->globals.values[i] = value;
                            vm_pop(vm);
                            goto define_global_done;
                        }
                    }
                    
                    // Add new global to module
                    if (vm->current_module->globals.count == vm->current_module->globals.capacity) {
                        // Grow arrays
                        size_t new_capacity = vm->current_module->globals.capacity == 0 ? 8 : vm->current_module->globals.capacity * 2;
                        vm->current_module->globals.names = realloc(vm->current_module->globals.names, sizeof(char*) * new_capacity);
                        vm->current_module->globals.values = realloc(vm->current_module->globals.values, sizeof(TaggedValue) * new_capacity);
                        vm->current_module->globals.capacity = new_capacity;
                    }
                    
                    vm->current_module->globals.names[vm->current_module->globals.count] = strdup(name);
                    vm->current_module->globals.values[vm->current_module->globals.count] = value;
                    vm->current_module->globals.count++;
                } else {
                    // Not in module context, use VM globals
                    define_global(vm, name, value);
                }
                
                vm_pop(vm);
                define_global_done:
                break;
            }

        case OP_DEFINE_LOCAL:
            {
                // For now, just consume the name constant index
                // Local variables are already on the stack
                READ_BYTE();
                break;
            }

        case OP_ARRAY:
        case OP_BUILD_ARRAY:
            {
                uint8_t count = READ_BYTE();
                Object* array = array_create();

                // Pop elements in reverse order and add to array
                // We need to collect them first because they're in reverse order on the stack
                TaggedValue* temp = malloc(sizeof(TaggedValue) * count);
                for (int i = count - 1; i >= 0; i--) {
                    temp[i] = vm_pop(vm);
                }

                // Now add them to the array in the correct order
                for (int i = 0; i < count; i++) {
                    array_push(array, temp[i]);
                }
                free(temp);

                // Create a TaggedValue that holds the array as an object
                TaggedValue array_val = OBJECT_VAL(array);
                vm_push(vm, array_val);
                break;
            }

        case OP_GET_SUBSCRIPT:
            {
                TaggedValue index = vm_pop(vm);
                TaggedValue array_val = vm_pop(vm);

                if (!IS_ARRAY(array_val)) {
                    runtime_error(vm, "Can only index arrays.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!IS_NUMBER(index)) {
                    runtime_error(vm, "Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                Object* array = AS_ARRAY(array_val);
                int idx = (int)AS_NUMBER(index);

                if (idx < 0) {
                    runtime_error(vm, "Array index cannot be negative.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                // array_get handles bounds checking and returns NIL for out of bounds
                TaggedValue result = array_get(array, (size_t)idx);
                vm_push(vm, result);
                break;
            }

        case OP_SET_SUBSCRIPT:
            {
                TaggedValue value = vm_pop(vm);
                TaggedValue index = vm_pop(vm);
                TaggedValue array_val = vm_pop(vm);

                if (!IS_ARRAY(array_val)) {
                    runtime_error(vm, "Can only index arrays.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (!IS_NUMBER(index)) {
                    runtime_error(vm, "Array index must be a number.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                Object* array = AS_ARRAY(array_val);
                int idx = (int)AS_NUMBER(index);

                if (idx < 0) {
                    runtime_error(vm, "Array index cannot be negative.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                // array_set handles growing the array if needed
                array_set(array, (size_t)idx, value);
                vm_push(vm, value); // Leave value on stack
                break;
            }

        case OP_CREATE_OBJECT:
            {
                Object* obj = object_create();
                vm_push(vm, OBJECT_VAL(obj));
                break;
            }
            
        case OP_GET_PROPERTY:
            {
                TaggedValue name = vm_pop(vm);
                TaggedValue object = vm_pop(vm);

                if (!IS_STRING(name)) {
                    runtime_error(vm, "Property name must be a string.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                char* prop_name = AS_STRING(name);

                // // fprintf(stderr, "[DEBUG] Main VM OP_GET_PROPERTY: prop='%s', object_type=%d, frame_count=%d\n", 
                //         prop_name, object.type, vm->frame_count);
                if (IS_OBJECT(object)) {
                    Object* obj = AS_OBJECT(object);
                    
                    // Get property from object (including prototype chain)
                    TaggedValue* prop_value = object_get_property(obj, prop_name);
                    
                    if (prop_value) {
                        if (IS_FUNCTION(*prop_value)) {
                            Function* func = AS_FUNCTION(*prop_value);
                            // // fprintf(stderr, "[DEBUG] Retrieved function '%s' from object property '%s' (module=%p)\n",
                            //         func->name, prop_name, (void*)func->module);
                            if (func->module) {
                                // fprintf(stderr, "[DEBUG]   Function module: %s (globals=%zu)\n", 
                                //         func->module->path, func->module->globals.count);
                            }
                        }
                        vm_push(vm, *prop_value);
                    } else {
                        runtime_error(vm, "Unknown property '%s'.", prop_name);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else if (IS_STRING(object)) {
                    // For strings, look up in String.prototype
                    Object* string_proto = get_string_prototype();
                    TaggedValue* prop_value = object_get_property(string_proto, prop_name);
                    
                    if (prop_value) {
                        vm_push(vm, *prop_value);
                    } else {
                        runtime_error(vm, "Unknown string property '%s'.", prop_name);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else if (IS_NUMBER(object)) {
                    // For numbers, look up in Number prototype
                    Object* number_proto = get_number_prototype();
                    if (number_proto) {
                        TaggedValue* prop_value = object_get_property(number_proto, prop_name);
                        if (prop_value) {
                            vm_push(vm, *prop_value);
                        } else {
                            runtime_error(vm, "Unknown number property '%s'.", prop_name);
                            return INTERPRET_RUNTIME_ERROR;
                        }
                    } else {
                        runtime_error(vm, "Number prototype not initialized.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    runtime_error(vm, "Property access not supported for this type.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_SET_PROPERTY:
            {
                TaggedValue value = vm_pop(vm);
                TaggedValue name = vm_pop(vm);
                TaggedValue object = vm_pop(vm);

                if (!IS_STRING(name)) {
                    runtime_error(vm, "Property name must be a string.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                if (IS_OBJECT(object)) {
                    Object* obj = AS_OBJECT(object);
                    // // fprintf(stderr, "[DEBUG] Setting property '%s' on object\n", AS_STRING(name));
                    object_set_property(obj, AS_STRING(name), value);
                    vm_push(vm, value); // Leave value on stack
                } else {
                    runtime_error(vm, "Can only set properties on objects.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            
        case OP_SET_PROTOTYPE:
            {
                TaggedValue prototype = vm_pop(vm);
                TaggedValue object = peek(vm, 0); // Keep object on stack
                
                if (!IS_OBJECT(object)) {
                    runtime_error(vm, "Can only set prototype on objects.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!IS_OBJECT(prototype)) {
                    runtime_error(vm, "Prototype must be an object.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                Object* obj = AS_OBJECT(object);
                Object* proto = AS_OBJECT(prototype);
                obj->prototype = proto;
                
                break;
            }

        case OP_GET_ITER:
            {
                TaggedValue value = peek(vm, 0); // Don't pop yet
                
                if (IS_ARRAY(value)) {
                    // Create an iterator for the array
                    // Iterator format: [array, index] packed as a special object
                    // For simplicity, we'll use the array itself and track index separately
                    // Push index 0 to start iteration
                    vm_push(vm, NUMBER_VAL(0));
                } else {
                    runtime_error(vm, "Can only iterate over arrays.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }

        case OP_FOR_ITER:
            {
                // Stack: [array, index]
                TaggedValue index_val = peek(vm, 0);  // Peek index
                TaggedValue array_val = peek(vm, 1);  // Peek array
                
                if (!IS_ARRAY(array_val) || !IS_NUMBER(index_val)) {
                    runtime_error(vm, "Invalid iterator state.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                Object* array = AS_ARRAY(array_val);
                int index = (int)AS_NUMBER(index_val);
                
                if (index < 0 || (size_t)index >= array_length(array)) {
                    // No more items - leave iterator on stack
                    vm_push(vm, BOOL_VAL(false)); // Signal end of iteration
                } else {
                    // Pop the current index
                    vm_pop(vm);
                    
                    // Get current element
                    TaggedValue element = array_get(array, (size_t)index);
                    
                    // Update index for next iteration
                    vm_push(vm, NUMBER_VAL(index + 1));
                    
                    // Push element (this will be the loop variable)
                    vm_push(vm, element);
                    
                    // Push success flag
                    vm_push(vm, BOOL_VAL(true));
                }
                break;
            }

        case OP_FUNCTION:
            {
                // Function constant is already on the stack
                // For now, functions are created at compile time and stored as constants
                break;
            }
            
        case OP_CLOSURE:
            {
                uint8_t index = READ_BYTE();
                // Get the correct chunk based on current frame
                Chunk* chunk = (vm->frame_count == 1) ? vm->chunk : &AS_FUNCTION(frame->slots[0])->chunk;
                TaggedValue constant = chunk->constants.values[index];
                if (constant.type != VAL_FUNCTION) {
                    runtime_error(vm, "Expected function constant for closure");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                Function* function = constant.as.function;
                Closure* closure = malloc(sizeof(Closure));
                closure->function = function;
                closure->upvalue_count = function->upvalue_count;
                closure->upvalues = NULL;
                
                if (closure->upvalue_count > 0) {
                    closure->upvalues = malloc(sizeof(Upvalue*) * closure->upvalue_count);
                    
                    // Read upvalue information and capture variables
                    // This is the runtime counterpart to the compiler's upvalue emission
                    for (int i = 0; i < closure->upvalue_count; i++) {
                        uint8_t is_local = READ_BYTE();      // Is this capturing a local variable?
                        uint8_t upvalue_index = READ_BYTE(); // Index in locals or upvalues array
                        
                        if (is_local) {
                            // Capture a local variable from the current call frame
                            // The variable lives on the stack in the current frame
                            TaggedValue* slot_location = frame->slots + upvalue_index;
                            
                            // Validate the slot location to prevent memory corruption
                            if (slot_location < vm->stack || slot_location >= vm->stack_top) {
                                runtime_error(vm, "Invalid upvalue slot index %d", upvalue_index);
                                free(closure->upvalues);
                                free(closure);
                                return INTERPRET_RUNTIME_ERROR;
                            }
                            
                            // capture_upvalue creates or reuses an Upvalue object
                            // This allows multiple closures to share the same captured variable
                            closure->upvalues[i] = capture_upvalue(vm, slot_location);
                        } else {
                            // Capture an upvalue from the enclosing function's closure
                            // This handles nested closures (closure within a closure)
                            if (frame->closure && frame->closure->upvalues && 
                                upvalue_index < frame->closure->upvalue_count) {
                                // Share the same upvalue object - maintains variable identity
                                closure->upvalues[i] = frame->closure->upvalues[upvalue_index];
                            } else {
                                runtime_error(vm, "Cannot capture upvalue from non-closure function or invalid index");
                                free(closure->upvalues);
                                free(closure);
                                return INTERPRET_RUNTIME_ERROR;
                            }
                        }
                    }
                }
                
                vm_push(vm, (TaggedValue){.type = VAL_CLOSURE, .as.closure = closure});
                break;
            }

        case OP_CALL:
            {
                uint8_t arg_count = READ_BYTE();
                TaggedValue callee = peek(vm, arg_count);

                if (IS_NATIVE(callee))
                {
                    // Call native function
                    NativeFn native = AS_NATIVE(callee);
                    
                    // Validate function pointer
                    if (!native) {
                        runtime_error(vm, "Native function pointer is NULL");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    // Check if pointer looks valid (not in low memory)
                    if ((uintptr_t)native < 0x1000) {
                        runtime_error(vm, "Native function pointer looks invalid: %p", (void*)native);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    TaggedValue* args = vm->stack_top - arg_count;
                    
                    // Validate args pointer
                    if (args < vm->stack || args > vm->stack_top) {
                        runtime_error(vm, "Invalid arguments pointer");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    
                    TaggedValue result = native(arg_count, args);

                    // Pop arguments and function
                    vm->stack_top -= arg_count + 1;

                    // Push result
                    vm_push(vm, result);
                }
                else if (IS_FUNCTION(callee) || IS_CLOSURE(callee))
                {
                    // Unified handling for both functions and closures
                    Function* function = IS_FUNCTION(callee) ? AS_FUNCTION(callee) : AS_CLOSURE(callee)->function;
                    Closure* closure = IS_CLOSURE(callee) ? AS_CLOSURE(callee) : NULL;
                    
                    if (arg_count != function->arity)
                    {
                        runtime_error(vm, "Expected %d arguments but got %d.",
                                      function->arity, arg_count);
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    // Check for stack overflow
                    if (vm->frame_count == FRAMES_MAX)
                    {
                        runtime_error(vm, "Stack overflow.");
                        return INTERPRET_RUNTIME_ERROR;
                    }

                    // Set up new call frame
                    CallFrame* new_frame = &vm->frames[vm->frame_count++];
                    new_frame->ip = function->chunk.code;
                    new_frame->slots = vm->stack_top - arg_count - 1;
                    new_frame->closure = closure;
                    new_frame->saved_module = vm->current_module; // Save current module context

                    // If this function belongs to a module, switch to that module's context
                    if (function->module) {
                        vm->current_module = function->module;
                    }

                    // Frame will be updated at the start of next iteration
                }
                else
                {
                    runtime_error(vm, "Can only call functions and closures.");
                    return INTERPRET_RUNTIME_ERROR;
                }

                break;
            }

        case OP_METHOD_CALL:
            {
                uint8_t arg_count = READ_BYTE();
                // Stack layout: [method, object, arg1, arg2, ...]
                TaggedValue method = peek(vm, arg_count + 1);
                TaggedValue object = peek(vm, arg_count);
                // // fprintf(stderr, "[DEBUG] OP_METHOD_CALL: arg_count=%d, method type=%d, object type=%d\n", 
                //         arg_count, method.type, object.type);
                // Debug stack
                // // fprintf(stderr, "[DEBUG] Stack at OP_METHOD_CALL:\n");
                // for (int i = arg_count + 1; i >= 0; i--) {
                //     TaggedValue v = peek(vm, i);
                //     fprintf(stderr, "  [-%d] type=%d", i, v.type);
                //     if (v.type == VAL_FUNCTION && IS_FUNCTION(v)) {
                //         fprintf(stderr, " (function: %s)", AS_FUNCTION(v)->name);
                //     }
                //     fprintf(stderr, "\n");
                // }
                // TaggedValue object = peek(vm, arg_count);  // TODO: Use for method dispatch
                
                if (IS_NATIVE(method)) {
                    // Native method call
                    NativeFn native = AS_NATIVE(method);
                    
                    // Prepare arguments: [object, arg1, arg2, ...]
                    TaggedValue* args = vm->stack_top - arg_count - 2;
                    
                    // Call native method with object as first argument
                    TaggedValue result = native(arg_count + 1, args + 1); // +1 to include object
                    
                    // Pop method, object, and arguments
                    vm->stack_top -= arg_count + 2;
                    
                    // Push result
                    vm_push(vm, result);
                } else if (IS_FUNCTION(method) || IS_CLOSURE(method)) {
                    Function* function = IS_CLOSURE(method) ? AS_CLOSURE(method)->function : AS_FUNCTION(method);
                    
                    // Get the object that the method is being called on
                    TaggedValue object = peek(vm, arg_count);
                    // // fprintf(stderr, "[DEBUG] OP_METHOD_CALL: method='%s', object_type=%d, is_struct=%d\n", 
                    //         function->name, object.type, IS_STRUCT(object));
                    
                    /**
                     * Module functions vs. regular methods:
                     * 
                     * When calling a method on an object, we need to distinguish between:
                     * 1. Module functions: Functions exported from a module (e.g., math.sin())
                     *    - These are regular functions that don't expect 'self'
                     *    - The module object should NOT be passed as first argument
                     * 
                     * 2. Regular methods: Methods on structs/classes (e.g., person.getName())
                     *    - These expect 'self' as the first argument
                     *    - The object should be passed as first argument
                     * 
                     * We detect module functions by checking if the object is a plain Object
                     * (not a struct instance). This allows module functions to work like
                     * namespaced functions rather than methods.
                     */
                    bool is_module_function = IS_OBJECT(object) && !IS_STRUCT(object);
                    
                    // Check arity
                    // For extension methods (name contains _ext_), arity includes 'this'
                    // For module functions, we don't pass the module as first arg
                    int expected_args = function->arity;
                    int actual_args = arg_count;
                    
                    if (strstr(function->name, "_ext_") != NULL) {
                        // Extension method: function expects 'this' as first arg
                        actual_args = arg_count + 1; // +1 for the object/'this'
                    }
                    
                    if (actual_args != expected_args) {
                        runtime_error(vm, "Expected %d arguments but got %d.",
                                      expected_args, actual_args);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    // Check for stack overflow
                    if (vm->frame_count == FRAMES_MAX) {
                        runtime_error(vm, "Stack overflow.");
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    
                    // Set up new call frame
                    // The stack has: [method, object, arg1, arg2, ...]
                    CallFrame* new_frame = &vm->frames[vm->frame_count++];
                    new_frame->ip = function->chunk.code;
                    
                    if (is_module_function) {
                        // For module functions, we need to rearrange the stack
                        // Current: [method, object, arg1, arg2, ...]
                        // We want: [method, arg1, arg2, ...]
                        // Remove the object from the stack
                        TaggedValue* object_slot = vm->stack_top - arg_count - 1;
                        for (int i = 0; i < arg_count; i++) {
                            object_slot[i] = object_slot[i + 1];
                        }
                        vm->stack_top--;
                        new_frame->slots = vm->stack_top - arg_count - 1; // -1 for method only
                    } else {
                        // Regular method call - object is at slots[1] (self)
                        new_frame->slots = vm->stack_top - arg_count - 2; // -2 for method and object
                    }
                    new_frame->saved_module = vm->current_module; // Save current module context
                    
                    // If this function belongs to a module, switch to that module's context
                    if (function->module) {
                        vm->current_module = function->module;
                    }
                    
                    // Handle closures
                    if (IS_CLOSURE(method)) {
                        new_frame->closure = AS_CLOSURE(method);
                    } else {
                        new_frame->closure = NULL;
                    }
                } else {
                    // fprintf(stderr, "[DEBUG] OP_METHOD_CALL: method type = %d\n", method.type);
                    runtime_error(vm, "Can only call functions, closures, and native functions as methods.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                break;
            }

        case OP_RETURN:
            {
                // Get the return value
                TaggedValue result = vm_pop(vm);

                // Close upvalues for this frame
                close_upvalues(vm, frame->slots);

                // Restore module context from the frame we're returning from
                vm->current_module = frame->saved_module;

                // Restore stack to before the function call
                vm->stack_top = frame->slots;

                // Decrease frame count
                vm->frame_count--;

                // If this was the last frame, we're done
                if (vm->frame_count == 0)
                {
                    // Push result back for REPL/tests
                    vm_push(vm, result);
                    return INTERPRET_OK;
                }

                // Push return value
                vm_push(vm, result);

                // Frame will be updated at the start of next iteration
                break;
            }
            
        case OP_LOAD_BUILTIN:
            {
                // Pop export name and module name from stack
                TaggedValue export_name = vm_pop(vm);
                TaggedValue module_name = vm_pop(vm);
                
                if (!IS_STRING(module_name) || !IS_STRING(export_name)) {
                    runtime_error(vm, "Module and export names must be strings");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Get the builtin function
                TaggedValue builtin_value;
                if (builtin_module_get_export(AS_STRING(module_name), AS_STRING(export_name), &builtin_value)) {
                    vm_push(vm, builtin_value);
                } else {
                    runtime_error(vm, "Failed to load builtin: %s.%s", 
                                AS_STRING(module_name), AS_STRING(export_name));
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            
        case OP_LOAD_MODULE:
            {
                // Pop module path from stack
                TaggedValue module_path = vm_pop(vm);
                
                if (!IS_STRING(module_path)) {
                    runtime_error(vm, "Module path must be a string");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Load the module
                if (!vm->module_loader) {
                    vm->module_loader = module_loader_create(vm);
                }
                
                if (getenv("SWIFTLANG_DEBUG")) {
                    fprintf(stderr, "[DEBUG VM] Loading module: %s\n", AS_STRING(module_path));
                }
                Module* module = module_load_relative(vm->module_loader, AS_STRING(module_path), false, vm->current_module_path);
                if (!module) {
                    runtime_error(vm, "Failed to load module: %s", AS_STRING(module_path));
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Ensure module is initialized (handles lazy loading)
                if (!ensure_module_initialized(module, vm)) {
                    runtime_error(vm, "Failed to initialize module: %s", AS_STRING(module_path));
                    return INTERPRET_RUNTIME_ERROR;
                }
                if (getenv("SWIFTLANG_DEBUG")) {
                    fprintf(stderr, "[DEBUG VM] Module loaded, state=%d\n", module->state);
                }
                
                // Push the module object
                if (module->module_object) {
                    if (getenv("SWIFTLANG_DEBUG")) {
                        printf("DEBUG: Module has %zu exports\n", module->exports.count);
                        for (size_t i = 0; i < module->exports.count; i++) {
                            printf("  Export[%zu]: %s\n", i, module->exports.names[i]);
                        }
                    }
                    vm_push(vm, OBJECT_VAL(module->module_object));
                } else {
                    // Create module object from exports
                    Object* mod_obj = object_create();
                    if (getenv("SWIFTLANG_DEBUG")) {
                        printf("DEBUG: Creating module object with %zu exports\n", module->exports.count);
                    }
                    for (size_t i = 0; i < module->exports.count; i++) {
                        object_set_property(mod_obj, module->exports.names[i], 
                                          module->exports.values[i]);
                        if (getenv("SWIFTLANG_DEBUG")) {
                            printf("  Added export: %s\n", module->exports.names[i]);
                        }
                    }
                    module->module_object = mod_obj;
                    vm_push(vm, OBJECT_VAL(mod_obj));
                }
                break;
            }
            
        case OP_LOAD_NATIVE_MODULE:
            {
                // Pop module path from stack
                TaggedValue module_path = vm_pop(vm);
                
                if (!IS_STRING(module_path)) {
                    runtime_error(vm, "Module path must be a string");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Load the native module
                if (!vm->module_loader) {
                    vm->module_loader = module_loader_create(vm);
                }
                
                Module* module = module_load_relative(vm->module_loader, AS_STRING(module_path), true, vm->current_module_path);
                if (!module) {
                    runtime_error(vm, "Failed to load native module: %s", AS_STRING(module_path));
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Native modules should already be initialized, but ensure anyway
                if (!ensure_module_initialized(module, vm)) {
                    runtime_error(vm, "Failed to initialize native module: %s", AS_STRING(module_path));
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Push the module object
                if (module->module_object) {
                    if (getenv("SWIFTLANG_DEBUG")) {
                        printf("DEBUG: Module has %zu exports\n", module->exports.count);
                        for (size_t i = 0; i < module->exports.count; i++) {
                            printf("  Export[%zu]: %s\n", i, module->exports.names[i]);
                        }
                    }
                    vm_push(vm, OBJECT_VAL(module->module_object));
                } else {
                    // Create module object from exports
                    Object* mod_obj = object_create();
                    if (getenv("SWIFTLANG_DEBUG")) {
                        printf("DEBUG: Creating module object with %zu exports\n", module->exports.count);
                    }
                    for (size_t i = 0; i < module->exports.count; i++) {
                        object_set_property(mod_obj, module->exports.names[i], 
                                          module->exports.values[i]);
                        if (getenv("SWIFTLANG_DEBUG")) {
                            printf("  Added export: %s\n", module->exports.names[i]);
                        }
                    }
                    module->module_object = mod_obj;
                    vm_push(vm, OBJECT_VAL(mod_obj));
                }
                break;
            }
            
        case OP_IMPORT_FROM:
            {
                // Pop export name and module object from stack
                TaggedValue export_name = vm_pop(vm);
                TaggedValue module_obj = vm_pop(vm);
                
                if (!IS_STRING(export_name)) {
                    runtime_error(vm, "Export name must be a string");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                if (!IS_OBJECT(module_obj)) {
                    runtime_error(vm, "Module must be an object");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Get the export from the module object
                TaggedValue* export_value_ptr = object_get_property(AS_OBJECT(module_obj), 
                                                                    AS_STRING(export_name));
                if (export_value_ptr) {
                    vm_push(vm, *export_value_ptr);
                } else {
                    vm_push(vm, NIL_VAL);
                }
                break;
            }
            
        case OP_IMPORT_ALL_FROM:
            {
                /**
                 * Import all exports from a module into the current scope.
                 * Stack: [module_object]
                 * 
                 * This iterates through all properties of the module object
                 * and defines each as a global variable in the current scope.
                 */
                TaggedValue module_obj = vm_pop(vm);
                
                if (!IS_OBJECT(module_obj)) {
                    runtime_error(vm, "Can only import from module objects");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                Object* module = AS_OBJECT(module_obj);
                ObjectProperty* prop = module->properties;
                
                // Iterate through all module properties
                while (prop) {
                    // Check if global already exists
                    int existing_idx = find_global(vm, prop->key);
                    if (existing_idx != -1) {
                        // Update existing global
                        vm->globals.values[existing_idx] = *prop->value;
                    } else {
                        // Define new global
                        define_global(vm, prop->key, *prop->value);
                    }
                    prop = prop->next;
                }
                break;
            }
            
        case OP_MODULE_EXPORT:
            {
                if (getenv("SWIFTLANG_DEBUG")) {
                    fprintf(stderr, "[DEBUG] OP_MODULE_EXPORT executed\n");
                }
                // Stack: [export_name, value]
                TaggedValue value = vm_pop(vm);
                TaggedValue export_name = vm_pop(vm);
                
                if (!IS_STRING(export_name)) {
                    runtime_error(vm, "Export name must be a string");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                const char* name = AS_STRING(export_name);
                if (getenv("SWIFTLANG_DEBUG")) {
                    fprintf(stderr, "[DEBUG] Exporting: %s, current_module=%p\n", name, vm->current_module);
                }
                
                // If exporting a function in a module context, set its module reference
                // BUT only if it doesn't already have one
                if (vm->current_module && IS_FUNCTION(value) && AS_FUNCTION(value)->module == NULL) {
                    AS_FUNCTION(value)->module = vm->current_module;
                }
                
                // If we're in a module context, mark as exported in module scope
                if (vm->current_module) {
                    // Look up the value in module scope
                    TaggedValue existing = module_get_from_scope(vm->current_module, name);
                    if (!IS_NIL(existing)) {
                        // Update existing entry to be exported
                        module_scope_define(vm->current_module->scope, name, existing, true);
                    } else {
                        // Define new exported entry
                        module_scope_define(vm->current_module->scope, name, value, true);
                    }
                }
                
                // Also handle __module_exports__ for compatibility
                int exports_idx = find_global(vm, "__module_exports__");
                if (exports_idx != -1) {
                    TaggedValue exports = vm->globals.values[exports_idx];
                    if (IS_OBJECT(exports)) {
                        if (getenv("SWIFTLANG_DEBUG")) {
                            // fprintf(stderr, "[DEBUG] Setting export: %s on object\n", name);
                        }
                        object_set_property(AS_OBJECT(exports), name, value);
                    } else {
                        if (getenv("SWIFTLANG_DEBUG")) {
                            // fprintf(stderr, "[DEBUG] __module_exports__ is not an object\n");
                        }
                    }
                } else {
                    if (getenv("SWIFTLANG_DEBUG")) {
                        // fprintf(stderr, "[DEBUG] __module_exports__ not found in globals\n");
                    }
                }
                break;
            }

        case OP_DEFINE_STRUCT:
            {
                // Read struct name constant index
                uint8_t name_const = READ_BYTE();
                
                // Get the appropriate chunk
                Chunk* chunk = (vm->frame_count == 1) ? vm->chunk : 
                              (frame->closure ? &frame->closure->function->chunk : 
                               &AS_FUNCTION(frame->slots[0])->chunk);
                
                if (name_const >= chunk->constants.count) {
                    runtime_error(vm, "Invalid constant index for struct name");
                    return INTERPRET_RUNTIME_ERROR;
                }
                TaggedValue name_val = chunk->constants.values[name_const];
                if (!IS_STRING(name_val)) {
                    runtime_error(vm, "Struct name must be a string");
                    return INTERPRET_RUNTIME_ERROR;
                }
                char* struct_name = AS_STRING(name_val);
                
                // Read field count
                uint8_t field_count = READ_BYTE();
                
                // Read field names from bytecode
                char** field_names = malloc(sizeof(char*) * field_count);
                for (int i = 0; i < field_count; i++) {
                    uint8_t field_const = READ_BYTE();
                    if (field_const >= chunk->constants.count) {
                        runtime_error(vm, "Invalid constant index for field name");
                        for (int j = 0; j < i; j++) free(field_names[j]);
                        free(field_names);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    TaggedValue field_val = chunk->constants.values[field_const];
                    if (!IS_STRING(field_val)) {
                        runtime_error(vm, "Struct field name must be a string");
                        for (int j = 0; j < i; j++) free(field_names[j]);
                        free(field_names);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                    field_names[i] = strdup(AS_STRING(field_val));
                }
                
                // Create struct type
                StructType* struct_type = struct_type_create(struct_name, field_names, field_count);
                
                // Register the struct type in VM
                if (vm->struct_types.count >= vm->struct_types.capacity) {
                    size_t new_capacity = vm->struct_types.capacity < 8 ? 8 : vm->struct_types.capacity * 2;
                    vm->struct_types.names = realloc(vm->struct_types.names, sizeof(char*) * new_capacity);
                    vm->struct_types.types = realloc(vm->struct_types.types, sizeof(StructType*) * new_capacity);
                    vm->struct_types.capacity = new_capacity;
                }
                
                vm->struct_types.names[vm->struct_types.count] = strdup(struct_name);
                vm->struct_types.types[vm->struct_types.count] = struct_type;
                vm->struct_types.count++;
                
                // Debug output
                // // fprintf(stderr, "[DEBUG] Defined struct '%s' with %d fields\n", struct_name, field_count);
                // for (int i = 0; i < field_count; i++) {
                //     // fprintf(stderr, "[DEBUG]   Field %d: %s\n", i, field_names[i]);
                // }
                
                // Don't push struct type on stack - it's registered in the VM
                break;
            }
            
        case OP_CREATE_STRUCT:
            {
                // Read struct type name
                char* type_name = READ_STRING();
                
                // Find struct type
                StructType* struct_type = NULL;
                for (size_t i = 0; i < vm->struct_types.count; i++) {
                    if (strcmp(vm->struct_types.names[i], type_name) == 0) {
                        struct_type = vm->struct_types.types[i];
                        break;
                    }
                }
                
                if (!struct_type) {
                    runtime_error(vm, "Unknown struct type: %s", type_name);
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Create struct instance as an object with the correct prototype
                Object* obj = object_create();
                
                // Set the struct prototype
                Object* struct_proto = get_struct_prototype(type_name);
                obj->prototype = struct_proto;
                
                // Store struct type name for debugging
                object_set_property(obj, "__struct_type__", STRING_VAL(strdup(type_name)));
                
                // Pop field values from stack and set as properties
                for (int i = struct_type->field_count - 1; i >= 0; i--) {
                    TaggedValue value = vm_pop(vm);
                    object_set_property(obj, struct_type->field_names[i], value);
                }
                
                // Push as object with struct prototype
                // // fprintf(stderr, "[DEBUG] Created struct instance of type '%s' as object with struct prototype %p\n", 
                //         struct_type->name, (void*)obj->prototype);
                vm_push(vm, OBJECT_VAL(obj));
                break;
            }
            
        case OP_GET_FIELD:
            {
                char* field_name = READ_STRING();
                TaggedValue instance_val = vm_pop(vm);
                
                // Since structs are now objects, just get the property
                if (IS_OBJECT(instance_val)) {
                    Object* obj = AS_OBJECT(instance_val);
                    TaggedValue* prop_value = object_get_property(obj, field_name);
                    if (prop_value) {
                        vm_push(vm, *prop_value);
                    } else {
                        runtime_error(vm, "Unknown field: %s", field_name);
                        return INTERPRET_RUNTIME_ERROR;
                    }
                } else {
                    runtime_error(vm, "Can only get fields from objects");
                    return INTERPRET_RUNTIME_ERROR;
                }
                break;
            }
            
        case OP_SET_FIELD:
            {
                char* field_name = READ_STRING();
                TaggedValue value = vm_pop(vm);
                TaggedValue instance_val = vm_pop(vm);
                
                // Since structs are now objects, just set the property
                if (IS_OBJECT(instance_val)) {
                    Object* obj = AS_OBJECT(instance_val);
                    object_set_property(obj, field_name, value);
                } else {
                    runtime_error(vm, "Can only set fields on objects");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Push the value back (assignment expressions return the value)
                vm_push(vm, value);
                break;
            }

        case OP_GET_OBJECT_PROTO:
            {
                // Push the object prototype onto the stack
                Object* proto = get_object_prototype();
                vm_push(vm, OBJECT_VAL(proto));
                break;
            }
            
        case OP_GET_STRUCT_PROTO:
            {
                // Pop struct name from stack
                TaggedValue name_val = vm_pop(vm);
                if (!IS_STRING(name_val)) {
                    runtime_error(vm, "Struct name must be a string");
                    return INTERPRET_RUNTIME_ERROR;
                }
                
                // Get or create struct prototype
                Object* proto = get_struct_prototype(AS_STRING(name_val));
                vm_push(vm, OBJECT_VAL(proto));
                break;
            }

        case OP_HALT:
            return INTERPRET_OK;

        default:
            runtime_error(vm, "Unknown opcode %d (0x%02x).", instruction, instruction);
            // Debug: print surrounding bytecode
            fprintf(stderr, "DEBUG: IP position in chunk, nearby bytes: ");
            uint8_t* debug_ip = frame->ip - 5;
            for (int i = 0; i < 10; i++) {
                if (debug_ip >= current_chunk->code && debug_ip < current_chunk->code + current_chunk->count) {
                    fprintf(stderr, "%02x ", *debug_ip);
                }
                debug_ip++;
            }
            fprintf(stderr, "\n");
            return INTERPRET_RUNTIME_ERROR;
        }
    }

    #undef READ_BYTE
    #undef READ_SHORT
    #undef READ_CONSTANT
    #undef READ_STRING
    #undef BINARY_OP
}

InterpretResult vm_interpret(VM* vm, Chunk* chunk)
{
    LOG_DEBUG(LOG_MODULE_VM, "Starting VM interpretation with chunk of %d bytes", chunk->count);
    vm->chunk = chunk;

    // Initialize the first call frame for the main script
    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->ip = chunk->code;
    frame->slots = vm->stack;
    frame->closure = NULL;  // Main script is not a closure
    frame->saved_module = vm->current_module; // Save current module (usually NULL for main)

    InterpretResult result = run(vm);
    LOG_DEBUG(LOG_MODULE_VM, "VM interpretation completed with result: %d", result);
    return result;
}

Function* function_create(const char* name, int arity)
{
    Function* function = malloc(sizeof(Function));
    function->name = strdup(name);
    function->arity = arity;
    function->upvalue_count = 0;
    function->module = NULL;  // Will be set when function is defined in a module
    chunk_init(&function->chunk);
    return function;
}

void function_free(Function* function)
{
    if (function)
    {
        chunk_free(&function->chunk);
        free((void*)function->name);
        free(function);
    }
}

// VM callback support implementation
TaggedValue vm_call_value(VM* vm, TaggedValue callee, int arg_count, TaggedValue* args)
{
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

// Helper function to execute a single function without recursing into run()
static TaggedValue execute_single_function(VM* vm, int initial_frame_count)
{
    // Save current frame pointer
    CallFrame* frame = &vm->frames[vm->frame_count - 1];
    
    // Execute until we return from this function
    while (vm->frame_count > initial_frame_count) {
        uint8_t instruction = *frame->ip++;
        
        // Debug: show what instruction we're executing
        if (instruction > OP_HALT) {
            // fprintf(stderr, "[DEBUG] execute_single_function: Invalid opcode %d (0x%02x) at IP offset %ld\n", 
            //         instruction, instruction, 
            //         (long)(frame->ip - 1 - (frame->closure ? frame->closure->function->chunk.code : NULL)));
        }
        
        // Debug all opcodes for now
        // fprintf(stderr, "[DEBUG] execute_single_function: opcode=%d (%s)\n", instruction,
        //         instruction == OP_GET_LOCAL ? "GET_LOCAL" :
        //         instruction == OP_GET_PROPERTY ? "GET_PROPERTY" :
        //         instruction == OP_CONSTANT ? "CONSTANT" :
        //         instruction == OP_RETURN ? "RETURN" :
        //         "OTHER");
        
        switch (instruction) {
            case OP_RETURN: {
                TaggedValue result = vm_pop(vm);
                vm->stack_top = frame->slots;
                vm->frame_count--;
                if (vm->frame_count > initial_frame_count) {
                    // Returning from a nested call within our function
                    frame = &vm->frames[vm->frame_count - 1];
                    vm_push(vm, result);
                } else {
                    // This is our final return
                    return result;
                }
                break;
            }
            
            case OP_CONSTANT: {
                uint8_t constant = *frame->ip++;
                Chunk* chunk;
                if (frame->closure) {
                    chunk = &frame->closure->function->chunk;
                } else {
                    // For non-closure calls, check if it's a function or closure in slots[0]
                    TaggedValue fn = frame->slots[0];
                    if (IS_FUNCTION(fn)) {
                        chunk = &AS_FUNCTION(fn)->chunk;
                    } else if (IS_CLOSURE(fn)) {
                        chunk = &AS_CLOSURE(fn)->function->chunk;
                    } else {
                        // This shouldn't happen
                        return NIL_VAL;
                    }
                }
                vm_push(vm, chunk->constants.values[constant]);
                break;
            }
            
            case OP_ADD: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) + AS_NUMBER(b)));
                } else if (IS_STRING(a) && IS_STRING(b)) {
                    // String concatenation
                    char* str_a = AS_STRING(a);
                    char* str_b = AS_STRING(b);
                    size_t len_a = strlen(str_a);
                    size_t len_b = strlen(str_b);
                    char* result = malloc(len_a + len_b + 1);
                    strcpy(result, str_a);
                    strcat(result, str_b);
                    vm_push(vm, STRING_VAL(result));
                } else {
                    // Type error
                    return NIL_VAL;
                }
                break;
            }
            
            case OP_MULTIPLY: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) * AS_NUMBER(b)));
                } else {
                    return NIL_VAL;
                }
                break;
            }
            
            case OP_GET_LOCAL: {
                uint8_t slot = *frame->ip++;
                vm_push(vm, frame->slots[slot]);
                break;
            }
            
            case OP_SET_LOCAL: {
                uint8_t slot = *frame->ip++;
                frame->slots[slot] = peek(vm, 0);
                break;
            }
            
            case OP_SUBTRACT: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) - AS_NUMBER(b)));
                } else {
                    return NIL_VAL;
                }
                break;
            }
            
            case OP_DIVIDE: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    if (AS_NUMBER(b) == 0) {
                        return NIL_VAL; // Division by zero
                    }
                    vm_push(vm, NUMBER_VAL(AS_NUMBER(a) / AS_NUMBER(b)));
                } else {
                    return NIL_VAL;
                }
                break;
            }
            
            case OP_MODULO: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    double divisor = AS_NUMBER(b);
                    if (divisor == 0) {
                        return NIL_VAL; // Division by zero
                    }
                    vm_push(vm, NUMBER_VAL(fmod(AS_NUMBER(a), divisor)));
                } else {
                    return NIL_VAL;
                }
                break;
            }
            
            case OP_EQUAL: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                vm_push(vm, BOOL_VAL(values_equal(a, b)));
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
                TaggedValue val = peek(vm, 0);
                vm_push(vm, val);
                break;
            }
            
            case OP_SWAP: {
                TaggedValue top = vm_pop(vm);
                TaggedValue second = vm_pop(vm);
                vm_push(vm, top);
                vm_push(vm, second);
                break;
            }
                
            case OP_JUMP_IF_FALSE: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                if (is_falsey(peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }
            
            case OP_JUMP_IF_TRUE: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                if (!is_falsey(peek(vm, 0))) {
                    frame->ip += offset;
                }
                break;
            }
            
            case OP_JUMP: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                frame->ip += offset;
                break;
            }
            
            case OP_LOOP: {
                uint16_t offset = (uint16_t)(frame->ip[0] << 8) | frame->ip[1];
                frame->ip += 2;
                frame->ip -= offset;
                break;
            }
            
            case OP_GET_PROPERTY: {
                // fprintf(stderr, "[DEBUG] execute_single_function: Entering OP_GET_PROPERTY\n");
                
                // OP_GET_PROPERTY expects the property name to already be on the stack
                // The compiler emits: OP_CONSTANT (pushes name), then OP_GET_PROPERTY
                TaggedValue name = vm_pop(vm);
                TaggedValue object = vm_pop(vm);
                
                if (!IS_STRING(name)) {
                    // fprintf(stderr, "[DEBUG] execute_single_function: Property name is not a string (type=%d)\n", name.type);
                    return NIL_VAL;
                }
                
                char* prop_name = AS_STRING(name);
                
                // fprintf(stderr, "[DEBUG] execute_single_function GET_PROPERTY: prop='%s', object_type=%d\n", prop_name, object.type);
                if (IS_OBJECT(object)) {
                    Object* obj = AS_OBJECT(object);
                    TaggedValue* value_ptr = object_get_property(obj, prop_name);
                    TaggedValue value = value_ptr ? *value_ptr : NIL_VAL;
                    // fprintf(stderr, "[DEBUG] execute_single_function GET_PROPERTY: found=%s, value_type=%d\n", 
                    //         value_ptr ? "yes" : "no", value.type);
                    vm_push(vm, value);
                } else if (IS_STRUCT(object)) {
                    // Structs are stored as STRUCT_VAL but internally are objects
                    Object* obj = AS_STRUCT(object);
                    TaggedValue* value_ptr = object_get_property(obj, prop_name);
                    TaggedValue value = value_ptr ? *value_ptr : NIL_VAL;
                    // fprintf(stderr, "[DEBUG] execute_single_function GET_PROPERTY on struct: found=%s, value_type=%d\n", 
                    //         value_ptr ? "yes" : "no", value.type);
                    vm_push(vm, value);
                } else {
                    // fprintf(stderr, "[DEBUG] execute_single_function GET_PROPERTY on non-object type %d\n", object.type);
                    return NIL_VAL;
                }
                break;
            }
            
            case OP_BUILD_ARRAY: {
                uint8_t count = *frame->ip++;
                Object* array = array_create();
                
                // Pop elements in reverse order and add to array
                for (int i = count - 1; i >= 0; i--) {
                    TaggedValue value = vm_pop(vm);
                    array_set(array, i, value);
                }
                
                vm_push(vm, OBJECT_VAL(array));
                break;
            }
            
            case OP_GET_SUBSCRIPT: {
                TaggedValue index = vm_pop(vm);
                TaggedValue object = vm_pop(vm);
                
                if (!IS_OBJECT(object) || !IS_NUMBER(index)) {
                    vm_push(vm, NIL_VAL);
                } else {
                    Object* obj = AS_OBJECT(object);
                    if (obj->is_array) {
                        int idx = (int)AS_NUMBER(index);
                        vm_push(vm, array_get(obj, idx));
                    } else {
                        vm_push(vm, NIL_VAL);
                    }
                }
                break;
            }
            
            case OP_DEFINE_LOCAL:
                // Local is already on the stack, just skip the slot
                frame->ip++;
                break;
            
            case OP_GREATER: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) > AS_NUMBER(b)));
                } else {
                    return NIL_VAL;
                }
                break;
            }
            
            case OP_GREATER_EQUAL: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) >= AS_NUMBER(b)));
                } else {
                    return NIL_VAL;
                }
                break;
            }
            
            case OP_LESS: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) < AS_NUMBER(b)));
                } else {
                    return NIL_VAL;
                }
                break;
            }
            
            case OP_LESS_EQUAL: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                if (IS_NUMBER(a) && IS_NUMBER(b)) {
                    vm_push(vm, BOOL_VAL(AS_NUMBER(a) <= AS_NUMBER(b)));
                } else {
                    return NIL_VAL;
                }
                break;
            }
            
            case OP_NOT_EQUAL: {
                TaggedValue b = vm_pop(vm);
                TaggedValue a = vm_pop(vm);
                vm_push(vm, BOOL_VAL(!values_equal(a, b)));
                break;
            }
            
            case OP_TO_STRING: {
                TaggedValue value = vm_pop(vm);
                if (IS_STRING(value)) {
                    vm_push(vm, value);
                } else if (IS_NUMBER(value)) {
                    char buffer[32];
                    double num = AS_NUMBER(value);
                    if (num == (long)num) {
                        snprintf(buffer, sizeof(buffer), "%ld", (long)num);
                    } else {
                        snprintf(buffer, sizeof(buffer), "%.6g", num);
                    }
                    vm_push(vm, STRING_VAL(strdup(buffer)));
                } else if (IS_BOOL(value)) {
                    vm_push(vm, STRING_VAL(strdup(AS_BOOL(value) ? "true" : "false")));
                } else if (IS_NIL(value)) {
                    vm_push(vm, STRING_VAL(strdup("nil")));
                } else {
                    vm_push(vm, STRING_VAL(strdup("<object>")));
                }
                break;
            }
            
            case OP_METHOD_CALL: {
                uint8_t arg_count = *frame->ip++;
                // Stack layout: [method, object, arg1, arg2, ...]
                TaggedValue method = peek(vm, arg_count + 1);
                
                // fprintf(stderr, "[DEBUG] execute_single_function OP_METHOD_CALL: method type = %d\n", method.type);
                
                if (IS_FUNCTION(method) || IS_CLOSURE(method)) {
                    // For functions/closures, we need to call them
                    // This creates a new frame
                    Function* function = IS_CLOSURE(method) ? AS_CLOSURE(method)->function : AS_FUNCTION(method);
                    
                    // Check arity
                    int expected_args = function->arity;
                    int actual_args = arg_count;
                    
                    if (strstr(function->name, "_ext_") != NULL) {
                        // Extension method: function expects 'this' as first arg
                        actual_args = arg_count + 1; // +1 for the object/'this'
                    }
                    
                    if (actual_args != expected_args) {
                        return NIL_VAL; // Arity mismatch
                    }
                    
                    // Check for stack overflow
                    if (vm->frame_count == FRAMES_MAX) {
                        return NIL_VAL;
                    }
                    
                    // Set up new call frame
                    CallFrame* new_frame = &vm->frames[vm->frame_count++];
                    new_frame->ip = function->chunk.code;
                    new_frame->slots = vm->stack_top - arg_count - 2; // -2 for method and object
                    new_frame->saved_module = vm->current_module;
                    
                    if (IS_CLOSURE(method)) {
                        new_frame->closure = AS_CLOSURE(method);
                    } else {
                        new_frame->closure = NULL;
                    }
                    
                    // Update frame pointer for next iteration
                    frame = new_frame;
                } else {
                    return NIL_VAL; // Can't call non-function as method
                }
                break;
            }
            
            case OP_CONSTANT_LONG: {
                uint8_t low = *frame->ip++;
                uint8_t high = *frame->ip++;
                uint16_t constant_index = (uint16_t)(low | (high << 8));
                Chunk* chunk;
                if (frame->closure) {
                    chunk = &frame->closure->function->chunk;
                } else {
                    // For non-closure calls, check if it's a function or closure in slots[0]
                    TaggedValue fn = frame->slots[0];
                    if (IS_FUNCTION(fn)) {
                        chunk = &AS_FUNCTION(fn)->chunk;
                    } else if (IS_CLOSURE(fn)) {
                        chunk = &AS_CLOSURE(fn)->function->chunk;
                    } else {
                        // This shouldn't happen
                        // fprintf(stderr, "[DEBUG] OP_CONSTANT_LONG: No valid function/closure in slots[0]\n");
                        return NIL_VAL;
                    }
                }
                // // fprintf(stderr, "[DEBUG] OP_CONSTANT_LONG: index=%d, chunk=%p\n", constant_index, (void*)chunk);
                if (constant_index >= chunk->constants.count) {
                    // // fprintf(stderr, "[DEBUG] OP_CONSTANT_LONG: Constant index %d out of bounds (count=%d)\n", 
                    //         constant_index, chunk->constants.count);
                    return NIL_VAL;
                }
                vm_push(vm, chunk->constants.values[constant_index]);
                break;
            }
            
            // Add more opcodes as needed for basic lambda execution
            default:
                // For now, return NIL for unsupported operations
                // fprintf(stderr, "[DEBUG] Unsupported opcode in lambda: %d\n", instruction);
                return NIL_VAL;
        }
    }
    
    return NIL_VAL;
}

/**
 * Unified function for calling both functions and closures.
 * 
 * This function provides a single implementation for calling any callable value,
 * eliminating code duplication between vm_call_function and vm_call_closure.
 * It handles:
 * - Argument validation (arity checking)
 * - Stack overflow detection
 * - Module context switching
 * - Call frame setup
 * - Proper cleanup and restoration of VM state
 * 
 * The function supports both regular functions and closures transparently,
 * making the VM's calling convention more uniform and maintainable.
 * 
 * @param vm The VM instance
 * @param callable A FUNCTION_VAL or CLOSURE_VAL to call
 * @param arg_count Number of arguments being passed
 * @param args Array of argument values
 * @return The function's return value, or NIL_VAL on error
 */
static TaggedValue vm_call_callable(VM* vm, TaggedValue callable, int arg_count, TaggedValue* args)
{
    Function* function = NULL;
    Closure* closure = NULL;
    
    // Extract the function from either a function or closure value
    if (IS_FUNCTION(callable)) {
        function = AS_FUNCTION(callable);
    } else if (IS_CLOSURE(callable)) {
        closure = AS_CLOSURE(callable);
        function = closure->function;
    } else {
        // Not a callable value
        return NIL_VAL;
    }
    
    // Check arity
    if (arg_count != function->arity) {
        // Arity mismatch
        return NIL_VAL;
    }
    
    // Check for stack overflow
    if (vm->frame_count == FRAMES_MAX) {
        return NIL_VAL;
    }
    
    // Save initial frame count
    int initial_frame_count = vm->frame_count;
    
    // Save current module context
    struct Module* saved_module = vm->current_module;
    
    // If this function belongs to a module, switch to that module's context
    if (function->module) {
        vm->current_module = function->module;
    }
    
    // Push the callable value (required by VM calling convention)
    vm_push(vm, callable);
    
    // Push arguments
    for (int i = 0; i < arg_count; i++) {
        vm_push(vm, args[i]);
    }
    
    // Set up new call frame
    CallFrame* frame = &vm->frames[vm->frame_count++];
    frame->ip = function->chunk.code;
    frame->slots = vm->stack_top - arg_count - 1;
    frame->closure = closure;  // NULL for regular functions, actual closure for closures
    frame->saved_module = saved_module;
    
    // Execute the function
    TaggedValue result = execute_single_function(vm, initial_frame_count);
    
    // Clean up stack
    vm->stack_top = frame->slots;
    
    // Restore frame count  
    vm->frame_count = initial_frame_count;
    
    // Restore previous module context
    vm->current_module = saved_module;
    
    return result;
}

/**
 * Public wrapper for calling functions.
 * Delegates to the unified vm_call_callable function.
 */
TaggedValue vm_call_function(VM* vm, Function* function, int arg_count, TaggedValue* args)
{
    return vm_call_callable(vm, FUNCTION_VAL(function), arg_count, args);
}

/**
 * Public wrapper for calling closures.
 * Delegates to the unified vm_call_callable function.
 */
TaggedValue vm_call_closure(VM* vm, Closure* closure, int arg_count, TaggedValue* args)
{
    TaggedValue closure_val = {.type = VAL_CLOSURE, .as.closure = closure};
    return vm_call_callable(vm, closure_val, arg_count, args);
}
