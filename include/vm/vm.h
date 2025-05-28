#ifndef VM_H
#define VM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "vm/string_pool.h"
#include "vm/object.h"

#define STACK_MAX 256
#define FRAMES_MAX 64

typedef enum {
    // Constants
    OP_CONSTANT,
    OP_NIL,
    OP_TRUE,
    OP_FALSE,
    
    // Stack operations
    OP_POP,
    OP_DUP,
    OP_SWAP,
    
    // Arithmetic
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_NEGATE,
    
    // Comparison
    OP_EQUAL,
    OP_NOT_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    
    // Logical
    OP_NOT,
    OP_AND,
    OP_OR,
    
    // Bitwise
    OP_BIT_AND,
    OP_BIT_OR,
    OP_BIT_XOR,
    OP_BIT_NOT,
    OP_SHIFT_LEFT,
    OP_SHIFT_RIGHT,
    
    // Variables
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_DEFINE_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSE_UPVALUE,
    
    // Control flow
    OP_JUMP,
    OP_JUMP_IF_FALSE,
    OP_JUMP_IF_TRUE,
    OP_LOOP,
    
    // Functions
    OP_FUNCTION,
    OP_CLOSURE,
    OP_CALL,
    OP_METHOD_CALL,
    OP_RETURN,
    OP_LOAD_BUILTIN,
    
    // Arrays
    OP_ARRAY,  // Same as BUILD_ARRAY
    OP_BUILD_ARRAY,
    OP_GET_SUBSCRIPT,
    OP_SET_SUBSCRIPT,
    
    // Objects
    OP_CREATE_OBJECT,
    OP_GET_PROPERTY,
    OP_SET_PROPERTY,
    OP_SET_PROTOTYPE,
    
    // Structs
    OP_DEFINE_STRUCT,
    OP_CREATE_STRUCT,
    OP_GET_FIELD,
    OP_SET_FIELD,
    
    // Prototypes
    OP_GET_OBJECT_PROTO,
    OP_GET_STRUCT_PROTO,
    
    // Optionals
    OP_OPTIONAL_CHAIN,
    OP_FORCE_UNWRAP,
    
    // Iterators
    OP_GET_ITER,
    OP_FOR_ITER,
    
    // Locals
    OP_DEFINE_LOCAL,
    
    // Async
    OP_AWAIT,
    
    // Modules
    OP_LOAD_MODULE,
    OP_LOAD_NATIVE_MODULE,
    OP_IMPORT_FROM,
    OP_IMPORT_ALL_FROM,
    OP_MODULE_EXPORT,
    
    // Long constants
    OP_CONSTANT_LONG,
    
    // Type conversion
    OP_TO_STRING,
    
    // Misc
    OP_PRINT,
    OP_HALT
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

typedef struct {
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
