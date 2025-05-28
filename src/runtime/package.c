#include "runtime/package.h"
#include "runtime/module.h"
#include "runtime/module_archive.h"
#include "vm/object.h"
#include "vm/vm.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "ast/ast.h"
#include "utils/compiler_wrapper.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/stat.h>

// Forward declaration for module loading
Module* load_module_from_metadata(ModuleLoader* loader, ModuleMetadata* metadata);

static char* expand_path(const char* path) {
    if (!path) return NULL;
    
    char expanded[PATH_MAX];
    const char* home = getenv("HOME");
    
    if (strstr(path, "${HOME}") && home) {
        char* pos = strstr(path, "${HOME}");
        size_t prefix_len = pos - path;
        strncpy(expanded, path, prefix_len);
        strcpy(expanded + prefix_len, home);
        strcat(expanded, pos + 7); // Skip "${HOME}"
        return strdup(expanded);
    }
    
    return strdup(path);
}

// Simple JSON parser for module.json
// In production, use a proper JSON library
static char* json_get_string(const char* json, const char* key) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char* start = strstr(json, search_key);
    if (!start) return NULL;
    
    // Move past the key and colon
    start += strlen(search_key);
    
    // Skip whitespace
    while (*start && (*start == ' ' || *start == '\t' || *start == '\n')) {
        start++;
    }
    
    // Check for opening quote
    if (*start != '"') return NULL;
    start++; // Skip opening quote
    
    // Find closing quote
    const char* end = start;
    while (*end && *end != '"') {
        if (*end == '\\' && *(end + 1)) {
            end += 2; // Skip escaped character
        } else {
            end++;
        }
    }
    
    if (*end != '"') return NULL;
    
    size_t len = end - start;
    char* result = malloc(len + 1);
    strncpy(result, start, len);
    result[len] = '\0';
    
    return result;
}

static double json_get_number(const char* json, const char* key) {
    char search_key[256];
    snprintf(search_key, sizeof(search_key), "\"%s\":", key);
    
    const char* start = strstr(json, search_key);
    if (!start) return 0.0;
    
    start = strchr(start, ':');
    if (!start) return 0.0;
    start++; // Skip colon
    
    while (*start && (*start == ' ' || *start == '\t')) {
        start++;
    }
    
    return strtod(start, NULL);
}

static ModuleExport* parse_export(const char* json_obj) {
    ModuleExport* export = calloc(1, sizeof(ModuleExport));
    
    // Get type
    char* type_str = json_get_string(json_obj, "type");
    if (type_str) {
        if (strcmp(type_str, "function") == 0) {
            export->type = MODULE_EXPORT_FUNCTION;
        } else if (strcmp(type_str, "constant") == 0) {
            export->type = MODULE_EXPORT_CONSTANT;
        } else if (strcmp(type_str, "variable") == 0) {
            export->type = MODULE_EXPORT_VARIABLE;
        } else if (strcmp(type_str, "class") == 0) {
            export->type = MODULE_EXPORT_CLASS;
        } else if (strcmp(type_str, "struct") == 0) {
            export->type = MODULE_EXPORT_STRUCT;
        } else if (strcmp(type_str, "trait") == 0) {
            export->type = MODULE_EXPORT_TRAIT;
        }
        free(type_str);
    }
    
    // Get signature
    export->signature = json_get_string(json_obj, "signature");
    
    // Get native name for functions
    if (export->type == MODULE_EXPORT_FUNCTION) {
        export->native_name = json_get_string(json_obj, "native");
    }
    
    // Get value for constants
    if (export->type == MODULE_EXPORT_CONSTANT) {
        double value = json_get_number(json_obj, "value");
        export->constant_value = NUMBER_VAL(value);
    }
    
    return export;
}

// Parse a JSON array of strings
static char** json_parse_string_array(const char* json_array, size_t* count) {
    if (!json_array || *json_array != '[') {
        *count = 0;
        return NULL;
    }
    
    // Count elements
    *count = 0;
    const char* p = json_array + 1;
    bool in_string = false;
    while (*p && *p != ']') {
        if (!in_string && *p == '"') {
            (*count)++;
            in_string = true;
        } else if (in_string && *p == '"' && *(p-1) != '\\') {
            in_string = false;
        }
        p++;
    }
    
    if (*count == 0) return NULL;
    
    // Allocate array
    char** array = calloc(*count, sizeof(char*));
    
    // Parse strings
    p = json_array + 1;
    size_t idx = 0;
    while (*p && *p != ']' && idx < *count) {
        // Skip whitespace
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',')) p++;
        
        if (*p == '"') {
            const char* start = p + 1;
            const char* end = strchr(start, '"');
            if (end) {
                size_t len = end - start;
                array[idx] = malloc(len + 1);
                strncpy(array[idx], start, len);
                array[idx][len] = '\0';
                idx++;
                p = end + 1;
            } else {
                break;
            }
        } else {
            p++;
        }
    }
    
    *count = idx;
    return array;
}

