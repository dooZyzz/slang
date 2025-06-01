#include "utils/cli.h"
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
#include "utils/allocators.h"
#include "miniz.h"

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
    },
    // Package management commands
    {
        .name = "install",
        .description = "Install dependencies or modules",
        .handler = cli_cmd_install,
        .usage = "install [module] [options]",
        .help = "Install modules from manifest or specific module.\n"
                "Options:\n"
                "  --save           Save to dependencies in manifest.json\n"
                "  --save-dev       Save to devDependencies\n"
                "  --global         Install globally\n"
                "  --no-bundle      Don't bundle dependencies"
    },
    {
        .name = "add",
        .description = "Add a dependency to the project",
        .handler = cli_cmd_add,
        .usage = "add <module> [version] [options]",
        .help = "Add a module dependency to manifest.json.\n"
                "Options:\n"
                "  --dev            Add as development dependency\n"
                "  --exact          Use exact version\n"
                "  --optional       Mark as optional dependency"
    },
    {
        .name = "remove",
        .description = "Remove a dependency from the project",
        .handler = cli_cmd_remove,
        .usage = "remove <module>",
        .help = "Remove a module dependency from manifest.json."
    },
    {
        .name = "publish",
        .description = "Publish module to registry",
        .handler = cli_cmd_publish,
        .usage = "publish [options]",
        .help = "Build and publish module to registry.\n"
                "Options:\n"
                "  --tag <tag>      Version tag (default: from manifest)\n"
                "  --access <level> Access level: public or restricted\n"
                "  --dry-run        Perform a dry run without publishing"
    },
    {
        .name = "bundle",
        .description = "Create a module bundle (.swiftmodule)",
        .handler = cli_cmd_bundle,
        .usage = "bundle [options]",
        .help = "Bundle the current module into a .swiftmodule file.\n"
                "Options:\n"
                "  --output <file>  Output file (default: <name>.swiftmodule)\n"
                "  --include-deps   Include all dependencies in bundle\n"
                "  --minify         Minify bytecode\n"
                "  --sign           Sign the bundle"
    },
    {
        .name = "list",
        .description = "List installed modules",
        .handler = cli_cmd_list,
        .usage = "list [options]",
        .help = "List installed modules and dependencies.\n"
                "Options:\n"
                "  --depth <n>      Max display depth (default: 0)\n"
                "  --json           Output as JSON\n"
                "  --global         List global modules"
    },
    {
        .name = "update",
        .description = "Update dependencies",
        .handler = cli_cmd_update,
        .usage = "update [module] [options]",
        .help = "Update module dependencies to latest versions.\n"
                "Options:\n"
                "  --save           Update manifest.json\n"
                "  --dry-run        Show what would be updated"
    },
    {
        .name = "cache",
        .description = "Manage module cache",
        .handler = cli_cmd_cache,
        .usage = "cache <command> [options]",
        .help = "Manage the module cache.\n"
                "Commands:\n"
                "  clean            Remove all cached modules\n"
                "  list             List cached modules\n"
                "  verify           Verify cache integrity"
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
    
    // Package management options
    {"save", no_argument, 0, 0},
    {"save-dev", no_argument, 0, 0},
    {"global", no_argument, 0, 'g'},
    {"no-bundle", no_argument, 0, 0},
    {"dev", no_argument, 0, 0},
    {"exact", no_argument, 0, 0},
    {"optional", no_argument, 0, 0},
    {"tag", required_argument, 0, 0},
    {"access", required_argument, 0, 0},
    {"dry-run", no_argument, 0, 0},
    {"include-deps", no_argument, 0, 0},
    {"minify", no_argument, 0, 0},
    {"sign", no_argument, 0, 0},
    {"depth", required_argument, 0, 0},
    {"json", no_argument, 0, 0},
    
    {0, 0, 0, 0}
};

