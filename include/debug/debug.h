#ifndef DEBUG_H
#define DEBUG_H

#include <stdio.h>
#include <stdbool.h>
#include "runtime/core/vm.h"
#include "ast/ast.h"

// Disassembler for bytecode
void disassemble_chunk(Chunk* chunk, const char* name);
int disassemble_instruction(Chunk* chunk, int offset);

// Debug flags
typedef struct {
    bool print_tokens;
    bool print_ast;
    bool print_bytecode;
    bool trace_execution;
    bool module_loading;
    bool module_cache;
    bool module_hooks;
} DebugFlags;

extern DebugFlags debug_flags;

// Debug logging macros
#define DEBUG_LOG(category, ...) do { \
    if (debug_flags.category) { \
        fprintf(stderr, "[DEBUG] "); \
        fprintf(stderr, __VA_ARGS__); \
    } \
} while(0)

#define MODULE_DEBUG(...) DEBUG_LOG(module_loading, __VA_ARGS__)
#define CACHE_DEBUG(...) DEBUG_LOG(module_cache, __VA_ARGS__)
#define HOOKS_DEBUG(...) DEBUG_LOG(module_hooks, __VA_ARGS__)

void debug_init(void);
void debug_set_flags(bool tokens, bool ast, bool bytecode, bool trace);

#endif