// Parse module definition from JSON object
static ModuleDefinition* parse_module_definition(const char* json_obj) {
    ModuleDefinition* def = calloc(1, sizeof(ModuleDefinition));
    
    def->name = json_get_string(json_obj, "name");
    def->type = json_get_string(json_obj, "type");
    
    // Parse sources array
    const char* sources_start = strstr(json_obj, "\"sources\":");
    if (sources_start) {
        sources_start = strchr(sources_start, '[');
        if (sources_start) {
            // Find end of array
            const char* sources_end = sources_start + 1;
            int bracket_count = 1;
            while (*sources_end && bracket_count > 0) {
                if (*sources_end == '[') bracket_count++;
                else if (*sources_end == ']') bracket_count--;
                sources_end++;
            }
            
            size_t len = sources_end - sources_start;
            char* sources_array = malloc(len + 1);
            strncpy(sources_array, sources_start, len);
            sources_array[len] = '\0';
            
            def->sources = json_parse_string_array(sources_array, &def->source_count);
            free(sources_array);
        }
    }
    
    // Parse main array or string
    const char* main_start = strstr(json_obj, "\"main\":");
    if (main_start) {
        main_start = strchr(main_start, ':');
        if (main_start) {
            main_start++; // Skip colon
            while (*main_start && (*main_start == ' ' || *main_start == '\t')) main_start++;
            
            if (*main_start == '[') {
                // Array of main files
                const char* main_end = main_start + 1;
                int bracket_count = 1;
                while (*main_end && bracket_count > 0) {
                    if (*main_end == '[') bracket_count++;
                    else if (*main_end == ']') bracket_count--;
                    main_end++;
                }
                
                size_t len = main_end - main_start;
                char* main_array = malloc(len + 1);
                strncpy(main_array, main_start, len);
                main_array[len] = '\0';
                
                def->main = json_parse_string_array(main_array, &def->main_count);
                free(main_array);
            } else if (*main_start == '"') {
                // Single main file as string
                def->main_count = 1;
                def->main = calloc(1, sizeof(char*));
                
                const char* end = strchr(main_start + 1, '"');
                if (end) {
                    size_t len = end - main_start - 1;
                    def->main[0] = malloc(len + 1);
                    strncpy(def->main[0], main_start + 1, len);
                    def->main[0][len] = '\0';
                }
            }
        }
    }
    
    // Parse dependencies
    const char* deps_start = strstr(json_obj, "\"dependencies\":");
    if (deps_start) {
        deps_start = strchr(deps_start, '[');
        if (deps_start) {
            // Find end of array
            const char* deps_end = deps_start + 1;
            int bracket_count = 1;
            while (*deps_end && bracket_count > 0) {
                if (*deps_end == '[') bracket_count++;
                else if (*deps_end == ']') bracket_count--;
                deps_end++;
            }
            
            size_t len = deps_end - deps_start;
            char* deps_array = malloc(len + 1);
            strncpy(deps_array, deps_start, len);
            deps_array[len] = '\0';
            
            def->dependencies = json_parse_string_array(deps_array, &def->dependency_count);
            free(deps_array);
        }
    }
    
    return def;
}

