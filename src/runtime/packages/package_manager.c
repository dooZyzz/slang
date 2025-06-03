#include "runtime/packages/package_manager.h"
#include "runtime/modules/loader/module_loader.h"
#include "runtime/modules/loader/module_compiler.h"
#include "runtime/modules/formats/module_archive.h"
#include "runtime/modules/formats/module_bundle.h"
#include "utils/compiler_wrapper.h"
#include "utils/platform_compat.h"
#include "utils/platform_dir.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <limits.h>
#include <errno.h>

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
    #define PATH_SEPARATOR "\\"
#else
    #include <pwd.h>
    #define PATH_SEPARATOR "/"
#endif

// Get the global cache directory based on platform
const char* package_manager_get_global_cache_dir(void) {
    static char cache_dir[PATH_MAX];
    static bool initialized = false;
    
    if (initialized) {
        return cache_dir;
    }
    
    // Check SWIFTLANG_CACHE environment variable first
    const char* env_cache = getenv("SWIFTLANG_CACHE");
    if (env_cache) {
        strncpy(cache_dir, env_cache, PATH_MAX - 1);
        initialized = true;
        return cache_dir;
    }
    
#ifdef _WIN32
    // Windows: Use %LOCALAPPDATA%\SwiftLang\cache
    char local_app_data[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local_app_data) == S_OK) {
        snprintf(cache_dir, PATH_MAX, "%s\\SwiftLang\\cache", local_app_data);
    } else {
        // Fallback to user profile
        const char* userprofile = getenv("USERPROFILE");
        if (userprofile) {
            snprintf(cache_dir, PATH_MAX, "%s\\.swiftlang\\cache", userprofile);
        } else {
            strcpy(cache_dir, "C:\\SwiftLang\\cache");
        }
    }
#else
    // Unix-like: Use ~/.swiftlang/cache
    const char* home = getenv("HOME");
    if (!home) {
        struct passwd* pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        }
    }
    
    if (home) {
        snprintf(cache_dir, PATH_MAX, "%s/.swiftlang/cache", home);
    } else {
        strcpy(cache_dir, "/tmp/.swiftlang/cache");
    }
#endif
    
    initialized = true;
    return cache_dir;
}

// Get the local cache directory for a module
const char* package_manager_get_local_cache_dir(const char* module_path) {
    static char cache_dir[PATH_MAX];
    
    if (!module_path) {
        module_path = ".";
    }
    
    // Find module root (directory containing module.json)
    char abs_path[PATH_MAX];
    if (realpath(module_path, abs_path) == NULL) {
        strncpy(abs_path, module_path, PATH_MAX - 1);
    }
    
    // Search upward for module.json
    char test_path[PATH_MAX];
    char* current = abs_path;
    
    while (true) {
        snprintf(test_path, PATH_MAX, "%s%smodule.json", current, PATH_SEPARATOR);
        
        struct stat st;
        if (stat(test_path, &st) == 0) {
            // Found module.json
            snprintf(cache_dir, PATH_MAX, "%s%s.cache", current, PATH_SEPARATOR);
            return cache_dir;
        }
        
        // Move up one directory
        char* last_sep = strrchr(current, PATH_SEPARATOR[0]);
        if (!last_sep || last_sep == current) {
            break;
        }
        *last_sep = '\0';
    }
    
    // Default to current directory
    snprintf(cache_dir, PATH_MAX, "%s%s.cache", module_path, PATH_SEPARATOR);
    return cache_dir;
}

// Ensure cache directories exist
static bool ensure_directory(const char* path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return S_ISDIR(st.st_mode);
    }
    
#ifdef _WIN32
    return CreateDirectoryA(path, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#else
    // Create parent directories if needed
    char* path_copy = strdup(path);
    char* p = path_copy;
    
    while (*p) {
        if (*p == '/' && p != path_copy) {
            *p = '\0';
            mkdir(path_copy, 0755);
            *p = '/';
        }
        p++;
    }
    
    int result = mkdir(path, 0755);
    free(path_copy);
    return result == 0 || errno == EEXIST;
#endif
}