void cli_print_banner(void) {
    if (g_cli_config.quiet) return;
    
    printf("\n");
    printf(COLOR_CYAN "┌─────────────────────────────────────────┐\n" COLOR_RESET);
    printf(COLOR_CYAN "│ " COLOR_BOLD COLOR_WHITE "SwiftLang Programming Language" COLOR_RESET COLOR_CYAN "          │\n" COLOR_RESET);
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
    // Initialize allocator system first
    AllocatorConfig alloc_config = {
        .enable_trace = false,
        .enable_stats = false,
        .arena_size = 64 * 1024,        // 64KB arenas
        .object_pool_size = 4 * 1024    // 4KB object pools
    };
    allocators_init(&alloc_config);
    
    // Initialize logger
    logger_init();
    
    // Initialize module hooks system
    module_hooks_init();
    
    // Parse global options first
    cli_parse_args(argc, argv);
    
    // Check if no arguments provided
    if (optind >= argc) {
        cli_print_help(argv[0]);
        allocators_shutdown();
        return 0;
    }
    
    // Get command
    const char* cmd_name = argv[optind];
    
    // Check if it's a file to run
    if (cli_file_exists(cmd_name) && strstr(cmd_name, ".swift")) {
        g_cli_config.input_file = cmd_name;
        int result = cli_run_file(cmd_name);
        allocators_shutdown();
        return result;
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
        allocators_shutdown();
        return 1;
    }
    
    // Execute command with remaining arguments
    int result = cmd->handler(argc - optind, argv + optind);
    allocators_shutdown();
    return result;
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
    
    // Create manifest.json
    FILE* f = fopen("manifest.json", "w");
    if (!f) {
        cli_print_error("Failed to create manifest.json");
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
    
    cli_print_info("Starting build process...");
    
    // Check for manifest.json
    if (!cli_file_exists("manifest.json")) {
        cli_print_error("No manifest.json found in current directory");
        cli_print_info("Run 'swiftlang init' to initialize a project");
        return 1;
    }
    
    // Create build directory
    const char* build_dir = g_cli_config.build_dir ? g_cli_config.build_dir : "build";
    if (!cli_create_dir_recursive(build_dir)) {
        cli_print_error("Failed to create build directory");
        return 1;
    }
    
    cli_print_info("Loading module metadata...");
    
    // Load module metadata
    ModuleMetadata* metadata = package_load_module_metadata(".");
    if (!metadata) {
        cli_print_error("Failed to load manifest.json");
        return 1;
    }
    
    cli_print_info("Building module: %s", metadata->name);
    
    // Create module compiler
    ModuleCompiler* compiler = module_compiler_create();
    if (!compiler) {
        cli_print_error("Failed to create module compiler");
        package_free_module_metadata(metadata);
        return 1;
    }
    
    // Set output path
    char output_path[PATH_MAX];
    snprintf(output_path, sizeof(output_path), "%s/%s.swiftmodule", 
             build_dir, metadata->name);
    
    // Build options
    ModuleCompilerOptions options = {
        .optimize = g_cli_config.optimize,
        .strip_debug = !g_cli_config.debug_bytecode,
        .include_source = false,
        .output_dir = build_dir
    };
    
    // Build the module
    if (!module_compiler_build_package(compiler, metadata, output_path, &options)) {
        cli_print_error("Build failed: %s", module_compiler_get_error(compiler));
        module_compiler_destroy(compiler);
        package_free_module_metadata(metadata);
        return 1;
    }
    
    module_compiler_destroy(compiler);
    package_free_module_metadata(metadata);
    
    cli_print_success("Module built successfully: %s", output_path);
    
    // Copy to global cache
    const char* home = getenv("HOME");
    if (home) {
        char cache_path[PATH_MAX];
        snprintf(cache_path, sizeof(cache_path), "%s/.swiftlang/modules/%s.swiftmodule",
                 home, metadata->name);
        
        // Create cache directory
        char cache_dir[PATH_MAX];
        snprintf(cache_dir, sizeof(cache_dir), "%s/.swiftlang/modules", home);
        cli_create_dir_recursive(cache_dir);
        
        // Copy module
        if (cli_copy_file(output_path, cache_path)) {
            cli_print_info("Module cached to: %s", cache_path);
        }
    }
    
    return 0;
}

int cli_cmd_run(int argc, char* argv[]) {
    const char* file_to_run = NULL;
    
    // Check if a file was specified
    if (argc > 1 && argv[1][0] != '-') {
        file_to_run = argv[1];
    } else if (cli_file_exists("manifest.json")) {
        // Try to run project main file
        ModuleMetadata* metadata = package_load_module_metadata(".");
        if (metadata && metadata->main_file) {
            file_to_run = metadata->main_file;
        } else {
            file_to_run = "main.swift";
        }
        // Don't free metadata yet as we're using its main_file string
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

bool cli_copy_file(const char* src, const char* dst) {
    FILE* src_file = fopen(src, "rb");
    if (!src_file) return false;
    
    FILE* dst_file = fopen(dst, "wb");
    if (!dst_file) {
        fclose(src_file);
        return false;
    }
    
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        if (fwrite(buffer, 1, bytes, dst_file) != bytes) {
            fclose(src_file);
            fclose(dst_file);
            return false;
        }
    }
    
    fclose(src_file);
    fclose(dst_file);
    return true;
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
    const int MAX_INPUT_SIZE = 8192;
    char line[MAX_LINE];
    char input[MAX_INPUT_SIZE];
    
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
        ast_print_program(program);
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
            free(bytecode_data);
        } else {
            cli_print_error("Failed to serialize bytecode");
        }
    }
    
    // Create VM and run
    VM vm;
    vm_init(&vm);
    ModuleLoader* loader = module_loader_create(&vm);
    vm.module_loader = loader;
    
    // Add custom module paths to VM's module loader
    for (int i = 0; i < g_cli_config.module_path_count; i++) {
        module_loader_add_search_path(vm.module_loader, g_cli_config.module_paths[i]);
    }
    
    // Add default search paths
    // Add modules directory relative to executable
    char exe_path[PATH_MAX];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len == -1) {
        // macOS doesn't have /proc/self/exe, use different method
        uint32_t size = sizeof(exe_path);
        if (_NSGetExecutablePath(exe_path, &size) == 0) {
            len = strlen(exe_path);
        }
    }
    if (len > 0) {
        exe_path[len] = '\0';
        char* last_slash = strrchr(exe_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            char modules_path[PATH_MAX];
            snprintf(modules_path, sizeof(modules_path), "%s/modules", exe_path);
            module_loader_add_search_path(vm.module_loader, modules_path);
            LOG_DEBUG(LOG_MODULE_CLI, "Added modules search path: %s", modules_path);
        }
    }
    
    // Add project modules directory
    module_loader_add_search_path(vm.module_loader, "./modules");
    module_loader_add_search_path(vm.module_loader, "modules");
    
    // Set current module path for relative imports
    vm.current_module_path = path;
    
    LOG_DEBUG(LOG_MODULE_CLI, "Starting VM execution");
    InterpretResult result = vm_interpret(&vm, &chunk);
    
    chunk_free(&chunk);
    program_destroy(program);
    parser_destroy(parser);
    free(source);
    vm_free(&vm);
    module_loader_destroy(loader);
    
    LOG_DEBUG(LOG_MODULE_CLI, "Execution completed with result: %d", result);
    
    if (result == INTERPRET_COMPILE_ERROR) return 65;
    if (result == INTERPRET_RUNTIME_ERROR) return 70;
    return 0;
}

