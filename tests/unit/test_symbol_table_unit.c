#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "semantic/symbol_table.h"

DEFINE_TEST(create_destroy) {
    SymbolTable* table = symbol_table_create();
    TEST_ASSERT_NOT_NULL(suite, table, "create_destroy");
    
    symbol_table_destroy(table);
}

DEFINE_TEST(define_lookup) {
    SymbolTable* table = symbol_table_create();
    TEST_ASSERT_NOT_NULL(suite, table, "define_lookup");
    
    // Define a symbol
    Token token = {.type = TOKEN_IDENTIFIER, .lexeme = "myVar", .line = 1};
    Symbol* sym = symbol_declare(table, "myVar", SYMBOL_VARIABLE, NULL, &token);
    TEST_ASSERT_NOT_NULL(suite, sym, "define_lookup");
    TEST_ASSERT_EQUAL_STR(suite, "myVar", sym->name, "define_lookup");
    TEST_ASSERT_EQUAL_INT(suite, SYMBOL_VARIABLE, sym->kind, "define_lookup");
    
    // Look it up
    Symbol* found = symbol_lookup(table, "myVar");
    TEST_ASSERT_NOT_NULL(suite, found, "define_lookup");
    TEST_ASSERT(suite, sym == found, "define_lookup");
    
    // Non-existent symbol
    Symbol* not_found = symbol_lookup(table, "notDefined");
    TEST_ASSERT_NULL(suite, not_found, "define_lookup");
    
    symbol_table_destroy(table);
}

DEFINE_TEST(multiple_symbols) {
    SymbolTable* table = symbol_table_create();
    
    Token tok_a = {.type = TOKEN_IDENTIFIER, .lexeme = "a", .line = 1};
    Token tok_b = {.type = TOKEN_IDENTIFIER, .lexeme = "b", .line = 2};
    Token tok_c = {.type = TOKEN_IDENTIFIER, .lexeme = "c", .line = 3};
    
    Symbol* a = symbol_declare(table, "a", SYMBOL_VARIABLE, NULL, &tok_a);
    Symbol* b = symbol_declare(table, "b", SYMBOL_VARIABLE, NULL, &tok_b);
    Symbol* c = symbol_declare(table, "c", SYMBOL_VARIABLE, NULL, &tok_c);
    
    TEST_ASSERT_NOT_NULL(suite, a, "multiple_symbols");
    TEST_ASSERT_NOT_NULL(suite, b, "multiple_symbols");
    TEST_ASSERT_NOT_NULL(suite, c, "multiple_symbols");
    
    TEST_ASSERT_EQUAL_STR(suite, "a", a->name, "multiple_symbols");
    TEST_ASSERT_EQUAL_STR(suite, "b", b->name, "multiple_symbols");
    TEST_ASSERT_EQUAL_STR(suite, "c", c->name, "multiple_symbols");
    
    symbol_table_destroy(table);
}

DEFINE_TEST(scoping) {
    SymbolTable* global = symbol_table_create();
    
    // Define in global scope
    Token tok_x = {.type = TOKEN_IDENTIFIER, .lexeme = "x", .line = 1};
    Symbol* x = symbol_declare(global, "x", SYMBOL_VARIABLE, NULL, &tok_x);
    TEST_ASSERT_NOT_NULL(suite, x, "scoping");
    TEST_ASSERT_EQUAL_INT(suite, 0, symbol_table_depth(global), "scoping");
    
    // Enter new scope
    symbol_table_enter_scope(global);
    TEST_ASSERT_EQUAL_INT(suite, 1, symbol_table_depth(global), "scoping");
    
    // Define in local scope
    Token tok_y = {.type = TOKEN_IDENTIFIER, .lexeme = "y", .line = 2};
    Symbol* y = symbol_declare(global, "y", SYMBOL_VARIABLE, NULL, &tok_y);
    TEST_ASSERT_NOT_NULL(suite, y, "scoping");
    
    // Shadow global variable
    Token tok_local_x = {.type = TOKEN_IDENTIFIER, .lexeme = "x", .line = 3};
    Symbol* local_x = symbol_declare(global, "x", SYMBOL_VARIABLE, NULL, &tok_local_x);
    TEST_ASSERT_NOT_NULL(suite, local_x, "scoping");
    TEST_ASSERT(suite, local_x != x, "scoping");
    
    // Lookup finds local version
    Symbol* found_x = symbol_lookup(global, "x");
    TEST_ASSERT(suite, found_x == local_x, "scoping");
    
    // Exit scope
    symbol_table_exit_scope(global);
    
    // Now lookup finds global version
    found_x = symbol_lookup(global, "x");
    TEST_ASSERT(suite, found_x == x, "scoping");
    
    // Local variable no longer accessible
    Symbol* found_y = symbol_lookup(global, "y");
    TEST_ASSERT_NULL(suite, found_y, "scoping");
    
    symbol_table_destroy(global);
}

