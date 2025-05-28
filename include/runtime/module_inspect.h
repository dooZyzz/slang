#ifndef MODULE_INSPECT_H
#define MODULE_INSPECT_H

#include "runtime/module.h"
#include "vm/vm.h"
#include <stdbool.h>
#include <stddef.h>

/**
 * Module metadata inspection API.
 * Provides runtime access to module information, exports, dependencies, and statistics.
 */

// Module information structure
typedef struct ModuleInfo {
    const char* path;           // Module path/name
    const char* absolute_path;  // Full file path
    const char* version;        // Module version
    const char* description;    // Module description (from metadata)
    const char* type;           // Module type (library, application, native)
    ModuleState state;          // Current module state
    bool is_native;            // Whether it's a native module
    bool is_lazy;              // Whether it's lazy-loaded
    size_t export_count;       // Number of exports
    size_t global_count;       // Number of globals
    size_t memory_usage;       // Estimated memory usage in bytes
} ModuleInfo;

// Export information structure
typedef struct ExportInfo {
    const char* name;          // Export name
    ValueType type;            // Export type
    const char* type_name;     // Human-readable type name
    bool is_function;          // Whether it's a function
    bool is_constant;          // Whether it's a constant
    uint8_t visibility;        // Export visibility level
    
    // Function-specific info
    struct {
        int arity;             // Number of parameters (-1 if unknown)
        bool is_native;        // Whether it's a native function
        bool is_closure;       // Whether it's a closure
        const char* module;    // Module that defined this function
    } function;
} ExportInfo;

// Dependency information
typedef struct DependencyInfo {
    const char* name;          // Dependency module name
    const char* version;       // Required version
    const char* resolved_path; // Resolved module path
    bool is_loaded;           // Whether dependency is loaded
    bool is_optional;         // Whether dependency is optional
} DependencyInfo;

// Module statistics
typedef struct ModuleStats {
    size_t load_time_ms;      // Time taken to load module
    size_t init_time_ms;      // Time taken to initialize
    size_t access_count;      // Number of times accessed
    size_t export_lookups;    // Number of export lookups
    size_t cache_hits;        // Number of cache hits
    size_t cache_misses;      // Number of cache misses
} ModuleStats;

// Basic inspection functions

/**
 * Get information about a loaded module.
 * 
 * @param module The module to inspect
 * @return Module information (caller must free with module_info_free)
 */
ModuleInfo* module_get_info(Module* module);

/**
 * Free module information structure.
 * 
 * @param info The module info to free
 */
void module_info_free(ModuleInfo* info);

/**
 * Get list of all loaded modules.
 * 
 * @param loader The module loader
 * @param count Output parameter for number of modules
 * @return Array of module pointers (caller must free array, not modules)
 */
Module** module_get_all_loaded(ModuleLoader* loader, size_t* count);

// Export inspection

/**
 * Get information about all exports from a module.
 * 
 * @param module The module to inspect
 * @param count Output parameter for number of exports
 * @return Array of export info (caller must free with module_exports_free)
 */
ExportInfo* module_get_exports(Module* module, size_t* count);

/**
 * Get information about a specific export.
 * 
 * @param module The module to inspect
 * @param export_name Name of the export
 * @return Export info or NULL if not found (caller must free)
 */
ExportInfo* module_get_export_info(Module* module, const char* export_name);

/**
 * Free export information.
 * 
 * @param exports Array of export info
 * @param count Number of exports
 */
void module_exports_free(ExportInfo* exports, size_t count);

// Dependency inspection

/**
 * Get module dependencies.
 * 
 * @param module The module to inspect
 * @param count Output parameter for number of dependencies
 * @return Array of dependency info (caller must free)
 */
DependencyInfo* module_get_dependencies(Module* module, size_t* count);

/**
 * Free dependency information.
 * 
 * @param deps Array of dependency info
 * @param count Number of dependencies
 */
void module_dependencies_free(DependencyInfo* deps, size_t count);

// Statistics

/**
 * Get module statistics.
 * 
 * @param module The module to inspect
 * @return Module statistics (caller must free)
 */
ModuleStats* module_get_stats(Module* module);

/**
 * Free module statistics.
 * 
 * @param stats The statistics to free
 */
void module_stats_free(ModuleStats* stats);

// Search and filtering

/**
 * Find modules by pattern.
 * 
 * @param loader The module loader
 * @param pattern Pattern to match (supports wildcards)
 * @param count Output parameter for number of matches
 * @return Array of matching modules (caller must free array)
 */
Module** module_find_by_pattern(ModuleLoader* loader, const char* pattern, size_t* count);

/**
 * Find modules that export a specific symbol.
 * 
 * @param loader The module loader
 * @param symbol_name The symbol to search for
 * @param count Output parameter for number of matches
 * @return Array of modules that export the symbol (caller must free array)
 */
Module** module_find_by_export(ModuleLoader* loader, const char* symbol_name, size_t* count);

// Serialization

/**
 * Serialize module info to JSON.
 * 
 * @param module The module to serialize
 * @param include_exports Whether to include export details
 * @param include_stats Whether to include statistics
 * @return JSON string (caller must free)
 */
char* module_to_json(Module* module, bool include_exports, bool include_stats);

/**
 * Serialize all loaded modules to JSON.
 * 
 * @param loader The module loader
 * @return JSON string (caller must free)
 */
char* module_loader_to_json(ModuleLoader* loader);

// Utility functions

/**
 * Get human-readable string for module state.
 * 
 * @param state The module state
 * @return State string (do not free)
 */
const char* module_state_to_string(ModuleState state);

/**
 * Get human-readable string for value type.
 * 
 * @param type The value type
 * @return Type string (do not free)
 */
const char* value_type_to_string(ValueType type);

/**
 * Check if a module has a specific capability.
 * 
 * @param module The module to check
 * @param capability The capability name (e.g., "async", "native", "lazy")
 * @return true if module has the capability
 */
bool module_has_capability(Module* module, const char* capability);

// Debug helpers

/**
 * Print module information to stderr.
 * 
 * @param module The module to print
 * @param verbose Whether to include detailed information
 */
void module_print_info(Module* module, bool verbose);

/**
 * Print module dependency tree.
 * 
 * @param module The root module
 * @param max_depth Maximum depth to print (-1 for unlimited)
 */
void module_print_dependency_tree(Module* module, int max_depth);

// Tracking functions for statistics
void module_track_access(Module* module);
void module_track_export_lookup(Module* module, bool hit);
void module_track_load_start(Module* module);
void module_track_load_end(Module* module);
void module_track_init_start(Module* module);
void module_track_init_end(Module* module);

// Native function registration
void register_module_natives(VM* vm);

#endif // MODULE_INSPECT_H