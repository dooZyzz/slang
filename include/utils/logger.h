#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// Log levels
typedef enum {
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_WARN = 3,
    LOG_LEVEL_ERROR = 4,
    LOG_LEVEL_FATAL = 5,
    LOG_LEVEL_NONE = 6
} LogLevel;

// Log categories/modules
typedef enum {
    LOG_MODULE_ALL = 0,
    LOG_MODULE_LEXER = 1 << 0,
    LOG_MODULE_PARSER = 1 << 1,
    LOG_MODULE_COMPILER = 1 << 2,
    LOG_MODULE_VM = 1 << 3,
    LOG_MODULE_MODULE_LOADER = 1 << 4,
    LOG_MODULE_BOOTSTRAP = 1 << 5,
    LOG_MODULE_MEMORY = 1 << 6,
    LOG_MODULE_GC = 1 << 7,
    LOG_MODULE_OPTIMIZER = 1 << 8,
    LOG_MODULE_BYTECODE = 1 << 9,
    LOG_MODULE_STDLIB = 1 << 10,
    LOG_MODULE_PACKAGE = 1 << 11,
    LOG_MODULE_CLI = 1 << 12,
} LogModule;

// Logger configuration
typedef struct {
    LogLevel min_level;
    unsigned int enabled_modules;
    bool use_colors;
    bool show_timestamp;
    bool show_file_line;
    bool show_module;
    FILE* output_file;
    const char* log_file_path;
} LoggerConfig;

// Global logger instance
extern LoggerConfig g_logger_config;

// Logger initialization and configuration
void logger_init(void);
void logger_shutdown(void);
void logger_set_level(LogLevel level);
void logger_enable_module(LogModule module);
void logger_disable_module(LogModule module);
void logger_enable_all_modules(void);
void logger_disable_all_modules(void);
void logger_set_output_file(const char* path);
void logger_configure(LoggerConfig* config);

// Logging macros
#define LOG_TRACE(module, ...) logger_log(LOG_LEVEL_TRACE, module, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(module, ...) logger_log(LOG_LEVEL_DEBUG, module, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(module, ...) logger_log(LOG_LEVEL_INFO, module, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(module, ...) logger_log(LOG_LEVEL_WARN, module, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(module, ...) logger_log(LOG_LEVEL_ERROR, module, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_FATAL(module, ...) logger_log(LOG_LEVEL_FATAL, module, __FILE__, __LINE__, __VA_ARGS__)

// Convenience macros for each module
#define LOG_LEXER(...) LOG_DEBUG(LOG_MODULE_LEXER, __VA_ARGS__)
#define LOG_PARSER(...) LOG_DEBUG(LOG_MODULE_PARSER, __VA_ARGS__)
#define LOG_COMPILER(...) LOG_DEBUG(LOG_MODULE_COMPILER, __VA_ARGS__)
#define LOG_VM(...) LOG_DEBUG(LOG_MODULE_VM, __VA_ARGS__)
#define LOG_MODULE(...) LOG_DEBUG(LOG_MODULE_MODULE_LOADER, __VA_ARGS__)
#define LOG_PACKAGE(...) LOG_DEBUG(LOG_MODULE_PACKAGE, __VA_ARGS__)

// Core logging function
void logger_log(LogLevel level, LogModule module, const char* file, int line, const char* fmt, ...);

// Utility functions
const char* logger_level_to_string(LogLevel level);
const char* logger_module_to_string(LogModule module);
LogLevel logger_string_to_level(const char* str);
LogModule logger_string_to_module(const char* str);

// Pretty printing helpers
void logger_dump_bytecode(const uint8_t* code, size_t size);
void logger_dump_value(const char* name, void* value);
void logger_enter_scope(const char* scope_name);
void logger_exit_scope(const char* scope_name);

#endif // LOGGER_H