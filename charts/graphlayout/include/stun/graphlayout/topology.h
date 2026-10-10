#pragma once

#include "stun/graphlayout/orthogonal.h"

#include <cstddef>
#include <string>
#include <vector>

namespace stun::graphlayout {

// Pairwise proper intersections of oriented graph edges, sorted by arclength
// along each edge. A crossing's sign is relative to the edge being inspected.
// Equal signatures preserve edge-pair identity, crossing orientation and the
// ordered intersection sequence along every edge, not just crossing counts.
struct TopologyCrossing {
    std::size_t other_edge = 0;
    int sign = 0;
};

struct TopologySignature {
    std::vector<std::vector<TopologyCrossing>> ordered_crossings;
    std::size_t crossing_count = 0;
};

struct TopologyOptions {
    // Coordinates are bounded to protect orientation and interpolation math.
    double coordinate_limit = 1.0e8;
    // Relative orientation tolerance; borderline/tangent arrangements fail
    // as ambiguous rather than claiming a stable topology.
    double orientation_epsilon = 1.0e-12;
    std::size_t max_nodes = 256;
    std::size_t max_edges = 512;
    std::size_t max_points_per_route = 256;
    std::size_t max_total_points = 32768;
    std::size_t max_segment_checks = 2500000; // Across baseline and ALL trials.
    std::size_t max_trials = 16; // Candidate distance fractions: 1, 1/2, ...
    std::size_t samples_per_trial = 8; // Inspect the entire interpolation prefix.
    // Analytically cap the candidate motion at local node-segment and bend
    // orientation events before the sampled full-topology continuation.
    bool analytic_guards = true;
    std::size_t max_guard_constraints = 32768;
    std::size_t max_guard_evaluations = 2500000;
    double guard_clearance = 0.0;
    double min_bend_area_ratio = 1e-8;
    double guard_backoff = 1e-5;
};

enum class TopologyError {
    None,
    InvalidOptions,
    InvalidInput,
    InvalidGeometry,
    AmbiguousTopology,
    CapacityExceeded,
    NoAdmissibleStep,
    InternalInvariant,
};

struct TopologyStatus {
    TopologyError error = TopologyError::None;
    std::string message;
    std::size_t edge_index = 0;
    explicit operator bool() const { return error == TopologyError::None; }
};

struct TopologyReport {
    double accepted_fraction = 0.0;
    std::size_t baseline_crossings = 0;
    std::size_t trials = 0;
    std::size_t sampled_frames = 0;
    std::size_t segment_checks = 0;
    double analytic_guard_fraction = 1.0;
    std::size_t straight_constraints = 0;
    std::size_t bend_constraints = 0;
    std::size_t segment_pair_constraints = 0;
    std::size_t guard_evaluations = 0;
};

// Renderer-neutral audit of a straight-segment *polyline embedding*.
// All endpoint ports must specify a compass Side rather than Auto. Node
// rectangles may not overlap; routes may not penetrate any node interior.
// Proper edge intersections are recorded in order. Non-endpoint touches,
// segment overlaps and self-intersections are ambiguous, not silently ignored.
// Self-loop edges are deliberately unsupported by this initial audit.
// Output is cleared on failure. The numerical signature is not a proof of
// ambient isotopy or continuous topological equivalence.
TopologyStatus audit_topology(const Graph& graph, const Layout& layout,
                              const std::vector<RouteRequest>& ports,
                              const Routes& routes, TopologySignature& out,
                              const TopologyOptions& options = {});

// Conservative bounded continuation: linearly interpolate node rectangles
// toward 'desired'. Each original route waypoint is moved by a deterministic,
// arclength-weighted blend of its source/target-node displacements. Each trial
// first analytically caps motion at local node/segment and bend events, then
// checks intermediate sampled frames against the exact baseline crossing
// signature, segment contacts and node collisions. An accepted result need
// not reach the requested target; report.accepted_fraction states its extent.
// This does NOT certify all unsampled instants of the deformation and does not
// substitute for arbitrary obstacle-aware rerouting or topology homotopy proof.
// On failure, layouts, routes and report are reset with no fallback.
TopologyStatus move_topology_preserving(
    const Graph& graph, const Layout& baseline,
    const std::vector<RouteRequest>& ports, const Routes& original_routes,
    const Layout& desired, Layout& out_layout, Routes& out_routes,
    const TopologyOptions& options = {}, TopologyReport* report = nullptr);

} // namespace stun::graphlayout