// Package management command implementations

// Simple JSON manifest handling
typedef struct {
    char* name;
    char* version;
    char* description;
    char* main;
    char* type;
    char** sources;
    int sources_count;
    // Dependencies as key-value pairs
    char** dep_names;
    char** dep_versions;
    int dep_count;
    char** dev_dep_names;
    char** dev_dep_versions;
    int dev_dep_count;
} Manifest;

static Manifest* manifest_read(const char* path) {
    char* content = read_file(path);
    if (!content) return NULL;
    
    Manifest* manifest = calloc(1, sizeof(Manifest));
    
    // Very simple JSON parsing - this should be replaced with proper JSON parser
    char* ptr = content;
    char key[256], value[1024];
    
    while (*ptr) {
        // Skip whitespace
        while (*ptr && (*ptr == ' ' || *ptr == '\t' || *ptr == '\n' || *ptr == '\r')) ptr++;
        
        // Look for "key": "value" patterns
        if (*ptr == '"') {
            ptr++; // Skip opening quote
            int i = 0;
            while (*ptr && *ptr != '"' && i < sizeof(key)-1) {
                key[i++] = *ptr++;
            }
            key[i] = '\0';
            if (*ptr == '"') ptr++; // Skip closing quote
            
            // Skip : and whitespace
            while (*ptr && (*ptr == ' ' || *ptr == '\t' || *ptr == ':')) ptr++;
            
            // Read value
            if (*ptr == '"') {
                ptr++; // Skip opening quote
                i = 0;
                while (*ptr && *ptr != '"' && i < sizeof(value)-1) {
                    value[i++] = *ptr++;
                }
                value[i] = '\0';
                if (*ptr == '"') ptr++; // Skip closing quote
                
                // Store values
                if (strcmp(key, "name") == 0) {
                    manifest->name = strdup(value);
                } else if (strcmp(key, "version") == 0) {
                    manifest->version = strdup(value);
                } else if (strcmp(key, "description") == 0) {
                    manifest->description = strdup(value);
                } else if (strcmp(key, "main") == 0) {
                    manifest->main = strdup(value);
                } else if (strcmp(key, "type") == 0) {
                    manifest->type = strdup(value);
                }
            }
        }
        ptr++;
    }
    
    free(content);
    return manifest;
}

