#define MODULE_BUNDLE_EXPORTS
#include "runtime/modules/module_bundle.h"
#include "utils/logger.h"
#include "utils/memory.h"
#include "utils/platform_compat.h"
#include "utils/hash_map.h"
#include "utils/allocators.h"
#include "runtime/core/vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

// Global cache for bundles
static HashMap* g_bundle_cache = NULL;
static ModuleBundleError g_last_error = MODULE_BUNDLE_OK;

// Platform-specific library extensions
#ifdef _WIN32
    #define DYLIB_EXT ".dll"
#elif defined(__APPLE__)
    #define DYLIB_EXT ".dylib"  
#else
    #define DYLIB_EXT ".so"
#endif

// Include module allocator macros
#include "runtime/modules/module_allocator_macros.h"

// Cache initialization
static void ensure_cache_initialized(void) {
    if (!g_bundle_cache) {
        g_bundle_cache = hash_map_create();
    }
}

// Error handling
ModuleBundleError module_bundle_get_last_error(void) {
    return g_last_error;
}

const char* module_bundle_error_string(ModuleBundleError error) {
    switch (error) {
        case MODULE_BUNDLE_OK: return "Success";
        case MODULE_BUNDLE_ERROR_INVALID_ARGS: return "Invalid arguments";
        case MODULE_BUNDLE_ERROR_FILE_NOT_FOUND: return "File not found";
        case MODULE_BUNDLE_ERROR_INVALID_FORMAT: return "Invalid format";
        case MODULE_BUNDLE_ERROR_COMPRESSION_FAILED: return "Compression failed";
        case MODULE_BUNDLE_ERROR_EXTRACTION_FAILED: return "Extraction failed";
        case MODULE_BUNDLE_ERROR_DEPENDENCY_NOT_FOUND: return "Dependency not found";
        case MODULE_BUNDLE_ERROR_VERSION_CONFLICT: return "Version conflict";
        case MODULE_BUNDLE_ERROR_SIGNATURE_INVALID: return "Invalid signature";
        case MODULE_BUNDLE_ERROR_PERMISSION_DENIED: return "Permission denied";
        case MODULE_BUNDLE_ERROR_OUT_OF_MEMORY: return "Out of memory";
        default: return "Unknown error";
    }
}

// Platform detection
const char* module_bundle_get_platform_name(void) {
#ifdef _WIN32
    return "windows";
#elif defined(__APPLE__)
    return "darwin";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

// Version comparison utilities
bool module_bundle_version_satisfies(const char* version, const char* requirement) {
    if (!version || !requirement) return false;
    
    // Simple implementation - can be enhanced with proper semver
    if (strcmp(requirement, "*") == 0) return true;
    if (strncmp(requirement, "^", 1) == 0) {
        // Caret range - compatible within major version
        // For now, just do string comparison
        return strcmp(version, requirement + 1) >= 0;
    }
    if (strncmp(requirement, ">=", 2) == 0) {
        return strcmp(version, requirement + 2) >= 0;
    }
    if (strncmp(requirement, "<=", 2) == 0) {
        return strcmp(version, requirement + 2) <= 0;
    }
    if (strncmp(requirement, ">", 1) == 0) {
        return strcmp(version, requirement + 1) > 0;
    }
    if (strncmp(requirement, "<", 1) == 0) {
        return strcmp(version, requirement + 1) < 0;
    }
    
    // Exact match
    return strcmp(version, requirement) == 0;
}

// Path resolution
char* module_bundle_resolve_path(const char* base_path, const char* relative_path) {
    if (!base_path || !relative_path) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return NULL;
    }
    
    size_t base_len = strlen(base_path);
    size_t rel_len = strlen(relative_path);
    size_t total_len = base_len + rel_len + 2; // +1 for '/', +1 for null terminator
    
    char* resolved = MODULES_ALLOC(total_len);
    if (!resolved) {
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return NULL;
    }
    
    snprintf(resolved, total_len, "%s/%s", base_path, relative_path);
    return resolved;
}

// Metadata management
static ModuleBundleMetadata* create_metadata(const char* name, const char* version) {
    ModuleBundleMetadata* metadata = MODULES_NEW_ZERO(ModuleBundleMetadata);
    if (!metadata) {
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return NULL;
    }
    
    metadata->name = STRINGS_STRDUP(name);
    metadata->version = STRINGS_STRDUP(version);
    metadata->created_time = time(NULL);
    metadata->modified_time = metadata->created_time;
    
    return metadata;
}

