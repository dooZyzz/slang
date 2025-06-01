#include <stdio.h>
#include <string.h>
#include "utils/test_framework.h"
#include "utils/test_macros.h"
#include "lexer/lexer.h"
#include "lexer/token.h"

DEFINE_TEST(single_tokens) {
    const char* source = "+ - * / % = == != < > <= >= && || ! & | ^ ~ ( ) { } [ ] , ; : . ? ?? -> ... ..<";
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "single_tokens");
    
    TokenType expected[] = {
        TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
        TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_NOT_EQUAL,
        TOKEN_LESS, TOKEN_GREATER, TOKEN_LESS_EQUAL, TOKEN_GREATER_EQUAL,
        TOKEN_AND_AND, TOKEN_OR_OR, TOKEN_NOT,
        TOKEN_AMPERSAND, TOKEN_PIPE, TOKEN_CARET, TOKEN_TILDE,
        TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
        TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
        TOKEN_LEFT_BRACKET, TOKEN_RIGHT_BRACKET,
        TOKEN_COMMA, TOKEN_SEMICOLON, TOKEN_COLON,
        TOKEN_DOT, TOKEN_QUESTION, TOKEN_QUESTION_QUESTION, TOKEN_ARROW,
        TOKEN_DOT_DOT_DOT, TOKEN_DOT_DOT_LESS,
        TOKEN_EOF
    };
    
    int i = 0;
    while (!lexer_is_at_end(lexer)) {
        Token token = lexer_next_token(lexer);
        TEST_ASSERT_EQUAL_INT(suite, expected[i], token.type, "single_tokens");
        if (token.type == TOKEN_EOF) break;
        i++;
    }
    
    lexer_destroy(lexer);
}

DEFINE_TEST(keywords) {
    const char* source = "var let func class struct if else for while return true false nil";
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "keywords");
    
    TokenType expected[] = {
        TOKEN_VAR, TOKEN_LET, TOKEN_FUNC, TOKEN_CLASS, TOKEN_STRUCT,
        TOKEN_IF, TOKEN_ELSE, TOKEN_FOR, TOKEN_WHILE, TOKEN_RETURN,
        TOKEN_TRUE, TOKEN_FALSE, TOKEN_NIL,
        TOKEN_EOF
    };
    
    int i = 0;
    while (!lexer_is_at_end(lexer)) {
        Token token = lexer_next_token(lexer);
        TEST_ASSERT_EQUAL_INT(suite, expected[i], token.type, "keywords");
        if (token.type == TOKEN_EOF) break;
        i++;
    }
    
    lexer_destroy(lexer);
}

DEFINE_TEST(identifiers) {
    const char* source = "hello world123 _underscore camelCase PascalCase snake_case CONST_CASE";
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "identifiers");
    
    const char* expected[] = {
        "hello", "world123", "_underscore", "camelCase", "PascalCase", "snake_case", "CONST_CASE"
    };
    
    int i = 0;
    while (!lexer_is_at_end(lexer)) {
        Token token = lexer_next_token(lexer);
        if (token.type == TOKEN_EOF) break;
        TEST_ASSERT_EQUAL_INT(suite, TOKEN_IDENTIFIER, token.type, "identifiers");
        
        // Compare lexeme content
        char lexeme[256];
        strncpy(lexeme, token.lexeme, token.lexeme_length);
        lexeme[token.lexeme_length] = '\0';
        TEST_ASSERT_EQUAL_STR(suite, expected[i], lexeme, "identifiers");
        i++;
    }
    
    lexer_destroy(lexer);
}

DEFINE_TEST(numbers) {
    const char* source = "42 3.14 0.5 123.456 .789 0 1000000";
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "numbers");
    
    TokenType expected[] = {
        TOKEN_INTEGER, TOKEN_FLOAT, TOKEN_FLOAT, TOKEN_FLOAT,
        TOKEN_DOT, TOKEN_INTEGER,  // .789 is lexed as DOT followed by INTEGER
        TOKEN_INTEGER, TOKEN_INTEGER
    };
    
    int i = 0;
    while (!lexer_is_at_end(lexer)) {
        Token token = lexer_next_token(lexer);
        if (token.type == TOKEN_EOF) break;
        TEST_ASSERT_EQUAL_INT(suite, expected[i], token.type, "numbers");
        i++;
    }
    
    lexer_destroy(lexer);
}

