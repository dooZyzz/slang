#include "semantic/type.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct TypeEntry {
    char* name;
    Type* type;
    struct TypeEntry* next;
} TypeEntry;

struct TypeContext {
    TypeEntry** buckets;
    size_t bucket_count;
    size_t entry_count;
};

static Type* type_create(TypeKind kind) {
    Type* type = calloc(1, sizeof(Type));
    type->kind = kind;
    return type;
}

Type* type_void(void) {
    Type* type = type_create(TYPE_KIND_VOID);
    type->name = "Void";
    return type;
}

Type* type_bool(void) {
    Type* type = type_create(TYPE_KIND_BOOL);
    type->name = "Bool";
    return type;
}

Type* type_int(void) {
    Type* type = type_create(TYPE_KIND_INT);
    type->name = "Int";
    return type;
}

Type* type_float(void) {
    Type* type = type_create(TYPE_KIND_FLOAT);
    type->name = "Float";
    return type;
}

Type* type_double(void) {
    Type* type = type_create(TYPE_KIND_DOUBLE);
    type->name = "Double";
    return type;
}

Type* type_string(void) {
    Type* type = type_create(TYPE_KIND_STRING);
    type->name = "String";
    return type;
}

Type* type_nil(void) {
    Type* type = type_create(TYPE_KIND_NIL);
    type->name = "nil";
    return type;
}

Type* type_any(void) {
    Type* type = type_create(TYPE_KIND_ANY);
    type->name = "Any";
    return type;
}

Type* type_array(Type* element_type) {
    Type* type = type_create(TYPE_KIND_ARRAY);
    type->data.array = calloc(1, sizeof(ArrayType));
    type->data.array->element_type = element_type;
    return type;
}

Type* type_dictionary(Type* key_type, Type* value_type) {
    Type* type = type_create(TYPE_KIND_DICTIONARY);
    type->data.dictionary = calloc(1, sizeof(DictionaryType));
    type->data.dictionary->key_type = key_type;
    type->data.dictionary->value_type = value_type;
    return type;
}

Type* type_optional(Type* wrapped) {
    Type* type = type_create(TYPE_KIND_OPTIONAL);
    type->data.wrapped = wrapped;
    type->is_optional = true;
    return type;
}

Type* type_function(Type** params, size_t param_count, Type* return_type) {
    Type* type = type_create(TYPE_KIND_FUNCTION);
    type->data.function = calloc(1, sizeof(FunctionType));
    type->data.function->parameter_count = param_count;
    if (param_count > 0) {
        type->data.function->parameter_types = calloc(param_count, sizeof(Type*));
        memcpy(type->data.function->parameter_types, params, param_count * sizeof(Type*));
    }
    type->data.function->return_type = return_type;
    return type;
}

Type* type_tuple(Type** elements, size_t element_count) {
    Type* type = type_create(TYPE_KIND_TUPLE);
    type->data.tuple = calloc(1, sizeof(TupleType));
    type->data.tuple->element_count = element_count;
    if (element_count > 0) {
        type->data.tuple->element_types = calloc(element_count, sizeof(Type*));
        memcpy(type->data.tuple->element_types, elements, element_count * sizeof(Type*));
    }
    return type;
}

Type* type_struct(const char* name) {
    Type* type = type_create(TYPE_KIND_STRUCT);
    type->name = strdup(name);
    type->data.composite = calloc(1, sizeof(CompositeType));
    return type;
}

Type* type_class(const char* name) {
    Type* type = type_create(TYPE_KIND_CLASS);
    type->name = strdup(name);
    type->data.composite = calloc(1, sizeof(CompositeType));
    return type;
}

Type* type_enum(const char* name) {
    Type* type = type_create(TYPE_KIND_ENUM);
    type->name = strdup(name);
    type->data.composite = calloc(1, sizeof(CompositeType));
    return type;
}

Type* type_protocol(const char* name) {
    Type* type = type_create(TYPE_KIND_PROTOCOL);
    type->name = strdup(name);
    type->data.composite = calloc(1, sizeof(CompositeType));
    return type;
}

