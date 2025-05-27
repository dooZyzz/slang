#include "utils/syntax_test.h"
#include "parser/parser.h"
#include "codegen/compiler.h"
#include "vm/vm.h"
#include "utils/error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <regex.h>

// ANSI color codes
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_DIM     "\033[2m"

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) return NULL;
    
    fseek(file, 0L, SEEK_END);
    size_t file_size = ftell(file);
    rewind(file);
    
    char* buffer = malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }
    
    size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);
    
    return buffer;
}

static TestDirective parse_directive(const char* line) {
    TestDirective directive = {.type = TEST_DIRECTIVE_NONE, .pattern = NULL, .line = 0};
    
    // Skip leading whitespace
    while (*line && (*line == ' ' || *line == '\t')) line++;
    
    // Check for comment start
    if (strncmp(line, "//", 2) != 0) return directive;
    line += 2;
    
    // Skip whitespace after //
    while (*line && (*line == ' ' || *line == '\t')) line++;
    
    // Check for @ directive
    if (*line != '@') return directive;
    line++;
    
    // Parse directive type
    if (strncmp(line, "compile-fail:", 13) == 0) {
        directive.type = TEST_DIRECTIVE_COMPILE_FAIL;
        line += 13;
    } else if (strncmp(line, "runtime-error:", 14) == 0) {
        directive.type = TEST_DIRECTIVE_RUNTIME_ERROR;
        line += 14;
    } else if (strncmp(line, "output:", 7) == 0) {
        directive.type = TEST_DIRECTIVE_OUTPUT;
        line += 7;
    } else if (strncmp(line, "output-regex:", 13) == 0) {
        directive.type = TEST_DIRECTIVE_OUTPUT_REGEX;
        line += 13;
    } else if (strncmp(line, "parse-only", 10) == 0) {
        directive.type = TEST_DIRECTIVE_PARSE_ONLY;
        return directive;
    } else if (strncmp(line, "no-run", 6) == 0) {
        directive.type = TEST_DIRECTIVE_NO_RUN;
        return directive;
    } else if (strncmp(line, "ignore", 6) == 0) {
        directive.type = TEST_DIRECTIVE_IGNORE;
        return directive;
    } else {
        return directive;
    }
    
    // Skip whitespace before pattern
    while (*line && (*line == ' ' || *line == '\t')) line++;
    
    // Extract pattern (rest of line)
    if (*line) {
        // Remove trailing newline if present
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            len--;
        }
        
        directive.pattern = malloc(len + 1);
        memcpy(directive.pattern, line, len);
        directive.pattern[len] = '\0';
    }
    
    return directive;
}

SyntaxTest* syntax_test_parse(const char* filepath) {
    char* content = read_file(filepath);
    if (!content) return NULL;
    
    SyntaxTest* test = calloc(1, sizeof(SyntaxTest));
    test->filename = strdup(filepath);
    test->directive_capacity = 8;
    test->directives = calloc(test->directive_capacity, sizeof(TestDirective));
    
    // Make a copy for directive parsing (strtok modifies the string)
    char* parse_copy = strdup(content);
    test->content = content;  // Keep original for compilation
    
    // Parse directives from content
    char* line = strtok(parse_copy, "\n");
    int line_num = 1;
    
    while (line) {
        TestDirective directive = parse_directive(line);
        if (directive.type != TEST_DIRECTIVE_NONE) {
            directive.line = line_num;
            
            // Grow array if needed
            if (test->directive_count >= test->directive_capacity) {
                test->directive_capacity *= 2;
                test->directives = realloc(test->directives, 
                    test->directive_capacity * sizeof(TestDirective));
            }
            
            test->directives[test->directive_count++] = directive;
        }
        
        line = strtok(NULL, "\n");
        line_num++;
    }
    
    free(parse_copy);  // Free the copy used for parsing
    return test;
}

void syntax_test_destroy(SyntaxTest* test) {
    if (!test) return;
    
    free(test->filename);
    free(test->content);
    
    for (size_t i = 0; i < test->directive_count; i++) {
        free(test->directives[i].pattern);
    }
    free(test->directives);
    
    free(test);
}

