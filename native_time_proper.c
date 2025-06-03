#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

// Forward declarations for SwiftLang types
typedef struct VM VM;
typedef struct Module Module;
typedef struct TaggedValue TaggedValue;

// Native function signature
typedef TaggedValue (*NativeFn)(VM* vm, int arg_count, TaggedValue* args);

// Module export function
void module_export(Module* module, const char* name, TaggedValue value);

// Value creation helpers
TaggedValue create_native_function(NativeFn fn, const char* name, int arity);
TaggedValue create_number_value(double num);
TaggedValue create_string_value(const char* str);

// Native function implementations
static TaggedValue native_time_now(VM* vm, int arg_count, TaggedValue* args) {
    (void)vm; (void)arg_count; (void)args;
    
    time_t current_time = time(NULL);
    return create_number_value((double)current_time);
}

static TaggedValue native_time_format(VM* vm, int arg_count, TaggedValue* args) {
    (void)vm; (void)args;
    
    time_t timestamp;
    if (arg_count > 0) {
        // Use provided timestamp
        // Note: In real implementation, we'd need to extract the number from args[0]
        timestamp = (time_t)time(NULL); // For now, use current time
    } else {
        timestamp = time(NULL);
    }
    
    struct tm* time_info = localtime(&timestamp);
    char* time_str = malloc(64);
    strftime(time_str, 64, "%Y-%m-%d %H:%M:%S", time_info);
    
    return create_string_value(time_str);
}

// Module initialization
bool swiftlang_module_init(Module* module) {
    printf("Initializing native time module with proper exports\n");
    
    // Export native functions
    module_export(module, "now", 
        create_native_function(native_time_now, "now", 0));
    
    module_export(module, "format", 
        create_native_function(native_time_format, "format", 1));
    
    return true;
}