#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "runtime/packages/package.h"
#include "runtime/modules/loader/module_loader.h"
#include "runtime/modules/formats/module_format.h"
#include "runtime/core/vm.h"
#include "runtime/core/object.h"

// Test module metadata loading
DEFINE_TEST(load_module_metadata) {
    // Create a temporary module.json file
    const char* test_json = "{\n"
        "  \"name\": \"test.module\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"description\": \"Test module\",\n"
        "  \"type\": \"library\",\n"
        "  \"exports\": {\n"
        "    \"testFunc\": {\n"
        "      \"type\": \"function\",\n"
        "      \"signature\": \"() -> Int\"\n"
        "    },\n"
        "    \"PI\": {\n"
        "      \"type\": \"constant\",\n"
        "      \"value\": 3.14159\n"
        "    }\n"
        "  }\n"
        "}";
    
    // Write to temp file
    FILE* f = fopen("/tmp/test_module.json", "w");
    TEST_ASSERT_NOT_NULL(suite, f, "load_module_metadata");
    fputs(test_json, f);
    fclose(f);
    
    // Load metadata
    ModuleMetadata* metadata = package_load_module_metadata("/tmp/test_module.json");
    TEST_ASSERT_NOT_NULL(suite, metadata, "load_module_metadata");
    
    // Verify metadata
    TEST_ASSERT_STRING_EQUAL(suite, "test.module", metadata->name, "load_module_metadata");
    TEST_ASSERT_STRING_EQUAL(suite, "1.0.0", metadata->version, "load_module_metadata");
    TEST_ASSERT_STRING_EQUAL(suite, "library", metadata->type, "load_module_metadata");
    TEST_ASSERT_EQUAL_INT(suite, 2, metadata->export_count, "load_module_metadata");
    
    // Verify exports
    TEST_ASSERT_STRING_EQUAL(suite, "testFunc", metadata->exports[0].name, "load_module_metadata");
    TEST_ASSERT_EQUAL_INT(suite, MODULE_EXPORT_FUNCTION, metadata->exports[0].type, "load_module_metadata");
    
    TEST_ASSERT_STRING_EQUAL(suite, "PI", metadata->exports[1].name, "load_module_metadata");
    TEST_ASSERT_EQUAL_INT(suite, MODULE_EXPORT_CONSTANT, metadata->exports[1].type, "load_module_metadata");
    TEST_ASSERT_EQUAL_DOUBLE(suite, 3.14159, AS_NUMBER(metadata->exports[1].constant_value), 0.00001, "load_module_metadata");
    
    // Clean up
    package_free_module_metadata(metadata);
    unlink("/tmp/test_module.json");
}

// Test package system initialization
DEFINE_TEST(package_system_create) {
    VM* vm = vm_create();
    TEST_ASSERT_NOT_NULL(suite, vm, "package_system_create");
    
    PackageSystem* pkg_sys = package_system_create(vm);
    TEST_ASSERT_NOT_NULL(suite, pkg_sys, "package_system_create");
    
    // Clean up
    package_system_destroy(pkg_sys);
    vm_destroy(vm);
}

// Test module resolution
DEFINE_TEST(module_resolution) {
    // Create test directory structure
    system("mkdir -p /tmp/test_modules/sys/math");
    
    // Create module.json for math module
    const char* math_json = "{\n"
        "  \"name\": \"sys.math\",\n"
        "  \"version\": \"0.1.0\",\n"
        "  \"type\": \"library\",\n"
        "  \"exports\": {\n"
        "    \"sin\": {\n"
        "      \"type\": \"function\",\n"
        "      \"native\": \"math_sin\",\n"
        "      \"signature\": \"(Double) -> Double\"\n"
        "    }\n"
        "  }\n"
        "}";
    
    FILE* f = fopen("/tmp/test_modules/sys/math/module.json", "w");
    TEST_ASSERT_NOT_NULL(suite, f, "module_resolution");
    fputs(math_json, f);
    fclose(f);
    
    // Create root module.json
    const char* root_json = "{\n"
        "  \"name\": \"test_app\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"type\": \"application\",\n"
        "  \"dependencies\": {\n"
        "    \"sys.math\": \"file:/tmp/test_modules/sys/math\"\n"
        "  },\n"
        "  \"paths\": {\n"
        "    \"modules\": [\"/tmp/test_modules\"]\n"
        "  }\n"
        "}";
    
    f = fopen("/tmp/test_root_module.json", "w");
    TEST_ASSERT_NOT_NULL(suite, f, "module_resolution");
    fputs(root_json, f);
    fclose(f);
    
    // Test resolution
    VM* vm = vm_create();
    PackageSystem* pkg_sys = package_system_create(vm);
    
    bool loaded = package_system_load_root(pkg_sys, "/tmp/test_root_module.json");
    TEST_ASSERT_TRUE(suite, loaded, "module_resolution");
    
    char* resolved = package_resolve_module_path(pkg_sys, "sys.math");
    TEST_ASSERT_NOT_NULL(suite, resolved, "module_resolution");
    TEST_ASSERT_STRING_EQUAL(suite, "/tmp/test_modules/sys/math", resolved, "module_resolution");
    
    // Clean up
    free(resolved);
    package_system_destroy(pkg_sys);
    vm_destroy(vm);
    
    // Remove test files
    system("rm -rf /tmp/test_modules");
    unlink("/tmp/test_root_module.json");
}

