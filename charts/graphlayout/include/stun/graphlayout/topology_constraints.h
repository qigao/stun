#pragma once

#include "stun/graphlayout/orthogonal.h"

#include <cstddef>
#include <string>
#include <vector>

namespace stun::graphlayout {

// A scalar follows initial + alpha*(desired-initial), 0 <= alpha <= 1.
struct TopologyScalarTrack {
    double initial = 0.0;
    double desired = 0.0;
};

struct TopologyPointTrack {
    TopologyScalarTrack x, y;
};

// Stun-owned analogue of an Adaptagrams TriConstraint. For below=true,
//       (1-p)*u + p*v - w >= gap;
// for below=false the inequality is reversed. All positions are affine in
// alpha; this makes the first violation analytically computable, not sampled.
struct TopologyTriConstraint {
    TopologyScalarTrack u, v, w;
    double p = 0.5;
    double gap = 0.0;
    bool below = true;
};

// Explicit node/segment StraightConstraint, with a scan-line material
// fraction and the supporting node face encoded as a TriConstraint.
// It is a local linear separation guard, not a full swept-shape certificate.
struct TopologyStraightConstraint {
    std::size_t node_index = 0;
    std::size_t edge_index = 0;
    std::size_t segment_index = 0;
    TopologyTriConstraint separation;
};

// Keeps a non-straight bend's signed area from vanishing or changing sign.
// The three tracked points move affinely; the signed area is a quadratic in
// alpha, so all roots within [0,1] can be checked analytically.
struct TopologyBendConstraint {
    TopologyPointTrack first, bend, last;
};

struct TopologyGuardOptions {
    double coordinate_limit = 1e8;
    // Strictly positive extra distance between unrelated node face and a
    // material position on a segment. Zero is allowed, but tangencies are
    // still rejected by the independent topology audit.
    double node_segment_clearance = 0.0;
    // Fraction of baseline signed bend area preserved throughout movement.
    double min_bend_area_ratio = 1e-8;
    // Stay a controlled distance before the first analytic constraint event.
    double event_backoff = 1e-5;
    std::size_t max_constraints = 32768;
    std::size_t max_evaluations = 2500000;
    std::size_t max_nodes = 256;
    std::size_t max_routes = 512;
    std::size_t max_route_points = 256;
};

enum class TopologyGuardError {
    None,
    InvalidInput,
    InvalidOptions,
    InvalidGeometry,
    InfeasibleBaseline,
    CapacityExceeded,
};

struct TopologyGuardStatus {
    TopologyGuardError error = TopologyGuardError::None;
    std::string message;
    explicit operator bool() const { return error == TopologyGuardError::None; }
};

struct TopologyGuardReport {
    double max_safe_fraction = 1.0;
    std::size_t straight_constraints = 0;
    std::size_t bend_constraints = 0;
    std::size_t segment_pair_constraints = 0;
    std::size_t evaluations = 0;
};

// Compute the strict safe prefix for a single affine tri-constraint.
// No partial result is published on error; if baseline violates the
// constraint it returns InfeasibleBaseline rather than switching solvers.
TopologyGuardStatus limit_topology_tri(const TopologyTriConstraint& constraint,
                                       double& max_safe_fraction,
                                       const TopologyGuardOptions& options = {});

TopologyGuardStatus limit_topology_straight(const TopologyStraightConstraint& constraint,
                                            double& max_safe_fraction,
                                            const TopologyGuardOptions& options = {});

// Preserve the sign of a genuine three-point bend over the entire prefix.
// Baseline-collinear triples have no bend sign and impose no sign guard.
TopologyGuardStatus limit_topology_bend(const TopologyBendConstraint& constraint,
                                        double& max_safe_fraction,
                                        const TopologyGuardOptions& options = {});

// Construct topology guards from the exact same affine route-point displacement
// model as move_topology_preserving(): source/target translations are blended
// by original route arclength. For every unrelated node and segment whose
// scan projection lies inside the segment, generate a fixed-fraction
// node-face/segment TriConstraint. Consecutive route triples generate bend
// orientation guards. For every pair of independent edge segments, each of
// the four endpoint-vs-segment orientation triples also generates a quadratic
// event guard. This detects hidden two-segment swaps even if a sampled audit
// observes only the final frame. The minimum safe fraction limits a later
// full topology audit; it is NOT an ambient isotopy guarantee. In particular,
// a moving scan intersection may shift along a segment and must be checked
// by the independent full geometry/topology validator.
TopologyGuardStatus limit_topology_movement(
    const Graph& graph, const Layout& baseline, const Layout& desired,
    const Routes& routes, TopologyGuardReport& report,
    const TopologyGuardOptions& options = {});

} // namespace stun::graphlayout
