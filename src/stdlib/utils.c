#include "stdlib/utils.h"
#include "vm/object.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

// System functions

TaggedValue sys_exit(int arg_count, TaggedValue* args) {
    int code = 0;
    if (arg_count > 0 && IS_NUMBER(args[0])) {
        code = (int)AS_NUMBER(args[0]);
    }
    exit(code);
    return NIL_VAL; // Never reached
}

TaggedValue sys_getenv(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    const char* name = AS_STRING(args[0]);
    char* value = getenv(name);
    
    if (value) {
        return STRING_VAL(strdup(value));
    }
    return NIL_VAL;
}

TaggedValue sys_setenv(int arg_count, TaggedValue* args) {
    if (arg_count < 2 || !IS_STRING(args[0]) || !IS_STRING(args[1])) {
        return BOOL_VAL(false);
    }
    
    const char* name = AS_STRING(args[0]);
    const char* value = AS_STRING(args[1]);
    
    return BOOL_VAL(setenv(name, value, 1) == 0);
}

TaggedValue sys_time(int arg_count, TaggedValue* args) {
    (void)arg_count;
    (void)args;
    
    return NUMBER_VAL((double)time(NULL));
}

TaggedValue sys_sleep(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_NUMBER(args[0])) {
        return NIL_VAL;
    }
    
    double seconds = AS_NUMBER(args[0]);
    if (seconds < 0) seconds = 0;
    
    usleep((useconds_t)(seconds * 1000000));
    return NIL_VAL;
}

// Type checking functions

TaggedValue type_of(int arg_count, TaggedValue* args) {
    if (arg_count < 1) {
        return STRING_VAL(strdup("nil"));
    }
    
    TaggedValue value = args[0];
    
    switch (value.type) {
        case VAL_NIL: return STRING_VAL(strdup("nil"));
        case VAL_BOOL: return STRING_VAL(strdup("bool"));
        case VAL_NUMBER: return STRING_VAL(strdup("number"));
        case VAL_STRING: return STRING_VAL(strdup("string"));
        case VAL_OBJECT: {
            Object* obj = AS_OBJECT(value);
            if (obj->is_array) {
                return STRING_VAL(strdup("array"));
            }
            return STRING_VAL(strdup("object"));
        }
        case VAL_FUNCTION: return STRING_VAL(strdup("function"));
        case VAL_CLOSURE: return STRING_VAL(strdup("function"));
        case VAL_NATIVE: return STRING_VAL(strdup("native"));
        default: return STRING_VAL(strdup("unknown"));
    }
}

TaggedValue is_string(int arg_count, TaggedValue* args) {
    return BOOL_VAL(arg_count > 0 && IS_STRING(args[0]));
}

TaggedValue is_number(int arg_count, TaggedValue* args) {
    return BOOL_VAL(arg_count > 0 && IS_NUMBER(args[0]));
}

TaggedValue is_bool(int arg_count, TaggedValue* args) {
    return BOOL_VAL(arg_count > 0 && IS_BOOL(args[0]));
}

TaggedValue is_nil(int arg_count, TaggedValue* args) {
    return BOOL_VAL(arg_count > 0 && IS_NIL(args[0]));
}

TaggedValue is_array(int arg_count, TaggedValue* args) {
    return BOOL_VAL(arg_count > 0 && IS_OBJECT(args[0]) && AS_OBJECT(args[0])->is_array);
}

TaggedValue is_object(int arg_count, TaggedValue* args) {
    return BOOL_VAL(arg_count > 0 && IS_OBJECT(args[0]) && !AS_OBJECT(args[0])->is_array);
}

TaggedValue is_function(int arg_count, TaggedValue* args) {
    return BOOL_VAL(arg_count > 0 && 
                   (IS_FUNCTION(args[0]) || IS_CLOSURE(args[0]) || IS_NATIVE(args[0])));
}

// Conversion functions

TaggedValue to_string(int arg_count, TaggedValue* args) {
    if (arg_count < 1) {
        return STRING_VAL(strdup(""));
    }
    
    TaggedValue value = args[0];
    
    if (IS_STRING(value)) {
        return value;
    } else if (IS_NUMBER(value)) {
        char buf[64];
        double num = AS_NUMBER(value);
        if (num == (int)num) {
            sprintf(buf, "%d", (int)num);
        } else {
            sprintf(buf, "%g", num);
        }
        return STRING_VAL(strdup(buf));
    } else if (IS_BOOL(value)) {
        return STRING_VAL(strdup(AS_BOOL(value) ? "true" : "false"));
    } else if (IS_NIL(value)) {
        return STRING_VAL(strdup("nil"));
    } else if (IS_OBJECT(value)) {
        Object* obj = AS_OBJECT(value);
        if (obj->is_array) {
            return STRING_VAL(strdup("[Array]"));
        }
        return STRING_VAL(strdup("[Object]"));
    } else if (IS_FUNCTION(value) || IS_CLOSURE(value)) {
        return STRING_VAL(strdup("[Function]"));
    }
    
    return STRING_VAL(strdup("[Unknown]"));
}

TaggedValue to_number(int arg_count, TaggedValue* args) {
    if (arg_count < 1) {
        return NUMBER_VAL(0);
    }
    
    TaggedValue value = args[0];
    
    if (IS_NUMBER(value)) {
        return value;
    } else if (IS_STRING(value)) {
        char* end;
        double num = strtod(AS_STRING(value), &end);
        if (*end == '\0') {
            return NUMBER_VAL(num);
        }
        return NUMBER_VAL(0); // NaN
    } else if (IS_BOOL(value)) {
        return NUMBER_VAL(AS_BOOL(value) ? 1.0 : 0.0);
    }
    
    return NUMBER_VAL(0);
}

TaggedValue to_int(int arg_count, TaggedValue* args) {
    if (arg_count < 1) {
        return NUMBER_VAL(0);
    }
    
    TaggedValue num_val = to_number(arg_count, args);
    if (IS_NUMBER(num_val)) {
        return NUMBER_VAL((double)(int)AS_NUMBER(num_val));
    }
    
    return NUMBER_VAL(0);
}

// Math extensions

static bool random_initialized = false;

TaggedValue math_random(int arg_count, TaggedValue* args) {
    (void)arg_count;
    (void)args;
    
    if (!random_initialized) {
        srand(time(NULL));
        random_initialized = true;
    }
    
    return NUMBER_VAL((double)rand() / RAND_MAX);
}

TaggedValue math_randomInt(int arg_count, TaggedValue* args) {
    if (!random_initialized) {
        srand(time(NULL));
        random_initialized = true;
    }
    
    int min = 0;
    int max = RAND_MAX;
    
    if (arg_count >= 1 && IS_NUMBER(args[0])) {
        max = (int)AS_NUMBER(args[0]);
    }
    
    if (arg_count >= 2 && IS_NUMBER(args[1])) {
        min = (int)AS_NUMBER(args[0]);
        max = (int)AS_NUMBER(args[1]);
    }
    
    if (min > max) {
        int temp = min;
        min = max;
        max = temp;
    }
    
    return NUMBER_VAL((double)(min + rand() % (max - min + 1)));
}