// Test module format writer/reader
DEFINE_TEST(module_format) {
    const char* test_path = "/tmp/test.swiftmodule";
    
    // Write module
    ModuleWriter* writer = module_writer_create(test_path);
    TEST_ASSERT_NOT_NULL(suite, writer, "module_format");
    
    module_writer_add_metadata(writer, "test.module", "1.0.0");
    module_writer_add_export(writer, "testFunc", MODULE_EXPORT_FUNCTION, 0, "(Int) -> Int");
    
    uint8_t test_bytecode[] = {0x01, 0x02, 0x03, 0x04};
    module_writer_add_bytecode(writer, test_bytecode, sizeof(test_bytecode));
    
    bool finalized = module_writer_finalize(writer);
    TEST_ASSERT_TRUE(suite, finalized, "module_format");
    module_writer_destroy(writer);
    
    // Read module
    ModuleReader* reader = module_reader_create(test_path);
    TEST_ASSERT_NOT_NULL(suite, reader, "module_format");
    
    bool verified = module_reader_verify(reader);
    TEST_ASSERT_TRUE(suite, verified, "module_format");
    
    const char* name = module_reader_get_name(reader);
    TEST_ASSERT_STRING_EQUAL(suite, "test.module", name, "module_format");
    
    const char* version = module_reader_get_version(reader);
    TEST_ASSERT_STRING_EQUAL(suite, "1.0.0", version, "module_format");
    
    size_t export_count = module_reader_get_export_count(reader);
    TEST_ASSERT_EQUAL_INT(suite, 1, export_count, "module_format");
    
    ExportEntry* export = module_reader_get_export(reader, 0);
    TEST_ASSERT_NOT_NULL(suite, export, "module_format");
    TEST_ASSERT_STRING_EQUAL(suite, "testFunc", export->name, "module_format");
    TEST_ASSERT_EQUAL_INT(suite, MODULE_EXPORT_FUNCTION, export->type, "module_format");
    
    size_t bytecode_size;
    const uint8_t* bytecode = module_reader_get_bytecode(reader, &bytecode_size);
    TEST_ASSERT_NOT_NULL(suite, (void*)bytecode, "module_format");
    TEST_ASSERT_EQUAL_INT(suite, sizeof(test_bytecode), bytecode_size, "module_format");
    TEST_ASSERT_EQUAL_INT(suite, 0, memcmp(bytecode, test_bytecode, bytecode_size), "module_format");
    
    module_reader_destroy(reader);
    
    // Clean up
    unlink(test_path);
}

// Test module loader with package system
DEFINE_TEST(module_loader_integration) {
    // Create test module structure
    system("mkdir -p /tmp/test_stdlib/math");
    
    const char* math_json = "{\n"
        "  \"name\": \"sys.math\",\n"
        "  \"version\": \"0.1.0\",\n"
        "  \"type\": \"library\",\n"
        "  \"exports\": {\n"
        "    \"PI\": {\n"
        "      \"type\": \"constant\",\n"
        "      \"value\": 3.141592653589793\n"
        "    },\n"
        "    \"E\": {\n"
        "      \"type\": \"constant\",\n"
        "      \"value\": 2.718281828459045\n"
        "    }\n"
        "  }\n"
        "}";
    
    FILE* f = fopen("/tmp/test_stdlib/math/module.json", "w");
    TEST_ASSERT_NOT_NULL(suite, f, "module_loader_integration");
    fputs(math_json, f);
    fclose(f);
    
    // Create root module.json
    const char* root_json = "{\n"
        "  \"name\": \"test_app\",\n"
        "  \"version\": \"1.0.0\",\n"
        "  \"type\": \"application\",\n"
        "  \"dependencies\": {\n"
        "    \"sys.math\": \"file:/tmp/test_stdlib/math\"\n"
        "  }\n"
        "}";
    
    f = fopen("/tmp/test_app_module.json", "w");
    TEST_ASSERT_NOT_NULL(suite, f, "module_loader_integration");
    fputs(root_json, f);
    fclose(f);
    
    // Test loading
    VM* vm = vm_create();
    ModuleLoader* loader = module_loader_create(vm);
    TEST_ASSERT_NOT_NULL(suite, loader, "module_loader_integration");
    
    // Load root configuration
    bool loaded = package_system_load_root(loader->package_system, "/tmp/test_app_module.json");
    TEST_ASSERT_TRUE(suite, loaded, "module_loader_integration");
    
    // Load module
    Module* module = module_load(loader, "sys.math", false);
    TEST_ASSERT_NOT_NULL(suite, module, "module_loader_integration");
    TEST_ASSERT_EQUAL_INT(suite, MODULE_STATE_LOADED, module->state, "module_loader_integration");
    
    // Check exports
    TaggedValue pi = module_get_export(module, "PI");
    TEST_ASSERT_TRUE(suite, IS_NUMBER(pi), "module_loader_integration");
    TEST_ASSERT_EQUAL_DOUBLE(suite, 3.141592653589793, AS_NUMBER(pi), 0.00000001, "module_loader_integration");
    
    TaggedValue e = module_get_export(module, "E");
    TEST_ASSERT_TRUE(suite, IS_NUMBER(e), "module_loader_integration");
    TEST_ASSERT_EQUAL_DOUBLE(suite, 2.718281828459045, AS_NUMBER(e), 0.00000001, "module_loader_integration");
    
    // Clean up
    module_loader_destroy(loader);
    vm_destroy(vm);
    
    system("rm -rf /tmp/test_stdlib");
    unlink("/tmp/test_app_module.json");
}

// Define test suite
TEST_SUITE(module_system_unit)
    TEST_CASE(load_module_metadata, "Load Module Metadata")
    TEST_CASE(package_system_create, "Package System Create")
    TEST_CASE(module_resolution, "Module Resolution")
    TEST_CASE(module_format, "Module Format")
    TEST_CASE(module_loader_integration, "Module Loader Integration")
END_TEST_SUITE(module_system_unit)