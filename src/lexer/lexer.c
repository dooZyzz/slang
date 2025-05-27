#include "lexer/lexer.h"
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
    
    Lexer* lexer = malloc(sizeof(Lexer));
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
    free(lexer);
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
                while (peek(lexer) != '\n' && !lexer_is_at_end(lexer))
                {
                    advance(lexer);
                }
            }
            else if (peek_next(lexer) == '*')
            {
                advance(lexer);
                advance(lexer);
                int depth = 1;
                while (depth > 0 && !lexer_is_at_end(lexer))
                {
                    if (peek(lexer) == '/' && peek_next(lexer) == '*')
                    {
                        advance(lexer);
                        advance(lexer);
                        depth++;
                    }
                    else if (peek(lexer) == '*' && peek_next(lexer) == '/')
                    {
                        advance(lexer);
                        advance(lexer);
                        depth--;
                    }
                    else
                    {
                        advance(lexer);
                    }
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

static Token make_token(Lexer* lexer, TokenType type, size_t start)
{
    Token token = {
        .type = type,
        .lexeme = lexer->source + start,
        .lexeme_length = lexer->current - start,
        .line = lexer->token_line,
        .column = lexer->token_column
    };
    if (debug_flags.print_tokens)
    {
        printf("[%zu:%zu] %s '%.*s'\n",
               token.line, token.column,
               token_type_to_string(type),
               (int)token.lexeme_length, token.lexeme);
    }
    return token;
}

static Token make_error_token(Lexer* lexer, const char* message)
{
    Token token = {
        .type = TOKEN_ERROR,
        .lexeme = message,
        .lexeme_length = strlen(message),
        .line = lexer->line,
        .column = lexer->column
    };
    if (debug_flags.print_tokens)
    {
        printf("[%zu:%zu] ERROR: %s\n", token.line, token.column, message);
    }
    return token;
}

static bool is_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_alnum(char c)
{
    return is_alpha(c) || isdigit(c);
}

// Process escape sequences in a string and return the processed string
static char* process_escape_sequences(const char* src, size_t length) {
    char* result = malloc(length + 1);
    if (!result) return NULL;
    
    size_t dst_index = 0;
    for (size_t i = 0; i < length; i++) {
        if (src[i] == '\\' && i + 1 < length) {
            i++; // Skip the backslash
            switch (src[i]) {
                case 'n': result[dst_index++] = '\n'; break;
                case 't': result[dst_index++] = '\t'; break;
                case 'r': result[dst_index++] = '\r'; break;
                case '\\': result[dst_index++] = '\\'; break;
                case '"': result[dst_index++] = '"'; break;
                case '\'': result[dst_index++] = '\''; break;
                case '0': result[dst_index++] = '\0'; break;
                default: 
                    // Invalid escape sequence - just include both characters
                    result[dst_index++] = '\\';
                    result[dst_index++] = src[i];
                    break;
            }
        } else {
            result[dst_index++] = src[i];
        }
    }
    result[dst_index] = '\0';
    return result;
}

static Token scan_string(Lexer* lexer)
{
    size_t start = lexer->current - 1;
    size_t string_start = lexer->current;
    
    // Scan the string to check for interpolations
    while (peek(lexer) != '"' && !lexer_is_at_end(lexer))
    {
        // Allow newlines in strings (multi-line strings)
        if (peek(lexer) == '\\')
        {
            advance(lexer);
            if (!lexer_is_at_end(lexer))
            {
                advance(lexer);
            }
        }
        else if (peek(lexer) == '$')
        {
            // Found interpolation - return the string part before it
            size_t string_length = lexer->current - string_start;
            
            Token token = make_token(lexer, TOKEN_STRING_INTERP_START, start);
            token.literal.string_value = process_escape_sequences(lexer->source + string_start, string_length);
            
            // Mark that we're in an interpolated string
            lexer->in_string_interp = true;
            lexer->interp_brace_depth = 0;
            
            return token;
        }
        else
        {
            advance(lexer);
        }
    }

    if (lexer_is_at_end(lexer))
    {
        return make_error_token(lexer, "Unterminated string");
    }

    // No interpolation, regular string
    size_t string_length = lexer->current - string_start;
    advance(lexer); // consume closing quote

    Token token = make_token(lexer, TOKEN_STRING, start);
    token.literal.string_value = process_escape_sequences(lexer->source + string_start, string_length);

    return token;
}

static Token scan_character(Lexer* lexer)
{
    size_t start = lexer->current - 1;

    if (lexer_is_at_end(lexer))
    {
        return make_error_token(lexer, "Unterminated character literal");
    }

    char c;
    if (peek(lexer) == '\\')
    {
        advance(lexer);
        if (lexer_is_at_end(lexer))
        {
            return make_error_token(lexer, "Unterminated character literal");
        }
        char escape = advance(lexer);
        switch (escape)
        {
        case 'n': c = '\n';
            break;
        case 't': c = '\t';
            break;
        case 'r': c = '\r';
            break;
        case '\\': c = '\\';
            break;
        case '\'': c = '\'';
            break;
        case '"': c = '"';
            break;
        case '0': c = '\0';
            break;
        default:
            return make_error_token(lexer, "Invalid escape sequence");
        }
    }
    else
    {
        c = advance(lexer);
    }

    if (peek(lexer) != '\'')
    {
        return make_error_token(lexer, "Unterminated character literal");
    }
    advance(lexer);

    Token token = make_token(lexer, TOKEN_CHARACTER, start);
    token.literal.character_value = c;
    return token;
}

static Token scan_number(Lexer* lexer)
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
        advance(lexer);
        while (isdigit(peek(lexer)))
        {
            advance(lexer);
        }
    }

    Token token = make_token(lexer, is_float ? TOKEN_FLOAT : TOKEN_INTEGER, start);

    if (is_float)
    {
        char* end;
        token.literal.float_value = strtod(lexer->source + start, &end);
    }
    else
    {
        char* end;
        token.literal.integer_value = strtoll(lexer->source + start, &end, 10);
    }

    return token;
}

