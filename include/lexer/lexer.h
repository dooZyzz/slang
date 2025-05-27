#ifndef LEXER_H
#define LEXER_H

#include "lexer/token.h"
#include <stdbool.h>

typedef struct {
    const char* source;
    size_t source_length;
    size_t current;
    size_t line;
    size_t column;
    size_t line_start;
    size_t token_start;
    size_t token_line;
    size_t token_column;
    
    // String interpolation state
    bool in_string_interp;
    int interp_brace_depth;
    bool just_closed_interp;  // Track if we just closed an interpolation
} Lexer;

Lexer* lexer_create(const char* source);
void lexer_destroy(Lexer* lexer);
Token lexer_next_token(Lexer* lexer);
bool lexer_is_at_end(const Lexer* lexer);

#endif
