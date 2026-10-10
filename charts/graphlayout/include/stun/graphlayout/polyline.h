#pragma once

#include "stun/graphlayout/orthogonal.h" // Shared port, point, route and status contracts only.

#include <cstddef>
#include <vector>

namespace stun::graphlayout {

// Independent obstacle-avoiding polyline routing. Unlike orthogonal A*, the
// interior path may contain arbitrary straight-line segments. The objective is
// total Euclidean length on an explicit, bounded rectangle-corner visibility
// graph, not multi-edge global crossing/nudging quality.
struct PolylineOptions {
    double clearance = 8.0;
    std::size_t max_nodes = 256;
    std::size_t max_routes = 1024;
    std::size_t max_visibility_vertices = 2048; // Per route.
    std::size_t max_expansions = 2048;         // Per route.
    std::size_t max_segment_candidates = 4000000; // Per route.
    std::size_t max_obstacle_tests = 12000000;    // Per route.
    std::size_t max_queue_entries = 100000;      // Per route.
    std::size_t max_total_points = 250000;       // Whole output batch.
};

// Route requests preserve input order, including self-loops. Each route
// contains the two exact boundary anchors and two explicit outward stubs.
// Failure clears the entire batch; there is no libavoid or orthogonal fallback.
RouteStatus route_polyline(const Layout& layout,
                           const std::vector<RouteRequest>& requests,
                           Routes& out,
                           const PolylineOptions& options = {});

// Independently recompute anchors/stubs, check finite, nonzero line segments
// against original and clearance-inflated rectangles and enforce port exit
// directions. This is a geometric postcondition, not an optimality proof.
RouteStatus validate_polyline_routes(const Layout& layout,
                                      const std::vector<RouteRequest>& requests,
                                      const Routes& routes,
                                      const PolylineOptions& options = {});

} // namespace stun::graphlayout
