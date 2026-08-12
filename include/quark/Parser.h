#pragma once

#include <vector>
#include "Token.h"
#include "AST.h"

class Parser { 
private:
    std::vector<Token> tokens;
    int curr_pos = 0;
    
    Token peek();
    Token advance();
    bool match(TokenType type);
    Token consume(TokenType type);

    Expr* parsePrimary();
    Expr* parseMultiplication();
    Expr* parseAddition();

public:
    Parser(cost std::vector<Token>& tokens);

    Expr* parseExpression();
};