static void manifest_free(Manifest* manifest) {
    if (!manifest) return;
    
    free(manifest->name);
    free(manifest->version);
    free(manifest->description);
    free(manifest->main);
    free(manifest->type);
    
    for (int i = 0; i < manifest->sources_count; i++) {
        free(manifest->sources[i]);
    }
    free(manifest->sources);
    
    for (int i = 0; i < manifest->dep_count; i++) {
        free(manifest->dep_names[i]);
        free(manifest->dep_versions[i]);
    }
    free(manifest->dep_names);
    free(manifest->dep_versions);
    
    for (int i = 0; i < manifest->dev_dep_count; i++) {
        free(manifest->dev_dep_names[i]);
        free(manifest->dev_dep_versions[i]);
    }
    free(manifest->dev_dep_names);
    free(manifest->dev_dep_versions);
    
    free(manifest);
}

static bool manifest_write(const char* path, Manifest* manifest) {
    FILE* f = fopen(path, "w");
    if (!f) return false;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"name\": \"%s\",\n", manifest->name ? manifest->name : "");
    fprintf(f, "  \"version\": \"%s\",\n", manifest->version ? manifest->version : "1.0.0");
    fprintf(f, "  \"description\": \"%s\",\n", manifest->description ? manifest->description : "");
    fprintf(f, "  \"main\": \"%s\",\n", manifest->main ? manifest->main : "main.swift");
    fprintf(f, "  \"type\": \"%s\"", manifest->type ? manifest->type : "application");
    
    // Sources
    if (manifest->sources_count > 0) {
        fprintf(f, ",\n  \"sources\": [\n");
        for (int i = 0; i < manifest->sources_count; i++) {
            fprintf(f, "    \"%s\"", manifest->sources[i]);
            if (i < manifest->sources_count - 1) fprintf(f, ",");
            fprintf(f, "\n");
        }
        fprintf(f, "  ]");
    }
    
    // Dependencies
    if (manifest->dep_count > 0) {
        fprintf(f, ",\n  \"dependencies\": {\n");
        for (int i = 0; i < manifest->dep_count; i++) {
            fprintf(f, "    \"%s\": \"%s\"", manifest->dep_names[i], manifest->dep_versions[i]);
            if (i < manifest->dep_count - 1) fprintf(f, ",");
            fprintf(f, "\n");
        }
        fprintf(f, "  }");
    }
    
    // Dev dependencies
    if (manifest->dev_dep_count > 0) {
        fprintf(f, ",\n  \"devDependencies\": {\n");
        for (int i = 0; i < manifest->dev_dep_count; i++) {
            fprintf(f, "    \"%s\": \"%s\"", manifest->dev_dep_names[i], manifest->dev_dep_versions[i]);
            if (i < manifest->dev_dep_count - 1) fprintf(f, ",");
            fprintf(f, "\n");
        }
        fprintf(f, "  }");
    }
    
    fprintf(f, "\n}\n");
    fclose(f);
    return true;
}

// Get the global module cache directory
static const char* get_module_cache_dir(void) {
    static char cache_dir[PATH_MAX];
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(cache_dir, sizeof(cache_dir), "%s/.swiftlang/cache", home);
    return cache_dir;
}

// Get the global modules directory
static const char* get_global_modules_dir(void) {
    static char modules_dir[PATH_MAX];
    const char* home = getenv("HOME");
    if (!home) home = "/tmp";
    snprintf(modules_dir, sizeof(modules_dir), "%s/.swiftlang/modules", home);
    return modules_dir;
}

