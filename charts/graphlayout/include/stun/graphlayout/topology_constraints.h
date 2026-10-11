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
// alpha, expanded directly from relative positions/displacements rather than
// reconstructed from distant area samples. Roots in [0,1] are checked numerically.
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
    // Nonzero orientation guards, disjoint-collinear interval guards and
    // adjacent proper-crossing order gaps on each carrier segment.
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
// Determinants compensate product cancellation with the same private numeric
// primitive as the auditor and edit eligibility; this is not an exact predicate.
TopologyGuardStatus limit_topology_bend(const TopologyBendConstraint& constraint,
                                        double& max_safe_fraction,
                                        const TopologyGuardOptions& options = {});

// Construct topology guards from the exact same affine route-point displacement
// model as move_topology_preserving(): source/target translations are blended
// by original route arclength. All node pairs have continuous open-rectangle
// collision guards, including isolated nodes; boundary-only contact is allowed.
// Each pair consumes one constraint/evaluation from the shared budgets, with
// O(nodes^2) time and O(1) auxiliary storage for this part of the guard.
// Every internal route waypoint also has a continuous open-interior collision
// guard against every node, including its source and target. Each waypoint/node
// pair consumes one shared constraint/evaluation, taking O(nodes * internal-
// waypoints) time and O(1) additional storage. Boundary-only contact is allowed;
// baseline interior penetration is InfeasibleBaseline. Terminal anchors stay
// on their own nodes, and node-pair guards cover collisions with other nodes.
// Waypoint checks contribute to evaluations rather than Straight/Bend counts.
// Every segment/node pair, including incident nodes, also has a continuous
// open-interior guard. Eight affine endpoint/face events and four quadratic
// corner/line events partition [0,1]; separating-axis tests classify each
// open interval. Boundary-only tangencies remain admissible. Each pair consumes
// one constraint and 1..30 evaluations (initial check, up to 12 polynomials and
// 17 intervals); this phase is O(nodes * segments) time and O(1) extra storage,
// with at most 18 event times per pair. These checks contribute to evaluations
// rather than the local Straight/Bend counters. Counts can vary with distinct
// root times. This is a floating-point numerical guard, not an exact certificate.
// For every unrelated node and segment whose
// scan projection lies inside the segment, generate a fixed-fraction
// node-face/segment TriConstraint. Consecutive route triples generate bend
// orientation guards. For every pair of independent segments, including
// nonadjacent segments of the same route, each of
// the four endpoint-vs-segment orientation triples with nonzero baseline area
// generates a quadratic event guard. Baseline-collinear disjoint pairs retain
// strict interval separation on the first segment's dominant axis by checking
// all four affine endpoint gaps. This is conservative if transverse movement
// would permit a longer step. Each fully collinear pair consumes one additional
// constraint/evaluation, including baseline touching/overlapping pairs that
// are left to the full audit. The report counts each disjoint interval guard
// once. Segment-pair checks take O(segment-pair-count) time and O(1) additional
// storage beyond the motion tracks. Adjacent segments use bend guards instead.
// Proper baseline crossings are ordered on each carrier segment. Adjacent
// crossing-parameter differences have quartic numerators; Bernstein control
// hull subdivision retains their strict baseline order, including triple
// contacts without a sign reversal. Each order gap consumes one constraint,
// one construction evaluation and one evaluation per visited subdivision.
// The first interval unresolved after double-fraction precision subdivision
// bounds the step conservatively; evaluation exhaustion returns CapacityExceeded.
// A coefficient-scaled machine-precision margin can also shorten near-degenerate
// motion. Storage is O(C + double precision bits) for C directed crossings,
// bounded by the pair-constraint budget; sorting takes O(C log C), subsequent
// subdivision O(consumed evaluations). No exact-arithmetic guarantee is made.
// This detects hidden two-segment swaps and self contacts even if a sampled audit
// observes only the final frame. The minimum safe fraction limits a later
// full topology audit; it is NOT an ambient isotopy guarantee. In particular,
// a moving scan intersection may shift along a segment and must be checked
// by the independent full geometry/topology validator.
TopologyGuardStatus limit_topology_movement(
    const Graph& graph, const Layout& baseline, const Layout& desired,
    const Routes& routes, TopologyGuardReport& report,
    const TopologyGuardOptions& options = {});

} // namespace stun::graphlayout