Type* type_generic(const char* name) {
    Type* type = type_create(TYPE_KIND_GENERIC);
    type->name = strdup(name);
    type->data.generic = calloc(1, sizeof(GenericType));
    type->data.generic->name = type->name;
    return type;
}

Type* type_alias(const char* name, Type* target) {
    Type* type = type_create(TYPE_KIND_ALIAS);
    type->name = strdup(name);
    type->data.alias_target = target;
    return type;
}

Type* type_unresolved(const char* name) {
    Type* type = type_create(TYPE_KIND_UNRESOLVED);
    type->name = strdup(name);
    return type;
}

bool type_equals(const Type* a, const Type* b) {
    if (!a || !b) return a == b;
    if (a->kind != b->kind) return false;
    if (a->is_optional != b->is_optional) return false;
    
    switch (a->kind) {
        case TYPE_KIND_VOID:
        case TYPE_KIND_BOOL:
        case TYPE_KIND_INT:
        case TYPE_KIND_FLOAT:
        case TYPE_KIND_DOUBLE:
        case TYPE_KIND_STRING:
        case TYPE_KIND_NIL:
        case TYPE_KIND_ANY:
            return true;
            
        case TYPE_KIND_ARRAY:
            return type_equals(a->data.array->element_type, b->data.array->element_type);
            
        case TYPE_KIND_DICTIONARY:
            return type_equals(a->data.dictionary->key_type, b->data.dictionary->key_type) &&
                   type_equals(a->data.dictionary->value_type, b->data.dictionary->value_type);
                   
        case TYPE_KIND_OPTIONAL:
            return type_equals(a->data.wrapped, b->data.wrapped);
            
        case TYPE_KIND_FUNCTION: {
            FunctionType* fa = a->data.function;
            FunctionType* fb = b->data.function;
            if (fa->parameter_count != fb->parameter_count) return false;
            if (!type_equals(fa->return_type, fb->return_type)) return false;
            for (size_t i = 0; i < fa->parameter_count; i++) {
                if (!type_equals(fa->parameter_types[i], fb->parameter_types[i])) return false;
            }
            return true;
        }
        
        case TYPE_KIND_TUPLE: {
            TupleType* ta = a->data.tuple;
            TupleType* tb = b->data.tuple;
            if (ta->element_count != tb->element_count) return false;
            for (size_t i = 0; i < ta->element_count; i++) {
                if (!type_equals(ta->element_types[i], tb->element_types[i])) return false;
            }
            return true;
        }
        
        case TYPE_KIND_STRUCT:
        case TYPE_KIND_CLASS:
        case TYPE_KIND_ENUM:
        case TYPE_KIND_PROTOCOL:
        case TYPE_KIND_GENERIC:
        case TYPE_KIND_UNRESOLVED:
            return a->name && b->name && strcmp(a->name, b->name) == 0;
            
        case TYPE_KIND_ALIAS:
            return type_equals(a->data.alias_target, b->data.alias_target);
    }
    
    return false;
}

bool type_is_assignable(const Type* from, const Type* to) {
    if (!from || !to) return false;
    
    if (type_equals(from, to)) return true;
    
    if (to->kind == TYPE_KIND_ANY) return true;
    
    if (from->kind == TYPE_KIND_NIL && to->is_optional) return true;
    
    if (from->kind == TYPE_KIND_OPTIONAL && to->kind == TYPE_KIND_OPTIONAL) {
        return type_is_assignable(from->data.wrapped, to->data.wrapped);
    }
    
    if (from->kind == TYPE_KIND_ALIAS) {
        return type_is_assignable(from->data.alias_target, to);
    }
    if (to->kind == TYPE_KIND_ALIAS) {
        return type_is_assignable(from, to->data.alias_target);
    }
    
    if (from->kind == TYPE_KIND_CLASS && to->kind == TYPE_KIND_CLASS && from->data.composite->supertype) {
        return type_is_assignable(from->data.composite->supertype, to);
    }
    
    if (to->kind == TYPE_KIND_PROTOCOL && from->data.composite) {
        for (size_t i = 0; i < from->data.composite->protocol_count; i++) {
            if (type_equals(from->data.composite->protocols[i], to)) {
                return true;
            }
        }
    }
    
    return false;
}