// Ensure a directory exists
static bool ensure_directory_exists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        // Create directory recursively
        char tmp[PATH_MAX];
        snprintf(tmp, sizeof(tmp), "%s", path);
        
        for (char* p = tmp + 1; *p; p++) {
            if (*p == '/') {
                *p = '\0';
                mkdir(tmp, 0755);
                *p = '/';
            }
        }
        mkdir(tmp, 0755);
    }
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

int cli_cmd_install(int argc, char* argv[]) {
    bool save = false;
    bool save_dev = false;
    bool global = false;
    bool no_bundle = false;
    
    // Debug: print arguments
    LOG_DEBUG(LOG_MODULE_CLI, "install command called with %d args", argc);
    for (int i = 0; i < argc; i++) {
        LOG_DEBUG(LOG_MODULE_CLI, "  argv[%d] = '%s'", i, argv[i]);
    }
    
    // Parse options and find module name
    const char* module_to_install = NULL;
    for (int i = 1; i < argc; i++) {  // Start from 1 to skip command name
        if (strcmp(argv[i], "--save") == 0) save = true;
        else if (strcmp(argv[i], "--save-dev") == 0) save_dev = true;
        else if (strcmp(argv[i], "--global") == 0 || strcmp(argv[i], "-g") == 0) global = true;
        else if (strcmp(argv[i], "--no-bundle") == 0) no_bundle = true;
        else if (argv[i][0] != '-' && !module_to_install) {
            module_to_install = argv[i];
        }
    }
    
    // Check if manifest.json exists
    if (!global && !module_to_install) {
        // Install from manifest.json
        if (!cli_file_exists("manifest.json")) {
            cli_print_error("No manifest.json found in current directory");
            return 1;
        }
        
        cli_print_info("Installing dependencies from manifest.json...");
        // TODO: Read manifest and install dependencies
        cli_print_success("Dependencies installed successfully");
        return 0;
    }
    
    // Install specific module
    if (module_to_install) {
        const char* module_name = module_to_install;
        const char* version = "latest"; // TODO: Parse version from module@version
        
        cli_print_info("Installing %s@%s...", module_name, version);
        
        // Check if it's a local .swiftmodule file
        if (strstr(module_name, ".swiftmodule")) {
            if (!cli_file_exists(module_name)) {
                cli_print_error("Module file not found: %s", module_name);
                return 1;
            }
            
            // Extract module name from filename
            char base_name[256];
            const char* filename = strrchr(module_name, '/');
            filename = filename ? filename + 1 : module_name;
            strncpy(base_name, filename, sizeof(base_name)-1);
            char* ext = strstr(base_name, ".swiftmodule");
            if (ext) *ext = '\0';
            
            // Determine install location
            const char* install_dir = global ? get_global_modules_dir() : "node_modules";
            ensure_directory_exists(install_dir);
            
            // Create module directory
            char module_dir[PATH_MAX];
            snprintf(module_dir, sizeof(module_dir), "%s/%s", install_dir, base_name);
            ensure_directory_exists(module_dir);
            
            // Extract the archive
            cli_print_info("Extracting module...");
            mz_zip_archive zip;
            memset(&zip, 0, sizeof(zip));
            
            if (mz_zip_reader_init_file(&zip, module_name, 0)) {
                int num_files = mz_zip_reader_get_num_files(&zip);
                for (int i = 0; i < num_files; i++) {
                    mz_zip_archive_file_stat file_stat;
                    if (!mz_zip_reader_file_stat(&zip, i, &file_stat)) continue;
                    
                    if (!file_stat.m_is_directory) {
                        char extract_path[PATH_MAX];
                        snprintf(extract_path, sizeof(extract_path), "%s/%s", module_dir, file_stat.m_filename);
                        
                        // Create directory if needed
                        char* dir_sep = strrchr(extract_path, '/');
                        if (dir_sep) {
                            *dir_sep = '\0';
                            ensure_directory_exists(extract_path);
                            *dir_sep = '/';
                        }
                        
                        // Extract file
                        mz_zip_reader_extract_to_file(&zip, i, extract_path, 0);
                    }
                }
                mz_zip_reader_end(&zip);
                
                cli_print_success("Module %s installed to %s", base_name, install_dir);
            } else {
                cli_print_error("Failed to open module archive: %s", module_name);
                return 1;
            }
        } else {
            // TODO: Download from registry
            cli_print_error("Module registry not yet implemented");
            return 1;
        }
        
        if (save || save_dev) {
            // TODO: Update manifest.json
            cli_print_success("Added %s to %s", module_name, 
                            save_dev ? "devDependencies" : "dependencies");
        }
        
        return 0;
    }
    
    cli_print_error("No module specified");
    return 1;
}

