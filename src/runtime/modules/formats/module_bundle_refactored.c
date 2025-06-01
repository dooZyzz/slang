#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

// Prevent miniz from defining compress macro
#define MINIZ_NO_ZLIB_COMPATIBLE_NAMES

#include "runtime/modules/formats/module_bundle.h"
#include "runtime/modules/formats/module_archive.h"
#include "runtime/modules/formats/module_format.h"
#include "runtime/modules/loader/module_loader.h"
#include "runtime/packages/package.h"
#include "runtime/core/vm.h"
#include "utils/hash_map.h"
#include "utils/allocators.h"
#include "runtime/modules/module_allocator_macros.h"
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <miniz.h>

/**
 * Module bundling implementation.
 * 
 * Bundle format:
 * - ZIP archive containing:
 *   - bundle.json: Bundle metadata
 *   - modules/: Directory with .swiftmodule files
 *   - resources/: Optional resource files
 *   - manifest.json: Module dependency manifest
 */

// Bundle builder implementation
typedef struct BundleBuilder {
    BundleOptions options;
    mz_zip_archive zip;
    HashMap* modules;           // module name -> module path
    HashMap* resources;         // resource bundle path -> local path
    BundleMetadata metadata;
    char* temp_path;           // Temporary directory for building
    bool initialized;
} BundleBuilder;

// Bundle reader implementation
typedef struct Bundle {
    char* path;
    mz_zip_archive zip;
    BundleMetadata metadata;
} Bundle;

// Helper: Create directory
static bool create_directory(const char* path) {
    return mkdir(path, 0755) == 0 || errno == EEXIST;
}

// Helper: Read file contents
static char* read_file(const char* path, size_t* size) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = MODULES_ALLOC(*size + 1);
    fread(content, 1, *size, file);
    content[*size] = '\0';
    
    fclose(file);
    return content;
}

// Helper: Simple JSON string extraction for bundle metadata
static char* json_get_string_from_bundle(const char* json, const char* key) {
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char* start = strstr(json, search_key);
    if (!start) return NULL;
    
    // Move past the key and colon
    start += strlen(search_key);
    
    // Skip whitespace
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') {
        start++;
    }
    
    // Must start with quote
    if (*start != '"') return NULL;
    start++;
    
    // Find end quote
    const char* end = start;
    while (*end && *end != '"') {
        if (*end == '\\' && *(end + 1)) {
            end += 2; // Skip escaped character
        } else {
            end++;
        }
    }
    
    if (*end != '"') return NULL;
    
    // Extract string
    size_t len = end - start;
    char* result = STRINGS_ALLOC(len + 1);
    strncpy(result, start, len);
    result[len] = '\0';
    
    return result;
}

// Create bundle builder
void* bundle_builder_create(const BundleOptions* options) {
    if (!options || !options->output_path) {
        return NULL;
    }
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    BundleBuilder* builder = MODULES_NEW_ZERO(BundleBuilder);
    builder->options = *options;
    builder->modules = hash_map_create();
    builder->resources = hash_map_create();
    
    // Initialize metadata
    builder->metadata.type = options->type;
    builder->metadata.created_at = time(NULL);
    builder->metadata.creator = STRINGS_STRDUP("SwiftLang Bundle Builder v1.0");
    builder->metadata.platform = STRINGS_STRDUP("universal");
    builder->metadata.min_version = STRINGS_STRDUP("1.0.0");
    
    if (options->entry_point) {
        builder->metadata.entry_point = STRINGS_STRDUP(options->entry_point);
    }
    
    // Create temporary directory
    char temp_template[] = "/tmp/swiftbundle.XXXXXX";
    builder->temp_path = mkdtemp(temp_template);
    if (!builder->temp_path) {
        bundle_builder_destroy(builder);
        return NULL;
    }
    builder->temp_path = STRINGS_STRDUP(builder->temp_path);
    
    // Initialize ZIP archive
    memset(&builder->zip, 0, sizeof(builder->zip));
    if (!mz_zip_writer_init_file(&builder->zip, options->output_path, 0)) {
        bundle_builder_destroy(builder);
        return NULL;
    }
    
    builder->initialized = true;
    return builder;
}

