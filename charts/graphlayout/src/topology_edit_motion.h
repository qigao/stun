#pragma once

#include "stun/graphlayout/topology_constraints.h"

#include <array>

namespace stun::graphlayout::detail {

// Private cumulative work for explicit bend transitions. The layout/routes
// have passed the complete baseline topology audit; only the new bend moves.
struct TopologyMotionWork {
    std::size_t constraints=0, evaluations=0;
};

// Bound both affine segments of a three-point bend against every node,
// including incident nodes. Shares the movement guard's event partition.
// Work is cumulative across the edit batch; safe_fraction commits on success.
TopologyGuardStatus limit_bend_node_motion(
    const std::array<TopologyPointTrack,3>& bend, const Layout& nodes,
    double& safe_fraction, TopologyMotionWork& work,
    const TopologyGuardOptions& options);

// The split segment is still present in routes. Only the bend's middle point
// moves; its two outer points and every other route segment remain fixed.
// Bound finite segment contacts, preserving legal adjacent/shared anchors.
TopologyGuardStatus limit_bend_route_motion(
    const std::array<TopologyPointTrack,3>& bend, const Graph& graph,
    const Routes& routes, std::size_t edge, std::size_t split_segment,
    double& safe_fraction, TopologyMotionWork& work,
    const TopologyGuardOptions& options);

} // namespace stun::graphlayout::detail
