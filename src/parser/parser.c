#include "parser/parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

Parser* parser_create(const char* source)
{
    Parser* parser = malloc(sizeof(Parser));
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
        free(parser);
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
    // Semicolon is optional, so we only consume it if it's there
    if (check(parser, TOKEN_SEMICOLON))
    {
        advance(parser);
    }
}

static void synchronize(Parser* parser)
{
    parser->panic_mode = false;

    while (parser->current.type != TOKEN_EOF)
    {
        if (parser->previous.type == TOKEN_SEMICOLON) return;

        switch (parser->current.type)
        {
        case TOKEN_CLASS:
        case TOKEN_FUNC:
        case TOKEN_VAR:
        case TOKEN_LET:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_RETURN:
            return;
        default:
            ;
        }

        advance(parser);
    }
}

// Forward declarations
static Expr* expression(Parser* parser);
static Stmt* statement(Parser* parser);
static Stmt* declaration(Parser* parser);
static Stmt* block_statement(Parser* parser);
static Expr* parse_object_literal(Parser* parser);
static Expr* finish_call(Parser* parser, Expr* callee);
static char* parse_module_path(Parser* parser);

// Expression parsing using recursive descent
// This implements operator precedence through a series of parsing functions
// Each function handles operators at a specific precedence level

// Primary expressions - the highest precedence (literals, identifiers, parentheses)
static Expr* primary(Parser* parser)
{
    if (match(parser, TOKEN_TRUE))
    {
        return expr_create_literal_bool(true);
    }

    if (match(parser, TOKEN_FALSE))
    {
        return expr_create_literal_bool(false);
    }

    if (match(parser, TOKEN_NIL))
    {
        return expr_create_literal_nil();
    }

    if (match(parser, TOKEN_INTEGER))
    {
        return expr_create_literal_int(parser->previous.literal.integer_value);
    }

    if (match(parser, TOKEN_FLOAT))
    {
        return expr_create_literal_float(parser->previous.literal.float_value);
    }

    if (match(parser, TOKEN_STRING))
    {
        Expr* node = expr_create_literal_string(parser->previous.literal.string_value);
        token_free(&parser->previous);
        return node;
    }
    
    if (match(parser, TOKEN_STRING_INTERP_START))
    {
        // Parse interpolated string like "Hello, ${name}!" or "Value: $count"
        // String interpolation is parsed into parts and expressions:
        // "Hello, ${name}!" -> parts: ["Hello, ", "!"], expressions: [name]
        size_t part_capacity = 4;
        size_t expr_capacity = 4;
        char** parts = malloc(part_capacity * sizeof(char*));
        Expr** expressions = malloc(expr_capacity * sizeof(Expr*));
        size_t part_count = 0;
        size_t expr_count = 0;
        
        // Add first part (the string before the first interpolation)
        parts[part_count++] = strdup(parser->previous.literal.string_value);
        token_free(&parser->previous);
        
        while (true) {
            // Parse the expression after $
            Expr* expr = NULL;
            
            if (match(parser, TOKEN_DOLLAR)) {
                // ${expr} form - allows any expression
                expr = expression(parser);
                consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after interpolation expression.");
            } else if (match(parser, TOKEN_DOLLAR_IDENT)) {
                // $identifier form - shorthand for simple variables
                // Extract the identifier part (skip the $)
                char* ident = malloc(parser->previous.lexeme_length);
                memcpy(ident, parser->previous.lexeme + 1, parser->previous.lexeme_length - 1);
                ident[parser->previous.lexeme_length - 1] = '\0';
                
                expr = expr_create_variable(ident);
                free(ident);
            } else if (match(parser, TOKEN_IDENTIFIER)) {
                // Simple identifier form (lexer now returns TOKEN_IDENTIFIER for $name)
                expr = expr_create_variable(parser->previous.literal.string_value);
                token_free(&parser->previous);
            } else {
                parser_error_at_current(parser, "Expect interpolation expression after string part.");
                break;
            }
            
            // Store the expression if we got one
            if (expr) {
                if (expr_count >= expr_capacity) {
                    expr_capacity *= 2;
                    expressions = realloc(expressions, expr_capacity * sizeof(Expr*));
                }
                expressions[expr_count++] = expr;
            }
            
            // Get the next string part (after the interpolation)
            if (match(parser, TOKEN_STRING_INTERP_MID)) {
                // Middle part - more interpolations to come
                if (part_count >= part_capacity) {
                    part_capacity *= 2;
                    parts = realloc(parts, part_capacity * sizeof(char*));
                }
                parts[part_count++] = strdup(parser->previous.literal.string_value);
                token_free(&parser->previous);
            } else if (match(parser, TOKEN_STRING_INTERP_END)) {
                // Final part - end of interpolated string
                if (part_count >= part_capacity) {
                    part_capacity *= 2;
                    parts = realloc(parts, part_capacity * sizeof(char*));
                }
                parts[part_count++] = strdup(parser->previous.literal.string_value);
                token_free(&parser->previous);
                break;
            } else {
                parser_error_at_current(parser, "Expect string continuation or end in interpolation.");
                break;
            }
        }
        
        return expr_create_string_interp(parts, expressions, part_count, expr_count);
    }

    if (match(parser, TOKEN_CHARACTER))
    {
        char str[2] = {parser->previous.literal.character_value, '\0'};
        return expr_create_literal_string(str);
    }

    if (match(parser, TOKEN_SELF))
    {
        return expr_create_variable("self");
    }

    if (match(parser, TOKEN_IDENTIFIER))
    {
        char* name = malloc(parser->previous.lexeme_length + 1);
        memcpy(name, parser->previous.lexeme, parser->previous.lexeme_length);
        name[parser->previous.lexeme_length] = '\0';
        Expr* node = expr_create_variable(name);
        free(name);
        return node;
    }

    if (match(parser, TOKEN_LEFT_PAREN))
    {
        Expr* expr = expression(parser);
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
        return expr;
    }

    if (match(parser, TOKEN_LEFT_BRACKET))
    {
        size_t capacity = 8;
        size_t count = 0;
        Expr** elements = malloc(capacity * sizeof(Expr*));

        if (!check(parser, TOKEN_RIGHT_BRACKET))
        {
            do
            {
                if (count >= capacity)
                {
                    capacity *= 2;
                    elements = realloc(elements, capacity * sizeof(Expr*));
                }
                elements[count++] = expression(parser);
            }
            while (match(parser, TOKEN_COMMA));
        }

        consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after array elements.");

        return expr_create_array_literal(elements, count);
    }
    
    if (match(parser, TOKEN_LEFT_BRACE))
    {
        // Check if this is an object literal or a closure
        // Object literal: { key: value, ... } or {}
        // Closure: { params in body } or { statements }
        
        // For empty braces, treat as empty object literal
        if (check(parser, TOKEN_RIGHT_BRACE)) {
            advance(parser); // consume }
            return expr_create_object_literal(NULL, NULL, 0);
        }
        
        // Disambiguation between object literal and closure
        // Object literal: { key: value, ... } where key is identifier or string
        // Closure: { params in body } or { body }
        
        // Strategy: Look at the first few tokens to decide
        // - If we see string followed by ':', it's definitely object literal
        // - If we see identifier followed by ':', it's object literal
        // - If we see identifier followed by 'in', it's closure with params
        // - Otherwise, it's a closure body
        
        // Check for string key (always object literal)
        if (check(parser, TOKEN_STRING)) {
            // Parse as object literal
            return parse_object_literal(parser);
        }
        
        // For identifiers, we need to be more careful
        if (check(parser, TOKEN_IDENTIFIER)) {
            // We need to look ahead to see if this is an object key or closure param/body
            // Save current state
            Token first_token = parser->current;
            advance(parser); // consume identifier
            
            if (check(parser, TOKEN_COLON)) {
                // It's an object literal! Parse it properly
                // We've already consumed the first key
                char* first_key = malloc(first_token.lexeme_length + 1);
                memcpy(first_key, first_token.lexeme, first_token.lexeme_length);
                first_key[first_token.lexeme_length] = '\0';
                
                advance(parser); // consume ':'
                Expr* first_value = expression(parser);
                
                // Parse remaining key-value pairs
                size_t capacity = 8;
                size_t count = 1;
                const char** keys = malloc(capacity * sizeof(char*));
                Expr** values = malloc(capacity * sizeof(Expr*));
                keys[0] = first_key;
                values[0] = first_value;
                
                while (match(parser, TOKEN_COMMA)) {
                    if (count >= capacity) {
                        capacity *= 2;
                        keys = realloc(keys, capacity * sizeof(char*));
                        values = realloc(values, capacity * sizeof(Expr*));
                    }
                    
                    // Parse key
                    char* key;
                    if (match(parser, TOKEN_IDENTIFIER)) {
                        key = malloc(parser->previous.lexeme_length + 1);
                        memcpy(key, parser->previous.lexeme, parser->previous.lexeme_length);
                        key[parser->previous.lexeme_length] = '\0';
                    } else if (match(parser, TOKEN_STRING)) {
                        size_t str_len = parser->previous.lexeme_length - 2;
                        key = malloc(str_len + 1);
                        memcpy(key, parser->previous.lexeme + 1, str_len);
                        key[str_len] = '\0';
                    } else {
                        parser_error_at_current(parser, "Expect property key.");
                        // Clean up
                        for (size_t i = 0; i < count; i++) {
                            free((char*)keys[i]);
                        }
                        free(keys);
                        free(values);
                        return NULL;
                    }
                    
                    consume(parser, TOKEN_COLON, "Expect ':' after property key.");
                    
                    keys[count] = key;
                    values[count] = expression(parser);
                    count++;
                }
                
                consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after object literal.");
                return expr_create_object_literal(keys, values, count);
            } else {
                // Not an object literal - we consumed an identifier that's part of closure
                // We need to check if it's a parameter (followed by 'in' or ',')
                // or the start of the closure body
                
                // Check for closure parameters
                if (check(parser, TOKEN_IN) || check(parser, TOKEN_COMMA)) {
                    // It's a closure with parameters!
                    // Start collecting parameters
                    size_t param_capacity = 8;
                    size_t param_count = 1;
                    const char** param_names = malloc(param_capacity * sizeof(char*));
                    TypeExpr** param_types = malloc(param_capacity * sizeof(TypeExpr*));
                    
                    // Save the first parameter we already consumed
                    char* first_param = malloc(first_token.lexeme_length + 1);
                    memcpy(first_param, first_token.lexeme, first_token.lexeme_length);
                    first_param[first_token.lexeme_length] = '\0';
                    param_names[0] = first_param;
                    param_types[0] = NULL;
                    
                    // Parse remaining parameters if any
                    while (match(parser, TOKEN_COMMA)) {
                        if (!match(parser, TOKEN_IDENTIFIER)) {
                            parser_error_at_current(parser, "Expect parameter name.");
                            free(param_names);
                            free(param_types);
                            return NULL;
                        }
                        
                        if (param_count >= param_capacity) {
                            param_capacity *= 2;
                            param_names = realloc(param_names, param_capacity * sizeof(char*));
                            param_types = realloc(param_types, param_capacity * sizeof(TypeExpr*));
                        }
                        
                        char* param = malloc(parser->previous.lexeme_length + 1);
                        memcpy(param, parser->previous.lexeme, parser->previous.lexeme_length);
                        param[parser->previous.lexeme_length] = '\0';
                        param_names[param_count] = param;
                        param_types[param_count] = NULL;
                        param_count++;
                    }
                    
                    // Consume 'in'
                    consume(parser, TOKEN_IN, "Expect 'in' after closure parameters.");
                    
                    // Parse body as expression or block
                    Stmt* body;
                    if (check(parser, TOKEN_RIGHT_BRACE)) {
                        // Empty body
                        body = stmt_create_block(NULL, 0);
                    } else {
                        // Try to parse as single expression first
                        Expr* expr = expression(parser);
                        if (check(parser, TOKEN_RIGHT_BRACE)) {
                            // Single expression body
                            Stmt* return_stmt = stmt_create_return(expr);
                            body = stmt_create_block(&return_stmt, 1);
                        } else {
                            // Multiple statements - but we already parsed first as expression
                            // Create statement from expression
                            size_t stmt_capacity = 8;
                            size_t stmt_count = 1;
                            Stmt** statements = malloc(stmt_capacity * sizeof(Stmt*));
                            statements[0] = stmt_create_expression(expr);
                            
                            // Parse remaining statements
                            while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
                                if (stmt_count >= stmt_capacity) {
                                    stmt_capacity *= 2;
                                    statements = realloc(statements, stmt_capacity * sizeof(Stmt*));
                                }
                                statements[stmt_count++] = statement(parser);
                            }
                            
                            body = stmt_create_block(statements, stmt_count);
                        }
                    }
                    
                    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after closure body.");
                    return expr_create_closure(param_names, param_types, param_count, NULL, body);
                } else {
                    // The identifier we consumed is the first part of the closure body
                    // We need to parse it as a statement/expression
                    // This is tricky because we've already consumed the token
                    
                    // Check if the identifier is followed by a function call or other expression
                    // For now, let's create a variable expression and continue parsing
                    Expr* first_expr = expr_create_variable(first_token.lexeme);
                    
                    // Check if there's more to the expression (like function call)
                    while (match(parser, TOKEN_LEFT_PAREN) || match(parser, TOKEN_DOT) || 
                           match(parser, TOKEN_LEFT_BRACKET)) {
                        if (parser->previous.type == TOKEN_LEFT_PAREN) {
                            // Function call
                            first_expr = finish_call(parser, first_expr);
                        } else if (parser->previous.type == TOKEN_DOT) {
                            // Member access
                            consume(parser, TOKEN_IDENTIFIER, "Expect property name after '.'.");
                            char* member = malloc(parser->previous.lexeme_length + 1);
                            memcpy(member, parser->previous.lexeme, parser->previous.lexeme_length);
                            member[parser->previous.lexeme_length] = '\0';
                            first_expr = expr_create_member(first_expr, member);
                        } else if (parser->previous.type == TOKEN_LEFT_BRACKET) {
                            // Array subscript
                            Expr* index = expression(parser);
                            consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after array index.");
                            first_expr = expr_create_subscript(first_expr, index);
                        }
                    }
                    
                    // Now parse the rest of the closure body
                    size_t stmt_capacity = 8;
                    size_t stmt_count = 0;
                    Stmt** statements = malloc(stmt_capacity * sizeof(Stmt*));
                    
                    // Add the first expression as a statement
                    statements[stmt_count++] = stmt_create_expression(first_expr);
                    
                    // Parse remaining statements
                    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
                        if (stmt_count >= stmt_capacity) {
                            stmt_capacity *= 2;
                            statements = realloc(statements, stmt_capacity * sizeof(Stmt*));
                        }
                        statements[stmt_count++] = statement(parser);
                    }
                    
                    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after closure body.");
                    
                    // Create closure with no parameters
                    Stmt* body = stmt_create_block(statements, stmt_count);
                    return expr_create_closure(NULL, NULL, 0, NULL, body);
                }
            }
        }
        
        // If we get here, it's a closure with no parameters starting with something else
        // Parse as closure body
        size_t stmt_capacity = 8;
        size_t stmt_count = 0;
        Stmt** statements = malloc(stmt_capacity * sizeof(Stmt*));
        
        while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF)) {
            if (stmt_count >= stmt_capacity) {
                stmt_capacity *= 2;
                statements = realloc(statements, stmt_capacity * sizeof(Stmt*));
            }
            statements[stmt_count++] = statement(parser);
            
            // Safety check to prevent infinite loop
            if (parser->had_error) {
                break;
            }
        }
        
        consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after closure body.");
        
        // Create closure with no parameters
        Stmt* body = stmt_create_block(statements, stmt_count);
        return expr_create_closure(NULL, NULL, 0, NULL, body);
    }

    parser_error_at_current(parser, "Expect expression.");
    return NULL;
}

