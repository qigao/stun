#pragma once

#include <mindmap/mindmap_ast.h>
#include <stun/graphlayout/tidy_tree.h>

#include <functional>
#include <utility>
#include <vector>

namespace mermaid::mindmap {

// Geometry is supplied by the Chart/text-measurement consumer, never guessed
// from UTF-8 byte lengths by the graph algorithm.
using MeasureNode = std::function<std::pair<double,double>(const MindmapNode&)>;

struct Layout {
    // Index is preorder traversal of the source sibling/child links.
    std::vector<const MindmapNode*> nodes;
    stun::graphlayout::TidyTreeLayout geometry;
};

// Build an ordered forest from the existing Mindmap AST without depending on
// its parser, SVG renderer, UI framework, or host graphics backend.
// Failure clears the output; no fallback to a different graph algorithm.
stun::graphlayout::TidyTreeStatus layout_mindmap(const MindmapDiagram& diagram,
    const MeasureNode& measure, Layout& out,
    const stun::graphlayout::TidyTreeOptions& options = {});

} // namespace mermaid::mindmap
