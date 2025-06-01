#include "runtime/modules/lifecycle/builtin_modules.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <ctype.h>
#include "runtime/core/object.h"

// Global registry of builtin modules
static struct {
    BuiltinModule* modules;
    size_t count;
    size_t capacity;
} module_registry = {0};

// Current module being registered
static BuiltinModule* current_module = NULL;

void builtin_module_register(const char* name, void (*init_func)(void)) {
    if (module_registry.count >= module_registry.capacity) {
        size_t new_capacity = module_registry.capacity == 0 ? 8 : module_registry.capacity * 2;
        module_registry.modules = realloc(module_registry.modules, 
                                        new_capacity * sizeof(BuiltinModule));
        module_registry.capacity = new_capacity;
    }
    
    BuiltinModule* module = &module_registry.modules[module_registry.count++];
    module->name = strdup(name);
    module->init_func = init_func;
    module->exports.names = NULL;
    module->exports.functions = NULL;
    module->exports.count = 0;
    
    // Set as current module and initialize
    current_module = module;
    if (init_func) {
        init_func();
    }
    current_module = NULL;
}

static void register_export(const char* name, NativeFn func) {
    if (!current_module) return;
    
    size_t idx = current_module->exports.count++;
    current_module->exports.names = realloc(current_module->exports.names,
                                           current_module->exports.count * sizeof(char*));
    current_module->exports.functions = realloc(current_module->exports.functions,
                                              current_module->exports.count * sizeof(NativeFn));
    
    current_module->exports.names[idx] = strdup(name);
    current_module->exports.functions[idx] = func;
}

bool builtin_module_get_export(const char* module_name, const char* export_name, 
                              TaggedValue* out_value) {
    for (size_t i = 0; i < module_registry.count; i++) {
        if (strcmp(module_registry.modules[i].name, module_name) == 0) {
            BuiltinModule* module = &module_registry.modules[i];
            for (size_t j = 0; j < module->exports.count; j++) {
                if (strcmp(module->exports.names[j], export_name) == 0) {
                    *out_value = NATIVE_VAL(module->exports.functions[j]);
                    return true;
                }
            }
            break;
        }
    }
    return false;
}

bool builtin_module_get_all_exports(const char* module_name, 
                                   const char*** names, 
                                   TaggedValue** values, 
                                   size_t* count) {
    for (size_t i = 0; i < module_registry.count; i++) {
        if (strcmp(module_registry.modules[i].name, module_name) == 0) {
            BuiltinModule* module = &module_registry.modules[i];
            *names = module->exports.names;
            *count = module->exports.count;
            
            // Create values array
            *values = malloc(module->exports.count * sizeof(TaggedValue));
            for (size_t j = 0; j < module->exports.count; j++) {
                (*values)[j] = NATIVE_VAL(module->exports.functions[j]);
            }
            return true;
        }
    }
    return false;
}

// Math module implementation
static TaggedValue math_sin(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_NUMBER(args[0])) {
        return NIL_VAL;
    }
    return NUMBER_VAL(sin(AS_NUMBER(args[0])));
}

static TaggedValue math_cos(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_NUMBER(args[0])) {
        return NIL_VAL;
    }
    return NUMBER_VAL(cos(AS_NUMBER(args[0])));
}

static TaggedValue math_tan(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_NUMBER(args[0])) {
        return NIL_VAL;
    }
    return NUMBER_VAL(tan(AS_NUMBER(args[0])));
}

static TaggedValue math_sqrt(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_NUMBER(args[0])) {
        return NIL_VAL;
    }
    double n = AS_NUMBER(args[0]);
    if (n < 0) {
        return NIL_VAL;
    }
    return NUMBER_VAL(sqrt(n));
}

static TaggedValue math_pow(int arg_count, TaggedValue* args) {
    if (arg_count != 2 || !IS_NUMBER(args[0]) || !IS_NUMBER(args[1])) {
        return NIL_VAL;
    }
    return NUMBER_VAL(pow(AS_NUMBER(args[0]), AS_NUMBER(args[1])));
}

static TaggedValue math_abs(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_NUMBER(args[0])) {
        return NIL_VAL;
    }
    return NUMBER_VAL(fabs(AS_NUMBER(args[0])));
}

static TaggedValue math_floor(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_NUMBER(args[0])) {
        return NIL_VAL;
    }
    return NUMBER_VAL(floor(AS_NUMBER(args[0])));
}

static TaggedValue math_ceil(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_NUMBER(args[0])) {
        return NIL_VAL;
    }
    return NUMBER_VAL(ceil(AS_NUMBER(args[0])));
}

static TaggedValue math_round(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_NUMBER(args[0])) {
        return NIL_VAL;
    }
    return NUMBER_VAL(round(AS_NUMBER(args[0])));
}

static TaggedValue math_min(int arg_count, TaggedValue* args) {
    if (arg_count == 0) {
        return NIL_VAL;
    }
    double min = AS_NUMBER(args[0]);
    for (int i = 1; i < arg_count; i++) {
        if (!IS_NUMBER(args[i])) {
            return NIL_VAL;
        }
        double val = AS_NUMBER(args[i]);
        if (val < min) {
            min = val;
        }
    }
    return NUMBER_VAL(min);
}

