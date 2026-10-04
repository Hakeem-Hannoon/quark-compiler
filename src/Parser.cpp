#include "quark/Parser.h"
#include "quark/Error.h"
#include <algorithm>
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

    // The parser recurses once per level of nesting, and every later stage
    // recurses once per level of the tree it built, so an expression with no
    // limit on either would overflow the stack. Nobody writes one this deep by
    // hand; the cap turns a crash on generated or hostile input into an error.
    const int MAX_EXPRESSION_DEPTH = 500;

    std::string tooDeepMessage() {
        return "Expression is too complex (more than " +
               std::to_string(MAX_EXPRESSION_DEPTH) + " levels deep)";
    }

    // Holds one level of Parser::nesting for as long as it is in scope.
    struct NestingGuard {
        int& nesting;

        explicit NestingGuard(int& nesting) : nesting(nesting) {}
        ~NestingGuard() { nesting--; }
    };

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

// Where to report that something expected at the cursor is not there. When the
// next token has wandered onto a later line, the gap is back at the end of the
// line that was being parsed, not wherever the source happens to resume.
int Parser::missingLine(){
    int line = peek().line;

    if (curr_pos > 0 && tokens[curr_pos - 1].line < line){
        line = tokens[curr_pos - 1].line;
    }

    return line;
}

Token Parser::consume(TokenType type, const std::string& expected){
    if (peek().type != type){
        throw CompileError(
            missingLine(),
            "Expected " + expected + ", got " + describe(peek())
        );
    }

    return advance();
}

// Called on the way down, before recursing into a nested expression. The
// caller pairs it with a NestingGuard to give the level back.
void Parser::enterExpression(const Token& at){
    if (nesting >= MAX_EXPRESSION_DEPTH){
        throw CompileError(at.line, tooDeepMessage());
    }

    nesting++;
}

// Called on the way back up, with the height a node about to be built would
// have. Parentheses add nesting but no height; a long `a + b + c + ...` adds
// height but no nesting, because it is parsed in a loop. Hence both checks.
void Parser::checkHeight(int candidate, const Token& at){
    if (candidate > MAX_EXPRESSION_DEPTH){
        throw CompileError(at.line, tooDeepMessage());
    }
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
        height = 1;

        return expr;
    }

    if (token.type == TokenType::IDENTIFIER){
        advance();

        // A name followed by '(' is a call, otherwise it is a variable read.
        if (check(TokenType::LPAREN)){
            advance();

            std::vector<Expr*> args;
            int tallest_arg = 0;

            // Clean up the arguments parsed so far if a later one throws.
            try {
                enterExpression(token);
                NestingGuard guard(nesting);

                if (!check(TokenType::RPAREN)){
                    do {
                        args.push_back(parseExpression());
                        tallest_arg = std::max(tallest_arg, height);
                    } while (match(TokenType::COMMA));
                }

                consume(TokenType::RPAREN, "')' after arguments");
                checkHeight(tallest_arg + 1, token);
            } catch (...) {
                for (Expr* arg : args) {
                    delete arg;
                }
                throw;
            }

            Expr* expr = new CallExpr(token.value, args);
            expr->line = token.line;
            height = tallest_arg + 1;

            return expr;
        }

        Expr* expr = new IdentifierExpr(token.value);
        expr->line = token.line;
        height = 1;

        return expr;
    }

    if (match(TokenType::LPAREN)){
        enterExpression(token);
        NestingGuard guard(nesting);

        // Grouping builds no node of its own, so `height` passes through.
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
        missingLine(),
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
            height = 1;

            return expr;
        }
    }

    if (check(TokenType::MINUS) || check(TokenType::PLUS)){
        Token op = advance();

        enterExpression(op);
        NestingGuard guard(nesting);

        std::unique_ptr<Expr> operand(parseUnary());
        checkHeight(height + 1, op);

        Expr* expr = new UnaryExpr(op.value, operand.release());
        expr->line = op.line;
        height = height + 1;

        return expr;
    }

    return parsePrimary();
}

Expr* Parser::parseMultiplication(){
    std::unique_ptr<Expr> left(parseUnary());
    int left_height = height;

    while (
        check(TokenType::STAR) ||
        check(TokenType::SLASH)
    ){
        Token op = advance();

        std::unique_ptr<Expr> right(parseUnary());

        left_height = std::max(left_height, height) + 1;
        checkHeight(left_height, op);

        Expr* combined = new BinaryExpr(op.value, left.release(), right.release());
        combined->line = op.line;

        left.reset(combined);
    }

    height = left_height;

    return left.release();
}

Expr* Parser::parseAddition() {
    std::unique_ptr<Expr> left(parseMultiplication());
    int left_height = height;

    while (
        check(TokenType::PLUS) ||
        check(TokenType::MINUS)
    ) {
        Token op = advance();

        std::unique_ptr<Expr> right(parseMultiplication());

        left_height = std::max(left_height, height) + 1;
        checkHeight(left_height, op);

        Expr* combined = new BinaryExpr(
            op.value,
            left.release(),
            right.release()
        );
        combined->line = op.line;

        left.reset(combined);
    }

    height = left_height;

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