bool type_is_numeric(const Type* type) {
    if (!type) return false;
    switch (type->kind) {
        case TYPE_KIND_INT:
        case TYPE_KIND_FLOAT:
        case TYPE_KIND_DOUBLE:
            return true;
        default:
            return false;
    }
}

bool type_is_reference(const Type* type) {
    if (!type) return false;
    return type->kind == TYPE_KIND_CLASS;
}

bool type_is_value(const Type* type) {
    if (!type) return false;
    switch (type->kind) {
        case TYPE_KIND_STRUCT:
        case TYPE_KIND_ENUM:
        case TYPE_KIND_TUPLE:
            return true;
        default:
            return !type_is_reference(type);
    }
}

bool type_is_optional(const Type* type) {
    return type && type->is_optional;
}

bool type_is_collection(const Type* type) {
    if (!type) return false;
    return type->kind == TYPE_KIND_ARRAY || type->kind == TYPE_KIND_DICTIONARY;
}

Type* type_unwrap_optional(const Type* type) {
    if (!type || type->kind != TYPE_KIND_OPTIONAL) return NULL;
    return type->data.wrapped;
}

Type* type_common_type(const Type* a, const Type* b) {
    if (type_equals(a, b)) return (Type*)a;
    
    if (type_is_numeric(a) && type_is_numeric(b)) {
        if (a->kind == TYPE_KIND_DOUBLE || b->kind == TYPE_KIND_DOUBLE) {
            return type_double();
        }
        if (a->kind == TYPE_KIND_FLOAT || b->kind == TYPE_KIND_FLOAT) {
            return type_float();
        }
        return type_int();
    }
    
    if ((a->kind == TYPE_KIND_NIL && b->is_optional) ||
        (b->kind == TYPE_KIND_NIL && a->is_optional)) {
        return a->kind == TYPE_KIND_NIL ? (Type*)b : (Type*)a;
    }
    
    return type_any();
}

const char* type_to_string(const Type* type) {
    if (!type) return "<null>";
    
    static char buffer[256];
    
    switch (type->kind) {
        case TYPE_KIND_VOID: return "Void";
        case TYPE_KIND_BOOL: return "Bool";
        case TYPE_KIND_INT: return "Int";
        case TYPE_KIND_FLOAT: return "Float";
        case TYPE_KIND_DOUBLE: return "Double";
        case TYPE_KIND_STRING: return "String";
        case TYPE_KIND_NIL: return "nil";
        case TYPE_KIND_ANY: return "Any";
        
        case TYPE_KIND_ARRAY:
            snprintf(buffer, sizeof(buffer), "[%s]", 
                    type_to_string(type->data.array->element_type));
            return buffer;
            
        case TYPE_KIND_DICTIONARY:
            snprintf(buffer, sizeof(buffer), "[%s: %s]",
                    type_to_string(type->data.dictionary->key_type),
                    type_to_string(type->data.dictionary->value_type));
            return buffer;
            
        case TYPE_KIND_OPTIONAL:
            snprintf(buffer, sizeof(buffer), "%s?",
                    type_to_string(type->data.wrapped));
            return buffer;
            
        case TYPE_KIND_FUNCTION: {
            FunctionType* ft = type->data.function;
            char params[128] = "";
            for (size_t i = 0; i < ft->parameter_count; i++) {
                if (i > 0) strcat(params, ", ");
                strcat(params, type_to_string(ft->parameter_types[i]));
            }
            snprintf(buffer, sizeof(buffer), "(%s) -> %s",
                    params, type_to_string(ft->return_type));
            return buffer;
        }
        
        case TYPE_KIND_TUPLE: {
            TupleType* tt = type->data.tuple;
            char elements[128] = "";
            for (size_t i = 0; i < tt->element_count; i++) {
                if (i > 0) strcat(elements, ", ");
                strcat(elements, type_to_string(tt->element_types[i]));
            }
            snprintf(buffer, sizeof(buffer), "(%s)", elements);
            return buffer;
        }
        
        case TYPE_KIND_STRUCT:
        case TYPE_KIND_CLASS:
        case TYPE_KIND_ENUM:
        case TYPE_KIND_PROTOCOL:
        case TYPE_KIND_GENERIC:
        case TYPE_KIND_UNRESOLVED:
            return type->name ? type->name : "<unnamed>";
            
        case TYPE_KIND_ALIAS:
            return type->name ? type->name : type_to_string(type->data.alias_target);
    }
    
    return "<unknown>";
}

