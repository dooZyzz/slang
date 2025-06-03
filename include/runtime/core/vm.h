#ifndef VM_H
#define VM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "runtime/core/string_pool.h"
#include "runtime/core/object.h"

#define STACK_MAX 256
#define FRAMES_MAX 64

#define STACK_MAX 256
#define FRAMES_MAX 64

typedef enum {
    // Constants
    OP_CONSTANT = 0,
    OP_NIL = 1,
    OP_TRUE = 2,
    OP_FALSE = 3,

    // Stack operations
    OP_POP = 4,
    OP_DUP = 5,
    OP_SWAP = 6,

    // Arithmetic
    OP_ADD = 7,
    OP_SUBTRACT = 8,
    OP_MULTIPLY = 9,
    OP_DIVIDE = 10,
    OP_MODULO = 11,
    OP_NEGATE = 12,

    // Comparison
    OP_EQUAL = 13,
    OP_NOT_EQUAL = 14,
    OP_GREATER = 15,
    OP_GREATER_EQUAL = 16,
    OP_LESS = 17,
    OP_LESS_EQUAL = 18,

    // Logical
    OP_NOT = 19,
    OP_AND = 20,
    OP_OR = 21,

    // Bitwise
    OP_BIT_AND = 22,
    OP_BIT_OR = 23,
    OP_BIT_XOR = 24,
    OP_BIT_NOT = 25,
    OP_SHIFT_LEFT = 26,
    OP_SHIFT_RIGHT = 27,

    // Variables
    OP_GET_LOCAL = 28,
    OP_SET_LOCAL = 29,
    OP_GET_GLOBAL = 30,
    OP_SET_GLOBAL = 31,
    OP_DEFINE_GLOBAL = 32,
    OP_GET_UPVALUE = 33,
    OP_SET_UPVALUE = 34,
    OP_CLOSE_UPVALUE = 35,

    // Control flow
    OP_JUMP = 36,
    OP_JUMP_IF_FALSE = 37,
    OP_JUMP_IF_TRUE = 38,
    OP_LOOP = 39,

    // Functions
    OP_FUNCTION = 40,
    OP_CLOSURE = 41,
    OP_CALL = 42,
    OP_METHOD_CALL = 43,
    OP_RETURN = 44,
    OP_LOAD_BUILTIN = 45,

    // Arrays
    OP_ARRAY = 46,  // Same as BUILD_ARRAY
    OP_BUILD_ARRAY = 47,
    OP_GET_SUBSCRIPT = 48,
    OP_SET_SUBSCRIPT = 49,

    // Objects
    OP_CREATE_OBJECT = 50,
    OP_GET_PROPERTY = 51,
    OP_SET_PROPERTY = 52,
    OP_SET_PROTOTYPE = 53,

    // Structs
    OP_DEFINE_STRUCT = 54,
    OP_CREATE_STRUCT = 55,
    OP_GET_FIELD = 56,
    OP_SET_FIELD = 57,

    // Prototypes
    OP_GET_OBJECT_PROTO = 58,
    OP_GET_STRUCT_PROTO = 59,

    // Optionals
    OP_OPTIONAL_CHAIN = 60,
    OP_FORCE_UNWRAP = 61,

    // Iterators
    OP_GET_ITER = 62,
    OP_FOR_ITER = 63,

    // Locals
    OP_DEFINE_LOCAL = 64,

    // Async
    OP_AWAIT = 65,

    // Modules
    OP_LOAD_MODULE = 66,
    OP_LOAD_NATIVE_MODULE = 67,
    OP_IMPORT_FROM = 68,
    OP_IMPORT_ALL_FROM = 69,
    OP_MODULE_EXPORT = 70,

    // Long constants
    OP_CONSTANT_LONG = 71,
    OP_CLOSURE_LONG = 72,

    // Type conversion
    OP_TO_STRING = 73,

    // String operations
    OP_STRING_CONCAT = 74,
    OP_STRING_INTERP = 75,
    OP_INTERN_STRING = 76,

    // Math extensions
    OP_POWER = 77,

    // Array operations
    OP_LENGTH = 78,

    // Object construction
    OP_OBJECT_LITERAL = 79,

    // Misc
    OP_HALT = 80
} OpCode;

// Forward declarations
typedef struct Function Function;
typedef struct Closure Closure;

typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_OBJECT,
    VAL_FUNCTION,
    VAL_CLOSURE,
    VAL_NATIVE,
    VAL_STRUCT
} ValueType;

typedef struct TaggedValue TaggedValue;

typedef TaggedValue (*NativeFn)(int arg_count, TaggedValue* args);

typedef union {
    bool boolean;
    double number;
    char* string;
    void* object;
    Function* function;
    Closure* closure;
    NativeFn native;
} Value;

typedef struct TaggedValue {
    ValueType type;
    Value as;
} TaggedValue;