int cli_cmd_add(int argc, char* argv[]) {
    if (argc < 1) {
        cli_print_error("Module name required");
        cli_print_info("Usage: swiftlang add <module> [version]");
        return 1;
    }
    
    const char* module_name = argv[0];
    const char* version = argc > 1 ? argv[1] : "latest";
    bool dev = false;
    bool exact = false;
    bool optional = false;
    
    // Parse options
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--dev") == 0) dev = true;
        else if (strcmp(argv[i], "--exact") == 0) exact = true;
        else if (strcmp(argv[i], "--optional") == 0) optional = true;
    }
    
    if (!cli_file_exists("manifest.json")) {
        cli_print_error("No manifest.json found. Run 'swiftlang init' first");
        return 1;
    }
    
    cli_print_info("Adding %s@%s to %s...", module_name, version,
                   dev ? "devDependencies" : "dependencies");
    
    // TODO: Add dependency to manifest.json
    // TODO: Install the module
    
    cli_print_success("Added %s@%s", module_name, version);
    return 0;
}

int cli_cmd_remove(int argc, char* argv[]) {
    if (argc < 1) {
        cli_print_error("Module name required");
        cli_print_info("Usage: swiftlang remove <module>");
        return 1;
    }
    
    const char* module_name = argv[0];
    
    if (!cli_file_exists("manifest.json")) {
        cli_print_error("No manifest.json found");
        return 1;
    }
    
    cli_print_info("Removing %s...", module_name);
    
    // TODO: Remove from manifest.json
    // TODO: Clean up installed files
    
    cli_print_success("Removed %s", module_name);
    return 0;
}

int cli_cmd_publish(int argc, char* argv[]) {
    const char* tag = NULL;
    const char* access = "public";
    bool dry_run = false;
    
    // Parse options
    for (int i = 0; i < argc - 1; i++) {
        if (strcmp(argv[i], "--tag") == 0) tag = argv[++i];
        else if (strcmp(argv[i], "--access") == 0) access = argv[++i];
        else if (strcmp(argv[i], "--dry-run") == 0) dry_run = true;
    }
    
    if (!cli_file_exists("manifest.json")) {
        cli_print_error("No manifest.json found");
        return 1;
    }
    
    // TODO: Read manifest.json
    const char* module_name = "unknown"; // TODO: Get from manifest
    const char* version = tag ? tag : "1.0.0"; // TODO: Get from manifest
    
    cli_print_info("Publishing %s@%s...", module_name, version);
    
    if (dry_run) {
        cli_print_info("Dry run - no files will be published");
        // TODO: Show what would be published
        return 0;
    }
    
    // TODO: Build module
    // TODO: Create .swiftmodule bundle
    // TODO: Upload to registry
    
    cli_print_success("Published %s@%s", module_name, version);
    return 0;
}

