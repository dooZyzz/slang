#include "stdlib/stdlib.h"
#include "runtime/module_inspect.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

// Helper to check if a value is truthy
#define IS_TRUTHY(value) (!IS_NIL(value) && (!IS_BOOL(value) || AS_BOOL(value)))

// Global VM pointer for callbacks
static VM* g_vm = NULL;

void stdlib_set_vm(VM* vm) {
    g_vm = vm;
}

// Main stdlib initialization
void stdlib_init(VM* vm) {
    
    // Initialize builtin prototypes first
    init_builtin_prototypes();
    
    // Then add stdlib methods to each prototype
    stdlib_init_object_prototype(get_object_prototype());
    stdlib_init_array_prototype(get_array_prototype());
    stdlib_init_string_prototype(get_string_prototype());
    stdlib_init_function_prototype(get_function_prototype());
    
    // Register global functions
    
    // Register module introspection natives
    register_module_natives(vm);
    
    // Note: Global function registration would need to be done in vm_init
    // For now, these functions can be accessed through module imports
    // or added to a global object
}

// Object prototype methods
void stdlib_init_object_prototype(Object* proto) {
    // TODO: Add toString, valueOf, hasOwnProperty, etc.
}

// Array prototype methods
void stdlib_init_array_prototype(Object* proto) {
    // Basic array methods
    object_set_property(proto, "push", NATIVE_VAL(array_push_method));
    object_set_property(proto, "pop", NATIVE_VAL(array_pop_method));
    object_set_property(proto, "length", NATIVE_VAL(array_length_method));
    
    // Higher-order methods
    object_set_property(proto, "map", NATIVE_VAL(array_map_method));
    object_set_property(proto, "filter", NATIVE_VAL(array_filter_method));
    object_set_property(proto, "reduce", NATIVE_VAL(array_reduce_method));
    
    // Additional array methods
    object_set_property(proto, "count", NATIVE_VAL(array_length_method));  // count is an alias for length
    object_set_property(proto, "isEmpty", NATIVE_VAL(array_isEmpty_method));
}

// String prototype methods
void stdlib_init_string_prototype(Object* proto) {
    object_set_property(proto, "length", NATIVE_VAL(string_length_method));
    object_set_property(proto, "charAt", NATIVE_VAL(string_charAt_method));
    object_set_property(proto, "indexOf", NATIVE_VAL(string_indexOf_method));
    object_set_property(proto, "substring", NATIVE_VAL(string_substring_method));
    object_set_property(proto, "toUpperCase", NATIVE_VAL(string_toUpperCase_method));
    object_set_property(proto, "toLowerCase", NATIVE_VAL(string_toLowerCase_method));
    object_set_property(proto, "split", NATIVE_VAL(string_split_method));
    object_set_property(proto, "trim", NATIVE_VAL(string_trim_method));
}

// Function prototype methods
void stdlib_init_function_prototype(Object* proto) {
    // TODO: Add call, apply, bind
}

// Array method implementations
TaggedValue array_push_method(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_OBJECT(args[0])) return NIL_VAL;
    
    Object* array = AS_OBJECT(args[0]);
    if (!array->is_array) return NIL_VAL;
    
    // Push all arguments (starting from index 1)
    for (int i = 1; i < arg_count; i++) {
        array_push(array, args[i]);
    }
    
    return NUMBER_VAL((double)array_length(array));
}

TaggedValue array_pop_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_OBJECT(args[0])) return NIL_VAL;
    
    Object* array = AS_OBJECT(args[0]);
    if (!array->is_array) return NIL_VAL;
    
    return array_pop(array);
}

TaggedValue array_length_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_OBJECT(args[0])) return NIL_VAL;
    
    Object* array = AS_OBJECT(args[0]);
    if (!array->is_array) return NIL_VAL;
    
    return NUMBER_VAL((double)array_length(array));
}

