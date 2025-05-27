#ifndef LANG_TYPE_H
#define LANG_TYPE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TYPE_KIND_VOID,
    TYPE_KIND_BOOL,
    TYPE_KIND_INT,
    TYPE_KIND_FLOAT,
    TYPE_KIND_DOUBLE,
    TYPE_KIND_STRING,
    TYPE_KIND_NIL,
    TYPE_KIND_ANY,
    TYPE_KIND_ARRAY,
    TYPE_KIND_DICTIONARY,
    TYPE_KIND_OPTIONAL,
    TYPE_KIND_FUNCTION,
    TYPE_KIND_TUPLE,
    TYPE_KIND_STRUCT,
    TYPE_KIND_CLASS,
    TYPE_KIND_ENUM,
    TYPE_KIND_PROTOCOL,
    TYPE_KIND_GENERIC,
    TYPE_KIND_ALIAS,
    TYPE_KIND_UNRESOLVED
} TypeKind;

typedef struct Type Type;
typedef struct FunctionType FunctionType;
typedef struct ArrayType ArrayType;
typedef struct DictionaryType DictionaryType;
typedef struct TupleType TupleType;
typedef struct CompositeType CompositeType;
typedef struct GenericType GenericType;

struct Type {
    TypeKind kind;
    const char* name;
    bool is_mutable;
    bool is_optional;
    
    union {
        ArrayType* array;
        DictionaryType* dictionary;
        FunctionType* function;
        TupleType* tuple;
        CompositeType* composite;
        GenericType* generic;
        Type* wrapped;
        Type* alias_target;
    } data;
};

struct ArrayType {
    Type* element_type;
};

struct DictionaryType {
    Type* key_type;
    Type* value_type;
};

struct FunctionType {
    Type** parameter_types;
    const char** parameter_names;
    size_t parameter_count;
    Type* return_type;
    bool is_async;
    bool is_throwing;
    bool is_mutating;
};

struct TupleType {
    Type** element_types;
    const char** element_names;
    size_t element_count;
};

typedef struct {
    const char* name;
    Type* type;
    bool is_let;
    bool is_static;
    bool is_private;
} TypeMember;

typedef struct {
    const char* name;
    FunctionType* type;
    bool is_static;
    bool is_private;
    bool is_mutating;
    bool is_override;
    bool is_required;
} TypeMethod;

struct CompositeType {
    TypeMember* members;
    size_t member_count;
    TypeMethod* methods;
    size_t method_count;
    Type* supertype;
    Type** protocols;
    size_t protocol_count;
};

struct GenericType {
    const char* name;
    Type** constraints;
    size_t constraint_count;
};

Type* type_void(void);
Type* type_bool(void);
Type* type_int(void);
Type* type_float(void);
Type* type_double(void);
Type* type_string(void);
Type* type_nil(void);
Type* type_any(void);

Type* type_array(Type* element_type);
Type* type_dictionary(Type* key_type, Type* value_type);
Type* type_optional(Type* wrapped);
Type* type_function(Type** params, size_t param_count, Type* return_type);
Type* type_tuple(Type** elements, size_t element_count);

Type* type_struct(const char* name);
Type* type_class(const char* name);
Type* type_enum(const char* name);
Type* type_protocol(const char* name);

Type* type_generic(const char* name);
Type* type_alias(const char* name, Type* target);
Type* type_unresolved(const char* name);

bool type_equals(const Type* a, const Type* b);
bool type_is_assignable(const Type* from, const Type* to);
bool type_is_numeric(const Type* type);
bool type_is_reference(const Type* type);
bool type_is_value(const Type* type);
bool type_is_optional(const Type* type);
bool type_is_collection(const Type* type);

Type* type_unwrap_optional(const Type* type);
Type* type_common_type(const Type* a, const Type* b);

// Protocol/Trait conformance
bool type_conforms_to_protocol(const Type* type, const Type* protocol);
bool type_implements_method(const Type* type, const char* method_name, FunctionType* method_type);
void type_add_protocol_conformance(Type* type, Type* protocol);

const char* type_to_string(const Type* type);
void type_free(Type* type);

typedef struct TypeContext TypeContext;

TypeContext* type_context_create(void);
void type_context_destroy(TypeContext* ctx);

Type* type_context_get(TypeContext* ctx, const char* name);
void type_context_register(TypeContext* ctx, const char* name, Type* type);
void type_context_register_builtin_types(TypeContext* ctx);

#endif