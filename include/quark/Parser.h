#pragma once

#include <string>
#include <vector>
#include "Token.h"
#include "AST.h"

// Recursive-descent parser for Quark.
//
//   program        := statement* EOF
//   statement      := letStmt | assignStmt | exprStmt
//   letStmt        := "let" IDENTIFIER "=" expression ";"
//   assignStmt     := IDENTIFIER "=" expression ";"
//   exprStmt       := expression ";"
//   expression     := addition
//   addition       := multiplication (("+" | "-") multiplication)*
//   multiplication := unary (("*" | "/") unary)*
//   unary          := ("-" | "+") unary | primary
//   primary        := INTEGER | IDENTIFIER | call | "(" expression ")"
//   call           := IDENTIFIER "(" (expression ("," expression)*)? ")"
class Parser {
private:
    std::vector<Token> tokens;
    int curr_pos = 0;

    Token peek();
    Token peekNext();
    Token advance();
    bool check(TokenType type);
    bool match(TokenType type);
    Token consume(TokenType type, const std::string& expected);

    Stmt* parseStatement();
    Stmt* parseLet();
    Stmt* parseAssignment();
    Stmt* parseExprStatement();

    Expr* parsePrimary();
    Expr* parseUnary();
    Expr* parseMultiplication();
    Expr* parseAddition();

public:
    Parser(const std::vector<Token>& tokens);

    // Parses the whole token stream. The caller owns the returned Program,
    // which in turn owns every node below it.
    Program* parseProgram();

    Expr* parseExpression();
};
