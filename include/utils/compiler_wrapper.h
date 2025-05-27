#ifndef COMPILER_WRAPPER_H
#define COMPILER_WRAPPER_H

#include <stdbool.h>

typedef enum {
    COMPILER_UNKNOWN,
    COMPILER_CLANG,
    COMPILER_GCC,
    COMPILER_MSVC,
    COMPILER_MINGW
} CompilerType;

typedef enum {
    PLATFORM_UNKNOWN,
    PLATFORM_MACOS,
    PLATFORM_LINUX,
    PLATFORM_WINDOWS
} PlatformType;

typedef struct {
    CompilerType type;
    char* path;
    char* version;
} CompilerInfo;

typedef struct {
    char** sources;           // Source files to compile
    size_t source_count;
    char** include_dirs;      // Include directories
    size_t include_count;
    char** libraries;         // Libraries to link
    size_t library_count;
    char** library_dirs;      // Library directories
    size_t library_dir_count;
    char** defines;           // Preprocessor defines
    size_t define_count;
    char* output_name;        // Output file name
    bool is_shared;           // Build shared library
    bool debug;               // Debug build
    char** extra_flags;       // Extra compiler flags
    size_t extra_flag_count;
} CompilerOptions;

// Platform detection
PlatformType compiler_get_platform(void);
const char* compiler_get_platform_name(PlatformType platform);

// Compiler detection
CompilerInfo* compiler_detect(void);
void compiler_info_free(CompilerInfo* info);

// Compilation
bool compiler_compile(const CompilerOptions* options, const char* output_dir);

// Helper to build options from module metadata
CompilerOptions* compiler_options_from_module(const char* module_name, 
                                               const char* module_type,
                                               char** sources, 
                                               size_t source_count);
void compiler_options_free(CompilerOptions* options);

// Check if native module needs recompilation
bool compiler_needs_rebuild(const char* module_path, const char** sources, size_t source_count);

// Get the expected output path for a module
char* compiler_get_module_output_path(const char* module_name, const char* output_dir);

#endif // COMPILER_WRAPPER_H