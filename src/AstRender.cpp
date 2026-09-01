#include "quark/AstView.h"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

namespace {

// ------------------------------------------------------------------- palette

struct Palette {
    const char* name;    // CSS class suffix / DOT grouping
    const char* fill;
    const char* stroke;
};

Palette paletteFor(NodeKind kind) {
    switch (kind) {
        case NodeKind::Program:   return {"program",   "#e2e8f0", "#64748b"};
        case NodeKind::Statement: return {"statement", "#e0e7ff", "#6366f1"};
        case NodeKind::Operator:  return {"operator",  "#fef3c7", "#d97706"};
        case NodeKind::Literal:   return {"literal",   "#d1fae5", "#059669"};
        case NodeKind::Variable:  return {"variable",  "#e0f2fe", "#0284c7"};
        case NodeKind::Call:      return {"call",      "#ffe4e6", "#e11d48"};
        case NodeKind::Unknown:   break;
    }

    return {"unknown", "#fee2e2", "#dc2626"};
}

// ---------------------------------------------------------------- text tree

void writeTree(
    std::ostringstream& out,
    const TreeNode& node,
    const std::string& prefix,
    bool is_root,
    bool is_last
) {
    if (is_root) {
        out << node.label << "\n";
    } else {
        out << prefix << (is_last ? "└─ " : "├─ ") << node.label << "\n";
    }

    // Children of the last sibling need blank space where the trunk would be;
    // children of any other sibling need the trunk continued.
    const std::string child_prefix =
        is_root ? "" : prefix + (is_last ? "   " : "│  ");

    for (std::size_t i = 0; i < node.children.size(); i++) {
        writeTree(
            out,
            node.children[i],
            child_prefix,
            false,
            i + 1 == node.children.size()
        );
    }
}

// ---------------------------------------------------------------------- DOT

std::string escapeDot(const std::string& text) {
    std::string escaped;

    for (char c : text) {
        if (c == '"' || c == '\\') {
            escaped += '\\';
        }
        escaped += c;
    }

    return escaped;
}

void writeDotNode(std::ostringstream& out, const TreeNode& node, int& counter, int id) {
    const Palette palette = paletteFor(node.kind);

    out << "    n" << id << " [label=\"" << escapeDot(node.label) << "\""
        << ", fillcolor=\"" << palette.fill << "\""
        << ", color=\"" << palette.stroke << "\""
        << ", tooltip=\"line " << node.line << "\"];\n";

    for (const TreeNode& child : node.children) {
        const int child_id = ++counter;

        writeDotNode(out, child, counter, child_id);

        out << "    n" << id << " -> n" << child_id << ";\n";
    }
}

// ---------------------------------------------------------------------- SVG

const double CHAR_W     = 7.8;   // width of one glyph at 13px monospace
const double PAD_X      = 13.0;
const double MIN_W      = 66.0;
const double BOX_H      = 32.0;
const double LEVEL_H    = 76.0;  // vertical distance between depths
const double SIBLING_GAP = 24.0;
const double MARGIN     = 24.0;

struct Box {
    const TreeNode* node = nullptr;
    double cx = 0;    // centre x
    double top = 0;   // top edge y
    double w = 0;
    int parent = -1;
};

double nodeWidth(const TreeNode& node) {
    const double w = static_cast<double>(node.label.size()) * CHAR_W + 2 * PAD_X;

    return w < MIN_W ? MIN_W : w;
}

// Lays the subtree out with its left edge at x = 0 and returns its total width.
// Boxes are appended depth-first, so a subtree always occupies a contiguous
// tail of the vector, which is what makes the shifts below cheap.
double layout(const TreeNode& node, int depth, int parent, std::vector<Box>& boxes) {
    const int me = static_cast<int>(boxes.size());

    Box box;
    box.node = &node;
    box.w = nodeWidth(node);
    box.top = MARGIN + depth * LEVEL_H;
    box.parent = parent;
    boxes.push_back(box);

    if (node.children.empty()) {
        boxes[me].cx = boxes[me].w / 2.0;

        return boxes[me].w;
    }

    double cursor = 0.0;
    int first_child = -1;
    int last_child = -1;

    for (const TreeNode& child : node.children) {
        const int child_index = static_cast<int>(boxes.size());
        const double child_width = layout(child, depth + 1, me, boxes);

        // The child laid itself out starting at 0; slide its whole subtree over
        // to sit after its previous siblings.
        for (std::size_t i = child_index; i < boxes.size(); i++) {
            boxes[i].cx += cursor;
        }

        if (first_child < 0) {
            first_child = child_index;
        }
        last_child = child_index;

        cursor += child_width + SIBLING_GAP;
    }

    double span = cursor - SIBLING_GAP;
    double centre = (boxes[first_child].cx + boxes[last_child].cx) / 2.0;

    // A node wider than the row of its children would otherwise stick out past
    // x = 0, so nudge the children right instead and let the parent set the span.
    if (boxes[me].w > span) {
        const double delta = (boxes[me].w - span) / 2.0;

        for (int i = me + 1; i < static_cast<int>(boxes.size()); i++) {
            boxes[i].cx += delta;
        }

        centre += delta;
        span = boxes[me].w;
    }

    boxes[me].cx = centre;

    return span;
}

std::string escapeXml(const std::string& text) {
    std::string escaped;

    for (char c : text) {
        switch (c) {
            case '&': escaped += "&amp;";  break;
            case '<': escaped += "&lt;";   break;
            case '>': escaped += "&gt;";   break;
            case '"': escaped += "&quot;"; break;
            default:  escaped += c;        break;
        }
    }

    return escaped;
}

std::string number(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(1) << value;

    return out.str();
}

void writeSvgStyle(std::ostringstream& out) {
    out << "  <style>\n";
    out << "    .bg { fill: #ffffff; }\n";
    out << "    .edge { fill: none; stroke: #94a3b8; stroke-width: 1.4; }\n";
    // rx lives on the element rather than here: as a CSS property it is SVG2
    // only, and older renderers silently square the corners off.
    out << "    .box { stroke-width: 1.4; }\n";
    out << "    .label { font: 13px ui-monospace, SFMono-Regular, Menlo, monospace;"
        << " fill: #0f172a; }\n";

    // One rule per kind, so the colours stay in step with the DOT output.
    for (NodeKind kind : {NodeKind::Program, NodeKind::Statement, NodeKind::Operator,
                          NodeKind::Literal, NodeKind::Variable, NodeKind::Call,
                          NodeKind::Unknown}) {
        const Palette palette = paletteFor(kind);

        out << "    .k-" << palette.name << " { fill: " << palette.fill
            << "; stroke: " << palette.stroke << "; }\n";
    }

    out << "    @media (prefers-color-scheme: dark) {\n";
    out << "      .bg { fill: #0f172a; }\n";
    out << "      .edge { stroke: #475569; }\n";
    out << "      .label { fill: #e2e8f0; }\n";
    out << "      .k-program { fill: #334155; stroke: #94a3b8; }\n";
    out << "      .k-statement { fill: #312e81; stroke: #818cf8; }\n";
    out << "      .k-operator { fill: #78350f; stroke: #fbbf24; }\n";
    out << "      .k-literal { fill: #064e3b; stroke: #34d399; }\n";
    out << "      .k-variable { fill: #0c4a6e; stroke: #38bdf8; }\n";
    out << "      .k-call { fill: #881337; stroke: #fb7185; }\n";
    out << "      .k-unknown { fill: #7f1d1d; stroke: #f87171; }\n";
    out << "    }\n";
    out << "  </style>\n";
}

} // namespace

