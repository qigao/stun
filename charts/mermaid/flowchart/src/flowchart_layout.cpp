#include "flowchart_layout.h"
#include "stun/graphlayout/layered.h"
#include "stun/graphlayout/polyline.h"
#include "stun/graphlayout/nudging.h"

#include <stdexcept>
#include <vector>
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


stun::graphlayout::Routes route_flowchart_edges(
    const FlowchartDiagram* diagram,
    const std::unordered_map<std::string, std::size_t>& index_of,
    const stun::graphlayout::Layout& placement) {
    using namespace stun::graphlayout;
    if (!diagram) throw std::invalid_argument("graphlayout: null Mermaid flowchart");
    if (diagram->routing_mode != FC_ROUTE_ORTHOGONAL &&
        diagram->routing_mode != FC_ROUTE_POLYLINE)
        throw std::invalid_argument("graphlayout: unsupported Mermaid routing mode");
    if (diagram->routing_crossing_penalty >= 0.0 ||
        diagram->routing_angle_penalty >= 0.0 ||
        diagram->routing_nudge_orthogonal_ends >= 0 ||
        diagram->routing_nudge_shared_paths >= 0 ||
        (diagram->routing_mode == FC_ROUTE_POLYLINE &&
         (diagram->routing_segment_penalty >= 0.0 ||
          diagram->routing_nudging_distance >= 0.0)))
        throw std::invalid_argument("graphlayout: Mermaid native routing does not implement crossing/angle/terminal nudging or polyline segment/nudging penalties");
    std::vector<RouteRequest> requests;
    for (const auto* edge = diagram->edges; edge; edge = edge->next) {
        const auto first = index_of.find(edge->from ? edge->from : "");
        const auto second = index_of.find(edge->to ? edge->to : "");
        if (first == index_of.end() || second == index_of.end() ||
            first->second >= placement.nodes.size() ||
            second->second >= placement.nodes.size())
            throw std::invalid_argument("graphlayout: Mermaid edge references missing node");
        requests.push_back({{first->second}, {second->second}});
    }
    Routes paths;
    RouteStatus status;
    if (diagram->routing_mode == FC_ROUTE_ORTHOGONAL) {
        RouteOptions options;
        if (diagram->routing_shape_buffer >= 0.0)
            options.clearance = diagram->routing_shape_buffer;
        if (diagram->routing_segment_penalty >= 0.0)
            options.bend_penalty = diagram->routing_segment_penalty;
        if (diagram->routing_nudging_distance > 0.0) {
            NudgingOptions joint;
            joint.routing = options;
            joint.lane_spacing = diagram->routing_nudging_distance;
            status = route_orthogonal_nudged(placement, requests, paths, joint);
        } else {
            // Absent or explicitly zero nudging distance: direct orthogonal A*.
            status = route_orthogonal(placement, requests, paths, options);
        }
    } else {
        PolylineOptions options;
        if (diagram->routing_shape_buffer >= 0.0)
            options.clearance = diagram->routing_shape_buffer;
        status = route_polyline(placement, requests, paths, options);
    }
    if (!status)
        throw std::invalid_argument("graphlayout: Mermaid edge " +
            std::to_string(status.route_index) + ": " + status.message);
    return paths;
}

} // namespace mermaid::flowchart
