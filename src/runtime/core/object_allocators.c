#include "runtime/core/object.h"
#include "runtime/core/object_sizes.h"
#include "runtime/core/vm.h"
#include "utils/allocators.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
    ObjectProperty* prop = OBJ_NEW(ObjectProperty, "property");
    if (!prop) return NULL;
    
    prop->key = OBJ_ALLOC(strlen(key) + 1, "prop-key");
    if (!prop->key) {
        OBJ_FREE(prop, SIZE_OBJECT_PROPERTY);
        return NULL;
    }
    strcpy(prop->key, key);
    
    prop->value = OBJ_NEW(TaggedValue, "prop-value");
    if (!prop->value) {
        OBJ_FREE(prop->key, strlen(key) + 1);
        OBJ_FREE(prop, SIZE_OBJECT_PROPERTY);
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
        if (prop->key) {
            OBJ_FREE(prop->key, strlen(prop->key) + 1);
        }
        if (prop->value) {
            OBJ_FREE(prop->value, sizeof(TaggedValue));
        }
        OBJ_FREE(prop, SIZE_OBJECT_PROPERTY);
    }
}

// Create a new object
Object* object_create(void)
{
    Object* obj = OBJ_NEW(Object, "object");
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
    
    ObjectProperty* prop = obj->properties;
    while (prop)
    {
        ObjectProperty* next = prop->next;
        property_destroy(prop);
        prop = next;
    }
    
    OBJ_FREE(obj, SIZE_OBJECT);
}

// Get property, checking prototype chain
TaggedValue* object_get_property(Object* obj, const char* key)
{
    if (!obj) return NULL;
    
    // Check own properties first
    ObjectProperty* prop = obj->properties;
    while (prop)
    {
        if (strcmp(prop->key, key) == 0)
        {
            return prop->value;
        }
        prop = prop->next;
    }
    
    // Check prototype chain
    if (obj->prototype)
    {
        return object_get_property(obj->prototype, key);
    }
    
    return NULL;
}

// Set property (always on the object itself, not prototype)
void object_set_property(Object* obj, const char* key, TaggedValue value)
{
    if (!obj) return;
    
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
    
    // Create new property
    ObjectProperty* new_prop = property_create(key, value);
    if (!new_prop) return;
    
    new_prop->next = obj->properties;
    obj->properties = new_prop;
    obj->property_count++;
}

// Delete property
bool object_delete_property(Object* obj, const char* key)
{
    if (!obj || !obj->properties) return false;
    
    // Handle first property
    if (strcmp(obj->properties->key, key) == 0)
    {
        ObjectProperty* to_delete = obj->properties;
        obj->properties = to_delete->next;
        property_destroy(to_delete);
        obj->property_count--;
        return true;
    }
    
    // Handle rest
    ObjectProperty* current = obj->properties;
    while (current->next)
    {
        if (strcmp(current->next->key, key) == 0)
        {
            ObjectProperty* to_delete = current->next;
            current->next = to_delete->next;
            property_destroy(to_delete);
            obj->property_count--;
            return true;
        }
        current = current->next;
    }
    
    return false;
}

// Check if object has property (including prototype chain)
bool object_has_property(Object* obj, const char* key)
{
    return object_get_property(obj, key) != NULL;
}

// Check if object has own property (not in prototype chain)
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

// Array creation
Array* array_create(size_t initial_capacity)
{
    Array* array = OBJ_NEW(Array, "array");
    if (!array) return NULL;
    
    array->base.properties = NULL;
    array->base.prototype = array_prototype;
    array->base.property_count = 0;
    array->base.is_array = true;
    
    array->elements = NULL;
    array->count = 0;
    array->capacity = 0;
    
    if (initial_capacity > 0)
    {
        array->elements = OBJ_ALLOC_ZERO(SIZE_VAL_ARRAY(initial_capacity), "array-elements");
        if (!array->elements) {
            OBJ_FREE(array, SIZE_ARRAY);
            return NULL;
        }
        array->capacity = initial_capacity;
    }
    
    return array;
}

