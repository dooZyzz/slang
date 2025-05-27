#include "semantic/symbol_table.h"
#include <stdlib.h>
#include <string.h>

typedef struct SymbolEntry {
    Symbol* symbol;
    struct SymbolEntry* next;
} SymbolEntry;

struct Scope {
    SymbolEntry** buckets;
    size_t bucket_count;
    size_t symbol_count;
    Scope* parent;
    size_t depth;
};

struct SymbolTable {
    Scope* current;
    Scope* global;
};

static size_t hash_string(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static Scope* scope_create(Scope* parent) {
    Scope* scope = calloc(1, sizeof(Scope));
    scope->bucket_count = 32;
    scope->buckets = calloc(scope->bucket_count, sizeof(SymbolEntry*));
    scope->parent = parent;
    scope->depth = parent ? parent->depth + 1 : 0;
    return scope;
}

static void scope_destroy(Scope* scope) {
    if (!scope) return;
    
    for (size_t i = 0; i < scope->bucket_count; i++) {
        SymbolEntry* entry = scope->buckets[i];
        while (entry) {
            SymbolEntry* next = entry->next;
            free((void*)entry->symbol->name);
            free(entry->symbol);
            free(entry);
            entry = next;
        }
    }
    
    free(scope->buckets);
    free(scope);
}

static Symbol* scope_lookup(Scope* scope, const char* name) {
    if (!scope || !name) return NULL;
    
    size_t index = hash_string(name) % scope->bucket_count;
    SymbolEntry* entry = scope->buckets[index];
    
    while (entry) {
        if (strcmp(entry->symbol->name, name) == 0) {
            return entry->symbol;
        }
        entry = entry->next;
    }
    
    return NULL;
}

static void scope_insert(Scope* scope, Symbol* symbol) {
    if (!scope || !symbol) return;
    
    size_t index = hash_string(symbol->name) % scope->bucket_count;
    
    SymbolEntry* entry = calloc(1, sizeof(SymbolEntry));
    entry->symbol = symbol;
    entry->next = scope->buckets[index];
    scope->buckets[index] = entry;
    scope->symbol_count++;
}

SymbolTable* symbol_table_create(void) {
    SymbolTable* table = calloc(1, sizeof(SymbolTable));
    table->global = scope_create(NULL);
    table->current = table->global;
    return table;
}

void symbol_table_destroy(SymbolTable* table) {
    if (!table) return;
    
    while (table->current != table->global) {
        symbol_table_exit_scope(table);
    }
    
    scope_destroy(table->global);
    free(table);
}

void symbol_table_enter_scope(SymbolTable* table) {
    if (!table) return;
    table->current = scope_create(table->current);
}

void symbol_table_exit_scope(SymbolTable* table) {
    if (!table || table->current == table->global) return;
    
    Scope* old = table->current;
    table->current = old->parent;
    scope_destroy(old);
}

Scope* symbol_table_current_scope(SymbolTable* table) {
    return table ? table->current : NULL;
}

Scope* symbol_table_global_scope(SymbolTable* table) {
    return table ? table->global : NULL;
}

Symbol* symbol_declare(SymbolTable* table, const char* name, SymbolKind kind,
                      Type* type, const Token* token) {
    if (!table || !name) return NULL;
    
    if (scope_lookup(table->current, name)) {
        return NULL;
    }
    
    Symbol* symbol = calloc(1, sizeof(Symbol));
    symbol->name = strdup(name);
    symbol->kind = kind;
    symbol->type = type;
    if (token) {
        symbol->declaration_token = *token;
    }
    
    symbol->is_global = (table->current == table->global);
    
    if (kind == SYMBOL_VARIABLE || kind == SYMBOL_PARAMETER) {
        symbol->data.variable.scope_depth = table->current->depth;
        symbol->data.variable.local_index = table->current->symbol_count;
    }
    
    scope_insert(table->current, symbol);
    return symbol;
}

Symbol* symbol_lookup(SymbolTable* table, const char* name) {
    if (!table || !name) return NULL;
    
    Scope* scope = table->current;
    while (scope) {
        Symbol* symbol = scope_lookup(scope, name);
        if (symbol) {
            if (scope != table->current && scope != table->global &&
                (symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_PARAMETER)) {
                symbol->is_captured = true;
            }
            return symbol;
        }
        scope = scope->parent;
    }
    
    return NULL;
}

Symbol* symbol_lookup_local(SymbolTable* table, const char* name) {
    if (!table || !name) return NULL;
    return scope_lookup(table->current, name);
}

bool symbol_is_declared_in_scope(SymbolTable* table, const char* name) {
    return symbol_lookup_local(table, name) != NULL;
}

void symbol_mark_initialized(Symbol* symbol) {
    if (symbol) symbol->is_initialized = true;
}

void symbol_mark_used(Symbol* symbol) {
    if (symbol) symbol->is_used = true;
}

void symbol_mark_captured(Symbol* symbol) {
    if (symbol) symbol->is_captured = true;
}

size_t symbol_table_depth(SymbolTable* table) {
    return table ? table->current->depth : 0;
}

size_t symbol_count_in_scope(SymbolTable* table) {
    return table ? table->current->symbol_count : 0;
}

static void scope_foreach(Scope* scope, SymbolVisitor visitor, void* context) {
    if (!scope || !visitor) return;
    
    for (size_t i = 0; i < scope->bucket_count; i++) {
        SymbolEntry* entry = scope->buckets[i];
        while (entry) {
            visitor(entry->symbol, context);
            entry = entry->next;
        }
    }
}

void symbol_table_foreach(SymbolTable* table, SymbolVisitor visitor, void* context) {
    if (!table || !visitor) return;
    
    Scope* scope = table->current;
    while (scope) {
        scope_foreach(scope, visitor, context);
        scope = scope->parent;
    }
}

void symbol_table_foreach_in_scope(SymbolTable* table, SymbolVisitor visitor, void* context) {
    if (!table || !visitor) return;
    scope_foreach(table->current, visitor, context);
}

static void collect_uninitialized(Symbol* symbol, void* context) {
    SymbolList* list = (SymbolList*)context;
    if ((symbol->kind == SYMBOL_VARIABLE || symbol->kind == SYMBOL_PARAMETER) &&
        !symbol->is_initialized && symbol->is_mutable) {
        if (list->count >= list->capacity) {
            list->capacity *= 2;
            list->symbols = realloc(list->symbols, list->capacity * sizeof(Symbol*));
        }
        list->symbols[list->count++] = symbol;
    }
}

static void collect_unused(Symbol* symbol, void* context) {
    SymbolList* list = (SymbolList*)context;
    if (!symbol->is_used) {
        if (list->count >= list->capacity) {
            list->capacity *= 2;
            list->symbols = realloc(list->symbols, list->capacity * sizeof(Symbol*));
        }
        list->symbols[list->count++] = symbol;
    }
}

SymbolList* symbol_table_get_uninitialized(SymbolTable* table) {
    if (!table) return NULL;
    
    SymbolList* list = calloc(1, sizeof(SymbolList));
    list->capacity = 16;
    list->symbols = calloc(list->capacity, sizeof(Symbol*));
    
    symbol_table_foreach(table, collect_uninitialized, list);
    return list;
}

SymbolList* symbol_table_get_unused(SymbolTable* table) {
    if (!table) return NULL;
    
    SymbolList* list = calloc(1, sizeof(SymbolList));
    list->capacity = 16;
    list->symbols = calloc(list->capacity, sizeof(Symbol*));
    
    symbol_table_foreach(table, collect_unused, list);
    return list;
}

void symbol_list_free(SymbolList* list) {
    if (!list) return;
    free(list->symbols);
    free(list);
}