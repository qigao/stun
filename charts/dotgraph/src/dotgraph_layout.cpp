#include "dotgraph_layout.h"
#include "stun/graphlayout/layered.h"
#include "stun/graphlayout/polyline.h"

#include <stdexcept>
#include <vector>
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


namespace {
stun::graphlayout::Port dot_port(std::size_t node, DotGraphCompass compass,
                                DotGraphCompass fallback) {
    using stun::graphlayout::Side;
    const auto selected = compass == DG_COMPASS_NONE ? fallback : compass;
    switch (selected) {
    case DG_COMPASS_N: return {node, Side::North, 0.5};
    case DG_COMPASS_NE: return {node, Side::North, 1.0};
    case DG_COMPASS_E: return {node, Side::East, 0.5};
    case DG_COMPASS_SE: return {node, Side::South, 1.0};
    case DG_COMPASS_S: return {node, Side::South, 0.5};
    case DG_COMPASS_SW: return {node, Side::South, 0.0};
    case DG_COMPASS_W: return {node, Side::West, 0.5};
    case DG_COMPASS_NW: return {node, Side::North, 0.0};
    case DG_COMPASS_C:
        throw std::invalid_argument("graphlayout: DOT center port does not lie on the node boundary");
    default: throw std::invalid_argument("graphlayout: unsupported DOT compass");
    }
}
}

stun::graphlayout::Routes route_dot_edges(
    const DotGraphDiagram* diagram,
    const std::unordered_map<std::string, std::size_t>& index_of,
    const stun::graphlayout::Layout& placement) {
    using namespace stun::graphlayout;
    if (!diagram) throw std::invalid_argument("graphlayout: null DOT diagram");
    if (diagram->routing_mode != DG_ROUTE_ORTHOGONAL &&
        diagram->routing_mode != DG_ROUTE_POLYLINE)
        throw std::invalid_argument("graphlayout: unsupported DOT routing mode");
    if (diagram->routing_nudging_distance >= 0.0 ||
        diagram->routing_crossing_penalty >= 0.0 ||
        diagram->routing_angle_penalty >= 0.0 ||
        diagram->routing_nudge_orthogonal_ends >= 0 ||
        diagram->routing_nudge_shared_paths >= 0 ||
        (diagram->routing_mode == DG_ROUTE_POLYLINE &&
         diagram->routing_segment_penalty >= 0.0))
        throw std::invalid_argument("graphlayout: DOT native routing does not implement crossing/nudging/angle or polyline segment penalties");

    const bool horizontal = diagram->rankdir == DG_RANKDIR_LR ||
                            diagram->rankdir == DG_RANKDIR_RL;
    std::vector<RouteRequest> requests;
    for (const auto* edge = diagram->edges; edge; edge = edge->next) {
        if ((edge->from_port && edge->from_port[0]) ||
            (edge->to_port && edge->to_port[0]))
            throw std::invalid_argument("graphlayout: named DOT ports need explicit node geometry");
        const auto source = index_of.find(edge->from ? edge->from : "");
        const auto target = index_of.find(edge->to ? edge->to : "");
        if (source == index_of.end() || target == index_of.end() ||
            source->second >= placement.nodes.size() ||
            target->second >= placement.nodes.size())
            throw std::invalid_argument("graphlayout: DOT edge references missing node");
        const std::size_t u = source->second, v = target->second;
        const bool forward = horizontal ?
            placement.nodes[v].x >= placement.nodes[u].x :
            placement.nodes[v].y >= placement.nodes[u].y;
        const auto first_default = u == v ? DG_COMPASS_E :
            (horizontal ? (forward ? DG_COMPASS_E : DG_COMPASS_W) :
                          (forward ? DG_COMPASS_S : DG_COMPASS_N));
        const auto last_default = u == v ? DG_COMPASS_N :
            (horizontal ? (forward ? DG_COMPASS_W : DG_COMPASS_E) :
                          (forward ? DG_COMPASS_N : DG_COMPASS_S));
        requests.push_back({dot_port(u,edge->from_compass,first_default),
                            dot_port(v,edge->to_compass,last_default)});
    }
    Routes paths;
    RouteStatus status;
    if (diagram->routing_mode == DG_ROUTE_ORTHOGONAL) {
        RouteOptions options;
        if (diagram->routing_shape_buffer >= 0.0)
            options.clearance = diagram->routing_shape_buffer;
        if (diagram->routing_segment_penalty >= 0.0)
            options.bend_penalty = diagram->routing_segment_penalty;
        status = route_orthogonal(placement, requests, paths, options);
    } else {
        PolylineOptions options;
        if (diagram->routing_shape_buffer >= 0.0)
            options.clearance = diagram->routing_shape_buffer;
        status = route_polyline(placement, requests, paths, options);
    }
    if (!status)
        throw std::invalid_argument("graphlayout: DOT edge " +
            std::to_string(status.route_index) + ": " + status.message);
    return paths;
}

} // namespace dotgraph
