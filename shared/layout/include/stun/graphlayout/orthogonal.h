#pragma once

#include "stun/graphlayout/graph.h"

#include <cstddef>
#include <string>
#include <vector>

namespace stun::graphlayout {

struct Point {
    double x = 0.0;
    double y = 0.0;
};

// Side is the outward-facing side of the node, for both source and target.
// Auto uses the relative node centers, with a stable self-loop convention.
enum class Side { Auto, North, East, South, West };

struct Port {
    std::size_t node = 0;
    Side side = Side::Auto;
    // Fraction along N/S from left to right, or E/W from top to bottom.
    // Both 0 and 1 are allowed; corners still leave along the selected side.
    double offset = 0.5;
};

struct RouteRequest {
    Port source;
    Port target;
};

struct Route {
    std::vector<Point> points; // inclusive boundary anchors, without duplicates
};

struct Routes {
    // Output order is identical to the input RouteRequest order.
    std::vector<Route> edges;
};

struct RouteOptions {
    // Every non-terminal segment stays outside the *interior* of rectangles
    // expanded by clearance. Terminal stubs may cross their own expanded box.
    double clearance = 8.0;
    double bend_penalty = 18.0;
    std::size_t max_nodes = 256;
    std::size_t max_routes = 1024;
    std::size_t max_grid_vertices = 250000;
    std::size_t max_expansions = 350000; // per route
    std::size_t max_queue_entries = 900000; // per route
};

enum class RouteError {
    None,
    InvalidInput,
    InvalidOptions,
    CapacityExceeded,
    NoPath,
    InternalInvariant,
};

struct RouteStatus {
    RouteError error = RouteError::None;
    std::string message;
    std::size_t route_index = 0;
    explicit operator bool() const { return error == RouteError::None; }
};

// Orthogonal compressed-visibility grid + direction-aware A*.
// Nodes are obstacles. Failure clears 'out', with no straight-line fallback.
// Path search, port anchors, route order and tie breaking are deterministic.
RouteStatus route_orthogonal(const Layout& layout,
                             const std::vector<RouteRequest>& requests,
                             Routes& out,
                             const RouteOptions& options = {});

// Independently inspect full returned polylines, including anchor and port
// direction, nonzero orthogonal segments, and collisions with node rectangles.
// This is a geometric postcondition, not a claim of global multi-edge quality.
RouteStatus validate_orthogonal_routes(const Layout& layout,
                                       const std::vector<RouteRequest>& requests,
                                       const Routes& routes,
                                       const RouteOptions& options = {});

} // namespace stun::graphlayout
