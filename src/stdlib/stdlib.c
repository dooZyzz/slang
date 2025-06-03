#include "stdlib/stdlib.h"
#include "runtime/modules/extensions/module_inspect.h"
#include "utils/allocators.h"
#include <string.h>
#include <ctype.h>
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
    fprintf(stderr, "[DEBUG] stdlib_init called with vm=%p\n", (void*)vm);
    
    // Store VM reference for later use
    stdlib_set_vm(vm);
    
    // Don't call init_builtin_prototypes here - it's already called in vm_init
    // init_builtin_prototypes();
    
    // Then add stdlib methods to each prototype
    Object* obj_proto = get_object_prototype();
    Object* arr_proto = get_array_prototype();
    Object* str_proto = get_string_prototype();
    Object* func_proto = get_function_prototype();
    
    if (!obj_proto || !arr_proto || !str_proto || !func_proto) {
        fprintf(stderr, "[ERROR] Failed to get prototypes: obj=%p, arr=%p, str=%p, func=%p\n",
                (void*)obj_proto, (void*)arr_proto, (void*)str_proto, (void*)func_proto);
        return;
    }
    
    fprintf(stderr, "[DEBUG] Initializing object prototype\n");
    stdlib_init_object_prototype(obj_proto);
    fprintf(stderr, "[DEBUG] Initializing array prototype\n");
    stdlib_init_array_prototype(arr_proto);
    fprintf(stderr, "[DEBUG] Initializing string prototype\n");
    stdlib_init_string_prototype(str_proto);
    fprintf(stderr, "[DEBUG] Initializing function prototype\n");
    stdlib_init_function_prototype(func_proto);
    fprintf(stderr, "[DEBUG] All prototypes initialized\n");
    
    // Register global functions
    
    // Register module introspection natives
    // Temporarily disabled to debug crash
    // register_module_natives(vm);
    
    // Note: Global function registration would need to be done in vm_init
    // For now, these functions can be accessed through module imports
    // or added to a global object
}

// Object prototype method implementations
static TaggedValue object_toString_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1) return NIL_VAL;
    
    TaggedValue self = args[0];
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    if (IS_NIL(self)) {
        return STRING_VAL(MEM_STRDUP(str_alloc, "nil"));
    } else if (IS_BOOL(self)) {
        return STRING_VAL(MEM_STRDUP(str_alloc, AS_BOOL(self) ? "true" : "false"));
    } else if (IS_NUMBER(self)) {
        char buffer[32];
        double num = AS_NUMBER(self);
        if (num == (int)num) {
            snprintf(buffer, sizeof(buffer), "%d", (int)num);
        } else {
            snprintf(buffer, sizeof(buffer), "%.14g", num);
        }
        return STRING_VAL(MEM_STRDUP(str_alloc, buffer));
    } else if (IS_STRING(self)) {
        return self; // Strings return themselves
    } else if (IS_OBJECT(self)) {
        Object* obj = AS_OBJECT(self);
        if (obj->is_array) {
            // For arrays, create a string representation
            return STRING_VAL(MEM_STRDUP(str_alloc, "[Array]"));
        } else {
            return STRING_VAL(MEM_STRDUP(str_alloc, "[Object]"));
        }
    } else if (IS_FUNCTION(self) || IS_CLOSURE(self) || IS_NATIVE(self)) {
        return STRING_VAL(MEM_STRDUP(str_alloc, "[Function]"));
    }
    
    return STRING_VAL(MEM_STRDUP(str_alloc, "[Unknown]"));
}

static TaggedValue object_valueOf_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1) return NIL_VAL;
    
    // valueOf returns the primitive value of the object
    // For most objects, this is just the object itself
    return args[0];
}

static TaggedValue object_hasOwnProperty_method(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_OBJECT(args[0]) || !IS_STRING(args[1])) {
        return BOOL_VAL(false);
    }
    
    Object* obj = AS_OBJECT(args[0]);
    const char* prop_name = AS_STRING(args[1]);
    
    // Check if the property exists directly on this object (not prototype)
    ObjectProperty* prop = obj->properties;
    while (prop != NULL) {
        if (strcmp(prop->key, prop_name) == 0) {
            return BOOL_VAL(true);
        }
        prop = prop->next;
    }
    
    return BOOL_VAL(false);
}

