#include "runtime/modules/loader/module_compiler.h"
#include "runtime/modules/formats/module_format.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "runtime/core/vm.h"
#include "utils/bytecode_format.h"
#include "utils/platform_compat.h"
#include "utils/platform_dir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>

struct ModuleCompiler {
    char error_message[1024];
    VM* vm; // For compilation context
};

ModuleCompiler* module_compiler_create(void) {
    ModuleCompiler* compiler = calloc(1, sizeof(ModuleCompiler));
    compiler->vm = vm_create();
    return compiler;
}

void module_compiler_destroy(ModuleCompiler* compiler) {
    if (!compiler) return;
    if (compiler->vm) {
        vm_destroy(compiler->vm);
    }
    free(compiler);
}

static void set_error(ModuleCompiler* compiler, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(compiler->error_message, sizeof(compiler->error_message), fmt, args);
    va_end(args);
}

bool module_compiler_compile_file(ModuleCompiler* compiler,
                                const char* source_path,
                                uint8_t** bytecode,
                                size_t* bytecode_size) {
    // Read source file
    FILE* f = fopen(source_path, "r");
    if (!f) {
        set_error(compiler, "Failed to open source file: %s", source_path);
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    size_t source_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(source_size + 1);
    fread(source, 1, source_size, f);
    source[source_size] = '\0';
    fclose(f);
    
    // Parse the source
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    
    if (parser->had_error) {
        set_error(compiler, "Parse error in %s", source_path);
        parser_destroy(parser);
        free(source);
        return false;
    }
    
    // Compile to bytecode
    Chunk* chunk = malloc(sizeof(Chunk));
    chunk_init(chunk);
    
    // For module compilation, we need a dummy module context
    // The actual module will be created when loading
    if (!compile(program, chunk)) {
        set_error(compiler, "Compilation error in %s", source_path);
        chunk_free(chunk);
        free(chunk);
        parser_destroy(parser);
        free(source);
        return false;
    }
    
    // Use the new bytecode serialization
    bool success = bytecode_serialize(chunk, bytecode, bytecode_size);
    
    // Clean up
    chunk_free(chunk);
    free(chunk);
    parser_destroy(parser);
    free(source);
    
    if (!success) {
        set_error(compiler, "Failed to serialize bytecode");
        return false;
    }
    
    return true;
}

static bool compile_directory(ModuleCompiler* compiler, 
                            const char* dir_path,
                            ModuleArchive* archive,
                            const char* base_path) {
    platform_dir_t* dir = platform_opendir(dir_path);
    if (!dir) {
        set_error(compiler, "Failed to open directory: %s", dir_path);
        return false;
    }
    
    platform_dirent_t entry;
    while (platform_readdir(dir, &entry)) {
        const char* entry_name = entry.name;
        if (entry_name[0] == '.') continue;
        
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry_name);
        
        struct stat st;
        if (stat(full_path, &st) != 0) continue;
        
        if (S_ISDIR(st.st_mode)) {
            // Recursively compile subdirectory
            if (!compile_directory(compiler, full_path, archive, base_path)) {
                platform_closedir(dir);
                return false;
            }
        } else if (strstr(entry_name, ".swift") && !strstr(entry_name, ".swiftmodule")) {
            // Compile Swift file
            uint8_t* bytecode;
            size_t bytecode_size;
            
            if (!module_compiler_compile_file(compiler, full_path, &bytecode, &bytecode_size)) {
                platform_closedir(dir);
                return false;
            }
            
            // Calculate module name from path
            char module_name[512];
            const char* rel_path = full_path + strlen(base_path);
            if (*rel_path == '/') rel_path++;
            
            strncpy(module_name, rel_path, sizeof(module_name) - 1);
            char* ext = strstr(module_name, ".swift");
            if (ext) *ext = '\0';
            
            // Replace path separators with dots
            for (char* p = module_name; *p; p++) {
                if (*p == '/') *p = '.';
            }
            
            // Add to archive
            module_archive_add_bytecode(archive, module_name, bytecode, bytecode_size);
            free(bytecode);
        }
    }
    
    platform_closedir(dir);
    return true;
}