// Add module to bundle
bool bundle_builder_add_module(void* builder_ptr, const char* module_path, const char* bundle_path) {
    BundleBuilder* builder = (BundleBuilder*)builder_ptr;
    if (!builder || !builder->initialized || !module_path) {
        return false;
    }
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    // Determine bundle path
    const char* final_bundle_path = bundle_path;
    char* allocated_bundle_path = NULL;
    
    if (!final_bundle_path) {
        // Extract module name from path
        const char* module_name = strrchr(module_path, '/');
        module_name = module_name ? module_name + 1 : module_path;
        
        // Remove .swiftmodule extension if present
        allocated_bundle_path = STRINGS_STRDUP(module_name);
        char* ext = strstr(allocated_bundle_path, ".swiftmodule");
        if (ext) *ext = '\0';
        
        final_bundle_path = allocated_bundle_path;
    }
    
    // Check if module is already added
    if (hash_map_contains(builder->modules, final_bundle_path)) {
        if (allocated_bundle_path) {
            STRINGS_FREE(allocated_bundle_path, strlen(allocated_bundle_path) + 1);
        }
        return true; // Already added
    }
    
    // Read module file
    size_t module_size;
    char* module_data = read_file(module_path, &module_size);
    if (!module_data) {
        fprintf(stderr, "Failed to read module: %s\n", module_path);
        if (allocated_bundle_path) {
            STRINGS_FREE(allocated_bundle_path, strlen(allocated_bundle_path) + 1);
        }
        return false;
    }
    
    // Add to ZIP with path modules/name.swiftmodule
    char zip_path[256];
    snprintf(zip_path, sizeof(zip_path), "modules/%s.swiftmodule", final_bundle_path);
    
    mz_uint32 flags = MZ_ZIP_FLAG_COMPRESSED_DATA;
    if (builder->options.compress) {
        flags |= MZ_DEFAULT_COMPRESSION;
    }
    
    if (!mz_zip_writer_add_mem(&builder->zip, zip_path, module_data, module_size, flags)) {
        MODULES_FREE(module_data, module_size + 1);
        if (allocated_bundle_path) {
            STRINGS_FREE(allocated_bundle_path, strlen(allocated_bundle_path) + 1);
        }
        return false;
    }
    
    MODULES_FREE(module_data, module_size + 1);
    
    // Track module
    hash_map_put(builder->modules, final_bundle_path, STRINGS_STRDUP(module_path));
    
    // Extract version from module metadata if available
    char* module_version = STRINGS_STRDUP("1.0.0"); // default version
    
    // Try to load module metadata to get version
    ModuleMetadata* mod_metadata = package_load_module_metadata(module_path);
    if (mod_metadata && mod_metadata->version) {
        STRINGS_FREE(module_version, strlen(module_version) + 1);
        module_version = STRINGS_STRDUP(mod_metadata->version);
        package_free_module_metadata(mod_metadata);
    }
    
    // Update metadata
    size_t new_count = builder->metadata.modules.count + 1;
    size_t old_capacity = builder->metadata.modules.count;
    
    char** new_names = MODULES_NEW_ARRAY(char*, new_count);
    char** new_versions = MODULES_NEW_ARRAY(char*, new_count);
    
    if (builder->metadata.modules.names) {
        memcpy(new_names, builder->metadata.modules.names, old_capacity * sizeof(char*));
        MODULES_FREE(builder->metadata.modules.names, old_capacity * sizeof(char*));
    }
    if (builder->metadata.modules.versions) {
        memcpy(new_versions, builder->metadata.modules.versions, old_capacity * sizeof(char*));
        MODULES_FREE(builder->metadata.modules.versions, old_capacity * sizeof(char*));
    }
    
    builder->metadata.modules.names = new_names;
    builder->metadata.modules.versions = new_versions;
    
    builder->metadata.modules.names[builder->metadata.modules.count] = STRINGS_STRDUP(final_bundle_path);
    builder->metadata.modules.versions[builder->metadata.modules.count] = module_version;
    builder->metadata.modules.count = new_count;
    
    if (allocated_bundle_path) {
        STRINGS_FREE(allocated_bundle_path, strlen(allocated_bundle_path) + 1);
    }
    
    return true;
}