// Create package manager
PackageManager* package_manager_create(void) {
    PackageManager* pm = calloc(1, sizeof(PackageManager));
    if (!pm) return NULL;
    
    pm->cache = calloc(1, sizeof(PackageCache));
    if (!pm->cache) {
        free(pm);
        return NULL;
    }
    
    pm->cache->global_cache_dir = strdup(package_manager_get_global_cache_dir());
    
    // Get current working directory for local cache
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd))) {
        pm->cache->local_cache_dir = strdup(package_manager_get_local_cache_dir(cwd));
    }
    
    pm->cache->package_capacity = 16;
    pm->cache->installed_packages = calloc(pm->cache->package_capacity, sizeof(InstalledPackage));
    
    return pm;
}

// Destroy package manager
void package_manager_destroy(PackageManager* pm) {
    if (!pm) return;
    
    if (pm->cache) {
        free(pm->cache->global_cache_dir);
        free(pm->cache->local_cache_dir);
        
        for (size_t i = 0; i < pm->cache->package_count; i++) {
            free(pm->cache->installed_packages[i].name);
            free(pm->cache->installed_packages[i].version);
            free(pm->cache->installed_packages[i].path);
        }
        free(pm->cache->installed_packages);
        free(pm->cache);
    }
    
    free(pm->current_module_path);
    free(pm);
}

// Ensure cache directories exist
bool package_manager_ensure_cache_dirs(PackageManager* pm) {
    if (!pm || !pm->cache) return false;
    
    bool success = true;
    
    if (pm->cache->global_cache_dir) {
        success &= ensure_directory(pm->cache->global_cache_dir);
    }
    
    if (pm->cache->local_cache_dir) {
        success &= ensure_directory(pm->cache->local_cache_dir);
    }
    
    return success;
}

// Parse module specification
ModuleSpec* package_manager_parse_module_spec(const char* spec) {
    if (!spec) return NULL;
    
    ModuleSpec* ms = calloc(1, sizeof(ModuleSpec));
    if (!ms) return NULL;
    
    char* spec_copy = strdup(spec);
    char* p = spec_copy;
    
    // Check for prefix
    if (*p == '@' || *p == '$') {
        ms->prefix = malloc(2);
        ms->prefix[0] = *p;
        ms->prefix[1] = '\0';
        p++;
    }
    
    // Parse package name
    char* slash = strchr(p, '/');
    char* at = strchr(p, '@');
    
    if (at && (!slash || at < slash)) {
        // Version specified
        size_t pkg_len = at - p;
        ms->package = malloc(pkg_len + 1);
        strncpy(ms->package, p, pkg_len);
        ms->package[pkg_len] = '\0';
        
        p = at + 1;
        if (slash) {
            size_t ver_len = slash - p;
            ms->version = malloc(ver_len + 1);
            strncpy(ms->version, p, ver_len);
            ms->version[ver_len] = '\0';
            
            ms->module = strdup(slash + 1);
        } else {
            ms->version = strdup(p);
        }
    } else if (slash) {
        // Module within package
        size_t pkg_len = slash - p;
        ms->package = malloc(pkg_len + 1);
        strncpy(ms->package, p, pkg_len);
        ms->package[pkg_len] = '\0';
        
        ms->module = strdup(slash + 1);
    } else {
        // Just package name
        ms->package = strdup(p);
    }
    
    free(spec_copy);
    return ms;
}

void module_spec_free(ModuleSpec* spec) {
    if (!spec) return;
    free(spec->prefix);
    free(spec->package);
    free(spec->module);
    free(spec->version);
    free(spec);
}

