#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <time.h>
#include "utils/compiler_wrapper.h"
#include "utils/platform_compat.h"

#ifdef _WIN32
    #include <windows.h>
    #define PATH_SEPARATOR "\\"
    #define LIB_EXTENSION ".dll"
    #define OBJ_EXTENSION ".obj"
#else
    #include <dlfcn.h>
    #define PATH_SEPARATOR "/"
    #ifdef __APPLE__
        #define LIB_EXTENSION ".dylib"
    #else
        #define LIB_EXTENSION ".so"
    #endif
    #define OBJ_EXTENSION ".o"
#endif

// Platform detection
PlatformType compiler_get_platform(void) {
#ifdef __APPLE__
    return PLATFORM_MACOS;
#elif defined(__linux__)
    return PLATFORM_LINUX;
#elif defined(_WIN32)
    return PLATFORM_WINDOWS;
#else
    return PLATFORM_UNKNOWN;
#endif
}

const char* compiler_get_platform_name(PlatformType platform) {
    switch (platform) {
        case PLATFORM_MACOS: return "macos";
        case PLATFORM_LINUX: return "linux";
        case PLATFORM_WINDOWS: return "windows";
        default: return "unknown";
    }
}

// Check if a command exists in PATH
static bool command_exists(const char* cmd) {
    char* path_env = getenv("PATH");
    if (!path_env) return false;
    
    char* path_copy = strdup(path_env);
    char* dir = strtok(path_copy, ":");
    
    while (dir) {
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
        
        if (access(full_path, X_OK) == 0) {
            free(path_copy);
            return true;
        }
        
        dir = strtok(NULL, ":");
    }
    
    free(path_copy);
    return false;
}

// Get compiler version
static char* get_compiler_version(const char* compiler_cmd) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "%s --version 2>&1", compiler_cmd);
    
    FILE* pipe = popen(cmd, "r");
    if (!pipe) return strdup("unknown");
    
    char buffer[256];
    if (fgets(buffer, sizeof(buffer), pipe)) {
        pclose(pipe);
        // Remove newline
        buffer[strcspn(buffer, "\n")] = 0;
        return strdup(buffer);
    }
    
    pclose(pipe);
    return strdup("unknown");
}

// Detect available compiler
CompilerInfo* compiler_detect(void) {
    CompilerInfo* info = malloc(sizeof(CompilerInfo));
    
    // Check for compilers in order of preference
    if (command_exists("clang")) {
        info->type = COMPILER_CLANG;
        info->path = strdup("clang");
        info->version = get_compiler_version("clang");
    } else if (command_exists("gcc")) {
        info->type = COMPILER_GCC;
        info->path = strdup("gcc");
        info->version = get_compiler_version("gcc");
    } else if (command_exists("cl")) {
        info->type = COMPILER_MSVC;
        info->path = strdup("cl");
        info->version = get_compiler_version("cl");
    } else {
        info->type = COMPILER_UNKNOWN;
        info->path = NULL;
        info->version = strdup("unknown");
    }
    
    return info;
}

void compiler_info_free(CompilerInfo* info) {
    if (!info) return;
    free(info->path);
    free(info->version);
    free(info);
}

