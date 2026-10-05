#include "quark/Analyzer.h"
#include "quark/Error.h"

namespace {
    // The only function Quark knows about, and how many arguments it takes.
    const char* BUILTIN_PRINT = "print";
    const std::size_t PRINT_ARITY = 1;

    bool isArithmetic(const std::string& op) {
        return op == "+" || op == "-" || op == "*" || op == "/";
    }

    bool isComparison(const std::string& op) {
        return op == "<" || op == "<=" || op == ">" || op == ">=";
    }

    bool isEquality(const std::string& op) {
        return op == "==" || op == "!=";
    }

    bool isLogical(const std::string& op) {
        return op == "&&" || op == "||";
    }

    // The error for a binary operator whose operands are not the type it
    // works on, e.g. "Operator '+' needs int operands, got bool and int".
    CompileError operandError(BinaryExpr* binary, Type wanted, Type left, Type right) {
        return CompileError(
            binary->line,
            binary->column,
            "Operator '" + binary->op + "' needs " + typeName(wanted) +
                " operands, got " + typeName(left) + " and " + typeName(right)
        );
    }
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
        Type type = analyzeExpression(let->value);

        if (variables.count(let->name)) {
            throw CompileError(let->line, let->column, "Variable already declared: " + let->name);
        }

        // No annotation is written in the source: the variable is whatever
        // type its first value is.
        variables[let->name] = type;
        return;
    }

    if (AssignStmt* assign = dynamic_cast<AssignStmt*>(stmt)) {
        auto variable = variables.find(assign->name);

        if (variable == variables.end()) {
            throw CompileError(
                assign->line,
                assign->column,
                "Assignment to undeclared variable: " + assign->name +
                    " (use 'let " + assign->name + " = ...')"
            );
        }

        Type declared = variable->second;
        Type assigned = analyzeExpression(assign->value);

        if (assigned != declared) {
            throw CompileError(
                assign->line,
                assign->column,
                "Cannot assign " + typeName(assigned) + " to " + typeName(declared) +
                    " variable '" + assign->name + "'"
            );
        }
        return;
    }

    if (ExprStmt* expression = dynamic_cast<ExprStmt*>(stmt)) {
        // Nothing in Quark has a side effect except a call, so any other
        // expression statement is a mistake rather than dead code.
        CallExpr* call = dynamic_cast<CallExpr*>(expression->expr);

        if (!call) {
            throw CompileError(
                expression->line,
                expression->column,
                "Expression result is unused"
            );
        }

        analyzeCall(call);
        return;
    }

    throw CompileError(stmt->line, stmt->column, "Unsupported statement");
}

// Checks an expression and returns its type, leaving the same answer on the
// node so that later stages do not have to work it out again.
Type Analyzer::analyzeExpression(Expr* expr) {
    expr->type = typeOf(expr);

    return expr->type;
}

// The typing rules, one per kind of expression. Each asks for the types of its
// parts first and then decides its own from them, so types flow up the tree
// from the leaves: `1 + 2 < 4` is int + int, which is int; then int < int.
Type Analyzer::typeOf(Expr* expr) {
    if (dynamic_cast<IntegerExpr*>(expr)) {
        return Type::Int;
    }

    if (dynamic_cast<BoolExpr*>(expr)) {
        return Type::Bool;
    }

    if (IdentifierExpr* identifier = dynamic_cast<IdentifierExpr*>(expr)) {
        auto variable = variables.find(identifier->name);

        if (variable == variables.end()) {
            throw CompileError(
                identifier->line,
                identifier->column,
                "Undeclared variable: " + identifier->name
            );
        }

        return variable->second;
    }

    if (UnaryExpr* unary = dynamic_cast<UnaryExpr*>(expr)) {
        Type operand = analyzeExpression(unary->operand);

        // !       : bool -> bool
        // - and + : int  -> int
        Type wanted = (unary->op == "!") ? Type::Bool : Type::Int;

        if (operand != wanted) {
            throw CompileError(
                unary->line,
                unary->column,
                "Operator '" + unary->op + "' needs " +
                    (wanted == Type::Bool ? "a bool" : "an int") +
                    " operand, got " + typeName(operand)
            );
        }

        return wanted;
    }

    if (BinaryExpr* binary = dynamic_cast<BinaryExpr*>(expr)) {
        Type left = analyzeExpression(binary->left);
        Type right = analyzeExpression(binary->right);

        // + - * / : int, int -> int
        if (isArithmetic(binary->op)) {
            if (left != Type::Int || right != Type::Int) {
                throw operandError(binary, Type::Int, left, right);
            }

            if (binary->op == "/") {
                IntegerExpr* divisor = dynamic_cast<IntegerExpr*>(binary->right);

                if (divisor && divisor->value == 0) {
                    throw CompileError(binary->line, binary->column, "Division by zero");
                }
            }

            return Type::Int;
        }

        // < <= > >= : int, int -> bool
        //
        // The result is not an int, which is what makes `3 > 2 > 1` an error
        // here instead of the quiet wrong answer it is in C: the second `>`
        // would be comparing a bool with an int.
        if (isComparison(binary->op)) {
            if (left != Type::Int || right != Type::Int) {
                throw operandError(binary, Type::Int, left, right);
            }

            return Type::Bool;
        }

        // == != : any two values of the same type -> bool
        if (isEquality(binary->op)) {
            if (left != right) {
                throw CompileError(
                    binary->line,
                    binary->column,
                    "Operator '" + binary->op + "' needs operands of the same type, got " +
                        typeName(left) + " and " + typeName(right)
                );
            }

            return Type::Bool;
        }

        // && || : bool, bool -> bool
        if (isLogical(binary->op)) {
            if (left != Type::Bool || right != Type::Bool) {
                throw operandError(binary, Type::Bool, left, right);
            }

            return Type::Bool;
        }

        throw CompileError(binary->line, binary->column, "Unsupported operator: " + binary->op);
    }

    if (CallExpr* call = dynamic_cast<CallExpr*>(expr)) {
        analyzeCall(call);

        // Reaching a call from here means it sits inside a larger expression.
        // print() has nothing to give one; letting it through would hand the
        // program whatever C's printf happens to return.
        throw CompileError(
            call->line,
            call->column,
            "print() does not return a value"
        );
    }

    throw CompileError(expr->line, expr->column, "Unsupported expression");
}

void Analyzer::analyzeCall(CallExpr* call) {
    if (call->callee != BUILTIN_PRINT) {
        throw CompileError(call->line, call->column, "Unknown function: " + call->callee);
    }

    if (call->args.size() != PRINT_ARITY) {
        throw CompileError(
            call->line,
            call->column,
            "print() takes 1 argument, got " + std::to_string(call->args.size())
        );
    }

    // print() accepts a value of either type; the code generator looks at the
    // type recorded here to decide how to show it.
    for (Expr* arg : call->args) {
        analyzeExpression(arg);
    }
}