static void destroy_metadata(ModuleBundleMetadata* metadata) {
    if (!metadata) return;
    
    // Free string fields
    if (metadata->name) STRINGS_FREE(metadata->name, strlen(metadata->name) + 1);
    if (metadata->version) STRINGS_FREE(metadata->version, strlen(metadata->version) + 1);
    if (metadata->description) STRINGS_FREE(metadata->description, strlen(metadata->description) + 1);
    if (metadata->main_file) STRINGS_FREE(metadata->main_file, strlen(metadata->main_file) + 1);
    if (metadata->author) STRINGS_FREE(metadata->author, strlen(metadata->author) + 1);
    if (metadata->license) STRINGS_FREE(metadata->license, strlen(metadata->license) + 1);
    if (metadata->min_engine_version) STRINGS_FREE(metadata->min_engine_version, strlen(metadata->min_engine_version) + 1);
    
    // Free arrays
    for (size_t i = 0; i < metadata->keyword_count; i++) {
        if (metadata->keywords[i]) STRINGS_FREE(metadata->keywords[i], strlen(metadata->keywords[i]) + 1);
    }
    if (metadata->keywords) MODULES_FREE(metadata->keywords, metadata->keyword_count * sizeof(char*));
    
    // Free dependencies
    for (size_t i = 0; i < metadata->dependency_count; i++) {
        ModuleBundleDependency* dep = &metadata->dependencies[i];
        if (dep->name) STRINGS_FREE(dep->name, strlen(dep->name) + 1);
        if (dep->version_requirement) STRINGS_FREE(dep->version_requirement, strlen(dep->version_requirement) + 1);
        if (dep->resolved_version) STRINGS_FREE(dep->resolved_version, strlen(dep->resolved_version) + 1);
        if (dep->resolved_path) STRINGS_FREE(dep->resolved_path, strlen(dep->resolved_path) + 1);
    }
    if (metadata->dependencies) MODULES_FREE(metadata->dependencies, metadata->dependency_count * sizeof(ModuleBundleDependency));
    
    // Free dev dependencies
    for (size_t i = 0; i < metadata->dev_dependency_count; i++) {
        ModuleBundleDependency* dep = &metadata->dev_dependencies[i];
        if (dep->name) STRINGS_FREE(dep->name, strlen(dep->name) + 1);
        if (dep->version_requirement) STRINGS_FREE(dep->version_requirement, strlen(dep->version_requirement) + 1);
        if (dep->resolved_version) STRINGS_FREE(dep->resolved_version, strlen(dep->resolved_version) + 1);
        if (dep->resolved_path) STRINGS_FREE(dep->resolved_path, strlen(dep->resolved_path) + 1);
    }
    if (metadata->dev_dependencies) MODULES_FREE(metadata->dev_dependencies, metadata->dev_dependency_count * sizeof(ModuleBundleDependency));
    
    // Free exports
    for (size_t i = 0; i < metadata->export_count; i++) {
        ModuleBundleExport* exp = &metadata->exports[i];
        if (exp->name) STRINGS_FREE(exp->name, strlen(exp->name) + 1);
        if (exp->export_path) STRINGS_FREE(exp->export_path, strlen(exp->export_path) + 1);
        if (exp->export_type) STRINGS_FREE(exp->export_type, strlen(exp->export_type) + 1);
    }
    if (metadata->exports) MODULES_FREE(metadata->exports, metadata->export_count * sizeof(ModuleBundleExport));
    
    // Free native info
    ModuleBundleNative* native = &metadata->native;
    if (native->library_name) STRINGS_FREE(native->library_name, strlen(native->library_name) + 1);
    if (native->darwin_lib) STRINGS_FREE(native->darwin_lib, strlen(native->darwin_lib) + 1);
    if (native->linux_lib) STRINGS_FREE(native->linux_lib, strlen(native->linux_lib) + 1);
    if (native->windows_lib) STRINGS_FREE(native->windows_lib, strlen(native->windows_lib) + 1);
    if (native->loaded_path) STRINGS_FREE(native->loaded_path, strlen(native->loaded_path) + 1);
    
    for (size_t i = 0; i < native->source_count; i++) {
        if (native->source_files[i]) STRINGS_FREE(native->source_files[i], strlen(native->source_files[i]) + 1);
    }
    if (native->source_files) MODULES_FREE(native->source_files, native->source_count * sizeof(char*));
    
    for (size_t i = 0; i < native->header_count; i++) {
        if (native->header_files[i]) STRINGS_FREE(native->header_files[i], strlen(native->header_files[i]) + 1);
    }
    if (native->header_files) MODULES_FREE(native->header_files, native->header_count * sizeof(char*));
    
    // Free scripts and patterns
    for (size_t i = 0; i < metadata->script_count; i++) {
        if (metadata->build_scripts[i]) STRINGS_FREE(metadata->build_scripts[i], strlen(metadata->build_scripts[i]) + 1);
        if (metadata->script_commands[i]) STRINGS_FREE(metadata->script_commands[i], strlen(metadata->script_commands[i]) + 1);
    }
    if (metadata->build_scripts) MODULES_FREE(metadata->build_scripts, metadata->script_count * sizeof(char*));
    if (metadata->script_commands) MODULES_FREE(metadata->script_commands, metadata->script_count * sizeof(char*));
    
    for (size_t i = 0; i < metadata->file_pattern_count; i++) {
        if (metadata->file_patterns[i]) STRINGS_FREE(metadata->file_patterns[i], strlen(metadata->file_patterns[i]) + 1);
    }
    if (metadata->file_patterns) MODULES_FREE(metadata->file_patterns, metadata->file_pattern_count * sizeof(char*));
    
    MODULES_FREE(metadata, sizeof(ModuleBundleMetadata));
}

