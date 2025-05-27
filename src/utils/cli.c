#include "utils/cli.h"
#include "utils/logger.h"
#include "lexer/lexer.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "vm/vm.h"
#include "runtime/module.h"
#include "runtime/package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <getopt.h>
#include <glob.h>
#include <errno.h>

#include "debug/debug.h"
// #include "ast/ast_printer.h" // TODO: Fix AST printer

// Global CLI configuration
CLIConfig g_cli_config = {
    .log_level = LOG_LEVEL_INFO,
    .log_modules = LOG_MODULE_ALL,
    .log_colors = true,
    .stack_size = 256 * 1024,  // 256KB default stack
    .heap_size = 16 * 1024 * 1024,  // 16MB default heap
    .jobs = 1,
    .format = "zip"
};

// Global debug flags (for backward compatibility)
bool g_debug_tokens = false;
bool g_debug_ast = false;
bool g_debug_bytecode = false;
bool g_debug_trace = false;

// Command definitions
static const Command commands[] = {
    {
        .name = "init",
        .description = "Initialize a new SwiftLang project",
        .handler = cli_cmd_init,
        .usage = "init [options]",
        .help = "Initialize a new project in the current directory.\n"
                "Options:\n"
                "  --name <name>    Project name (default: directory name)\n"
                "  --type <type>    Project type: app, lib, or module (default: app)"
    },
    {
        .name = "new",
        .description = "Create a new SwiftLang project",
        .handler = cli_cmd_new,
        .usage = "new <name> [options]",
        .help = "Create a new project in a new directory.\n"
                "Options:\n"
                "  --type <type>    Project type: app, lib, or module (default: app)\n"
                "  --template <t>   Use a project template"
    },
    {
        .name = "build",
        .description = "Build the current project",
        .handler = cli_cmd_build,
        .usage = "build [options]",
        .help = "Build the project in the current directory.\n"
                "Options:\n"
                "  --output <dir>   Output directory (default: build/)\n"
                "  --optimize       Enable optimizations\n"
                "  --emit-bytecode  Save bytecode files\n"
                "  --jobs <n>       Number of parallel jobs"
    },
    {
        .name = "run",
        .description = "Run a SwiftLang file or project",
        .handler = cli_cmd_run,
        .usage = "run [file] [options]",
        .help = "Run a SwiftLang file or the current project.\n"
                "Options:\n"
                "  --args <args>    Arguments to pass to the program\n"
                "  --watch          Watch for file changes and re-run"
    },
    {
        .name = "test",
        .description = "Run tests",
        .handler = cli_cmd_test,
        .usage = "test [options]",
        .help = "Run all tests in the project.\n"
                "Options:\n"
                "  --filter <pat>   Only run tests matching pattern\n"
                "  --verbose        Show detailed test output"
    },
    {
        .name = "repl",
        .description = "Start interactive REPL",
        .handler = cli_cmd_repl,
        .usage = "repl [options]",
        .help = "Start an interactive SwiftLang REPL session.\n"
                "Options:\n"
                "  --history <file> History file (default: ~/.swiftlang_history)"
    },
    {
        .name = "help",
        .description = "Show help information",
        .handler = cli_cmd_help,
        .usage = "help [command]",
        .help = "Show help for a specific command or general help."
    },
    {
        .name = "version",
        .description = "Show version information",
        .handler = cli_cmd_version,
        .usage = "version",
        .help = "Display SwiftLang version and build information."
    }
};

static const int num_commands = sizeof(commands) / sizeof(commands[0]);

// Long options for getopt
static struct option long_options[] = {
    // Debug options
    {"debug-tokens", no_argument, 0, 0},
    {"debug-ast", no_argument, 0, 0},
    {"debug-bytecode", no_argument, 0, 0},
    {"debug-trace", no_argument, 0, 0},
    {"debug-all", no_argument, 0, 0},
    
    // Logging options
    {"log-level", required_argument, 0, 0},
    {"log-modules", required_argument, 0, 0},
    {"log-file", required_argument, 0, 0},
    {"no-colors", no_argument, 0, 0},
    {"timestamps", no_argument, 0, 0},
    {"source-location", no_argument, 0, 0},
    