bool module_compiler_compile(ModuleCompiler* compiler, 
                           const char* module_json_path,
                           const char* output_path,
                           ModuleCompilerOptions* options) {
    // Load module metadata
    ModuleMetadata* metadata = package_load_module_metadata(module_json_path);
    if (!metadata) {
        set_error(compiler, "Failed to load module metadata from %s", module_json_path);
        return false;
    }
    
    bool result = module_compiler_build_package(compiler, metadata, output_path, options);
    
    package_free_module_metadata(metadata);
    return result;
}

bool module_compiler_build_package(ModuleCompiler* compiler,
                                 ModuleMetadata* metadata,
                                 const char* output_path,
                                 ModuleCompilerOptions* options) {
    // Create archive
    ModuleArchive* archive = module_archive_create(output_path);
    
    // Add module.json
    char module_json_path[1024];
    
    // metadata->path might already include the module.json filename
    if (strstr(metadata->path, "module.json")) {
        // Path is the module.json file itself
        strncpy(module_json_path, metadata->path, sizeof(module_json_path) - 1);
        
        // Update metadata->path to be the directory
        char* dir_path = strdup(metadata->path);
        char* last_slash = strrchr(dir_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            free(metadata->path);
            metadata->path = dir_path;
        } else {
            free(dir_path);
        }
    } else {
        // Path is a directory
        snprintf(module_json_path, sizeof(module_json_path), "%s/module.json", metadata->path);
    }
    
    FILE* f = fopen(module_json_path, "r");
    if (!f) {
        set_error(compiler, "Failed to open module.json");
        module_archive_destroy(archive);
        return false;
    }
    
    fseek(f, 0, SEEK_END);
    size_t json_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* json_content = malloc(json_size + 1);
    fread(json_content, 1, json_size, f);
    json_content[json_size] = '\0';
    fclose(f);
    
    module_archive_add_json(archive, json_content);
    free(json_content);
    
    // Compile source files
    char src_path[1024];
    snprintf(src_path, sizeof(src_path), "%s/src", metadata->path);
    
    struct stat st;
    if (stat(src_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        if (!compile_directory(compiler, src_path, archive, src_path)) {
            module_archive_destroy(archive);
            return false;
        }
    } else {
        // Try compiling files in module root
        if (!compile_directory(compiler, metadata->path, archive, metadata->path)) {
            module_archive_destroy(archive);
            return false;
        }
    }
    
    // Add native library if present
    if (metadata->native.library) {
        char lib_path[1024];
        snprintf(lib_path, sizeof(lib_path), "%s/%s", metadata->path, metadata->native.library);
        
        // printf("Checking for native library: %s\n", lib_path);
        if (stat(lib_path, &st) == 0) {
            const char* platform = module_archive_get_platform();
            // printf("Adding native library to archive: %s (platform: %s)\n", lib_path, platform);
            if (!module_archive_add_native_lib(archive, lib_path, platform)) {
                // printf("Failed to add native library to archive\n");
            }
        } else {
            // printf("Native library not found at: %s\n", lib_path);
        }
    } else {
        // printf("No native library specified in metadata\n");
    }
    
    // Include source files if requested
    if (options && options->include_source) {
        // Add source files to archive
        platform_dir_t* dir = platform_opendir(metadata->path);
        if (dir) {
            platform_dirent_t entry;
            while (platform_readdir(dir, &entry)) {
                const char* entry_name = entry.name;
                if (strstr(entry_name, ".swift")) {
                    char file_path[1024];
                    char archive_path[512];
                    snprintf(file_path, sizeof(file_path), "%s/%s", metadata->path, entry_name);
                    snprintf(archive_path, sizeof(archive_path), "source/%s", entry_name);
                    module_archive_add_file(archive, file_path, archive_path);
                }
            }
            platform_closedir(dir);
        }
    }
    
    // Write archive
    if (!module_archive_write(archive)) {
        set_error(compiler, "Failed to write module archive");
        module_archive_destroy(archive);
        return false;
    }
    
    module_archive_destroy(archive);
    return true;
}

const char* module_compiler_get_error(ModuleCompiler* compiler) {
    return compiler ? compiler->error_message : "No compiler instance";
}