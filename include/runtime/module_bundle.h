#ifndef MODULE_BUNDLE_H
#define MODULE_BUNDLE_H

#include "runtime/module.h"
#include "runtime/package.h"
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

/**
 * Module bundling system for deployment.
 * Creates self-contained application bundles with all dependencies.
 */

// Bundle format version
#define BUNDLE_FORMAT_VERSION 1

// Bundle types
typedef enum {
    BUNDLE_TYPE_APPLICATION,    // Executable application bundle
    BUNDLE_TYPE_LIBRARY,        // Library bundle for distribution
    BUNDLE_TYPE_PLUGIN          // Plugin bundle
} BundleType;

// Bundle options
typedef struct BundleOptions {
    BundleType type;            // Type of bundle to create
    bool compress;              // Whether to compress modules
    bool strip_debug;           // Strip debug information
    bool include_sources;       // Include source files for debugging
    bool optimize;              // Optimize bytecode
    const char* output_path;    // Output file path
    const char* entry_point;    // Main module for applications
    
    // Signing/encryption (future)
    const char* signing_key;    // Code signing key
    const char* encrypt_key;    // Encryption key for protection
} BundleOptions;

// Bundle metadata
typedef struct BundleMetadata {
    char* name;                 // Bundle name
    char* version;              // Bundle version
    char* description;          // Bundle description
    BundleType type;            // Bundle type
    char* entry_point;          // Entry point module
    time_t created_at;          // Creation timestamp
    char* creator;              // Creator/build tool info
    
    // Module list
    struct {
        char** names;           // Module names
        char** versions;        // Module versions
        size_t count;          // Number of modules
    } modules;
    
    // Platform info
    char* platform;             // Target platform
    char* min_version;          // Minimum SwiftLang version
} BundleMetadata;

// Bundle creation

/**
 * Create a new bundle builder.
 * 
 * @param options Bundle creation options
 * @return Bundle builder handle or NULL on error
 */
void* bundle_builder_create(const BundleOptions* options);

/**
 * Add a module to the bundle.
 * 
 * @param builder Bundle builder handle
 * @param module_path Path to module or .swiftmodule
 * @param bundle_path Path within bundle (NULL for auto)
 * @return true on success
 */
bool bundle_builder_add_module(void* builder, const char* module_path, const char* bundle_path);

/**
 * Add all dependencies of a module.
 * 
 * @param builder Bundle builder handle
 * @param module_path Path to root module
 * @param recursive Whether to add transitive dependencies
 * @return Number of modules added
 */
int bundle_builder_add_dependencies(void* builder, const char* module_path, bool recursive);

/**
 * Add resource files to the bundle.
 * 
 * @param builder Bundle builder handle
 * @param resource_path Path to resource file/directory
 * @param bundle_path Path within bundle
 * @return true on success
 */
bool bundle_builder_add_resource(void* builder, const char* resource_path, const char* bundle_path);

/**
 * Set bundle metadata.
 * 
 * @param builder Bundle builder handle
 * @param metadata Bundle metadata
 * @return true on success
 */
bool bundle_builder_set_metadata(void* builder, const BundleMetadata* metadata);

/**
 * Build the bundle.
 * 
 * @param builder Bundle builder handle
 * @return true on success
 */
bool bundle_builder_build(void* builder);

/**
 * Destroy bundle builder.
 * 
 * @param builder Bundle builder handle
 */
void bundle_builder_destroy(void* builder);

// Bundle loading

/**
 * Open a bundle for loading.
 * 
 * @param bundle_path Path to bundle file
 * @return Bundle handle or NULL on error
 */
void* bundle_open(const char* bundle_path);

/**
 * Get bundle metadata.
 * 
 * @param bundle Bundle handle
 * @return Bundle metadata (do not free)
 */
const BundleMetadata* bundle_get_metadata(void* bundle);

/**
 * Load a module from bundle.
 * 
 * @param bundle Bundle handle
 * @param module_name Module name
 * @param loader Module loader to use
 * @return Loaded module or NULL
 */
Module* bundle_load_module(void* bundle, const char* module_name, ModuleLoader* loader);

/**
 * Extract a resource from bundle.
 * 
 * @param bundle Bundle handle
 * @param resource_path Resource path in bundle
 * @param output_path Local path to extract to
 * @return true on success
 */
bool bundle_extract_resource(void* bundle, const char* resource_path, const char* output_path);

/**
 * List all modules in bundle.
 * 
 * @param bundle Bundle handle
 * @param count Output for module count
 * @return Array of module names (caller must free)
 */
char** bundle_list_modules(void* bundle, size_t* count);

/**
 * List all resources in bundle.
 * 
 * @param bundle Bundle handle
 * @param count Output for resource count
 * @return Array of resource paths (caller must free)
 */
char** bundle_list_resources(void* bundle, size_t* count);

/**
 * Close bundle.
 * 
 * @param bundle Bundle handle
 */
void bundle_close(void* bundle);

// Bundle execution

/**
 * Execute a bundle directly.
 * 
 * @param bundle_path Path to bundle file
 * @param argc Argument count
 * @param argv Argument values
 * @return Exit code
 */
int bundle_execute(const char* bundle_path, int argc, char** argv);

/**
 * Create a standalone executable from bundle.
 * 
 * @param bundle_path Path to bundle file
 * @param output_path Path for executable
 * @param options Platform-specific options
 * @return true on success
 */
bool bundle_create_executable(const char* bundle_path, const char* output_path, void* options);

// Bundle utilities

/**
 * Verify bundle integrity.
 * 
 * @param bundle_path Path to bundle file
 * @return true if valid
 */
bool bundle_verify(const char* bundle_path);

/**
 * Get bundle information as JSON.
 * 
 * @param bundle_path Path to bundle file
 * @return JSON string (caller must free)
 */
char* bundle_info_json(const char* bundle_path);

/**
 * Optimize an existing bundle.
 * 
 * @param input_path Input bundle path
 * @param output_path Output bundle path
 * @param strip_debug Whether to strip debug info
 * @return true on success
 */
bool bundle_optimize(const char* input_path, const char* output_path, bool strip_debug);

#endif // MODULE_BUNDLE_H