// Build a module to a .swiftmodule file
bool package_manager_build(PackageManager* pm, const char* module_path, const char* output_path) {
    if (!pm) return false;
    
    // Load module metadata
    char module_json_path[PATH_MAX];
    snprintf(module_json_path, sizeof(module_json_path), "%s%smodule.json", 
             module_path, PATH_SEPARATOR);
    
    ModuleMetadata* metadata = package_load_module_metadata(module_json_path);
    if (!metadata) {
        fprintf(stderr, "Failed to load module.json from %s\n", module_json_path);
        return false;
    }
    
    // Determine output path
    char actual_output[PATH_MAX];
    if (output_path) {
        strncpy(actual_output, output_path, PATH_MAX - 1);
    } else {
        snprintf(actual_output, sizeof(actual_output), "%s.swiftmodule", metadata->name);
    }
    
    // Compile all native modules first
    if (metadata->modules) {
        for (size_t i = 0; i < metadata->module_count; i++) {
            ModuleDefinition* mod_def = &metadata->modules[i];
            
            if (strcmp(mod_def->type, "native") == 0) {
                // Build source paths
                char** full_sources = malloc(mod_def->source_count * sizeof(char*));
                for (size_t j = 0; j < mod_def->source_count; j++) {
                    full_sources[j] = malloc(PATH_MAX);
                    snprintf(full_sources[j], PATH_MAX, "%s%s%s", 
                             module_path, PATH_SEPARATOR, mod_def->sources[j]);
                }
                
                // Get output path for native module
                char* native_output = compiler_get_module_output_path(mod_def->name, module_path);
                
                // Check if rebuild needed
                if (compiler_needs_rebuild(native_output, (const char**)full_sources, mod_def->source_count)) {
                    if (pm->verbose) {
                        printf("Compiling native module: %s\n", mod_def->name);
                    }
                    
                    CompilerOptions* options = compiler_options_from_module(
                        mod_def->name, mod_def->type, full_sources, mod_def->source_count);
                    
                    // Add module path as include directory
                    options->include_dirs = realloc(options->include_dirs, 
                                                  (options->include_count + 1) * sizeof(char*));
                    options->include_dirs[options->include_count] = strdup(module_path);
                    options->include_count++;
                    
                    if (!compiler_compile(options, module_path)) {
                        fprintf(stderr, "Failed to compile native module: %s\n", mod_def->name);
                        compiler_options_free(options);
                        for (size_t j = 0; j < mod_def->source_count; j++) {
                            free(full_sources[j]);
                        }
                        free(full_sources);
                        free(native_output);
                        package_free_module_metadata(metadata);
                        return false;
                    }
                    
                    compiler_options_free(options);
                }
                
                // Clean up
                for (size_t j = 0; j < mod_def->source_count; j++) {
                    free(full_sources[j]);
                }
                free(full_sources);
                free(native_output);
            }
        }
    }
    
    // Create module compiler
    ModuleCompiler* compiler = module_compiler_create();
    if (!compiler) {
        fprintf(stderr, "Failed to create module compiler\n");
        package_free_module_metadata(metadata);
        return false;
    }
    
    // Compile the module
    ModuleCompilerOptions options = {
        .optimize = false,
        .strip_debug = false,
        .include_source = true,
        .output_dir = module_path
    };
    
    bool success = module_compiler_build_package(compiler, metadata, actual_output, &options);
    
    if (success && pm->verbose) {
        printf("Successfully built module: %s\n", actual_output);
    }
    
    module_compiler_destroy(compiler);
    package_free_module_metadata(metadata);
    
    return success;
}