ModuleMetadata* package_load_module_metadata(const char* path) {
    char module_json_path[PATH_MAX];
    struct stat st;
    
    fprintf(stderr, "package_load_module_metadata called with path: %s\n", path);
    
    // Check if path ends with .swiftmodule (archive)
    size_t path_len = strlen(path);
    if (path_len > 12 && strcmp(path + path_len - 12, ".swiftmodule") == 0) {
        fprintf(stderr, "Path is a .swiftmodule archive, extracting module.json\n");
        
        // Open archive and extract module.json
        ModuleArchive* archive = module_archive_open(path);
        if (!archive) {
            fprintf(stderr, "Failed to open archive: %s\n", path);
            return NULL;
        }
        
        char* json_content = NULL;
        size_t json_size = 0;
        if (!module_archive_extract_json(archive, &json_content, &json_size)) {
            fprintf(stderr, "Failed to extract module.json from archive\n");
            module_archive_destroy(archive);
            return NULL;
        }
        
        // Parse module.json content
        ModuleMetadata* metadata = calloc(1, sizeof(ModuleMetadata));
        metadata->name = json_get_string(json_content, "name");
        metadata->version = json_get_string(json_content, "version");
        metadata->description = json_get_string(json_content, "description");
        metadata->type = json_get_string(json_content, "type");
        metadata->main_file = json_get_string(json_content, "main");
        metadata->path = strdup(path);
        metadata->compiled_path = strdup(path); // The archive is the compiled module
        
        // Parse exports from JSON content
        const char* exports_start = strstr(json_content, "\"exports\":");
        if (exports_start) {
            // TODO: Parse exports properly
            metadata->export_count = 0;
            metadata->exports = NULL;
        }
        
        free(json_content);
        module_archive_destroy(archive);
        
        fprintf(stderr, "Successfully loaded metadata from archive: name=%s\n", metadata->name);
        return metadata;
    }
    
    // Check if path is a directory
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(module_json_path, sizeof(module_json_path), "%s/module.json", path);
    } else {
        // Assume path is the module.json file itself
        strncpy(module_json_path, path, sizeof(module_json_path) - 1);
    }
    
    fprintf(stderr, "Loading module.json from: %s\n", module_json_path);
    
    FILE* file = fopen(module_json_path, "r");
    if (!file) return NULL;
    
    // Read file
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);
    
    // Parse module.json
    ModuleMetadata* metadata = calloc(1, sizeof(ModuleMetadata));
    metadata->name = json_get_string(content, "name");
    metadata->version = json_get_string(content, "version");
    metadata->description = json_get_string(content, "description");
    metadata->type = json_get_string(content, "type");
    metadata->main_file = json_get_string(content, "main");
    
    // Extract directory path
    char* last_slash = strrchr(module_json_path, '/');
    if (last_slash) {
        size_t dir_len = last_slash - module_json_path;
        metadata->path = malloc(dir_len + 1);
        strncpy(metadata->path, module_json_path, dir_len);
        metadata->path[dir_len] = '\0';
    } else {
        metadata->path = strdup(".");
    }
    
    // Parse exports (simplified - in production use proper JSON parser)
    const char* exports_start = strstr(content, "\"exports\":");
    if (exports_start) {
        exports_start = strchr(exports_start, '{');
        if (exports_start) {
            // Count exports
            metadata->export_count = 0;
            const char* p = exports_start + 1;
            int brace_count = 1;
            bool in_string = false;
            
            while (*p && brace_count > 0) {
                if (!in_string) {
                    if (*p == '"') {
                        in_string = true;
                        // This might be an export name
                        const char* name_end = strchr(p + 1, '"');
                        if (name_end && *(name_end + 1) == ':') {
                            metadata->export_count++;
                        }
                    } else if (*p == '{') {
                        brace_count++;
                    } else if (*p == '}') {
                        brace_count--;
                    }
                } else if (*p == '"' && *(p - 1) != '\\') {
                    in_string = false;
                }
                p++;
            }
            
            // Allocate exports
            metadata->exports = calloc(metadata->export_count, sizeof(ModuleExport));
            
            // Parse each export (simplified)
            p = exports_start + 1;
            size_t export_idx = 0;
            while (*p && export_idx < metadata->export_count) {
                if (*p == '"') {
                    const char* name_start = p + 1;
                    const char* name_end = strchr(name_start, '"');
                    if (name_end && *(name_end + 1) == ':') {
                        // Found export name
                        size_t name_len = name_end - name_start;
                        metadata->exports[export_idx].name = malloc(name_len + 1);
                        strncpy(metadata->exports[export_idx].name, name_start, name_len);
                        metadata->exports[export_idx].name[name_len] = '\0';
                        
                        // Find export object
                        const char* obj_start = strchr(name_end + 2, '{');
                        if (obj_start) {
                            const char* obj_end = obj_start + 1;
                            int obj_brace_count = 1;
                            while (*obj_end && obj_brace_count > 0) {
                                if (*obj_end == '{') obj_brace_count++;
                                else if (*obj_end == '}') obj_brace_count--;
                                obj_end++;
                            }
                            
                            // Parse export object
                            size_t obj_len = obj_end - obj_start;
                            char* obj_str = malloc(obj_len + 1);
                            strncpy(obj_str, obj_start, obj_len);
                            obj_str[obj_len] = '\0';
                            
                            ModuleExport* parsed = parse_export(obj_str);
                            metadata->exports[export_idx].type = parsed->type;
                            metadata->exports[export_idx].signature = parsed->signature;
                            metadata->exports[export_idx].native_name = parsed->native_name;
                            metadata->exports[export_idx].constant_value = parsed->constant_value;
                            
                            free(parsed);
                            free(obj_str);
                            
                            export_idx++;
                            p = obj_end;
                            continue;
                        }
                    }
                }
                p++;
            }
        }
    }
    
    // Parse modules array (new multi-module support)
    const char* modules_start = strstr(content, "\"modules\":");
    if (modules_start) {
        // printf("Found modules array\n");
        modules_start = strchr(modules_start, '[');
        if (modules_start) {
            // Count modules
            metadata->module_count = 0;
            const char* p = modules_start + 1;
            int brace_count = 0;
            bool in_string = false;
            
            while (*p && (*p != ']' || brace_count > 0)) {
                if (!in_string) {
                    if (*p == '{') {
                        if (brace_count == 0) metadata->module_count++;
                        brace_count++;
                    } else if (*p == '}') {
                        brace_count--;
                    } else if (*p == '"') {
                        in_string = true;
                    }
                } else if (*p == '"' && *(p - 1) != '\\') {
                    in_string = false;
                }
                p++;
            }
            
            if (metadata->module_count > 0) {
                // Allocate modules
                metadata->modules = calloc(metadata->module_count, sizeof(ModuleDefinition));
                
                // Parse each module
                p = modules_start + 1;
                size_t module_idx = 0;
                while (*p && module_idx < metadata->module_count) {
                    // Skip whitespace
                    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',')) p++;
                    
                    if (*p == '{') {
                        // Find end of module object
                        const char* obj_start = p;
                        const char* obj_end = p + 1;
                        int obj_brace_count = 1;
                        while (*obj_end && obj_brace_count > 0) {
                            if (*obj_end == '{') obj_brace_count++;
                            else if (*obj_end == '}') obj_brace_count--;
                            obj_end++;
                        }
                        
                        // Parse module object
                        size_t obj_len = obj_end - obj_start;
                        char* obj_str = malloc(obj_len + 1);
                        strncpy(obj_str, obj_start, obj_len);
                        obj_str[obj_len] = '\0';
                        
                        ModuleDefinition* parsed = parse_module_definition(obj_str);
                        metadata->modules[module_idx] = *parsed;
                        free(parsed);
                        free(obj_str);
                        
                        module_idx++;
                        p = obj_end;
                    } else {
                        p++;
                    }
                }
            }
        }
    }
    
    // Parse native section (legacy single-module support)
    const char* native_start = strstr(content, "\"native\":");
    if (native_start && !modules_start) {
        // printf("Found native section\n");
        native_start = strchr(native_start, '{');
        if (native_start) {
            metadata->native.source = json_get_string(native_start, "source");
            metadata->native.header = json_get_string(native_start, "header");
            metadata->native.library = json_get_string(native_start, "library");
            // printf("Native library from section: %s\n", metadata->native.library ? metadata->native.library : "NULL");
        }
    } else if (!modules_start) {
        // printf("No native section found, checking top level\n");
        // Check for native_library at top level (legacy format)
        
        // Debug: print first 200 chars of JSON
        // printf("JSON content (first 200 chars): %.200s\n", content);
        
        metadata->native.library = json_get_string(content, "native_library");
        if (metadata->native.library) {
            // printf("Found native_library at top level: %s\n", metadata->native.library);
        } else {
            // printf("No native_library found at top level\n");
            
            // Try to find it manually
            const char* search = strstr(content, "\"native_library\"");
            if (search) {
                // printf("Found 'native_library' string at position %ld\n", search - content);
            } else {
                // printf("String 'native_library' not found in JSON\n");
            }
        }
    }
    
    // Check for compiled module in multiple locations
    char compiled_path[PATH_MAX];
    
    // First check in the module directory
    snprintf(compiled_path, sizeof(compiled_path), "%s/%s.swiftmodule", 
             metadata->path, metadata->name);
    if (access(compiled_path, F_OK) == 0) {
        metadata->compiled_path = strdup(compiled_path);
    } else {
        // Check in parent directory
        snprintf(compiled_path, sizeof(compiled_path), "%s/../%s.swiftmodule", 
                 metadata->path, metadata->name);
        if (access(compiled_path, F_OK) == 0) {
            metadata->compiled_path = strdup(compiled_path);
        } else {
            // Check in current directory
            snprintf(compiled_path, sizeof(compiled_path), "./%s.swiftmodule", metadata->name);
            if (access(compiled_path, F_OK) == 0) {
                metadata->compiled_path = strdup(compiled_path);
            }
        }
    }
    
    free(content);
    return metadata;
}

