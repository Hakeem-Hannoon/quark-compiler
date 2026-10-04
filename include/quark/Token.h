#pragma once

#include <string>

enum class TokenType {
    LET, IDENTIFIER, INTEGER,

    EQUAL, PLUS, MINUS, STAR, SLASH,

    LPAREN, RPAREN, COMMA, SEMICOLON,

    END_OF_FILE
};

// Where a token starts is kept as a 1-based line and column. Columns count
// characters rather than bytes, so they stay right after a non-ASCII comment.
struct Token {
    TokenType type;
    std::string value;
    int line = 1;
    int column = 1;
};

std::string tokenTypeToString(TokenType type);