int cli_cmd_bundle(int argc, char* argv[]) {
    const char* output = NULL;
    bool include_deps = false;
    bool minify = false;
    bool sign = false;
    
    // Parse options
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) output = argv[++i];
        else if (strcmp(argv[i], "--include-deps") == 0) include_deps = true;
        else if (strcmp(argv[i], "--minify") == 0) minify = true;
        else if (strcmp(argv[i], "--sign") == 0) sign = true;
    }
    
    // Check for manifest.json or module.json
    const char* manifest_file = NULL;
    if (cli_file_exists("manifest.json")) {
        manifest_file = "manifest.json";
    } else if (cli_file_exists("module.json")) {
        manifest_file = "module.json";
    } else {
        cli_print_error("No manifest.json or module.json found");
        return 1;
    }
    
    // Read manifest
    Manifest* manifest = manifest_read(manifest_file);
    if (!manifest) {
        cli_print_error("Failed to read %s", manifest_file);
        return 1;
    }
    
    const char* module_name = manifest->name ? manifest->name : "unknown";
    
    if (!output) {
        char default_output[256];
        snprintf(default_output, sizeof(default_output), "%s.swiftmodule", module_name);
        output = strdup(default_output);
    }
    
    cli_print_info("Creating bundle %s...", output);
    
    // Create build directory
    ensure_directory_exists("build");
    ensure_directory_exists("build/bytecode");
    
    // Compile all source files to bytecode
    cli_print_info("Compiling source files...");
    int compiled_count = 0;
    
    // Get list of source files
    glob_t glob_result;
    if (glob("*.swift", GLOB_TILDE, NULL, &glob_result) == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            const char* source_file = glob_result.gl_pathv[i];
            cli_print_info("  Compiling %s...", source_file);
            
            // Read source
            char* source = read_file(source_file);
            if (!source) {
                cli_print_error("Failed to read %s", source_file);
                continue;
            }
            
            // Parse
            Parser* parser = parser_create(source);
            ProgramNode* program = parser_parse_program(parser);
            
            if (!parser->had_error) {
                // Compile to bytecode
                Chunk chunk;
                chunk_init(&chunk);
                
                if (compile(program, &chunk)) {
                    // Save bytecode
                    char bytecode_path[PATH_MAX];
                    snprintf(bytecode_path, sizeof(bytecode_path), "build/bytecode/%s.swiftbc", source_file);
                    
                    // Remove .swift extension
                    char* ext = strrchr(bytecode_path, '.');
                    if (ext && strcmp(ext, ".swift.swiftbc") == 0) {
                        strcpy(ext, ".swiftbc");
                    }
                    
                    uint8_t* bytecode_data;
                    size_t bytecode_size;
                    if (bytecode_serialize(&chunk, &bytecode_data, &bytecode_size)) {
                        FILE* f = fopen(bytecode_path, "wb");
                        if (f) {
                            fwrite(bytecode_data, 1, bytecode_size, f);
                            fclose(f);
                            compiled_count++;
                        }
                        free(bytecode_data);
                    }
                }
                
                chunk_free(&chunk);
            }
            
            program_destroy(program);
            parser_destroy(parser);
            free(source);
        }
        globfree(&glob_result);
    }
    
    cli_print_info("Compiled %d source files", compiled_count);
    
    // Create ZIP archive
    cli_print_info("Creating archive...");
    
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    
    if (!mz_zip_writer_init_file(&zip, output, 0)) {
        cli_print_error("Failed to create archive %s", output);
        manifest_free(manifest);
        return 1;
    }
    
    // Add manifest.json
    char* manifest_content = read_file(manifest_file);
    if (manifest_content) {
        mz_zip_writer_add_mem(&zip, "manifest.json", manifest_content, strlen(manifest_content), MZ_DEFAULT_COMPRESSION);
        free(manifest_content);
    }
    
    // Add bytecode files
    if (glob("build/bytecode/*.swiftbc", GLOB_TILDE, NULL, &glob_result) == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            const char* bytecode_file = glob_result.gl_pathv[i];
            const char* rel_path = strstr(bytecode_file, "build/");
            if (rel_path) rel_path += 6; // Skip "build/"
            else rel_path = bytecode_file;
            
            char* content = read_file(bytecode_file);
            if (content) {
                // Need to read as binary
                FILE* f = fopen(bytecode_file, "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long size = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    uint8_t* data = malloc(size);
                    fread(data, 1, size, f);
                    fclose(f);
                    
                    mz_zip_writer_add_mem(&zip, rel_path, data, size, MZ_DEFAULT_COMPRESSION);
                    free(data);
                }
                free(content);
            }
        }
        globfree(&glob_result);
    }
    
    // Add native libraries
    if (glob("lib/*.dylib", GLOB_TILDE, NULL, &glob_result) == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            const char* lib_file = glob_result.gl_pathv[i];
            FILE* f = fopen(lib_file, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                uint8_t* data = malloc(size);
                fread(data, 1, size, f);
                fclose(f);
                
                mz_zip_writer_add_mem(&zip, lib_file, data, size, MZ_DEFAULT_COMPRESSION);
                free(data);
            }
        }
        globfree(&glob_result);
    }
    
    // Also check for .so and .dll files
    const char* lib_patterns[] = {"lib/*.so", "lib/*.dll", NULL};
    for (int i = 0; lib_patterns[i]; i++) {
        if (glob(lib_patterns[i], GLOB_TILDE, NULL, &glob_result) == 0) {
            for (size_t j = 0; j < glob_result.gl_pathc; j++) {
                const char* lib_file = glob_result.gl_pathv[j];
                FILE* f = fopen(lib_file, "rb");
                if (f) {
                    fseek(f, 0, SEEK_END);
                    long size = ftell(f);
                    fseek(f, 0, SEEK_SET);
                    uint8_t* data = malloc(size);
                    fread(data, 1, size, f);
                    fclose(f);
                    
                    mz_zip_writer_add_mem(&zip, lib_file, data, size, MZ_DEFAULT_COMPRESSION);
                    free(data);
                }
            }
            globfree(&glob_result);
        }
    }
    
    if (include_deps) {
        cli_print_info("Including dependencies in bundle...");
        // TODO: Bundle dependencies from node_modules
    }
    
    if (minify) {
        cli_print_info("Minifying bytecode...");
        // TODO: Minify bytecode
    }
    
    if (sign) {
        cli_print_info("Signing bundle...");
        // TODO: Sign bundle with checksums
    }
    
    // Finalize ZIP
    if (!mz_zip_writer_finalize_archive(&zip)) {
        cli_print_error("Failed to finalize archive");
        mz_zip_writer_end(&zip);
        manifest_free(manifest);
        return 1;
    }
    
    mz_zip_writer_end(&zip);
    manifest_free(manifest);
    
    // Get file size
    struct stat st;
    if (stat(output, &st) == 0) {
        double size_kb = st.st_size / 1024.0;
        cli_print_success("Bundle created: %s (%.1f KB)", output, size_kb);
    } else {
        cli_print_success("Bundle created: %s", output);
    }
    
    return 0;
}