// Build command line for different compilers
static char* build_command_line(const CompilerInfo* compiler, const CompilerOptions* options, const char* output_dir) {
    size_t cmd_size = 4096;
    char* cmd = malloc(cmd_size);
    cmd[0] = '\0';
    
    // Start with compiler
    strcat(cmd, compiler->path);
    
    // Platform-specific flags
    PlatformType platform = compiler_get_platform();
    
    switch (compiler->type) {
        case COMPILER_CLANG:
        case COMPILER_GCC:
            // Common flags
            strcat(cmd, " -fPIC");
            
            if (options->is_shared) {
                strcat(cmd, " -shared");
                if (platform == PLATFORM_MACOS) {
                    strcat(cmd, " -dynamiclib");
                }
            }
            
            if (options->debug) {
                strcat(cmd, " -g -O0");
            } else {
                strcat(cmd, " -O2");
            }
            
            // Include directories
            for (size_t i = 0; i < options->include_count; i++) {
                strcat(cmd, " -I");
                strcat(cmd, options->include_dirs[i]);
            }
            
            // Defines
            for (size_t i = 0; i < options->define_count; i++) {
                strcat(cmd, " -D");
                strcat(cmd, options->defines[i]);
            }
            
            // Source files
            for (size_t i = 0; i < options->source_count; i++) {
                strcat(cmd, " ");
                strcat(cmd, options->sources[i]);
            }
            
            // Output
            strcat(cmd, " -o ");
            strcat(cmd, output_dir);
            strcat(cmd, PATH_SEPARATOR);
            strcat(cmd, options->output_name);
            strcat(cmd, LIB_EXTENSION);
            
            // Library directories
            for (size_t i = 0; i < options->library_dir_count; i++) {
                strcat(cmd, " -L");
                strcat(cmd, options->library_dirs[i]);
            }
            
            // Libraries
            for (size_t i = 0; i < options->library_count; i++) {
                strcat(cmd, " -l");
                strcat(cmd, options->libraries[i]);
            }
            
            // Extra flags
            for (size_t i = 0; i < options->extra_flag_count; i++) {
                strcat(cmd, " ");
                strcat(cmd, options->extra_flags[i]);
            }
            
            // Platform-specific linking flags
            if (platform == PLATFORM_MACOS) {
                strcat(cmd, " -undefined dynamic_lookup");
            }
            
            break;
            
        case COMPILER_MSVC:
            // MSVC flags
            if (options->is_shared) {
                strcat(cmd, " /LD");
            }
            
            if (options->debug) {
                strcat(cmd, " /Od /MDd /Zi");
            } else {
                strcat(cmd, " /O2 /MD");
            }
            
            // Include directories
            for (size_t i = 0; i < options->include_count; i++) {
                strcat(cmd, " /I");
                strcat(cmd, options->include_dirs[i]);
            }
            
            // Defines
            for (size_t i = 0; i < options->define_count; i++) {
                strcat(cmd, " /D");
                strcat(cmd, options->defines[i]);
            }
            
            // Source files
            for (size_t i = 0; i < options->source_count; i++) {
                strcat(cmd, " ");
                strcat(cmd, options->sources[i]);
            }
            
            // Output
            strcat(cmd, " /Fe:");
            strcat(cmd, output_dir);
            strcat(cmd, PATH_SEPARATOR);
            strcat(cmd, options->output_name);
            strcat(cmd, LIB_EXTENSION);
            
            // Libraries
            for (size_t i = 0; i < options->library_count; i++) {
                strcat(cmd, " ");
                strcat(cmd, options->libraries[i]);
                strcat(cmd, ".lib");
            }
            
            break;
            
        default:
            free(cmd);
            return NULL;
    }
    
    return cmd;
}

// Get file modification time
static time_t get_file_mtime(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) {
        return 0;
    }
    return st.st_mtime;
}

// Check if native module needs recompilation
bool compiler_needs_rebuild(const char* module_path, const char** sources, size_t source_count) {
    time_t module_time = get_file_mtime(module_path);
    if (module_time == 0) {
        // Module doesn't exist, needs build
        return true;
    }
    
    // Check if any source is newer than the module
    for (size_t i = 0; i < source_count; i++) {
        time_t source_time = get_file_mtime(sources[i]);
        if (source_time > module_time) {
            return true;
        }
    }
    
    return false;
}