// Bundle creation and lifecycle
ModuleBundle* module_bundle_create(const char* name, const char* version) {
    if (!name || !version) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return NULL;
    }
    
    LOG_DEBUG(LOG_MODULE_MODULE_LOADER, "Creating module bundle: %s@%s", name, version);
    
    ModuleBundle* bundle = MODULES_NEW_ZERO(ModuleBundle);
    if (!bundle) {
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return NULL;
    }
    
    bundle->metadata = create_metadata(name, version);
    if (!bundle->metadata) {
        MODULES_FREE(bundle, sizeof(ModuleBundle));
        return NULL;
    }
    
    bundle->ref_count = 1;
    bundle->last_access = time(NULL);
    
    // Initialize pending bytecode storage
    bundle->pending_bytecode.module_names = NULL;
    bundle->pending_bytecode.bytecode_data = NULL;
    bundle->pending_bytecode.bytecode_sizes = NULL;
    bundle->pending_bytecode.count = 0;
    bundle->pending_bytecode.capacity = 0;
    
    return bundle;
}

ModuleBundle* module_bundle_load(const char* path) {
    if (!path) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return NULL;
    }
    
    LOG_DEBUG(LOG_MODULE_MODULE_LOADER, "Loading module bundle from: %s", path);
    
    // Check if file exists
    struct stat st;
    if (stat(path, &st) != 0) {
        g_last_error = MODULE_BUNDLE_ERROR_FILE_NOT_FOUND;
        return NULL;
    }
    
    // Read file into memory
    FILE* file = fopen(path, "rb");
    if (!file) {
        g_last_error = MODULE_BUNDLE_ERROR_FILE_NOT_FOUND;
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    uint8_t* data = MODULES_ALLOC(file_size);
    if (!data) {
        fclose(file);
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return NULL;
    }
    
    size_t read_size = fread(data, 1, file_size, file);
    fclose(file);
    
    if (read_size != file_size) {
        MODULES_FREE(data, file_size);
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_FORMAT;
        return NULL;
    }
    
    ModuleBundle* bundle = module_bundle_load_from_memory(data, file_size);
    if (bundle) {
        bundle->archive_path = STRINGS_STRDUP(path);
        bundle->owns_archive_data = true;
    } else {
        MODULES_FREE(data, file_size);
    }
    
    return bundle;
}

ModuleBundle* module_bundle_load_from_memory(const uint8_t* data, size_t size) {
    if (!data || size == 0) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return NULL;
    }
    
    ModuleBundle* bundle = MODULES_NEW_ZERO(ModuleBundle);
    if (!bundle) {
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return NULL;
    }
    
    // Initialize ZIP archive
    bundle->zip_archive = MODULES_NEW_ZERO(mz_zip_archive);
    if (!bundle->zip_archive) {
        MODULES_FREE(bundle, sizeof(ModuleBundle));
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return NULL;
    }
    
    // Initialize miniz with memory buffer
    if (!mz_zip_reader_init_mem(bundle->zip_archive, data, size, 0)) {
        MODULES_FREE(bundle->zip_archive, sizeof(mz_zip_archive));
        MODULES_FREE(bundle, sizeof(ModuleBundle));
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_FORMAT;
        return NULL;
    }
    
    bundle->archive_data = (uint8_t*)data;
    bundle->archive_size = size;
    bundle->ref_count = 1;
    bundle->last_access = time(NULL);
    
    // Extract and parse manifest.json
    size_t manifest_size;
    void* manifest_data = mz_zip_reader_extract_file_to_heap(bundle->zip_archive, "manifest.json", &manifest_size, 0);
    if (!manifest_data) {
        mz_zip_reader_end(bundle->zip_archive);
        MODULES_FREE(bundle->zip_archive, sizeof(mz_zip_archive));
        MODULES_FREE(bundle, sizeof(ModuleBundle));
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_FORMAT;
        return NULL;
    }
    
    // Parse manifest JSON (simplified parser)
    bundle->metadata = MODULES_NEW_ZERO(ModuleBundleMetadata);
    if (!bundle->metadata) {
        mz_free(manifest_data);
        mz_zip_reader_end(bundle->zip_archive);
        MODULES_FREE(bundle->zip_archive, sizeof(mz_zip_archive));
        MODULES_FREE(bundle, sizeof(ModuleBundle));
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return NULL;
    }
    
    // Simple JSON parsing - extract name and version at minimum
    char* manifest_str = (char*)manifest_data;
    manifest_str[manifest_size] = '\0'; // Ensure null termination
    
    // Extract name
    char* name_start = strstr(manifest_str, "\"name\"");
    if (name_start) {
        name_start = strchr(name_start, ':');
        if (name_start) {
            name_start = strchr(name_start, '"');
            if (name_start) {
                name_start++; // Skip opening quote
                char* name_end = strchr(name_start, '"');
                if (name_end) {
                    size_t name_len = name_end - name_start;
                    bundle->metadata->name = STRINGS_ALLOC(name_len + 1);
                    strncpy(bundle->metadata->name, name_start, name_len);
                    bundle->metadata->name[name_len] = '\0';
                }
            }
        }
    }
    
    // Extract version
    char* version_start = strstr(manifest_str, "\"version\"");
    if (version_start) {
        version_start = strchr(version_start, ':');
        if (version_start) {
            version_start = strchr(version_start, '"');
            if (version_start) {
                version_start++; // Skip opening quote
                char* version_end = strchr(version_start, '"');
                if (version_end) {
                    size_t version_len = version_end - version_start;
                    bundle->metadata->version = STRINGS_ALLOC(version_len + 1);
                    strncpy(bundle->metadata->version, version_start, version_len);
                    bundle->metadata->version[version_len] = '\0';
                }
            }
        }
    }
    
    mz_free(manifest_data);
    
    // Set defaults if parsing failed
    if (!bundle->metadata->name) {
        bundle->metadata->name = STRINGS_STRDUP("unknown");
    }
    if (!bundle->metadata->version) {
        bundle->metadata->version = STRINGS_STRDUP("0.0.0");
    }
    
    bundle->metadata->created_time = time(NULL);
    bundle->metadata->modified_time = bundle->metadata->created_time;
    
    LOG_DEBUG(LOG_MODULE_MODULE_LOADER, "Loaded bundle: %s@%s", 
              bundle->metadata->name, bundle->metadata->version);
    
    return bundle;
}