DEFINE_TEST(strings) {
    const char* source = "\"hello\" \"world with spaces\" \"\" \"escaped \\\"quotes\\\"\" \"newline\\ntest\"";
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "strings");
    
    const char* expected[] = {
        "hello", "world with spaces", "", "escaped \"quotes\"", "newline\ntest"
    };
    
    int i = 0;
    while (!lexer_is_at_end(lexer)) {
        Token token = lexer_next_token(lexer);
        if (token.type == TOKEN_EOF) break;
        TEST_ASSERT_EQUAL_INT(suite, TOKEN_STRING, token.type, "strings");
        TEST_ASSERT_EQUAL_STR(suite, expected[i], token.literal.string_value, "strings");
        i++;
    }
    
    lexer_destroy(lexer);
}

DEFINE_TEST(characters) {
    const char* source = "'a' 'b' '\\n' '\\'' '\\\\' '0'";
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "characters");
    
    char expected[] = {'a', 'b', '\n', '\'', '\\', '0'};
    
    int i = 0;
    while (!lexer_is_at_end(lexer)) {
        Token token = lexer_next_token(lexer);
        if (token.type == TOKEN_EOF) break;
        TEST_ASSERT_EQUAL_INT(suite, TOKEN_CHARACTER, token.type, "characters");
        TEST_ASSERT_EQUAL_INT(suite, expected[i], token.literal.character_value, "characters");
        i++;
    }
    
    lexer_destroy(lexer);
}

DEFINE_TEST(comments) {
    const char* source = "// single line comment\n"
                        "var x = 42 // inline comment\n"
                        "/* multi\n"
                        "   line\n"
                        "   comment */\n"
                        "var y = 3.14\n"
                        "/* nested /* comments */ are */ supported";
    
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "comments");
    
    TokenType expected[] = {
        TOKEN_VAR, TOKEN_IDENTIFIER, TOKEN_EQUAL, TOKEN_INTEGER,
        TOKEN_VAR, TOKEN_IDENTIFIER, TOKEN_EQUAL, TOKEN_FLOAT,
        TOKEN_IDENTIFIER,  // "supported"
        TOKEN_EOF
    };
    
    int i = 0;
    while (!lexer_is_at_end(lexer)) {
        Token token = lexer_next_token(lexer);
        TEST_ASSERT_EQUAL_INT(suite, expected[i], token.type, "comments");
        if (token.type == TOKEN_EOF) break;
        i++;
    }
    
    lexer_destroy(lexer);
}

DEFINE_TEST(compound_operators) {
    const char* source = "+= -= *= /= ++ -- << >>";
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "compound_operators");
    
    TokenType expected[] = {
        TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL, TOKEN_STAR_EQUAL, 
        TOKEN_SLASH_EQUAL,
        TOKEN_PLUS_PLUS, TOKEN_MINUS_MINUS,
        TOKEN_SHIFT_LEFT, TOKEN_SHIFT_RIGHT,
        TOKEN_EOF
    };
    
    int i = 0;
    while (!lexer_is_at_end(lexer)) {
        Token token = lexer_next_token(lexer);
        TEST_ASSERT_EQUAL_INT(suite, expected[i], token.type, "compound_operators");
        if (token.type == TOKEN_EOF) break;
        i++;
    }
    
    lexer_destroy(lexer);
}

DEFINE_TEST(line_column_tracking) {
    const char* source = "x\ny\n\nz";
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "line_column_tracking");
    
    Token token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, 1, token.line, "line_column_tracking");
    TEST_ASSERT_EQUAL_INT(suite, 1, token.column, "line_column_tracking");
    
    token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, 2, token.line, "line_column_tracking");
    TEST_ASSERT_EQUAL_INT(suite, 1, token.column, "line_column_tracking");
    
    token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, 4, token.line, "line_column_tracking");
    TEST_ASSERT_EQUAL_INT(suite, 1, token.column, "line_column_tracking");
    
    lexer_destroy(lexer);
}

DEFINE_TEST(error_handling) {
    // Test unterminated string
    const char* source1 = "\"unterminated string";
    Lexer* lexer1 = lexer_create(source1);
    TEST_ASSERT_NOT_NULL(suite, lexer1, "error_handling");
    
    Token token1 = lexer_next_token(lexer1);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_ERROR, token1.type, "error_handling");
    
    lexer_destroy(lexer1);
    
    // Test invalid character - use a character that's not in the language
    const char* source2 = "§"; // Section sign, not a valid Swift character
    Lexer* lexer2 = lexer_create(source2);
    TEST_ASSERT_NOT_NULL(suite, lexer2, "error_handling");
    
    Token token2 = lexer_next_token(lexer2);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_ERROR, token2.type, "error_handling");
    
    lexer_destroy(lexer2);
}

