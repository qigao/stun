#pragma once

#include "stun/graphlayout/orthogonal.h"

#include <cstddef>

namespace stun::graphlayout {

// Bounded multi-edge post-routing optimizer for *orthogonal* paths. It changes
// only routes whose geometry can be independently revalidated; anchors remain
// exact. This is a deterministic local improvement, not a global libavoid
// crossing/nudging replacement, and it does not apply to arbitrary polylines.
struct NudgingOptions {
    RouteOptions routing;
    double lane_spacing = 12.0;
    std::size_t max_routes = 128;
    std::size_t max_passes = 2;
    std::size_t max_lanes = 3;
    std::size_t max_candidates = 16000;
    std::size_t max_segment_pair_checks = 3000000;
    std::size_t max_points_per_route = 512;
    std::size_t max_total_points = 32768;
};

struct RouteInteractions {
    std::size_t crossings = 0;
    double shared_length = 0.0; // Sum of exact collinear overlap lengths.
};

struct NudgingReport {
    RouteInteractions initial;
    RouteInteractions final;
    std::size_t changed_routes = 0;
    std::size_t accepted_moves = 0;
    std::size_t candidate_trials = 0;
    std::size_t segment_pair_checks = 0;
};

// Audit pairwise intersections between routes. Proper (interior/interior)
// orthogonal crossings and collinear overlap have distinct measurements.
// Caller-supplied geometry is independently verified and resource-bounded.
// On error, 'out' is cleared; no partial audit is published.
RouteStatus audit_orthogonal_interactions(const Layout& layout,
                                          const std::vector<RouteRequest>& requests,
                                          const Routes& routes,
                                          RouteInteractions& out,
                                          const NudgingOptions& options = {});

// Generate native orthogonal routes, then greedily improve the *whole batch*:
// explore signed lane doglegs along each segment, keeping the best candidate
// that lexicographically improves (crossings, shared_length), with shorter
// length breaking ties. Unavoidable shared terminal stubs may remain.
// Every trial must pass the independent orthogonal geometric validator; the
// batch is atomic and all work/space budgets fail explicitly. No libavoid,
// hidden retries, silent relaxation, or straight-line fallback.
RouteStatus route_orthogonal_nudged(const Layout& layout,
                                    const std::vector<RouteRequest>& requests,
                                    Routes& out,
                                    const NudgingOptions& options = {},
                                    NudgingReport* report = nullptr);

} // namespace stun::graphlayout
