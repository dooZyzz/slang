#ifndef OBJECT_H
#define OBJECT_H

#include <stdbool.h>
#include <stddef.h>

// Forward declarations
typedef struct TaggedValue TaggedValue;
typedef struct Object Object;
typedef struct ObjectProperty ObjectProperty;
typedef struct StructType StructType;
typedef struct StructInstance StructInstance;

// Object property - linked list node
struct ObjectProperty {
    char* key;
    TaggedValue* value;
    ObjectProperty* next;
};

// Object structure with prototype chain
struct Object {
    ObjectProperty* properties;
    Object* prototype;  // Prototype object for inheritance
    size_t property_count;
    bool is_array;  // Special flag for array objects
};

// Struct type definition
struct StructType {
    const char* name;
    char** field_names;
    size_t field_count;
    Object* methods;  // Methods as object properties
};

// Struct instance with value semantics
struct StructInstance {
    StructType* type;
    TaggedValue* fields;  // Array of field values
};

// Object creation and destruction
Object* object_create(void);
Object* object_create_with_prototype(Object* prototype);
void object_destroy(Object* obj);

// Property access
TaggedValue* object_get_property(Object* obj, const char* key);
void object_set_property(Object* obj, const char* key, TaggedValue value);
bool object_has_property(Object* obj, const char* key);
bool object_has_own_property(Object* obj, const char* key);

// Prototype chain
void object_set_prototype(Object* obj, Object* prototype);
Object* object_get_prototype(Object* obj);

// Array-specific functions
Object* array_create(void);
Object* array_create_with_capacity(size_t capacity);
void array_push(Object* array, TaggedValue value);
TaggedValue array_pop(Object* array);
TaggedValue array_get(Object* array, size_t index);
void array_set(Object* array, size_t index, TaggedValue value);
size_t array_length(Object* array);

// Built-in prototypes
Object* get_object_prototype(void);
Object* get_array_prototype(void);
Object* get_string_prototype(void);
Object* get_function_prototype(void);
Object* get_number_prototype(void);

// Initialize built-in prototypes
void init_builtin_prototypes(void);

// Get or create prototype for a struct type
Object* get_struct_prototype(const char* struct_name);

// Struct type functions
StructType* struct_type_create(const char* name, char** field_names, size_t field_count);
void struct_type_destroy(StructType* type);
void struct_type_add_method(StructType* type, const char* name, TaggedValue method);

// Struct instance functions
StructInstance* struct_instance_create(StructType* type);
StructInstance* struct_instance_copy(StructInstance* instance);  // For value semantics
void struct_instance_destroy(StructInstance* instance);
TaggedValue* struct_instance_get_field(StructInstance* instance, const char* field_name);
void struct_instance_set_field(StructInstance* instance, const char* field_name, TaggedValue value);
TaggedValue* struct_instance_get_field_by_index(StructInstance* instance, size_t index);
void struct_instance_set_field_by_index(StructInstance* instance, size_t index, TaggedValue value);

#endif