// Capture output during test execution
typedef struct {
    char* buffer;
    size_t size;
    size_t capacity;
} OutputCapture;

static OutputCapture* g_capture_output = NULL;

// Hook for VM print operations
void vm_captured_print(const char* str) {
    if (!g_capture_output) {
        printf("%s", str);
        return;
    }
    
    size_t len = strlen(str);
    
    // Grow buffer if needed
    while (g_capture_output->size + len + 1 > g_capture_output->capacity) {
        g_capture_output->capacity *= 2;
        g_capture_output->buffer = realloc(g_capture_output->buffer, g_capture_output->capacity);
    }
    
    // Copy to buffer
    memcpy(g_capture_output->buffer + g_capture_output->size, str, len);
    g_capture_output->size += len;
    g_capture_output->buffer[g_capture_output->size] = '\0';
}

static bool check_pattern_match(const char* text, const char* pattern, bool is_regex) {
    if (!is_regex) {
        // Simple substring match
        return strstr(text, pattern) != NULL;
    } else {
        // Regex match
        regex_t regex;
        int ret = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB);
        if (ret != 0) {
            fprintf(stderr, "Invalid regex pattern: %s\n", pattern);
            return false;
        }
        
        ret = regexec(&regex, text, 0, NULL, 0);
        regfree(&regex);
        
        return ret == 0;
    }
}

