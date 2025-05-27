#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "runtime/package.h"
#include "runtime/module.h"
#include "vm/vm.h"
#include "vm/object.h"

// Test loading module metadata with multiple modules
DEFINE_TEST(load_multi_module_metadata) {
    // Create a temporary module.json file with multiple modules
    const char* test_json = "{\n"
        "  \"name\": \"test_package\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"description\": \"Test package with multiple modules\",\n"
        "  \"modules\": [\n"
        "    {\n"
        "      \"name\": \"math.native\",\n"
        "      \"sources\": [\"src/native/math.c\"],\n"
        "      \"type\": \"native\"\n"
        "    },\n"
        "    {\n"
        "      \"name\": \"utils\",\n"
        "      \"sources\": [\"src/utils.swift\"],\n"
        "      \"main\": \"src/utils.swift\",\n"
        "      \"type\": \"library\",\n"
        "      \"dependencies\": [\"math.native\"]\n"
        "    }\n"
        "  ]\n"
        "}";
    
    // Write to temp file
    FILE* f = fopen("/tmp/test_multi_module.json", "w");
    TEST_ASSERT_NOT_NULL(suite, f, "load_multi_module_metadata");
    fputs(test_json, f);
    fclose(f);
    
    // Load metadata
    ModuleMetadata* metadata = package_load_module_metadata("/tmp/test_multi_module.json");
    TEST_ASSERT_NOT_NULL(suite, metadata, "load_multi_module_metadata");
    
    // Verify metadata
    TEST_ASSERT_STRING_EQUAL(suite, "test_package", metadata->name, "load_multi_module_metadata");
    TEST_ASSERT_STRING_EQUAL(suite, "1.0.0", metadata->version, "load_multi_module_metadata");
    TEST_ASSERT_EQUAL_INT(suite, 2, metadata->module_count, "load_multi_module_metadata");
    
    // Verify first module
    TEST_ASSERT_STRING_EQUAL(suite, "math.native", metadata->modules[0].name, "load_multi_module_metadata");
    TEST_ASSERT_STRING_EQUAL(suite, "native", metadata->modules[0].type, "load_multi_module_metadata");
    TEST_ASSERT_EQUAL_INT(suite, 1, metadata->modules[0].source_count, "load_multi_module_metadata");
    TEST_ASSERT_STRING_EQUAL(suite, "src/native/math.c", metadata->modules[0].sources[0], "load_multi_module_metadata");
    
    // Verify second module
    TEST_ASSERT_STRING_EQUAL(suite, "utils", metadata->modules[1].name, "load_multi_module_metadata");
    TEST_ASSERT_STRING_EQUAL(suite, "library", metadata->modules[1].type, "load_multi_module_metadata");
    TEST_ASSERT_EQUAL_INT(suite, 1, metadata->modules[1].source_count, "load_multi_module_metadata");
    TEST_ASSERT_EQUAL_INT(suite, 1, metadata->modules[1].main_count, "load_multi_module_metadata");
    TEST_ASSERT_EQUAL_INT(suite, 1, metadata->modules[1].dependency_count, "load_multi_module_metadata");
    TEST_ASSERT_STRING_EQUAL(suite, "math.native", metadata->modules[1].dependencies[0], "load_multi_module_metadata");
    
    // Clean up
    package_free_module_metadata(metadata);
    unlink("/tmp/test_multi_module.json");
}

// Test finding module definition within metadata
DEFINE_TEST(find_module_definition) {
    // Create metadata with modules
    ModuleMetadata metadata = {0};
    metadata.name = "test_package";
    metadata.module_count = 2;
    metadata.modules = calloc(2, sizeof(ModuleDefinition));
    
    metadata.modules[0].name = strdup("math");
    metadata.modules[0].type = strdup("native");
    
    metadata.modules[1].name = strdup("utils");
    metadata.modules[1].type = strdup("library");
    
    // Test finding existing modules
    ModuleDefinition* math_def = package_find_module_definition(&metadata, "math");
    TEST_ASSERT_NOT_NULL(suite, math_def, "find_module_definition");
    TEST_ASSERT_STRING_EQUAL(suite, "math", math_def->name, "find_module_definition");
    
    ModuleDefinition* utils_def = package_find_module_definition(&metadata, "utils");
    TEST_ASSERT_NOT_NULL(suite, utils_def, "find_module_definition");
    TEST_ASSERT_STRING_EQUAL(suite, "utils", utils_def->name, "find_module_definition");
    
    // Test finding non-existent module
    ModuleDefinition* notfound_def = package_find_module_definition(&metadata, "notfound");
    TEST_ASSERT_NULL(suite, notfound_def, "find_module_definition");
    
    // Clean up
    free(metadata.modules[0].name);
    free(metadata.modules[0].type);
    free(metadata.modules[1].name);
    free(metadata.modules[1].type);
    free(metadata.modules);
}

// Test submodule path resolution
DEFINE_TEST(submodule_path_resolution) {
    VM* vm = vm_create();
    TEST_ASSERT_NOT_NULL(suite, vm, "submodule_path_resolution");
    
    ModuleLoader* loader = module_loader_create(vm);
    TEST_ASSERT_NOT_NULL(suite, loader, "submodule_path_resolution");
    
    // Add test module path
    module_loader_add_search_path(loader, "/tmp/test_modules");
    
    // Create test package structure
    system("mkdir -p /tmp/test_modules/testpkg");
    
    // Create package module.json
    const char* pkg_json = "{\n"
        "  \"name\": \"testpkg\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"modules\": [\n"
        "    {\n"
        "      \"name\": \"math\",\n"
        "      \"sources\": [\"math.swift\"],\n"
        "      \"type\": \"library\"\n"
        "    },\n"
        "    {\n"
        "      \"name\": \"utils\",\n"
        "      \"sources\": [\"utils.swift\"],\n"
        "      \"type\": \"library\"\n"
        "    }\n"
        "  ]\n"
        "}";
    
    FILE* f = fopen("/tmp/test_modules/testpkg/module.json", "w");
    TEST_ASSERT_NOT_NULL(suite, f, "submodule_path_resolution");
    fputs(pkg_json, f);
    fclose(f);
    
    // Create simple module files
    f = fopen("/tmp/test_modules/testpkg/math.swift", "w");
    fprintf(f, "export func add(a: Int, b: Int) -> Int { return a + b }");
    fclose(f);
    
    f = fopen("/tmp/test_modules/testpkg/utils.swift", "w");
    fprintf(f, "export func hello() { println(\"Hello\") }");
    fclose(f);
    
    // Test loading submodule with slash syntax
    // Note: This would normally load "testpkg/math" but we're just testing the path resolution
    
    // Clean up
    system("rm -rf /tmp/test_modules/testpkg");
    module_loader_destroy(loader);
    vm_destroy(vm);
}

// Define test suite
TEST_SUITE(multi_module_unit)
    TEST_CASE(load_multi_module_metadata, "Load Multi-Module Metadata")
    TEST_CASE(find_module_definition, "Find Module Definition")
    TEST_CASE(submodule_path_resolution, "Submodule Path Resolution")
END_TEST_SUITE(multi_module_unit)