    // Build options
    {"output", required_argument, 0, 'o'},
    {"build-dir", required_argument, 0, 'b'},
    {"optimize", no_argument, 0, 'O'},
    {"emit-bytecode", no_argument, 0, 0},
    {"emit-ast", no_argument, 0, 0},
    {"format", required_argument, 0, 0},
    {"jobs", required_argument, 0, 'j'},
    
    // Module options
    {"module-path", required_argument, 0, 'M'},
    
    // Runtime options
    {"stack-size", required_argument, 0, 0},
    {"heap-size", required_argument, 0, 0},
    {"gc-stress", no_argument, 0, 0},
    
    // Other options
    {"quiet", no_argument, 0, 'q'},
    {"verbose", no_argument, 0, 'v'},
    {"help", no_argument, 0, 'h'},
    {"version", no_argument, 0, 'V'},
    {"watch", no_argument, 0, 'w'},
    
    {0, 0, 0, 0}
};

void cli_print_banner(void) {
    if (g_cli_config.quiet) return;
    
    printf("\n");
    printf(COLOR_CYAN "┌─────────────────────────────────────────┐\n" COLOR_RESET);
    printf(COLOR_CYAN "│ " COLOR_BOLD COLOR_WHITE "SwiftLang Programming Language" COLOR_RESET COLOR_CYAN "         │\n" COLOR_RESET);
    printf(COLOR_CYAN "│ " COLOR_RESET "A modern, fast, and expressive language" COLOR_CYAN " │\n" COLOR_RESET);
    printf(COLOR_CYAN "└─────────────────────────────────────────┘\n" COLOR_RESET);
    printf("\n");
}

void cli_print_version(void) {
    printf("SwiftLang version 0.1.0\n");
    printf("Copyright (c) 2024 SwiftLang Contributors\n");
    printf("\n");
    printf("Build info:\n");
    printf("  Compiler: %s\n", __VERSION__);
    printf("  Date: %s %s\n", __DATE__, __TIME__);
#ifdef DEBUG
    printf("  Type: Debug\n");
#else
    printf("  Type: Release\n");
#endif
}

void cli_print_help(const char* program_name) {
    cli_print_banner();
    
    printf("Usage: %s [command] [options] [file]\n\n", program_name);
    
    printf(COLOR_BOLD "Commands:\n" COLOR_RESET);
    for (int i = 0; i < num_commands; i++) {
        printf("  %-12s  %s\n", commands[i].name, commands[i].description);
    }
    printf("\n");
    
    printf(COLOR_BOLD "Common Options:\n" COLOR_RESET);
    printf("  -h, --help              Show help message\n");
    printf("  -V, --version           Show version information\n");
    printf("  -v, --verbose           Enable verbose output\n");
    printf("  -q, --quiet             Suppress non-error output\n");
    printf("\n");
    
    printf(COLOR_BOLD "Debug Options:\n" COLOR_RESET);
    printf("  --debug-tokens          Print tokens during lexing\n");
    printf("  --debug-ast             Print AST after parsing\n");
    printf("  --debug-bytecode        Print bytecode after compilation\n");
    printf("  --debug-trace           Trace execution\n");
    printf("  --debug-all             Enable all debug output\n");
    printf("\n");
    
    printf(COLOR_BOLD "Logging Options:\n" COLOR_RESET);
    printf("  --log-level <level>     Set log level (trace/debug/info/warn/error)\n");
    printf("  --log-modules <mods>    Enable specific modules (comma-separated):\n");
    printf("                          lexer,parser,compiler,vm,module,bootstrap,\n");
    printf("                          memory,gc,optimizer,bytecode,stdlib,package\n");
    printf("  --log-file <file>       Write logs to file\n");
    printf("  --no-colors             Disable colored output\n");
    printf("  --timestamps            Show timestamps in logs\n");
    printf("  --source-location       Show source file and line in logs\n");
    printf("\n");
    
    printf(COLOR_BOLD "Build Options:\n" COLOR_RESET);
    printf("  -o, --output <dir>      Output directory\n");
    printf("  -b, --build-dir <dir>   Build directory for intermediate files\n");
    printf("  -O, --optimize          Enable optimizations\n");
    printf("  -j, --jobs <n>          Number of parallel jobs\n");
    printf("  -M, --module-path <dir> Add module search path\n");
    printf("\n");
    
    printf(COLOR_BOLD "Examples:\n" COLOR_RESET);
    printf("  %s init                           # Initialize project\n", program_name);
    printf("  %s new myapp                      # Create new project\n", program_name);
    printf("  %s build -O                       # Build with optimizations\n", program_name);
    printf("  %s run main.swift                 # Run a file\n", program_name);
    printf("  %s run --debug-trace              # Run with execution trace\n", program_name);
    printf("  %s --log-level debug --log-modules vm,module\n", program_name);
    printf("\n");
    
    printf("Environment Variables:\n");
    printf("  SWIFTLANG_LOG_LEVEL     Default log level\n");
    printf("  SWIFTLANG_LOG_MODULES   Default log modules\n");
    printf("  SWIFTLANG_LOG_FILE      Default log file\n");
    printf("  SWIFTLANG_MODULE_PATH   Additional module search paths (colon-separated)\n");
    printf("\n");
}

