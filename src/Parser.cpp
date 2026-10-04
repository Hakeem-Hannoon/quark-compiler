#include "quark/Parser.h"
#include "quark/Error.h"
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    // How a token should read inside an error message.
    std::string describe(const Token& token) {
        if (token.type == TokenType::END_OF_FILE) {
            return "end of file";
        }

        return "'" + token.value + "'";
    }

    const long long INT_MAX_VALUE = 2147483647LL;

    // The most negative int is one further from zero than the most positive,
    // so its digits are only a valid literal directly under a unary minus.
    const long long INT_MIN_MAGNITUDE = 2147483648LL;

    // The value of an INTEGER token's digits, or false if there are too many
    // of them to hold at all.
    bool literalMagnitude(const Token& token, long long& value) {
        try {
            value = std::stoll(token.value);
        } catch (const std::out_of_range&) {
            return false;
        }

        return true;
    }
}

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), curr_pos(0) {}

Token Parser::peek(){
    return tokens[curr_pos];
}

// One token past the cursor, clamped to the EOF token so lookahead is always
// safe. Used to tell `x = 1;` from a bare expression statement.
Token Parser::peekNext(){
    if (curr_pos + 1 < static_cast<int>(tokens.size())){
        return tokens[curr_pos + 1];
    }

    return tokens.back();
}

Token Parser::advance(){
    if (curr_pos < static_cast<int>(tokens.size()) - 1){
        return tokens[curr_pos++];
    }

    return tokens.back();
}

bool Parser::check(TokenType type){
    return peek().type == type;
}

bool Parser::match(TokenType type){
    if (check(type)){
        advance();
        return true;
    }

    return false;
}

Token Parser::consume(TokenType type, const std::string& expected){
    if (peek().type != type){
        throw CompileError(
            peek().line,
            "Expected " + expected + ", got " + describe(peek())
        );
    }

    return advance();
}

// ---------------------------------------------------------------- expressions

Expr* Parser::parsePrimary(){
    Token token = peek();

    if (token.type == TokenType::INTEGER){
        advance();

        long long value = 0;

        if (!literalMagnitude(token, value) || value > INT_MAX_VALUE) {
            throw CompileError(token.line, "Integer literal out of range: " + token.value);
        }

        Expr* expr = new IntegerExpr(static_cast<int>(value));
        expr->line = token.line;

        return expr;
    }

    if (token.type == TokenType::IDENTIFIER){
        advance();

        // A name followed by '(' is a call, otherwise it is a variable read.
        if (check(TokenType::LPAREN)){
            advance();

            std::vector<Expr*> args;

            // Clean up the arguments parsed so far if a later one throws.
            try {
                if (!check(TokenType::RPAREN)){
                    do {
                        args.push_back(parseExpression());
                    } while (match(TokenType::COMMA));
                }

                consume(TokenType::RPAREN, "')' after arguments");
            } catch (...) {
                for (Expr* arg : args) {
                    delete arg;
                }
                throw;
            }

            Expr* expr = new CallExpr(token.value, args);
            expr->line = token.line;

            return expr;
        }

        Expr* expr = new IdentifierExpr(token.value);
        expr->line = token.line;

        return expr;
    }

    if (match(TokenType::LPAREN)){
        Expr* expr = parseExpression();

        try {
            consume(TokenType::RPAREN, "')' after expression");
        } catch (...) {
            delete expr;
            throw;
        }

        return expr;
    }

    throw CompileError(
        token.line,
        "Expected expression, got " + describe(token)
    );
}

Expr* Parser::parseUnary(){
    // `-2147483648` has to be read as one literal: negating 2147483648 would
    // mean first holding a value that does not fit in an int.
    if (check(TokenType::MINUS) && peekNext().type == TokenType::INTEGER){
        long long magnitude = 0;

        if (literalMagnitude(peekNext(), magnitude) && magnitude == INT_MIN_MAGNITUDE){
            Token op = advance();
            advance();

            Expr* expr = new IntegerExpr(std::numeric_limits<int>::min());
            expr->line = op.line;

            return expr;
        }
    }

    if (check(TokenType::MINUS) || check(TokenType::PLUS)){
        Token op = advance();

        Expr* operand = parseUnary();

        Expr* expr = new UnaryExpr(op.value, operand);
        expr->line = op.line;

        return expr;
    }

    return parsePrimary();
}

Expr* Parser::parseMultiplication(){
    std::unique_ptr<Expr> left(parseUnary());

    while (
        check(TokenType::STAR) ||
        check(TokenType::SLASH)
    ){
        Token op = advance();

        Expr* right = parseUnary();

        Expr* combined = new BinaryExpr(op.value, left.release(), right);
        combined->line = op.line;

        left.reset(combined);
    }

    return left.release();
}

Expr* Parser::parseAddition() {
    std::unique_ptr<Expr> left(parseMultiplication());

    while (
        check(TokenType::PLUS) ||
        check(TokenType::MINUS)
    ) {
        Token op = advance();

        Expr* right = parseMultiplication();

        Expr* combined = new BinaryExpr(
            op.value,
            left.release(),
            right
        );
        combined->line = op.line;

        left.reset(combined);
    }

    return left.release();
}

Expr* Parser::parseExpression() {
    return parseAddition();
}

// ----------------------------------------------------------------- statements

Stmt* Parser::parseLet(){
    Token keyword = consume(TokenType::LET, "'let'");
    Token name = consume(TokenType::IDENTIFIER, "a variable name after 'let'");

    consume(TokenType::EQUAL, "'=' after variable name");

    std::unique_ptr<Expr> value(parseExpression());

    consume(TokenType::SEMICOLON, "';' after declaration");

    Stmt* stmt = new LetStmt(name.value, value.release());
    stmt->line = keyword.line;

    return stmt;
}

Stmt* Parser::parseAssignment(){
    Token name = consume(TokenType::IDENTIFIER, "a variable name");

    consume(TokenType::EQUAL, "'=' after variable name");

    std::unique_ptr<Expr> value(parseExpression());

    consume(TokenType::SEMICOLON, "';' after assignment");

    Stmt* stmt = new AssignStmt(name.value, value.release());
    stmt->line = name.line;

    return stmt;
}

Stmt* Parser::parseExprStatement(){
    int line = peek().line;

    std::unique_ptr<Expr> expr(parseExpression());

    consume(TokenType::SEMICOLON, "';' after expression");

    Stmt* stmt = new ExprStmt(expr.release());
    stmt->line = line;

    return stmt;
}

Stmt* Parser::parseStatement(){
    if (check(TokenType::LET)){
        return parseLet();
    }

    if (check(TokenType::IDENTIFIER) && peekNext().type == TokenType::EQUAL){
        return parseAssignment();
    }

    return parseExprStatement();
}

Program* Parser::parseProgram(){
    std::unique_ptr<Program> program(new Program());
    program->line = 1;

    while (!check(TokenType::END_OF_FILE)){
        program->statements.push_back(parseStatement());
    }

    return program.release();
}
