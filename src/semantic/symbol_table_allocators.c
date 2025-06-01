#include "semantic/symbol_table.h"
#include "utils/allocators.h"
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

// Symbol table uses arena allocator - all symbols are freed when analysis is done

static size_t hash_string(const char* str) {
    size_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static Scope* scope_create(Scope* parent) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_SYMBOLS);
    Scope* scope = MEM_NEW(alloc, Scope);
    if (!scope) return NULL;
    
    scope->bucket_count = 32;
    scope->buckets = MEM_NEW_ARRAY(alloc, SymbolEntry*, scope->bucket_count);
    if (!scope->buckets) return NULL;
    
    scope->parent = parent;
    scope->depth = parent ? parent->depth + 1 : 0;
    scope->symbol_count = 0;
    
    return scope;
}

static void scope_destroy(Scope* scope) {
    // With arena allocator, individual scopes don't need to be freed
    // The arena will be reset after semantic analysis
    (void)scope;
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
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_SYMBOLS);
    size_t index = hash_string(symbol->name) % scope->bucket_count;
    
    SymbolEntry* entry = MEM_NEW(alloc, SymbolEntry);
    if (!entry) return;
    
    entry->symbol = symbol;
    entry->next = scope->buckets[index];
    scope->buckets[index] = entry;
    scope->symbol_count++;
}

SymbolTable* symbol_table_create(void) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_SYMBOLS);
    SymbolTable* table = MEM_NEW(alloc, SymbolTable);
    if (!table) return NULL;
    
    table->global = scope_create(NULL);
    table->current = table->global;
    
    return table;
}

void symbol_table_destroy(SymbolTable* table) {
    // With arena allocator, the entire symbol table is freed at once
    // when the arena is reset
    (void)table;
}

void symbol_table_enter_scope(SymbolTable* table) {
    if (!table) return;
    
    Scope* new_scope = scope_create(table->current);
    if (new_scope) {
        table->current = new_scope;
    }
}

void symbol_table_exit_scope(SymbolTable* table) {
    if (!table || !table->current || table->current == table->global) return;
    
    table->current = table->current->parent;
}

bool symbol_table_insert(SymbolTable* table, const char* name, SymbolType type, Type* data_type, bool is_mutable) {
    if (!table || !name) return false;
    
    // Check for redeclaration in current scope
    if (scope_lookup(table->current, name)) {
        return false;
    }
    
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_SYMBOLS);
    Symbol* symbol = MEM_NEW(alloc, Symbol);
    if (!symbol) return false;
    
    symbol->name = MEM_STRDUP(alloc, name);
    if (!symbol->name) return false;
    
    symbol->type = type;
    symbol->data_type = data_type;
    symbol->is_mutable = is_mutable;
    symbol->is_initialized = (type == SYMBOL_FUNCTION);
    symbol->scope_depth = table->current->depth;
    
    scope_insert(table->current, symbol);
    return true;
}

Symbol* symbol_table_lookup(SymbolTable* table, const char* name) {
    if (!table || !name) return NULL;
    
    Scope* scope = table->current;
    while (scope) {
        Symbol* symbol = scope_lookup(scope, name);
        if (symbol) {
            return symbol;
        }
        scope = scope->parent;
    }
    
    return NULL;
}

Symbol* symbol_table_lookup_local(SymbolTable* table, const char* name) {
    if (!table || !name) return NULL;
    return scope_lookup(table->current, name);
}

void symbol_table_mark_initialized(SymbolTable* table, const char* name) {
    Symbol* symbol = symbol_table_lookup_local(table, name);
    if (symbol) {
        symbol->is_initialized = true;
    }
}

size_t symbol_table_current_depth(SymbolTable* table) {
    return table && table->current ? table->current->depth : 0;
}

bool symbol_table_is_global_scope(SymbolTable* table) {
    return table && table->current == table->global;
}

// Symbol iteration
typedef struct {
    SymbolIteratorCallback callback;
    void* user_data;
} IteratorContext;

static void iterate_scope(Scope* scope, IteratorContext* ctx) {
    if (!scope) return;
    
    for (size_t i = 0; i < scope->bucket_count; i++) {
        SymbolEntry* entry = scope->buckets[i];
        while (entry) {
            ctx->callback(entry->symbol, ctx->user_data);
            entry = entry->next;
        }
    }
}

void symbol_table_iterate(SymbolTable* table, SymbolIteratorCallback callback, void* user_data) {
    if (!table || !callback) return;
    
    IteratorContext ctx = { callback, user_data };
    
    // Iterate from global to current scope
    Scope* scopes[256]; // Max nesting depth
    size_t scope_count = 0;
    
    Scope* scope = table->current;
    while (scope && scope_count < 256) {
        scopes[scope_count++] = scope;
        scope = scope->parent;
    }
    
    // Iterate in reverse order (global to current)
    for (size_t i = scope_count; i > 0; i--) {
        iterate_scope(scopes[i - 1], &ctx);
    }
}

void symbol_table_iterate_current_scope(SymbolTable* table, SymbolIteratorCallback callback, void* user_data) {
    if (!table || !callback) return;
    
    IteratorContext ctx = { callback, user_data };
    iterate_scope(table->current, &ctx);
}