void cli_parse_args(int argc, char* argv[]) {
    int option_index = 0;
    int c;
    
    // Reset getopt
    optind = 1;
    
    while ((c = getopt_long(argc, argv, "hVvqo:b:Oj:M:w", long_options, &option_index)) != -1) {
        switch (c) {
            case 0: {
                const char* name = long_options[option_index].name;
                
                // Debug options
                if (strcmp(name, "debug-tokens") == 0) {
                    g_cli_config.debug_tokens = true;
                    g_debug_tokens = true;
                } else if (strcmp(name, "debug-ast") == 0) {
                    g_cli_config.debug_ast = true;
                    g_debug_ast = true;
                } else if (strcmp(name, "debug-bytecode") == 0) {
                    g_cli_config.debug_bytecode = true;
                    g_debug_bytecode = true;
                } else if (strcmp(name, "debug-trace") == 0) {
                    g_cli_config.debug_trace = true;
                    g_debug_trace = true;
                } else if (strcmp(name, "debug-all") == 0) {
                    g_cli_config.debug_all = true;
                    g_debug_tokens = g_debug_ast = g_debug_bytecode = g_debug_trace = true;
                    g_cli_config.debug_tokens = g_cli_config.debug_ast = true;
                    g_cli_config.debug_bytecode = g_cli_config.debug_trace = true;
                    g_cli_config.log_level = LOG_LEVEL_TRACE;
                    logger_enable_all_modules();
                }
                
                // Logging options
                else if (strcmp(name, "log-level") == 0) {
                    g_cli_config.log_level = logger_string_to_level(optarg);
                } else if (strcmp(name, "log-modules") == 0) {
                    if (strcmp(optarg, "all") == 0) {
                        g_cli_config.log_modules = ~0u;
                    } else if (strcmp(optarg, "none") == 0) {
                        g_cli_config.log_modules = 0;
                    } else {
                        g_cli_config.log_modules = 0;
                        char* modules = strdup(optarg);
                        char* token = strtok(modules, ",");
                        while (token) {
                            g_cli_config.log_modules |= logger_string_to_module(token);
                            token = strtok(NULL, ",");
                        }
                        free(modules);
                    }
                } else if (strcmp(name, "log-file") == 0) {
                    g_cli_config.log_file = optarg;
                } else if (strcmp(name, "no-colors") == 0) {
                    g_cli_config.log_colors = false;
                } else if (strcmp(name, "timestamps") == 0) {
                    g_cli_config.log_timestamps = true;
                } else if (strcmp(name, "source-location") == 0) {
                    g_cli_config.log_source_location = true;
                }
                
                // Build options
                else if (strcmp(name, "emit-bytecode") == 0) {
                    g_cli_config.emit_bytecode = true;
                } else if (strcmp(name, "emit-ast") == 0) {
                    g_cli_config.emit_ast = true;
                } else if (strcmp(name, "format") == 0) {
                    g_cli_config.format = optarg;
                }
                
                // Runtime options
                else if (strcmp(name, "stack-size") == 0) {
                    g_cli_config.stack_size = atoi(optarg) * 1024;
                } else if (strcmp(name, "heap-size") == 0) {
                    g_cli_config.heap_size = atoi(optarg) * 1024 * 1024;
                } else if (strcmp(name, "gc-stress") == 0) {
                    g_cli_config.gc_stress_test = true;
                }
                break;
            }
            
            case 'h':
                cli_print_help(argv[0]);
                exit(0);
                
            case 'V':
                cli_print_version();
                exit(0);
                
            case 'v':
                g_cli_config.verbose = true;
                if (g_cli_config.log_level > LOG_LEVEL_DEBUG) {
                    g_cli_config.log_level = LOG_LEVEL_DEBUG;
                }
                break;
                
            case 'q':
                g_cli_config.quiet = true;
                if (g_cli_config.log_level < LOG_LEVEL_ERROR) {
                    g_cli_config.log_level = LOG_LEVEL_ERROR;
                }
                break;
                
            case 'o':
                g_cli_config.output_dir = optarg;
                break;
                
            case 'b':
                g_cli_config.build_dir = optarg;
                break;
                
            case 'O':
                g_cli_config.optimize = true;
                break;
                
            case 'j':
                g_cli_config.jobs = atoi(optarg);
                if (g_cli_config.jobs < 1) g_cli_config.jobs = 1;
                break;
                
            case 'M': {
                // Add module path
                int count = g_cli_config.module_path_count;
                g_cli_config.module_paths = realloc(g_cli_config.module_paths, 
                                                   (count + 1) * sizeof(char*));
                g_cli_config.module_paths[count] = optarg;
                g_cli_config.module_path_count++;
                break;
            }
                
            case 'w':
                g_cli_config.watch_mode = true;
                break;
                
            case '?':
                // getopt_long already printed an error message
                exit(1);
                
            default:
                break;
        }
    }
    
    // Configure logger based on CLI options
    logger_set_level(g_cli_config.log_level);
    if (g_cli_config.log_modules != LOG_MODULE_ALL) {
        logger_disable_all_modules();
        unsigned int modules = g_cli_config.log_modules;
        for (int i = 0; i < 32; i++) {
            if (modules & (1 << i)) {
                logger_enable_module(1 << i);
            }
        }
    }
    
    if (g_cli_config.log_file) {
        logger_set_output_file(g_cli_config.log_file);
    }
    
    g_logger_config.use_colors = g_cli_config.log_colors && cli_is_terminal();
    g_logger_config.show_timestamp = g_cli_config.log_timestamps;
    g_logger_config.show_file_line = g_cli_config.log_source_location;
}