// Helper function to parse object literals
static Expr* parse_object_literal(Parser* parser)
{
    size_t capacity = 8;
    size_t count = 0;
    const char** keys = malloc(capacity * sizeof(char*));
    Expr** values = malloc(capacity * sizeof(Expr*));
    
    do {
        if (count > 0 && !match(parser, TOKEN_COMMA)) {
            break;
        }
        
        if (check(parser, TOKEN_RIGHT_BRACE)) {
            break;  // Allow trailing comma
        }
        
        // Parse key
        char* key;
        if (match(parser, TOKEN_IDENTIFIER)) {
            key = malloc(parser->previous.lexeme_length + 1);
            memcpy(key, parser->previous.lexeme, parser->previous.lexeme_length);
            key[parser->previous.lexeme_length] = '\0';
        } else if (match(parser, TOKEN_STRING)) {
            size_t str_len = parser->previous.lexeme_length - 2;
            key = malloc(str_len + 1);
            memcpy(key, parser->previous.lexeme + 1, str_len);
            key[str_len] = '\0';
        } else {
            parser_error_at_current(parser, "Expect property key.");
            // Clean up
            for (size_t i = 0; i < count; i++) {
                free((char*)keys[i]);
            }
            free(keys);
            free(values);
            return NULL;
        }
        
        consume(parser, TOKEN_COLON, "Expect ':' after property key.");
        
        if (count >= capacity) {
            capacity *= 2;
            keys = realloc(keys, capacity * sizeof(char*));
            values = realloc(values, capacity * sizeof(Expr*));
        }
        
        keys[count] = key;
        values[count] = expression(parser);
        count++;
    } while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF));
    
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after object literal.");
    return expr_create_object_literal(keys, values, count);
}

