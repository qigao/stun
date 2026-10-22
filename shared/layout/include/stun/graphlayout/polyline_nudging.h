#pragma once

#include "stun/graphlayout/polyline.h"

#include <cstddef>

namespace stun::graphlayout {

// Deterministic, bounded multi-edge *polyline* post-optimization. This is not
// orthogonal segment nudging: routes can contain arbitrary straight segments.
// Each candidate is regenerated through exactly one explicit checkpoint and
// independently revalidated against inflated node obstacles and port stubs.
struct PolylineNudgingOptions {
    PolylineOptions routing;
    double lane_spacing = 12.0;
    std::size_t max_routes = 128;
    std::size_t max_passes = 2;
    std::size_t max_lanes = 2;
    std::size_t max_candidates = 6000;
    std::size_t max_segment_pair_checks = 2000000;
    std::size_t max_points_per_route = 512;
    std::size_t max_total_points = 32768;
};

struct PolylineInteractions {
    std::size_t proper_crossings = 0;
    double shared_length = 0.0;
};

struct PolylineNudgingReport {
    PolylineInteractions initial;
    PolylineInteractions final;
    std::size_t changed_routes = 0;
    std::size_t accepted_moves = 0;
    std::size_t candidate_trials = 0;
    std::size_t segment_pair_checks = 0;
};

// Checks all input paths geometrically before independently counting pairwise
// proper crossings and collinear overlap. A touch at a shared endpoint is
// not a proper crossing; entire overlapping segments count their true length.
// Output cleared on error; all pair checks share the supplied operation cap.
RouteStatus audit_polyline_interactions(const Layout& layout,
    const std::vector<RouteRequest>& requests, const Routes& paths,
    PolylineInteractions& out, const PolylineNudgingOptions& options = {});

// First route using native Polyline visibility A*, then bounded greedy
// single-checkpoint lane reroutes. Accept only strictly better lexicographic
// (proper crossings, collinear overlap length) metrics; routing/geometry
// validity is rechecked for every accepted candidate. Endpoints remain exact.
// All budgets are explicit; any capacity failure clears the entire batch,
// without libavoid, orthogonal or arbitrary-straight fallback.
RouteStatus route_polyline_nudged(const Layout& layout,
    const std::vector<RouteRequest>& requests, Routes& out,
    const PolylineNudgingOptions& options = {},
    PolylineNudgingReport* report = nullptr);

} // namespace stun::graphlayout
