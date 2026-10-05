#pragma once

#include <map>
#include <string>
#include "AST.h"

// Walks the AST and rejects programs that parse but cannot mean anything:
// reads of undeclared variables, duplicate declarations, calls to unknown
// functions, wrong argument counts, print() used as a value, statements whose
// value is discarded, division by a literal zero, and operators applied to
// values of the wrong type.
//
// It is also the type checker. It works out the type of every expression from
// the types of its parts, without running anything, and records the answer on
// the node for the code generator to use.
//
// Quark has a single global scope, so one table of variables is enough.
class Analyzer {
private:
    // Every declared variable and its type. A variable takes the type of the
    // value it is declared with and keeps it for good.
    std::map<std::string, Type> variables;

    void analyzeStatement(Stmt* stmt);
    Type analyzeExpression(Expr* expr);
    Type typeOf(Expr* expr);
    void analyzeCall(CallExpr* call);

public:
    // Throws CompileError on the first problem found.
    void analyze(Program* program);
};
