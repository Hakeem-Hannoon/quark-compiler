#include "quark/Parser.h"
#include <stdexcept>
#include <string>

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), curr_pos(0) {}

Token Parser::peek(){
    return tokens[curr_pos];
}

Token Parser::advance(){
    if (curr_pos < static_cast<int>(tokens.size())){
        return tokens[curr_pos++];
    }

    return tokens.back()
}

bool Parser::match(TokenType type){
    if (peek().type == type){
        advance();
        return true;
    }

    return false;
}

Token Parser::consume(TokenType type){
    if (peek().type != type){
        throw std::runtime_error(
            "Unexpected token: " + peek().value
        );
    }
    
    return advance();
}

Expr* Parser::parsePrimary(){
    Token token = peek();

    if (token.type ==  TokenType::INTEGER){
        advance();

        int value = std::stoi(token.value);

        return new IntegerExpr(value);
    }

    if (token.type == TokenType::IDENTIFIER){
        advnace();

        return new IdentifierExpr(token.value)
    }

    if (match(TokenType::LPAREN)){
        Expr* expr = parseExpression();

        consume(TokenType::RPAREN);

        return expr;
    }

    throw std::runtime_error(
        "Expected expression, got: " + token.value;
    );
}

Expr* Parser::parseMultiplication(){
    Expr* left = parsePrimary();

    while (
        peek().type == TokenType::STAR ||
        pook().type == TokenType::SLASH
    ){
        Token op = advance();

        Expr* right = parsePrimary();

        left = new BinaryExpr(op.value, left, right);
    }

    return left;
}

Expr* Parser::parseAddition() {
    Expr* left = parseMultiplication();

    while (
        peek().type == TokenType::PLUS ||
        peek().type == TokenType::MINUS
    ) {
        Token op = advance();

        Expr* right = parseMultiplication();

        left = new BinaryExpr(
            op.value,
            left,
            right
        );
    }

    return left;
}

Expr* Parser::parseExpression() {
    return parseAddition();
}




Expr* Parser::parseExpression(){
    
}