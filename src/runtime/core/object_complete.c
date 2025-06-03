#include "runtime/core/object.h"
#include "runtime/core/object_sizes.h"
#include "runtime/core/vm.h"
#include "runtime/core/gc.h"
#include "utils/allocators.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Thread-local storage for current VM (needed for GC allocation)
#ifdef _MSC_VER
    static __declspec(thread) VM* current_vm = NULL;
#else
    static __thread VM* current_vm = NULL;
#endif

void object_set_current_vm(VM* vm) {
    current_vm = vm;
}

// Global prototype objects
static Object* object_prototype = NULL;
static Object* array_prototype = NULL;
static Object* string_prototype = NULL;
static Object* function_prototype = NULL;
static Object* number_prototype = NULL;

// Struct prototype registry
typedef struct StructPrototype {
    char* name;
    Object* prototype;
    struct StructPrototype* next;
} StructPrototype;

static StructPrototype* struct_prototypes = NULL;

// Helper to create a property node
static ObjectProperty* property_create(const char* key, TaggedValue value)
{
    ObjectProperty* prop;
    
    // Use GC if available
    if (current_vm && current_vm->gc) {
        prop = GC_ALLOC(current_vm->gc, ObjectProperty, "property");
    } else {
        prop = OBJ_NEW(ObjectProperty, "property");
    }
    if (!prop) return NULL;
    
    size_t key_size = strlen(key) + 1;
    
    // Allocate key string
    if (current_vm && current_vm->gc) {
        prop->key = GC_ALLOC_ARRAY(current_vm->gc, char, key_size, "prop-key");
    } else {
        prop->key = STR_ALLOC(key_size);
    }
    if (!prop->key) {
        if (!(current_vm && current_vm->gc)) {
            OBJ_FREE(prop, SIZE_OBJECT_PROPERTY);
        }
        return NULL;
    }
    strcpy(prop->key, key);
    
    // Allocate value holder
    if (current_vm && current_vm->gc) {
        prop->value = GC_ALLOC(current_vm->gc, TaggedValue, "prop-value");
    } else {
        prop->value = OBJ_NEW(TaggedValue, "prop-value");
    }
    if (!prop->value) {
        if (!(current_vm && current_vm->gc)) {
            STR_FREE(prop->key, key_size);
            OBJ_FREE(prop, SIZE_OBJECT_PROPERTY);
        }
        return NULL;
    }
    *prop->value = value;
    prop->next = NULL;
    
    return prop;
}

// Helper to destroy a property node
static void property_destroy(ObjectProperty* prop)
{
    if (prop)
    {
        // If GC is active, it will handle memory cleanup
        if (!(current_vm && current_vm->gc)) {
            // Manual cleanup only when GC is not active
            if (prop->key) {
                STR_FREE(prop->key, strlen(prop->key) + 1);
            }
            if (prop->value) {
                OBJ_FREE(prop->value, sizeof(TaggedValue));
            }
            OBJ_FREE(prop, SIZE_OBJECT_PROPERTY);
        }
        // With GC, we don't free - GC will handle it
    }
}

// Create a new object
Object* object_create(void)
{
    Object* obj;
    
    // Use GC if available, otherwise fall back to direct allocation
    if (current_vm && current_vm->gc) {
        obj = GC_ALLOC(current_vm->gc, Object, "object");
    } else {
        obj = OBJ_NEW(Object, "object");
    }
    
    if (!obj) return NULL;
    
    obj->properties = NULL;
    obj->prototype = object_prototype;  // Default to Object.prototype
    obj->property_count = 0;
    obj->is_array = false;
    return obj;
}

// Create object with specific prototype
Object* object_create_with_prototype(Object* prototype)
{
    Object* obj = object_create();
    if (obj) {
        obj->prototype = prototype;
    }
    return obj;
}