std::string renderTree(const TreeNode& root) {
    std::ostringstream out;

    writeTree(out, root, "", true, true);

    return out.str();
}

std::string renderDot(const TreeNode& root) {
    std::ostringstream out;

    out << "digraph AST {\n";
    out << "    graph [rankdir=TB, bgcolor=\"transparent\", nodesep=0.3, ranksep=0.5];\n";
    out << "    node [shape=box, style=\"rounded,filled\", penwidth=1.4,"
        << " fontname=\"Menlo,Consolas,monospace\", fontsize=11,"
        << " fontcolor=\"#0f172a\", margin=\"0.14,0.08\"];\n";
    out << "    edge [color=\"#94a3b8\", penwidth=1.2, arrowsize=0.6];\n";
    out << "\n";

    int counter = 0;
    writeDotNode(out, root, counter, 0);

    out << "}\n";

    return out.str();
}

std::string renderSvg(const TreeNode& root) {
    std::vector<Box> boxes;

    const double tree_width = layout(root, 0, -1, boxes);

    double height = 0;
    for (Box& box : boxes) {
        box.cx += MARGIN;

        const double bottom = box.top + BOX_H;
        if (bottom > height) {
            height = bottom;
        }
    }

    const double width = tree_width + 2 * MARGIN;
    height += MARGIN;

    std::ostringstream out;

    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << number(width)
        << "\" height=\"" << number(height) << "\" viewBox=\"0 0 "
        << number(width) << " " << number(height) << "\">\n";

    writeSvgStyle(out);

    out << "  <rect class=\"bg\" x=\"0\" y=\"0\" width=\"" << number(width)
        << "\" height=\"" << number(height) << "\"/>\n";

    // Edges first so the boxes paint over the curve ends.
    out << "  <g class=\"edges\">\n";

    for (const Box& box : boxes) {
        if (box.parent < 0) {
            continue;
        }

        const Box& parent = boxes[box.parent];
        const double from_y = parent.top + BOX_H;
        const double to_y = box.top;
        const double bend = (to_y - from_y) / 2.0;

        out << "    <path class=\"edge\" d=\"M " << number(parent.cx) << " "
            << number(from_y) << " C " << number(parent.cx) << " "
            << number(from_y + bend) << ", " << number(box.cx) << " "
            << number(to_y - bend) << ", " << number(box.cx) << " "
            << number(to_y) << "\"/>\n";
    }

    out << "  </g>\n";
    out << "  <g class=\"nodes\">\n";

    for (const Box& box : boxes) {
        const Palette palette = paletteFor(box.node->kind);

        out << "    <g>\n";
        out << "      <title>line " << box.node->line << "</title>\n";
        out << "      <rect class=\"box k-" << palette.name << "\" x=\""
            << number(box.cx - box.w / 2.0) << "\" y=\"" << number(box.top)
            << "\" width=\"" << number(box.w) << "\" height=\"" << number(BOX_H)
            << "\" rx=\"7\"/>\n";
        out << "      <text class=\"label\" x=\"" << number(box.cx) << "\" y=\""
            << number(box.top + BOX_H / 2.0)
            << "\" text-anchor=\"middle\" dominant-baseline=\"central\">"
            << escapeXml(box.node->label) << "</text>\n";
        out << "    </g>\n";
    }

    out << "  </g>\n";
    out << "</svg>\n";

    return out.str();
}