int cli_main(int argc, char* argv[]) {
    // Initialize logger
    logger_init();
    
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
        return cli_run_file(cmd_name);
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

// Command implementations

int cli_cmd_help(int argc, char* argv[]) {
    if (argc > 1) {
        // Show help for specific command
        const char* cmd_name = argv[1];
        const Command* cmd = NULL;
        
        for (int i = 0; i < num_commands; i++) {
            if (strcmp(cmd_name, commands[i].name) == 0) {
                cmd = &commands[i];
                break;
            }
        }
        
        if (cmd) {
            printf(COLOR_BOLD "%s" COLOR_RESET " - %s\n\n", cmd->name, cmd->description);
            printf("Usage: swiftlang %s\n\n", cmd->usage);
            if (cmd->help) {
                printf("%s\n", cmd->help);
            }
        } else {
            cli_print_error("Unknown command: %s", cmd_name);
            return 1;
        }
    } else {
        cli_print_help("swiftlang");
    }
    return 0;
}

int cli_cmd_version(int argc, char* argv[]) {
    (void)argc; (void)argv;
    cli_print_version();
    return 0;
}

int cli_cmd_init(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    LOG_INFO(LOG_MODULE_CLI, "Initializing new project");
    
    // Create module.json
    FILE* f = fopen("module.json", "w");
    if (!f) {
        cli_print_error("Failed to create module.json");
        return 1;
    }
    
    char* dir_name = cli_resolve_path(".");
    char* base_name = strrchr(dir_name, '/');
    base_name = base_name ? base_name + 1 : dir_name;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"name\": \"%s\",\n", base_name);
    fprintf(f, "  \"version\": \"0.1.0\",\n");
    fprintf(f, "  \"description\": \"A new SwiftLang project\",\n");
    fprintf(f, "  \"main\": \"main.swift\",\n");
    fprintf(f, "  \"type\": \"application\"\n");
    fprintf(f, "}\n");
    fclose(f);
    
    // Create main.swift
    f = fopen("main.swift", "w");
    if (f) {
        fprintf(f, "// Welcome to SwiftLang!\n\n");
        fprintf(f, "print(\"Hello, World!\")\n");
        fclose(f);
    }
    
    // Create .gitignore
    f = fopen(".gitignore", "w");
    if (f) {
        fprintf(f, "# Build artifacts\n");
        fprintf(f, "/build/\n");
        fprintf(f, "*.swiftmodule\n");
        fprintf(f, "*.swift.bc\n\n");
        fprintf(f, "# Logs\n");
        fprintf(f, "*.log\n");
        fclose(f);
    }
    
    cli_print_success("Project initialized successfully!");
    cli_print_info("Run 'swiftlang build' to build your project");
    cli_print_info("Run 'swiftlang run' to run your project");
    
    free(dir_name);
    return 0;
}