TaggedValue array_map_method(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_OBJECT(args[0])) return NIL_VAL;
    
    Object* array = AS_OBJECT(args[0]);
    if (!array->is_array) return NIL_VAL;
    
    TaggedValue callback = args[1];
    if (!IS_FUNCTION(callback) && !IS_CLOSURE(callback) && !IS_NATIVE(callback)) {
        return NIL_VAL;
    }
    
    size_t length = array_length(array);
    Object* result = array_create();
    
    // Call callback for each element
    for (size_t i = 0; i < length; i++) {
        TaggedValue element = array_get(array, i);
        
        // Call with (element, index, array)
        TaggedValue callback_args[3];
        callback_args[0] = element;
        callback_args[1] = NUMBER_VAL((double)i);
        callback_args[2] = args[0];
        
        TaggedValue mapped;
        if (g_vm && !IS_NATIVE(callback)) {
            // For user functions, only pass the element (they expect 1 arg)
            // Debug: Print VM state before call
            // fprintf(stderr, "[DEBUG] array_map before vm_call_value: frame_count=%d\n", g_vm->frame_count);
            mapped = vm_call_value(g_vm, callback, 1, callback_args);
            // fprintf(stderr, "[DEBUG] array_map after vm_call_value: frame_count=%d\n", g_vm->frame_count);
        } else if (IS_NATIVE(callback)) {
            // Native callbacks get all 3 args (element, index, array)
            NativeFn native = AS_NATIVE(callback);
            mapped = native(3, callback_args);
        } else {
            mapped = element; // Can't call without VM
        }
        
        array_push(result, mapped);
    }
    
    return OBJECT_VAL(result);
}

TaggedValue array_filter_method(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_OBJECT(args[0])) return NIL_VAL;
    
    Object* array = AS_OBJECT(args[0]);
    if (!array->is_array) return NIL_VAL;
    
    TaggedValue callback = args[1];
    if (!IS_FUNCTION(callback) && !IS_CLOSURE(callback) && !IS_NATIVE(callback)) {
        return NIL_VAL;
    }
    
    size_t length = array_length(array);
    Object* result = array_create();
    
    // Call callback for each element
    for (size_t i = 0; i < length; i++) {
        TaggedValue element = array_get(array, i);
        
        // Call with (element, index, array)
        TaggedValue callback_args[3];
        callback_args[0] = element;
        callback_args[1] = NUMBER_VAL((double)i);
        callback_args[2] = args[0];
        
        TaggedValue should_include;
        if (g_vm && !IS_NATIVE(callback)) {
            // For user functions, only pass the element (they expect 1 arg)
            should_include = vm_call_value(g_vm, callback, 1, callback_args);
            // Debug: Check what the callback returned
            // if (should_include.type == VAL_NIL) {
            //     fprintf(stderr, "[DEBUG] Filter callback returned NIL for element %f\n", AS_NUMBER(element));
            // }
        } else if (IS_NATIVE(callback)) {
            // Native callbacks get all 3 args (element, index, array)
            NativeFn native = AS_NATIVE(callback);
            should_include = native(3, callback_args);
        } else {
            // fprintf(stderr, "[DEBUG] No VM available for filter callback\n");
            should_include = BOOL_VAL(false); // Can't call without VM
        }
        
        if (IS_TRUTHY(should_include)) {
            array_push(result, element);
        }
    }
    
    return OBJECT_VAL(result);
}

TaggedValue array_reduce_method(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_OBJECT(args[0])) return NIL_VAL;
    
    Object* array = AS_OBJECT(args[0]);
    if (!array->is_array) return NIL_VAL;
    
    TaggedValue callback = args[1];
    if (!IS_FUNCTION(callback) && !IS_CLOSURE(callback) && !IS_NATIVE(callback)) {
        return NIL_VAL;
    }
    
    size_t length = array_length(array);
    if (length == 0) {
        return arg_count >= 3 ? args[2] : NIL_VAL;
    }
    
    size_t start_index = 0;
    TaggedValue accumulator;
    
    if (arg_count >= 3) {
        // Initial value provided
        accumulator = args[2];
    } else {
        // No initial value, use first element
        accumulator = array_get(array, 0);
        start_index = 1;
    }
    
    // Call callback for each element
    for (size_t i = start_index; i < length; i++) {
        TaggedValue element = array_get(array, i);
        
        // Call with (accumulator, element, index, array)
        TaggedValue callback_args[4];
        callback_args[0] = accumulator;
        callback_args[1] = element;
        callback_args[2] = NUMBER_VAL((double)i);
        callback_args[3] = args[0];
        
        if (g_vm && !IS_NATIVE(callback)) {
            // For user functions, only pass accumulator and element (they expect 2 args)
            accumulator = vm_call_value(g_vm, callback, 2, callback_args);
        } else if (IS_NATIVE(callback)) {
            // Native callbacks get all 4 args
            NativeFn native = AS_NATIVE(callback);
            accumulator = native(4, callback_args);
        } else {
            return accumulator; // Can't call without VM
        }
    }
    
    return accumulator;
}