// Destroy an object and all its properties
void object_destroy(Object* obj)
{
    if (!obj) return;
    
    // If GC is active, it will handle the memory - we just need to clean up resources
    if (current_vm && current_vm->gc) {
        // GC will free the memory, but we still need to clean up property chains
        // to prevent leaks in the property linked list
        ObjectProperty* prop = obj->properties;
        while (prop)
        {
            ObjectProperty* next = prop->next;
            // Only free the property structure, not the object itself
            property_destroy(prop);
            prop = next;
        }
        // Don't free the object - GC will handle it
    } else {
        // No GC, manual cleanup
        ObjectProperty* prop = obj->properties;
        while (prop)
        {
            ObjectProperty* next = prop->next;
            property_destroy(prop);
            prop = next;
        }
        OBJ_FREE(obj, SIZE_OBJECT);
    }
}

// Get property, checking prototype chain
// This implements JavaScript-style prototypal inheritance
// Properties are looked up in order: own properties -> prototype -> prototype's prototype -> etc.
TaggedValue* object_get_property(Object* obj, const char* key)
{
    if (!obj) return NULL;
    
    // Check own properties first (properties directly on this object)
    ObjectProperty* prop = obj->properties;
    while (prop)
    {
        if (strcmp(prop->key, key) == 0)
        {
            return prop->value;
        }
        prop = prop->next;
    }
    
    // Property not found on object itself - check prototype chain
    // This allows objects to inherit methods and properties from their prototype
    if (obj->prototype)
    {
        return object_get_property(obj->prototype, key);
    }
    
    return NULL;  // Property not found anywhere in the chain
}

// Set property (always on the object itself, not prototype)
void object_set_property(Object* obj, const char* key, TaggedValue value)
{
    if (!obj) {
        fprintf(stderr, "[ERROR] object_set_property: obj is NULL!\n");
        return;
    }
    if (!key) {
        fprintf(stderr, "[ERROR] object_set_property: key is NULL!\n");
        return;
    }
    
    // Debug for stdlib init
    if (strstr(key, "push") || strstr(key, "map")) {
        fprintf(stderr, "[DEBUG] object_set_property: obj=%p, key='%s', value.type=%d\n", 
                (void*)obj, key, value.type);
    }
    
    // Check if property already exists
    ObjectProperty* prop = obj->properties;
    while (prop)
    {
        if (strcmp(prop->key, key) == 0)
        {
            *prop->value = value;
            return;
        }
        prop = prop->next;
    }
    
    // Add new property
    ObjectProperty* new_prop = property_create(key, value);
    if (!new_prop) {
        fprintf(stderr, "[ERROR] object_set_property: Failed to create property for key='%s'\n", key);
        return;
    }
    
    new_prop->next = obj->properties;
    obj->properties = new_prop;
    obj->property_count++;
}

// Check if property exists (including prototype chain)
bool object_has_property(Object* obj, const char* key)
{
    return object_get_property(obj, key) != NULL;
}

// Check if property exists on object itself
bool object_has_own_property(Object* obj, const char* key)
{
    if (!obj) return false;
    
    ObjectProperty* prop = obj->properties;
    while (prop)
    {
        if (strcmp(prop->key, key) == 0)
        {
            return true;
        }
        prop = prop->next;
    }
    
    return false;
}

// Set prototype
void object_set_prototype(Object* obj, Object* prototype)
{
    if (obj)
    {
        obj->prototype = prototype;
    }
}

// Get prototype
Object* object_get_prototype(Object* obj)
{
    return obj ? obj->prototype : NULL;
}

// Array-specific implementation
Object* array_create(void)
{
    Object* array = object_create_with_prototype(array_prototype);
    if (!array) return NULL;
    
    array->is_array = true;
    
    // Initialize length property
    TaggedValue length_val = NUMBER_VAL(0);
    object_set_property(array, "length", length_val);
    
    return array;
}

Object* array_create_with_capacity(size_t capacity)
{
    (void)capacity;
    // For now, just create a regular array
    // We can optimize with pre-allocated space later
    return array_create();
}

