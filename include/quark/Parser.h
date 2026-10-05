#pragma once

#include <initializer_list>
#include <string>
#include <vector>
#include "Token.h"
#include "AST.h"
#include "Error.h"

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
//   primary        := INTEGER | "true" | "false" | IDENTIFIER | call
//                   | "(" expression ")"
//   call           := IDENTIFIER "(" (expression ("," expression)*)? ")"
class Parser {
private:
    std::vector<Token> tokens;
    int curr_pos = 0;

    // Index of the first token of the statement being parsed.
    int statement_start = 0;

    // How many expressions deep the parser currently is, and how tall the tree
    // for the expression it most recently finished came out. Both are capped
    // so that no input can overflow the stack; see enterExpression().
    int nesting = 0;
    int height = 0;

    std::vector<CompileError> found_errors;

    Token peek();
    Token peekNext();
    Token advance();
    bool check(TokenType type);
    bool match(TokenType type);
    [[noreturn]] void fail(const std::string& expected);
    void synchronize();
    void enterExpression(const Token& at);
    void checkHeight(int candidate, const Token& at);
    Token consume(TokenType type, const std::string& expected);

    Stmt* parseStatement();
    Stmt* parseLet();
    Stmt* parseAssignment();
    Stmt* parseExprStatement();

    // One precedence level of the grammar, as a member function to call.
    using Level = Expr* (Parser::*)();

    Expr* parseBinaryLevel(Level operand, std::initializer_list<TokenType> operators);

    Expr* parsePrimary();
    Expr* parseUnary();
    Expr* parseMultiplication();
    Expr* parseAddition();

public:
    Parser(const std::vector<Token>& tokens);

    // Parses the whole token stream. The caller owns the returned Program,
    // which in turn owns every node below it.
    //
    // A statement that does not parse is recorded in errors() and left out,
    // and parsing resumes at the next statement, so one run reports every
    // syntax error in the file. A Program is only meaningful if errors() is
    // empty afterwards.
    Program* parseProgram();

    const std::vector<CompileError>& errors() const { return found_errors; }

    Expr* parseExpression();
};
