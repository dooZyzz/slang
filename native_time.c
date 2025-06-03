#include <stdio.h>
#include <time.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// SwiftLang value system
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

typedef union {
    bool boolean;
    double number;
    char* string;
    void* object;
    void* function;
    void* closure;
    void* native;
} Value;

typedef struct {
    ValueType type;
    Value as;
} TaggedValue;

// Helper macros
#define NUMBER_VAL(value) ((TaggedValue){VAL_NUMBER, {.number = value}})
#define STRING_VAL(value) ((TaggedValue){VAL_STRING, {.string = value}})
#define NIL_VAL ((TaggedValue){VAL_NIL, {.number = 0}})

// Native function for getting current time
TaggedValue native_get_time(int arg_count, TaggedValue* args) {
    (void)arg_count; // Unused
    (void)args;      // Unused
    
    time_t current_time = time(NULL);
    return NUMBER_VAL((double)current_time);
}

// Native function for getting formatted time
TaggedValue native_get_time_string(int arg_count, TaggedValue* args) {
    (void)arg_count; // Unused
    (void)args;      // Unused
    
    time_t current_time = time(NULL);
    struct tm* time_info = localtime(&current_time);
    
    static char time_buffer[64];
    strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", time_info);
    
    return STRING_VAL(time_buffer);
}

// Module structure
typedef struct Module Module;

// Module initialization function
bool swiftlang_module_init(Module* module);

bool swiftlang_module_init(Module* module) {
    printf("Initializing native time module\n");
    
    // This is where we would register native functions
    // For now, we'll just print a message
    
    return true;
}