
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "runtime/core/object.h"
#include "runtime/core/vm.h"

// Test object creation and destruction
DEFINE_TEST(create_destroy_object)
{
    Object* obj = object_create();
    TEST_ASSERT(suite, obj != NULL, "create_destroy_object");
    TEST_ASSERT(suite, obj->properties == NULL, "create_destroy_object");
    TEST_ASSERT(suite, obj->property_count == 0, "create_destroy_object");
    TEST_ASSERT(suite, obj->is_array == false, "create_destroy_object");
    
    object_destroy(obj);
}

// Test setting and getting properties
DEFINE_TEST(set_get_property)
{
    Object* obj = object_create();
    
    // Set a string property
    TaggedValue str_val = {.type = VAL_STRING, .as.string = "Hello"};
    object_set_property(obj, "greeting", str_val);
    
    // Set a number property
    TaggedValue num_val = {.type = VAL_NUMBER, .as.number = 42.0};
    object_set_property(obj, "answer", num_val);
    
    // Get properties back
    TaggedValue* greeting = object_get_property(obj, "greeting");
    TEST_ASSERT(suite, greeting != NULL, "set_get_property");
    TEST_ASSERT(suite, greeting->type == VAL_STRING, "set_get_property");
    TEST_ASSERT(suite, strcmp(greeting->as.string, "Hello") == 0, "set_get_property");
    
    TaggedValue* answer = object_get_property(obj, "answer");
    TEST_ASSERT(suite, answer != NULL, "set_get_property");
    TEST_ASSERT(suite, answer->type == VAL_NUMBER, "set_get_property");
    TEST_ASSERT(suite, answer->as.number == 42.0, "set_get_property");
    
    // Test non-existent property
    TaggedValue* nonexistent = object_get_property(obj, "doesnotexist");
    TEST_ASSERT(suite, nonexistent == NULL, "set_get_property");
    
    object_destroy(obj);
}

// Test property overwriting
DEFINE_TEST(overwrite_property)
{
    Object* obj = object_create();
    
    // Set initial value
    TaggedValue val1 = {.type = VAL_NUMBER, .as.number = 10.0};
    object_set_property(obj, "value", val1);
    
    TaggedValue* result = object_get_property(obj, "value");
    TEST_ASSERT(suite, result->as.number == 10.0, "overwrite_property");
    
    // Overwrite with new value
    TaggedValue val2 = {.type = VAL_NUMBER, .as.number = 20.0};
    object_set_property(obj, "value", val2);
    
    result = object_get_property(obj, "value");
    TEST_ASSERT(suite, result->as.number == 20.0, "overwrite_property");
    TEST_ASSERT(suite, obj->property_count == 1, "overwrite_property"); // Should still be 1
    
    object_destroy(obj);
}

// Test multiple properties
DEFINE_TEST(multiple_properties)
{
    Object* obj = object_create();
    
    // Add several properties
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "prop%d", i);
        TaggedValue val = {.type = VAL_NUMBER, .as.number = (double)i};
        object_set_property(obj, key, val);
    }
    
    TEST_ASSERT(suite, obj->property_count == 10, "multiple_properties");
    
    // Verify all properties exist
    for (int i = 0; i < 10; i++) {
        char key[32];
        snprintf(key, sizeof(key), "prop%d", i);
        TaggedValue* val = object_get_property(obj, key);
        TEST_ASSERT(suite, val != NULL, "multiple_properties");
        TEST_ASSERT(suite, val->type == VAL_NUMBER, "multiple_properties");
        TEST_ASSERT(suite, val->as.number == (double)i, "multiple_properties");
    }
    
    object_destroy(obj);
}

// Test object with prototype
DEFINE_TEST(object_with_prototype)
{
    // Create prototype object
    Object* proto = object_create();
    TaggedValue proto_val = {.type = VAL_STRING, .as.string = "inherited"};
    object_set_property(proto, "inherited_prop", proto_val);
    
    // Create object with prototype
    Object* obj = object_create_with_prototype(proto);
    TEST_ASSERT(suite, obj->prototype == proto, "object_with_prototype");
    
    // Set own property
    TaggedValue own_val = {.type = VAL_STRING, .as.string = "own"};
    object_set_property(obj, "own_prop", own_val);
    
    // Test own property
    TaggedValue* own_result = object_get_property(obj, "own_prop");
    TEST_ASSERT(suite, own_result != NULL, "object_with_prototype");
    TEST_ASSERT(suite, strcmp(own_result->as.string, "own") == 0, "object_with_prototype");
    
    // Test inherited property
    TaggedValue* inherited_result = object_get_property(obj, "inherited_prop");
    TEST_ASSERT(suite, inherited_result != NULL, "object_with_prototype");
    TEST_ASSERT(suite, strcmp(inherited_result->as.string, "inherited") == 0, "object_with_prototype");
    
    object_destroy(obj);
    object_destroy(proto);
}

