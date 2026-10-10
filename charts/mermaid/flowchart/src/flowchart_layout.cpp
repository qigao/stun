#include "flowchart_layout.h"
#include "stun/graphlayout/layered.h"

#include <stdexcept>
#include <string>

namespace mermaid::flowchart {

stun::graphlayout::Layout place_flowchart_nodes(
    const FlowchartDiagram* diagram,
    const std::vector<FlowchartNode*>& nodes,
    const std::unordered_map<std::string, std::size_t>& index_of,
    const std::vector<double>& widths,
    const std::vector<double>& heights,
    double node_gap, double layer_gap) {
    if (!diagram || nodes.size() != widths.size() || nodes.size() != heights.size())
        throw std::invalid_argument("flowchart: mismatched graph placement inputs");

    stun::graphlayout::Graph graph;
    graph.nodes.reserve(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (!nodes[i] || !nodes[i]->id)
            throw std::invalid_argument("flowchart: unnamed layout node");
        graph.nodes.push_back({nodes[i]->id, widths[i], heights[i]});
    }
    for (const auto* edge = diagram->edges; edge; edge = edge->next) {
        const auto source = index_of.find(edge->from ? edge->from : "");
        const auto target = index_of.find(edge->to ? edge->to : "");
        if (source == index_of.end() || target == index_of.end())
            throw std::invalid_argument("graphlayout: Mermaid edge references missing node");
        graph.edges.push_back({source->second, target->second});
    }

    stun::graphlayout::Options options;
    options.node_gap = node_gap;
    options.layer_gap = layer_gap;
    const std::string rankdir = diagram->direction ? diagram->direction : "";
    if (rankdir.empty() || rankdir == "TB" || rankdir == "TD")
        options.direction = stun::graphlayout::Direction::TopToBottom;
    else if (rankdir == "BT") options.direction = stun::graphlayout::Direction::BottomToTop;
    else if (rankdir == "LR") options.direction = stun::graphlayout::Direction::LeftToRight;
    else if (rankdir == "RL") options.direction = stun::graphlayout::Direction::RightToLeft;
    else throw std::invalid_argument("graphlayout: unsupported Mermaid flowchart direction");

    stun::graphlayout::Layout placement;
    const auto result = stun::graphlayout::layout_layered(graph, placement, options);
    if (!result) throw std::invalid_argument("graphlayout: " + result.message);
    return placement;
}

} // namespace mermaid::flowchart