bool module_bundle_save(ModuleBundle* bundle, const char* path) {
    if (!bundle || !path) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return false;
    }
    
    LOG_DEBUG(LOG_MODULE_MODULE_LOADER, "Saving module bundle to: %s", path);
    
    // Create new ZIP archive for writing
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    
    if (!mz_zip_writer_init_file(&zip, path, 0)) {
        g_last_error = MODULE_BUNDLE_ERROR_COMPRESSION_FAILED;
        return false;
    }
    
    // Generate manifest JSON
    char* manifest_json = module_bundle_get_manifest_json(bundle);
    if (!manifest_json) {
        mz_zip_writer_end(&zip);
        return false;
    }
    
    // Add manifest to archive
    if (!mz_zip_writer_add_mem(&zip, "manifest.json", manifest_json, strlen(manifest_json), MZ_DEFAULT_COMPRESSION)) {
        STRINGS_FREE(manifest_json, strlen(manifest_json) + 1);
        mz_zip_writer_end(&zip);
        g_last_error = MODULE_BUNDLE_ERROR_COMPRESSION_FAILED;
        return false;
    }
    
    STRINGS_FREE(manifest_json, strlen(manifest_json) + 1);
    
    // Copy existing files if this is an existing bundle
    if (bundle->zip_archive) {
        int num_files = mz_zip_reader_get_num_files(bundle->zip_archive);
        for (int i = 0; i < num_files; i++) {
            mz_zip_archive_file_stat file_stat;
            if (!mz_zip_reader_file_stat(bundle->zip_archive, i, &file_stat)) continue;
            
            // Skip manifest.json as we already added the updated version
            if (strcmp(file_stat.m_filename, "manifest.json") == 0) continue;
            
            // Extract and add to new archive
            size_t file_size;
            void* file_data = mz_zip_reader_extract_to_heap(bundle->zip_archive, file_stat.m_filename, &file_size, 0);
            if (file_data) {
                mz_zip_writer_add_mem(&zip, file_stat.m_filename, file_data, file_size, MZ_DEFAULT_COMPRESSION);
                mz_free(file_data);
            }
        }
    }
    
    // Add pending bytecode files
    for (size_t i = 0; i < bundle->pending_bytecode.count; i++) {
        char bytecode_path[512];
        snprintf(bytecode_path, sizeof(bytecode_path), "bytecode/%s.swiftbc", 
                bundle->pending_bytecode.module_names[i]);
        
        if (!mz_zip_writer_add_mem(&zip, bytecode_path, 
                                  bundle->pending_bytecode.bytecode_data[i],
                                  bundle->pending_bytecode.bytecode_sizes[i], 
                                  MZ_DEFAULT_COMPRESSION)) {
            mz_zip_writer_end(&zip);
            g_last_error = MODULE_BUNDLE_ERROR_COMPRESSION_FAILED;
            return false;
        }
    }
    
    // Finalize archive
    if (!mz_zip_writer_finalize_archive(&zip)) {
        mz_zip_writer_end(&zip);
        g_last_error = MODULE_BUNDLE_ERROR_COMPRESSION_FAILED;
        return false;
    }
    
    mz_zip_writer_end(&zip);
    
    // Update bundle path
    if (bundle->archive_path) {
        STRINGS_FREE(bundle->archive_path, strlen(bundle->archive_path) + 1);
    }
    bundle->archive_path = STRINGS_STRDUP(path);
    bundle->is_dirty = false;
    
    return true;
}

