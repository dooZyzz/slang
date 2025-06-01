#include "lexer/lexer.h"
#include "utils/allocators.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include "debug/debug.h"
#include "utils/logger.h"

typedef struct
{
    const char* keyword;
    TokenType type;
} Keyword;

static const Keyword keywords[] = {
    {"var", TOKEN_VAR},
    {"let", TOKEN_LET},
    {"func", TOKEN_FUNC},
    {"class", TOKEN_CLASS},
    {"struct", TOKEN_STRUCT},
    {"protocol", TOKEN_PROTOCOL},
    {"extension", TOKEN_EXTENSION},
    {"enum", TOKEN_ENUM},
    {"if", TOKEN_IF},
    {"else", TOKEN_ELSE},
    {"switch", TOKEN_SWITCH},
    {"case", TOKEN_CASE},
    {"default", TOKEN_DEFAULT},
    {"for", TOKEN_FOR},
    {"in", TOKEN_IN},
    {"while", TOKEN_WHILE},
    {"do", TOKEN_DO},
    {"break", TOKEN_BREAK},
    {"continue", TOKEN_CONTINUE},
    {"return", TOKEN_RETURN},
    {"guard", TOKEN_GUARD},
    {"defer", TOKEN_DEFER},
    {"try", TOKEN_TRY},
    {"catch", TOKEN_CATCH},
    {"throw", TOKEN_THROW},
    {"throws", TOKEN_THROWS},
    {"import", TOKEN_IMPORT},
    {"export", TOKEN_EXPORT},
    {"from", TOKEN_FROM},
    {"mod", TOKEN_MOD},
    {"public", TOKEN_PUBLIC},
    {"private", TOKEN_PRIVATE},
    {"internal", TOKEN_INTERNAL},
    {"static", TOKEN_STATIC},
    {"self", TOKEN_SELF},
    {"super", TOKEN_SUPER},
    {"init", TOKEN_INIT},
    {"deinit", TOKEN_DEINIT},
    {"as", TOKEN_AS},
    {"is", TOKEN_IS},
    {"typealias", TOKEN_TYPEALIAS},
    {"true", TOKEN_TRUE},
    {"false", TOKEN_FALSE},
    {"nil", TOKEN_NIL},
    {NULL, 0}
};

Lexer* lexer_create(const char* source)
{
    LOG_DEBUG(LOG_MODULE_LEXER, "Creating lexer with source length: %zu", strlen(source));
    
    // Lexer uses parser allocator - temporary lifetime
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    Lexer* lexer = MEM_NEW(alloc, Lexer);
    if (!lexer) {
        LOG_ERROR(LOG_MODULE_LEXER, "Failed to allocate memory for lexer");
        return NULL;
    }

    lexer->source = source;
    lexer->source_length = strlen(source);
    lexer->current = 0;
    lexer->line = 1;
    lexer->column = 1;
    lexer->line_start = 0;
    lexer->in_string_interp = false;
    lexer->interp_brace_depth = 0;
    lexer->just_closed_interp = false;

    LOG_TRACE(LOG_MODULE_LEXER, "Lexer created successfully at %p", (void*)lexer);
    return lexer;
}

void lexer_destroy(Lexer* lexer)
{
    if (lexer) {
        Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
        MEM_FREE(alloc, lexer, sizeof(Lexer));
    }
}

bool lexer_is_at_end(const Lexer* lexer)
{
    return lexer->current >= lexer->source_length;
}

static char peek(const Lexer* lexer)
{
    if (lexer_is_at_end(lexer)) return '\0';
    return lexer->source[lexer->current];
}

static char peek_next(const Lexer* lexer)
{
    if (lexer->current + 1 >= lexer->source_length) return '\0';
    return lexer->source[lexer->current + 1];
}

static char advance(Lexer* lexer)
{
    if (lexer_is_at_end(lexer)) return '\0';

    char c = lexer->source[lexer->current++];
    if (c == '\n')
    {
        lexer->line++;
        lexer->line_start = lexer->current;
        lexer->column = 1;
    }
    else
    {
        lexer->column++;
    }
    return c;
}

static bool match(Lexer* lexer, char expected)
{
    if (lexer_is_at_end(lexer)) return false;
    if (lexer->source[lexer->current] != expected) return false;
    advance(lexer);
    return true;
}

static void skip_whitespace(Lexer* lexer)
{
    while (!lexer_is_at_end(lexer))
    {
        char c = peek(lexer);
        switch (c)
        {
        case ' ':
        case '\r':
        case '\t':
        case '\n':
            advance(lexer);
            break;
        case '/':
            if (peek_next(lexer) == '/')
            {
                // Comment until end of line
                while (peek(lexer) != '\n' && !lexer_is_at_end(lexer))
                {
                    advance(lexer);
                }
            }
            else if (peek_next(lexer) == '*')
            {
                // Block comment
                advance(lexer); // /
                advance(lexer); // *
                
                while (!lexer_is_at_end(lexer))
                {
                    if (peek(lexer) == '*' && peek_next(lexer) == '/')
                    {
                        advance(lexer); // *
                        advance(lexer); // /
                        break;
                    }
                    advance(lexer);
                }
            }
            else
            {
                return;
            }
            break;
        default:
            return;
        }
    }
}

