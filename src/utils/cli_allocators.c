#include "utils/cli.h"
#include "utils/allocators.h"
#include "utils/logger.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "runtime/core/vm.h"
#include "runtime/modules/loader/module_loader.h"
#include "runtime/packages/package.h"
#include "runtime/modules/loader/module_compiler.h"
#include "runtime/modules/extensions/module_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <glob.h>
#include <errno.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#include "debug/debug.h"
#include "ast/ast_printer.h"
#include "utils/bytecode_format.h"

// External from cli.c
extern CLIConfig g_cli_config;
extern bool g_debug_tokens;
extern bool g_debug_ast;
extern bool g_debug_bytecode;
extern bool g_debug_trace;

// Modified cli_main that properly manages allocators
int cli_main_with_allocators(int argc, char* argv[]) {
    // Initialize logger
    logger_init();
    
    // Initialize module hooks system
    module_hooks_init();
    
    // Parse global options first
    cli_parse_args(argc, argv);
    
    // Check if no arguments provided
    if (optind >= argc) {
        cli_print_help(argv[0]);
        return 0;
    }
    
    // Get command
    const char* cmd_name = argv[optind];
    
    // Check if it's a file to run
    if (cli_file_exists(cmd_name) && strstr(cmd_name, ".swift")) {
        g_cli_config.input_file = cmd_name;
        return cli_run_file_with_allocators(cmd_name);
    }
    
    // Find command
    const Command* cmd = NULL;
    for (int i = 0; i < num_commands; i++) {
        if (strcmp(cmd_name, commands[i].name) == 0) {
            cmd = &commands[i];
            break;
        }
    }
    
    if (!cmd) {
        cli_print_error("Unknown command: %s", cmd_name);
        cli_print_info("Run '%s help' for usage information", argv[0]);
        return 1;
    }
    
    // Execute command with remaining arguments
    return cmd->handler(argc - optind, argv + optind);
}

// Modified run_file that manages allocator lifecycle
int cli_run_file_with_allocators(const char* path) {
    LOG_INFO(LOG_MODULE_CLI, "Running file: %s", path);
    
    // Use temporary allocator for file reading
    Allocator* temp = allocators_get(ALLOC_SYSTEM_TEMP);
    
    // Read file
    char* source = read_file(path);
    if (!source) return 1;
    
    // Update module path for imports
    char* abs_path = cli_resolve_path(path);
    if (abs_path) {
        char* dir = MEM_STRDUP(temp, abs_path);
        char* last_slash = strrchr(dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            // Add directory to module search paths
            int count = g_cli_config.module_path_count;
            g_cli_config.module_paths = MEM_REALLOC(temp, 
                                                    g_cli_config.module_paths,
                                                    count * sizeof(char*),
                                                    (count + 1) * sizeof(char*));
            g_cli_config.module_paths[count] = dir;
            g_cli_config.module_path_count++;
        }
        free(abs_path);
    }
    
    // Parse - parser and AST use their own allocators
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    
    if (parser->had_error) {
        cli_print_error("Parse error detected");
        parser_destroy(parser);
        free(source);
        allocators_reset_temp();  // Clean up temp allocations
        return 65;
    }
    
    // Print AST if requested
    if (g_cli_config.debug_ast) {
        printf("\n=== AST ===\n");
        ast_print_program(program);
        printf("\n");
    }
    
    // Compile - compiler uses its own allocator
    Chunk chunk;
    chunk_init(&chunk);
    
    if (!compile(program, &chunk)) {
        cli_print_error("Compilation error");
        // Don't need to free AST - arena will be reset
        parser_destroy(parser);
        free(source);
        allocators_reset_ast();      // Clean up AST allocations
        allocators_reset_compiler(); // Clean up compiler allocations
        allocators_reset_temp();     // Clean up temp allocations
        return 65;
    }
    
    // Reset AST arena - we're done with the AST
    allocators_reset_ast();
    
    // Debug: disassemble bytecode
    if (g_cli_config.debug_bytecode) {
        printf("\n=== Bytecode ===\n");
        disassemble_chunk(&chunk, path);
        printf("\n");
    }
    
    // Save bytecode if requested
    if (g_cli_config.emit_bytecode) {
        char bytecode_path[1024];
        snprintf(bytecode_path, sizeof(bytecode_path), "%s.bc", path);
        
        uint8_t* bytecode_data;
        size_t bytecode_size;
        if (bytecode_serialize(&chunk, &bytecode_data, &bytecode_size)) {
            FILE* f = fopen(bytecode_path, "wb");
            if (f) {
                fwrite(bytecode_data, 1, bytecode_size, f);
                fclose(f);
                cli_print_success("Bytecode saved to: %s", bytecode_path);
            } else {
                cli_print_error("Failed to save bytecode to: %s", bytecode_path);
            }
            // bytecode_data allocated with compiler allocator
        } else {
            cli_print_error("Failed to serialize bytecode");
        }
    }
    
    // Create VM and run
    VM* vm = vm_create();
    if (!vm) {
        cli_print_error("Failed to create VM");
        chunk_free(&chunk);
        parser_destroy(parser);
        free(source);
        allocators_reset_compiler();
        allocators_reset_temp();
        return 70;
    }
    
    // Enable debug trace if requested
    if (g_cli_config.debug_trace) {
        vm_set_debug_trace(vm, true);
    }
    
    // Create module loader if needed
    if (g_cli_config.module_path_count > 0) {
        ModuleLoader* loader = module_loader_create(vm);
        for (int i = 0; i < g_cli_config.module_path_count; i++) {
            module_loader_add_search_path(loader, g_cli_config.module_paths[i]);
        }
        vm_init_with_loader(vm, loader);
    }
    
    // Run the chunk
    InterpretResult result = vm_interpret(vm, &chunk);
    
    int exit_code = 0;
    switch (result) {
        case INTERPRET_OK:
            break;
        case INTERPRET_COMPILE_ERROR:
            exit_code = 65;
            break;
        case INTERPRET_RUNTIME_ERROR:
            exit_code = 70;
            break;
    }
    
    // Cleanup
    vm_destroy(vm);
    chunk_free(&chunk);
    parser_destroy(parser);
    free(source);
    
    // Reset allocators for next run
    allocators_reset_compiler();
    allocators_reset_temp();
    
    return exit_code;
}