// Add module dependencies
int bundle_builder_add_dependencies(void* builder_ptr, const char* module_path, bool recursive) {
    BundleBuilder* builder = (BundleBuilder*)builder_ptr;
    if (!builder || !builder->initialized || !module_path) {
        return 0;
    }
    
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    int added = 0;
    
    // Load module metadata to find dependencies
    ModuleMetadata* metadata = package_load_module_metadata(module_path);
    if (!metadata) {
        return 0;
    }
    
    // Add each dependency
    for (size_t i = 0; i < metadata->dependency_count; i++) {
        const char* dep_name = metadata->dependencies[i].name;
        const char* dep_version = metadata->dependencies[i].version;
        
        // Resolve dependency path
        char* resolved_path = NULL;
        
        // Try standard module paths
        {
            char dep_path[256];
            // Try common locations
            const char* locations[] = {
                "modules/%s/build/%s.swiftmodule",
                "build/modules/%s.swiftmodule",
                "%s.swiftmodule",
                NULL
            };
            
            for (int j = 0; locations[j]; j++) {
                snprintf(dep_path, sizeof(dep_path), locations[j], dep_name, dep_name);
                if (access(dep_path, F_OK) == 0) {
                    resolved_path = STRINGS_STRDUP(dep_path);
                    break;
                }
            }
        }
        
        if (resolved_path) {
            if (bundle_builder_add_module(builder, resolved_path, dep_name)) {
                added++;
                
                // Recursively add transitive dependencies
                if (recursive) {
                    added += bundle_builder_add_dependencies(builder, resolved_path, true);
                }
            }
            STRINGS_FREE(resolved_path, strlen(resolved_path) + 1);
        } else {
            fprintf(stderr, "Warning: Could not resolve dependency '%s' version '%s'\n", 
                    dep_name, dep_version ? dep_version : "any");
        }
    }
    
    package_free_module_metadata(metadata);
    return added;
}

// Add resource to bundle
bool bundle_builder_add_resource(void* builder_ptr, const char* resource_path, const char* bundle_path) {
    BundleBuilder* builder = (BundleBuilder*)builder_ptr;
    if (!builder || !builder->initialized || !resource_path || !bundle_path) {
        return false;
    }
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    // Read resource file
    size_t resource_size;
    char* resource_data = read_file(resource_path, &resource_size);
    if (!resource_data) {
        return false;
    }
    
    // Add to ZIP with path resources/bundle_path
    char zip_path[256];
    snprintf(zip_path, sizeof(zip_path), "resources/%s", bundle_path);
    
    if (!mz_zip_writer_add_mem(&builder->zip, zip_path, resource_data, resource_size, 
                               builder->options.compress ? MZ_DEFAULT_COMPRESSION : 0)) {
        MODULES_FREE(resource_data, resource_size + 1);
        return false;
    }
    
    MODULES_FREE(resource_data, resource_size + 1);
    
    // Track resource
    hash_map_put(builder->resources, bundle_path, STRINGS_STRDUP(resource_path));
    
    return true;
}