// Helper function to finish parsing a function call
static Expr* finish_call(Parser* parser, Expr* callee)
{
    size_t capacity = 8;
    size_t count = 0;
    Expr** arguments = malloc(capacity * sizeof(Expr*));

    if (!check(parser, TOKEN_RIGHT_PAREN))
    {
        do
        {
            if (count >= capacity)
            {
                capacity *= 2;
                arguments = realloc(arguments, capacity * sizeof(Expr*));
            }
            
            // Check for named parameter syntax: name: value
            // First, try to parse as expression
            Expr* arg = expression(parser);
            
            // Check if we have a colon after parsing an identifier
            if (arg && arg->type == EXPR_VARIABLE && match(parser, TOKEN_COLON))
            {
                // This was a parameter name, not a value
                // Free the expression we created
                expr_destroy(arg);
                // Now parse the actual value
                arguments[count++] = expression(parser);
            }
            else
            {
                // It was a regular expression argument
                arguments[count++] = arg;
            }
        }
        while (match(parser, TOKEN_COMMA));
    }

    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return expr_create_call(callee, arguments, count);
}

static Expr* call(Parser* parser)
{
    Expr* expr = primary(parser);

    while (true)
    {
        if (match(parser, TOKEN_LEFT_PAREN))
        {
            // Parse function call
            expr = finish_call(parser, expr);
        }
        else if (match(parser, TOKEN_LEFT_BRACKET))
        {
            // Parse array subscript
            Expr* index = expression(parser);
            consume(parser, TOKEN_RIGHT_BRACKET, "Expect ']' after array index.");
            expr = expr_create_subscript(expr, index);
        }
        else if (match(parser, TOKEN_DOT))
        {
            // Parse member access
            consume(parser, TOKEN_IDENTIFIER, "Expect property name after '.'.");
            char* property = malloc(parser->previous.lexeme_length + 1);
            memcpy(property, parser->previous.lexeme, parser->previous.lexeme_length);
            property[parser->previous.lexeme_length] = '\0';
            expr = expr_create_member(expr, property);
            free(property);
        }
        else if (match(parser, TOKEN_PLUS_PLUS) || match(parser, TOKEN_MINUS_MINUS))
        {
            // Parse postfix increment/decrement
            // Convert to assignment: x++ becomes x = x + 1
            Token op = parser->previous;
            Token add_op = op;
            add_op.type = (op.type == TOKEN_PLUS_PLUS) ? TOKEN_PLUS : TOKEN_MINUS;
            
            // Create the increment expression: x + 1 or x - 1
            Expr* one = expr_create_literal_int(1);
            Expr* add_expr = expr_create_binary(add_op, expr, one);
            
            // Create assignment: x = x + 1
            expr = expr_create_assignment(expr, add_expr);
        }
        else
        {
            break;
        }
    }

    return expr;
}


static Expr* unary(Parser* parser)
{
    if (match(parser, TOKEN_NOT) || match(parser, TOKEN_MINUS) ||
        match(parser, TOKEN_PLUS) || match(parser, TOKEN_TILDE))
    {
        Token op = parser->previous;
        Expr* right = unary(parser);

        return expr_create_unary(op, right);
    }

    return call(parser);
}

static Expr* multiplication(Parser* parser)
{
    Expr* expr = unary(parser);

    while (match(parser, TOKEN_STAR) || match(parser, TOKEN_SLASH) || match(parser, TOKEN_PERCENT))
    {
        Token op = parser->previous;
        Expr* right = unary(parser);
        expr = expr_create_binary(op, expr, right);
    }

    return expr;
}

static Expr* addition(Parser* parser)
{
    Expr* expr = multiplication(parser);

    while (match(parser, TOKEN_PLUS) || match(parser, TOKEN_MINUS))
    {
        Token op = parser->previous;
        Expr* right = multiplication(parser);
        expr = expr_create_binary(op, expr, right);
    }

    return expr;
}

static Expr* shift(Parser* parser)
{
    Expr* expr = addition(parser);

    while (match(parser, TOKEN_SHIFT_LEFT) || match(parser, TOKEN_SHIFT_RIGHT))
    {
        Token op = parser->previous;
        Expr* right = addition(parser);
        expr = expr_create_binary(op, expr, right);
    }

    return expr;
}

static Expr* comparison(Parser* parser)
{
    Expr* expr = shift(parser);

    while (match(parser, TOKEN_GREATER) || match(parser, TOKEN_GREATER_EQUAL) ||
        match(parser, TOKEN_LESS) || match(parser, TOKEN_LESS_EQUAL))
    {
        Token op = parser->previous;
        Expr* right = shift(parser);
        expr = expr_create_binary(op, expr, right);
    }

    return expr;
}

static Expr* equality(Parser* parser)
{
    Expr* expr = comparison(parser);

    while (match(parser, TOKEN_NOT_EQUAL) || match(parser, TOKEN_EQUAL_EQUAL))
    {
        Token op = parser->previous;
        Expr* right = comparison(parser);
        expr = expr_create_binary(op, expr, right);
    }

    return expr;
}

static Expr* bitwise_and(Parser* parser)
{
    Expr* expr = equality(parser);

    while (match(parser, TOKEN_AMPERSAND))
    {
        Token op = parser->previous;
        Expr* right = equality(parser);
        expr = expr_create_binary(op, expr, right);
    }

    return expr;
}

static Expr* bitwise_xor(Parser* parser)
{
    Expr* expr = bitwise_and(parser);

    while (match(parser, TOKEN_CARET))
    {
        Token op = parser->previous;
        Expr* right = bitwise_and(parser);
        expr = expr_create_binary(op, expr, right);
    }

    return expr;
}

static Expr* bitwise_or(Parser* parser)
{
    Expr* expr = bitwise_xor(parser);

    while (match(parser, TOKEN_PIPE))
    {
        Token op = parser->previous;
        Expr* right = bitwise_xor(parser);
        expr = expr_create_binary(op, expr, right);
    }

    return expr;
}

static Expr* logical_and(Parser* parser)
{
    Expr* expr = bitwise_or(parser);

    while (match(parser, TOKEN_AND_AND))
    {
        Token op = parser->previous;
        Expr* right = bitwise_or(parser);
        expr = expr_create_binary(op, expr, right);
    }

    return expr;
}

static Expr* logical_or(Parser* parser)
{
    Expr* expr = logical_and(parser);

    while (match(parser, TOKEN_OR_OR))
    {
        Token op = parser->previous;
        Expr* right = logical_and(parser);
        expr = expr_create_binary(op, expr, right);
    }

    return expr;
}

