#ifndef LANG_MODULE_COMPILER_H
#define LANG_MODULE_COMPILER_H

#include <stdbool.h>
#include <stddef.h>
#include "runtime/module_archive.h"
#include "runtime/package.h"

typedef struct ModuleCompiler ModuleCompiler;

// Module compilation options
typedef struct {
    bool optimize;           // Enable optimizations
    bool strip_debug;        // Strip debug information
    bool include_source;     // Include source files in archive
    const char* output_dir;  // Output directory for temporary files
} ModuleCompilerOptions;

// Create a module compiler
ModuleCompiler* module_compiler_create(void);
void module_compiler_destroy(ModuleCompiler* compiler);

// Compile a module from its manifest
bool module_compiler_compile(ModuleCompiler* compiler, 
                           const char* module_json_path,
                           const char* output_path,
                           ModuleCompilerOptions* options);

// Compile individual source files
bool module_compiler_compile_file(ModuleCompiler* compiler,
                                const char* source_path,
                                uint8_t** bytecode,
                                size_t* bytecode_size);

// Build a complete module package
bool module_compiler_build_package(ModuleCompiler* compiler,
                                 ModuleMetadata* metadata,
                                 const char* output_path,
                                 ModuleCompilerOptions* options);

// Get last error message
const char* module_compiler_get_error(ModuleCompiler* compiler);

#endif // LANG_MODULE_COMPILER_H