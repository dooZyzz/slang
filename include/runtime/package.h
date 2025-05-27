#ifndef PACKAGE_H
#define PACKAGE_H

#include <stdbool.h>
#include "vm/vm.h"

// Forward declarations
typedef struct Module Module;
typedef struct ModuleLoader ModuleLoader;

// Module export types
typedef enum {
    MODULE_EXPORT_FUNCTION,
    MODULE_EXPORT_VARIABLE,
    MODULE_EXPORT_CONSTANT,
    MODULE_EXPORT_CLASS,
    MODULE_EXPORT_STRUCT,
    MODULE_EXPORT_TRAIT
} ModuleExportType;

// Module export definition
typedef struct {
    char* name;
    ModuleExportType type;
    char* signature;
    
    // For native functions
    char* native_name;
    
    // For constants
    TaggedValue constant_value;
    
    // For compiled exports
    uint32_t bytecode_offset;
} ModuleExport;

// Module dependency
typedef struct {
    char* name;
    char* version;
    char* path;
} ModuleDependency;

// Individual module definition within a package
typedef struct {
    char* name;               // Module name (e.g., "math.native", "stb")
    char* type;               // "native", "library", "application"
    char** sources;           // Source files (glob patterns supported)
    size_t source_count;
    char** main;              // Main entry points
    size_t main_count;
    char** dependencies;      // Module dependencies
    size_t dependency_count;
    
    // For native modules
    struct {
        char* source;
        char* header;
        char* library;
    } native;
    
    // Compiled module path (.swiftmodule)
    char* compiled_path;
} ModuleDefinition;

// Module metadata from module.json
typedef struct {
    char* name;
    char* version;
    char* description;
    char* type;  // "library" or "application"
    char* path;  // Directory containing module.json
    
    // Multiple modules defined in this package
    ModuleDefinition* modules;
    size_t module_count;
    
    // Legacy single-module support
    char* main_file;  // Main source file (e.g., "main.swift" or "src/math.swift")
    
    // Exports
    ModuleExport* exports;
    size_t export_count;
    
    // Dependencies
    ModuleDependency* dependencies;
    size_t dependency_count;
    
    // Native library info
    struct {
        char* source;
        char* header;
        char* library;
    } native;
    
    // Compiled module path (.swiftmodule)
    char* compiled_path;
} ModuleMetadata;

// Package system configuration
typedef struct PackageSystem {
    // Root module metadata
    ModuleMetadata* root_module;
    
    // Module cache - loaded module metadata
    struct {
        ModuleMetadata** modules;
        size_t count;
        size_t capacity;
    } cache;
    
    // Module search paths
    char** search_paths;
    size_t search_path_count;
    
    // Dependency resolution map
    struct {
        char** names;      // Module names
        char** paths;      // Resolved paths
        size_t count;
        size_t capacity;
    } dependencies;
    
    VM* vm;
} PackageSystem;

// Package system functions
PackageSystem* package_system_create(VM* vm);
void package_system_destroy(PackageSystem* pkg_sys);

// Load module metadata from module.json
ModuleMetadata* package_load_module_metadata(const char* path);
void package_free_module_metadata(ModuleMetadata* metadata);

// Load root module configuration
bool package_system_load_root(PackageSystem* pkg_sys, const char* path);

// Resolve module path from import name
char* package_resolve_module_path(PackageSystem* pkg_sys, const char* module_name);

// Get cached module metadata
ModuleMetadata* package_get_module_metadata(PackageSystem* pkg_sys, const char* module_name);

// Cache module metadata
void package_cache_module_metadata(PackageSystem* pkg_sys, ModuleMetadata* metadata);

// Resolve dependencies for a module
bool package_resolve_dependencies(PackageSystem* pkg_sys, ModuleMetadata* module);

// Load native library for module
void* package_load_native_library(ModuleMetadata* module);

// Initialize standard library namespace
void package_init_stdlib_namespace(PackageSystem* pkg_sys);

// Load a specific module from a package
Module* package_load_module_from_metadata(ModuleLoader* loader, ModuleMetadata* metadata, const char* module_name);

// Find module definition by name within metadata
ModuleDefinition* package_find_module_definition(ModuleMetadata* metadata, const char* module_name);

#endif