void array_push(Object* array, TaggedValue value)
{
    if (!array || !array->is_array) return;
    
    // Get current length
    TaggedValue* length_prop = object_get_property(array, "length");
    if (!length_prop || length_prop->type != VAL_NUMBER) return;
    
    size_t length = (size_t)length_prop->as.number;
    
    // Set value at index
    char index_str[32];
    snprintf(index_str, sizeof(index_str), "%zu", length);
    object_set_property(array, index_str, value);
    
    // Update length
    TaggedValue new_length = NUMBER_VAL((double)(length + 1));
    object_set_property(array, "length", new_length);
}

TaggedValue array_pop(Object* array)
{
    if (!array || !array->is_array) return NIL_VAL;
    
    // Get current length
    TaggedValue* length_prop = object_get_property(array, "length");
    if (!length_prop || length_prop->type != VAL_NUMBER) return NIL_VAL;
    
    size_t length = (size_t)length_prop->as.number;
    if (length == 0) return NIL_VAL;
    
    // Get value at last index
    char index_str[32];
    snprintf(index_str, sizeof(index_str), "%zu", length - 1);
    TaggedValue* value = object_get_property(array, index_str);
    TaggedValue result = value ? *value : NIL_VAL;
    
    // Remove property
    // TODO: Implement property removal
    
    // Update length
    TaggedValue new_length = NUMBER_VAL((double)(length - 1));
    object_set_property(array, "length", new_length);
    
    return result;
}

// Array implementation using the object system
// Arrays store elements as properties with numeric string keys ("0", "1", "2", etc.)
// This allows arrays to have methods and behave like objects while maintaining array semantics

TaggedValue array_get(Object* array, size_t index)
{
    if (!array || !array->is_array) return NIL_VAL;
    
    // Convert numeric index to string key
    char index_str[32];
    snprintf(index_str, sizeof(index_str), "%zu", index);
    TaggedValue* value = object_get_property(array, index_str);
    
    return value ? *value : NIL_VAL;
}

void array_set(Object* array, size_t index, TaggedValue value)
{
    if (!array || !array->is_array) return;
    
    // Convert numeric index to string key
    char index_str[32];
    snprintf(index_str, sizeof(index_str), "%zu", index);
    object_set_property(array, index_str, value);
    
    // Update length if necessary - arrays maintain a "length" property
    // that tracks the highest index + 1
    TaggedValue* length_prop = object_get_property(array, "length");
    if (length_prop && length_prop->type == VAL_NUMBER)
    {
        size_t current_length = (size_t)length_prop->as.number;
        if (index >= current_length)
        {
            TaggedValue new_length = NUMBER_VAL((double)(index + 1));
            object_set_property(array, "length", new_length);
        }
    }
}

size_t array_length(Object* array)
{
    if (!array || !array->is_array) return 0;
    
    TaggedValue* length_prop = object_get_property(array, "length");
    if (length_prop && length_prop->type == VAL_NUMBER)
    {
        return (size_t)length_prop->as.number;
    }
    
    return 0;
}

// Initialize built-in prototypes
void init_builtin_prototypes(void)
{
    // Skip if already initialized
    if (object_prototype) {
        return;
    }
    
    // Create Object.prototype
    if (current_vm && current_vm->gc) {
        object_prototype = GC_ALLOC(current_vm->gc, Object, "object-prototype");
    } else {
        object_prototype = OBJ_NEW(Object, "object-prototype");
    }
    if (!object_prototype) return;
    
    object_prototype->properties = NULL;
    object_prototype->prototype = NULL;  // Object.prototype has no prototype
    object_prototype->property_count = 0;
    object_prototype->is_array = false;
    
    // TODO: Add Object.prototype methods (toString, valueOf, etc.)
    
    // Create Array.prototype
    array_prototype = object_create_with_prototype(object_prototype);
    // Array methods will be added by stdlib
    
    // Create String.prototype
    string_prototype = object_create_with_prototype(object_prototype);
    // String methods will be added by stdlib
    
    // Create Function.prototype
    function_prototype = object_create_with_prototype(object_prototype);
    // TODO: Add function methods (call, apply, bind)
    
    // Create Number.prototype
    number_prototype = object_create_with_prototype(object_prototype);
    // Number methods will be added by stdlib
}

