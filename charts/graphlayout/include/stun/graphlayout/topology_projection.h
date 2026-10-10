#pragma once

#include "stun/graphlayout/projection.h"
#include "stun/graphlayout/topology_edits.h"

#include <string>
#include <vector>

namespace stun::graphlayout {

// Explicit composition of VPSC projection, topology-preserving local route
// edits and bounded connector continuation. This is not a second optimizer.
// In particular, partially reaching a hard-pinned/constraint-projected goal
// is a FAILURE, never reported as satisfying the projected constraints.
struct TopologyProjectionOptions {
    ProjectionOptions projection;
    TopologyEditOptions edits;
    TopologyOptions topology;
};

struct TopologyProjectionReport {
    ProjectionReport projection;
    TopologyEditReport edits;
    TopologyReport topology;
};

struct TopologyProjectionStatus {
    ProjectionError projection_error = ProjectionError::None;
    TopologyError topology_error = TopologyError::None;
    std::string message;
    explicit operator bool() const {
        return projection_error == ProjectionError::None &&
               topology_error == TopologyError::None;
    }
};

// 1. Project desired node geometry under exact pins, alignment and separation.
// 2. Apply explicit Straight/Bend route edits at the valid baseline geometry.
// 3. Accept ONLY a complete topology-preserving move to the projected layout.
// The projected VPSC solution and its required pins are never silently relaxed
// to a partial movement. Outputs, including reports, are reset on ANY error.
TopologyProjectionStatus project_topology_with_edits(
    const Graph& graph, const Layout& baseline,
    const std::vector<RouteRequest>& ports, const Routes& baseline_routes,
    const Layout& desired, const ProjectionConstraints& constraints,
    const std::vector<TopologyEdit>& edits,
    Layout& out_layout, Routes& out_routes,
    const TopologyProjectionOptions& options = {},
    TopologyProjectionReport* report = nullptr);

} // namespace stun::graphlayout