// Object prototype methods
void stdlib_init_object_prototype(Object* proto) {
    object_set_property(proto, "toString", NATIVE_VAL(object_toString_method));
    object_set_property(proto, "valueOf", NATIVE_VAL(object_valueOf_method));
    object_set_property(proto, "hasOwnProperty", NATIVE_VAL(object_hasOwnProperty_method));
}

// Array prototype methods
void stdlib_init_array_prototype(Object* proto) {
    fprintf(stderr, "[DEBUG] stdlib_init_array_prototype: proto=%p\n", (void*)proto);
    if (!proto) {
        fprintf(stderr, "[ERROR] Array prototype is NULL!\n");
        return;
    }
    
    // Basic array methods
    fprintf(stderr, "[DEBUG] Adding push method...\n");
    object_set_property(proto, "push", NATIVE_VAL(array_push_method));
    fprintf(stderr, "[DEBUG] Adding pop method...\n");
    object_set_property(proto, "pop", NATIVE_VAL(array_pop_method));
    fprintf(stderr, "[DEBUG] Adding length method...\n");
    object_set_property(proto, "length", NATIVE_VAL(array_length_method));
    
    // Higher-order methods
    fprintf(stderr, "[DEBUG] Adding map method...\n");
    object_set_property(proto, "map", NATIVE_VAL(array_map_method));
    fprintf(stderr, "[DEBUG] Adding filter method...\n");
    object_set_property(proto, "filter", NATIVE_VAL(array_filter_method));
    fprintf(stderr, "[DEBUG] Adding reduce method...\n");
    object_set_property(proto, "reduce", NATIVE_VAL(array_reduce_method));
    
    // Additional array methods
    fprintf(stderr, "[DEBUG] Adding count method...\n");
    object_set_property(proto, "count", NATIVE_VAL(array_length_method));  // count is an alias for length
    fprintf(stderr, "[DEBUG] Adding isEmpty method...\n");
    object_set_property(proto, "isEmpty", NATIVE_VAL(array_isEmpty_method));
    fprintf(stderr, "[DEBUG] stdlib_init_array_prototype completed\n");
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

// Function prototype method implementations
static TaggedValue function_call_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_FUNCTION(args[0])) {
        return NIL_VAL;
    }
    
    // First argument is the function itself
    // Second argument is 'this' context
    // Rest are arguments to pass to the function
    TaggedValue func = args[0];
    TaggedValue this_arg = (arg_count > 1) ? args[1] : NIL_VAL;
    
    // Prepare arguments for the function call
    int func_arg_count = arg_count - 2;  // Exclude function and 'this'
    TaggedValue* func_args = NULL;
    
    // Use VM allocator for temporary function arguments
    Allocator* vm_alloc = allocators_get(ALLOC_SYSTEM_VM);
    
    if (func_arg_count > 0) {
        func_args = MEM_ALLOC(vm_alloc, sizeof(TaggedValue) * (func_arg_count + 1));
        if (!func_args) return NIL_VAL;
        
        func_args[0] = this_arg;  // 'this' is always the first argument
        for (int i = 0; i < func_arg_count; i++) {
            func_args[i + 1] = args[i + 2];
        }
    } else {
        func_args = MEM_ALLOC(vm_alloc, sizeof(TaggedValue));
        if (!func_args) return NIL_VAL;
        
        func_args[0] = this_arg;
        func_arg_count = 0;
    }
    
    // Call the function
    TaggedValue result;
    if (IS_NATIVE(func)) {
        NativeFn native = AS_NATIVE(func);
        result = native(func_arg_count + 1, func_args);
    } else {
        // For user-defined functions, we'd need VM integration
        // For now, return nil
        result = NIL_VAL;
    }
    
    SLANG_MEM_FREE(vm_alloc, func_args, sizeof(TaggedValue) * (func_arg_count > 0 ? func_arg_count + 1 : 1));
    return result;
}