typedef struct {
    uint8_t* code;
    size_t count;
    size_t capacity;
    int* lines;
    
    struct {
        TaggedValue* values;
        size_t count;
        size_t capacity;
    } constants;
} Chunk;

typedef struct Upvalue {
    TaggedValue* location;
    TaggedValue closed;
    struct Upvalue* next;
} Upvalue;

struct Closure {
    Function* function;
    Upvalue** upvalues;
    int upvalue_count;
};

struct Function {
    Chunk chunk;
    const char* name;
    int arity;
    int upvalue_count;
    struct Module* module; // Module this function belongs to (NULL for non-module functions)
};

typedef struct {
    uint8_t* ip;
    TaggedValue* slots;
    Closure* closure;
    struct Module* saved_module; // Saved module context from caller
} CallFrame;

// Forward declaration
typedef struct ModuleLoader ModuleLoader;

// Forward declare GC
typedef struct GarbageCollector GarbageCollector;

typedef struct VM {
    Chunk* chunk;
    uint8_t* ip;
    
    TaggedValue stack[STACK_MAX];
    TaggedValue* stack_top;
    
    CallFrame frames[FRAMES_MAX];
    int frame_count;
    
    struct {
        char** names;
        TaggedValue* values;
        size_t count;
        size_t capacity;
    } globals;
    
    struct {
        char** names;
        StructType** types;
        size_t count;
        size_t capacity;
    } struct_types;
    
    Upvalue* open_upvalues;
    StringPool strings;
    ModuleLoader* module_loader;
    
    // Current module path for relative imports
    const char* current_module_path;
    
    // Current module context (for accessing module globals)
    struct Module* current_module;
    
    // Debug trace flag
    bool debug_trace;
    
    // Garbage collector
    GarbageCollector* gc;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

// Chunk functions
void chunk_init(Chunk* chunk);
void chunk_free(Chunk* chunk);
void chunk_write(Chunk* chunk, uint8_t byte, int line);
int chunk_add_constant(Chunk* chunk, TaggedValue value);

// VM functions
void vm_init(VM* vm);
void vm_init_with_loader(VM* vm, ModuleLoader* loader);
void vm_free(VM* vm);
VM* vm_create(void);
void vm_destroy(VM* vm);
InterpretResult vm_interpret(VM* vm, Chunk* chunk);
InterpretResult vm_interpret_function(VM* vm, Function* function);
void vm_push(VM* vm, TaggedValue value);
TaggedValue vm_pop(VM* vm);

// Value helpers
#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)
#define IS_STRING(value)  ((value).type == VAL_STRING)
#define IS_OBJECT(value)  ((value).type == VAL_OBJECT)
#define IS_FUNCTION(value) ((value).type == VAL_FUNCTION)
#define IS_CLOSURE(value)  ((value).type == VAL_CLOSURE)
#define IS_NATIVE(value)  ((value).type == VAL_NATIVE)
#define IS_STRUCT(value)  ((value).type == VAL_STRUCT)

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)
#define AS_STRING(value)  ((value).as.string)
#define AS_OBJECT(value)  ((Object*)(value).as.object)
#define AS_FUNCTION(value) ((value).as.function)
#define AS_CLOSURE(value)  ((value).as.closure)
#define AS_NATIVE(value)  ((value).as.native)
#define AS_STRUCT(value)  ((StructInstance*)(value).as.object)

#define BOOL_VAL(value)   ((TaggedValue){VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((TaggedValue){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((TaggedValue){VAL_NUMBER, {.number = value}})
#define STRING_VAL(value) ((TaggedValue){VAL_STRING, {.string = value}})
#define OBJECT_VAL(value) ((TaggedValue){VAL_OBJECT, {.object = value}})
#define FUNCTION_VAL(value) ((TaggedValue){VAL_FUNCTION, {.function = value}})
#define NATIVE_VAL(value) ((TaggedValue){VAL_NATIVE, {.native = value}})
#define STRUCT_VAL(value) ((TaggedValue){VAL_STRUCT, {.object = value}})

void print_value(TaggedValue value);
bool values_equal(TaggedValue a, TaggedValue b);

// Function helpers
Function* function_create(const char* name, int arity);
void function_free(Function* function);

// Output redirection for testing
typedef void (*PrintHook)(const char* str);
void vm_set_print_hook(PrintHook hook);
void vm_print(const char* str);

// VM callback support - allows native functions to call VM functions
TaggedValue vm_call_value(VM* vm, TaggedValue callee, int arg_count, TaggedValue* args);
TaggedValue vm_call_function(VM* vm, Function* function, int arg_count, TaggedValue* args);
TaggedValue vm_call_closure(VM* vm, Closure* closure, int arg_count, TaggedValue* args);

// Global variable management
void define_global(VM* vm, const char* name, TaggedValue value);
void undefine_global(VM* vm, const char* name);

#endif