int cli_cmd_new(int argc, char* argv[]) {
    if (argc < 2) {
        cli_print_error("Project name required");
        cli_print_info("Usage: swiftlang new <name>");
        return 1;
    }
    
    const char* project_name = argv[1];
    
    // Create project directory
    if (!cli_create_dir(project_name)) {
        cli_print_error("Failed to create project directory");
        return 1;
    }
    
    // Change to project directory
    if (chdir(project_name) != 0) {
        cli_print_error("Failed to enter project directory");
        return 1;
    }
    
    // Initialize project
    return cli_cmd_init(0, NULL);
}

int cli_cmd_build(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    LOG_INFO(LOG_MODULE_CLI, "Building project");
    
    // Check for module.json
    if (!cli_file_exists("module.json")) {
        cli_print_error("No module.json found in current directory");
        cli_print_info("Run 'swiftlang init' to initialize a project");
        return 1;
    }
    
    // Create build directory
    const char* build_dir = g_cli_config.build_dir ? g_cli_config.build_dir : "build";
    if (!cli_create_dir_recursive(build_dir)) {
        cli_print_error("Failed to create build directory");
        return 1;
    }
    
    // TODO: Implement actual build logic
    cli_print_info("Building project...");
    cli_print_success("Build completed successfully!");
    
    return 0;
}

int cli_cmd_run(int argc, char* argv[]) {
    const char* file_to_run = NULL;
    
    // Check if a file was specified
    if (argc > 1 && argv[1][0] != '-') {
        file_to_run = argv[1];
    } else if (cli_file_exists("module.json")) {
        // Try to run project main file
        // TODO: Parse module.json to get main file
        file_to_run = "main.swift";
    }
    
    if (!file_to_run) {
        cli_print_error("No file specified and no project found");
        return 1;
    }
    
    if (!cli_file_exists(file_to_run)) {
        cli_print_error("File not found: %s", file_to_run);
        return 1;
    }
    
    return cli_run_file(file_to_run);
}

int cli_cmd_test(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    LOG_INFO(LOG_MODULE_CLI, "Running tests");
    
    // Look for test files
    glob_t glob_result;
    int result = glob("tests/*.swift", GLOB_NOSORT, NULL, &glob_result);
    
    if (result != 0) {
        cli_print_warning("No test files found");
        return 0;
    }
    
    int passed = 0;
    int failed = 0;
    
    cli_print_info("Running %zu test files...", glob_result.gl_pathc);
    
    for (size_t i = 0; i < glob_result.gl_pathc; i++) {
        const char* test_file = glob_result.gl_pathv[i];
        printf("\n[TEST] %s\n", test_file);
        
        int result = cli_run_file(test_file);
        if (result == 0) {
            cli_print_success("PASSED");
            passed++;
        } else {
            cli_print_error("FAILED (exit code %d)", result);
            failed++;
        }
    }
    
    globfree(&glob_result);
    
    printf("\n");
    cli_print_info("Test Summary: %d passed, %d failed", passed, failed);
    
    return failed > 0 ? 1 : 0;
}

int cli_cmd_repl(int argc, char* argv[]) {
    (void)argc; (void)argv;
    cli_run_repl();
    return 0;
}

// Utility functions

