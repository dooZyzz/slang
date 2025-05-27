#ifndef PARSER_H
#define PARSER_H

#include "ast/ast.h"
#include "lexer/token.h"
#include "lexer/lexer.h"
#include <stdbool.h>

typedef struct {
    Lexer* lexer;
    Token current;
    Token previous;
    bool had_error;
    bool panic_mode;
} Parser;

// Parser creation and destruction
Parser* parser_create(const char* source);
void parser_destroy(Parser* parser);

// Main parsing function
ProgramNode* parser_parse_program(Parser* parser);

// Error handling
void parser_error(Parser* parser, const char* message);
void parser_error_at_current(Parser* parser, const char* message);
void parser_error_at(Parser* parser, Token* token, const char* message);

#endif
