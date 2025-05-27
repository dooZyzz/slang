#include "utils/logger.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <strings.h>

// ANSI color codes
#define COLOR_RESET   "\x1b[0m"
#define COLOR_RED     "\x1b[31m"
#define COLOR_GREEN   "\x1b[32m"
#define COLOR_YELLOW  "\x1b[33m"
#define COLOR_BLUE    "\x1b[34m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_CYAN    "\x1b[36m"
#define COLOR_WHITE   "\x1b[37m"
#define COLOR_GRAY    "\x1b[90m"

// Global logger configuration
LoggerConfig g_logger_config = {
    .min_level = LOG_LEVEL_INFO,
    .enabled_modules = LOG_MODULE_ALL,
    .use_colors = true,
    .show_timestamp = false,
    .show_file_line = false,
    .show_module = true,
    .output_file = NULL,
    .log_file_path = NULL
};

static int scope_depth = 0;

void logger_init(void) {
    // Check if we're in a terminal for color support
    g_logger_config.use_colors = isatty(fileno(stderr));
    
    // Check environment variables
    const char* log_level = getenv("SWIFTLANG_LOG_LEVEL");
    if (log_level) {
        g_logger_config.min_level = logger_string_to_level(log_level);
    }
    
    const char* log_modules = getenv("SWIFTLANG_LOG_MODULES");
    if (log_modules) {
        if (strcmp(log_modules, "all") == 0) {
            logger_enable_all_modules();
        } else if (strcmp(log_modules, "none") == 0) {
            logger_disable_all_modules();
        } else {
            // Parse comma-separated list
            char* modules_copy = strdup(log_modules);
            char* token = strtok(modules_copy, ",");
            logger_disable_all_modules();
            while (token) {
                LogModule module = logger_string_to_module(token);
                if (module != 0) {
                    logger_enable_module(module);
                }
                token = strtok(NULL, ",");
            }
            free(modules_copy);
        }
    }
    
    const char* log_file = getenv("SWIFTLANG_LOG_FILE");
    if (log_file) {
        logger_set_output_file(log_file);
    }
}

void logger_shutdown(void) {
    if (g_logger_config.output_file && g_logger_config.output_file != stderr) {
        fclose(g_logger_config.output_file);
        g_logger_config.output_file = NULL;
    }
}

void logger_set_level(LogLevel level) {
    g_logger_config.min_level = level;
}

void logger_enable_module(LogModule module) {
    g_logger_config.enabled_modules |= module;
}

void logger_disable_module(LogModule module) {
    g_logger_config.enabled_modules &= ~module;
}

void logger_enable_all_modules(void) {
    g_logger_config.enabled_modules = ~0u;
}

void logger_disable_all_modules(void) {
    g_logger_config.enabled_modules = 0;
}

void logger_set_output_file(const char* path) {
    if (g_logger_config.output_file && g_logger_config.output_file != stderr) {
        fclose(g_logger_config.output_file);
    }
    
    g_logger_config.output_file = fopen(path, "w");
    if (!g_logger_config.output_file) {
        g_logger_config.output_file = stderr;
        fprintf(stderr, "Failed to open log file: %s\n", path);
    } else {
        g_logger_config.log_file_path = path;
    }
}

void logger_configure(LoggerConfig* config) {
    if (config) {
        g_logger_config = *config;
    }
}

static const char* get_level_color(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_TRACE: return COLOR_GRAY;
        case LOG_LEVEL_DEBUG: return COLOR_CYAN;
        case LOG_LEVEL_INFO:  return COLOR_GREEN;
        case LOG_LEVEL_WARN:  return COLOR_YELLOW;
        case LOG_LEVEL_ERROR: return COLOR_RED;
        case LOG_LEVEL_FATAL: return COLOR_MAGENTA;
        default: return COLOR_RESET;
    }
}

void logger_log(LogLevel level, LogModule module, const char* file, int line, const char* fmt, ...) {
    // Check if we should log this
    if (level < g_logger_config.min_level) return;
    if (module != LOG_MODULE_ALL && !(g_logger_config.enabled_modules & module)) return;
    
    FILE* out = g_logger_config.output_file ? g_logger_config.output_file : stderr;
    
    // Timestamp
    if (g_logger_config.show_timestamp) {
        time_t now;
        time(&now);
        struct tm* tm_info = localtime(&now);
        char timestamp[32];
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(out, "[%s] ", timestamp);
    }
    
    // Level
    const char* level_str = logger_level_to_string(level);
    if (g_logger_config.use_colors) {
        fprintf(out, "%s%-5s%s ", get_level_color(level), level_str, COLOR_RESET);
    } else {
        fprintf(out, "%-5s ", level_str);
    }
    
    // Module
    if (g_logger_config.show_module && module != LOG_MODULE_ALL) {
        const char* module_str = logger_module_to_string(module);
        if (g_logger_config.use_colors) {
            fprintf(out, "[%s%-12s%s] ", COLOR_BLUE, module_str, COLOR_RESET);
        } else {
            fprintf(out, "[%-12s] ", module_str);
        }
    }
    
    // File and line
    if (g_logger_config.show_file_line) {
        const char* filename = strrchr(file, '/');
        filename = filename ? filename + 1 : file;
        fprintf(out, "%s:%d: ", filename, line);
    }
    
    // Indentation for scopes
    for (int i = 0; i < scope_depth; i++) {
        fprintf(out, "  ");
    }
    
    // Message
    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);
    
    fprintf(out, "\n");
    fflush(out);
}

