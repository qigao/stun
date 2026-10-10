#pragma once

#include "dotgraph/dotgraph_ast.h"
#include "stun/graphlayout/graph.h"

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

} // namespace dotgraph