// Test property shadowing
DEFINE_TEST(property_shadowing)
{
    // Create prototype with a property
    Object* proto = object_create();
    TaggedValue proto_val = {.type = VAL_NUMBER, .as.number = 100.0};
    object_set_property(proto, "value", proto_val);
    
    // Create object with same property name
    Object* obj = object_create_with_prototype(proto);
    TaggedValue obj_val = {.type = VAL_NUMBER, .as.number = 200.0};
    object_set_property(obj, "value", obj_val);
    
    // Object's own property should shadow prototype's
    TaggedValue* result = object_get_property(obj, "value");
    TEST_ASSERT(suite, result != NULL, "property_shadowing");
    TEST_ASSERT(suite, result->as.number == 200.0, "property_shadowing");
    
    // Prototype should still have original value
    TaggedValue* proto_result = object_get_property(proto, "value");
    TEST_ASSERT(suite, proto_result->as.number == 100.0, "property_shadowing");
    
    object_destroy(obj);
    object_destroy(proto);
}

// Test deep prototype chain
DEFINE_TEST(deep_prototype_chain)
{
    // Create a chain: obj -> proto1 -> proto2
    Object* proto2 = object_create();
    TaggedValue val2 = {.type = VAL_STRING, .as.string = "from proto2"};
    object_set_property(proto2, "deep_prop", val2);
    
    Object* proto1 = object_create_with_prototype(proto2);
    TaggedValue val1 = {.type = VAL_STRING, .as.string = "from proto1"};
    object_set_property(proto1, "mid_prop", val1);
    
    Object* obj = object_create_with_prototype(proto1);
    TaggedValue val0 = {.type = VAL_STRING, .as.string = "from obj"};
    object_set_property(obj, "own_prop", val0);
    
    // Test access to all levels
    TaggedValue* own = object_get_property(obj, "own_prop");
    TEST_ASSERT(suite, own != NULL && strcmp(own->as.string, "from obj") == 0, "deep_prototype_chain");
    
    TaggedValue* mid = object_get_property(obj, "mid_prop");
    TEST_ASSERT(suite, mid != NULL && strcmp(mid->as.string, "from proto1") == 0, "deep_prototype_chain");
    
    TaggedValue* deep = object_get_property(obj, "deep_prop");
    TEST_ASSERT(suite, deep != NULL && strcmp(deep->as.string, "from proto2") == 0, "deep_prototype_chain");
    
    object_destroy(obj);
    object_destroy(proto1);
    object_destroy(proto2);
}

// Test nil and boolean values
DEFINE_TEST(nil_and_bool_properties)
{
    Object* obj = object_create();
    
    // Set nil property
    TaggedValue nil_val = {.type = VAL_NIL};
    object_set_property(obj, "nil_prop", nil_val);
    
    // Set boolean properties
    TaggedValue true_val = {.type = VAL_BOOL, .as.boolean = true};
    object_set_property(obj, "true_prop", true_val);
    
    TaggedValue false_val = {.type = VAL_BOOL, .as.boolean = false};
    object_set_property(obj, "false_prop", false_val);
    
    // Test retrieval
    TaggedValue* nil_result = object_get_property(obj, "nil_prop");
    TEST_ASSERT(suite, nil_result != NULL && nil_result->type == VAL_NIL, "nil_and_bool_properties");
    
    TaggedValue* true_result = object_get_property(obj, "true_prop");
    TEST_ASSERT(suite, true_result != NULL && true_result->type == VAL_BOOL, "nil_and_bool_properties");
    TEST_ASSERT(suite, true_result->as.boolean == true, "nil_and_bool_properties");
    
    TaggedValue* false_result = object_get_property(obj, "false_prop");
    TEST_ASSERT(suite, false_result != NULL && false_result->type == VAL_BOOL, "nil_and_bool_properties");
    TEST_ASSERT(suite, false_result->as.boolean == false, "nil_and_bool_properties");
    
    object_destroy(obj);
}

// Test has_property function
DEFINE_TEST(has_property_check)
{
    Object* proto = object_create();
    TaggedValue proto_val = {.type = VAL_NUMBER, .as.number = 1.0};
    object_set_property(proto, "inherited", proto_val);
    
    Object* obj = object_create_with_prototype(proto);
    TaggedValue obj_val = {.type = VAL_NUMBER, .as.number = 2.0};
    object_set_property(obj, "own", obj_val);
    
    // Test has_property
    TEST_ASSERT(suite, object_has_property(obj, "own") == true, "has_property_check");
    TEST_ASSERT(suite, object_has_property(obj, "inherited") == true, "has_property_check");
    TEST_ASSERT(suite, object_has_property(obj, "nonexistent") == false, "has_property_check");
    
    // Test has_own_property
    TEST_ASSERT(suite, object_has_own_property(obj, "own") == true, "has_property_check");
    TEST_ASSERT(suite, object_has_own_property(obj, "inherited") == false, "has_property_check");
    TEST_ASSERT(suite, object_has_own_property(obj, "nonexistent") == false, "has_property_check");
    
    object_destroy(obj);
    object_destroy(proto);
}

// Define test suite
TEST_SUITE(object_unit)
    TEST_CASE(create_destroy_object, "Create and Destroy Object")
    TEST_CASE(set_get_property, "Set and Get Property")
    TEST_CASE(overwrite_property, "Overwrite Property")
    TEST_CASE(multiple_properties, "Multiple Properties")
    TEST_CASE(object_with_prototype, "Object with Prototype")
    TEST_CASE(property_shadowing, "Property Shadowing")
    TEST_CASE(deep_prototype_chain, "Deep Prototype Chain")
    TEST_CASE(nil_and_bool_properties, "Nil and Bool Properties")
    TEST_CASE(has_property_check, "Has Property Check")
END_TEST_SUITE(object_unit)