void cli_print_error(const char* fmt, ...) {
    if (g_cli_config.quiet) return;
    
    fprintf(stderr, "%s✗%s ", COLOR_RED, COLOR_RESET);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void cli_print_warning(const char* fmt, ...) {
    if (g_cli_config.quiet) return;
    
    fprintf(stderr, "%s⚠%s ", COLOR_YELLOW, COLOR_RESET);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void cli_print_success(const char* fmt, ...) {
    if (g_cli_config.quiet) return;
    
    printf("%s✓%s ", COLOR_GREEN, COLOR_RESET);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

void cli_print_info(const char* fmt, ...) {
    if (g_cli_config.quiet) return;
    
    printf("%sℹ%s ", COLOR_BLUE, COLOR_RESET);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
}

bool cli_file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

bool cli_dir_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool cli_create_dir(const char* path) {
    return mkdir(path, 0755) == 0 || (errno == EEXIST && cli_dir_exists(path));
}

bool cli_create_dir_recursive(const char* path) {
    char* copy = strdup(path);
    char* p = copy;
    
    while (*p) {
        if (*p == '/') {
            *p = '\0';
            if (strlen(copy) > 0 && !cli_dir_exists(copy)) {
                if (mkdir(copy, 0755) != 0) {
                    free(copy);
                    return false;
                }
            }
            *p = '/';
        }
        p++;
    }
    
    bool result = cli_create_dir(copy);
    free(copy);
    return result;
}

char* cli_resolve_path(const char* path) {
    return realpath(path, NULL);
}

int cli_get_terminal_width(void) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        return w.ws_col;
    }
    return 80; // default
}

bool cli_is_terminal(void) {
    return isatty(STDOUT_FILENO);
}

bool cli_confirm(const char* message) {
    printf("%s [y/N] ", message);
    fflush(stdout);
    
    char response[10];
    if (fgets(response, sizeof(response), stdin)) {
        return response[0] == 'y' || response[0] == 'Y';
    }
    return false;
}

char* cli_prompt(const char* message, const char* default_value) {
    printf("%s", message);
    if (default_value) {
        printf(" [%s]", default_value);
    }
    printf(": ");
    fflush(stdout);
    
    static char buffer[256];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        // Remove newline
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        
        // Use default if empty
        if (strlen(buffer) == 0 && default_value) {
            strcpy(buffer, default_value);
        }
        
        return buffer;
    }
    
    return (char*)default_value;
}

// Progress indicator
struct CLIProgress {
    const char* message;
    int total;
    int current;
    int width;
};

CLIProgress* cli_progress_start(const char* message, int total) {
    CLIProgress* progress = malloc(sizeof(CLIProgress));
    progress->message = message;
    progress->total = total;
    progress->current = 0;
    progress->width = cli_get_terminal_width() - 40;
    
    if (!g_cli_config.quiet && cli_is_terminal()) {
        printf("%s ", message);
        fflush(stdout);
    }
    
    return progress;
}

void cli_progress_update(CLIProgress* progress, int current) {
    if (!progress || g_cli_config.quiet || !cli_is_terminal()) return;
    
    progress->current = current;
    
    // Calculate percentage
    int percent = (progress->total > 0) ? (current * 100 / progress->total) : 0;
    
    // Draw progress bar
    printf("\r%s [", progress->message);
    
    int filled = (progress->width * percent) / 100;
    for (int i = 0; i < progress->width; i++) {
        if (i < filled) {
            printf("█");
        } else {
            printf("░");
        }
    }
    
    printf("] %d%%", percent);
    fflush(stdout);
}

void cli_progress_finish(CLIProgress* progress, const char* message) {
    if (!progress) return;
    
    if (!g_cli_config.quiet && cli_is_terminal()) {
        printf("\r%s%*s\r", "", (int)(strlen(progress->message) + progress->width + 10), "");
        if (message) {
            cli_print_success("%s", message);
        }
    }
}

void cli_progress_free(CLIProgress* progress) {
    free(progress);
}

// File reading utility
static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        cli_print_error("Could not open file '%s'", path);
        return NULL;
    }
    
    fseek(file, 0L, SEEK_END);
    size_t fileSize = ftell(file);
    rewind(file);
    
    char* buffer = (char*)malloc(fileSize + 1);
    if (!buffer) {
        cli_print_error("Not enough memory to read '%s'", path);
        fclose(file);
        return NULL;
    }
    
    size_t bytesRead = fread(buffer, sizeof(char), fileSize, file);
    if (bytesRead < fileSize) {
        cli_print_error("Could not read file '%s'", path);
        free(buffer);
        fclose(file);
        return NULL;
    }
    
    buffer[bytesRead] = '\0';
    fclose(file);
    return buffer;
}