const char* logger_level_to_string(LogLevel level) {
    switch (level) {
        case LOG_LEVEL_TRACE: return "TRACE";
        case LOG_LEVEL_DEBUG: return "DEBUG";
        case LOG_LEVEL_INFO:  return "INFO";
        case LOG_LEVEL_WARN:  return "WARN";
        case LOG_LEVEL_ERROR: return "ERROR";
        case LOG_LEVEL_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

const char* logger_module_to_string(LogModule module) {
    switch (module) {
        case LOG_MODULE_LEXER: return "LEXER";
        case LOG_MODULE_PARSER: return "PARSER";
        case LOG_MODULE_COMPILER: return "COMPILER";
        case LOG_MODULE_VM: return "VM";
        case LOG_MODULE_MODULE_LOADER: return "MODULE";
        case LOG_MODULE_BOOTSTRAP: return "BOOTSTRAP";
        case LOG_MODULE_MEMORY: return "MEMORY";
        case LOG_MODULE_GC: return "GC";
        case LOG_MODULE_OPTIMIZER: return "OPTIMIZER";
        case LOG_MODULE_BYTECODE: return "BYTECODE";
        case LOG_MODULE_STDLIB: return "STDLIB";
        case LOG_MODULE_PACKAGE: return "PACKAGE";
        case LOG_MODULE_CLI: return "CLI";
        default: return "UNKNOWN";
    }
}

LogLevel logger_string_to_level(const char* str) {
    if (strcasecmp(str, "trace") == 0) return LOG_LEVEL_TRACE;
    if (strcasecmp(str, "debug") == 0) return LOG_LEVEL_DEBUG;
    if (strcasecmp(str, "info") == 0) return LOG_LEVEL_INFO;
    if (strcasecmp(str, "warn") == 0) return LOG_LEVEL_WARN;
    if (strcasecmp(str, "error") == 0) return LOG_LEVEL_ERROR;
    if (strcasecmp(str, "fatal") == 0) return LOG_LEVEL_FATAL;
    if (strcasecmp(str, "none") == 0) return LOG_LEVEL_NONE;
    return LOG_LEVEL_INFO; // default
}

LogModule logger_string_to_module(const char* str) {
    if (strcasecmp(str, "lexer") == 0) return LOG_MODULE_LEXER;
    if (strcasecmp(str, "parser") == 0) return LOG_MODULE_PARSER;
    if (strcasecmp(str, "compiler") == 0) return LOG_MODULE_COMPILER;
    if (strcasecmp(str, "vm") == 0) return LOG_MODULE_VM;
    if (strcasecmp(str, "module") == 0) return LOG_MODULE_MODULE_LOADER;
    if (strcasecmp(str, "bootstrap") == 0) return LOG_MODULE_BOOTSTRAP;
    if (strcasecmp(str, "memory") == 0) return LOG_MODULE_MEMORY;
    if (strcasecmp(str, "gc") == 0) return LOG_MODULE_GC;
    if (strcasecmp(str, "optimizer") == 0) return LOG_MODULE_OPTIMIZER;
    if (strcasecmp(str, "bytecode") == 0) return LOG_MODULE_BYTECODE;
    if (strcasecmp(str, "stdlib") == 0) return LOG_MODULE_STDLIB;
    if (strcasecmp(str, "package") == 0) return LOG_MODULE_PACKAGE;
    if (strcasecmp(str, "cli") == 0) return LOG_MODULE_CLI;
    return 0;
}

void logger_dump_bytecode(const uint8_t* code, size_t size) {
    if (!(g_logger_config.enabled_modules & LOG_MODULE_BYTECODE)) return;
    
    FILE* out = g_logger_config.output_file ? g_logger_config.output_file : stderr;
    fprintf(out, "=== BYTECODE DUMP (%zu bytes) ===\n", size);
    
    for (size_t i = 0; i < size; i += 16) {
        fprintf(out, "%04zx: ", i);
        
        // Hex bytes
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            fprintf(out, "%02x ", code[i + j]);
        }
        
        // Padding
        for (size_t j = size - i; j < 16 && i + j >= size; j++) {
            fprintf(out, "   ");
        }
        
        // ASCII representation
        fprintf(out, " |");
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            uint8_t byte = code[i + j];
            fprintf(out, "%c", (byte >= 32 && byte < 127) ? byte : '.');
        }
        fprintf(out, "|\n");
    }
    
    fprintf(out, "=== END BYTECODE DUMP ===\n");
}

void logger_enter_scope(const char* scope_name) {
    LOG_TRACE(LOG_MODULE_ALL, ">>> Entering %s", scope_name);
    scope_depth++;
}

void logger_exit_scope(const char* scope_name) {
    scope_depth--;
    LOG_TRACE(LOG_MODULE_ALL, "<<< Exiting %s", scope_name);
}