// Modified REPL with proper allocator management
void cli_run_repl_with_allocators(void) {
    const int MAX_LINE = 1024;
    const int MAX_INPUT_SIZE = 8192;
    char line[MAX_LINE];
    char input[MAX_INPUT_SIZE];
    
    VM* vm = vm_create();
    if (!vm) {
        cli_print_error("Failed to create VM");
        return;
    }
    
    cli_print_banner();
    printf("SwiftLang REPL v0.1.0\n");
    printf("Type 'exit' or 'quit' to exit.\n\n");
    
    while (true) {
        printf("> ");
        input[0] = '\0';
        
        // Read potentially multi-line input
        while (true) {
            if (!fgets(line, sizeof(line), stdin)) {
                printf("\n");
                goto exit_repl;
            }
            
            // Append line to input buffer
            if (strlen(input) + strlen(line) >= MAX_INPUT_SIZE - 1) {
                cli_print_error("Input too long");
                input[0] = '\0';
                break;
            }
            strcat(input, line);
            
            // Check if we need more input
            if (!needs_more_input(input)) {
                break;
            }
            
            // Show continuation prompt
            printf("  ");
        }
        
        // Remove trailing newline
        size_t len = strlen(input);
        if (len > 0 && input[len - 1] == '\n') {
            input[len - 1] = '\0';
        }
        
        // Check for exit commands
        if (strcmp(input, "exit") == 0 || strcmp(input, "quit") == 0) {
            break;
        }
        
        // Skip empty input
        if (strlen(input) == 0) {
            continue;
        }
        
        LOG_DEBUG(LOG_MODULE_CLI, "REPL input: %s", input);
        
        // Parse
        Parser* parser = parser_create(input);
        ProgramNode* program = parser_parse_program(parser);
        
        if (!parser->had_error) {
            // Compile
            Chunk chunk;
            chunk_init(&chunk);
            
            if (compile(program, &chunk)) {
                // Debug: disassemble bytecode
                if (g_cli_config.debug_bytecode) {
                    printf("\n");
                    disassemble_chunk(&chunk, "code");
                    printf("\n");
                }
                
                // Run
                InterpretResult result = vm_interpret(vm, &chunk);
                
                if (result == INTERPRET_RUNTIME_ERROR) {
                    // Error already printed by VM
                }
                
            } else {
                cli_print_error("Compilation error");
            }
            
            chunk_free(&chunk);
        }
        
        parser_destroy(parser);
        
        // Reset temporary allocators after each REPL command
        allocators_reset_ast();
        allocators_reset_compiler();
        allocators_reset_temp();
    }
    
exit_repl:
    vm_destroy(vm);
}