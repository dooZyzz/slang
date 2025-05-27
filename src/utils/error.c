#include "utils/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define MAX_ERRORS_DEFAULT 100
#define CONTEXT_LINES 2

typedef struct {
    char* filename;
    char* source;
} SourceFile;

typedef struct {
    ErrorInfo* errors;
    size_t count;
    size_t capacity;
} ErrorList;

struct ErrorReporter {
    ErrorList errors;
    ErrorList warnings;
    SourceFile* sources;
    size_t source_count;
    size_t source_capacity;
    bool color_enabled;
    size_t max_errors;
    bool fatal_encountered;
};

static const char* level_strings[] = {
    [ERROR_LEVEL_WARNING] = "warning",
    [ERROR_LEVEL_ERROR] = "error",
    [ERROR_LEVEL_FATAL] = "fatal error"
};


static const char* color_red = "\033[31m";
static const char* color_yellow = "\033[33m";
static const char* color_cyan = "\033[36m";
static const char* color_bold = "\033[1m";
static const char* color_reset = "\033[0m";

ErrorReporter* error_reporter_create(void) {
    ErrorReporter* reporter = calloc(1, sizeof(ErrorReporter));
    if (!reporter) return NULL;
    
    reporter->errors.capacity = 16;
    reporter->errors.errors = calloc(reporter->errors.capacity, sizeof(ErrorInfo));
    
    reporter->warnings.capacity = 16;
    reporter->warnings.errors = calloc(reporter->warnings.capacity, sizeof(ErrorInfo));
    
    reporter->source_capacity = 4;
    reporter->sources = calloc(reporter->source_capacity, sizeof(SourceFile));
    
    reporter->color_enabled = true;
    reporter->max_errors = MAX_ERRORS_DEFAULT;
    
    return reporter;
}

void error_reporter_destroy(ErrorReporter* reporter) {
    if (!reporter) return;
    
    for (size_t i = 0; i < reporter->errors.count; i++) {
        free((void*)reporter->errors.errors[i].message);
        free((void*)reporter->errors.errors[i].suggestion);
    }
    free(reporter->errors.errors);
    
    for (size_t i = 0; i < reporter->warnings.count; i++) {
        free((void*)reporter->warnings.errors[i].message);
        free((void*)reporter->warnings.errors[i].suggestion);
    }
    free(reporter->warnings.errors);
    
    for (size_t i = 0; i < reporter->source_count; i++) {
        free(reporter->sources[i].filename);
        free(reporter->sources[i].source);
    }
    free(reporter->sources);
    
    free(reporter);
}

static void error_list_add(ErrorList* list, const ErrorInfo* info) {
    if (list->count >= list->capacity) {
        list->capacity *= 2;
        list->errors = realloc(list->errors, list->capacity * sizeof(ErrorInfo));
    }
    
    ErrorInfo* error = &list->errors[list->count++];
    *error = *info;
    error->message = strdup(info->message);
    if (info->suggestion) {
        error->suggestion = strdup(info->suggestion);
    }
}

static const char* get_color(const ErrorReporter* reporter, const char* color) {
    return reporter->color_enabled ? color : "";
}

static void print_error_header(const ErrorReporter* reporter, const ErrorInfo* info) {
    const char* level_color = "";
    switch (info->level) {
        case ERROR_LEVEL_WARNING:
            level_color = get_color(reporter, color_yellow);
            break;
        case ERROR_LEVEL_ERROR:
        case ERROR_LEVEL_FATAL:
            level_color = get_color(reporter, color_red);
            break;
    }
    
    const char* level_str = level_strings[info->level];
    
    fprintf(stderr, "%s%s:%s %s%s%s: %s\n",
            get_color(reporter, color_bold),
            info->location.filename ? info->location.filename : "<unknown>",
            get_color(reporter, color_reset),
            level_color,
            level_str,
            get_color(reporter, color_reset),
            info->message);
    
    if (info->location.filename && info->location.line > 0) {
        fprintf(stderr, "  %s-->%s %s:%zu:%zu\n",
                get_color(reporter, color_cyan),
                get_color(reporter, color_reset),
                info->location.filename,
                info->location.line,
                info->location.column);
    }
}

static SourceFile* find_source(ErrorReporter* reporter, const char* filename) {
    for (size_t i = 0; i < reporter->source_count; i++) {
        if (strcmp(reporter->sources[i].filename, filename) == 0) {
            return &reporter->sources[i];
        }
    }
    return NULL;
}