static Expr* nil_coalescing(Parser* parser)
{
    Expr* expr = logical_or(parser);

    while (match(parser, TOKEN_QUESTION_QUESTION))
    {
        Token op = parser->previous;
        Expr* right = logical_or(parser);
        expr = expr_create_binary(op, expr, right);
    }

    return expr;
}

static Expr* assignment(Parser* parser)
{
    // Parse left-hand side first (could be variable, array element, or member)
    Expr* expr = nil_coalescing(parser);

    if (match(parser, TOKEN_EQUAL))
    {
        // Right-associative: a = b = c parses as a = (b = c)
        Expr* value = assignment(parser);

        // Validate that we have a valid assignment target (lvalue)
        if (expr->type == EXPR_VARIABLE)
        {
            return expr_create_assignment(expr, value);
        }
        if (expr->type == EXPR_SUBSCRIPT)
        {
            // Allow array element assignment: arr[i] = value
            return expr_create_assignment(expr, value);
        }
        if (expr->type == EXPR_MEMBER)
        {
            // Allow member assignment: obj.prop = value
            return expr_create_assignment(expr, value);
        }

        parser_error(parser, "Invalid assignment target.");
    }

    return expr;
}

// Top level expression parsing - this is the entry point
static Expr* expression(Parser* parser)
{
    return assignment(parser);
}

// Statement parsing
static Stmt* expression_statement(Parser* parser)
{
    Expr* expr = expression(parser);
    optional_semicolon(parser);
    return stmt_create_expression(expr);
}

static Stmt* var_statement(Parser* parser)
{
    bool is_mutable = parser->previous.type == TOKEN_VAR;

    consume(parser, TOKEN_IDENTIFIER, "Expect variable name.");
    char* name = malloc(parser->previous.lexeme_length + 1);
    memcpy(name, parser->previous.lexeme, parser->previous.lexeme_length);
    name[parser->previous.lexeme_length] = '\0';

    char* type_annotation = NULL;
    if (match(parser, TOKEN_COLON))
    {
        consume(parser, TOKEN_IDENTIFIER, "Expect type name.");
        type_annotation = malloc(parser->previous.lexeme_length + 1);
        memcpy(type_annotation, parser->previous.lexeme, parser->previous.lexeme_length);
        type_annotation[parser->previous.lexeme_length] = '\0';
    }

    Expr* initializer = NULL;
    if (match(parser, TOKEN_EQUAL))
    {
        initializer = expression(parser);
    }

    optional_semicolon(parser);

    Stmt* stmt = stmt_create_var_decl(is_mutable, name, type_annotation, initializer);
    free(name);
    free(type_annotation);
    return stmt;
}

static Stmt* block_statement(Parser* parser)
{
    size_t capacity = 8;
    size_t count = 0;
    Stmt** statements = malloc(capacity * sizeof(Stmt*));

    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF))
    {
        if (count >= capacity)
        {
            capacity *= 2;
            statements = realloc(statements, capacity * sizeof(Stmt*));
        }
        statements[count++] = declaration(parser);
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after block.");
    return stmt_create_block(statements, count);
}

static Stmt* if_statement(Parser* parser)
{
    // Check if we have parentheses (optional in Swift-style)
    bool has_parens = match(parser, TOKEN_LEFT_PAREN);
    
    Expr* condition = expression(parser);
    
    if (has_parens)
    {
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after if condition.");
    }

    Stmt* then_branch = statement(parser);
    Stmt* else_branch = NULL;
    if (match(parser, TOKEN_ELSE))
    {
        else_branch = statement(parser);
    }

    return stmt_create_if(condition, then_branch, else_branch);
}

static Stmt* while_statement(Parser* parser)
{
    // Check if we have parentheses (optional in Swift-style)
    bool has_parens = match(parser, TOKEN_LEFT_PAREN);
    
    Expr* condition = expression(parser);
    
    if (has_parens)
    {
        consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after condition.");
    }
    
    Stmt* body = statement(parser);

    return stmt_create_while(condition, body);
}

static Stmt* for_statement(Parser* parser)
{
    // Check if we have parentheses (Java-style) or not (Swift-style)
    bool has_parens = match(parser, TOKEN_LEFT_PAREN);
    
    if (!has_parens)
    {
        // Swift-style for-in loop: for identifier in expression
        consume(parser, TOKEN_IDENTIFIER, "Expect variable name after 'for'.");
        char* var_name = malloc(parser->previous.lexeme_length + 1);
        memcpy(var_name, parser->previous.lexeme, parser->previous.lexeme_length);
        var_name[parser->previous.lexeme_length] = '\0';
        
        consume(parser, TOKEN_IN, "Expect 'in' after for loop variable.");
        Expr* iterable = expression(parser);
        
        consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before loop body.");
        Stmt* body = block_statement(parser);
        
        Stmt* stmt = stmt_create_for_in(var_name, iterable, body);
        free(var_name);
        return stmt;
    }
    
    // We have parentheses - check if this is for-in or traditional for
    if (check(parser, TOKEN_IDENTIFIER))
    {
        // Save current position
        Token saved_current = parser->current;
        Token saved_previous = parser->previous;
        
        // Advance to get the identifier
        advance(parser);
        
        // Check if next token is 'in'
        if (check(parser, TOKEN_IN))
        {
            // This is a for-in loop with parentheses
            char* var_name = malloc(parser->previous.lexeme_length + 1);
            memcpy(var_name, parser->previous.lexeme, parser->previous.lexeme_length);
            var_name[parser->previous.lexeme_length] = '\0';
            
            advance(parser);  // consume 'in'
            Expr* iterable = expression(parser);
            consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after iterable.");
            
            Stmt* body = statement(parser);
            
            Stmt* stmt = stmt_create_for_in(var_name, iterable, body);
            free(var_name);
            return stmt;
        }
        else
        {
            // Restore position for regular for loop
            parser->current = saved_current;
            parser->previous = saved_previous;
        }
    }
    
    // Traditional for loop: for (init; condition; increment)
    Stmt* initializer = NULL;
    
    if (match(parser, TOKEN_SEMICOLON))
    {
        // No initializer
    }
    else if (match(parser, TOKEN_VAR))
    {
        initializer = var_statement(parser);
    }
    else
    {
        initializer = expression_statement(parser);
    }
    
    Expr* condition = NULL;
    if (!check(parser, TOKEN_SEMICOLON))
    {
        condition = expression(parser);
    }
    consume(parser, TOKEN_SEMICOLON, "Expect ';' after loop condition.");
    
    Expr* increment = NULL;
    if (!check(parser, TOKEN_RIGHT_PAREN))
    {
        increment = expression(parser);
    }
    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after for clauses.");
    
    Stmt* body = statement(parser);
    
    return stmt_create_for(initializer, condition, increment, body);
}

static Stmt* return_statement(Parser* parser)
{
    Expr* value = NULL;
    if (!check(parser, TOKEN_SEMICOLON))
    {
        value = expression(parser);
    }
    optional_semicolon(parser);
    return stmt_create_return(value);
}

static Stmt* break_statement(Parser* parser)
{
    optional_semicolon(parser);
    return stmt_create_break();
}

static Stmt* continue_statement(Parser* parser)
{
    optional_semicolon(parser);
    return stmt_create_continue();
}

static Stmt* statement(Parser* parser)
{
    if (match(parser, TOKEN_IF)) return if_statement(parser);
    if (match(parser, TOKEN_WHILE)) return while_statement(parser);
    if (match(parser, TOKEN_FOR)) return for_statement(parser);
    if (match(parser, TOKEN_RETURN)) return return_statement(parser);
    if (match(parser, TOKEN_BREAK)) return break_statement(parser);
    if (match(parser, TOKEN_CONTINUE)) return continue_statement(parser);
    if (match(parser, TOKEN_LEFT_BRACE)) return block_statement(parser);

    return expression_statement(parser);
}

// Forward declarations
static Stmt* function_declaration(Parser* parser);

