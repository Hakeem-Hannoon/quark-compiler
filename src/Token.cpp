#include "quark/Token.h"

std::string tokenTypeToString(TokenType type){
    switch (type){
        case TokenType::LET: return "LET";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::INTEGER: return "INTEGER";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::END_OF_FILE: return "EOF";
    }

    return "UNKNOWN";
}
