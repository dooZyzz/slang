#ifndef PACKAGE_MANAGER_H
#define PACKAGE_MANAGER_H

#include <stdbool.h>
#include <stddef.h>
#include "runtime/packages/package.h"

// Package cache types
typedef enum {
    CACHE_LOCAL,    // .cache in module root
    CACHE_GLOBAL    // System-wide cache
} CacheType;

typedef struct {
    char* name;
    char* version;
    char* path;         // Path to .swiftmodule file
    CacheType cache_type;
} InstalledPackage;

typedef struct {
    char* global_cache_dir;
    char* local_cache_dir;
    InstalledPackage* installed_packages;
    size_t package_count;
    size_t package_capacity;
} PackageCache;

typedef struct {
    PackageCache* cache;
    char* current_module_path;
    bool verbose;
} PackageManager;

// Package manager lifecycle
PackageManager* package_manager_create(void);
void package_manager_destroy(PackageManager* pm);

// Cache management
const char* package_manager_get_global_cache_dir(void);
const char* package_manager_get_local_cache_dir(const char* module_path);
bool package_manager_ensure_cache_dirs(PackageManager* pm);

// Package installation
bool package_manager_install(PackageManager* pm, const char* package_spec, bool global);
bool package_manager_install_from_path(PackageManager* pm, const char* module_path, bool global);

// Package building
bool package_manager_build(PackageManager* pm, const char* module_path, const char* output_path);

// Package running
bool package_manager_run(PackageManager* pm, const char* module_spec, int argc, char** argv);
bool package_manager_dev(PackageManager* pm, const char* module_path, int argc, char** argv);

// Module resolution
char* package_manager_resolve_module(PackageManager* pm, const char* module_name, const char* from_path);

// Cache queries
InstalledPackage* package_manager_find_package(PackageManager* pm, const char* name, const char* version);
void package_manager_list_packages(PackageManager* pm, bool global_only);

// Module name parsing
typedef struct {
    char* prefix;      // "@", "$", or NULL
    char* package;     // Package name
    char* module;      // Module within package (optional)
    char* version;     // Version constraint (optional)
} ModuleSpec;

ModuleSpec* package_manager_parse_module_spec(const char* spec);
void module_spec_free(ModuleSpec* spec);

#endif // PACKAGE_MANAGER_H