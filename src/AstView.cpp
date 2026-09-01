#include "quark/AstView.h"

namespace {

TreeNode viewExpression(Expr* expr);

TreeNode makeNode(const std::string& label, NodeKind kind, Node* source) {
    TreeNode node;
    node.label = label;
    node.kind = kind;
    node.line = source->line;

    return node;
}

TreeNode viewStatement(Stmt* stmt) {
    if (LetStmt* let = dynamic_cast<LetStmt*>(stmt)) {
        TreeNode node = makeNode("Let " + let->name, NodeKind::Statement, stmt);
        node.children.push_back(viewExpression(let->value));

        return node;
    }

    if (AssignStmt* assign = dynamic_cast<AssignStmt*>(stmt)) {
        TreeNode node = makeNode("Assign " + assign->name, NodeKind::Statement, stmt);
        node.children.push_back(viewExpression(assign->value));

        return node;
    }

    if (ExprStmt* expression = dynamic_cast<ExprStmt*>(stmt)) {
        TreeNode node = makeNode("ExprStmt", NodeKind::Statement, stmt);
        node.children.push_back(viewExpression(expression->expr));

        return node;
    }

    return makeNode("<unknown statement>", NodeKind::Unknown, stmt);
}

TreeNode viewExpression(Expr* expr) {
    if (IntegerExpr* integer = dynamic_cast<IntegerExpr*>(expr)) {
        return makeNode(
            "Integer " + std::to_string(integer->value),
            NodeKind::Literal,
            expr
        );
    }

    if (IdentifierExpr* identifier = dynamic_cast<IdentifierExpr*>(expr)) {
        return makeNode("Identifier " + identifier->name, NodeKind::Variable, expr);
    }

    if (UnaryExpr* unary = dynamic_cast<UnaryExpr*>(expr)) {
        TreeNode node = makeNode("Unary " + unary->op, NodeKind::Operator, expr);
        node.children.push_back(viewExpression(unary->operand));

        return node;
    }

    if (BinaryExpr* binary = dynamic_cast<BinaryExpr*>(expr)) {
        TreeNode node = makeNode("Binary " + binary->op, NodeKind::Operator, expr);
        node.children.push_back(viewExpression(binary->left));
        node.children.push_back(viewExpression(binary->right));

        return node;
    }

    if (CallExpr* call = dynamic_cast<CallExpr*>(expr)) {
        TreeNode node = makeNode("Call " + call->callee, NodeKind::Call, expr);

        for (Expr* arg : call->args) {
            node.children.push_back(viewExpression(arg));
        }

        return node;
    }

    return makeNode("<unknown expression>", NodeKind::Unknown, expr);
}

} // namespace

TreeNode buildAstView(Program* program) {
    TreeNode root = makeNode("Program", NodeKind::Program, program);

    for (Stmt* stmt : program->statements) {
        root.children.push_back(viewStatement(stmt));
    }

    return root;
}