// Install a module to cache
bool package_manager_install(PackageManager* pm, const char* package_spec, bool global) {
    if (!pm) return false;
    
    ModuleSpec* spec = package_manager_parse_module_spec(package_spec);
    if (!spec) {
        fprintf(stderr, "Invalid package specification: %s\n", package_spec);
        return false;
    }
    
    // For now, we'll look for the module in the current directory or a known location
    // In the future, this would download from a registry
    
    char module_path[PATH_MAX];
    
    // Check if it's a local path
    struct stat st;
    if (stat(package_spec, &st) == 0 && S_ISDIR(st.st_mode)) {
        strncpy(module_path, package_spec, PATH_MAX - 1);
    } else {
        // Look in standard locations
        // For now, check test_modules directory
        snprintf(module_path, sizeof(module_path), "test_modules/%s", spec->package);
        
        if (stat(module_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
            fprintf(stderr, "Package not found: %s\n", spec->package);
            module_spec_free(spec);
            return false;
        }
    }
    
    bool result = package_manager_install_from_path(pm, module_path, global);
    module_spec_free(spec);
    return result;
}

// Install from a local path
bool package_manager_install_from_path(PackageManager* pm, const char* module_path, bool global) {
    if (!pm || !module_path) return false;
    
    // Ensure cache directories exist
    package_manager_ensure_cache_dirs(pm);
    
    // Build the module first
    char temp_output[PATH_MAX];
    snprintf(temp_output, sizeof(temp_output), "%s%stemp.swiftmodule", 
             global ? pm->cache->global_cache_dir : pm->cache->local_cache_dir,
             PATH_SEPARATOR);
    
    if (!package_manager_build(pm, module_path, temp_output)) {
        fprintf(stderr, "Failed to build module from %s\n", module_path);
        return false;
    }
    
    // Load metadata to get package info
    char module_json_path[PATH_MAX];
    snprintf(module_json_path, sizeof(module_json_path), "%s%smodule.json", 
             module_path, PATH_SEPARATOR);
    
    ModuleMetadata* metadata = package_load_module_metadata(module_json_path);
    if (!metadata) {
        unlink(temp_output);
        return false;
    }
    
    // Move to final location
    char final_output[PATH_MAX];
    snprintf(final_output, sizeof(final_output), "%s%s%s-%s.swiftmodule",
             global ? pm->cache->global_cache_dir : pm->cache->local_cache_dir,
             PATH_SEPARATOR, metadata->name, metadata->version);
    
    if (rename(temp_output, final_output) != 0) {
        fprintf(stderr, "Failed to install module: %s\n", strerror(errno));
        package_free_module_metadata(metadata);
        unlink(temp_output);
        return false;
    }
    
    // Add to installed packages
    if (pm->cache->package_count >= pm->cache->package_capacity) {
        pm->cache->package_capacity *= 2;
        pm->cache->installed_packages = realloc(pm->cache->installed_packages,
                                               pm->cache->package_capacity * sizeof(InstalledPackage));
    }
    
    InstalledPackage* pkg = &pm->cache->installed_packages[pm->cache->package_count++];
    pkg->name = strdup(metadata->name);
    pkg->version = strdup(metadata->version);
    pkg->path = strdup(final_output);
    pkg->cache_type = global ? CACHE_GLOBAL : CACHE_LOCAL;
    
    if (pm->verbose) {
        printf("Installed %s@%s to %s cache\n", 
               metadata->name, metadata->version, global ? "global" : "local");
    }
    
    package_free_module_metadata(metadata);
    return true;
}

// Resolve a module import
char* package_manager_resolve_module(PackageManager* pm, const char* module_name, const char* from_path) {
    if (!pm || !module_name) return NULL;
    
    ModuleSpec* spec = package_manager_parse_module_spec(module_name);
    if (!spec) return NULL;
    
    char* resolved_path = NULL;
    
    // Handle different prefixes
    if (spec->prefix) {
        if (strcmp(spec->prefix, "@") == 0) {
            // Local module within same package
            if (from_path) {
                // Find module root
                char module_root[PATH_MAX];
                strncpy(module_root, from_path, PATH_MAX - 1);
                
                // Remove filename if present
                char* last_sep = strrchr(module_root, PATH_SEPARATOR[0]);
                if (last_sep) {
                    *last_sep = '\0';
                }
                
                // Look for the module
                resolved_path = malloc(PATH_MAX);
                snprintf(resolved_path, PATH_MAX, "%s%s%s", 
                         module_root, PATH_SEPARATOR, spec->package);
            }
        } else if (strcmp(spec->prefix, "$") == 0) {
            // Native module - these are handled specially
            resolved_path = malloc(strlen(module_name) + 1);
            strcpy(resolved_path, module_name);
        }
    } else {
        // Package dependency - look in caches
        // First check local cache
        if (pm->cache->local_cache_dir) {
            char test_path[PATH_MAX];
            snprintf(test_path, sizeof(test_path), "%s%s%s-%s.swiftmodule",
                     pm->cache->local_cache_dir, PATH_SEPARATOR,
                     spec->package, spec->version ? spec->version : "*");
            
            // Simple version matching for now
            platform_dir_t* dir = platform_opendir(pm->cache->local_cache_dir);
            if (dir) {
                platform_dirent_t entry;
                while (platform_readdir(dir, &entry)) {
                    const char* entry_name = entry.name;
                    if (strstr(entry_name, spec->package) &&
                        strstr(entry_name, ".swiftmodule")) {
                        resolved_path = malloc(PATH_MAX);
                        snprintf(resolved_path, PATH_MAX, "%s%s%s",
                                 pm->cache->local_cache_dir, PATH_SEPARATOR, entry_name);
                        break;
                    }
                }
                platform_closedir(dir);
            }
        }
        
        // If not found, check global cache
        if (!resolved_path && pm->cache->global_cache_dir) {
            platform_dir_t* dir = platform_opendir(pm->cache->global_cache_dir);
            if (dir) {
                platform_dirent_t entry;
                while (platform_readdir(dir, &entry)) {
                    const char* entry_name = entry.name;
                    if (strstr(entry_name, spec->package) &&
                        strstr(entry_name, ".swiftmodule")) {
                        resolved_path = malloc(PATH_MAX);
                        snprintf(resolved_path, PATH_MAX, "%s%s%s",
                                 pm->cache->global_cache_dir, PATH_SEPARATOR, entry_name);
                        break;
                    }
                }
                platform_closedir(dir);
            }
        }
    }
    
    module_spec_free(spec);
    return resolved_path;
}

// Run a module
bool package_manager_run(PackageManager* pm, const char* module_spec, int argc, char** argv) {
    if (!pm || !module_spec) return false;
    
    // First, try to resolve the module spec to a path
    char* module_path = package_manager_resolve_module(pm, module_spec, NULL);
    if (!module_path) {
        // If not found as installed module, check if it's a direct path
        struct stat st;
        if (stat(module_spec, &st) == 0) {
            if (S_ISREG(st.st_mode) && strstr(module_spec, ".swiftmodule")) {
                // Direct path to .swiftmodule file
                module_path = strdup(module_spec);
            } else if (S_ISDIR(st.st_mode)) {
                // Directory path - build and run
                return package_manager_dev(pm, module_spec, argc, argv);
            }
        }
        
        if (!module_path) {
            fprintf(stderr, "Module not found: %s\n", module_spec);
            return false;
        }
    }
    
    // Check if the module file exists
    struct stat st;
    if (stat(module_path, &st) != 0) {
        fprintf(stderr, "Module file not found: %s\n", module_path);
        free(module_path);
        return false;
    }
    
    // Use bundle_execute to run the module
    int exit_code = bundle_execute(module_path, argc, argv);
    
    free(module_path);
    return exit_code == 0;
}

// Run in development mode
bool package_manager_dev(PackageManager* pm, const char* module_path, int argc, char** argv) {
    if (!pm) return false;
    
    const char* path = module_path ? module_path : ".";
    
    // Check if directory exists
    struct stat st;
    if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Directory not found: %s\n", path);
        return false;
    }
    
    // Create temporary output file
    char temp_output[PATH_MAX];
    snprintf(temp_output, sizeof(temp_output), "/tmp/swiftlang_dev_%d.swiftmodule", getpid());
    
    if (pm->verbose) {
        printf("Building module from %s...\n", path);
    }
    
    // Build the module
    if (!package_manager_build(pm, path, temp_output)) {
        fprintf(stderr, "Failed to build module\n");
        return false;
    }
    
    if (pm->verbose) {
        printf("Running module...\n");
    }
    
    // Execute the built module
    int exit_code = bundle_execute(temp_output, argc, argv);
    
    // Clean up temporary file
    unlink(temp_output);
    
    return exit_code == 0;
}