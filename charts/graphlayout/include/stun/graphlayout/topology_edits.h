#pragma once

#include "stun/graphlayout/topology.h"

#include <cstddef>
#include <vector>

namespace stun::graphlayout {

// A deliberately explicit subset of Adaptagrams-like Straight <-> Bend
// topology events, separate from the graph movement and constraint solvers.
// Edits use the route's CURRENT indices after all earlier edits in the batch.
// No automatically guessed bend position or implicit reroute is permitted.
enum class TopologyEditKind { StraightToBend, BendToStraight };

struct TopologyEdit {
    TopologyEditKind kind = TopologyEditKind::StraightToBend;
    std::size_t edge_index = 0;
    // StraightToBend: index of segment [point_index, point_index + 1].
    // BendToStraight: index of an internal waypoint to remove.
    std::size_t point_index = 0;
    // StraightToBend only: interior fraction 0 < t < 1 and displacement
    // relative to the point (1-t)*a+t*b. Must create a genuine bend.
    double segment_fraction = 0.5;
    Point displacement;
};

struct TopologyEditOptions {
    TopologyOptions audit;
    std::size_t max_edits = 128;
    std::size_t max_audits = 4096; // Cumulative geometric signature rechecks.
    // For a StraightToBend insertion, also audit 1/N,2/N,...,N/N
    // of the local continuous vertex displacement. This is a sampled guard,
    // NOT a certificate of all continuous-time configurations.
    std::size_t continuation_frames = 8;
    double collinear_tolerance = 1e-10;
};

struct TopologyEditReport {
    std::size_t inserted_bends = 0;
    std::size_t removed_bends = 0;
    std::size_t audits = 0;
    std::size_t continuation_frames = 0;
    std::size_t final_route_points = 0;
};

// Audits the baseline embedding, applies explicit route surgery on a private
// copy, then audits ALL intermediate split frames and all final edits against
// the baseline ordered/signed crossing signature. Every candidate must retain
// measured port anchors and avoid every node rectangle. Errors clear `out`
// and report atomically; no legacy/alternative routing fallback exists.
// The edit changes route topology *representation*, not arbitrary geometry.
// It does not schedule collision-triggered bends nor guarantee ambient isotopy.
TopologyStatus apply_topology_edits(
    const Graph& graph, const Layout& layout,
    const std::vector<RouteRequest>& ports, const Routes& baseline,
    const std::vector<TopologyEdit>& edits, Routes& out,
    const TopologyEditOptions& options = {}, TopologyEditReport* report = nullptr);

} // namespace stun::graphlayout