static Token make_token(Lexer* lexer, TokenType type, size_t start, size_t length)
{
    LOG_TRACE(LOG_MODULE_LEXER, "Creating token type %d at line %zu, col %zu", 
              type, lexer->line, start - lexer->line_start + 1);
    
    Token token;
    token.type = type;
    token.lexeme = &lexer->source[start];
    token.lexeme_length = length;
    token.line = lexer->line;
    token.column = start - lexer->line_start + 1;
    return token;
}

static Token error_token(Lexer* lexer, const char* message)
{
    LOG_ERROR(LOG_MODULE_LEXER, "Lexer error at line %zu: %s", lexer->line, message);
    
    Token token;
    token.type = TOKEN_ERROR;
    token.lexeme = message;
    token.lexeme_length = strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

static Token string(Lexer* lexer)
{
    size_t start = lexer->current - 1;
    
    while (peek(lexer) != '"' && !lexer_is_at_end(lexer))
    {
        if (peek(lexer) == '\\')
        {
            advance(lexer); // Skip escape character
            if (!lexer_is_at_end(lexer))
            {
                advance(lexer); // Skip escaped character
            }
        }
        else if (peek(lexer) == '\n')
        {
            return error_token(lexer, "Unterminated string");
        }
        else
        {
            advance(lexer);
        }
    }

    if (lexer_is_at_end(lexer))
    {
        return error_token(lexer, "Unterminated string");
    }

    advance(lexer); // Closing "
    return make_token(lexer, TOKEN_STRING, start, lexer->current - start);
}

static Token string_interpolation_start(Lexer* lexer)
{
    size_t start = lexer->current - 1;
    
    // Read string content until \(
    while (!lexer_is_at_end(lexer))
    {
        if (peek(lexer) == '\\' && peek_next(lexer) == '(')
        {
            advance(lexer); // Skip backslash
            advance(lexer); // Skip opening paren
            lexer->in_string_interp = true;
            lexer->interp_brace_depth = 0;
            return make_token(lexer, TOKEN_STRING_INTERP_START, start, lexer->current - start);
        }
        else if (peek(lexer) == '\\')
        {
            advance(lexer);
            if (!lexer_is_at_end(lexer))
            {
                advance(lexer);
            }
        }
        else if (peek(lexer) == '"')
        {
            // Regular string without interpolation
            advance(lexer);
            return make_token(lexer, TOKEN_STRING, start, lexer->current - start);
        }
        else if (peek(lexer) == '\n')
        {
            return error_token(lexer, "Unterminated string");
        }
        else
        {
            advance(lexer);
        }
    }
    
    return error_token(lexer, "Unterminated string");
}

static Token continue_string_interpolation(Lexer* lexer)
{
    size_t start = lexer->current;
    
    // Read string content until \( or end
    while (!lexer_is_at_end(lexer))
    {
        if (peek(lexer) == '\\' && peek_next(lexer) == '(')
        {
            advance(lexer); // Skip backslash
            advance(lexer); // Skip opening paren
            lexer->interp_brace_depth = 0;
            return make_token(lexer, TOKEN_STRING_INTERP_MID, start, lexer->current - start);
        }
        else if (peek(lexer) == '\\')
        {
            advance(lexer);
            if (!lexer_is_at_end(lexer))
            {
                advance(lexer);
            }
        }
        else if (peek(lexer) == '"')
        {
            advance(lexer);
            lexer->in_string_interp = false;
            return make_token(lexer, TOKEN_STRING_INTERP_END, start, lexer->current - start);
        }
        else if (peek(lexer) == '\n')
        {
            return error_token(lexer, "Unterminated string");
        }
        else
        {
            advance(lexer);
        }
    }
    
    return error_token(lexer, "Unterminated string");
}

static Token number(Lexer* lexer)
{
    size_t start = lexer->current - 1;
    bool is_float = false;

    while (isdigit(peek(lexer)))
    {
        advance(lexer);
    }

    if (peek(lexer) == '.' && isdigit(peek_next(lexer)))
    {
        is_float = true;
        advance(lexer); // Consume '.'
        while (isdigit(peek(lexer)))
        {
            advance(lexer);
        }
    }

    if (peek(lexer) == 'e' || peek(lexer) == 'E')
    {
        is_float = true;
        advance(lexer);
        if (peek(lexer) == '+' || peek(lexer) == '-')
        {
            advance(lexer);
        }
        while (isdigit(peek(lexer)))
        {
            advance(lexer);
        }
    }

    return make_token(lexer, is_float ? TOKEN_FLOAT : TOKEN_INTEGER, start, lexer->current - start);
}

static Token identifier(Lexer* lexer)
{
    size_t start = lexer->current - 1;

    while (isalnum(peek(lexer)) || peek(lexer) == '_')
    {
        advance(lexer);
    }

    size_t length = lexer->current - start;
    const char* text = &lexer->source[start];

    // Check if it's a keyword
    for (const Keyword* kw = keywords; kw->keyword != NULL; kw++)
    {
        size_t kw_len = strlen(kw->keyword);
        if (kw_len == length && memcmp(text, kw->keyword, length) == 0)
        {
            return make_token(lexer, kw->type, start, length);
        }
    }

    return make_token(lexer, TOKEN_IDENTIFIER, start, length);
}

Token lexer_next_token(Lexer* lexer)
{
    // Handle string interpolation continuation
    if (lexer->just_closed_interp)
    {
        lexer->just_closed_interp = false;
        return continue_string_interpolation(lexer);
    }

    skip_whitespace(lexer);

    if (lexer_is_at_end(lexer))
    {
        return make_token(lexer, TOKEN_EOF, lexer->current, 0);
    }

    size_t start = lexer->current;
    char c = advance(lexer);

    // Handle string interpolation
    if (lexer->in_string_interp)
    {
        if (c == '{')
        {
            lexer->interp_brace_depth++;
        }
        else if (c == '}')
        {
            lexer->interp_brace_depth--;
        }
        else if (c == ')' && lexer->interp_brace_depth == 0)
        {
            lexer->just_closed_interp = true;
            return make_token(lexer, TOKEN_RIGHT_PAREN, start, 1);
        }
    }

    if (isalpha(c) || c == '_')
    {
        return identifier(lexer);
    }

    if (isdigit(c))
    {
        return number(lexer);
    }

    switch (c)
    {
    case '(': return make_token(lexer, TOKEN_LEFT_PAREN, start, 1);
    case ')': return make_token(lexer, TOKEN_RIGHT_PAREN, start, 1);
    case '{': return make_token(lexer, TOKEN_LEFT_BRACE, start, 1);
    case '}': return make_token(lexer, TOKEN_RIGHT_BRACE, start, 1);
    case '[': return make_token(lexer, TOKEN_LEFT_BRACKET, start, 1);
    case ']': return make_token(lexer, TOKEN_RIGHT_BRACKET, start, 1);
    case ',': return make_token(lexer, TOKEN_COMMA, start, 1);
    case ';': return make_token(lexer, TOKEN_SEMICOLON, start, 1);
    case ':': return make_token(lexer, TOKEN_COLON, start, 1);
    case '+': return make_token(lexer, match(lexer, '=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS, start, lexer->current - start);
    case '-': 
        if (match(lexer, '>')) return make_token(lexer, TOKEN_ARROW, start, 2);
        return make_token(lexer, match(lexer, '=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS, start, lexer->current - start);
    case '*': return make_token(lexer, match(lexer, '=') ? TOKEN_STAR_EQUAL : TOKEN_STAR, start, lexer->current - start);
    case '/': return make_token(lexer, match(lexer, '=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH, start, lexer->current - start);
    case '%': return make_token(lexer, TOKEN_PERCENT, start, 1);
    case '!': return make_token(lexer, match(lexer, '=') ? TOKEN_NOT_EQUAL : TOKEN_NOT, start, lexer->current - start);
    case '=': return make_token(lexer, match(lexer, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL, start, lexer->current - start);
    case '<': 
        if (match(lexer, '<')) return make_token(lexer, TOKEN_SHIFT_LEFT, start, 2);
        return make_token(lexer, match(lexer, '=') ? TOKEN_LESS_EQUAL : TOKEN_LESS, start, lexer->current - start);
    case '>':
        if (match(lexer, '>')) return make_token(lexer, TOKEN_SHIFT_RIGHT, start, 2);
        return make_token(lexer, match(lexer, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER, start, lexer->current - start);
    case '&':
        if (match(lexer, '&')) return make_token(lexer, TOKEN_AND_AND, start, 2);
        return make_token(lexer, TOKEN_AMPERSAND, start, 1);
    case '|':
        if (match(lexer, '|')) return make_token(lexer, TOKEN_OR_OR, start, 2);
        return make_token(lexer, TOKEN_PIPE, start, 1);
    case '^': return make_token(lexer, TOKEN_CARET, start, 1);
    case '~': return make_token(lexer, TOKEN_TILDE, start, 1);
    case '?':
        if (match(lexer, '?')) return make_token(lexer, TOKEN_QUESTION_QUESTION, start, 2);
        return make_token(lexer, TOKEN_QUESTION, start, 1);
    case '.':
        if (match(lexer, '.'))
        {
            if (match(lexer, '.')) return make_token(lexer, TOKEN_DOT_DOT_DOT, start, 3);
            if (match(lexer, '<')) return make_token(lexer, TOKEN_DOT_DOT_LESS, start, 3);
            return error_token(lexer, ".. operator not supported");
        }
        return make_token(lexer, TOKEN_DOT, start, 1);
    case '"': return string_interpolation_start(lexer);
    }

    return error_token(lexer, "Unexpected character");
}

// Note: Token strings are not duplicated by the lexer
// They point into the source string which must remain valid
// If tokens need to outlive the source, the parser should duplicate them