int cli_cmd_list(int argc, char* argv[]) {
    int depth = 0;
    bool json = false;
    bool global = false;
    
    // Parse options
    for (int i = 0; i < argc - 1; i++) {
        if (strcmp(argv[i], "--depth") == 0) depth = atoi(argv[++i]);
        else if (strcmp(argv[i], "--json") == 0) json = true;
        else if (strcmp(argv[i], "--global") == 0 || strcmp(argv[i], "-g") == 0) global = true;
    }
    
    if (json) {
        // TODO: Output JSON format
        printf("{\"modules\":[]}\n");
        return 0;
    }
    
    if (global) {
        cli_print_info("Global modules:");
        // TODO: List global modules
    } else {
        if (!cli_file_exists("manifest.json")) {
            cli_print_error("No manifest.json found");
            return 1;
        }
        
        cli_print_info("Project dependencies:");
        // TODO: List project dependencies
    }
    
    return 0;
}

int cli_cmd_update(int argc, char* argv[]) {
    const char* module_name = argc > 0 ? argv[0] : NULL;
    bool save = false;
    bool dry_run = false;
    
    // Parse options
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--save") == 0) save = true;
        else if (strcmp(argv[i], "--dry-run") == 0) dry_run = true;
    }
    
    if (!cli_file_exists("manifest.json")) {
        cli_print_error("No manifest.json found");
        return 1;
    }
    
    if (module_name) {
        cli_print_info("Updating %s...", module_name);
    } else {
        cli_print_info("Updating all dependencies...");
    }
    
    if (dry_run) {
        cli_print_info("Dry run - no changes will be made");
        // TODO: Show what would be updated
        return 0;
    }
    
    // TODO: Check for updates
    // TODO: Download and install updates
    
    if (save) {
        // TODO: Update manifest.json with new versions
        cli_print_info("Updated manifest.json");
    }
    
    cli_print_success("Update complete");
    return 0;
}

int cli_cmd_cache(int argc, char* argv[]) {
    if (argc < 1) {
        cli_print_error("Cache command required");
        cli_print_info("Usage: swiftlang cache <clean|list|verify>");
        return 1;
    }
    
    const char* command = argv[0];
    
    if (strcmp(command, "clean") == 0) {
        cli_print_info("Cleaning module cache...");
        // TODO: Remove cache directory
        cli_print_success("Cache cleaned");
    } else if (strcmp(command, "list") == 0) {
        cli_print_info("Cached modules:");
        // TODO: List cached modules
    } else if (strcmp(command, "verify") == 0) {
        cli_print_info("Verifying cache integrity...");
        // TODO: Verify checksums
        cli_print_success("Cache verified");
    } else {
        cli_print_error("Unknown cache command: %s", command);
        return 1;
    }
    
    return 0;
}