void type_free(Type* type) {
    if (!type) return;
    
    switch (type->kind) {
        case TYPE_KIND_ARRAY:
            free(type->data.array);
            break;
            
        case TYPE_KIND_DICTIONARY:
            free(type->data.dictionary);
            break;
            
        case TYPE_KIND_FUNCTION:
            free(type->data.function->parameter_types);
            free(type->data.function->parameter_names);
            free(type->data.function);
            break;
            
        case TYPE_KIND_TUPLE:
            free(type->data.tuple->element_types);
            free(type->data.tuple->element_names);
            free(type->data.tuple);
            break;
            
        case TYPE_KIND_STRUCT:
        case TYPE_KIND_CLASS:
        case TYPE_KIND_ENUM:
        case TYPE_KIND_PROTOCOL:
            if (type->data.composite) {
                free(type->data.composite->members);
                free(type->data.composite->methods);
                free(type->data.composite->protocols);
                free(type->data.composite);
            }
            break;
            
        case TYPE_KIND_GENERIC:
            if (type->data.generic) {
                free(type->data.generic->constraints);
                free(type->data.generic);
            }
            break;
            
        default:
            break;
    }
    
    if (type->name && type->kind != TYPE_KIND_VOID && type->kind != TYPE_KIND_BOOL &&
        type->kind != TYPE_KIND_INT && type->kind != TYPE_KIND_FLOAT &&
        type->kind != TYPE_KIND_DOUBLE && type->kind != TYPE_KIND_STRING &&
        type->kind != TYPE_KIND_NIL && type->kind != TYPE_KIND_ANY) {
        free((void*)type->name);
    }
    
    free(type);
}