// Set bundle metadata
bool bundle_builder_set_metadata(void* builder_ptr, const BundleMetadata* metadata) {
    BundleBuilder* builder = (BundleBuilder*)builder_ptr;
    if (!builder || !builder->initialized || !metadata) {
        return false;
    }
    
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    // Copy provided metadata fields
    if (metadata->name) {
        if (builder->metadata.name) {
            STRINGS_FREE(builder->metadata.name, strlen(builder->metadata.name) + 1);
        }
        builder->metadata.name = STRINGS_STRDUP(metadata->name);
    }
    if (metadata->version) {
        if (builder->metadata.version) {
            STRINGS_FREE(builder->metadata.version, strlen(builder->metadata.version) + 1);
        }
        builder->metadata.version = STRINGS_STRDUP(metadata->version);
    }
    if (metadata->description) {
        if (builder->metadata.description) {
            STRINGS_FREE(builder->metadata.description, strlen(builder->metadata.description) + 1);
        }
        builder->metadata.description = STRINGS_STRDUP(metadata->description);
    }
    
    return true;
}

// Build the bundle
bool bundle_builder_build(void* builder_ptr) {
    BundleBuilder* builder = (BundleBuilder*)builder_ptr;
    if (!builder || !builder->initialized) {
        return false;
    }
    
    // Create bundle.json metadata
    char metadata_json[4096];
    snprintf(metadata_json, sizeof(metadata_json),
        "{\n"
        "  \"format_version\": %d,\n"
        "  \"name\": \"%s\",\n"
        "  \"version\": \"%s\",\n"
        "  \"description\": \"%s\",\n"
        "  \"type\": \"%s\",\n"
        "  \"entry_point\": \"%s\",\n"
        "  \"created_at\": %ld,\n"
        "  \"creator\": \"%s\",\n"
        "  \"platform\": \"%s\",\n"
        "  \"min_version\": \"%s\",\n"
        "  \"module_count\": %zu\n"
        "}\n",
        BUNDLE_FORMAT_VERSION,
        builder->metadata.name ? builder->metadata.name : "unnamed",
        builder->metadata.version ? builder->metadata.version : "1.0.0",
        builder->metadata.description ? builder->metadata.description : "",
        builder->metadata.type == BUNDLE_TYPE_APPLICATION ? "application" : 
            (builder->metadata.type == BUNDLE_TYPE_LIBRARY ? "library" : "plugin"),
        builder->metadata.entry_point ? builder->metadata.entry_point : "",
        builder->metadata.created_at,
        builder->metadata.creator,
        builder->metadata.platform,
        builder->metadata.min_version,
        builder->metadata.modules.count
    );
    
    // Add metadata to bundle
    if (!mz_zip_writer_add_mem(&builder->zip, "bundle.json", metadata_json, 
                               strlen(metadata_json), MZ_DEFAULT_COMPRESSION)) {
        return false;
    }
    
    // Create manifest.json with detailed module info
    char manifest_json[8192];
    size_t offset = 0;
    offset += snprintf(manifest_json + offset, sizeof(manifest_json) - offset,
                      "{\n  \"modules\": [\n");
    
    for (size_t i = 0; i < builder->metadata.modules.count; i++) {
        if (i > 0) {
            offset += snprintf(manifest_json + offset, sizeof(manifest_json) - offset, ",\n");
        }
        offset += snprintf(manifest_json + offset, sizeof(manifest_json) - offset,
                          "    {\n"
                          "      \"name\": \"%s\",\n"
                          "      \"version\": \"%s\",\n"
                          "      \"path\": \"modules/%s.swiftmodule\"\n"
                          "    }",
                          builder->metadata.modules.names[i],
                          builder->metadata.modules.versions[i],
                          builder->metadata.modules.names[i]);
    }
    
    offset += snprintf(manifest_json + offset, sizeof(manifest_json) - offset,
                      "\n  ]\n}\n");
    
    // Add manifest to bundle
    if (!mz_zip_writer_add_mem(&builder->zip, "manifest.json", manifest_json,
                               strlen(manifest_json), MZ_DEFAULT_COMPRESSION)) {
        return false;
    }
    
    // Finalize ZIP
    if (!mz_zip_writer_finalize_archive(&builder->zip)) {
        return false;
    }
    
    return true;
}