DEFINE_TEST(nested_scopes) {
    SymbolTable* table = symbol_table_create();
    
    // Global scope - depth 0
    Token tok_global = {.type = TOKEN_IDENTIFIER, .lexeme = "global", .line = 1};
    symbol_declare(table, "global", SYMBOL_VARIABLE, NULL, &tok_global);
    
    // Scope 1 - depth 1
    symbol_table_enter_scope(table);
    Token tok_scope1 = {.type = TOKEN_IDENTIFIER, .lexeme = "scope1", .line = 2};
    symbol_declare(table, "scope1", SYMBOL_VARIABLE, NULL, &tok_scope1);
    
    // Scope 2 - depth 2
    symbol_table_enter_scope(table);
    Token tok_scope2 = {.type = TOKEN_IDENTIFIER, .lexeme = "scope2", .line = 3};
    Symbol* scope2_var = symbol_declare(table, "scope2", SYMBOL_VARIABLE, NULL, &tok_scope2);
    TEST_ASSERT_NOT_NULL(suite, scope2_var, "nested_scopes");
    TEST_ASSERT_EQUAL_INT(suite, 2, symbol_table_depth(table), "nested_scopes");
    
    // Can see all parent scopes
    TEST_ASSERT_NOT_NULL(suite, symbol_lookup(table, "global"), "nested_scopes");
    TEST_ASSERT_NOT_NULL(suite, symbol_lookup(table, "scope1"), "nested_scopes");
    TEST_ASSERT_NOT_NULL(suite, symbol_lookup(table, "scope2"), "nested_scopes");
    
    // Exit scope 2
    symbol_table_exit_scope(table);
    TEST_ASSERT_NULL(suite, symbol_lookup(table, "scope2"), "nested_scopes");
    TEST_ASSERT_NOT_NULL(suite, symbol_lookup(table, "scope1"), "nested_scopes");
    
    // Exit scope 1
    symbol_table_exit_scope(table);
    TEST_ASSERT_NULL(suite, symbol_lookup(table, "scope1"), "nested_scopes");
    TEST_ASSERT_NOT_NULL(suite, symbol_lookup(table, "global"), "nested_scopes");
    
    symbol_table_destroy(table);
}

DEFINE_TEST(duplicate_definition) {
    SymbolTable* table = symbol_table_create();
    
    Token tok_first = {.type = TOKEN_IDENTIFIER, .lexeme = "x", .line = 1};
    Symbol* first = symbol_declare(table, "x", SYMBOL_VARIABLE, NULL, &tok_first);
    TEST_ASSERT_NOT_NULL(suite, first, "duplicate_definition");
    
    // Same scope - should fail (return NULL if already declared)
    bool already_declared = symbol_is_declared_in_scope(table, "x");
    TEST_ASSERT(suite, already_declared, "duplicate_definition");
    
    // Different scope - should succeed
    symbol_table_enter_scope(table);
    Token tok_shadow = {.type = TOKEN_IDENTIFIER, .lexeme = "x", .line = 3};
    Symbol* shadow = symbol_declare(table, "x", SYMBOL_VARIABLE, NULL, &tok_shadow);
    TEST_ASSERT_NOT_NULL(suite, shadow, "duplicate_definition");
    TEST_ASSERT(suite, shadow != first, "duplicate_definition");
    
    symbol_table_exit_scope(table);
    symbol_table_destroy(table);
}

// Define test suite
TEST_SUITE(symbol_table_unit)
    TEST_CASE(create_destroy, "Create and Destroy")
    TEST_CASE(define_lookup, "Define and Lookup")
    TEST_CASE(multiple_symbols, "Multiple Symbols")
    TEST_CASE(scoping, "Scoping")
    TEST_CASE(nested_scopes, "Nested Scopes")
    TEST_CASE(duplicate_definition, "Duplicate Definition")
END_TEST_SUITE(symbol_table_unit)

// Optional standalone runner
