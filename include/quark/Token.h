#pragma once

#include <string>

enum class TokenType {
    LET, IDENTIFIER, INTEGER,

    EQUAL, PLUS, MINUS, STAR, SLASH,

    LPAREN, RPAREN, COMMA, SEMICOLON,

    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
    int line = 1;
};

std::string tokenTypeToString(TokenType type);