// Destroy bundle builder
void bundle_builder_destroy(void* builder_ptr) {
    BundleBuilder* builder = (BundleBuilder*)builder_ptr;
    if (!builder) return;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    if (builder->initialized) {
        mz_zip_writer_end(&builder->zip);
    }
    
    // Clean up temporary directory
    if (builder->temp_path) {
        char cmd[256];
        snprintf(cmd, sizeof(cmd), "rm -rf %s", builder->temp_path);
        system(cmd);
        STRINGS_FREE(builder->temp_path, strlen(builder->temp_path) + 1);
    }
    
    // Free metadata
    if (builder->metadata.name) {
        STRINGS_FREE(builder->metadata.name, strlen(builder->metadata.name) + 1);
    }
    if (builder->metadata.version) {
        STRINGS_FREE(builder->metadata.version, strlen(builder->metadata.version) + 1);
    }
    if (builder->metadata.description) {
        STRINGS_FREE(builder->metadata.description, strlen(builder->metadata.description) + 1);
    }
    if (builder->metadata.entry_point) {
        STRINGS_FREE(builder->metadata.entry_point, strlen(builder->metadata.entry_point) + 1);
    }
    if (builder->metadata.creator) {
        STRINGS_FREE(builder->metadata.creator, strlen(builder->metadata.creator) + 1);
    }
    if (builder->metadata.platform) {
        STRINGS_FREE(builder->metadata.platform, strlen(builder->metadata.platform) + 1);
    }
    if (builder->metadata.min_version) {
        STRINGS_FREE(builder->metadata.min_version, strlen(builder->metadata.min_version) + 1);
    }
    
    for (size_t i = 0; i < builder->metadata.modules.count; i++) {
        STRINGS_FREE(builder->metadata.modules.names[i], strlen(builder->metadata.modules.names[i]) + 1);
        STRINGS_FREE(builder->metadata.modules.versions[i], strlen(builder->metadata.modules.versions[i]) + 1);
    }
    if (builder->metadata.modules.names) {
        MODULES_FREE(builder->metadata.modules.names, builder->metadata.modules.count * sizeof(char*));
    }
    if (builder->metadata.modules.versions) {
        MODULES_FREE(builder->metadata.modules.versions, builder->metadata.modules.count * sizeof(char*));
    }
    
    // Free hash maps
    hash_map_destroy(builder->modules);
    hash_map_destroy(builder->resources);
    
    MODULES_FREE(builder, sizeof(BundleBuilder));
}

