#include "dotgraph_layout.h"
#include "stun/graphlayout/layered.h"
#include "stun/graphlayout/polyline.h"
#include "stun/graphlayout/polyline_nudging.h"
#include "stun/graphlayout/nudging.h"
#include "stun/graphlayout/port_bindings.h"

#include <cmath>
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

stun::graphlayout::Side measured_side(DotGraphCompass side) {
    using stun::graphlayout::Side;
    switch (side) {
    case DG_COMPASS_N: return Side::North;
    case DG_COMPASS_E: return Side::East;
    case DG_COMPASS_S: return Side::South;
    case DG_COMPASS_W: return Side::West;
    default:
        throw std::invalid_argument("graphlayout: named DOT measurement requires cardinal boundary side");
    }
}

bool named_compass_matches(const stun::graphlayout::Port& port,
                           DotGraphCompass compass) {
    using stun::graphlayout::Side;
    constexpr double epsilon = 1e-12;
    switch (compass) {
    case DG_COMPASS_NONE: return true;
    case DG_COMPASS_N: return port.side == Side::North;
    case DG_COMPASS_E: return port.side == Side::East;
    case DG_COMPASS_S: return port.side == Side::South;
    case DG_COMPASS_W: return port.side == Side::West;
    case DG_COMPASS_NE:
        return (port.side == Side::North && std::abs(port.offset - 1.0) < epsilon) ||
               (port.side == Side::East && std::abs(port.offset) < epsilon);
    case DG_COMPASS_SE:
        return (port.side == Side::East && std::abs(port.offset - 1.0) < epsilon) ||
               (port.side == Side::South && std::abs(port.offset - 1.0) < epsilon);
    case DG_COMPASS_SW:
        return (port.side == Side::South && std::abs(port.offset) < epsilon) ||
               (port.side == Side::West && std::abs(port.offset - 1.0) < epsilon);
    case DG_COMPASS_NW:
        return (port.side == Side::North && std::abs(port.offset) < epsilon) ||
               (port.side == Side::West && std::abs(port.offset) < epsilon);
    case DG_COMPASS_C: return false;
    default: return false;
    }
}

stun::graphlayout::Port resolve_dot_port(
    std::size_t node, const char* name, DotGraphCompass compass,
    DotGraphCompass fallback, const stun::graphlayout::NamedPortBindings& bindings) {
    if (!name || !name[0]) return dot_port(node, compass, fallback);
    const auto* bound = stun::graphlayout::find_named_port(bindings, node, name);
    if (!bound)
        throw std::invalid_argument("graphlayout: DOT named port '" + std::string(name) +
                                    "' has no measured boundary geometry");
    if (!named_compass_matches(bound->port, compass))
        throw std::invalid_argument("graphlayout: DOT named port compass conflicts with measured side/corner");
    return bound->port;
}
}

stun::graphlayout::Routes route_dot_edges(
    const DotGraphDiagram* diagram,
    const std::unordered_map<std::string, std::size_t>& index_of,
    const stun::graphlayout::Layout& placement,
    const std::vector<MeasuredDotPort>& measured_ports) {
    using namespace stun::graphlayout;
    if (!diagram) throw std::invalid_argument("graphlayout: null DOT diagram");
    if (diagram->routing_mode != DG_ROUTE_ORTHOGONAL &&
        diagram->routing_mode != DG_ROUTE_POLYLINE)
        throw std::invalid_argument("graphlayout: unsupported DOT routing mode");
    if (diagram->routing_crossing_penalty >= 0.0 ||
        diagram->routing_angle_penalty >= 0.0 ||
        diagram->routing_nudge_orthogonal_ends >= 0 ||
        diagram->routing_nudge_shared_paths >= 0 ||
        (diagram->routing_mode == DG_ROUTE_POLYLINE &&
         diagram->routing_segment_penalty >= 0.0))
        throw std::invalid_argument("graphlayout: DOT native routing does not implement crossing/angle/terminal nudging or polyline segment penalty");

    // Translate actual Chart-measured local node ports into the shared
    // rectangle-boundary routing contract. Geometry is validated atomically.
    std::vector<MeasuredNamedPort> measurements;
    measurements.reserve(measured_ports.size());
    for (const auto& m : measured_ports) {
        const auto it = index_of.find(m.node_id);
        if (it == index_of.end() || it->second >= placement.nodes.size())
            throw std::invalid_argument("graphlayout: DOT named port references a missing node");
        measurements.push_back({it->second, m.name, measured_side(m.outward_side),
                                {m.local_x, m.local_y}});
    }
    NamedPortBindings bindings;
    const auto binding_status = bind_named_ports(placement, measurements, bindings);
    if (!binding_status)
        throw std::invalid_argument("graphlayout: DOT measured port " +
            std::to_string(binding_status.measurement_index) + ": " + binding_status.message);

    const bool horizontal = diagram->rankdir == DG_RANKDIR_LR ||
                            diagram->rankdir == DG_RANKDIR_RL;
    std::vector<RouteRequest> requests;
    for (const auto* edge = diagram->edges; edge; edge = edge->next) {
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
        requests.push_back({resolve_dot_port(u, edge->from_port, edge->from_compass,
                                             first_default, bindings),
                            resolve_dot_port(v, edge->to_port, edge->to_compass,
                                             last_default, bindings)});
    }
    Routes paths;
    RouteStatus status;
    if (diagram->routing_mode == DG_ROUTE_ORTHOGONAL) {
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
        if (diagram->routing_nudging_distance > 0.0) {
            PolylineNudgingOptions joint;
            joint.routing = options;
            joint.lane_spacing = diagram->routing_nudging_distance;
            status = route_polyline_nudged(placement, requests, paths, joint);
        } else {
            // Missing/zero nudging requests only the shortest individual paths.
            status = route_polyline(placement, requests, paths, options);
        }
    }
    if (!status)
        throw std::invalid_argument("graphlayout: DOT edge " +
            std::to_string(status.route_index) + ": " + status.message);
    return paths;
}

} // namespace dotgraph
