#pragma once

#include <string>

struct Expr{
    virtual ~Expr() = default;
};

struct IntegerExpr : Expr {
    int value;

    IntegerExpr(int value) : value(value){}
};

struct IdentifierExpr : Expr{
    std::string name;

    IdentifierExpr(const std::string& name): name(name){}
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
};