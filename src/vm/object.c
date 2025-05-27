#include "vm/object.h"
#include "vm/vm.h"
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
    ObjectProperty* prop = malloc(sizeof(ObjectProperty));
    prop->key = strdup(key);
    prop->value = malloc(sizeof(TaggedValue));
    *prop->value = value;
    prop->next = NULL;
    return prop;
}

// Helper to destroy a property node
static void property_destroy(ObjectProperty* prop)
{
    if (prop)
    {
        free(prop->key);
        free(prop->value);
        free(prop);
    }
}

// Create a new object
Object* object_create(void)
{
    Object* obj = malloc(sizeof(Object));
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
    obj->prototype = prototype;
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
    
    free(obj);
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
    
    // Add new property
    ObjectProperty* new_prop = property_create(key, value);
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

// Native array methods - moved to stdlib/stdlib.c
#if 0
static TaggedValue array_push_method(int arg_count, TaggedValue* args)
{
    if (arg_count < 2 || args[0].type != VAL_OBJECT) return NIL_VAL;
    
    Object* array = (Object*)args[0].as.object;
    if (!array->is_array) return NIL_VAL;
    
    for (int i = 1; i < arg_count; i++)
    {
        array_push(array, args[i]);
    }
    
    return NUMBER_VAL((double)array_length(array));
}

static TaggedValue array_pop_method(int arg_count, TaggedValue* args)
{
    if (arg_count < 1 || args[0].type != VAL_OBJECT) return NIL_VAL;
    
    Object* array = (Object*)args[0].as.object;
    if (!array->is_array) return NIL_VAL;
    
    return array_pop(array);
}

// Note: array length is a property, not a method, so we don't need a getter function

// Array map method - creates a new array with results of calling callback on each element
static TaggedValue array_map_method(int arg_count, TaggedValue* args)
{
    if (arg_count < 2 || args[0].type != VAL_OBJECT) return NIL_VAL;
    
    Object* array = (Object*)args[0].as.object;
    if (!array->is_array) return NIL_VAL;
    
    TaggedValue callback = args[1];
    if (!IS_FUNCTION(callback) && !IS_CLOSURE(callback) && !IS_NATIVE(callback)) {
        return NIL_VAL; // Callback must be callable
    }
    
    size_t length = array_length(array);
    Object* result = array_create();
    
    // For each element, call the callback and push result to new array
    for (size_t i = 0; i < length; i++) {
        TaggedValue element = array_get(array, i);
        
        // Prepare arguments for callback: element, index, array
        TaggedValue callback_args[3];
        callback_args[0] = element;
        callback_args[1] = NUMBER_VAL((double)i);
        callback_args[2] = args[0]; // Original array
        
        TaggedValue mapped_value;
        
        if (IS_NATIVE(callback)) {
            NativeFn native = AS_NATIVE(callback);
            mapped_value = native(3, callback_args);
        } else {
            // For user functions/closures, we need VM support
            // For now, we'll return the element unchanged as a placeholder
            // TODO: Add VM support for calling user functions from native code
            mapped_value = element;
        }
        
        array_push(result, mapped_value);
    }
    
    return OBJECT_VAL(result);
}

// Array filter method - creates a new array with elements that pass the test
static TaggedValue array_filter_method(int arg_count, TaggedValue* args)
{
    if (arg_count < 2 || args[0].type != VAL_OBJECT) return NIL_VAL;
    
    Object* array = (Object*)args[0].as.object;
    if (!array->is_array) return NIL_VAL;
    
    TaggedValue callback = args[1];
    if (!IS_FUNCTION(callback) && !IS_CLOSURE(callback) && !IS_NATIVE(callback)) {
        return NIL_VAL; // Callback must be callable
    }
    
    size_t length = array_length(array);
    Object* result = array_create();
    
    // For each element, call the callback and include if truthy
    for (size_t i = 0; i < length; i++) {
        TaggedValue element = array_get(array, i);
        
        // Prepare arguments for callback: element, index, array
        TaggedValue callback_args[3];
        callback_args[0] = element;
        callback_args[1] = NUMBER_VAL((double)i);
        callback_args[2] = args[0]; // Original array
        
        TaggedValue test_result;
        
        if (IS_NATIVE(callback)) {
            NativeFn native = AS_NATIVE(callback);
            test_result = native(3, callback_args);
        } else {
            // For user functions/closures, we need VM support
            // For now, we'll include all elements as a placeholder
            // TODO: Add VM support for calling user functions from native code
            test_result = BOOL_VAL(true);
        }
        
        // Include element if test result is truthy
        if ((test_result.type == VAL_BOOL && AS_BOOL(test_result)) ||
            (test_result.type == VAL_NUMBER && AS_NUMBER(test_result) != 0) ||
            (test_result.type == VAL_STRING && strlen(AS_STRING(test_result)) > 0)) {
            array_push(result, element);
        }
    }
    
    return OBJECT_VAL(result);
}

// Array reduce method - reduces array to single value by calling callback on each element
static TaggedValue array_reduce_method(int arg_count, TaggedValue* args)
{
    if (arg_count < 2 || args[0].type != VAL_OBJECT) return NIL_VAL;
    
    Object* array = (Object*)args[0].as.object;
    if (!array->is_array) return NIL_VAL;
    
    TaggedValue callback = args[1];
    if (!IS_FUNCTION(callback) && !IS_CLOSURE(callback) && !IS_NATIVE(callback)) {
        return NIL_VAL; // Callback must be callable
    }
    
    size_t length = array_length(array);
    if (length == 0) {
        // Empty array with no initial value
        if (arg_count < 3) return NIL_VAL;
        return args[2]; // Return initial value
    }
    
    TaggedValue accumulator;
    size_t start_index;
    
    if (arg_count >= 3) {
        // Initial value provided
        accumulator = args[2];
        start_index = 0;
    } else {
        // No initial value, use first element
        accumulator = array_get(array, 0);
        start_index = 1;
    }
    
    // For each element, call the callback with accumulator
    for (size_t i = start_index; i < length; i++) {
        TaggedValue element = array_get(array, i);
        
        // Prepare arguments for callback: accumulator, currentValue, currentIndex, array
        TaggedValue callback_args[4];
        callback_args[0] = accumulator;
        callback_args[1] = element;
        callback_args[2] = NUMBER_VAL((double)i);
        callback_args[3] = args[0]; // Original array
        
        if (IS_NATIVE(callback)) {
            NativeFn native = AS_NATIVE(callback);
            accumulator = native(4, callback_args);
        } else {
            // For user functions/closures, we need VM support
            // For now, we'll return the accumulator unchanged as a placeholder
            // TODO: Add VM support for calling user functions from native code
            // accumulator = accumulator; // No change
        }
    }
    
    return accumulator;
}
#endif


// Initialize built-in prototypes
void init_builtin_prototypes(void)
{
    // Create Object.prototype
    object_prototype = malloc(sizeof(Object));
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
    StructType* type = malloc(sizeof(StructType));
    type->name = strdup(name);
    type->field_count = field_count;
    
    // Copy field names
    type->field_names = malloc(field_count * sizeof(char*));
    for (size_t i = 0; i < field_count; i++)
    {
        type->field_names[i] = strdup(field_names[i]);
    }
    
    // Create methods object
    type->methods = object_create();
    
    return type;
}

// Destroy struct type
void struct_type_destroy(StructType* type)
{
    if (!type) return;
    
    free((char*)type->name);
    
    for (size_t i = 0; i < type->field_count; i++)
    {
        free(type->field_names[i]);
    }
    free(type->field_names);
    
    object_destroy(type->methods);
    free(type);
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
    
    StructInstance* instance = malloc(sizeof(StructInstance));
    instance->type = type;
    instance->fields = calloc(type->field_count, sizeof(TaggedValue));
    
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
    
    StructInstance* copy = malloc(sizeof(StructInstance));
    copy->type = instance->type;
    copy->fields = malloc(instance->type->field_count * sizeof(TaggedValue));
    
    // Deep copy fields
    for (size_t i = 0; i < instance->type->field_count; i++)
    {
        copy->fields[i] = instance->fields[i];
        
        // If field is a struct, recursively copy
        if (instance->fields[i].type == VAL_STRUCT)
        {
            copy->fields[i].as.object = (void*)struct_instance_copy((StructInstance*)instance->fields[i].as.object);
        }
        // If field is a string, duplicate it
        else if (instance->fields[i].type == VAL_STRING)
        {
            copy->fields[i].as.string = strdup(instance->fields[i].as.string);
        }
    }
    
    return copy;
}

// Destroy struct instance
void struct_instance_destroy(StructInstance* instance)
{
    if (!instance) return;
    
    // Clean up field values if needed
    for (size_t i = 0; i < instance->type->field_count; i++)
    {
        if (instance->fields[i].type == VAL_STRING)
        {
            free(instance->fields[i].as.string);
        }
        else if (instance->fields[i].type == VAL_STRUCT)
        {
            struct_instance_destroy((StructInstance*)instance->fields[i].as.object);
        }
    }
    
    free(instance->fields);
    free(instance);
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
                free(instance->fields[i].as.string);
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
                instance->fields[i].as.string = strdup(value.as.string);
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
        free(instance->fields[index].as.string);
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
        instance->fields[index].as.string = strdup(value.as.string);
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
    // fprintf(stderr, "[DEBUG] Created new struct prototype for '%s'\n", struct_name);
    
    // Add to registry
    StructPrototype* new_entry = malloc(sizeof(StructPrototype));
    new_entry->name = strdup(struct_name);
    new_entry->prototype = proto;
    new_entry->next = struct_prototypes;
    struct_prototypes = new_entry;
    
    return proto;
}