// Open bundle for reading
void* bundle_open(const char* bundle_path) {
    if (!bundle_path) return NULL;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    Bundle* bundle = MODULES_NEW_ZERO(Bundle);
    bundle->path = STRINGS_STRDUP(bundle_path);
    
    // Open ZIP archive
    memset(&bundle->zip, 0, sizeof(bundle->zip));
    if (!mz_zip_reader_init_file(&bundle->zip, bundle_path, 0)) {
        bundle_close(bundle);
        return NULL;
    }
    
    // Read bundle.json metadata
    size_t metadata_size;
    char* metadata_json = mz_zip_reader_extract_file_to_heap(&bundle->zip, "bundle.json", 
                                                             &metadata_size, 0);
    if (!metadata_json) {
        bundle_close(bundle);
        return NULL;
    }
    
    // Parse metadata from JSON
    bundle->metadata.name = json_get_string_from_bundle(metadata_json, "name");
    if (!bundle->metadata.name) {
        bundle->metadata.name = STRINGS_STRDUP("unnamed_bundle");
    }
    
    bundle->metadata.version = json_get_string_from_bundle(metadata_json, "version");
    if (!bundle->metadata.version) {
        bundle->metadata.version = STRINGS_STRDUP("1.0.0");
    }
    
    // Parse bundle type
    char* type_str = json_get_string_from_bundle(metadata_json, "type");
    if (type_str) {
        if (strcmp(type_str, "application") == 0) {
            bundle->metadata.type = BUNDLE_TYPE_APPLICATION;
        } else if (strcmp(type_str, "library") == 0) {
            bundle->metadata.type = BUNDLE_TYPE_LIBRARY;
        } else {
            bundle->metadata.type = BUNDLE_TYPE_APPLICATION; // default
        }
        STRINGS_FREE(type_str, strlen(type_str) + 1);
    } else {
        bundle->metadata.type = BUNDLE_TYPE_APPLICATION;
    }
    
    // Parse entry point
    bundle->metadata.entry_point = json_get_string_from_bundle(metadata_json, "entry_point");
    if (!bundle->metadata.entry_point) {
        bundle->metadata.entry_point = STRINGS_STRDUP("main");
    }
    
    // Parse main module
    bundle->metadata.main_module = json_get_string_from_bundle(metadata_json, "main_module");
    
    mz_free(metadata_json);
    return bundle;
}

// Get bundle metadata
const BundleMetadata* bundle_get_metadata(void* bundle_ptr) {
    Bundle* bundle = (Bundle*)bundle_ptr;
    return bundle ? &bundle->metadata : NULL;
}

// Load module from bundle
Module* bundle_load_module(void* bundle_ptr, const char* module_name, ModuleLoader* loader) {
    Bundle* bundle = (Bundle*)bundle_ptr;
    if (!bundle || !module_name || !loader) {
        return NULL;
    }
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    
    // Module caching is handled by ModuleLoader
    // Check if already loaded
    Module* existing = module_get_cached(loader, module_name);
    if (existing) {
        return existing;
    }
    
    // Extract module to temporary file
    char module_path[256];
    snprintf(module_path, sizeof(module_path), "modules/%s.swiftmodule", module_name);
    
    size_t module_size;
    void* module_data = mz_zip_reader_extract_file_to_heap(&bundle->zip, module_path,
                                                           &module_size, 0);
    if (!module_data) {
        return NULL;
    }
    
    // Write to temporary file
    char temp_path[256];
    snprintf(temp_path, sizeof(temp_path), "/tmp/bundle_module_%s.swiftmodule", module_name);
    
    FILE* temp_file = fopen(temp_path, "wb");
    if (!temp_file) {
        mz_free(module_data);
        return NULL;
    }
    
    fwrite(module_data, 1, module_size, temp_file);
    fclose(temp_file);
    mz_free(module_data);
    
    // Load module from temporary file
    Module* module = module_load(loader, temp_path, false);
    // Module will be cached by the loader
    
    // Clean up temporary file
    unlink(temp_path);
    
    return module;
}

// List modules in bundle
char** bundle_list_modules(void* bundle_ptr, size_t* count) {
    Bundle* bundle = (Bundle*)bundle_ptr;
    if (!bundle || !count) {
        return NULL;
    }
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    // Count modules by looking for modules/ prefix
    *count = 0;
    mz_uint num_files = mz_zip_reader_get_num_files(&bundle->zip);
    
    for (mz_uint i = 0; i < num_files; i++) {
        char filename[256];
        mz_zip_reader_get_filename(&bundle->zip, i, filename, sizeof(filename));
        
        if (strncmp(filename, "modules/", 8) == 0 && strstr(filename, ".swiftmodule")) {
            (*count)++;
        }
    }
    
    if (*count == 0) {
        return NULL;
    }
    
    // Allocate array
    char** modules = MODULES_NEW_ARRAY(char*, *count);
    size_t idx = 0;
    
    // Extract module names
    for (mz_uint i = 0; i < num_files && idx < *count; i++) {
        char filename[256];
        mz_zip_reader_get_filename(&bundle->zip, i, filename, sizeof(filename));
        
        if (strncmp(filename, "modules/", 8) == 0 && strstr(filename, ".swiftmodule")) {
            // Extract module name from path
            char* name_start = filename + 8; // Skip "modules/"
            char* ext = strstr(name_start, ".swiftmodule");
            if (ext) {
                size_t name_len = ext - name_start;
                modules[idx] = STRINGS_ALLOC(name_len + 1);
                strncpy(modules[idx], name_start, name_len);
                modules[idx][name_len] = '\0';
                idx++;
            }
        }
    }
    
    return modules;
}

