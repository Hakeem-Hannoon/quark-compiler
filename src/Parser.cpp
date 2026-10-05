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

    // Thrown to abandon a statement whose problem the lexer has already
    // reported, so that it is skipped without a second message.
    struct AlreadyReported {};

    // Records where in the source a freshly built node came from.
    void locate(Node* node, const Token& token) {
        node->line = token.line;
        node->column = token.column;
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

// Abandons the current statement because what the grammar needs at the cursor
// is not there. The error points at the token found instead, unless that token
// has wandered onto a later line than the rest of the statement: then the gap
// is back where the previous token ended, not wherever the source happens to
// resume.
void Parser::fail(const std::string& expected){
    const Token found = peek();

    if (found.type == TokenType::INVALID){
        throw AlreadyReported();
    }

    int line = found.line;
    int column = found.column;

    if (curr_pos > statement_start && tokens[curr_pos - 1].line < line){
        const Token& before = tokens[curr_pos - 1];

        line = before.line;
        column = before.column + static_cast<int>(before.value.size());
    }

    throw CompileError(line, column, "Expected " + expected + ", got " + describe(found));
}

// Skips to where the next statement starts: just past a ';', or at a 'let'.
// Without this, everything after a mistake would be read from the middle of a
// statement and reported as a pile of errors that are not really there.
void Parser::synchronize(){
    while (!check(TokenType::END_OF_FILE)){
        if (check(TokenType::LET)){
            return;
        }

        if (advance().type == TokenType::SEMICOLON){
            return;
        }
    }
}

Token Parser::consume(TokenType type, const std::string& expected){
    if (peek().type != type){
        fail(expected);
    }

    return advance();
}

// Called on the way down, before recursing into a nested expression. The
// caller pairs it with a NestingGuard to give the level back.
void Parser::enterExpression(const Token& at){
    if (nesting >= MAX_EXPRESSION_DEPTH){
        throw CompileError(at.line, at.column, tooDeepMessage());
    }

    nesting++;
}

// Called on the way back up, with the height a node about to be built would
// have. Parentheses add nesting but no height; a long `a + b + c + ...` adds
// height but no nesting, because it is parsed in a loop. Hence both checks.
void Parser::checkHeight(int candidate, const Token& at){
    if (candidate > MAX_EXPRESSION_DEPTH){
        throw CompileError(at.line, at.column, tooDeepMessage());
    }
}

// ---------------------------------------------------------------- expressions

Expr* Parser::parsePrimary(){
    Token token = peek();

    if (token.type == TokenType::INTEGER){
        advance();

        long long value = 0;

        if (!literalMagnitude(token, value) || value > INT_MAX_VALUE) {
            throw CompileError(
                token.line,
                token.column,
                "Integer literal out of range: " + token.value
            );
        }

        Expr* expr = new IntegerExpr(static_cast<int>(value));
        locate(expr, token);
        height = 1;

        return expr;
    }

    if (token.type == TokenType::BOOLEAN){
        advance();

        Expr* expr = new BoolExpr(token.value == "true");
        locate(expr, token);
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
            locate(expr, token);
            height = tallest_arg + 1;

            return expr;
        }

        Expr* expr = new IdentifierExpr(token.value);
        locate(expr, token);
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

    fail("expression");
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
            locate(expr, op);
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
        locate(expr, op);
        height = height + 1;

        return expr;
    }

    return parsePrimary();
}

// One left-associative level of the grammar:  operand (operator operand)*
//
// Every binary level has this shape. They differ only in which operators they
// accept and which tighter level supplies their operands, so they share this
// loop. `operand` is a pointer to a member function: it is how a caller says
// "parseUnary parses my operands", and `(this->*operand)()` calls it.
Expr* Parser::parseBinaryLevel(Level operand, std::initializer_list<TokenType> operators){
    std::unique_ptr<Expr> left((this->*operand)());
    int left_height = height;

    while (std::find(operators.begin(), operators.end(), peek().type) != operators.end()){
        Token op = advance();

        std::unique_ptr<Expr> right((this->*operand)());

        left_height = std::max(left_height, height) + 1;
        checkHeight(left_height, op);

        Expr* combined = new BinaryExpr(op.value, left.release(), right.release());
        locate(combined, op);

        left.reset(combined);
    }

    height = left_height;

    return left.release();
}

Expr* Parser::parseMultiplication(){
    return parseBinaryLevel(&Parser::parseUnary, {TokenType::STAR, TokenType::SLASH});
}

Expr* Parser::parseAddition(){
    return parseBinaryLevel(&Parser::parseMultiplication, {TokenType::PLUS, TokenType::MINUS});
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
    locate(stmt, keyword);

    return stmt;
}

Stmt* Parser::parseAssignment(){
    Token name = consume(TokenType::IDENTIFIER, "a variable name");

    consume(TokenType::EQUAL, "'=' after variable name");

    std::unique_ptr<Expr> value(parseExpression());

    consume(TokenType::SEMICOLON, "';' after assignment");

    Stmt* stmt = new AssignStmt(name.value, value.release());
    locate(stmt, name);

    return stmt;
}

Stmt* Parser::parseExprStatement(){
    Token first = peek();

    std::unique_ptr<Expr> expr(parseExpression());

    consume(TokenType::SEMICOLON, "';' after expression");

    Stmt* stmt = new ExprStmt(expr.release());
    locate(stmt, first);

    return stmt;
}

Stmt* Parser::parseStatement(){
    statement_start = curr_pos;

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
    program->column = 1;

    found_errors.clear();

    while (!check(TokenType::END_OF_FILE)){
        try {
            program->statements.push_back(parseStatement());
        } catch (const CompileError& error) {
            found_errors.push_back(error);
            synchronize();
        } catch (const AlreadyReported&) {
            synchronize();
        }
    }

    return program.release();
}
