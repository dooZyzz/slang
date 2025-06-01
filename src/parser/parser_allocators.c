#include "parser/parser.h"
#include "utils/allocators.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

// Parser uses temporary allocator for its own structures
// AST nodes are allocated with AST allocator

Parser* parser_create(const char* source)
{
    Parser* parser = MEM_NEW(allocators_get(ALLOC_SYSTEM_PARSER), Parser);
    if (!parser) return NULL;

    parser->lexer = lexer_create(source);
    parser->had_error = false;
    parser->panic_mode = false;

    // Initialize with first token
    parser->current = lexer_next_token(parser->lexer);

    return parser;
}

void parser_destroy(Parser* parser)
{
    if (parser)
    {
        lexer_destroy(parser->lexer);
        MEM_FREE(allocators_get(ALLOC_SYSTEM_PARSER), parser, sizeof(Parser));
    }
}

static void advance(Parser* parser)
{
    parser->previous = parser->current;

    for (;;)
    {
        parser->current = lexer_next_token(parser->lexer);
        if (parser->current.type != TOKEN_ERROR) break;

        parser_error_at_current(parser, parser->current.lexeme);
    }
}

static bool check(Parser* parser, TokenType type)
{
    return parser->current.type == type;
}

static bool match(Parser* parser, TokenType type)
{
    if (!check(parser, type)) return false;
    advance(parser);
    return true;
}

static void consume(Parser* parser, TokenType type, const char* message)
{
    if (parser->current.type == type)
    {
        advance(parser);
        return;
    }

    parser_error_at_current(parser, message);
}

void parser_error(Parser* parser, const char* message)
{
    parser_error_at(parser, &parser->previous, message);
}

void parser_error_at_current(Parser* parser, const char* message)
{
    parser_error_at(parser, &parser->current, message);
}

void parser_error_at(Parser* parser, Token* token, const char* message)
{
    if (parser->panic_mode) return;
    parser->panic_mode = true;
    parser->had_error = true;

    fprintf(stderr, "[line %zu] Error", token->line);

    if (token->type == TOKEN_EOF)
    {
        fprintf(stderr, " at end");
    }
    else if (token->type == TOKEN_ERROR)
    {
        // Nothing
    }
    else
    {
        fprintf(stderr, " at '%.*s'", (int)token->lexeme_length, token->lexeme);
    }

    fprintf(stderr, ": %s\n", message);
}

static void optional_semicolon(Parser* parser)
{
    // Allow optional semicolon - match if present but don't require
    match(parser, TOKEN_SEMICOLON);
}

// Helper for growing dynamic arrays during parsing
static void* grow_array(Parser* parser, void* array, size_t old_count, size_t new_count, size_t element_size)
{
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    return MEM_REALLOC(alloc, array, old_count * element_size, new_count * element_size);
}

// Forward declarations
static Expr* expression(Parser* parser);
static Stmt* statement(Parser* parser);
static Decl* declaration(Parser* parser);
static TypeExpr* type_expression(Parser* parser);

// Expression parsing with precedence climbing
typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // =
    PREC_NIL_COALESCE, // ??
    PREC_OR,          // ||
    PREC_AND,         // &&
    PREC_BIT_OR,      // |
    PREC_BIT_XOR,     // ^
    PREC_BIT_AND,     // &
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_SHIFT,       // << >>
    PREC_TERM,        // + -
    PREC_FACTOR,      // * / %
    PREC_UNARY,       // ! - ~ 
    PREC_CALL,        // . () [] ?.
    PREC_PRIMARY
} Precedence;

typedef Expr* (*ParseFn)(Parser* parser, Expr* left);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

// Primary expressions
static Expr* literal(Parser* parser, Expr* left)
{
    (void)left; // Unused
    
    switch (parser->previous.type)
    {
        case TOKEN_NIL:
            return expr_create_literal_nil();
        case TOKEN_TRUE:
            return expr_create_literal_bool(true);
        case TOKEN_FALSE:
            return expr_create_literal_bool(false);
        case TOKEN_INT:
        {
            long long value = strtoll(parser->previous.lexeme, NULL, 10);
            return expr_create_literal_int(value);
        }
        case TOKEN_FLOAT:
        {
            double value = strtod(parser->previous.lexeme, NULL);
            return expr_create_literal_float(value);
        }
        default:
            parser_error(parser, "Unexpected token in literal");
            return NULL;
    }
}

