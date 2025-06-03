#ifndef COMPILER_H
#define COMPILER_H

#include "ast/ast.h"
#include "runtime/core/vm.h"

typedef struct Loop {
    struct Loop* enclosing;
    int start;  // Start of loop for continue
    int scope_depth;  // Scope depth when loop started
    int* break_jumps;  // Array of break jump offsets to patch
    size_t break_count;
    size_t break_capacity;
} Loop;

typedef enum {
    FUNC_TYPE_FUNCTION,
    FUNC_TYPE_SCRIPT
} CompilerFunctionType;

typedef struct {
    uint8_t index;
    bool is_local;
} CompilerUpvalue;

typedef struct Compiler {
    struct Compiler* enclosing;
    CompilerFunctionType type;
    Function* function;
    Chunk* current_chunk;
    
    struct {
        char** names;
        int* depths;
        size_t count;
        size_t capacity;
    } locals;
    
    struct {
        CompilerUpvalue* values;
        size_t count;
        size_t capacity;
    } upvalues;
    
    int scope_depth;
    Loop* inner_most_loop;
    bool is_last_expr_stmt;
    ProgramNode* program;
    size_t current_stmt_index;
    
    // Module context
    struct Module* current_module;
    bool is_module_compilation;
} Compiler;

bool compile(ProgramNode* program, Chunk* chunk);
bool compile_module(ProgramNode* program, Chunk* chunk, struct Module* module);

#endif
