#ifndef LANG_SYMBOL_TABLE_H
#define LANG_SYMBOL_TABLE_H

#include "semantic/type.h"
#include "lexer/token.h"
#include <stdbool.h>

typedef enum {
    SYMBOL_VARIABLE,
    SYMBOL_FUNCTION,
    SYMBOL_TYPE,
    SYMBOL_CLASS,
    SYMBOL_STRUCT,
    SYMBOL_ENUM,
    SYMBOL_PROTOCOL,
    SYMBOL_PARAMETER,
    SYMBOL_METHOD,
    SYMBOL_PROPERTY
} SymbolKind;

typedef struct Symbol {
    const char* name;
    SymbolKind kind;
    Type* type;
    Token declaration_token;
    
    bool is_initialized;
    bool is_used;
    bool is_mutable;
    bool is_global;
    bool is_captured;
    
    union {
        struct {
            size_t local_index;
            size_t scope_depth;
        } variable;
        
        struct {
            bool is_async;
            bool is_throwing;
            bool is_mutating;
            bool is_override;
            size_t arity;
        } function;
        
        struct {
            Type* parent_type;
            bool is_static;
            bool is_private;
        } member;
    } data;
} Symbol;

typedef struct Scope Scope;
typedef struct SymbolTable SymbolTable;

SymbolTable* symbol_table_create(void);
void symbol_table_destroy(SymbolTable* table);

void symbol_table_enter_scope(SymbolTable* table);
void symbol_table_exit_scope(SymbolTable* table);

Scope* symbol_table_current_scope(SymbolTable* table);
Scope* symbol_table_global_scope(SymbolTable* table);

Symbol* symbol_declare(SymbolTable* table, const char* name, SymbolKind kind,
                      Type* type, const Token* token);
Symbol* symbol_lookup(SymbolTable* table, const char* name);
Symbol* symbol_lookup_local(SymbolTable* table, const char* name);

bool symbol_is_declared_in_scope(SymbolTable* table, const char* name);

void symbol_mark_initialized(Symbol* symbol);
void symbol_mark_used(Symbol* symbol);
void symbol_mark_captured(Symbol* symbol);

size_t symbol_table_depth(SymbolTable* table);
size_t symbol_count_in_scope(SymbolTable* table);

typedef void (*SymbolVisitor)(Symbol* symbol, void* context);
void symbol_table_foreach(SymbolTable* table, SymbolVisitor visitor, void* context);
void symbol_table_foreach_in_scope(SymbolTable* table, SymbolVisitor visitor, void* context);

typedef struct {
    Symbol** symbols;
    size_t count;
    size_t capacity;
} SymbolList;

SymbolList* symbol_table_get_uninitialized(SymbolTable* table);
SymbolList* symbol_table_get_unused(SymbolTable* table);
void symbol_list_free(SymbolList* list);

#endif