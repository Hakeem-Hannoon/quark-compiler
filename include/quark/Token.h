#pragma once

#include <string>

enum class TokenType {
    LET, IDENTIFIER, INTEGER,
    
    EQUAL, PLUS, MINUS, STAR, SLASH,
    
    LPAREN, RPAREN, SEMICOLON,
    
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
};
