#pragma once

#include <set>
#include <string>
#include "AST.h"

// Walks the AST and rejects programs that parse but cannot mean anything:
// reads of undeclared variables, duplicate declarations, calls to unknown
// functions, wrong argument counts, statements whose value is discarded, and
// division by a literal zero.
//
// Quark has a single global scope and one type (int), so this is the whole of
// the front end's checking.
class Analyzer {
private:
    std::set<std::string> declared;

    void analyzeStatement(Stmt* stmt);
    void analyzeExpression(Expr* expr);

public:
    // Throws CompileError on the first problem found.
    void analyze(Program* program);
};