void error_report(ErrorReporter* reporter, const ErrorInfo* info) {
    if (!reporter || !info) return;
    
    if (reporter->fatal_encountered) return;
    
    if (info->level == ERROR_LEVEL_FATAL) {
        reporter->fatal_encountered = true;
    }
    
    if (info->level == ERROR_LEVEL_WARNING) {
        error_list_add(&reporter->warnings, info);
    } else {
        if (reporter->errors.count >= reporter->max_errors) {
            if (reporter->errors.count == reporter->max_errors) {
                fprintf(stderr, "\nToo many errors. Compilation stopped.\n");
            }
            return;
        }
        error_list_add(&reporter->errors, info);
    }
    
    print_error_header(reporter, info);
    error_print_context(reporter, &info->location);
    
    if (info->suggestion) {
        fprintf(stderr, "\n%ssuggestion:%s %s\n",
                get_color(reporter, color_cyan),
                get_color(reporter, color_reset),
                info->suggestion);
    }
    
    fprintf(stderr, "\n");
}

void error_report_simple(ErrorReporter* reporter, ErrorLevel level, ErrorPhase phase,
                        const char* filename, size_t line, size_t column,
                        const char* format, ...) {
    char buffer[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    
    ErrorInfo info = {
        .level = level,
        .phase = phase,
        .message = buffer,
        .location = {
            .filename = filename,
            .line = line,
            .column = column,
            .length = 1
        },
        .suggestion = NULL
    };
    
    error_report(reporter, &info);
}

void error_set_source(ErrorReporter* reporter, const char* filename, const char* source) {
    if (!reporter || !filename || !source) return;
    
    SourceFile* existing = find_source(reporter, filename);
    if (existing) {
        free(existing->source);
        existing->source = strdup(source);
        return;
    }
    
    if (reporter->source_count >= reporter->source_capacity) {
        reporter->source_capacity *= 2;
        reporter->sources = realloc(reporter->sources,
                                   reporter->source_capacity * sizeof(SourceFile));
    }
    
    SourceFile* file = &reporter->sources[reporter->source_count++];
    file->filename = strdup(filename);
    file->source = strdup(source);
}

void error_print_context(ErrorReporter* reporter, const SourceLocation* location) {
    if (!reporter || !location || !location->filename || location->line == 0) return;
    
    SourceFile* source = find_source(reporter, location->filename);
    if (!source || !source->source) return;
    
    const char* text = source->source;
    size_t line = 1;
    const char* line_start = text;
    const char* line_end = NULL;
    
    while (*text && line < location->line) {
        if (*text == '\n') {
            line++;
            line_start = text + 1;
        }
        text++;
    }
    
    line_end = line_start;
    while (*line_end && *line_end != '\n') {
        line_end++;
    }
    
    char line_num[16];
    snprintf(line_num, sizeof(line_num), "%zu", location->line);
    size_t num_width = strlen(line_num);
    
    fprintf(stderr, "%*s %s|%s\n", (int)num_width, "", 
            get_color(reporter, color_cyan), get_color(reporter, color_reset));
    
    fprintf(stderr, "%s%*zu |%s ",
            get_color(reporter, color_cyan), (int)num_width, location->line,
            get_color(reporter, color_reset));
    
    fwrite(line_start, 1, line_end - line_start, stderr);
    fprintf(stderr, "\n");
    
    fprintf(stderr, "%*s %s|%s ", (int)num_width, "",
            get_color(reporter, color_cyan), get_color(reporter, color_reset));
    
    for (size_t i = 0; i < location->column - 1; i++) {
        fprintf(stderr, " ");
    }
    
    fprintf(stderr, "%s^", get_color(reporter, color_red));
    for (size_t i = 1; i < location->length; i++) {
        fprintf(stderr, "~");
    }
    fprintf(stderr, "%s\n", get_color(reporter, color_reset));
}

bool error_has_errors(const ErrorReporter* reporter) {
    return reporter && reporter->errors.count > 0;
}

bool error_has_fatal(const ErrorReporter* reporter) {
    return reporter && reporter->fatal_encountered;
}

size_t error_count(const ErrorReporter* reporter) {
    return reporter ? reporter->errors.count : 0;
}

size_t warning_count(const ErrorReporter* reporter) {
    return reporter ? reporter->warnings.count : 0;
}

void error_clear(ErrorReporter* reporter) {
    if (!reporter) return;
    
    for (size_t i = 0; i < reporter->errors.count; i++) {
        free((void*)reporter->errors.errors[i].message);
        free((void*)reporter->errors.errors[i].suggestion);
    }
    reporter->errors.count = 0;
    
    for (size_t i = 0; i < reporter->warnings.count; i++) {
        free((void*)reporter->warnings.errors[i].message);
        free((void*)reporter->warnings.errors[i].suggestion);
    }
    reporter->warnings.count = 0;
    
    reporter->fatal_encountered = false;
}

void error_enable_color(ErrorReporter* reporter, bool enable) {
    if (reporter) {
        reporter->color_enabled = enable;
    }
}

void error_set_max_errors(ErrorReporter* reporter, size_t max) {
    if (reporter) {
        reporter->max_errors = max;
    }
}