static TaggedValue function_apply_method(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_FUNCTION(args[0])) {
        return NIL_VAL;
    }
    
    // First argument is the function itself
    // Second argument is 'this' context
    // Third argument is an array of arguments
    TaggedValue func = args[0];
    TaggedValue this_arg = args[1];
    
    // Get arguments from array
    int func_arg_count = 0;
    TaggedValue* func_args = NULL;
    Allocator* vm_alloc = allocators_get(ALLOC_SYSTEM_VM);
    
    if (arg_count >= 3 && IS_OBJECT(args[2])) {
        Object* arg_array = AS_OBJECT(args[2]);
        if (arg_array->is_array) {
            func_arg_count = (int)array_length(arg_array);
            func_args = MEM_ALLOC(vm_alloc, sizeof(TaggedValue) * (func_arg_count + 1));
            if (!func_args) return NIL_VAL;
            
            func_args[0] = this_arg;  // 'this' is always the first argument
            
            for (int i = 0; i < func_arg_count; i++) {
                func_args[i + 1] = array_get(arg_array, i);
            }
        }
    }
    
    if (func_args == NULL) {
        func_args = MEM_ALLOC(vm_alloc, sizeof(TaggedValue));
        if (!func_args) return NIL_VAL;
        
        func_args[0] = this_arg;
        func_arg_count = 0;
    }
    
    // Call the function
    TaggedValue result;
    if (IS_NATIVE(func)) {
        NativeFn native = AS_NATIVE(func);
        result = native(func_arg_count + 1, func_args);
    } else {
        // For user-defined functions, we'd need VM integration
        result = NIL_VAL;
    }
    
    SLANG_MEM_FREE(vm_alloc, func_args, sizeof(TaggedValue) * (func_arg_count > 0 ? func_arg_count + 1 : 1));
    return result;
}

static TaggedValue function_bind_method(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_FUNCTION(args[0])) {
        return NIL_VAL;
    }
    
    // For now, we can't create true bound functions without VM support
    // This would require creating a new function object that captures
    // the 'this' context and any pre-bound arguments
    
    // TODO: Implement proper function binding when VM supports closures
    // For now, just return the original function
    return args[0];
}

// Function prototype methods
void stdlib_init_function_prototype(Object* proto) {
    // Add call method
    object_set_property(proto, "call", NATIVE_VAL(function_call_method));
    
    // Add apply method
    object_set_property(proto, "apply", NATIVE_VAL(function_apply_method));
    
    // Add bind method
    object_set_property(proto, "bind", NATIVE_VAL(function_bind_method));
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
        Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
        return STRING_VAL(MEM_STRDUP(str_alloc, ""));
    }
    
    char result[2] = { str[index], '\0' };
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    return STRING_VAL(MEM_STRDUP(str_alloc, result));
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
    if (start >= len) {
        Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
        return STRING_VAL(MEM_STRDUP(str_alloc, ""));
    }
    
    int end = len;
    if (arg_count >= 3 && IS_NUMBER(args[2])) {
        end = (int)AS_NUMBER(args[2]);
        if (end < start) end = start;
        if (end > len) end = len;
    }
    
    int result_len = end - start;
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    char* result = MEM_ALLOC(str_alloc, result_len + 1);
    if (result) {
        memcpy(result, str + start, result_len);
        result[result_len] = '\0';
    }
    
    return STRING_VAL(result);
}

TaggedValue string_toUpperCase_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* str = AS_STRING(args[0]);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    char* result = MEM_STRDUP(str_alloc, str);
    
    if (result) {
        for (char* p = result; *p; p++) {
            *p = toupper((unsigned char)*p);
        }
    }
    
    return STRING_VAL(result);
}

TaggedValue string_toLowerCase_method(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) return NIL_VAL;
    
    const char* str = AS_STRING(args[0]);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    char* result = MEM_STRDUP(str_alloc, str);
    
    if (result) {
        for (char* p = result; *p; p++) {
            *p = tolower((unsigned char)*p);
        }
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
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    // Empty delimiter = split each character
    if (strlen(delimiter) == 0) {
        for (const char* p = str; *p; p++) {
            char char_str[2];
            char_str[0] = *p;
            char_str[1] = '\0';
            array_push(result, STRING_VAL(MEM_STRDUP(str_alloc, char_str)));
        }
        return OBJECT_VAL(result);
    }
    
    // Make a copy to tokenize
    char* str_copy = MEM_STRDUP(str_alloc, str);
    if (!str_copy) return OBJECT_VAL(result);
    
    char* token = strtok(str_copy, delimiter);
    
    while (token != NULL) {
        array_push(result, STRING_VAL(MEM_STRDUP(str_alloc, token)));
        token = strtok(NULL, delimiter);
    }
    
    SLANG_MEM_FREE(str_alloc, str_copy, strlen(str) + 1);
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
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    char* result = MEM_ALLOC(str_alloc, len + 1);
    if (result) {
        memcpy(result, start, len);
        result[len] = '\0';
    }
    
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