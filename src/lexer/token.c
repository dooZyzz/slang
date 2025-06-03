#include "lexer/token.h"
#include "utils/allocators.h"
#include <stdlib.h>
#include <string.h>

const char* token_type_to_string(SlangTokenType type) {
    static const char* token_names[] = {
        [TOKEN_INTEGER] = "INTEGER",
        [TOKEN_FLOAT] = "FLOAT",
        [TOKEN_STRING] = "STRING",
        [TOKEN_CHARACTER] = "CHARACTER",
        [TOKEN_TRUE] = "TRUE",
        [TOKEN_FALSE] = "FALSE",
        [TOKEN_NIL] = "NIL",
        [TOKEN_IDENTIFIER] = "IDENTIFIER",
        [TOKEN_VAR] = "VAR",
        [TOKEN_LET] = "LET",
        [TOKEN_FUNC] = "FUNC",
        [TOKEN_CLASS] = "CLASS",
        [TOKEN_STRUCT] = "STRUCT",
        [TOKEN_PROTOCOL] = "PROTOCOL",
        [TOKEN_EXTENSION] = "EXTENSION",
        [TOKEN_ENUM] = "ENUM",
        [TOKEN_IF] = "IF",
        [TOKEN_ELSE] = "ELSE",
        [TOKEN_SWITCH] = "SWITCH",
        [TOKEN_CASE] = "CASE",
        [TOKEN_DEFAULT] = "DEFAULT",
        [TOKEN_FOR] = "FOR",
        [TOKEN_IN] = "IN",
        [TOKEN_WHILE] = "WHILE",
        [TOKEN_DO] = "DO",
        [TOKEN_BREAK] = "BREAK",
        [TOKEN_CONTINUE] = "CONTINUE",
        [TOKEN_RETURN] = "RETURN",
        [TOKEN_GUARD] = "GUARD",
        [TOKEN_DEFER] = "DEFER",
        [TOKEN_TRY] = "TRY",
        [TOKEN_CATCH] = "CATCH",
        [TOKEN_THROW] = "THROW",
        [TOKEN_THROWS] = "THROWS",
        [TOKEN_IMPORT] = "IMPORT",
        [TOKEN_EXPORT] = "EXPORT",
        [TOKEN_FROM] = "FROM",
        [TOKEN_AS_IMPORT] = "AS_IMPORT",
        [TOKEN_MOD] = "MOD",
        [TOKEN_PUBLIC] = "PUBLIC",
        [TOKEN_PRIVATE] = "PRIVATE",
        [TOKEN_INTERNAL] = "INTERNAL",
        [TOKEN_STATIC] = "STATIC",
        [TOKEN_SELF] = "SELF",
        [TOKEN_SUPER] = "SUPER",
        [TOKEN_INIT] = "INIT",
        [TOKEN_DEINIT] = "DEINIT",
        [TOKEN_AS] = "AS",
        [TOKEN_IS] = "IS",
        [TOKEN_TYPEALIAS] = "TYPEALIAS",
        [TOKEN_PLUS] = "PLUS",
        [TOKEN_MINUS] = "MINUS",
        [TOKEN_STAR] = "STAR",
        [TOKEN_SLASH] = "SLASH",
        [TOKEN_PERCENT] = "PERCENT",
        [TOKEN_PLUS_PLUS] = "PLUS_PLUS",
        [TOKEN_MINUS_MINUS] = "MINUS_MINUS",
        [TOKEN_EQUAL] = "EQUAL",
        [TOKEN_PLUS_EQUAL] = "PLUS_EQUAL",
        [TOKEN_MINUS_EQUAL] = "MINUS_EQUAL",
        [TOKEN_STAR_EQUAL] = "STAR_EQUAL",
        [TOKEN_SLASH_EQUAL] = "SLASH_EQUAL",
        [TOKEN_EQUAL_EQUAL] = "EQUAL_EQUAL",
        [TOKEN_NOT_EQUAL] = "NOT_EQUAL",
        [TOKEN_LESS] = "LESS",
        [TOKEN_GREATER] = "GREATER",
        [TOKEN_LESS_EQUAL] = "LESS_EQUAL",
        [TOKEN_GREATER_EQUAL] = "GREATER_EQUAL",
        [TOKEN_AND_AND] = "AND_AND",
        [TOKEN_OR_OR] = "OR_OR",
        [TOKEN_NOT] = "NOT",
        [TOKEN_AMPERSAND] = "AMPERSAND",
        [TOKEN_PIPE] = "PIPE",
        [TOKEN_CARET] = "CARET",
        [TOKEN_TILDE] = "TILDE",
        [TOKEN_SHIFT_LEFT] = "SHIFT_LEFT",
        [TOKEN_SHIFT_RIGHT] = "SHIFT_RIGHT",
        [TOKEN_QUESTION] = "QUESTION",
        [TOKEN_QUESTION_QUESTION] = "QUESTION_QUESTION",
        [TOKEN_ARROW] = "ARROW",
        [TOKEN_DOT] = "DOT",
        [TOKEN_DOT_DOT_DOT] = "DOT_DOT_DOT",
        [TOKEN_DOT_DOT_LESS] = "DOT_DOT_LESS",
        [TOKEN_AT] = "AT",
        [TOKEN_LEFT_PAREN] = "LEFT_PAREN",
        [TOKEN_RIGHT_PAREN] = "RIGHT_PAREN",
        [TOKEN_LEFT_BRACE] = "LEFT_BRACE",
        [TOKEN_RIGHT_BRACE] = "RIGHT_BRACE",
        [TOKEN_LEFT_BRACKET] = "LEFT_BRACKET",
        [TOKEN_RIGHT_BRACKET] = "RIGHT_BRACKET",
        [TOKEN_COMMA] = "COMMA",
        [TOKEN_SEMICOLON] = "SEMICOLON",
        [TOKEN_COLON] = "COLON",
        [TOKEN_STRING_INTERP_START] = "STRING_INTERP_START",
        [TOKEN_STRING_INTERP_MID] = "STRING_INTERP_MID",
        [TOKEN_STRING_INTERP_END] = "STRING_INTERP_END",
        [TOKEN_DOLLAR] = "DOLLAR",
        [TOKEN_DOLLAR_IDENT] = "DOLLAR_IDENT",
        [TOKEN_EOF] = "EOF",
        [TOKEN_ERROR] = "ERROR"
    };
    
    if (type >= 0 && type < TOKEN_COUNT) {
        return token_names[type];
    }
    return "UNKNOWN";
}

void token_free(Token* token) {
    if (token && token->literal.string_value) {
        if (token->type == TOKEN_STRING || 
            token->type == TOKEN_STRING_INTERP_START ||
            token->type == TOKEN_STRING_INTERP_MID ||
            token->type == TOKEN_STRING_INTERP_END) {
            // Tokens typically use parser allocator
            Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
            SLANG_MEM_FREE(alloc, token->literal.string_value, 
                    strlen(token->literal.string_value) + 1);
            token->literal.string_value = NULL;
        }
    }
}