void package_free_module_metadata(ModuleMetadata* metadata) {
    if (!metadata) return;
    
    free(metadata->name);
    free(metadata->version);
    free(metadata->description);
    free(metadata->type);
    free(metadata->path);
    free(metadata->compiled_path);
    
    // Free modules array
    for (size_t i = 0; i < metadata->module_count; i++) {
        ModuleDefinition* mod = &metadata->modules[i];
        free(mod->name);
        free(mod->type);
        
        // Free sources
        for (size_t j = 0; j < mod->source_count; j++) {
            free(mod->sources[j]);
        }
        free(mod->sources);
        
        // Free main entries
        for (size_t j = 0; j < mod->main_count; j++) {
            free(mod->main[j]);
        }
        free(mod->main);
        
        // Free dependencies
        for (size_t j = 0; j < mod->dependency_count; j++) {
            free(mod->dependencies[j]);
        }
        free(mod->dependencies);
        
        // Free native info
        free(mod->native.source);
        free(mod->native.header);
        free(mod->native.library);
        
        free(mod->compiled_path);
    }
    free(metadata->modules);
    
    // Free exports
    for (size_t i = 0; i < metadata->export_count; i++) {
        free(metadata->exports[i].name);
        free(metadata->exports[i].signature);
        free(metadata->exports[i].native_name);
    }
    free(metadata->exports);
    
    // Free dependencies
    for (size_t i = 0; i < metadata->dependency_count; i++) {
        free(metadata->dependencies[i].name);
        free(metadata->dependencies[i].version);
        free(metadata->dependencies[i].path);
    }
    free(metadata->dependencies);
    
    // Free native info
    free(metadata->native.source);
    free(metadata->native.header);
    free(metadata->native.library);
    
    free(metadata);
}