DEFINE_TEST(string_interpolation) {
    const char* source = "\"Hello, $name!\" \"Value: $value\" \"Nested: ${expr + 1}\"";
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "string_interpolation");
    
    // First interpolated string: "Hello, $name!"
    Token token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_STRING_INTERP_START, token.type, "string_interpolation");
    
    token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_IDENTIFIER, token.type, "string_interpolation");
    
    token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_STRING_INTERP_END, token.type, "string_interpolation");
    
    // Continue testing other interpolated strings...
    
    lexer_destroy(lexer);
}

DEFINE_TEST(multiline_strings) {
    const char* source = "\"First line\\nSecond line\" \"Line 1\nLine 2\nLine 3\" \"Multi\\nwith\\nescape\"";
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "multiline_strings");
    
    // First string with escape sequence
    Token token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_STRING, token.type, "multiline_strings");
    TEST_ASSERT_EQUAL_STR(suite, "First line\nSecond line", token.literal.string_value, "multiline_strings");
    
    // Second string with actual newlines
    token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_STRING, token.type, "multiline_strings");
    TEST_ASSERT_EQUAL_STR(suite, "Line 1\nLine 2\nLine 3", token.literal.string_value, "multiline_strings");
    
    // Third string with escape sequences
    token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_STRING, token.type, "multiline_strings");
    TEST_ASSERT_EQUAL_STR(suite, "Multi\nwith\nescape", token.literal.string_value, "multiline_strings");
    
    lexer_destroy(lexer);
}

DEFINE_TEST(multiline_string_interpolation) {
    const char* source = "\"Hello\\n$name\\nWelcome!\" \"Line 1\n${x + y}\nLine 3\"";
    Lexer* lexer = lexer_create(source);
    TEST_ASSERT_NOT_NULL(suite, lexer, "multiline_string_interpolation");
    
    // First interpolated multi-line string
    Token token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_STRING_INTERP_START, token.type, "multiline_string_interpolation");
    TEST_ASSERT_EQUAL_STR(suite, "Hello\n", token.literal.string_value, "multiline_string_interpolation");
    
    token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_IDENTIFIER, token.type, "multiline_string_interpolation");
    
    token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_STRING_INTERP_END, token.type, "multiline_string_interpolation");
    TEST_ASSERT_EQUAL_STR(suite, "\nWelcome!", token.literal.string_value, "multiline_string_interpolation");
    
    // Second interpolated multi-line string with expression
    token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_STRING_INTERP_START, token.type, "multiline_string_interpolation");
    TEST_ASSERT_EQUAL_STR(suite, "Line 1\n", token.literal.string_value, "multiline_string_interpolation");
    
    token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_DOLLAR, token.type, "multiline_string_interpolation");
    
    // Skip through the expression tokens (x + y)
    lexer_next_token(lexer); // x
    lexer_next_token(lexer); // +
    lexer_next_token(lexer); // y
    lexer_next_token(lexer); // }
    
    token = lexer_next_token(lexer);
    TEST_ASSERT_EQUAL_INT(suite, TOKEN_STRING_INTERP_END, token.type, "multiline_string_interpolation");
    TEST_ASSERT_EQUAL_STR(suite, "\nLine 3", token.literal.string_value, "multiline_string_interpolation");
    
    lexer_destroy(lexer);
}

// Define test suite
TEST_SUITE(lexer_unit)
    TEST_CASE(single_tokens, "Single Tokens")
    TEST_CASE(keywords, "Keywords")
    TEST_CASE(identifiers, "Identifiers")
    TEST_CASE(numbers, "Numbers")
    TEST_CASE(strings, "Strings")
    TEST_CASE(characters, "Characters")
    TEST_CASE(comments, "Comments")
    TEST_CASE(compound_operators, "Compound Operators")
    TEST_CASE(line_column_tracking, "Line/Column Tracking")
    TEST_CASE(error_handling, "Error Handling")
    TEST_CASE(string_interpolation, "String Interpolation")
    TEST_CASE(multiline_strings, "Multi-line Strings")
    TEST_CASE(multiline_string_interpolation, "Multi-line String Interpolation")
END_TEST_SUITE(lexer_unit)

// Optional standalone runner