bool syntax_test_run(SyntaxTest* test, bool verbose) {
    if (!test) return false;
    
    // Check for @ignore directive
    for (size_t i = 0; i < test->directive_count; i++) {
        if (test->directives[i].type == TEST_DIRECTIVE_IGNORE) {
            if (verbose) {
                printf("%s%s[IGNORED]%s %s\n", COLOR_YELLOW, COLOR_BOLD, COLOR_RESET, test->filename);
            }
            return true;
        }
    }
    
    bool has_compile_fail = false;
    bool has_runtime_error = false;
    bool has_output = false;
    bool parse_only = false;
    bool no_run = false;
    
    // Collect test expectations
    for (size_t i = 0; i < test->directive_count; i++) {
        switch (test->directives[i].type) {
            case TEST_DIRECTIVE_COMPILE_FAIL:
                has_compile_fail = true;
                break;
            case TEST_DIRECTIVE_RUNTIME_ERROR:
                has_runtime_error = true;
                break;
            case TEST_DIRECTIVE_OUTPUT:
            case TEST_DIRECTIVE_OUTPUT_REGEX:
                has_output = true;
                break;
            case TEST_DIRECTIVE_PARSE_ONLY:
                parse_only = true;
                break;
            case TEST_DIRECTIVE_NO_RUN:
                no_run = true;
                break;
            default:
                break;
        }
    }
    
    // Create error reporter
    ErrorReporter* errors = error_reporter_create();
    error_set_source(errors, test->filename, test->content);
    
    // Parse
    Parser* parser = parser_create(test->content);
    // parser->error_reporter = errors; // TODO: integrate error reporting
    ProgramNode* program = parser_parse_program(parser);
    
    bool parse_failed = parser->had_error;
    
    // Check parse-only tests
    if (parse_only) {
        bool success = !parse_failed;
        parser_destroy(parser);
        if (program) program_destroy(program);
        error_reporter_destroy(errors);
        
        if (verbose) {
            printf("%s%s[%s]%s %s\n", 
                   success ? COLOR_GREEN : COLOR_RED,
                   COLOR_BOLD,
                   success ? "PASS" : "FAIL",
                   COLOR_RESET,
                   test->filename);
        }
        return success;
    }
    
    // Check compile-fail tests
    if (has_compile_fail) {
        if (!parse_failed) {
            // Expected failure but parsing succeeded
            parser_destroy(parser);
            program_destroy(program);
            error_reporter_destroy(errors);
            
            if (verbose) {
                printf("%s%s[FAIL]%s %s - Expected compile error but succeeded\n",
                       COLOR_RED, COLOR_BOLD, COLOR_RESET, test->filename);
            }
            return false;
        }
        
        // Check if error matches expected pattern
        bool pattern_matched = false;
        for (size_t i = 0; i < test->directive_count; i++) {
            if (test->directives[i].type == TEST_DIRECTIVE_COMPILE_FAIL) {
                // TODO: Check actual error messages when error reporting is integrated
                pattern_matched = true; // For now, any error counts
                break;
            }
        }
        
        parser_destroy(parser);
        error_reporter_destroy(errors);
        
        if (verbose) {
            printf("%s%s[%s]%s %s\n",
                   pattern_matched ? COLOR_GREEN : COLOR_RED,
                   COLOR_BOLD,
                   pattern_matched ? "PASS" : "FAIL",
                   COLOR_RESET,
                   test->filename);
        }
        return pattern_matched;
    }
    
    // If parsing failed but we didn't expect it
    if (parse_failed) {
        parser_destroy(parser);
        error_reporter_destroy(errors);
        
        if (verbose) {
            printf("%s%s[FAIL]%s %s - Unexpected parse error\n",
                   COLOR_RED, COLOR_BOLD, COLOR_RESET, test->filename);
        }
        return false;
    }
    
    // Compile
    Chunk chunk;
    chunk_init(&chunk);
    
    if (!compile(program, &chunk)) {
        parser_destroy(parser);
        program_destroy(program);
        chunk_free(&chunk);
        error_reporter_destroy(errors);
        
        if (verbose) {
            printf("%s%s[FAIL]%s %s - Compilation failed\n",
                   COLOR_RED, COLOR_BOLD, COLOR_RESET, test->filename);
        }
        return false;
    }
    
    // Clean up parser data
    parser_destroy(parser);
    program_destroy(program);
    error_reporter_destroy(errors);
    
    // Check no-run tests
    if (no_run) {
        chunk_free(&chunk);
        
        if (verbose) {
            printf("%s%s[PASS]%s %s\n",
                   COLOR_GREEN, COLOR_BOLD, COLOR_RESET, test->filename);
        }
        return true;
    }
    
    // Run the test
    VM vm;
    vm_init(&vm);
    
    // Capture output if needed
    OutputCapture output = {0};
    if (has_output) {
        output.buffer = malloc(1024);
        output.capacity = 1024;
        output.size = 0;
        g_capture_output = &output;
        
        // Set VM print hook
        extern void vm_captured_print(const char* str);
        vm_set_print_hook(vm_captured_print);
    }
    
    InterpretResult result = vm_interpret(&vm, &chunk);
    
    // Check runtime expectations
    bool success = true;
    
    if (has_runtime_error) {
        if (result != INTERPRET_RUNTIME_ERROR) {
            success = false;
            if (verbose) {
                printf("%s%s[FAIL]%s %s - Expected runtime error but succeeded\n",
                       COLOR_RED, COLOR_BOLD, COLOR_RESET, test->filename);
            }
        }
    } else if (result == INTERPRET_RUNTIME_ERROR) {
        success = false;
        if (verbose) {
            printf("%s%s[FAIL]%s %s - Unexpected runtime error\n",
                   COLOR_RED, COLOR_BOLD, COLOR_RESET, test->filename);
        }
    }
    
    // Check output expectations
    if (has_output && success) {
        for (size_t i = 0; i < test->directive_count; i++) {
            TestDirective* dir = &test->directives[i];
            if (dir->type == TEST_DIRECTIVE_OUTPUT || 
                dir->type == TEST_DIRECTIVE_OUTPUT_REGEX) {
                
                bool is_regex = (dir->type == TEST_DIRECTIVE_OUTPUT_REGEX);
                bool matched = check_pattern_match(output.buffer, dir->pattern, is_regex);
                
                if (!matched) {
                    success = false;
                    if (verbose) {
                        printf("%s%s[FAIL]%s %s - Output mismatch\n",
                               COLOR_RED, COLOR_BOLD, COLOR_RESET, test->filename);
                        printf("  Expected: '%s'\n", dir->pattern);
                        printf("  Got: '%s'\n", output.buffer);
                        printf("  Got length: %zu\n", strlen(output.buffer));
                    }
                }
            }
        }
    }
    
    // Cleanup
    vm_free(&vm);
    chunk_free(&chunk);
    if (output.buffer) free(output.buffer);
    g_capture_output = NULL;
    vm_set_print_hook(NULL);  // Reset print hook
    
    if (verbose && success) {
        printf("%s%s[PASS]%s %s\n",
               COLOR_GREEN, COLOR_BOLD, COLOR_RESET, test->filename);
    }
    
    return success;
}