static size_t hash_string(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

TypeContext* type_context_create(void) {
    TypeContext* ctx = calloc(1, sizeof(TypeContext));
    ctx->bucket_count = 64;
    ctx->buckets = calloc(ctx->bucket_count, sizeof(TypeEntry*));
    return ctx;
}

void type_context_destroy(TypeContext* ctx) {
    if (!ctx) return;
    
    for (size_t i = 0; i < ctx->bucket_count; i++) {
        TypeEntry* entry = ctx->buckets[i];
        while (entry) {
            TypeEntry* next = entry->next;
            free(entry->name);
            free(entry);
            entry = next;
        }
    }
    
    free(ctx->buckets);
    free(ctx);
}

Type* type_context_get(TypeContext* ctx, const char* name) {
    if (!ctx || !name) return NULL;
    
    size_t index = hash_string(name) % ctx->bucket_count;
    TypeEntry* entry = ctx->buckets[index];
    
    while (entry) {
        if (strcmp(entry->name, name) == 0) {
            return entry->type;
        }
        entry = entry->next;
    }
    
    return NULL;
}

void type_context_register(TypeContext* ctx, const char* name, Type* type) {
    if (!ctx || !name || !type) return;
    
    size_t index = hash_string(name) % ctx->bucket_count;
    
    TypeEntry* existing = ctx->buckets[index];
    while (existing) {
        if (strcmp(existing->name, name) == 0) {
            existing->type = type;
            return;
        }
        existing = existing->next;
    }
    
    TypeEntry* entry = calloc(1, sizeof(TypeEntry));
    entry->name = strdup(name);
    entry->type = type;
    entry->next = ctx->buckets[index];
    ctx->buckets[index] = entry;
    ctx->entry_count++;
}

void type_context_register_builtin_types(TypeContext* ctx) {
    if (!ctx) return;
    
    type_context_register(ctx, "Void", type_void());
    type_context_register(ctx, "Bool", type_bool());
    type_context_register(ctx, "Int", type_int());
    type_context_register(ctx, "Float", type_float());
    type_context_register(ctx, "Double", type_double());
    type_context_register(ctx, "String", type_string());
    type_context_register(ctx, "Any", type_any());
}

// Check if a type conforms to a protocol
bool type_conforms_to_protocol(const Type* type, const Type* protocol) {
    if (!type || !protocol || protocol->kind != TYPE_KIND_PROTOCOL) {
        return false;
    }
    
    // Check if type is a struct, class, or enum that can conform to protocols
    if (type->kind != TYPE_KIND_STRUCT && type->kind != TYPE_KIND_CLASS && 
        type->kind != TYPE_KIND_ENUM) {
        return false;
    }
    
    // Check if already conforms
    if (type->data.composite) {
        for (size_t i = 0; i < type->data.composite->protocol_count; i++) {
            if (type_equals(type->data.composite->protocols[i], protocol)) {
                return true;
            }
        }
    }
    
    // Check if all protocol requirements are satisfied
    if (protocol->data.composite) {
        // Check all required methods
        for (size_t i = 0; i < protocol->data.composite->method_count; i++) {
            TypeMethod* required = &protocol->data.composite->methods[i];
            if (!type_implements_method(type, required->name, required->type)) {
                return false;
            }
        }
        
        // Check all required properties
        for (size_t i = 0; i < protocol->data.composite->member_count; i++) {
            TypeMember* required = &protocol->data.composite->members[i];
            bool found = false;
            
            if (type->data.composite) {
                for (size_t j = 0; j < type->data.composite->member_count; j++) {
                    TypeMember* member = &type->data.composite->members[j];
                    if (strcmp(member->name, required->name) == 0 &&
                        type_equals(member->type, required->type)) {
                        found = true;
                        break;
                    }
                }
            }
            
            if (!found) return false;
        }
    }
    
    return true;
}

// Check if a type implements a specific method
bool type_implements_method(const Type* type, const char* method_name, FunctionType* method_type) {
    if (!type || !method_name || !method_type) {
        return false;
    }
    
    if (!type->data.composite) {
        return false;
    }
    
    // Look for the method in the type's methods
    for (size_t i = 0; i < type->data.composite->method_count; i++) {
        TypeMethod* method = &type->data.composite->methods[i];
        if (strcmp(method->name, method_name) == 0) {
            // Check if signatures match
            if (method->type->parameter_count != method_type->parameter_count) {
                return false;
            }
            
            // Check parameter types
            for (size_t j = 0; j < method->type->parameter_count; j++) {
                if (!type_equals(method->type->parameter_types[j], method_type->parameter_types[j])) {
                    return false;
                }
            }
            
            // Check return type
            if (!type_equals(method->type->return_type, method_type->return_type)) {
                return false;
            }
            
            return true;
        }
    }
    
    // Check supertype if class
    if (type->kind == TYPE_KIND_CLASS && type->data.composite->supertype) {
        return type_implements_method(type->data.composite->supertype, method_name, method_type);
    }
    
    return false;
}

// Add protocol conformance to a type
void type_add_protocol_conformance(Type* type, Type* protocol) {
    if (!type || !protocol || protocol->kind != TYPE_KIND_PROTOCOL) {
        return;
    }
    
    // Only struct, class, and enum can conform to protocols
    if (type->kind != TYPE_KIND_STRUCT && type->kind != TYPE_KIND_CLASS && 
        type->kind != TYPE_KIND_ENUM) {
        return;
    }
    
    // Ensure composite type exists
    if (!type->data.composite) {
        type->data.composite = calloc(1, sizeof(CompositeType));
    }
    
    // Check if already conforms
    for (size_t i = 0; i < type->data.composite->protocol_count; i++) {
        if (type_equals(type->data.composite->protocols[i], protocol)) {
            return;
        }
    }
    
    // Add protocol to the list
    size_t new_count = type->data.composite->protocol_count + 1;
    Type** new_protocols = realloc(type->data.composite->protocols, new_count * sizeof(Type*));
    if (new_protocols) {
        new_protocols[type->data.composite->protocol_count] = protocol;
        type->data.composite->protocols = new_protocols;
        type->data.composite->protocol_count = new_count;
    }
}