// Getters for built-in prototypes
Object* get_object_prototype(void)
{
    return object_prototype;
}

Object* get_array_prototype(void)
{
    return array_prototype;
}

Object* get_string_prototype(void)
{
    return string_prototype;
}

Object* get_function_prototype(void)
{
    return function_prototype;
}

Object* get_number_prototype(void)
{
    return number_prototype;
}

// Struct type creation
StructType* struct_type_create(const char* name, char** field_names, size_t field_count)
{
    StructType* type = OBJ_NEW(StructType, "struct-type");
    if (!type) return NULL;
    
    size_t name_size = strlen(name) + 1;
    type->name = STR_ALLOC(name_size);
    if (!type->name) {
        OBJ_FREE(type, SIZE_STRUCT_TYPE);
        return NULL;
    }
    strcpy((char*)type->name, name);
    
    type->field_count = field_count;
    
    // Copy field names
    type->field_names = OBJ_ALLOC(SIZE_STR_ARRAY(field_count), "field-names");
    if (!type->field_names) {
        STR_FREE((char*)type->name, name_size);
        OBJ_FREE(type, SIZE_STRUCT_TYPE);
        return NULL;
    }
    
    for (size_t i = 0; i < field_count; i++)
    {
        size_t field_name_size = strlen(field_names[i]) + 1;
        type->field_names[i] = STR_ALLOC(field_name_size);
        if (!type->field_names[i]) {
            // Clean up already allocated field names
            for (size_t j = 0; j < i; j++) {
                STR_FREE(type->field_names[j], strlen(type->field_names[j]) + 1);
            }
            OBJ_FREE(type->field_names, SIZE_STR_ARRAY(field_count));
            STR_FREE((char*)type->name, name_size);
            OBJ_FREE(type, SIZE_STRUCT_TYPE);
            return NULL;
        }
        strcpy(type->field_names[i], field_names[i]);
    }
    
    // Create methods object
    type->methods = object_create();
    if (!type->methods) {
        // Clean up
        for (size_t i = 0; i < field_count; i++) {
            STR_FREE(type->field_names[i], strlen(type->field_names[i]) + 1);
        }
        OBJ_FREE(type->field_names, SIZE_STR_ARRAY(field_count));
        STR_FREE((char*)type->name, name_size);
        OBJ_FREE(type, SIZE_STRUCT_TYPE);
        return NULL;
    }
    
    return type;
}

// Destroy struct type
void struct_type_destroy(StructType* type)
{
    if (!type) return;
    
    if (type->name) {
        STR_FREE((char*)type->name, strlen(type->name) + 1);
    }
    
    if (type->field_names) {
        for (size_t i = 0; i < type->field_count; i++)
        {
            if (type->field_names[i]) {
                STR_FREE(type->field_names[i], strlen(type->field_names[i]) + 1);
            }
        }
        OBJ_FREE(type->field_names, SIZE_STR_ARRAY(type->field_count));
    }
    
    if (type->methods) {
        object_destroy(type->methods);
    }
    
    OBJ_FREE(type, SIZE_STRUCT_TYPE);
}

// Add method to struct type
void struct_type_add_method(StructType* type, const char* name, TaggedValue method)
{
    if (!type || !type->methods) return;
    object_set_property(type->methods, name, method);
}