// Close bundle
void bundle_close(void* bundle_ptr) {
    Bundle* bundle = (Bundle*)bundle_ptr;
    if (!bundle) return;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    mz_zip_reader_end(&bundle->zip);
    
    // Module cache is managed by ModuleLoader
    
    if (bundle->path) {
        STRINGS_FREE(bundle->path, strlen(bundle->path) + 1);
    }
    if (bundle->metadata.name) {
        STRINGS_FREE(bundle->metadata.name, strlen(bundle->metadata.name) + 1);
    }
    if (bundle->metadata.version) {
        STRINGS_FREE(bundle->metadata.version, strlen(bundle->metadata.version) + 1);
    }
    if (bundle->metadata.description) {
        STRINGS_FREE(bundle->metadata.description, strlen(bundle->metadata.description) + 1);
    }
    if (bundle->metadata.entry_point) {
        STRINGS_FREE(bundle->metadata.entry_point, strlen(bundle->metadata.entry_point) + 1);
    }
    if (bundle->metadata.main_module) {
        STRINGS_FREE(bundle->metadata.main_module, strlen(bundle->metadata.main_module) + 1);
    }
    if (bundle->metadata.creator) {
        STRINGS_FREE(bundle->metadata.creator, strlen(bundle->metadata.creator) + 1);
    }
    if (bundle->metadata.platform) {
        STRINGS_FREE(bundle->metadata.platform, strlen(bundle->metadata.platform) + 1);
    }
    if (bundle->metadata.min_version) {
        STRINGS_FREE(bundle->metadata.min_version, strlen(bundle->metadata.min_version) + 1);
    }
    
    MODULES_FREE(bundle, sizeof(Bundle));
}