PackageSystem* package_system_create(VM* vm) {
    PackageSystem* pkg_sys = calloc(1, sizeof(PackageSystem));
    pkg_sys->vm = vm;
    
    // Initialize cache
    pkg_sys->cache.capacity = 16;
    pkg_sys->cache.modules = calloc(pkg_sys->cache.capacity, sizeof(ModuleMetadata*));
    
    // Initialize dependencies map
    pkg_sys->dependencies.capacity = 32;
    pkg_sys->dependencies.names = calloc(pkg_sys->dependencies.capacity, sizeof(char*));
    pkg_sys->dependencies.paths = calloc(pkg_sys->dependencies.capacity, sizeof(char*));
    
    return pkg_sys;
}

void package_system_destroy(PackageSystem* pkg_sys) {
    if (!pkg_sys) return;
    
    // Free root module
    if (pkg_sys->root_module) {
        package_free_module_metadata(pkg_sys->root_module);
    }
    
    // Free cached modules
    for (size_t i = 0; i < pkg_sys->cache.count; i++) {
        package_free_module_metadata(pkg_sys->cache.modules[i]);
    }
    free(pkg_sys->cache.modules);
    
    // Free search paths
    for (size_t i = 0; i < pkg_sys->search_path_count; i++) {
        free(pkg_sys->search_paths[i]);
    }
    free(pkg_sys->search_paths);
    
    // Free dependencies
    for (size_t i = 0; i < pkg_sys->dependencies.count; i++) {
        free(pkg_sys->dependencies.names[i]);
        free(pkg_sys->dependencies.paths[i]);
    }
    free(pkg_sys->dependencies.names);
    free(pkg_sys->dependencies.paths);
    
    free(pkg_sys);
}

bool package_system_load_root(PackageSystem* pkg_sys, const char* path) {
    pkg_sys->root_module = package_load_module_metadata(path);
    if (!pkg_sys->root_module) return false;
    
    // Parse dependencies from root module.json
    FILE* file = fopen(path, "r");
    if (!file) return false;
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    fread(content, 1, size, file);
    content[size] = '\0';
    fclose(file);
    
    // Parse dependencies
    const char* deps_start = strstr(content, "\"dependencies\":");
    if (deps_start) {
        deps_start = strchr(deps_start, '{');
        if (deps_start) {
            const char* p = deps_start + 1;
            while (*p && *p != '}') {
                if (*p == '"') {
                    // Parse dependency name
                    const char* name_start = p + 1;
                    const char* name_end = strchr(name_start, '"');
                    if (name_end && *(name_end + 1) == ':') {
                        size_t name_len = name_end - name_start;
                        char* dep_name = malloc(name_len + 1);
                        strncpy(dep_name, name_start, name_len);
                        dep_name[name_len] = '\0';
                        
                        // Parse dependency path
                        const char* path_start = strchr(name_end + 2, '"');
                        if (path_start) {
                            path_start++;
                            const char* path_end = strchr(path_start, '"');
                            if (path_end) {
                                size_t path_len = path_end - path_start;
                                char* dep_path = malloc(path_len + 1);
                                strncpy(dep_path, path_start, path_len);
                                dep_path[path_len] = '\0';
                                
                                // Handle file: prefix
                                if (strncmp(dep_path, "file:", 5) == 0) {
                                    char* file_path = dep_path + 5;
                                    while (*file_path == '/') file_path++;
                                    
                                    // Add to dependencies
                                    if (pkg_sys->dependencies.count >= pkg_sys->dependencies.capacity) {
                                        pkg_sys->dependencies.capacity *= 2;
                                        pkg_sys->dependencies.names = realloc(pkg_sys->dependencies.names,
                                            pkg_sys->dependencies.capacity * sizeof(char*));
                                        pkg_sys->dependencies.paths = realloc(pkg_sys->dependencies.paths,
                                            pkg_sys->dependencies.capacity * sizeof(char*));
                                    }
                                    
                                    pkg_sys->dependencies.names[pkg_sys->dependencies.count] = dep_name;
                                    pkg_sys->dependencies.paths[pkg_sys->dependencies.count] = strdup(file_path);
                                    pkg_sys->dependencies.count++;
                                } else {
                                    free(dep_name);
                                }
                                
                                free(dep_path);
                                p = path_end + 1;
                                continue;
                            }
                        }
                        free(dep_name);
                    }
                }
                p++;
            }
        }
    }
    
    // Parse search paths
    const char* paths_start = strstr(content, "\"paths\":");
    if (paths_start) {
        const char* modules_start = strstr(paths_start, "\"modules\":");
        if (modules_start) {
            modules_start = strchr(modules_start, '[');
            if (modules_start) {
                // Count paths
                size_t path_count = 0;
                const char* p = modules_start + 1;
                while (*p && *p != ']') {
                    if (*p == '"') {
                        path_count++;
                        p = strchr(p + 1, '"');
                        if (!p) break;
                    }
                    p++;
                }
                
                // Allocate paths
                pkg_sys->search_paths = calloc(path_count, sizeof(char*));
                pkg_sys->search_path_count = 0;
                
                // Parse paths
                p = modules_start + 1;
                while (*p && *p != ']' && pkg_sys->search_path_count < path_count) {
                    if (*p == '"') {
                        const char* path_start = p + 1;
                        const char* path_end = strchr(path_start, '"');
                        if (path_end) {
                            size_t len = path_end - path_start;
                            char* path = malloc(len + 1);
                            strncpy(path, path_start, len);
                            path[len] = '\0';
                            
                            pkg_sys->search_paths[pkg_sys->search_path_count++] = expand_path(path);
                            free(path);
                            
                            p = path_end + 1;
                            continue;
                        }
                    }
                    p++;
                }
            }
        }
    }
    
    free(content);
    return true;
}

