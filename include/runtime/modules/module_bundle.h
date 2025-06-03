#ifndef MODULE_BUNDLE_H
#define MODULE_BUNDLE_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "miniz.h"
#include "utils/allocators.h"

#ifdef __cplusplus
extern "C" {
#endif

// Windows DLL export/import macros
// For static linking (which we use), we don't need dllexport/dllimport
#ifdef _WIN32
    #ifdef MODULE_BUNDLE_SHARED
        #ifdef MODULE_BUNDLE_EXPORTS
            #define MODULE_BUNDLE_API __declspec(dllexport)
        #else
            #define MODULE_BUNDLE_API __declspec(dllimport)
        #endif
    #else
        #define MODULE_BUNDLE_API  // Static linking - no decoration needed
    #endif
#else
    #define MODULE_BUNDLE_API
#endif

// Forward declarations
struct VM;
struct TaggedValue;
struct ModuleDependency;
struct ModuleExport;
struct ModuleNative;

// Module dependency structure (bundle-specific)
typedef struct ModuleBundleDependency {
    char* name;
    char* version_requirement;  // e.g., "^1.0.0", ">=2.1.0"
    bool is_dev_dependency;
    bool is_optional;
    char* resolved_version;     // Actual version after resolution
    char* resolved_path;        // Path to resolved module
} ModuleBundleDependency;

// Module export structure (bundle-specific)
typedef struct ModuleBundleExport {
    char* name;
    char* export_path;          // Relative path within module
    char* export_type;          // "function", "constant", "type", etc.
    struct TaggedValue* value;  // Cached value if loaded
    bool is_loaded;
} ModuleBundleExport;

// Native library support (bundle-specific)
typedef struct ModuleBundleNative {
    char* library_name;         // Base name (e.g., "libmath")
    char** source_files;        // C/C++ source files
    size_t source_count;
    char** header_files;        // Header files  
    size_t header_count;
    
    // Platform-specific libraries
    char* darwin_lib;           // macOS .dylib
    char* linux_lib;            // Linux .so
    char* windows_lib;          // Windows .dll
    
    // Runtime handle
    void* loaded_handle;
    char* loaded_path;
} ModuleBundleNative;

// Bundle metadata
typedef struct ModuleBundleMetadata {
    char* name;
    char* version;
    char* description;
    char* main_file;
    char* author;
    char* license;
    char** keywords;
    size_t keyword_count;
    
    // Dependencies
    ModuleBundleDependency* dependencies;
    size_t dependency_count;
    ModuleBundleDependency* dev_dependencies;
    size_t dev_dependency_count;
    
    // Exports
    ModuleBundleExport* exports;
    size_t export_count;
    
    // Native support
    ModuleBundleNative native;
    
    // Build scripts
    char** build_scripts;
    char** script_commands;
    size_t script_count;
    
    // File patterns
    char** file_patterns;
    size_t file_pattern_count;
    
    // Engine requirements
    char* min_engine_version;
    
    // Timestamps
    time_t created_time;
    time_t modified_time;
} ModuleBundleMetadata;

// Main bundle structure
typedef struct ModuleBundle {
    ModuleBundleMetadata* metadata;
    
    // Archive data
    char* archive_path;
    mz_zip_archive* zip_archive;
    uint8_t* archive_data;
    size_t archive_size;
    bool owns_archive_data;
    
    // Bytecode storage (before saving to archive)
    struct {
        char** module_names;    // Array of module names
        uint8_t** bytecode_data; // Array of bytecode data
        size_t* bytecode_sizes;  // Array of bytecode sizes
        size_t count;           // Number of bytecode entries
        size_t capacity;        // Allocated capacity
    } pending_bytecode;
    
    // Cache information
    char* cache_path;
    char* install_path;
    bool is_cached;
    bool is_dirty;
    
    // Security
    char* signature;
    bool is_signed;
    bool signature_valid;
    
    // Reference counting
    int ref_count;
    time_t last_access;
} ModuleBundle;

// Bundle creation and lifecycle
MODULE_BUNDLE_API ModuleBundle* module_bundle_create(const char* name, const char* version);
MODULE_BUNDLE_API ModuleBundle* module_bundle_load(const char* path);
MODULE_BUNDLE_API ModuleBundle* module_bundle_load_from_memory(const uint8_t* data, size_t size);
MODULE_BUNDLE_API bool module_bundle_save(ModuleBundle* bundle, const char* path);
MODULE_BUNDLE_API void module_bundle_destroy(ModuleBundle* bundle);

// Reference counting
MODULE_BUNDLE_API ModuleBundle* module_bundle_retain(ModuleBundle* bundle);
MODULE_BUNDLE_API void module_bundle_release(ModuleBundle* bundle);

// Metadata management
bool module_bundle_set_metadata(ModuleBundle* bundle, const char* key, const char* value);
const char* module_bundle_get_metadata(ModuleBundle* bundle, const char* key);
bool module_bundle_add_dependency(ModuleBundle* bundle, const char* name, const char* version_req, bool is_dev);
bool module_bundle_remove_dependency(ModuleBundle* bundle, const char* name);

// File operations
bool module_bundle_add_file(ModuleBundle* bundle, const char* local_path, const char* archive_path);
bool module_bundle_add_bytecode(ModuleBundle* bundle, const char* module_name, const uint8_t* bytecode, size_t size);
bool module_bundle_add_native_library(ModuleBundle* bundle, const char* platform, const char* lib_path);
bool module_bundle_extract_file(ModuleBundle* bundle, const char* archive_path, const char* output_path);

// Content access
MODULE_BUNDLE_API uint8_t* module_bundle_get_bytecode(ModuleBundle* bundle, const char* module_name, size_t* size);
char* module_bundle_get_manifest_json(ModuleBundle* bundle);
char** module_bundle_list_files(ModuleBundle* bundle, size_t* count);
char** module_bundle_list_modules(ModuleBundle* bundle, size_t* count);

// Installation and caching
bool module_bundle_install(ModuleBundle* bundle, const char* install_dir);
bool module_bundle_uninstall(const char* name, const char* install_dir);
bool module_bundle_is_installed(const char* name, const char* install_dir);
char* module_bundle_get_install_path(const char* name, const char* install_dir);

// Cache management
bool module_bundle_cache_put(ModuleBundle* bundle);
ModuleBundle* module_bundle_cache_get(const char* name, const char* version);
bool module_bundle_cache_remove(const char* name, const char* version);
void module_bundle_cache_clear(void);
size_t module_bundle_cache_size(void);

// Dependency resolution
typedef struct ModuleBundleResolver ModuleBundleResolver;
ModuleBundleResolver* module_bundle_resolver_create(void);
void module_bundle_resolver_destroy(ModuleBundleResolver* resolver);
bool module_bundle_resolver_add_source(ModuleBundleResolver* resolver, const char* source_path);
ModuleBundle** module_bundle_resolve_dependencies(ModuleBundleResolver* resolver, ModuleBundle* bundle, size_t* count);

// Validation and verification
bool module_bundle_validate(ModuleBundle* bundle);
bool module_bundle_verify_signature(ModuleBundle* bundle);
bool module_bundle_sign(ModuleBundle* bundle, const char* private_key_path);

// Utility functions
const char* module_bundle_get_platform_name(void);
bool module_bundle_version_satisfies(const char* version, const char* requirement);
char* module_bundle_resolve_path(const char* base_path, const char* relative_path);

// Error handling
typedef enum {
    MODULE_BUNDLE_OK = 0,
    MODULE_BUNDLE_ERROR_INVALID_ARGS,
    MODULE_BUNDLE_ERROR_FILE_NOT_FOUND,
    MODULE_BUNDLE_ERROR_INVALID_FORMAT,
    MODULE_BUNDLE_ERROR_COMPRESSION_FAILED,
    MODULE_BUNDLE_ERROR_EXTRACTION_FAILED,
    MODULE_BUNDLE_ERROR_DEPENDENCY_NOT_FOUND,
    MODULE_BUNDLE_ERROR_VERSION_CONFLICT,
    MODULE_BUNDLE_ERROR_SIGNATURE_INVALID,
    MODULE_BUNDLE_ERROR_PERMISSION_DENIED,
    MODULE_BUNDLE_ERROR_OUT_OF_MEMORY
} ModuleBundleError;

ModuleBundleError module_bundle_get_last_error(void);
const char* module_bundle_error_string(ModuleBundleError error);

#ifdef __cplusplus
}
#endif

#endif // MODULE_BUNDLE_H