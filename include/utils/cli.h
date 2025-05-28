#ifndef CLI_H
#define CLI_H

#include <stdbool.h>
#include "utils/logger.h"

// Command handler function type
typedef int (*CommandHandler)(int argc, char* argv[]);

// Command structure
typedef struct {
    const char* name;
    const char* description;
    CommandHandler handler;
    const char* usage;
    const char* help; // Extended help text
} Command;

// CLI configuration (parsed from command line)
typedef struct {
    // Input/Output
    const char* input_file;
    const char* output_dir;
    const char* build_dir;
    
    // Debug options
    bool debug_tokens;
    bool debug_ast;
    bool debug_bytecode;
    bool debug_trace;
    bool debug_optimizer;
    bool debug_all;
    
    // Logging options
    LogLevel log_level;
    unsigned int log_modules;
    const char* log_file;
    bool log_colors;
    bool log_timestamps;
    bool log_source_location;
    
    // Module options
    const char** module_paths;
    int module_path_count;
    
    // Build options
    bool optimize;
    bool emit_bytecode;
    bool emit_ast;
    const char* target;
    const char* format; // Archive format
    
    // Runtime options
    size_t stack_size;
    size_t heap_size;
    bool gc_stress_test;
    
    // Other options
    bool quiet;
    bool verbose;
    bool interactive;
    bool watch_mode;
    int jobs; // Parallel jobs
} CLIConfig;

// Global CLI configuration
extern CLIConfig g_cli_config;

// Command handlers
int cli_cmd_init(int argc, char* argv[]);
int cli_cmd_new(int argc, char* argv[]);
int cli_cmd_build(int argc, char* argv[]);
int cli_cmd_run(int argc, char* argv[]);
int cli_cmd_test(int argc, char* argv[]);
int cli_cmd_help(int argc, char* argv[]);
int cli_cmd_version(int argc, char* argv[]);
int cli_cmd_repl(int argc, char* argv[]);

// Main CLI entry point
int cli_main(int argc, char* argv[]);

// CLI utilities
void cli_parse_args(int argc, char* argv[]);
void cli_print_help(const char* program_name);
void cli_print_version(void);
void cli_print_banner(void);
void cli_print_error(const char* fmt, ...);
void cli_print_warning(const char* fmt, ...);
void cli_print_success(const char* fmt, ...);
void cli_print_info(const char* fmt, ...);

// REPL functionality
void cli_run_repl(void);

// File execution
int cli_run_file(const char* path);

// Progress indicators
typedef struct CLIProgress CLIProgress;
CLIProgress* cli_progress_start(const char* message, int total);
void cli_progress_update(CLIProgress* progress, int current);
void cli_progress_finish(CLIProgress* progress, const char* message);
void cli_progress_free(CLIProgress* progress);

// Interactive prompts
bool cli_confirm(const char* message);
char* cli_prompt(const char* message, const char* default_value);

// File and directory utilities
bool cli_file_exists(const char* path);
bool cli_dir_exists(const char* path);
bool cli_create_dir(const char* path);
bool cli_create_dir_recursive(const char* path);
char* cli_resolve_path(const char* path);
bool cli_copy_file(const char* src, const char* dst);

// Terminal utilities
int cli_get_terminal_width(void);
bool cli_is_terminal(void);

// Color codes for output
#define COLOR_RESET   "\x1b[0m"
#define COLOR_BOLD    "\x1b[1m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_WHITE   "\x1b[37m"

#endif // CLI_H