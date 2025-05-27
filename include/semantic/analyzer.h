#ifndef LANG_ANALYZER_H
#define LANG_ANALYZER_H

#include "semantic/visitor.h"
#include "semantic/symbol_table.h"
#include "semantic/type.h"
#include "utils/error.h"
#include "ast/ast.h"

typedef struct ModuleLoader ModuleLoader;

typedef struct {
    SymbolTable* symbols;
    TypeContext* types;
    ErrorReporter* errors;
    ModuleLoader* module_loader;
    bool in_function;
    bool in_loop;
    bool in_class;
    Type* current_function_return_type;
    Type* current_class_type;
} SemanticContext;

typedef struct SemanticAnalyzer SemanticAnalyzer;

SemanticAnalyzer* semantic_analyzer_create(ErrorReporter* errors);
void semantic_analyzer_destroy(SemanticAnalyzer* analyzer);

bool semantic_analyze(SemanticAnalyzer* analyzer, ProgramNode* program);

SymbolTable* semantic_analyzer_get_symbols(SemanticAnalyzer* analyzer);
TypeContext* semantic_analyzer_get_types(SemanticAnalyzer* analyzer);

void semantic_analyzer_check_uninitialized_variables(SemanticAnalyzer* analyzer);
void semantic_analyzer_check_unused_variables(SemanticAnalyzer* analyzer);

#endif