static Expr* string(Parser* parser, Expr* left)
{
    (void)left; // Unused
    
    // Handle string interpolation
    if (parser->previous.type == TOKEN_STRING_INTERP_START)
    {
        // Use temporary allocator for building parts
        Allocator* temp_alloc = allocators_get(ALLOC_SYSTEM_TEMP);
        size_t part_capacity = 8;
        size_t part_count = 0;
        char** parts = MEM_NEW_ARRAY(temp_alloc, char*, part_capacity);
        
        size_t expr_capacity = 8;
        size_t expr_count = 0;
        Expr** expressions = MEM_NEW_ARRAY(temp_alloc, Expr*, expr_capacity);
        
        // First part is empty (before first expression)
        parts[part_count++] = AST_DUP("");
        
        while (!check(parser, TOKEN_STRING_INTERP_END))
        {
            // Parse expression
            Expr* expr = expression(parser);
            if (!expr) return NULL;
            
            if (expr_count >= expr_capacity)
            {
                expr_capacity *= 2;
                expressions = grow_array(parser, expressions, expr_count, expr_capacity, sizeof(Expr*));
            }
            expressions[expr_count++] = expr;
            
            // Get string part after expression
            if (match(parser, TOKEN_STRING_INTERP_MIDDLE))
            {
                if (part_count >= part_capacity)
                {
                    part_capacity *= 2;
                    parts = grow_array(parser, parts, part_count, part_capacity, sizeof(char*));
                }
                
                // Extract string content (remove quotes)
                size_t len = parser->previous.lexeme_length - 2;
                char* content = MEM_ALLOC(temp_alloc, len + 1);
                memcpy(content, parser->previous.lexeme + 1, len);
                content[len] = '\0';
                parts[part_count++] = AST_DUP(content);
                MEM_FREE(temp_alloc, content, len + 1);
            }
        }
        
        consume(parser, TOKEN_STRING_INTERP_END, "Expected end of string interpolation");
        
        // Last part
        if (part_count >= part_capacity)
        {
            part_capacity *= 2;
            parts = grow_array(parser, parts, part_count, part_capacity, sizeof(char*));
        }
        
        // Extract final string content
        size_t len = parser->previous.lexeme_length - 1;  // Remove ending quote
        char* content = MEM_ALLOC(temp_alloc, len + 1);
        memcpy(content, parser->previous.lexeme, len);
        content[len] = '\0';
        parts[part_count++] = AST_DUP(content);
        MEM_FREE(temp_alloc, content, len + 1);
        
        // Create AST node with copies in AST allocator
        char** ast_parts = AST_NEW_ARRAY(char*, part_count);
        memcpy(ast_parts, parts, part_count * sizeof(char*));
        
        Expr** ast_expressions = AST_NEW_ARRAY(Expr*, expr_count);
        memcpy(ast_expressions, expressions, expr_count * sizeof(Expr*));
        
        // Clean up temporary arrays
        MEM_FREE(temp_alloc, parts, part_capacity * sizeof(char*));
        MEM_FREE(temp_alloc, expressions, expr_capacity * sizeof(Expr*));
        
        return expr_create_string_interp(ast_parts, ast_expressions, part_count, expr_count);
    }
    else
    {
        // Regular string
        // Remove quotes
        size_t len = parser->previous.lexeme_length - 2;
        char* value = MEM_ALLOC(allocators_get(ALLOC_SYSTEM_TEMP), len + 1);
        memcpy(value, parser->previous.lexeme + 1, len);
        value[len] = '\0';
        
        Expr* result = expr_create_literal_string(value);
        MEM_FREE(allocators_get(ALLOC_SYSTEM_TEMP), value, len + 1);
        
        return result;
    }
}

// Note: This is a partial refactoring of parser.c
// The rest of the parser functions need similar treatment
// Key changes:
// 1. Parser structure allocated with parser allocator
// 2. Temporary arrays use temp allocator
// 3. AST nodes use AST allocator (through ast_allocators.c)
// 4. String manipulation uses temp allocator