// Array destruction
void array_destroy(Array* array)
{
    if (!array) return;
    
    // Free array-specific data
    if (array->elements) {
        OBJ_FREE(array->elements, SIZE_VAL_ARRAY(array->capacity));
    }
    
    // Free base object properties
    ObjectProperty* prop = array->base.properties;
    while (prop)
    {
        ObjectProperty* next = prop->next;
        property_destroy(prop);
        prop = next;
    }
    
    OBJ_FREE(array, SIZE_ARRAY);
}

// Array operations
void array_push(Array* array, TaggedValue value)
{
    if (!array) return;
    
    if (array->count >= array->capacity)
    {
        size_t new_capacity = array->capacity < 8 ? 8 : array->capacity * 2;
        array->elements = OBJ_ALLOC_ZERO(array->elements, 
                                         SIZE_VAL_ARRAY(array->capacity),
                                         SIZE_VAL_ARRAY(new_capacity), 
                                         "array-grow");
        if (!array->elements) return; // Allocation failed
        array->capacity = new_capacity;
    }
    
    array->elements[array->count++] = value;
    
    // Update length property
    TaggedValue length_val = NUMBER_VAL((double)array->count);
    object_set_property(&array->base, "length", length_val);
}

TaggedValue array_pop(Array* array)
{
    if (!array || array->count == 0) return NIL_VAL;
    
    TaggedValue value = array->elements[--array->count];
    
    // Update length property
    TaggedValue length_val = NUMBER_VAL((double)array->count);
    object_set_property(&array->base, "length", length_val);
    
    return value;
}

TaggedValue array_get(Array* array, size_t index)
{
    if (!array || index >= array->count) return NIL_VAL;
    return array->elements[index];
}

void array_set(Array* array, size_t index, TaggedValue value)
{
    if (!array || index >= array->count) return;
    array->elements[index] = value;
}

// StructInstance creation
StructInstance* struct_instance_create(StructType* type)
{
    if (!type) return NULL;
    
    StructInstance* instance = OBJ_NEW(StructInstance, "struct-instance");
    if (!instance) return NULL;
    
    instance->base.properties = NULL;
    instance->base.prototype = NULL;  // Will be set to struct prototype if exists
    instance->base.property_count = 0;
    instance->base.is_array = false;
    
    instance->type = type;
    instance->fields = OBJ_ALLOC_ZERO(SIZE_VAL_ARRAY(type->field_count), "struct-fields");
    if (!instance->fields) {
        OBJ_FREE(instance, SIZE_STRUCT_INSTANCE);
        return NULL;
    }
    
    // Initialize fields to nil
    for (size_t i = 0; i < type->field_count; i++)
    {
        instance->fields[i] = NIL_VAL;
    }
    
    // Set prototype if one exists for this struct type
    StructPrototype* sp = struct_prototypes;
    while (sp)
    {
        if (strcmp(sp->name, type->name) == 0)
        {
            instance->base.prototype = sp->prototype;
            break;
        }
        sp = sp->next;
    }
    
    return instance;
}

// StructInstance destruction
void struct_instance_destroy(StructInstance* instance)
{
    if (!instance) return;
    
    // Free fields array
    if (instance->fields && instance->type) {
        OBJ_FREE(instance->fields, SIZE_VAL_ARRAY(instance->type->field_count));
    }
    
    // Free base object properties
    ObjectProperty* prop = instance->base.properties;
    while (prop)
    {
        ObjectProperty* next = prop->next;
        property_destroy(prop);
        prop = next;
    }
    
    OBJ_FREE(instance, SIZE_STRUCT_INSTANCE);
}

// Get field index
int struct_get_field_index(StructType* type, const char* field_name)
{
    if (!type || !field_name) return -1;
    
    for (size_t i = 0; i < type->field_count; i++)
    {
        if (strcmp(type->fields[i], field_name) == 0)
        {
            return (int)i;
        }
    }
    
    return -1;
}

// Note: This is the refactored beginning of object.c using allocators
// The rest of the file (prototype initialization, etc.) needs similar treatment