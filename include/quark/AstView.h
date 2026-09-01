#pragma once

#include <string>
#include <vector>
#include "AST.h"

// A rendering-friendly view of the AST: every node flattened to a label, a
// broad kind (which drives colour), and its children. The three visualisers
// below all walk this instead of the AST itself, so the dynamic_cast cascade
// that classifies nodes lives in exactly one place.
enum class NodeKind {
    Program,
    Statement,
    Operator,
    Literal,
    Variable,
    Call,
    Unknown
};

struct TreeNode {
    std::string label;
    NodeKind kind = NodeKind::Unknown;
    int line = 0;
    std::vector<TreeNode> children;
};

TreeNode buildAstView(Program* program);

// Indented tree using box-drawing connectors, for reading in a terminal.
std::string renderTree(const TreeNode& root);

// Graphviz DOT, for `dot -Tpng`. Needs graphviz installed.
std::string renderDot(const TreeNode& root);

// A standalone, self-laying-out SVG. Needs nothing but a browser.
std::string renderSvg(const TreeNode& root);
