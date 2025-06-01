#include "utils/error.h"
#include "utils/allocators.h"
#include <stdio.h>
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
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
    
    ErrorReporter* reporter = MEM_ALLOC_ZERO(alloc, sizeof(ErrorReporter));
    if (!reporter) return NULL;
    
    reporter->errors.capacity = 16;
    reporter->errors.errors = MEM_ALLOC_ZERO(alloc, reporter->errors.capacity * sizeof(ErrorInfo));
    
    reporter->warnings.capacity = 16;
    reporter->warnings.errors = MEM_ALLOC_ZERO(alloc, reporter->warnings.capacity * sizeof(ErrorInfo));
    
    reporter->source_capacity = 4;
    reporter->sources = MEM_ALLOC_ZERO(alloc, reporter->source_capacity * sizeof(SourceFile));
    
    reporter->color_enabled = true;
    reporter->max_errors = MAX_ERRORS_DEFAULT;
    
    return reporter;
}

void error_reporter_destroy(ErrorReporter* reporter) {
    if (!reporter) return;
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
    
    // Free error messages and suggestions
    for (size_t i = 0; i < reporter->errors.count; i++) {
        if (reporter->errors.errors[i].message) {
            size_t msg_len = strlen(reporter->errors.errors[i].message) + 1;
            MEM_FREE(alloc, (void*)reporter->errors.errors[i].message, msg_len);
        }
        if (reporter->errors.errors[i].suggestion) {
            size_t sug_len = strlen(reporter->errors.errors[i].suggestion) + 1;
            MEM_FREE(alloc, (void*)reporter->errors.errors[i].suggestion, sug_len);
        }
    }
    MEM_FREE(alloc, reporter->errors.errors, reporter->errors.capacity * sizeof(ErrorInfo));
    
    // Free warning messages and suggestions
    for (size_t i = 0; i < reporter->warnings.count; i++) {
        if (reporter->warnings.errors[i].message) {
            size_t msg_len = strlen(reporter->warnings.errors[i].message) + 1;
            MEM_FREE(alloc, (void*)reporter->warnings.errors[i].message, msg_len);
        }
        if (reporter->warnings.errors[i].suggestion) {
            size_t sug_len = strlen(reporter->warnings.errors[i].suggestion) + 1;
            MEM_FREE(alloc, (void*)reporter->warnings.errors[i].suggestion, sug_len);
        }
    }
    MEM_FREE(alloc, reporter->warnings.errors, reporter->warnings.capacity * sizeof(ErrorInfo));
    
    // Free source files
    for (size_t i = 0; i < reporter->source_count; i++) {
        if (reporter->sources[i].filename) {
            size_t fname_len = strlen(reporter->sources[i].filename) + 1;
            MEM_FREE(alloc, reporter->sources[i].filename, fname_len);
        }
        if (reporter->sources[i].source) {
            size_t src_len = strlen(reporter->sources[i].source) + 1;
            MEM_FREE(alloc, reporter->sources[i].source, src_len);
        }
    }
    MEM_FREE(alloc, reporter->sources, reporter->source_capacity * sizeof(SourceFile));
    
    MEM_FREE(alloc, reporter, sizeof(ErrorReporter));
}

static void error_list_add(ErrorList* list, const ErrorInfo* info) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
    
    if (list->count >= list->capacity) {
        size_t old_capacity = list->capacity;
        list->capacity *= 2;
        
        // Allocate new array
        ErrorInfo* new_errors = MEM_ALLOC_ZERO(alloc, list->capacity * sizeof(ErrorInfo));
        
        // Copy old data
        if (list->errors) {
            memcpy(new_errors, list->errors, old_capacity * sizeof(ErrorInfo));
            MEM_FREE(alloc, list->errors, old_capacity * sizeof(ErrorInfo));
        }
        
        list->errors = new_errors;
    }
    
    ErrorInfo* error = &list->errors[list->count++];
    *error = *info;
    error->message = MEM_STRDUP(alloc, info->message);
    if (info->suggestion) {
        error->suggestion = MEM_STRDUP(alloc, info->suggestion);
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
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
    
    SourceFile* existing = find_source(reporter, filename);
    if (existing) {
        // Free old source
        if (existing->source) {
            size_t old_len = strlen(existing->source) + 1;
            MEM_FREE(alloc, existing->source, old_len);
        }
        existing->source = MEM_STRDUP(alloc, source);
        return;
    }
    
    if (reporter->source_count >= reporter->source_capacity) {
        size_t old_capacity = reporter->source_capacity;
        reporter->source_capacity *= 2;
        
        // Allocate new array
        SourceFile* new_sources = MEM_ALLOC_ZERO(alloc, reporter->source_capacity * sizeof(SourceFile));
        
        // Copy old data
        if (reporter->sources) {
            memcpy(new_sources, reporter->sources, old_capacity * sizeof(SourceFile));
            MEM_FREE(alloc, reporter->sources, old_capacity * sizeof(SourceFile));
        }
        
        reporter->sources = new_sources;
    }
    
    SourceFile* file = &reporter->sources[reporter->source_count++];
    file->filename = MEM_STRDUP(alloc, filename);
    file->source = MEM_STRDUP(alloc, source);
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
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_VM);
    
    // Free error messages and suggestions
    for (size_t i = 0; i < reporter->errors.count; i++) {
        if (reporter->errors.errors[i].message) {
            size_t msg_len = strlen(reporter->errors.errors[i].message) + 1;
            MEM_FREE(alloc, (void*)reporter->errors.errors[i].message, msg_len);
        }
        if (reporter->errors.errors[i].suggestion) {
            size_t sug_len = strlen(reporter->errors.errors[i].suggestion) + 1;
            MEM_FREE(alloc, (void*)reporter->errors.errors[i].suggestion, sug_len);
        }
    }
    reporter->errors.count = 0;
    
    // Free warning messages and suggestions
    for (size_t i = 0; i < reporter->warnings.count; i++) {
        if (reporter->warnings.errors[i].message) {
            size_t msg_len = strlen(reporter->warnings.errors[i].message) + 1;
            MEM_FREE(alloc, (void*)reporter->warnings.errors[i].message, msg_len);
        }
        if (reporter->warnings.errors[i].suggestion) {
            size_t sug_len = strlen(reporter->warnings.errors[i].suggestion) + 1;
            MEM_FREE(alloc, (void*)reporter->warnings.errors[i].suggestion, sug_len);
        }
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