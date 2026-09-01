#pragma once

#include <string>
#include <vector>

// Every node knows the line it came from so later stages can report errors
// against the original source.
struct Node {
    int line = 0;

    virtual ~Node() = default;
};

// ---------------------------------------------------------------- expressions

struct Expr : Node {};

struct IntegerExpr : Expr {
    int value;

    IntegerExpr(int value) : value(value){}
};

struct IdentifierExpr : Expr{
    std::string name;

    IdentifierExpr(const std::string& name): name(name){}
};

struct UnaryExpr : Expr {
    std::string op;
    Expr* operand;

    UnaryExpr(
        const std::string& op,
        Expr* operand
    ) : op(op), operand(operand) {}

    ~UnaryExpr() override { delete operand; }
};

struct BinaryExpr : Expr{
    std::string op;
    Expr* left;
    Expr* right;

    BinaryExpr(
        const std::string& op,
        Expr* left,
        Expr* right
    ) : op(op), left(left), right(right) {}

    ~BinaryExpr() override {
        delete left;
        delete right;
    }
};

struct CallExpr : Expr {
    std::string callee;
    std::vector<Expr*> args;

    CallExpr(
        const std::string& callee,
        const std::vector<Expr*>& args
    ) : callee(callee), args(args) {}

    ~CallExpr() override {
        for (Expr* arg : args) {
            delete arg;
        }
    }
};

// ----------------------------------------------------------------- statements

struct Stmt : Node {};

// let <name> = <value>;
struct LetStmt : Stmt {
    std::string name;
    Expr* value;

    LetStmt(
        const std::string& name,
        Expr* value
    ) : name(name), value(value) {}

    ~LetStmt() override { delete value; }
};

// <name> = <value>;
struct AssignStmt : Stmt {
    std::string name;
    Expr* value;

    AssignStmt(
        const std::string& name,
        Expr* value
    ) : name(name), value(value) {}

    ~AssignStmt() override { delete value; }
};

// <expr>;   (a call such as `print(y);`)
struct ExprStmt : Stmt {
    Expr* expr;

    ExprStmt(Expr* expr) : expr(expr) {}

    ~ExprStmt() override { delete expr; }
};

// The whole translation unit. Owns every node reachable from it.
struct Program : Node {
    std::vector<Stmt*> statements;

    ~Program() override {
        for (Stmt* stmt : statements) {
            delete stmt;
        }
    }
};