char* package_resolve_module_path(PackageSystem* pkg_sys, const char* module_name) {
    // Check dependencies first
    for (size_t i = 0; i < pkg_sys->dependencies.count; i++) {
        if (strcmp(pkg_sys->dependencies.names[i], module_name) == 0) {
            return strdup(pkg_sys->dependencies.paths[i]);
        }
    }
    
    // Check search paths
    for (size_t i = 0; i < pkg_sys->search_path_count; i++) {
        char path[PATH_MAX];
        
        // Try as directory with module.json
        snprintf(path, sizeof(path), "%s/%s/module.json", pkg_sys->search_paths[i], module_name);
        if (access(path, F_OK) == 0) {
            snprintf(path, sizeof(path), "%s/%s", pkg_sys->search_paths[i], module_name);
            return strdup(path);
        }
        
        // Try with dots replaced by slashes
        char converted_name[PATH_MAX];
        strncpy(converted_name, module_name, sizeof(converted_name) - 1);
        for (char* p = converted_name; *p; p++) {
            if (*p == '.') *p = '/';
        }
        
        snprintf(path, sizeof(path), "%s/%s/module.json", pkg_sys->search_paths[i], converted_name);
        if (access(path, F_OK) == 0) {
            snprintf(path, sizeof(path), "%s/%s", pkg_sys->search_paths[i], converted_name);
            return strdup(path);
        }
    }
    
    return NULL;
}

ModuleMetadata* package_get_module_metadata(PackageSystem* pkg_sys, const char* module_name) {
    // Check cache
    for (size_t i = 0; i < pkg_sys->cache.count; i++) {
        if (strcmp(pkg_sys->cache.modules[i]->name, module_name) == 0) {
            return pkg_sys->cache.modules[i];
        }
    }
    
    // Not in cache, try to load
    char* path = package_resolve_module_path(pkg_sys, module_name);
    if (!path) return NULL;
    
    ModuleMetadata* metadata = package_load_module_metadata(path);
    free(path);
    
    if (metadata) {
        package_cache_module_metadata(pkg_sys, metadata);
    }
    
    return metadata;
}

void package_cache_module_metadata(PackageSystem* pkg_sys, ModuleMetadata* metadata) {
    if (pkg_sys->cache.count >= pkg_sys->cache.capacity) {
        pkg_sys->cache.capacity *= 2;
        pkg_sys->cache.modules = realloc(pkg_sys->cache.modules,
                                        pkg_sys->cache.capacity * sizeof(ModuleMetadata*));
    }
    
    pkg_sys->cache.modules[pkg_sys->cache.count++] = metadata;
}

bool package_resolve_dependencies(PackageSystem* pkg_sys, ModuleMetadata* module) {
    // TODO: Implement dependency resolution
    // For now, dependencies are resolved at root level
    return true;
}

void* package_load_native_library(ModuleMetadata* module) {
    if (!module->native.library) return NULL;
    
    char lib_path[PATH_MAX];
    
    // Try absolute path first
    if (module->native.library[0] == '/') {
        strncpy(lib_path, module->native.library, sizeof(lib_path) - 1);
    } else {
        // Relative to module path
        snprintf(lib_path, sizeof(lib_path), "%s/%s", module->path, module->native.library);
    }
    
    // Add platform-specific extension if not present
    if (!strstr(lib_path, ".so") && !strstr(lib_path, ".dylib") && !strstr(lib_path, ".dll")) {
        #ifdef __APPLE__
        strcat(lib_path, ".dylib");
        #elif defined(_WIN32)
        strcat(lib_path, ".dll");
        #else
        strcat(lib_path, ".so");
        #endif
    }
    
    void* handle = dlopen(lib_path, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        fprintf(stderr, "Failed to load native library %s: %s\n", lib_path, dlerror());
    }
    
    return handle;
}