static Stmt* class_declaration(Parser* parser)
{
    consume(parser, TOKEN_IDENTIFIER, "Expect class name.");
    char* name = malloc(parser->previous.lexeme_length + 1);
    memcpy(name, parser->previous.lexeme, parser->previous.lexeme_length);
    name[parser->previous.lexeme_length] = '\0';

    char* superclass = NULL;
    if (match(parser, TOKEN_COLON)) {
        consume(parser, TOKEN_IDENTIFIER, "Expect superclass name.");
        superclass = malloc(parser->previous.lexeme_length + 1);
        memcpy(superclass, parser->previous.lexeme, parser->previous.lexeme_length);
        superclass[parser->previous.lexeme_length] = '\0';
    }

    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before class body.");

    size_t capacity = 16;
    size_t count = 0;
    Stmt** members = malloc(capacity * sizeof(Stmt*));

    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF))
    {
        if (count >= capacity)
        {
            capacity *= 2;
            members = realloc(members, capacity * sizeof(Stmt*));
        }

        // Parse member (property or method)
        if (match(parser, TOKEN_VAR) || match(parser, TOKEN_LET)) {
            // Property declaration
            bool is_mutable = parser->previous.type == TOKEN_VAR;
            
            consume(parser, TOKEN_IDENTIFIER, "Expect property name.");
            char* prop_name = malloc(parser->previous.lexeme_length + 1);
            memcpy(prop_name, parser->previous.lexeme, parser->previous.lexeme_length);
            prop_name[parser->previous.lexeme_length] = '\0';

            // Handle optional type annotation
            char* type_name = NULL;
            if (match(parser, TOKEN_COLON)) {
                consume(parser, TOKEN_IDENTIFIER, "Expect type name.");
                type_name = malloc(parser->previous.lexeme_length + 1);
                memcpy(type_name, parser->previous.lexeme, parser->previous.lexeme_length);
                type_name[parser->previous.lexeme_length] = '\0';
            }

            Expr* initializer = NULL;
            if (match(parser, TOKEN_EQUAL)) {
                initializer = expression(parser);
            }

            members[count++] = stmt_create_var_decl(is_mutable, prop_name, type_name, initializer);
            optional_semicolon(parser);
        }
        else if (match(parser, TOKEN_FUNC)) {
            // Method declaration
            members[count++] = function_declaration(parser);
        }
        else {
            parser_error_at_current(parser, "Expect property or method declaration in class body.");
            break;
        }
    }

    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after class body.");

    return stmt_create_class(name, superclass, members, count);
}

static Stmt* function_declaration(Parser* parser)
{
    // Check for extension method syntax: func Type.method(...)
    char* type_name = NULL;
    char* method_name = NULL;
    
    consume(parser, TOKEN_IDENTIFIER, "Expect function name or type name.");
    char* first_name = malloc(parser->previous.lexeme_length + 1);
    memcpy(first_name, parser->previous.lexeme, parser->previous.lexeme_length);
    first_name[parser->previous.lexeme_length] = '\0';
    
    if (match(parser, TOKEN_DOT))
    {
        // This is an extension method: Type.method
        type_name = first_name;
        consume(parser, TOKEN_IDENTIFIER, "Expect method name after '.'.");
        method_name = malloc(parser->previous.lexeme_length + 1);
        memcpy(method_name, parser->previous.lexeme, parser->previous.lexeme_length);
        method_name[parser->previous.lexeme_length] = '\0';
        
            // Debug
        // printf("DEBUG: Parsing extension method %s.%s\n", type_name, method_name);
    }
    else
    {
        // Regular function
        method_name = first_name;
    }
    
    char* name = method_name;

    consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after function name.");

    size_t capacity = 8;
    size_t count = 0;
    const char** param_names = malloc(capacity * sizeof(char*));
    const char** param_types = malloc(capacity * sizeof(char*));
    
    // For extension methods, automatically add 'this' as first parameter
    if (type_name != NULL)
    {
        param_names[count] = strdup("this");
        param_types[count] = type_name;  // The type of 'this' is the extended type
        count++;
    }

    if (!check(parser, TOKEN_RIGHT_PAREN))
    {
        do
        {
            if (count >= capacity)
            {
                capacity *= 2;
                param_names = realloc(param_names, capacity * sizeof(char*));
                param_types = realloc(param_types, capacity * sizeof(char*));
            }

            // Check for external parameter name syntax: externalName internalName
            // or just: parameterName (where external and internal are the same)
            consume(parser, TOKEN_IDENTIFIER, "Expect parameter name.");
            char* external_name = malloc(parser->previous.lexeme_length + 1);
            memcpy(external_name, parser->previous.lexeme, parser->previous.lexeme_length);
            external_name[parser->previous.lexeme_length] = '\0';
            
            char* internal_name = external_name;
            
            // Check if there's another identifier (internal name)
            if (check(parser, TOKEN_IDENTIFIER) && !check(parser, TOKEN_COLON))
            {
                advance(parser);
                internal_name = malloc(parser->previous.lexeme_length + 1);
                memcpy(internal_name, parser->previous.lexeme, parser->previous.lexeme_length);
                internal_name[parser->previous.lexeme_length] = '\0';
            }
            
            param_names[count] = internal_name;

            // Optional type annotation
            char* param_type = NULL;
            if (match(parser, TOKEN_COLON))
            {
                consume(parser, TOKEN_IDENTIFIER, "Expect type name.");
                param_type = malloc(parser->previous.lexeme_length + 1);
                memcpy(param_type, parser->previous.lexeme, parser->previous.lexeme_length);
                param_type[parser->previous.lexeme_length] = '\0';
            }
            param_types[count] = param_type;

            count++;
        }
        while (match(parser, TOKEN_COMMA));
    }

    consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    // Optional return type
    char* return_type = NULL;
    if (match(parser, TOKEN_ARROW))
    {
        consume(parser, TOKEN_IDENTIFIER, "Expect return type.");
        return_type = malloc(parser->previous.lexeme_length + 1);
        memcpy(return_type, parser->previous.lexeme, parser->previous.lexeme_length);
        return_type[parser->previous.lexeme_length] = '\0';
    }

    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    Stmt* body = block_statement(parser);

    // Create function statement
    Stmt* stmt = stmt_create_function(name, param_names, param_types, count, return_type, body);
    
    // If this is an extension method, mark it specially
    if (type_name != NULL)
    {
        // For extension methods, we need to prefix the name with the type
        // to distinguish it as an extension method
        size_t ext_name_len = strlen(type_name) + strlen(name) + 6; // "_ext_" + null
        char* ext_name = malloc(ext_name_len);
        snprintf(ext_name, ext_name_len, "%s_ext_%s", type_name, name);
        
        // printf("DEBUG: Created extension function name: %s\n", ext_name);
        
        // Free the old name and use the new one
        free((void*)stmt->function.name);
        stmt->function.name = ext_name;
        
        // Don't free type_name here - it's the same as first_name
    }

    // Clean up temporary allocations
    if (method_name != NULL) {
        // For extension methods, name points to method_name
        free(method_name);
        // first_name was used as type_name and already freed
    } else {
        // For regular functions, name points to first_name
        free(first_name);
    }
    
    for (size_t i = 0; i < count; i++)
    {
        free((void*)param_names[i]);
        if (param_types[i]) free((void*)param_types[i]);
    }
    free(param_names);
    free(param_types);
    if (return_type) free(return_type);

    return stmt;
}

