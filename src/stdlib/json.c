#include "stdlib/json.h"
#include "vm/object.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Forward declarations
static TaggedValue parse_value(const char** json);
static char* stringify_value(TaggedValue value);

// Helper to skip whitespace
static void skip_whitespace(const char** json) {
    while (isspace(**json)) {
        (*json)++;
    }
}

// Parse a JSON string
static char* parse_string(const char** json) {
    if (**json != '"') return NULL;
    (*json)++; // Skip opening quote
    
    size_t capacity = 64;
    size_t length = 0;
    char* result = malloc(capacity);
    
    while (**json && **json != '"') {
        if (length + 2 >= capacity) {
            capacity *= 2;
            result = realloc(result, capacity);
        }
        
        if (**json == '\\') {
            (*json)++;
            switch (**json) {
                case '"': result[length++] = '"'; break;
                case '\\': result[length++] = '\\'; break;
                case '/': result[length++] = '/'; break;
                case 'b': result[length++] = '\b'; break;
                case 'f': result[length++] = '\f'; break;
                case 'n': result[length++] = '\n'; break;
                case 'r': result[length++] = '\r'; break;
                case 't': result[length++] = '\t'; break;
                default: result[length++] = **json; break;
            }
            (*json)++;
        } else {
            result[length++] = **json;
            (*json)++;
        }
    }
    
    if (**json == '"') {
        (*json)++; // Skip closing quote
        result[length] = '\0';
        return result;
    }
    
    free(result);
    return NULL;
}

// Parse a JSON number
static TaggedValue parse_number(const char** json) {
    char* end;
    double value = strtod(*json, &end);
    
    if (end > *json) {
        *json = end;
        return NUMBER_VAL(value);
    }
    
    return NIL_VAL;
}

// Parse a JSON array
static TaggedValue parse_array(const char** json) {
    if (**json != '[') return NIL_VAL;
    (*json)++; // Skip '['
    
    Object* array = array_create();
    skip_whitespace(json);
    
    if (**json == ']') {
        (*json)++;
        return OBJECT_VAL(array);
    }
    
    while (1) {
        TaggedValue value = parse_value(json);
        array_push(array, value);
        
        skip_whitespace(json);
        
        if (**json == ']') {
            (*json)++;
            break;
        }
        
        if (**json == ',') {
            (*json)++;
            skip_whitespace(json);
        } else {
            // Error - expected ',' or ']'
            return NIL_VAL;
        }
    }
    
    return OBJECT_VAL(array);
}

// Parse a JSON object
static TaggedValue parse_object(const char** json) {
    if (**json != '{') return NIL_VAL;
    (*json)++; // Skip '{'
    
    Object* obj = object_create();
    skip_whitespace(json);
    
    if (**json == '}') {
        (*json)++;
        return OBJECT_VAL(obj);
    }
    
    while (1) {
        skip_whitespace(json);
        
        // Parse key
        char* key = parse_string(json);
        if (!key) return NIL_VAL;
        
        skip_whitespace(json);
        
        if (**json != ':') {
            free(key);
            return NIL_VAL;
        }
        (*json)++; // Skip ':'
        
        skip_whitespace(json);
        
        // Parse value
        TaggedValue value = parse_value(json);
        object_set_property(obj, key, value);
        free(key);
        
        skip_whitespace(json);
        
        if (**json == '}') {
            (*json)++;
            break;
        }
        
        if (**json == ',') {
            (*json)++;
            skip_whitespace(json);
        } else {
            // Error - expected ',' or '}'
            return NIL_VAL;
        }
    }
    
    return OBJECT_VAL(obj);
}

// Parse any JSON value
static TaggedValue parse_value(const char** json) {
    skip_whitespace(json);
    
    if (**json == '"') {
        char* str = parse_string(json);
        if (str) {
            return STRING_VAL(str);
        }
    } else if (**json == '{') {
        return parse_object(json);
    } else if (**json == '[') {
        return parse_array(json);
    } else if (strncmp(*json, "true", 4) == 0) {
        *json += 4;
        return BOOL_VAL(true);
    } else if (strncmp(*json, "false", 5) == 0) {
        *json += 5;
        return BOOL_VAL(false);
    } else if (strncmp(*json, "null", 4) == 0) {
        *json += 4;
        return NIL_VAL;
    } else if (**json == '-' || isdigit(**json)) {
        return parse_number(json);
    }
    
    return NIL_VAL;
}