static Token scan_identifier(Lexer* lexer)
{
    size_t start = lexer->current - 1;

    while (is_alnum(peek(lexer)))
    {
        advance(lexer);
    }

    size_t length = lexer->current - start;

    for (const Keyword* kw = keywords; kw->keyword != NULL; kw++)
    {
        if (strlen(kw->keyword) == length &&
            memcmp(lexer->source + start, kw->keyword, length) == 0)
        {
            return make_token(lexer, kw->type, start);
        }
    }

    return make_token(lexer, TOKEN_IDENTIFIER, start);
}

static Token scan_string_continuation(Lexer* lexer)
{
    size_t start = lexer->current;
    size_t string_start = lexer->current;

    // Scan until we hit another $ or the end quote
    while (peek(lexer) != '"' && !lexer_is_at_end(lexer))
    {
        // Allow newlines in strings (multi-line strings)
        if (peek(lexer) == '\\')
        {
            advance(lexer);
            if (!lexer_is_at_end(lexer))
            {
                advance(lexer);
            }
        }
        else if (peek(lexer) == '$')
        {
            break; // Found another interpolation
        }
        else
        {
            advance(lexer);
        }
    }

    size_t string_length = lexer->current - string_start;

    Token token;
    if (peek(lexer) == '$')
    {
        // More interpolation coming
        token = make_token(lexer, TOKEN_STRING_INTERP_MID, start);
        // Keep in_string_interp true for next iteration
    }
    else if (peek(lexer) == '"')
    {
        // End of string
        advance(lexer); // consume closing quote
        token = make_token(lexer, TOKEN_STRING_INTERP_END, start);
        lexer->in_string_interp = false;
        lexer->interp_brace_depth = 0;
    }
    else
    {
        return make_error_token(lexer, "Unterminated string");
    }

    // Even if string_length is 0 (empty string between interpolations), process it
    token.literal.string_value = process_escape_sequences(lexer->source + string_start, string_length);

    return token;
}