// Parse an import path with proper prefix handling
static char* parse_import_path(Parser* parser, bool* is_local, bool* is_native)
{
    *is_local = false;
    *is_native = false;
    
    if (match(parser, TOKEN_AT)) {
        // Local import: @module or @module/submodule
        *is_local = true;
        
        size_t capacity = 64;
        char* path = malloc(capacity);
        strcpy(path, "@");
        size_t path_len = 1;
        
        // Parse module name
        consume(parser, TOKEN_IDENTIFIER, "Expect module name after '@'.");
        
        // Append module name
        size_t part_len = parser->previous.lexeme_length;
        if (path_len + part_len + 1 > capacity) {
            capacity = path_len + part_len + 32;
            path = realloc(path, capacity);
        }
        memcpy(path + path_len, parser->previous.lexeme, part_len);
        path_len += part_len;
        
        // Parse optional path segments
        while (match(parser, TOKEN_SLASH)) {
            if (path_len + 1 >= capacity) {
                capacity = path_len + 32;
                path = realloc(path, capacity);
            }
            path[path_len++] = '/';
            
            consume(parser, TOKEN_IDENTIFIER, "Expect path segment after '/'.");
            part_len = parser->previous.lexeme_length;
            if (path_len + part_len + 1 > capacity) {
                capacity = path_len + part_len + 32;
                path = realloc(path, capacity);
            }
            memcpy(path + path_len, parser->previous.lexeme, part_len);
            path_len += part_len;
        }
        
        path[path_len] = '\0';
        return path;
    }
    else if (match(parser, TOKEN_DOLLAR)) {
        // Native import: $native_module
        *is_native = true;
        
        consume(parser, TOKEN_IDENTIFIER, "Expect native module name after '$'.");
        
        size_t len = parser->previous.lexeme_length;
        char* path = malloc(len + 2);
        path[0] = '$';
        memcpy(path + 1, parser->previous.lexeme, len);
        path[len + 1] = '\0';
        
        return path;
    }
    else {
        // Package import: just module name or dotted path
        return parse_module_path(parser);
    }
}

// Parse a dotted module path like sys.native.io
static char* parse_module_path(Parser* parser)
{
    size_t path_len = 0;
    size_t capacity = 64;
    char* path = malloc(capacity);
    path[0] = '\0';
    
    // Parse first identifier
    consume(parser, TOKEN_IDENTIFIER, "Expect module name.");
    
    // Copy first part
    if (parser->previous.lexeme_length + 1 > capacity) {
        capacity = parser->previous.lexeme_length + 1;
        path = realloc(path, capacity);
    }
    memcpy(path, parser->previous.lexeme, parser->previous.lexeme_length);
    path_len = parser->previous.lexeme_length;
    path[path_len] = '\0';
    
    // Parse remaining parts (dots or slashes)
    while (match(parser, TOKEN_DOT) || match(parser, TOKEN_SLASH)) {
        char separator = parser->previous.type == TOKEN_DOT ? '.' : '/';
        consume(parser, TOKEN_IDENTIFIER, separator == '.' ? "Expect identifier after '.'." : "Expect identifier after '/'.");
        
        // Resize if needed
        size_t needed = path_len + 1 + parser->previous.lexeme_length + 1;
        if (needed > capacity) {
            capacity = needed * 2;
            path = realloc(path, capacity);
        }
        
        // Add separator and next part
        path[path_len++] = separator;
        memcpy(path + path_len, parser->previous.lexeme, parser->previous.lexeme_length);
        path_len += parser->previous.lexeme_length;
        path[path_len] = '\0';
    }
    
    return path;
}

static Stmt* import_declaration(Parser* parser)
{
    // Parse different import syntaxes:
    // import sys.native.io
    // import sys.native.io as io
    // import { readFile, writeFile } from sys.native.fs
    // import * as fs from sys.native.fs
    
    Stmt* stmt = NULL;
    
    // Check for import with braces (named imports)
    if (match(parser, TOKEN_LEFT_BRACE))
    {
        // import { foo, bar } from sys.module
        size_t capacity = 4;
        size_t count = 0;
        ImportSpecifier* specifiers = malloc(capacity * sizeof(ImportSpecifier));
        
        do
        {
            if (count >= capacity)
            {
                capacity *= 2;
                specifiers = realloc(specifiers, capacity * sizeof(ImportSpecifier));
            }
            
            consume(parser, TOKEN_IDENTIFIER, "Expect import specifier name.");
            char* name = malloc(parser->previous.lexeme_length + 1);
            memcpy(name, parser->previous.lexeme, parser->previous.lexeme_length);
            name[parser->previous.lexeme_length] = '\0';
            
            char* alias = NULL;
            if (match(parser, TOKEN_AS))
            {
                consume(parser, TOKEN_IDENTIFIER, "Expect alias name after 'as'.");
                alias = malloc(parser->previous.lexeme_length + 1);
                memcpy(alias, parser->previous.lexeme, parser->previous.lexeme_length);
                alias[parser->previous.lexeme_length] = '\0';
            }
            
            specifiers[count].name = name;
            specifiers[count].alias = alias;
            count++;
        }
        while (match(parser, TOKEN_COMMA));
        
        consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after import specifiers.");
        consume(parser, TOKEN_FROM, "Expect 'from' after import specifiers.");
        
        bool is_local = false;
        bool is_native = false;
        char* module_path = parse_import_path(parser, &is_local, &is_native);
        
        stmt = stmt_create_import(IMPORT_SPECIFIC, module_path);
        stmt->import_decl.specifiers = specifiers;
        stmt->import_decl.specifier_count = count;
        stmt->import_decl.is_local = is_local;
        stmt->import_decl.is_native = is_native;
        free(module_path);
    }
    else if (match(parser, TOKEN_STAR))
    {
        // import * from module or import * as namespace from module
        if (match(parser, TOKEN_FROM)) {
            // import * from module (imports all exports into current scope)
            bool is_local = false;
            bool is_native = false;
            char* module_path = parse_import_path(parser, &is_local, &is_native);
            
            stmt = stmt_create_import(IMPORT_ALL, module_path);
            stmt->import_decl.is_local = is_local;
            stmt->import_decl.is_native = is_native;
            // No alias means import all exports into current scope
            stmt->import_decl.alias = NULL;
            free(module_path);
        } else {
            // import * as namespace from module
            consume(parser, TOKEN_AS, "Expect 'as' or 'from' after '*'.");
            consume(parser, TOKEN_IDENTIFIER, "Expect namespace name.");
            
            char* namespace_alias = malloc(parser->previous.lexeme_length + 1);
            memcpy(namespace_alias, parser->previous.lexeme, parser->previous.lexeme_length);
            namespace_alias[parser->previous.lexeme_length] = '\0';
            
            consume(parser, TOKEN_FROM, "Expect 'from' after namespace alias.");
            
            bool is_local = false;
            bool is_native = false;
            char* module_path = parse_import_path(parser, &is_local, &is_native);
            
            stmt = stmt_create_import(IMPORT_NAMESPACE, module_path);
            stmt->import_decl.namespace_alias = namespace_alias;
            stmt->import_decl.is_local = is_local;
            stmt->import_decl.is_native = is_native;
            free(module_path);
        }
    }
    else if (check(parser, TOKEN_IDENTIFIER) || check(parser, TOKEN_AT) || check(parser, TOKEN_DOLLAR))
    {
        // import module or import module as alias
        // or import @module (local) or import $native
        bool is_local = false;
        bool is_native = false;
        char* module_path = parse_import_path(parser, &is_local, &is_native);
        
        char* alias = NULL;
        if (match(parser, TOKEN_AS))
        {
            consume(parser, TOKEN_IDENTIFIER, "Expect alias after 'as'.");
            alias = malloc(parser->previous.lexeme_length + 1);
            memcpy(alias, parser->previous.lexeme, parser->previous.lexeme_length);
            alias[parser->previous.lexeme_length] = '\0';
        }
        else if (is_local && module_path[0] == '@')
        {
            // For @module imports without explicit alias, use module name as alias
            // This makes `import @math` equivalent to `import @math as math`
            const char* module_name = module_path + 1;  // Skip @
            alias = strdup(module_name);
        }
        
        stmt = stmt_create_import(IMPORT_ALL, module_path);
        stmt->import_decl.alias = alias;
        stmt->import_decl.is_local = is_local;
        stmt->import_decl.is_native = is_native;
        
        free(module_path);
    }
    else
    {
        parser_error_at_current(parser, "Invalid import syntax.");
        return NULL;
    }
    
    optional_semicolon(parser);
    return stmt;
}

