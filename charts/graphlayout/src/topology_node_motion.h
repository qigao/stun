#pragma once

#include "stun/graphlayout/topology_constraints.h"

#include <array>

namespace stun::graphlayout::detail {

// Private cumulative work for explicit bend transitions. The layout has
// already passed the complete baseline topology audit; nodes stay fixed.
struct TopologyNodeMotionWork {
    std::size_t constraints=0, evaluations=0;
};

// Bound both affine segments of a three-point bend against every node,
// including incident nodes. Shares the movement guard's event partition.
// Work is cumulative across the edit batch; safe_fraction commits on success.
TopologyGuardStatus limit_bend_node_motion(
    const std::array<TopologyPointTrack,3>& bend, const Layout& nodes,
    double& safe_fraction, TopologyNodeMotionWork& work,
    const TopologyGuardOptions& options);

} // namespace stun::graphlayout::detail