// Compile with detected compiler
bool compiler_compile(const CompilerOptions* options, const char* output_dir) {
    CompilerInfo* compiler = compiler_detect();
    if (compiler->type == COMPILER_UNKNOWN) {
        fprintf(stderr, "Error: No compatible C compiler found\n");
        compiler_info_free(compiler);
        return false;
    }
    
    fprintf(stderr, "Using compiler: %s\n", compiler->version);
    
    // Create output directory if it doesn't exist
    struct stat st;
    if (stat(output_dir, &st) != 0) {
        #ifdef _WIN32
            CreateDirectory(output_dir, NULL);
        #else
            mkdir(output_dir, 0755);
        #endif
    }
    
    // Build command line
    char* cmd = build_command_line(compiler, options, output_dir);
    if (!cmd) {
        fprintf(stderr, "Error: Failed to build command line\n");
        compiler_info_free(compiler);
        return false;
    }
    
    fprintf(stderr, "Compiling: %s\n", cmd);
    
    // Execute compilation
    int result = system(cmd);
    
    free(cmd);
    compiler_info_free(compiler);
    
    if (result != 0) {
        fprintf(stderr, "Compilation failed with code: %d\n", result);
        return false;
    }
    
    return true;
}

// Create compiler options from module metadata
CompilerOptions* compiler_options_from_module(const char* module_name, 
                                               const char* module_type,
                                               char** sources, 
                                               size_t source_count) {
    CompilerOptions* options = calloc(1, sizeof(CompilerOptions));
    
    options->sources = sources;
    options->source_count = source_count;
    
    // Add include directories
    options->include_dirs = malloc(sizeof(char*) * 2);
    options->include_dirs[0] = strdup("../../include");  // SwiftLang headers
    options->include_dirs[1] = strdup("include");        // Module local headers
    options->include_count = 2;
    
    // Set output name (remove .native suffix if present)
    char* name_copy = strdup(module_name);
    char* native_suffix = strstr(name_copy, ".native");
    if (native_suffix) {
        *native_suffix = '\0';
    }
    options->output_name = name_copy;
    
    // Always build shared libraries for modules
    options->is_shared = true;
    
    // Platform-specific settings
    PlatformType platform = compiler_get_platform();
    
    // Add standard libraries based on module name
    if (strstr(module_name, "math")) {
        options->libraries = malloc(sizeof(char*));
        options->libraries[0] = strdup("m");
        options->library_count = 1;
    }
    
    // Add platform-specific defines
    options->defines = malloc(sizeof(char*) * 2);
    options->defines[0] = strdup("SWIFTLANG_MODULE");
    options->define_count = 1;
    
    if (platform == PLATFORM_MACOS) {
        options->defines[1] = strdup("GL_SILENCE_DEPRECATION");
        options->define_count = 2;
    }
    
    return options;
}

void compiler_options_free(CompilerOptions* options) {
    if (!options) return;
    
    // Free include directories
    for (size_t i = 0; i < options->include_count; i++) {
        free(options->include_dirs[i]);
    }
    free(options->include_dirs);
    
    // Free libraries
    for (size_t i = 0; i < options->library_count; i++) {
        free(options->libraries[i]);
    }
    free(options->libraries);
    
    // Free library directories
    for (size_t i = 0; i < options->library_dir_count; i++) {
        free(options->library_dirs[i]);
    }
    free(options->library_dirs);
    
    // Free defines
    for (size_t i = 0; i < options->define_count; i++) {
        free(options->defines[i]);
    }
    free(options->defines);
    
    // Free extra flags
    for (size_t i = 0; i < options->extra_flag_count; i++) {
        free(options->extra_flags[i]);
    }
    free(options->extra_flags);
    
    free(options->output_name);
    free(options);
}

// Get the expected output path for a module
char* compiler_get_module_output_path(const char* module_name, const char* output_dir) {
    // Remove .native suffix if present
    char* name_copy = strdup(module_name);
    char* native_suffix = strstr(name_copy, ".native");
    if (native_suffix) {
        *native_suffix = '\0';
    }
    
    size_t path_len = strlen(output_dir) + strlen(PATH_SEPARATOR) + 
                      strlen(name_copy) + strlen(LIB_EXTENSION) + 1;
    char* path = malloc(path_len);
    
    snprintf(path, path_len, "%s%s%s%s", output_dir, PATH_SEPARATOR, name_copy, LIB_EXTENSION);
    
    free(name_copy);
    return path;
}