Token lexer_next_token(Lexer* lexer)
{
    // Handle string interpolation state
    if (lexer->in_string_interp)
    {
        // If we've just closed an interpolation, we need to return the string continuation
        if (lexer->just_closed_interp)
        {
            lexer->just_closed_interp = false;
            return scan_string_continuation(lexer);
        }
        
        // Check if we need to handle $ for interpolation
        if (peek(lexer) == '$')
        {
            size_t start = lexer->current;
            advance(lexer); // consume $
            
            if (peek(lexer) == '{')
            {
                advance(lexer); // consume {
                lexer->interp_brace_depth = 1;
                return make_token(lexer, TOKEN_DOLLAR, start);
            }
            else if (is_alpha(peek(lexer)))
            {
                // Simple $identifier - return just the identifier part
                size_t ident_start = lexer->current;
                while (is_alnum(peek(lexer)))
                {
                    advance(lexer);
                }
                Token token = make_token(lexer, TOKEN_IDENTIFIER, ident_start);
                // Set the identifier value
                size_t ident_length = lexer->current - ident_start;
                token.literal.string_value = malloc(ident_length + 1);
                if (token.literal.string_value) {
                    memcpy(token.literal.string_value, lexer->source + ident_start, ident_length);
                    token.literal.string_value[ident_length] = '\0';
                }
                // After parsing the identifier, mark that we need to continue the string
                lexer->just_closed_interp = true;
                return token;
            }
            else
            {
                return make_error_token(lexer, "Invalid $ usage in string interpolation");
            }
        }
        // If we're still in string interp but not processing $, continue scanning the string
        else if (lexer->interp_brace_depth == 0)
        {
            return scan_string_continuation(lexer);
        }
    }
    
    skip_whitespace(lexer);

    if (lexer_is_at_end(lexer))
    {
        Token token = {
            .type = TOKEN_EOF,
            .lexeme = lexer->source + lexer->current,
            .lexeme_length = 0,
            .line = lexer->line,
            .column = lexer->current - lexer->line_start + 1
        };
        if (debug_flags.print_tokens)
        {
            printf("[%zu:%zu] %s\n", token.line, token.column, token_type_to_string(token.type));
        }
        return token;
    }

    size_t start = lexer->current;
    lexer->token_line = lexer->line;
    lexer->token_column = lexer->current - lexer->line_start + 1;
    char c = advance(lexer);

    if (is_alpha(c))
    {
        lexer->current = start;
        advance(lexer);
        return scan_identifier(lexer);
    }

    if (isdigit(c))
    {
        lexer->current = start;
        advance(lexer);
        return scan_number(lexer);
    }

    switch (c)
    {
    case '(': return make_token(lexer, TOKEN_LEFT_PAREN, start);
    case ')': return make_token(lexer, TOKEN_RIGHT_PAREN, start);
    case '{':
        if (lexer->in_string_interp)
        {
            lexer->interp_brace_depth++;
        }
        return make_token(lexer, TOKEN_LEFT_BRACE, start);
    case '}':
        if (lexer->in_string_interp)
        {
            lexer->interp_brace_depth--;
            if (lexer->interp_brace_depth == 0)
            {
                // End of interpolation expression
                lexer->just_closed_interp = true;
                return make_token(lexer, TOKEN_RIGHT_BRACE, start);
            }
        }
        return make_token(lexer, TOKEN_RIGHT_BRACE, start);
    case '[': return make_token(lexer, TOKEN_LEFT_BRACKET, start);
    case ']': return make_token(lexer, TOKEN_RIGHT_BRACKET, start);
    case ',': return make_token(lexer, TOKEN_COMMA, start);
    case ';': return make_token(lexer, TOKEN_SEMICOLON, start);
    case ':': return make_token(lexer, TOKEN_COLON, start);
    case '~': return make_token(lexer, TOKEN_TILDE, start);
    case '^': return make_token(lexer, TOKEN_CARET, start);

    case '+':
        if (match(lexer, '+')) return make_token(lexer, TOKEN_PLUS_PLUS, start);
        return make_token(lexer, match(lexer, '=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS, start);
    case '-':
        if (match(lexer, '-')) return make_token(lexer, TOKEN_MINUS_MINUS, start);
        if (match(lexer, '>')) return make_token(lexer, TOKEN_ARROW, start);
        return make_token(lexer, match(lexer, '=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS, start);
    case '*':
        return make_token(lexer, match(lexer, '=') ? TOKEN_STAR_EQUAL : TOKEN_STAR, start);
    case '/':
        return make_token(lexer, match(lexer, '=') ? TOKEN_SLASH_EQUAL : TOKEN_SLASH, start);
    case '%':
        return make_token(lexer, TOKEN_PERCENT, start);
    case '!':
        return make_token(lexer, match(lexer, '=') ? TOKEN_NOT_EQUAL : TOKEN_NOT, start);
    case '=':
        return make_token(lexer, match(lexer, '=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL, start);
    case '<':
        if (match(lexer, '=')) return make_token(lexer, TOKEN_LESS_EQUAL, start);
        if (match(lexer, '<')) return make_token(lexer, TOKEN_SHIFT_LEFT, start);
        return make_token(lexer, TOKEN_LESS, start);
    case '>':
        if (match(lexer, '=')) return make_token(lexer, TOKEN_GREATER_EQUAL, start);
        if (match(lexer, '>')) return make_token(lexer, TOKEN_SHIFT_RIGHT, start);
        return make_token(lexer, TOKEN_GREATER, start);
    case '&':
        return make_token(lexer, match(lexer, '&') ? TOKEN_AND_AND : TOKEN_AMPERSAND, start);
    case '|':
        return make_token(lexer, match(lexer, '|') ? TOKEN_OR_OR : TOKEN_PIPE, start);
    case '?':
        return make_token(lexer, match(lexer, '?') ? TOKEN_QUESTION_QUESTION : TOKEN_QUESTION, start);
    case '.':
        if (match(lexer, '.'))
        {
            if (match(lexer, '.')) return make_token(lexer, TOKEN_DOT_DOT_DOT, start);
            if (match(lexer, '<')) return make_token(lexer, TOKEN_DOT_DOT_LESS, start);
            lexer->current = start + 1;
        }
        return make_token(lexer, TOKEN_DOT, start);
    case '$': return make_token(lexer, TOKEN_DOLLAR, start);
    case '"': return scan_string(lexer);
    case '\'': return scan_character(lexer);
    case '@': return make_token(lexer, TOKEN_AT, start);
    }

    return make_error_token(lexer, "Unexpected character");
}