// Execute bundle
int bundle_execute(const char* bundle_path, int argc, char** argv) {
    // Open bundle
    void* bundle = bundle_open(bundle_path);
    if (!bundle) {
        fprintf(stderr, "Failed to open bundle: %s\n", bundle_path);
        return 1;
    }
    
    const BundleMetadata* metadata = bundle_get_metadata(bundle);
    if (!metadata->entry_point) {
        fprintf(stderr, "Bundle has no entry point\n");
        bundle_close(bundle);
        return 1;
    }
    
    // Create VM and module loader
    VM vm;
    vm_init(&vm);
    
    // Load and execute entry point
    Module* entry_module = bundle_load_module(bundle, metadata->entry_point, vm.module_loader);
    if (!entry_module) {
        fprintf(stderr, "Failed to load entry point: %s\n", metadata->entry_point);
        bundle_close(bundle);
        vm_free(&vm);
        return 1;
    }
    
    // Ensure module is initialized
    if (!ensure_module_initialized(entry_module, &vm)) {
        fprintf(stderr, "Failed to initialize entry module: %s\n", metadata->entry_point);
        bundle_close(bundle);
        vm_free(&vm);
        return 1;
    }
    
    // Look for main function in module exports
    TaggedValue main_func = NIL_VAL;
    bool found_main = false;
    
    // Check for exported main function
    for (size_t i = 0; i < entry_module->exports.count; i++) {
        if (strcmp(entry_module->exports.names[i], "main") == 0) {
            main_func = entry_module->exports.values[i];
            found_main = true;
            break;
        }
    }
    
    // If not found in exports, check module object properties
    if (!found_main && entry_module->module_object) {
        TaggedValue* main_ptr = object_get_property(entry_module->module_object, "main");
        if (main_ptr) {
            main_func = *main_ptr;
            found_main = true;
        }
    }
    
    int exit_code = 0;
    
    if (found_main && (IS_FUNCTION(main_func) || IS_CLOSURE(main_func))) {
        // Prepare arguments (argc/argv if needed)
        TaggedValue* args = NULL;
        int arg_count = 0;
        
        // If the bundle supports command-line args, we could pass them here
        // For now, just call with no arguments
        
        // Call the main function
        TaggedValue result = vm_call_value(&vm, main_func, arg_count, args);
        
        // Check if the call succeeded
        if (IS_NIL(result) && !IS_NATIVE(main_func)) {
            // vm_call_value returns NIL on error for non-native functions
            // For now, we'll assume an error occurred
            fprintf(stderr, "Error calling main function\n");
            exit_code = 70;
        } else if (IS_NUMBER(result)) {
            // If main returns a number, use it as exit code
            exit_code = (int)AS_NUMBER(result);
        }
    } else if (found_main) {
        fprintf(stderr, "Error: 'main' export is not a function\n");
        exit_code = 1;
    } else {
        // No main function found - module executed successfully
        // This is OK for library modules
        if (metadata->type == BUNDLE_TYPE_APPLICATION) {
            fprintf(stderr, "Warning: No 'main' function found in application bundle\n");
        }
    }
    
    bundle_close(bundle);
    vm_free(&vm);
    return exit_code;
}

// Verify bundle integrity
bool bundle_verify(const char* bundle_path) {
    void* bundle = bundle_open(bundle_path);
    if (!bundle) {
        return false;
    }
    
    // TODO: Implement signature verification
    // For now, just check if we can read metadata
    const BundleMetadata* metadata = bundle_get_metadata(bundle);
    bool valid = metadata != NULL;
    
    bundle_close(bundle);
    return valid;
}

// Get bundle info as JSON
char* bundle_info_json(const char* bundle_path) {
    void* bundle = bundle_open(bundle_path);
    if (!bundle) {
        Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
        return STRINGS_STRDUP("{\"error\": \"Failed to open bundle\"}");
    }
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_MODULES);
    Allocator* str_alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
    
    const BundleMetadata* metadata = bundle_get_metadata(bundle);
    
    // List modules
    size_t module_count;
    char** modules = bundle_list_modules(bundle, &module_count);
    
    // Build JSON
    size_t json_size = 4096;
    char* json = MODULES_ALLOC(json_size);
    size_t offset = 0;
    
    offset += snprintf(json + offset, json_size - offset,
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"version\": \"%s\",\n"
        "  \"type\": \"%s\",\n"
        "  \"entry_point\": \"%s\",\n"
        "  \"modules\": [",
        metadata->name ? metadata->name : "unknown",
        metadata->version ? metadata->version : "unknown",
        metadata->type == BUNDLE_TYPE_APPLICATION ? "application" : "library",
        metadata->entry_point ? metadata->entry_point : "none"
    );
    
    for (size_t i = 0; i < module_count; i++) {
        if (i > 0) {
            offset += snprintf(json + offset, json_size - offset, ", ");
        }
        offset += snprintf(json + offset, json_size - offset, "\"%s\"", modules[i]);
        STRINGS_FREE(modules[i], strlen(modules[i]) + 1);
    }
    if (modules) {
        MODULES_FREE(modules, module_count * sizeof(char*));
    }
    
    offset += snprintf(json + offset, json_size - offset, "]\n}\n");
    
    bundle_close(bundle);
    
    // Transfer ownership to string allocator
    char* result = STRINGS_STRDUP(json);
    MODULES_FREE(json, json_size);
    return result;
}