void module_bundle_destroy(ModuleBundle* bundle) {
    if (!bundle) return;
    
    LOG_DEBUG(LOG_MODULE_MODULE_LOADER, "Destroying module bundle: %s@%s", 
              bundle->metadata ? bundle->metadata->name : "unknown",
              bundle->metadata ? bundle->metadata->version : "unknown");
    
    // Clean up ZIP archive
    if (bundle->zip_archive) {
        mz_zip_reader_end(bundle->zip_archive);
        MODULES_FREE(bundle->zip_archive, sizeof(mz_zip_archive));
    }
    
    // Clean up archive data
    if (bundle->owns_archive_data && bundle->archive_data) {
        MODULES_FREE(bundle->archive_data, bundle->archive_size);
    }
    
    // Clean up pending bytecode
    for (size_t i = 0; i < bundle->pending_bytecode.count; i++) {
        if (bundle->pending_bytecode.module_names[i]) {
            STRINGS_FREE(bundle->pending_bytecode.module_names[i], 
                        strlen(bundle->pending_bytecode.module_names[i]) + 1);
        }
        if (bundle->pending_bytecode.bytecode_data[i]) {
            MODULES_FREE(bundle->pending_bytecode.bytecode_data[i], 
                        bundle->pending_bytecode.bytecode_sizes[i]);
        }
    }
    if (bundle->pending_bytecode.module_names) {
        MODULES_FREE(bundle->pending_bytecode.module_names, 
                    bundle->pending_bytecode.capacity * sizeof(char*));
    }
    if (bundle->pending_bytecode.bytecode_data) {
        MODULES_FREE(bundle->pending_bytecode.bytecode_data, 
                    bundle->pending_bytecode.capacity * sizeof(uint8_t*));
    }
    if (bundle->pending_bytecode.bytecode_sizes) {
        MODULES_FREE(bundle->pending_bytecode.bytecode_sizes, 
                    bundle->pending_bytecode.capacity * sizeof(size_t));
    }
    
    // Clean up metadata
    destroy_metadata(bundle->metadata);
    
    // Clean up paths
    if (bundle->archive_path) STRINGS_FREE(bundle->archive_path, strlen(bundle->archive_path) + 1);
    if (bundle->cache_path) STRINGS_FREE(bundle->cache_path, strlen(bundle->cache_path) + 1);
    if (bundle->install_path) STRINGS_FREE(bundle->install_path, strlen(bundle->install_path) + 1);
    if (bundle->signature) STRINGS_FREE(bundle->signature, strlen(bundle->signature) + 1);
    
    MODULES_FREE(bundle, sizeof(ModuleBundle));
}

// Reference counting
ModuleBundle* module_bundle_retain(ModuleBundle* bundle) {
    if (!bundle) return NULL;
    
    bundle->ref_count++;
    bundle->last_access = time(NULL);
    return bundle;
}

void module_bundle_release(ModuleBundle* bundle) {
    if (!bundle) return;
    
    bundle->ref_count--;
    if (bundle->ref_count <= 0) {
        module_bundle_destroy(bundle);
    }
}

// Metadata operations
bool module_bundle_set_metadata(ModuleBundle* bundle, const char* key, const char* value) {
    if (!bundle || !key || !value) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return false;
    }
    
    ModuleBundleMetadata* meta = bundle->metadata;
    
    if (strcmp(key, "description") == 0) {
        if (meta->description) STRINGS_FREE(meta->description, strlen(meta->description) + 1);
        meta->description = STRINGS_STRDUP(value);
    } else if (strcmp(key, "main") == 0) {
        if (meta->main_file) STRINGS_FREE(meta->main_file, strlen(meta->main_file) + 1);
        meta->main_file = STRINGS_STRDUP(value);
    } else if (strcmp(key, "author") == 0) {
        if (meta->author) STRINGS_FREE(meta->author, strlen(meta->author) + 1);
        meta->author = STRINGS_STRDUP(value);
    } else if (strcmp(key, "license") == 0) {
        if (meta->license) STRINGS_FREE(meta->license, strlen(meta->license) + 1);
        meta->license = STRINGS_STRDUP(value);
    } else {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return false;
    }
    
    meta->modified_time = time(NULL);
    bundle->is_dirty = true;
    return true;
}

const char* module_bundle_get_metadata(ModuleBundle* bundle, const char* key) {
    if (!bundle || !key) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return NULL;
    }
    
    ModuleBundleMetadata* meta = bundle->metadata;
    
    if (strcmp(key, "name") == 0) return meta->name;
    if (strcmp(key, "version") == 0) return meta->version;
    if (strcmp(key, "description") == 0) return meta->description;
    if (strcmp(key, "main") == 0) return meta->main_file;
    if (strcmp(key, "author") == 0) return meta->author;
    if (strcmp(key, "license") == 0) return meta->license;
    
    return NULL;
}