static Stmt* export_declaration(Parser* parser)
{
    // Parse different export syntaxes:
    // export default foo
    // export { foo, bar }
    // export { foo as f, bar }
    // export * from "module"
    // export function foo() {}
    
    Stmt* stmt = NULL;
    
    if (match(parser, TOKEN_DEFAULT))
    {
        // export default expression or function
        if (match(parser, TOKEN_FUNC))
        {
            // export default function
            Stmt* func_stmt = function_declaration(parser);
            stmt = stmt_create_export(EXPORT_DEFAULT);
            stmt->export_decl.default_export.name = func_stmt->function.name;
            free(func_stmt);
        }
        else
        {
            // export default expression
            Expr* expr = expression(parser);
            stmt = stmt_create_export(EXPORT_DEFAULT);
            stmt->export_decl.default_export.name = "<default>";
            // TODO: Store the expression for compilation
            (void)expr; // Suppress unused variable warning
        }
    }
    else if (match(parser, TOKEN_STAR))
    {
        // export * from "module"
        consume(parser, TOKEN_FROM, "Expect 'from' after '*'.");
        consume(parser, TOKEN_STRING, "Expect module path string.");
        
        char* module_path = strdup(parser->previous.literal.string_value);
        
        stmt = stmt_create_export(EXPORT_ALL);
        stmt->export_decl.all_export.from_module = module_path;
    }
    else if (match(parser, TOKEN_LEFT_BRACE))
    {
        // export { foo, bar } [from "module"]
        size_t capacity = 4;
        size_t count = 0;
        ImportSpecifier* specifiers = malloc(capacity * sizeof(ImportSpecifier));
        
        do
        {
            if (count >= capacity)
            {
                capacity *= 2;
                specifiers = realloc(specifiers, capacity * sizeof(ImportSpecifier));
            }
            
            consume(parser, TOKEN_IDENTIFIER, "Expect export specifier name.");
            char* name = malloc(parser->previous.lexeme_length + 1);
            memcpy(name, parser->previous.lexeme, parser->previous.lexeme_length);
            name[parser->previous.lexeme_length] = '\0';
            
            char* alias = NULL;
            if (match(parser, TOKEN_AS))
            {
                consume(parser, TOKEN_IDENTIFIER, "Expect alias name after 'as'.");
                alias = malloc(parser->previous.lexeme_length + 1);
                memcpy(alias, parser->previous.lexeme, parser->previous.lexeme_length);
                alias[parser->previous.lexeme_length] = '\0';
            }
            
            specifiers[count].name = name;
            specifiers[count].alias = alias;
            count++;
        }
        while (match(parser, TOKEN_COMMA));
        
        consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after export specifiers.");
        
        char* from_module = NULL;
        if (match(parser, TOKEN_FROM))
        {
            consume(parser, TOKEN_STRING, "Expect module path string.");
            from_module = strdup(parser->previous.literal.string_value);
        }
        
        stmt = stmt_create_export(EXPORT_NAMED);
        stmt->export_decl.named_export.specifiers = specifiers;
        stmt->export_decl.named_export.specifier_count = count;
        stmt->export_decl.named_export.from_module = from_module;
    }
    else if (check(parser, TOKEN_FUNC) || check(parser, TOKEN_VAR) || 
             check(parser, TOKEN_LET) || check(parser, TOKEN_CLASS))
    {
        // export declaration
        Stmt* decl_stmt = declaration(parser);
        
        stmt = stmt_create_export(EXPORT_DECLARATION);
        // Store the declaration - convert statement to declaration
        // For now, we'll pass the statement as a declaration (ugly but works)
        stmt->export_decl.decl_export.declaration = (Decl*)decl_stmt;
    }
    else
    {
        parser_error_at_current(parser, "Invalid export syntax.");
        return NULL;
    }
    
    optional_semicolon(parser);
    return stmt;
}

static Stmt* module_declaration(Parser* parser)
{
    // Parse module declaration:
    // mod com.example.utils { ... }
    // export mod utils { ... }
    
    bool is_exported = false;
    
    // Check if we already consumed TOKEN_MOD, or if it's after export
    if (!check(parser, TOKEN_IDENTIFIER)) {
        parser_error_at_current(parser, "Expect module name after 'mod'.");
        return NULL;
    }
    
    // Parse module path
    char* module_name = parse_module_path(parser);
    
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before module body.");
    
    size_t capacity = 16;
    size_t count = 0;
    Decl** declarations = malloc(capacity * sizeof(Decl*));
    
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF))
    {
        if (count >= capacity)
        {
            capacity *= 2;
            declarations = realloc(declarations, capacity * sizeof(Decl*));
        }
        
        // Parse declarations inside module
        Stmt* stmt = declaration(parser);
        if (stmt && (stmt->type == STMT_FUNCTION || stmt->type == STMT_CLASS || 
                    stmt->type == STMT_STRUCT || stmt->type == STMT_VAR))
        {
            // Convert statement to declaration
            Decl* decl = malloc(sizeof(Decl));
            if (stmt->type == STMT_FUNCTION) {
                decl->type = DECL_FUNCTION;
                decl->function = stmt->function;
            } else if (stmt->type == STMT_CLASS) {
                decl->type = DECL_CLASS;
                decl->class_decl = stmt->class_decl;
            } else if (stmt->type == STMT_STRUCT) {
                decl->type = DECL_STRUCT;
                decl->struct_decl = stmt->struct_decl;
            }
            declarations[count++] = decl;
            free(stmt);
        }
        
        if (parser->panic_mode)
        {
            synchronize(parser);
        }
    }
    
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after module body.");
    
    // Create module declaration
    Stmt* stmt = malloc(sizeof(Stmt));
    stmt->type = STMT_MODULE;
    stmt->module_decl.name = module_name;
    stmt->module_decl.declarations = declarations;
    stmt->module_decl.decl_count = count;
    stmt->module_decl.is_exported = is_exported;
    
    return stmt;
}

static Stmt* struct_declaration(Parser* parser)
{
    consume(parser, TOKEN_IDENTIFIER, "Expect struct name.");
    char* name = malloc(parser->previous.lexeme_length + 1);
    memcpy(name, parser->previous.lexeme, parser->previous.lexeme_length);
    name[parser->previous.lexeme_length] = '\0';
    
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before struct body.");
    
    size_t capacity = 8;
    size_t count = 0;
    Stmt** members = malloc(capacity * sizeof(Stmt*));
    
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF))
    {
        if (count >= capacity)
        {
            capacity *= 2;
            members = realloc(members, capacity * sizeof(Stmt*));
        }
        
        // Parse member variable only (structs are data-only)
        if (match(parser, TOKEN_VAR) || match(parser, TOKEN_LET))
        {
            members[count++] = var_statement(parser);
        }
        else
        {
            parser_error_at_current(parser, "Only variable declarations are allowed in structs.");
            synchronize(parser);
        }
    }
    
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after struct body.");
    
    // Create a proper struct declaration statement
    Stmt* stmt = malloc(sizeof(Stmt));
    stmt->type = STMT_STRUCT;
    stmt->struct_decl.name = name;
    stmt->struct_decl.members = members;
    stmt->struct_decl.member_count = count;
    
    return stmt;
}