SyntaxTestSuite* syntax_test_suite_create(const char* name) {
    (void)name;  // Unused parameter
    SyntaxTestSuite* suite = calloc(1, sizeof(SyntaxTestSuite));
    suite->test_capacity = 16;
    suite->tests = calloc(suite->test_capacity, sizeof(SyntaxTest));
    return suite;
}

void syntax_test_suite_destroy(SyntaxTestSuite* suite) {
    if (!suite) return;
    
    for (size_t i = 0; i < suite->test_count; i++) {
        syntax_test_destroy(&suite->tests[i]);
    }
    free(suite->tests);
    free(suite);
}

void syntax_test_suite_add_file(SyntaxTestSuite* suite, const char* filepath) {
    SyntaxTest* test = syntax_test_parse(filepath);
    if (!test) return;
    
    // Grow array if needed
    if (suite->test_count >= suite->test_capacity) {
        suite->test_capacity *= 2;
        suite->tests = realloc(suite->tests, suite->test_capacity * sizeof(SyntaxTest));
    }
    
    suite->tests[suite->test_count++] = *test;
    free(test); // Only free the wrapper, not the contents
}

void syntax_test_suite_add_directory(SyntaxTestSuite* suite, const char* dirpath) {
    DIR* dir = opendir(dirpath);
    if (!dir) return;
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip hidden files and directories
        if (entry->d_name[0] == '.') continue;
        
        // Build full path
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
        
        struct stat st;
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                // Recurse into subdirectory
                syntax_test_suite_add_directory(suite, fullpath);
            } else if (S_ISREG(st.st_mode)) {
                // Check if it's a Swift file
                size_t len = strlen(entry->d_name);
                if (len > 6 && strcmp(entry->d_name + len - 6, ".swift") == 0) {
                    syntax_test_suite_add_file(suite, fullpath);
                }
            }
        }
    }
    
    closedir(dir);
}

bool syntax_test_suite_run(SyntaxTestSuite* suite, bool verbose) {
    if (!suite) return false;
    
    suite->passed = 0;
    suite->failed = 0;
    suite->ignored = 0;
    
    for (size_t i = 0; i < suite->test_count; i++) {
        bool has_ignore = false;
        for (size_t j = 0; j < suite->tests[i].directive_count; j++) {
            if (suite->tests[i].directives[j].type == TEST_DIRECTIVE_IGNORE) {
                has_ignore = true;
                break;
            }
        }
        
        if (has_ignore) {
            suite->ignored++;
            continue;
        }
        
        if (syntax_test_run(&suite->tests[i], verbose)) {
            suite->passed++;
        } else {
            suite->failed++;
        }
    }
    
    return suite->failed == 0;
}

void syntax_test_print_results(SyntaxTestSuite* suite) {
    if (!suite) return;
    
    printf("\n");
    printf("%s╔═══════════════════════════════════════════════════════════════╗%s\n",
           COLOR_BLUE, COLOR_RESET);
    printf("%s║               SYNTAX TEST RESULTS                             ║%s\n",
           COLOR_BLUE, COLOR_RESET);
    printf("%s╚═══════════════════════════════════════════════════════════════╝%s\n",
           COLOR_BLUE, COLOR_RESET);
    printf("\n");
    
    printf("Total:   %zu\n", suite->test_count);
    printf("%sPassed:  %zu%s\n", COLOR_GREEN, suite->passed, COLOR_RESET);
    printf("%sFailed:  %zu%s\n", COLOR_RED, suite->failed, COLOR_RESET);
    printf("%sIgnored: %zu%s\n", COLOR_YELLOW, suite->ignored, COLOR_RESET);
    
    printf("\n");
    if (suite->failed == 0) {
        printf("%s%s✓ All syntax tests passed!%s\n", COLOR_GREEN, COLOR_BOLD, COLOR_RESET);
    } else {
        printf("%s%s✗ %zu syntax tests failed%s\n", COLOR_RED, COLOR_BOLD, suite->failed, COLOR_RESET);
    }
}