// Generate manifest JSON
char* module_bundle_get_manifest_json(ModuleBundle* bundle) {
    if (!bundle || !bundle->metadata) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return NULL;
    }
    
    ModuleBundleMetadata* meta = bundle->metadata;
    
    // Estimate size and allocate buffer
    size_t buffer_size = 4096; // Start with 4KB
    char* json = STRINGS_ALLOC(buffer_size);
    if (!json) {
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return NULL;
    }
    
    // Build JSON (simplified - a real implementation would use a JSON library)
    int written = snprintf(json, buffer_size,
        "{\n"
        "  \"name\": \"%s\",\n"
        "  \"version\": \"%s\",\n"
        "  \"description\": \"%s\",\n"
        "  \"main\": \"%s\",\n"
        "  \"author\": \"%s\",\n"
        "  \"license\": \"%s\",\n"
        "  \"type\": \"module\",\n"
        "  \"dependencies\": {},\n"
        "  \"exports\": {},\n"
        "  \"created\": %ld,\n"
        "  \"modified\": %ld\n"
        "}",
        meta->name ? meta->name : "",
        meta->version ? meta->version : "0.0.0",
        meta->description ? meta->description : "",
        meta->main_file ? meta->main_file : "main.swift",
        meta->author ? meta->author : "",
        meta->license ? meta->license : "",
        (long long)meta->created_time,
        (long long)meta->modified_time
    );
    
    if (written >= (int)buffer_size) {
        // Buffer too small, reallocate
        STRINGS_FREE(json, buffer_size);
        buffer_size = written + 1;
        json = STRINGS_ALLOC(buffer_size);
        if (!json) {
            g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
            return NULL;
        }
        
        snprintf(json, buffer_size,
            "{\n"
            "  \"name\": \"%s\",\n"
            "  \"version\": \"%s\",\n"
            "  \"description\": \"%s\",\n"
            "  \"main\": \"%s\",\n"
            "  \"author\": \"%s\",\n"
            "  \"license\": \"%s\",\n"
            "  \"type\": \"module\",\n"
            "  \"dependencies\": {},\n"
            "  \"exports\": {},\n"
            "  \"created\": %ld,\n"
            "  \"modified\": %ld\n"
            "}",
            meta->name ? meta->name : "",
            meta->version ? meta->version : "0.0.0",
            meta->description ? meta->description : "",
            meta->main_file ? meta->main_file : "main.swift",
            meta->author ? meta->author : "",
            meta->license ? meta->license : "",
            (long long)meta->created_time,
            (long long)meta->modified_time
        );
    }
    
    return json;
}

// File operations
bool module_bundle_add_file(ModuleBundle* bundle, const char* local_path, const char* archive_path) {
    if (!bundle || !local_path || !archive_path) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return false;
    }
    
    // Read local file
    FILE* file = fopen(local_path, "rb");
    if (!file) {
        g_last_error = MODULE_BUNDLE_ERROR_FILE_NOT_FOUND;
        return false;
    }
    
    fseek(file, 0, SEEK_END);
    size_t file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    uint8_t* data = MODULES_ALLOC(file_size);
    if (!data) {
        fclose(file);
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return false;
    }
    
    size_t read_size = fread(data, 1, file_size, file);
    fclose(file);
    
    if (read_size != file_size) {
        MODULES_FREE(data, file_size);
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_FORMAT;
        return false;
    }
    
    // Add to archive (requires rebuilding - simplified for now)
    bundle->is_dirty = true;
    
    MODULES_FREE(data, file_size);
    return true;
}

