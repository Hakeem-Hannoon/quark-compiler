#include <fstream>
#include <iostream>  
#include <string>
#include <vector>       
#include <iterator>    
#include "Lexer.h" 

std::string tokenTypeToString(TokenType tokenType){
    switch (tokenType){
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
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::END_OF_FILE: return "EOF";
        default: return "UNKNOWN";
    }
}

int main(){
    std::ifstream file("1.qr");
    
    if (!file.is_open()) {
        std::cerr << "Could not open 1.qr\n";
        return 1;
    }

    std::string source(
        (std::istreambuf_iterator<char>(file)),
        std::istreambuf_iterator<char>()
    );

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenize();

    for(const Token& token : tokens){
        std::cout << tokenTypeToString(token.type) << "(\"" << token.value << "\")" << "\n";
    }

    return 0;
}