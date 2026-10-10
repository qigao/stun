#pragma once

#include "dotgraph/dotgraph_ast.h"
#include "stun/graphlayout/graph.h"
#include "stun/graphlayout/orthogonal.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace dotgraph {

// DOT-specific graph semantics and rank direction. The solver remains shared.
// Caller measures widths/heights and retains ownership of AST nodes.
// The returned placement preserves the caller's ordered node indices.
stun::graphlayout::Layout place_dot_nodes(
    const DotGraphDiagram* diagram,
    const std::vector<DotGraphNode*>& nodes,
    const std::unordered_map<std::string, std::size_t>& index_of,
    const std::vector<double>& widths,
    const std::vector<double>& heights,
    double node_gap, double layer_gap);

// Ordered source/target ports, routing policy and typed failure belong to the
// DOT adapter, not to the renderer. The returned route order is AST edge order.
// Native Polyline and Orthogonal are explicitly selected; no libavoid fallback.
stun::graphlayout::Routes route_dot_edges(
    const DotGraphDiagram* diagram,
    const std::unordered_map<std::string, std::size_t>& index_of,
    const stun::graphlayout::Layout& placement);

} // namespace dotgraph