bool module_bundle_add_bytecode(ModuleBundle* bundle, const char* module_name, const uint8_t* bytecode, size_t size) {
    if (!bundle || !module_name || !bytecode || size == 0) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return false;
    }
    
    // Ensure capacity for pending bytecode
    if (bundle->pending_bytecode.count >= bundle->pending_bytecode.capacity) {
        size_t old_capacity = bundle->pending_bytecode.capacity;
        size_t new_capacity = old_capacity == 0 ? 4 : old_capacity * 2;
        
        // Allocate new arrays
        char** new_names = MODULES_NEW_ARRAY(char*, new_capacity);
        uint8_t** new_data = MODULES_NEW_ARRAY(uint8_t*, new_capacity);
        size_t* new_sizes = MODULES_NEW_ARRAY(size_t, new_capacity);
        
        if (!new_names || !new_data || !new_sizes) {
            if (new_names) MODULES_FREE(new_names, new_capacity * sizeof(char*));
            if (new_data) MODULES_FREE(new_data, new_capacity * sizeof(uint8_t*));
            if (new_sizes) MODULES_FREE(new_sizes, new_capacity * sizeof(size_t));
            g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
            return false;
        }
        
        // Copy existing data
        if (bundle->pending_bytecode.module_names) {
            memcpy(new_names, bundle->pending_bytecode.module_names, old_capacity * sizeof(char*));
            memcpy(new_data, bundle->pending_bytecode.bytecode_data, old_capacity * sizeof(uint8_t*));
            memcpy(new_sizes, bundle->pending_bytecode.bytecode_sizes, old_capacity * sizeof(size_t));
            
            // Free old arrays
            MODULES_FREE(bundle->pending_bytecode.module_names, old_capacity * sizeof(char*));
            MODULES_FREE(bundle->pending_bytecode.bytecode_data, old_capacity * sizeof(uint8_t*));
            MODULES_FREE(bundle->pending_bytecode.bytecode_sizes, old_capacity * sizeof(size_t));
        }
        
        bundle->pending_bytecode.module_names = new_names;
        bundle->pending_bytecode.bytecode_data = new_data;
        bundle->pending_bytecode.bytecode_sizes = new_sizes;
        bundle->pending_bytecode.capacity = new_capacity;
    }
    
    // Copy module name
    bundle->pending_bytecode.module_names[bundle->pending_bytecode.count] = STRINGS_STRDUP(module_name);
    if (!bundle->pending_bytecode.module_names[bundle->pending_bytecode.count]) {
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return false;
    }
    
    // Copy bytecode data
    bundle->pending_bytecode.bytecode_data[bundle->pending_bytecode.count] = MODULES_ALLOC(size);
    if (!bundle->pending_bytecode.bytecode_data[bundle->pending_bytecode.count]) {
        STRINGS_FREE(bundle->pending_bytecode.module_names[bundle->pending_bytecode.count], strlen(module_name) + 1);
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return false;
    }
    
    memcpy(bundle->pending_bytecode.bytecode_data[bundle->pending_bytecode.count], bytecode, size);
    bundle->pending_bytecode.bytecode_sizes[bundle->pending_bytecode.count] = size;
    
    bundle->pending_bytecode.count++;
    bundle->is_dirty = true;
    
    LOG_DEBUG(LOG_MODULE_MODULE_LOADER, "Added bytecode for module %s (%zu bytes)", module_name, size);
    return true;
}

// Cache management
bool module_bundle_cache_put(ModuleBundle* bundle) {
    if (!bundle || !bundle->metadata) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return false;
    }
    
    ensure_cache_initialized();
    
    // Create cache key
    char key[512];
    snprintf(key, sizeof(key), "%s@%s", bundle->metadata->name, bundle->metadata->version);
    
    // Store in cache
    hash_map_put(g_bundle_cache, key, module_bundle_retain(bundle));
    
    LOG_DEBUG(LOG_MODULE_MODULE_LOADER, "Cached bundle: %s", key);
    return true;
}

ModuleBundle* module_bundle_cache_get(const char* name, const char* version) {
    if (!name || !version) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return NULL;
    }
    
    ensure_cache_initialized();
    
    char key[512];
    snprintf(key, sizeof(key), "%s@%s", name, version);
    
    ModuleBundle* bundle = (ModuleBundle*)hash_map_get(g_bundle_cache, key);
    if (bundle) {
        bundle->last_access = time(NULL);
        return module_bundle_retain(bundle);
    }
    
    return NULL;
}

bool module_bundle_cache_remove(const char* name, const char* version) {
    if (!name || !version) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return false;
    }
    
    ensure_cache_initialized();
    
    char key[512];
    snprintf(key, sizeof(key), "%s@%s", name, version);
    
    ModuleBundle* bundle = (ModuleBundle*)hash_map_get(g_bundle_cache, key);
    if (bundle) {
        hash_map_remove(g_bundle_cache, key);
        module_bundle_release(bundle);
        return true;
    }
    
    return false;
}

// Callback function for releasing bundles during cache clear
static void release_bundle_callback(const char* key, void* value, void* user_data) {
    (void)key;       // Unused parameter
    (void)user_data; // Unused parameter
    ModuleBundle* bundle = (ModuleBundle*)value;
    module_bundle_release(bundle);
}

void module_bundle_cache_clear(void) {
    if (g_bundle_cache) {
        // Release all cached bundles using the new iteration API
        hash_map_iterate(g_bundle_cache, release_bundle_callback, NULL);
        hash_map_clear(g_bundle_cache);
    }
}

size_t module_bundle_cache_size(void) {
    if (!g_bundle_cache) return 0;
    return hash_map_size(g_bundle_cache);
}

// Basic validation
bool module_bundle_validate(ModuleBundle* bundle) {
    if (!bundle || !bundle->metadata) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return false;
    }
    
    // Check required fields
    if (!bundle->metadata->name || strlen(bundle->metadata->name) == 0) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_FORMAT;
        return false;
    }
    
    if (!bundle->metadata->version || strlen(bundle->metadata->version) == 0) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_FORMAT;
        return false;
    }
    
    return true;
}