// Create struct instance
StructInstance* struct_instance_create(StructType* type)
{
    if (!type) return NULL;
    
    StructInstance* instance = OBJ_NEW(StructInstance, "struct-instance");
    if (!instance) return NULL;
    
    instance->type = type;
    instance->fields = OBJ_ALLOC_ZERO(SIZE_VAL_ARRAY(type->field_count), "struct-fields");
    if (!instance->fields) {
        OBJ_FREE(instance, SIZE_STRUCT_INSTANCE);
        return NULL;
    }
    
    // Initialize fields to nil
    for (size_t i = 0; i < type->field_count; i++)
    {
        instance->fields[i].type = VAL_NIL;
    }
    
    return instance;
}

// Copy struct instance (for value semantics)
StructInstance* struct_instance_copy(StructInstance* instance)
{
    if (!instance) return NULL;
    
    StructInstance* copy = OBJ_NEW(StructInstance, "struct-copy");
    if (!copy) return NULL;
    
    copy->type = instance->type;
    copy->fields = OBJ_ALLOC(SIZE_VAL_ARRAY(instance->type->field_count), "struct-fields-copy");
    if (!copy->fields) {
        OBJ_FREE(copy, SIZE_STRUCT_INSTANCE);
        return NULL;
    }
    
    // Deep copy fields
    for (size_t i = 0; i < instance->type->field_count; i++)
    {
        copy->fields[i] = instance->fields[i];
        
        // If field is a struct, recursively copy
        if (instance->fields[i].type == VAL_STRUCT)
        {
            copy->fields[i].as.object = (void*)struct_instance_copy((StructInstance*)instance->fields[i].as.object);
            if (!copy->fields[i].as.object) {
                // Clean up partial copy
                for (size_t j = 0; j < i; j++) {
                    if (copy->fields[j].type == VAL_STRING) {
                        STR_FREE((char*)copy->fields[j].as.string, strlen(copy->fields[j].as.string) + 1);
                    } else if (copy->fields[j].type == VAL_STRUCT) {
                        struct_instance_destroy((StructInstance*)copy->fields[j].as.object);
                    }
                }
                OBJ_FREE(copy->fields, SIZE_VAL_ARRAY(instance->type->field_count));
                OBJ_FREE(copy, SIZE_STRUCT_INSTANCE);
                return NULL;
            }
        }
        // If field is a string, duplicate it
        else if (instance->fields[i].type == VAL_STRING)
        {
            copy->fields[i].as.string = STR_DUP(instance->fields[i].as.string);
            if (!copy->fields[i].as.string) {
                // Clean up partial copy
                for (size_t j = 0; j < i; j++) {
                    if (copy->fields[j].type == VAL_STRING) {
                        STR_FREE((char*)copy->fields[j].as.string, strlen(copy->fields[j].as.string) + 1);
                    } else if (copy->fields[j].type == VAL_STRUCT) {
                        struct_instance_destroy((StructInstance*)copy->fields[j].as.object);
                    }
                }
                OBJ_FREE(copy->fields, SIZE_VAL_ARRAY(instance->type->field_count));
                OBJ_FREE(copy, SIZE_STRUCT_INSTANCE);
                return NULL;
            }
        }
    }
    
    return copy;
}

// Destroy struct instance
void struct_instance_destroy(StructInstance* instance)
{
    if (!instance) return;
    
    // Clean up field values if needed
    if (instance->fields && instance->type) {
        for (size_t i = 0; i < instance->type->field_count; i++)
        {
            if (instance->fields[i].type == VAL_STRING)
            {
                STR_FREE((char*)instance->fields[i].as.string, strlen(instance->fields[i].as.string) + 1);
            }
            else if (instance->fields[i].type == VAL_STRUCT)
            {
                struct_instance_destroy((StructInstance*)instance->fields[i].as.object);
            }
        }
        
        OBJ_FREE(instance->fields, SIZE_VAL_ARRAY(instance->type->field_count));
    }
    
    OBJ_FREE(instance, SIZE_STRUCT_INSTANCE);
}

