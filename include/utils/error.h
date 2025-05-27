#ifndef LANG_ERROR_H
#define LANG_ERROR_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    ERROR_LEVEL_WARNING,
    ERROR_LEVEL_ERROR,
    ERROR_LEVEL_FATAL
} ErrorLevel;

typedef enum {
    ERROR_PHASE_LEXER,
    ERROR_PHASE_PARSER,
    ERROR_PHASE_SEMANTIC,
    ERROR_PHASE_CODEGEN,
    ERROR_PHASE_RUNTIME
} ErrorPhase;

typedef struct {
    const char* filename;
    size_t line;
    size_t column;
    size_t length;
} SourceLocation;

typedef struct {
    ErrorLevel level;
    ErrorPhase phase;
    const char* message;
    SourceLocation location;
    const char* suggestion;
} ErrorInfo;

typedef struct ErrorReporter ErrorReporter;

ErrorReporter* error_reporter_create(void);
void error_reporter_destroy(ErrorReporter* reporter);

void error_report(ErrorReporter* reporter, const ErrorInfo* info);
void error_report_simple(ErrorReporter* reporter, ErrorLevel level, ErrorPhase phase,
                        const char* filename, size_t line, size_t column,
                        const char* format, ...);

void error_set_source(ErrorReporter* reporter, const char* filename, const char* source);
void error_print_context(ErrorReporter* reporter, const SourceLocation* location);

bool error_has_errors(const ErrorReporter* reporter);
bool error_has_fatal(const ErrorReporter* reporter);
size_t error_count(const ErrorReporter* reporter);
size_t warning_count(const ErrorReporter* reporter);

void error_clear(ErrorReporter* reporter);

void error_enable_color(ErrorReporter* reporter, bool enable);
void error_set_max_errors(ErrorReporter* reporter, size_t max);

#endif