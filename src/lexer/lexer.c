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
    SlangTokenType type;
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
        SLANG_MEM_FREE(alloc, lexer, sizeof(Lexer));
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
                // Block comment with nesting support
                advance(lexer); // /
                advance(lexer); // *
                
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

static Token make_token(Lexer* lexer, SlangTokenType type, size_t start, size_t length)
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

// Process escape sequences in a string and return the processed string
static char* process_escape_sequences(const char* src, size_t length) {
    Allocator* alloc = allocators_get(ALLOC_SYSTEM_PARSER);
    char* result = MEM_NEW_ARRAY(alloc, char, length + 1);
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

static Token string(Lexer* lexer)
{
    size_t start = lexer->current - 1;
    size_t string_start = lexer->current; // Start after the opening quote
    
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
            
            Token token = make_token(lexer, TOKEN_STRING_INTERP_START, start, lexer->current - start);
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
        return error_token(lexer, "Unterminated string");
    }

    // No interpolation, regular string
    size_t string_length = lexer->current - string_start;
    advance(lexer); // consume closing quote
    
    Token token = make_token(lexer, TOKEN_STRING, start, lexer->current - start);
    token.literal.string_value = process_escape_sequences(lexer->source + string_start, string_length);
    
    return token;
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
        token = make_token(lexer, TOKEN_STRING_INTERP_MID, start, lexer->current - start);
        // Keep in_string_interp true for next iteration
    }
    else if (peek(lexer) == '"')
    {
        // End of string
        advance(lexer); // consume closing quote
        token = make_token(lexer, TOKEN_STRING_INTERP_END, start, lexer->current - start);
        lexer->in_string_interp = false;
        lexer->interp_brace_depth = 0;
    }
    else
    {
        return error_token(lexer, "Unterminated string");
    }

    // Even if string_length is 0 (empty string between interpolations), process it
    token.literal.string_value = process_escape_sequences(lexer->source + string_start, string_length);

    return token;
}

static Token character(Lexer* lexer)
{
    size_t start = lexer->current - 1;

    if (lexer_is_at_end(lexer))
    {
        return error_token(lexer, "Unterminated character literal");
    }

    char c;
    if (peek(lexer) == '\\')
    {
        advance(lexer);
        if (lexer_is_at_end(lexer))
        {
            return error_token(lexer, "Unterminated character literal");
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
            return error_token(lexer, "Invalid escape sequence");
        }
    }
    else
    {
        c = advance(lexer);
    }

    if (peek(lexer) != '\'')
    {
        return error_token(lexer, "Unterminated character literal");
    }
    advance(lexer);

    Token token = make_token(lexer, TOKEN_CHARACTER, start, lexer->current - start);
    token.literal.character_value = c;

    return token;
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

    Token token = make_token(lexer, is_float ? TOKEN_FLOAT : TOKEN_INTEGER, start, lexer->current - start);
    
    // Parse the numeric value
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
                return make_token(lexer, TOKEN_DOLLAR, start, 2);
            }
            else if (isalpha(peek(lexer)))
            {
                // Simple $identifier - return just the identifier part
                size_t ident_start = lexer->current;
                while (isalnum(peek(lexer)) || peek(lexer) == '_')
                {
                    advance(lexer);
                }
                Token token = make_token(lexer, TOKEN_IDENTIFIER, ident_start, lexer->current - ident_start);
                // Set the identifier value
                size_t ident_length = lexer->current - ident_start;
                Allocator* alloc = allocators_get(ALLOC_SYSTEM_STRINGS);
                token.literal.string_value = MEM_ALLOC(alloc, ident_length + 1);
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
                return error_token(lexer, "Invalid $ usage in string interpolation");
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
        return make_token(lexer, TOKEN_EOF, lexer->current, 0);
    }

    size_t start = lexer->current;
    char c = advance(lexer);

    // Handle closing brace in interpolation
    if (lexer->in_string_interp && lexer->interp_brace_depth > 0)
    {
        if (c == '{')
        {
            lexer->interp_brace_depth++;
        }
        else if (c == '}')
        {
            lexer->interp_brace_depth--;
            if (lexer->interp_brace_depth == 0)
            {
                // Mark that we need to continue the string after this
                lexer->just_closed_interp = true;
            }
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
    case '+': 
        if (match(lexer, '+')) return make_token(lexer, TOKEN_PLUS_PLUS, start, 2);
        return make_token(lexer, match(lexer, '=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS, start, lexer->current - start);
    case '-': 
        if (match(lexer, '>')) return make_token(lexer, TOKEN_ARROW, start, 2);
        if (match(lexer, '-')) return make_token(lexer, TOKEN_MINUS_MINUS, start, 2);
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
    case '"': return string(lexer);
    case '\'': return character(lexer);
    case '@': return make_token(lexer, TOKEN_AT, start, 1);
    case '$': return make_token(lexer, TOKEN_DOLLAR, start, 1);
    }

    return error_token(lexer, "Unexpected character");
}

// Note: Token strings are not duplicated by the lexer
// They point into the source string which must remain valid
// If tokens need to outlive the source, the parser should duplicate them