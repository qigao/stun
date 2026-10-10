#include "dotgraph_layout.h"
#include "stun/graphlayout/layered.h"

#include <stdexcept>
#include <string>

namespace dotgraph {

stun::graphlayout::Layout place_dot_nodes(
    const DotGraphDiagram* diagram,
    const std::vector<DotGraphNode*>& nodes,
    const std::unordered_map<std::string, std::size_t>& index_of,
    const std::vector<double>& widths,
    const std::vector<double>& heights,
    double node_gap, double layer_gap) {
    if (!diagram || nodes.size() != widths.size() || nodes.size() != heights.size())
        throw std::invalid_argument("dotgraph: mismatched graph placement inputs");

    stun::graphlayout::Graph graph;
    graph.nodes.reserve(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        if (!nodes[i] || !nodes[i]->id)
            throw std::invalid_argument("dotgraph: unnamed layout node");
        graph.nodes.push_back({nodes[i]->id, widths[i], heights[i]});
    }
    for (const auto* edge = diagram->edges; edge; edge = edge->next) {
        const auto source = index_of.find(edge->from ? edge->from : "");
        const auto target = index_of.find(edge->to ? edge->to : "");
        if (source == index_of.end() || target == index_of.end())
            throw std::invalid_argument("graphlayout: DOT edge references missing node");
        graph.edges.push_back({source->second, target->second});
    }

    stun::graphlayout::Options options;
    options.node_gap = node_gap;
    options.layer_gap = layer_gap;
    switch (diagram->rankdir) {
    case DG_RANKDIR_TB: options.direction = stun::graphlayout::Direction::TopToBottom; break;
    case DG_RANKDIR_BT: options.direction = stun::graphlayout::Direction::BottomToTop; break;
    case DG_RANKDIR_LR: options.direction = stun::graphlayout::Direction::LeftToRight; break;
    case DG_RANKDIR_RL: options.direction = stun::graphlayout::Direction::RightToLeft; break;
    default: throw std::invalid_argument("graphlayout: unsupported DOT rank direction");
    }
    stun::graphlayout::Layout placement;
    const auto result = stun::graphlayout::layout_layered(graph, placement, options);
    if (!result) throw std::invalid_argument("graphlayout: " + result.message);
    return placement;
}

} // namespace dotgraph