static void package_register_namespace(PackageSystem* pkg_sys, const char* namespace, const char* path) {
    // This functionality is now handled by dependencies in module.json
    // The namespace resolution happens in package_resolve_module_path
    // For backward compatibility, we can add to search paths
    if (path && path[0]) {
        // Add to search paths if not already there
        for (size_t i = 0; i < pkg_sys->search_path_count; i++) {
            if (strcmp(pkg_sys->search_paths[i], path) == 0) {
                return; // Already added
            }
        }
        
        // Grow array if needed
        if (!pkg_sys->search_paths) {
            pkg_sys->search_paths = calloc(4, sizeof(char*));
        }
        
        pkg_sys->search_paths = realloc(pkg_sys->search_paths, 
                                       (pkg_sys->search_path_count + 1) * sizeof(char*));
        pkg_sys->search_paths[pkg_sys->search_path_count++] = strdup(path);
    }
}

void package_init_stdlib_namespace(PackageSystem* pkg_sys) {
    // Register the "sys" namespace for stdlib modules
    package_register_namespace(pkg_sys, "sys", "./stdlib");
    
    // Also register builtin namespace for modules implemented directly in C
    package_register_namespace(pkg_sys, "builtin", "");
}

ModuleDefinition* package_find_module_definition(ModuleMetadata* metadata, const char* module_name) {
    if (!metadata || !module_name) return NULL;
    
    // Search in modules array
    for (size_t i = 0; i < metadata->module_count; i++) {
        if (metadata->modules[i].name && 
            strcmp(metadata->modules[i].name, module_name) == 0) {
            return &metadata->modules[i];
        }
    }
    
    return NULL;
}

