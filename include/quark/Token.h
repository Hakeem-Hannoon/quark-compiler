#pragma once

#include <string>

enum class TokenType {
    LET, IDENTIFIER, INTEGER,

    // `true` or `false`; like INTEGER, the token's value says which.
    BOOLEAN,

    EQUAL, PLUS, MINUS, STAR, SLASH,

    // Comparison and logic. Several are two characters long and begin like a
    // shorter token (`=` and `==`, `<` and `<=`), so the lexer has to look one
    // character ahead before deciding.
    EQUAL_EQUAL, BANG_EQUAL,
    LESS, LESS_EQUAL, GREATER, GREATER_EQUAL,
    BANG, AND_AND, OR_OR,

    LPAREN, RPAREN, COMMA, SEMICOLON,

    // Text the lexer could not make sense of. It has already reported the
    // problem; the token marks the spot so the parser can step around it
    // without complaining a second time.
    INVALID,

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
