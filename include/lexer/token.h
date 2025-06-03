#ifndef TOKEN_H
#define TOKEN_H

#include <stddef.h>

typedef enum {
    // Literals
    TOKEN_INTEGER,
    TOKEN_FLOAT,
    TOKEN_STRING,
    TOKEN_CHARACTER,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NIL,
    
    // Identifiers
    TOKEN_IDENTIFIER,
    
    // Keywords
    TOKEN_VAR,
    TOKEN_LET,
    TOKEN_FUNC,
    TOKEN_CLASS,
    TOKEN_STRUCT,
    TOKEN_PROTOCOL,
    TOKEN_EXTENSION,
    TOKEN_ENUM,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_SWITCH,
    TOKEN_CASE,
    TOKEN_DEFAULT,
    TOKEN_FOR,
    TOKEN_IN,
    TOKEN_WHILE,
    TOKEN_DO,
    TOKEN_BREAK,
    TOKEN_CONTINUE,
    TOKEN_RETURN,
    TOKEN_GUARD,
    TOKEN_DEFER,
    TOKEN_TRY,
    TOKEN_CATCH,
    TOKEN_THROW,
    TOKEN_THROWS,
    TOKEN_IMPORT,
    TOKEN_EXPORT,
    TOKEN_FROM,
    TOKEN_AS_IMPORT,  // for import aliases
    TOKEN_MOD,        // for module declarations
    TOKEN_PUBLIC,
    TOKEN_PRIVATE,
    TOKEN_INTERNAL,
    TOKEN_STATIC,
    TOKEN_SELF,
    TOKEN_SUPER,
    TOKEN_INIT,
    TOKEN_DEINIT,
    TOKEN_AS,
    TOKEN_IS,
    TOKEN_TYPEALIAS,
    
    // Operators
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_PLUS_PLUS,
    TOKEN_MINUS_MINUS,
    TOKEN_EQUAL,
    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR_EQUAL,
    TOKEN_SLASH_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_LESS,
    TOKEN_GREATER,
    TOKEN_LESS_EQUAL,
    TOKEN_GREATER_EQUAL,
    TOKEN_AND_AND,
    TOKEN_OR_OR,
    TOKEN_NOT,
    TOKEN_AMPERSAND,
    TOKEN_PIPE,
    TOKEN_CARET,
    TOKEN_TILDE,
    TOKEN_SHIFT_LEFT,
    TOKEN_SHIFT_RIGHT,
    TOKEN_QUESTION,
    TOKEN_QUESTION_QUESTION,
    TOKEN_ARROW,
    TOKEN_DOT,
    TOKEN_DOT_DOT_DOT,
    TOKEN_DOT_DOT_LESS,
    TOKEN_AT,  // @ for local imports
    
    // Delimiters
    TOKEN_LEFT_PAREN,
    TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE,
    TOKEN_RIGHT_BRACE,
    TOKEN_LEFT_BRACKET,
    TOKEN_RIGHT_BRACKET,
    TOKEN_COMMA,
    TOKEN_SEMICOLON,
    TOKEN_COLON,
    
    // String interpolation
    TOKEN_STRING_INTERP_START,  // Start of interpolated string
    TOKEN_STRING_INTERP_MID,    // Middle part of interpolated string
    TOKEN_STRING_INTERP_END,    // End of interpolated string
    TOKEN_DOLLAR,               // $ in ${expr}
    TOKEN_DOLLAR_IDENT,         // $identifier
    
    // Special
    TOKEN_EOF,
    TOKEN_ERROR,
    
    TOKEN_COUNT
} SlangTokenType;

typedef struct {
    SlangTokenType type;
    const char* lexeme;
    size_t lexeme_length;
    size_t line;
    size_t column;
    
    union {
        long long integer_value;
        double float_value;
        char* string_value;
        char character_value;
    } literal;
} Token;

const char* token_type_to_string(SlangTokenType type);
void token_free(Token* token);

#endif