// String method implementations
TaggedValue string_length_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* str = AS_STRING(args[0]);
    return NUMBER_VAL((double)strlen(str));
}

TaggedValue string_charAt_method(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_STRING(args[0]) || !IS_NUMBER(args[1])) return NIL_VAL;
    
    const char* str = AS_STRING(args[0]);
    int index = (int)AS_NUMBER(args[1]);
    
    if (index < 0 || index >= (int)strlen(str)) {
        return STRING_VAL(strdup(""));
    }
    
    char result[2] = { str[index], '\0' };
    return STRING_VAL(strdup(result));
}

TaggedValue string_indexOf_method(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) return NIL_VAL;
    
    const char* str = AS_STRING(args[0]);
    const char* search = AS_STRING(args[1]);
    
    const char* found = strstr(str, search);
    if (found) {
        return NUMBER_VAL((double)(found - str));
    }
    
    return NUMBER_VAL(-1);
}

TaggedValue string_substring_method(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_STRING(args[0]) || !IS_NUMBER(args[1])) return NIL_VAL;
    
    const char* str = AS_STRING(args[0]);
    int start = (int)AS_NUMBER(args[1]);
    int len = strlen(str);
    
    if (start < 0) start = 0;
    if (start >= len) return STRING_VAL(strdup(""));
    
    int end = len;
    if (arg_count >= 3 && IS_NUMBER(args[2])) {
        end = (int)AS_NUMBER(args[2]);
        if (end < start) end = start;
        if (end > len) end = len;
    }
    
    int result_len = end - start;
    char* result = malloc(result_len + 1);
    memcpy(result, str + start, result_len);
    result[result_len] = '\0';
    
    return STRING_VAL(result);
}

TaggedValue string_toUpperCase_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* str = AS_STRING(args[0]);
    char* result = strdup(str);
    
    for (char* p = result; *p; p++) {
        *p = toupper((unsigned char)*p);
    }
    
    return STRING_VAL(result);
}

TaggedValue string_toLowerCase_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* str = AS_STRING(args[0]);
    char* result = strdup(str);
    
    for (char* p = result; *p; p++) {
        *p = tolower((unsigned char)*p);
    }
    
    return STRING_VAL(result);
}

TaggedValue string_split_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* str = AS_STRING(args[0]);
    const char* delimiter = "";
    
    if (arg_count >= 2 && IS_STRING(args[1])) {
        delimiter = AS_STRING(args[1]);
    }
    
    Object* result = array_create();
    
    // Empty delimiter = split each character
    if (strlen(delimiter) == 0) {
        for (const char* p = str; *p; p++) {
            char char_str[2];
            char_str[0] = *p;
            char_str[1] = '\0';
            array_push(result, STRING_VAL(char_str));
        }
        return OBJECT_VAL(result);
    }
    
    // Make a copy to tokenize
    char* str_copy = strdup(str);
    char* token = strtok(str_copy, delimiter);
    
    while (token != NULL) {
        array_push(result, STRING_VAL(strdup(token)));
        token = strtok(NULL, delimiter);
    }
    
    free(str_copy);
    return OBJECT_VAL(result);
}

TaggedValue string_trim_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* str = AS_STRING(args[0]);
    
    // Find first non-whitespace
    const char* start = str;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    
    // Find last non-whitespace
    const char* end = str + strlen(str) - 1;
    while (end > start && isspace((unsigned char)*end)) {
        end--;
    }
    
    // Create trimmed string
    size_t len = end - start + 1;
    char* result = malloc(len + 1);
    memcpy(result, start, len);
    result[len] = '\0';
    
    return STRING_VAL(result);
}

TaggedValue array_count_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_OBJECT(args[0])) return NIL_VAL;
    
    Object* array = AS_OBJECT(args[0]);
    if (!array->is_array) return NIL_VAL;
    
    return NUMBER_VAL((double)array_length(array));
}

TaggedValue array_isEmpty_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_OBJECT(args[0])) return NIL_VAL;
    
    Object* array = AS_OBJECT(args[0]);
    if (!array->is_array) return NIL_VAL;
    
    return BOOL_VAL(array_length(array) == 0);
}