#pragma once

#include <sstream>
#include <string>
#include "AST.h"

// Lowers a checked AST to a self-contained C translation unit, which the driver
// then hands to the host C compiler. C is the target because it is portable and
// needs no toolchain beyond the one already required to build quarkc.
class CodeGen {
private:
    std::ostringstream out;

    void emitStatement(Stmt* stmt);
    std::string emitExpression(Expr* expr);

public:
    std::string generate(Program* program);
};
