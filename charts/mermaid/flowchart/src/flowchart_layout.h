#pragma once

#include "flowchart/flowchart_ast.h"
#include "stun/graphlayout/graph.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace mermaid::flowchart {

// Mermaid flowchart AST and direction adapter, independent of the renderer.
stun::graphlayout::Layout place_flowchart_nodes(
    const FlowchartDiagram* diagram,
    const std::vector<FlowchartNode*>& nodes,
    const std::unordered_map<std::string, std::size_t>& index_of,
    const std::vector<double>& widths,
    const std::vector<double>& heights,
    double node_gap, double layer_gap);

} // namespace mermaid::flowchart
