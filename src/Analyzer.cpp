#include "quark/Analyzer.h"
#include "quark/Error.h"

namespace {
    // The only function Quark knows about, and how many arguments it takes.
    const char* BUILTIN_PRINT = "print";
    const std::size_t PRINT_ARITY = 1;
}

void Analyzer::analyze(Program* program) {
    for (Stmt* stmt : program->statements) {
        analyzeStatement(stmt);
    }
}

void Analyzer::analyzeStatement(Stmt* stmt) {
    if (LetStmt* let = dynamic_cast<LetStmt*>(stmt)) {
        // The initialiser is checked first, so `let x = x;` is an error rather
        // than a read of the variable being declared.
        analyzeExpression(let->value);

        if (declared.count(let->name)) {
            throw CompileError(let->line, "Variable already declared: " + let->name);
        }

        declared.insert(let->name);
        return;
    }

    if (AssignStmt* assign = dynamic_cast<AssignStmt*>(stmt)) {
        if (!declared.count(assign->name)) {
            throw CompileError(
                assign->line,
                "Assignment to undeclared variable: " + assign->name +
                    " (use 'let " + assign->name + " = ...')"
            );
        }

        analyzeExpression(assign->value);
        return;
    }

    if (ExprStmt* expression = dynamic_cast<ExprStmt*>(stmt)) {
        // Nothing in Quark has a side effect except a call, so any other
        // expression statement is a mistake rather than dead code.
        if (!dynamic_cast<CallExpr*>(expression->expr)) {
            throw CompileError(
                expression->line,
                "Expression result is unused"
            );
        }

        analyzeExpression(expression->expr);
        return;
    }

    throw CompileError(stmt->line, "Unsupported statement");
}

void Analyzer::analyzeExpression(Expr* expr) {
    if (dynamic_cast<IntegerExpr*>(expr)) {
        return;
    }

    if (IdentifierExpr* identifier = dynamic_cast<IdentifierExpr*>(expr)) {
        if (!declared.count(identifier->name)) {
            throw CompileError(
                identifier->line,
                "Undeclared variable: " + identifier->name
            );
        }
        return;
    }

    if (UnaryExpr* unary = dynamic_cast<UnaryExpr*>(expr)) {
        analyzeExpression(unary->operand);
        return;
    }

    if (BinaryExpr* binary = dynamic_cast<BinaryExpr*>(expr)) {
        analyzeExpression(binary->left);
        analyzeExpression(binary->right);

        if (binary->op == "/") {
            IntegerExpr* divisor = dynamic_cast<IntegerExpr*>(binary->right);

            if (divisor && divisor->value == 0) {
                throw CompileError(binary->line, "Division by zero");
            }
        }
        return;
    }

    if (CallExpr* call = dynamic_cast<CallExpr*>(expr)) {
        if (call->callee != BUILTIN_PRINT) {
            throw CompileError(call->line, "Unknown function: " + call->callee);
        }

        if (call->args.size() != PRINT_ARITY) {
            throw CompileError(
                call->line,
                "print() takes 1 argument, got " + std::to_string(call->args.size())
            );
        }

        for (Expr* arg : call->args) {
            analyzeExpression(arg);
        }
        return;
    }

    throw CompileError(expr->line, "Unsupported expression");
}