static TaggedValue math_max(int arg_count, TaggedValue* args) {
    if (arg_count == 0) {
        return NIL_VAL;
    }
    double max = AS_NUMBER(args[0]);
    for (int i = 1; i < arg_count; i++) {
        if (!IS_NUMBER(args[i])) {
            return NIL_VAL;
        }
        double val = AS_NUMBER(args[i]);
        if (val > max) {
            max = val;
        }
    }
    return NUMBER_VAL(max);
}

void builtin_math_init(void) {
    register_export("sin", math_sin);
    register_export("cos", math_cos);
    register_export("tan", math_tan);
    register_export("sqrt", math_sqrt);
    register_export("pow", math_pow);
    register_export("abs", math_abs);
    register_export("floor", math_floor);
    register_export("ceil", math_ceil);
    register_export("round", math_round);
    register_export("min", math_min);
    register_export("max", math_max);
    
    // Constants would need special handling since they're not functions
    // For now, we'll skip them
}

// String module implementation
static TaggedValue string_length(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    return NUMBER_VAL((double)strlen(AS_STRING(args[0])));
}

static TaggedValue string_charAt(int arg_count, TaggedValue* args) {
    if (arg_count != 2 || !IS_STRING(args[0]) || !IS_NUMBER(args[1])) {
        return NIL_VAL;
    }
    
    const char* str = AS_STRING(args[0]);
    int index = (int)AS_NUMBER(args[1]);
    size_t len = strlen(str);
    
    if (index < 0 || index >= (int)len) {
        return NIL_VAL;
    }
    
    char result[2] = {str[index], '\0'};
    return STRING_VAL(strdup(result));
}

static TaggedValue string_substring(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_STRING(args[0]) || !IS_NUMBER(args[1])) {
        return NIL_VAL;
    }
    
    const char* str = AS_STRING(args[0]);
    int start = (int)AS_NUMBER(args[1]);
    size_t len = strlen(str);
    
    if (start < 0) start = 0;
    if (start >= (int)len) return STRING_VAL(strdup(""));
    
    int end = len;
    if (arg_count >= 3 && IS_NUMBER(args[2])) {
        end = (int)AS_NUMBER(args[2]);
        if (end > (int)len) end = (int)len;
        if (end < start) end = start;
    }
    
    int sub_len = end - start;
    char* result = malloc(sub_len + 1);
    memcpy(result, str + start, sub_len);
    result[sub_len] = '\0';
    
    return STRING_VAL(result);
}

static TaggedValue string_toUpperCase(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    const char* str = AS_STRING(args[0]);
    size_t len = strlen(str);
    char* result = malloc(len + 1);
    
    for (size_t i = 0; i < len; i++) {
        result[i] = (str[i] >= 'a' && str[i] <= 'z') ? str[i] - 32 : str[i];
    }
    result[len] = '\0';
    
    return STRING_VAL(result);
}

static TaggedValue string_toLowerCase(int arg_count, TaggedValue* args) {
    if (arg_count != 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    const char* str = AS_STRING(args[0]);
    size_t len = strlen(str);
    char* result = malloc(len + 1);
    
    for (size_t i = 0; i < len; i++) {
        result[i] = (str[i] >= 'A' && str[i] <= 'Z') ? str[i] + 32 : str[i];
    }
    result[len] = '\0';
    
    return STRING_VAL(result);
}

void builtin_string_init(void) {
    register_export("length", string_length);
    register_export("charAt", string_charAt);
    register_export("substring", string_substring);
    register_export("toUpperCase", string_toUpperCase);
    register_export("toLowerCase", string_toLowerCase);
}

// Array module - additional array utilities
static TaggedValue array_range(int arg_count, TaggedValue* args) {
    if (arg_count == 0 || !IS_NUMBER(args[0])) {
        return NIL_VAL;
    }
    
    int start = 0;
    int end = (int)AS_NUMBER(args[0]);
    
    if (arg_count >= 2 && IS_NUMBER(args[1])) {
        start = (int)AS_NUMBER(args[0]);
        end = (int)AS_NUMBER(args[1]);
    }
    
    // Create array with range values
    Object* array = array_create();
    
    // Generate range values
    if (start <= end) {
        // Forward range
        for (int i = start; i < end; i++) {
            array_push(array, NUMBER_VAL(i));
        }
    } else {
        // Backward range
        for (int i = start; i > end; i--) {
            array_push(array, NUMBER_VAL(i));
        }
    }
    
    return OBJECT_VAL(array);
}

void builtin_array_init(void) {
    register_export("range", array_range);
}

// IO module - basic I/O operations  
static TaggedValue io_readLine(int arg_count, TaggedValue* args) {
    if (arg_count > 1) {
        return NIL_VAL;
    }
    
    // Print prompt if provided
    if (arg_count == 1 && IS_STRING(args[0])) {
        printf("%s", AS_STRING(args[0]));
        fflush(stdout);
    }
    
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        // Remove newline
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        return STRING_VAL(strdup(buffer));
    }
    
    return NIL_VAL;
}

void builtin_io_init(void) {
    register_export("readLine", io_readLine);
}

// Initialize all builtin modules
void builtin_modules_init(void) {
    builtin_module_register("math", builtin_math_init);
    builtin_module_register("string", builtin_string_init);
    builtin_module_register("array", builtin_array_init);
    builtin_module_register("io", builtin_io_init);
}
