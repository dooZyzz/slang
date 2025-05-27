#ifndef DEBUG_H
#define DEBUG_H

#include "vm/vm.h"
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
} DebugFlags;

extern DebugFlags debug_flags;

void debug_init(void);
void debug_set_flags(bool tokens, bool ast, bool bytecode, bool trace);

#endif