// Get field by name
TaggedValue* struct_instance_get_field(StructInstance* instance, const char* field_name)
{
    if (!instance || !field_name) return NULL;
    
    // Find field index
    for (size_t i = 0; i < instance->type->field_count; i++)
    {
        if (strcmp(instance->type->field_names[i], field_name) == 0)
        {
            return &instance->fields[i];
        }
    }
    
    return NULL;
}

// Set field by name
void struct_instance_set_field(StructInstance* instance, const char* field_name, TaggedValue value)
{
    if (!instance || !field_name) return;
    
    // Find field index
    for (size_t i = 0; i < instance->type->field_count; i++)
    {
        if (strcmp(instance->type->field_names[i], field_name) == 0)
        {
            // Clean up old value if needed
            if (instance->fields[i].type == VAL_STRING)
            {
                STR_FREE((char*)instance->fields[i].as.string, strlen(instance->fields[i].as.string) + 1);
            }
            else if (instance->fields[i].type == VAL_STRUCT)
            {
                struct_instance_destroy((StructInstance*)instance->fields[i].as.object);
            }
            
            // Set new value
            instance->fields[i] = value;
            
            // Copy string if needed
            if (value.type == VAL_STRING)
            {
                instance->fields[i].as.string = STR_DUP(value.as.string);
            }
            // Copy struct if needed (value semantics)
            else if (value.type == VAL_STRUCT)
            {
                instance->fields[i].as.object = (void*)struct_instance_copy((StructInstance*)value.as.object);
            }
            
            return;
        }
    }
}

// Get field by index
TaggedValue* struct_instance_get_field_by_index(StructInstance* instance, size_t index)
{
    if (!instance || index >= instance->type->field_count) return NULL;
    return &instance->fields[index];
}

// Set field by index
void struct_instance_set_field_by_index(StructInstance* instance, size_t index, TaggedValue value)
{
    if (!instance || index >= instance->type->field_count) return;
    
    // Clean up old value if needed
    if (instance->fields[index].type == VAL_STRING)
    {
        STR_FREE((char*)instance->fields[index].as.string, strlen(instance->fields[index].as.string) + 1);
    }
    else if (instance->fields[index].type == VAL_STRUCT)
    {
        struct_instance_destroy((StructInstance*)instance->fields[index].as.object);
    }
    
    // Set new value
    instance->fields[index] = value;
    
    // Copy string if needed
    if (value.type == VAL_STRING)
    {
        instance->fields[index].as.string = STR_DUP(value.as.string);
    }
    // Copy struct if needed (value semantics)
    else if (value.type == VAL_STRUCT)
    {
        instance->fields[index].as.object = (void*)struct_instance_copy((StructInstance*)value.as.object);
    }
}

// Get or create prototype for a struct type
Object* get_struct_prototype(const char* struct_name)
{
    // Search for existing prototype
    StructPrototype* current = struct_prototypes;
    while (current)
    {
        if (strcmp(current->name, struct_name) == 0)
        {
            // fprintf(stderr, "[DEBUG] Found existing struct prototype for '%s'\n", struct_name);
            return current->prototype;
        }
        current = current->next;
    }
    
    // Create new prototype
    Object* proto = object_create_with_prototype(object_prototype);
    if (!proto) return NULL;
    
    // fprintf(stderr, "[DEBUG] Created new struct prototype for '%s'\n", struct_name);
    
    // Add to registry
    StructPrototype* new_entry = OBJ_NEW(StructPrototype, "struct-prototype");
    if (!new_entry) {
        object_destroy(proto);
        return NULL;
    }
    
    new_entry->name = STR_DUP(struct_name);
    if (!new_entry->name) {
        OBJ_FREE(new_entry, sizeof(StructPrototype));
        object_destroy(proto);
        return NULL;
    }
    
    new_entry->prototype = proto;
    new_entry->next = struct_prototypes;
    struct_prototypes = new_entry;
    
    return proto;
}