// JSON.parse(string) - parses JSON string into values
TaggedValue json_parse(int arg_count, TaggedValue* args) {
    if (arg_count < 1 || !IS_STRING(args[0])) {
        return NIL_VAL;
    }
    
    const char* json = AS_STRING(args[0]);
    return parse_value(&json);
}

// Helper to append to a growing string
typedef struct {
    char* data;
    size_t length;
    size_t capacity;
} StringBuilder;

static void sb_init(StringBuilder* sb) {
    sb->capacity = 256;
    sb->length = 0;
    sb->data = malloc(sb->capacity);
    sb->data[0] = '\0';
}

static void sb_append(StringBuilder* sb, const char* str) {
    size_t len = strlen(str);
    while (sb->length + len + 1 > sb->capacity) {
        sb->capacity *= 2;
        sb->data = realloc(sb->data, sb->capacity);
    }
    strcpy(sb->data + sb->length, str);
    sb->length += len;
}

static void sb_append_char(StringBuilder* sb, char c) {
    if (sb->length + 2 > sb->capacity) {
        sb->capacity *= 2;
        sb->data = realloc(sb->data, sb->capacity);
    }
    sb->data[sb->length++] = c;
    sb->data[sb->length] = '\0';
}

// Stringify a string value
static void stringify_string(StringBuilder* sb, const char* str) {
    sb_append_char(sb, '"');
    while (*str) {
        switch (*str) {
            case '"': sb_append(sb, "\\\""); break;
            case '\\': sb_append(sb, "\\\\"); break;
            case '\b': sb_append(sb, "\\b"); break;
            case '\f': sb_append(sb, "\\f"); break;
            case '\n': sb_append(sb, "\\n"); break;
            case '\r': sb_append(sb, "\\r"); break;
            case '\t': sb_append(sb, "\\t"); break;
            default:
                if (*str >= 32 && *str < 127) {
                    sb_append_char(sb, *str);
                } else {
                    char buf[8];
                    sprintf(buf, "\\u%04x", (unsigned char)*str);
                    sb_append(sb, buf);
                }
                break;
        }
        str++;
    }
    sb_append_char(sb, '"');
}

// Stringify any value
static void stringify_value_to_sb(StringBuilder* sb, TaggedValue value) {
    if (IS_NIL(value)) {
        sb_append(sb, "null");
    } else if (IS_BOOL(value)) {
        sb_append(sb, AS_BOOL(value) ? "true" : "false");
    } else if (IS_NUMBER(value)) {
        char buf[64];
        double num = AS_NUMBER(value);
        if (num == (int)num) {
            sprintf(buf, "%d", (int)num);
        } else {
            sprintf(buf, "%.17g", num);
        }
        sb_append(sb, buf);
    } else if (IS_STRING(value)) {
        stringify_string(sb, AS_STRING(value));
    } else if (IS_OBJECT(value)) {
        Object* obj = AS_OBJECT(value);
        
        if (obj->is_array) {
            // Array
            sb_append_char(sb, '[');
            size_t length = array_length(obj);
            for (size_t i = 0; i < length; i++) {
                if (i > 0) sb_append(sb, ",");
                stringify_value_to_sb(sb, array_get(obj, i));
            }
            sb_append_char(sb, ']');
        } else {
            // Object
            sb_append_char(sb, '{');
            bool first = true;
            
            // Simple iteration - would need proper object property iteration
            // For now, just return empty object
            sb_append_char(sb, '}');
        }
    } else {
        // Other types become null
        sb_append(sb, "null");
    }
}

// JSON.stringify(value) - converts value to JSON string
TaggedValue json_stringify(int arg_count, TaggedValue* args) {
    if (arg_count < 1) {
        return NIL_VAL;
    }
    
    StringBuilder sb;
    sb_init(&sb);
    stringify_value_to_sb(&sb, args[0]);
    
    return STRING_VAL(sb.data);
}