// Stub implementations for remaining functions
bool module_bundle_add_dependency(ModuleBundle* bundle, const char* name, const char* version_req, bool is_dev) {
    // TODO: Implement dependency management
    (void)bundle; (void)name; (void)version_req; (void)is_dev;
    return true;
}

bool module_bundle_remove_dependency(ModuleBundle* bundle, const char* name) {
    // TODO: Implement dependency management
    (void)bundle; (void)name;
    return true;
}

bool module_bundle_add_native_library(ModuleBundle* bundle, const char* platform, const char* lib_path) {
    // TODO: Implement native library support
    (void)bundle; (void)platform; (void)lib_path;
    return true;
}

bool module_bundle_extract_file(ModuleBundle* bundle, const char* archive_path, const char* output_path) {
    // TODO: Implement file extraction
    (void)bundle; (void)archive_path; (void)output_path;
    return true;
}

uint8_t* module_bundle_get_bytecode(ModuleBundle* bundle, const char* module_name, size_t* size) {
    if (!bundle || !module_name || !size) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_ARGS;
        return NULL;
    }
    
    if (!bundle->zip_archive) {
        g_last_error = MODULE_BUNDLE_ERROR_INVALID_FORMAT;
        return NULL;
    }
    
    // Build bytecode path - bytecode files are stored in bytecode/ directory with .swiftbc extension
    char bytecode_path[512];
    snprintf(bytecode_path, sizeof(bytecode_path), "bytecode/%s.swiftbc", module_name);
    
    // Extract from ZIP archive
    size_t extracted_size;
    void* data = mz_zip_reader_extract_file_to_heap(bundle->zip_archive, bytecode_path, &extracted_size, 0);
    if (!data) {
        g_last_error = MODULE_BUNDLE_ERROR_EXTRACTION_FAILED;
        return NULL;
    }
    
    // Allocate our own buffer and copy data
    uint8_t* bytecode = MODULES_ALLOC(extracted_size);
    if (!bytecode) {
        mz_free(data);
        g_last_error = MODULE_BUNDLE_ERROR_OUT_OF_MEMORY;
        return NULL;
    }
    
    memcpy(bytecode, data, extracted_size);
    mz_free(data);
    
    *size = extracted_size;
    return bytecode;
}

char** module_bundle_list_files(ModuleBundle* bundle, size_t* count) {
    // TODO: Implement file listing
    (void)bundle; (void)count;
    return NULL;
}

char** module_bundle_list_modules(ModuleBundle* bundle, size_t* count) {
    // TODO: Implement module listing
    (void)bundle; (void)count;
    return NULL;
}

bool module_bundle_install(ModuleBundle* bundle, const char* install_dir) {
    // TODO: Implement installation
    (void)bundle; (void)install_dir;
    return true;
}

bool module_bundle_uninstall(const char* name, const char* install_dir) {
    // TODO: Implement uninstallation
    (void)name; (void)install_dir;
    return true;
}

bool module_bundle_is_installed(const char* name, const char* install_dir) {
    // TODO: Implement installation check
    (void)name; (void)install_dir;
    return false;
}

char* module_bundle_get_install_path(const char* name, const char* install_dir) {
    // TODO: Implement install path resolution
    (void)name; (void)install_dir;
    return NULL;
}

// Dependency resolver stubs
typedef struct ModuleBundleResolver {
    char** source_paths;
    size_t source_count;
} ModuleBundleResolver;

ModuleBundleResolver* module_bundle_resolver_create(void) {
    ModuleBundleResolver* resolver = MODULES_NEW_ZERO(ModuleBundleResolver);
    return resolver;
}

void module_bundle_resolver_destroy(ModuleBundleResolver* resolver) {
    if (!resolver) return;
    
    for (size_t i = 0; i < resolver->source_count; i++) {
        if (resolver->source_paths[i]) {
            STRINGS_FREE(resolver->source_paths[i], strlen(resolver->source_paths[i]) + 1);
        }
    }
    if (resolver->source_paths) {
        MODULES_FREE(resolver->source_paths, resolver->source_count * sizeof(char*));
    }
    
    MODULES_FREE(resolver, sizeof(ModuleBundleResolver));
}

bool module_bundle_resolver_add_source(ModuleBundleResolver* resolver, const char* source_path) {
    // TODO: Implement source management
    (void)resolver; (void)source_path;
    return true;
}

ModuleBundle** module_bundle_resolve_dependencies(ModuleBundleResolver* resolver, ModuleBundle* bundle, size_t* count) {
    // TODO: Implement dependency resolution
    (void)resolver; (void)bundle; (void)count;
    return NULL;
}

// Security stubs
bool module_bundle_verify_signature(ModuleBundle* bundle) {
    (void)bundle;
    return true;
}

bool module_bundle_sign(ModuleBundle* bundle, const char* private_key_path) {
    (void)bundle; (void)private_key_path;
    return true;
}