static Stmt* protocol_declaration(Parser* parser)
{
    consume(parser, TOKEN_IDENTIFIER, "Expect protocol name.");
    char* name = malloc(parser->previous.lexeme_length + 1);
    memcpy(name, parser->previous.lexeme, parser->previous.lexeme_length);
    name[parser->previous.lexeme_length] = '\0';
    
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before protocol body.");
    
    size_t capacity = 8;
    size_t count = 0;
    Stmt** requirements = malloc(capacity * sizeof(Stmt*));
    
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF))
    {
        if (count >= capacity)
        {
            capacity *= 2;
            requirements = realloc(requirements, capacity * sizeof(Stmt*));
        }
        
        // Parse protocol requirements (function signatures, property requirements)
        if (match(parser, TOKEN_FUNC))
        {
            // Parse function requirement
            consume(parser, TOKEN_IDENTIFIER, "Expect function name.");
            char* func_name = malloc(parser->previous.lexeme_length + 1);
            memcpy(func_name, parser->previous.lexeme, parser->previous.lexeme_length);
            func_name[parser->previous.lexeme_length] = '\0';
            
            consume(parser, TOKEN_LEFT_PAREN, "Expect '(' after function name.");
            
            // Parse parameters
            size_t param_capacity = 4;
            size_t param_count = 0;
            const char** param_names = malloc(param_capacity * sizeof(char*));
            const char** param_types = malloc(param_capacity * sizeof(char*));
            
            if (!check(parser, TOKEN_RIGHT_PAREN))
            {
                do
                {
                    if (param_count >= param_capacity)
                    {
                        param_capacity *= 2;
                        param_names = realloc(param_names, param_capacity * sizeof(char*));
                        param_types = realloc(param_types, param_capacity * sizeof(char*));
                    }
                    
                    consume(parser, TOKEN_IDENTIFIER, "Expect parameter name.");
                    char* param_name = malloc(parser->previous.lexeme_length + 1);
                    memcpy(param_name, parser->previous.lexeme, parser->previous.lexeme_length);
                    param_name[parser->previous.lexeme_length] = '\0';
                    param_names[param_count] = param_name;
                    
                    consume(parser, TOKEN_COLON, "Expect ':' after parameter name.");
                    consume(parser, TOKEN_IDENTIFIER, "Expect parameter type.");
                    char* param_type = malloc(parser->previous.lexeme_length + 1);
                    memcpy(param_type, parser->previous.lexeme, parser->previous.lexeme_length);
                    param_type[parser->previous.lexeme_length] = '\0';
                    param_types[param_count] = param_type;
                    
                    param_count++;
                }
                while (match(parser, TOKEN_COMMA));
            }
            
            consume(parser, TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
            
            const char* return_type = NULL;
            if (match(parser, TOKEN_ARROW))
            {
                consume(parser, TOKEN_IDENTIFIER, "Expect return type.");
                return_type = malloc(parser->previous.lexeme_length + 1);
                memcpy((char*)return_type, parser->previous.lexeme, parser->previous.lexeme_length);
                ((char*)return_type)[parser->previous.lexeme_length] = '\0';
            }
            
            // Create a function declaration without body (protocol requirement)
            requirements[count++] = stmt_create_function(func_name, param_names, param_types, 
                                                       param_count, return_type, NULL);
            
            free(func_name);
        }
        else if (match(parser, TOKEN_VAR) || match(parser, TOKEN_LET))
        {
            // Parse property requirement
            bool is_mutable = parser->previous.type == TOKEN_VAR;
            
            consume(parser, TOKEN_IDENTIFIER, "Expect property name.");
            char* prop_name = malloc(parser->previous.lexeme_length + 1);
            memcpy(prop_name, parser->previous.lexeme, parser->previous.lexeme_length);
            prop_name[parser->previous.lexeme_length] = '\0';
            
            consume(parser, TOKEN_COLON, "Expect ':' after property name.");
            consume(parser, TOKEN_IDENTIFIER, "Expect property type.");
            char* prop_type = malloc(parser->previous.lexeme_length + 1);
            memcpy(prop_type, parser->previous.lexeme, parser->previous.lexeme_length);
            prop_type[parser->previous.lexeme_length] = '\0';
            
            requirements[count++] = stmt_create_var_decl(is_mutable, prop_name, prop_type, NULL);
            
            free(prop_name);
            free(prop_type);
        }
        else
        {
            parser_error_at_current(parser, "Expect function or property requirement in protocol.");
            synchronize(parser);
        }
        
        optional_semicolon(parser);
    }
    
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after protocol body.");
    
    // For now, create a placeholder statement
    // In a full implementation, we'd need to add protocol support to the AST
    Stmt* stmt = malloc(sizeof(Stmt));
    stmt->type = STMT_EXPRESSION;
    stmt->expression.expression = expr_create_literal_nil();
    
    free(name);
    free(requirements);
    
    return stmt;
}

static Stmt* extension_declaration(Parser* parser)
{
    // extension TypeName { methods }
    consume(parser, TOKEN_IDENTIFIER, "Expect type name after 'extension'.");
    char* type_name = malloc(parser->previous.lexeme_length + 1);
    memcpy(type_name, parser->previous.lexeme, parser->previous.lexeme_length);
    type_name[parser->previous.lexeme_length] = '\0';
    
    // Optional protocol conformance
    char* protocol_name = NULL;
    if (match(parser, TOKEN_COLON))
    {
        consume(parser, TOKEN_IDENTIFIER, "Expect protocol name after ':'.");
        protocol_name = malloc(parser->previous.lexeme_length + 1);
        memcpy(protocol_name, parser->previous.lexeme, parser->previous.lexeme_length);
        protocol_name[parser->previous.lexeme_length] = '\0';
    }
    
    consume(parser, TOKEN_LEFT_BRACE, "Expect '{' before extension body.");
    
    size_t capacity = 8;
    size_t count = 0;
    Stmt** methods = malloc(capacity * sizeof(Stmt*));
    
    while (!check(parser, TOKEN_RIGHT_BRACE) && !check(parser, TOKEN_EOF))
    {
        if (count >= capacity)
        {
            capacity *= 2;
            methods = realloc(methods, capacity * sizeof(Stmt*));
        }
        
        // Only allow function declarations in extensions
        if (match(parser, TOKEN_FUNC))
        {
            methods[count++] = function_declaration(parser);
        }
        else
        {
            parser_error_at_current(parser, "Only method declarations are allowed in extensions.");
            synchronize(parser);
        }
    }
    
    consume(parser, TOKEN_RIGHT_BRACE, "Expect '}' after extension body.");
    
    // Create extension methods by adding them to the type's prototype
    // For now, we'll create a block statement containing special function declarations
    // that will be processed by the compiler to add to the appropriate prototype
    
    // Create a special statement to represent the extension
    Stmt* extension_stmt = malloc(sizeof(Stmt));
    extension_stmt->type = STMT_BLOCK;
    extension_stmt->block.statements = methods;
    extension_stmt->block.statement_count = count;
    
    // We'll need to modify the compiler to handle extension blocks specially
    // For now, let's add a comment to the first method to mark it as an extension
    if (count > 0 && methods[0]->type == STMT_FUNCTION)
    {
        // Prefix the function name with the type name to create extension methods
        for (size_t i = 0; i < count; i++)
        {
            if (methods[i]->type == STMT_FUNCTION)
            {
                const char* orig_name = methods[i]->function.name;
                size_t new_name_len = strlen(type_name) + strlen(orig_name) + 2;
                char* new_name = malloc(new_name_len);
                snprintf(new_name, new_name_len, "%s_%s", type_name, orig_name);
                methods[i]->function.name = new_name;
            }
        }
    }
    
    free(type_name);
    if (protocol_name) free(protocol_name);
    
    return extension_stmt;
}

static Stmt* declaration(Parser* parser)
{
    if (match(parser, TOKEN_IMPORT))
    {
        return import_declaration(parser);
    }
    if (match(parser, TOKEN_EXPORT))
    {
        return export_declaration(parser);
    }
    if (match(parser, TOKEN_MOD))
    {
        return module_declaration(parser);
    }
    if (match(parser, TOKEN_FUNC))
    {
        return function_declaration(parser);
    }
    if (match(parser, TOKEN_CLASS))
    {
        return class_declaration(parser);
    }
    if (match(parser, TOKEN_STRUCT))
    {
        return struct_declaration(parser);
    }
    if (match(parser, TOKEN_PROTOCOL))
    {
        return protocol_declaration(parser);
    }
    if (match(parser, TOKEN_EXTENSION))
    {
        return extension_declaration(parser);
    }
    if (match(parser, TOKEN_VAR) || match(parser, TOKEN_LET))
    {
        return var_statement(parser);
    }

    return statement(parser);
}

ProgramNode* parser_parse_program(Parser* parser)
{
    const char* module_name = NULL;
    
    // Check for file-level module declaration
    if (match(parser, TOKEN_MOD)) {
        module_name = parse_module_path(parser);
        optional_semicolon(parser);
    }
    
    size_t capacity = 16;
    size_t count = 0;
    Stmt** statements = malloc(capacity * sizeof(Stmt*));

    while (!check(parser, TOKEN_EOF))
    {
        if (count >= capacity)
        {
            capacity *= 2;
            statements = realloc(statements, capacity * sizeof(Stmt*));
        }

        Stmt* stmt = declaration(parser);
        if (stmt)
        {
            statements[count++] = stmt;
        }

        if (parser->panic_mode)
        {
            synchronize(parser);
        }
    }

    ProgramNode* program = program_create(statements, count);
    program->module_name = module_name;
    return program;
}
