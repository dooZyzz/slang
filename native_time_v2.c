// Native time module for SwiftLang
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

// SwiftLang's actual value types from vm.h
typedef enum {
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_STRING,
    VAL_OBJECT,
    VAL_FUNCTION,
    VAL_CLOSURE,
    VAL_NATIVE,
    VAL_STRUCT,
    VAL_MODULE
} ValueType;

typedef union {
    bool boolean;
    double number;
    char* string;
    void* object;
    void* function;
    void* closure;
    void* native;
    void* module;
} Value;

typedef struct {
    ValueType type;
    Value as;
} TaggedValue;

// Value creation macros
#define NUMBER_VAL(value) ((TaggedValue){VAL_NUMBER, {.number = value}})
#define STRING_VAL(value) ((TaggedValue){VAL_STRING, {.string = value}})
#define NATIVE_VAL(value) ((TaggedValue){VAL_NATIVE, {.native = value}})

// Forward declaration
typedef struct Module Module;

// Module export function declaration
extern void module_export(Module* module, const char* name, TaggedValue value);

// Native function: get current time as number
static TaggedValue native_time_now(int arg_count, TaggedValue* args) {
    (void)arg_count;
    (void)args;
    
    time_t current_time = time(NULL);
    return NUMBER_VAL((double)current_time);
}

// Native function: get formatted time string
static TaggedValue native_time_format(int arg_count, TaggedValue* args) {
    (void)arg_count;
    (void)args;
    
    time_t current_time = time(NULL);
    struct tm* time_info = localtime(&current_time);
    
    static char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time_info);
    
    return STRING_VAL(time_buffer);
}

// Module initialization
bool swiftlang_module_init(Module* module) {
    printf("Native time module: Registering functions\n");
    
    // Export native functions
    module_export(module, "now", NATIVE_VAL(native_time_now));
    module_export(module, "format", NATIVE_VAL(native_time_format));
    
    printf("Native time module: Exported 'now' and 'format' functions\n");
    
    return true;
}