Module* package_load_module_from_metadata(ModuleLoader* loader, ModuleMetadata* metadata, const char* module_name) {
    if (!loader || !metadata || !module_name) return NULL;
    
    // Find the specific module definition
    ModuleDefinition* mod_def = package_find_module_definition(metadata, module_name);
    if (!mod_def) {
        // If no specific module found, try to load the whole package
        if (metadata->module_count == 0) {
            // Legacy single-module package
            return load_module_from_metadata(loader, metadata);
        }
        return NULL;
    }
    
    // Create module
    Module* module = calloc(1, sizeof(Module));
    module->path = strdup(module_name);
    module->absolute_path = strdup(metadata->path);
    module->state = MODULE_STATE_LOADING;
    module->is_native = strcmp(mod_def->type, "native") == 0;
    
    // Initialize exports
    module->exports.capacity = 16;
    module->exports.names = calloc(module->exports.capacity, sizeof(char*));
    module->exports.values = calloc(module->exports.capacity, sizeof(TaggedValue));
    
    // Create module object
    module->module_object = object_create();
    
    if (module->is_native) {
        // Build source paths
        char** full_sources = malloc(mod_def->source_count * sizeof(char*));
        for (size_t i = 0; i < mod_def->source_count; i++) {
            full_sources[i] = malloc(PATH_MAX);
            snprintf(full_sources[i], PATH_MAX, "%s/%s", metadata->path, mod_def->sources[i]);
        }
        
        // Get expected output path
        char* compiled_lib = compiler_get_module_output_path(module_name, metadata->path);
        
        // Check if we need to compile
        if (compiler_needs_rebuild(compiled_lib, (const char**)full_sources, mod_def->source_count)) {
            fprintf(stderr, "Compiling native module: %s\n", module_name);
            
            // Create compiler options
            CompilerOptions* options = compiler_options_from_module(module_name, mod_def->type, 
                                                                   full_sources, mod_def->source_count);
            
            // Add additional include path for local headers
            options->include_dirs = realloc(options->include_dirs, 
                                          (options->include_count + 1) * sizeof(char*));
            options->include_dirs[options->include_count] = strdup(metadata->path);
            options->include_count++;
            
            // Compile the module
            if (!compiler_compile(options, metadata->path)) {
                fprintf(stderr, "Failed to compile native module %s\n", module_name);
                compiler_options_free(options);
                for (size_t i = 0; i < mod_def->source_count; i++) {
                    free(full_sources[i]);
                }
                free(full_sources);
                free(compiled_lib);
                module->state = MODULE_STATE_ERROR;
                return module;
            }
            
            compiler_options_free(options);
        }
        
        // Clean up source paths
        for (size_t i = 0; i < mod_def->source_count; i++) {
            free(full_sources[i]);
        }
        free(full_sources);
        
        // Load the compiled library
        module->native_handle = dlopen(compiled_lib, RTLD_LAZY);
        
        if (!module->native_handle) {
            // Try with explicit extension
            char lib_with_ext[PATH_MAX];
            snprintf(lib_with_ext, sizeof(lib_with_ext), "%s", compiled_lib);
            module->native_handle = dlopen(lib_with_ext, RTLD_LAZY);
        }
        
        free(compiled_lib);
        if (!module->native_handle) {
            fprintf(stderr, "Failed to load native library %s: %s\n", compiled_lib, dlerror());
            module->state = MODULE_STATE_ERROR;
            return module;
        }
        
        // Look for init function
        typedef bool (*ModuleInitFn)(Module*);
        
        // Try module-specific init function
        char init_fn_name[256];
        snprintf(init_fn_name, sizeof(init_fn_name), "swiftlang_");
        size_t pos = strlen(init_fn_name);
        for (const char* p = module_name; *p && pos < sizeof(init_fn_name) - 20; p++) {
            if (*p == '.' || *p == '/') {
                init_fn_name[pos++] = '_';
            } else {
                init_fn_name[pos++] = *p;
            }
        }
        strcpy(init_fn_name + pos, "_module_init");
        
        ModuleInitFn init_fn = (ModuleInitFn)dlsym(module->native_handle, init_fn_name);
        if (!init_fn) {
            fprintf(stderr, "Native module %s missing init function %s\n", module_name, init_fn_name);
            module->state = MODULE_STATE_ERROR;
            return module;
        }
        
        // Initialize the module
        if (!init_fn(module)) {
            fprintf(stderr, "Native module %s initialization failed\n", module_name);
            module->state = MODULE_STATE_ERROR;
            return module;
        }
    } else {
        // Load Swift module
        if (mod_def->source_count == 0) {
            fprintf(stderr, "No source files specified for module %s\n", module_name);
            module->state = MODULE_STATE_ERROR;
            return module;
        }
        
        // For simplicity, compile and run the module directly
        char src_path[PATH_MAX];
        if (mod_def->main_count > 0) {
            snprintf(src_path, sizeof(src_path), "%s/%s", metadata->path, mod_def->main[0]);
        } else {
            snprintf(src_path, sizeof(src_path), "%s/%s", metadata->path, mod_def->sources[0]);
        }
        
        // Check if source exists
        if (access(src_path, F_OK) != 0) {
            fprintf(stderr, "Source file not found: %s\n", src_path);
            module->state = MODULE_STATE_ERROR;
            return module;
        }
        
        // Read the source file
        FILE* file = fopen(src_path, "r");
        if (!file) {
            fprintf(stderr, "Failed to open source file: %s\n", src_path);
            module->state = MODULE_STATE_ERROR;
            return module;
        }
        
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        fseek(file, 0, SEEK_SET);
        
        char* source = malloc(file_size + 1);
        fread(source, 1, file_size, file);
        source[file_size] = '\0';
        fclose(file);
        
        // Parse the source
        Parser* parser = parser_create(source);
        ProgramNode* program = parser_parse_program(parser);
        
        if (parser->had_error) {
            fprintf(stderr, "Failed to parse module: %s\n", src_path);
            parser_destroy(parser);
            free(source);
            module->state = MODULE_STATE_ERROR;
            return module;
        }
        
        // Compile the module
        Chunk* chunk = malloc(sizeof(Chunk));
        chunk_init(chunk);
        
        if (!compile(program, chunk)) {
            fprintf(stderr, "Failed to compile module: %s\n", src_path);
            program_destroy(program);
            parser_destroy(parser);
            chunk_free(chunk);
            free(chunk);
            free(source);
            module->state = MODULE_STATE_ERROR;
            return module;
        }
        
        // Execute module in the context of the module loader
        VM* vm = loader->vm;
        
        // Save VM state
        Chunk* saved_chunk = vm->chunk;
        const char* saved_module_path = vm->current_module_path;
        
        // Set current module path for imports
        vm->current_module_path = src_path;
        
        // Define __module_exports__ for this module
        define_global(vm, "__module_exports__", OBJECT_VAL(module->module_object));
        // fprintf(stderr, "[DEBUG] Executing Swift module: %s\n", module_name);
        
        // Execute bytecode
        InterpretResult result = vm_interpret(vm, chunk);
        
        if (result != INTERPRET_OK) {
            fprintf(stderr, "Failed to execute module: %s\n", module_name);
            module->state = MODULE_STATE_ERROR;
        } else {
            fprintf(stderr, "[DEBUG] Successfully executed module: %s\n", module_name);
        }
        
        // Restore VM state
        vm->chunk = saved_chunk;
        vm->current_module_path = saved_module_path;
        undefine_global(vm, "__module_exports__");
        
        // Clean up
        program_destroy(program);
        parser_destroy(parser);
        chunk_free(chunk);
        free(chunk);
        free(source);
    }
    
    // Update module object with exports
    for (size_t i = 0; i < module->exports.count; i++) {
        object_set_property(module->module_object, module->exports.names[i], 
                          module->exports.values[i]);
    }
    
    module->state = MODULE_STATE_LOADED;
    return module;
}