// REPL implementation
static bool needs_more_input(const char* input) {
    int brace_count = 0;
    int paren_count = 0;
    bool in_string = false;
    bool in_char = false;
    bool escape = false;
    
    for (const char* p = input; *p; p++) {
        if (escape) {
            escape = false;
            continue;
        }
        
        if (*p == '\\') {
            escape = true;
            continue;
        }
        
        if (!in_string && !in_char) {
            if (*p == '"') in_string = true;
            else if (*p == '\'') in_char = true;
            else if (*p == '{') brace_count++;
            else if (*p == '}') brace_count--;
            else if (*p == '(') paren_count++;
            else if (*p == ')') paren_count--;
        } else if (in_string && *p == '"') {
            in_string = false;
        } else if (in_char && *p == '\'') {
            in_char = false;
        }
    }
    
    return brace_count > 0 || paren_count > 0 || in_string || in_char;
}

void cli_run_repl(void) {
    const int MAX_LINE = 1024;
    const int MAX_INPUT = 8192;
    char line[MAX_LINE];
    char input[MAX_INPUT];
    
    VM vm;
    vm_init(&vm);
    
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
            if (strlen(input) + strlen(line) >= MAX_INPUT - 1) {
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
                InterpretResult result = vm_interpret(&vm, &chunk);
                
                if (result == INTERPRET_RUNTIME_ERROR) {
                    // Error already printed by VM
                }
                
            } else {
                cli_print_error("Compilation error");
            }
            
            chunk_free(&chunk);
        }
        
        parser_destroy(parser);
    }
    
exit_repl:
    vm_free(&vm);
}

int cli_run_file(const char* path) {
    LOG_INFO(LOG_MODULE_CLI, "Running file: %s", path);
    
    // Read file
    char* source = read_file(path);
    if (!source) return 1;
    
    // Update module path for imports
    char* abs_path = cli_resolve_path(path);
    if (abs_path) {
        char* dir = strdup(abs_path);
        char* last_slash = strrchr(dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            // Add directory to module search paths
            int count = g_cli_config.module_path_count;
            g_cli_config.module_paths = realloc(g_cli_config.module_paths, 
                                               (count + 1) * sizeof(char*));
            g_cli_config.module_paths[count] = dir;
            g_cli_config.module_path_count++;
        }
        free(abs_path);
    }
    
    // Parse
    Parser* parser = parser_create(source);
    ProgramNode* program = parser_parse_program(parser);
    
    if (parser->had_error) {
        cli_print_error("Parse error detected");
        parser_destroy(parser);
        free(source);
        return 65;
    }
    
    // Print AST if requested
    if (g_cli_config.debug_ast) {
        printf("\n=== AST ===\n");
        printf("AST printing is not yet fully implemented\n");
        // TODO: Fix ast_print_program to match current AST structure
        // ast_print_program(program);
        printf("\n");
    }
    
    // Compile
    Chunk chunk;
    chunk_init(&chunk);
    
    if (!compile(program, &chunk)) {
        cli_print_error("Compilation error");
        program_destroy(program);
        parser_destroy(parser);
        free(source);
        return 65;
    }
    
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
        // TODO: Implement bytecode serialization
        LOG_DEBUG(LOG_MODULE_CLI, "Would save bytecode to: %s", bytecode_path);
    }
    
    // Create VM and run
    VM vm;
    vm_init(&vm);
    
    // Add custom module paths to VM's module loader
    for (int i = 0; i < g_cli_config.module_path_count; i++) {
        module_loader_add_search_path(vm.module_loader, g_cli_config.module_paths[i]);
    }
    
    // Set current module path for relative imports
    vm.current_module_path = path;
    
    LOG_DEBUG(LOG_MODULE_CLI, "Starting VM execution");
    InterpretResult result = vm_interpret(&vm, &chunk);
    
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
    free(source);
    vm_free(&vm);
    
    LOG_DEBUG(LOG_MODULE_CLI, "Execution completed with result: %d", result);
    
    if (result == INTERPRET_COMPILE_ERROR) return 65;
    if (result == INTERPRET_RUNTIME_ERROR) return 70;
    return 0;
}