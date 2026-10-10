#include "stun/graphlayout/topology_projection.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace stun::graphlayout {
namespace {
TopologyProjectionStatus project_failure(const ProjectionStatus& s) {
    return {s.error,TopologyError::None,"VPSC projection failed: "+s.message};
}
TopologyProjectionStatus topology_failure(const TopologyStatus& s) {
    return {ProjectionError::None,s.error,"topology operation failed: "+s.message};
}
}

TopologyProjectionStatus project_topology_with_edits(
    const Graph& graph, const Layout& baseline,
    const std::vector<RouteRequest>& ports, const Routes& baseline_routes,
    const Layout& desired, const ProjectionConstraints& constraints,
    const std::vector<TopologyEdit>& edits,
    Layout& out_layout, Routes& out_routes,
    const TopologyProjectionOptions& options,
    TopologyProjectionReport* report) {
    out_layout = {};
    out_routes = {};
    if (report) *report = {};
    TopologyProjectionReport result;
    Layout projected;
    auto pstatus = project_graph(graph,desired,constraints,projected,
                                 options.projection,&result.projection);
    if (!pstatus) return project_failure(pstatus);
    Routes prepared;
    auto tstatus = apply_topology_edits(graph,baseline,ports,baseline_routes,
                                        edits,prepared,options.edits,&result.edits);
    if (!tstatus) return topology_failure(tstatus);
    Layout moved;
    Routes paths;
    tstatus = move_topology_preserving(graph,baseline,ports,prepared,projected,
                                       moved,paths,options.topology,&result.topology);
    if (!tstatus) return topology_failure(tstatus);
    // The topology solver legitimately supports fractional moves, but those
    // MUST NOT be reported as satisfying the full VPSC-projected node goal.
    if (result.topology.accepted_fraction != 1.0)
        return {ProjectionError::None,TopologyError::NoAdmissibleStep,
                "topology allows only a partial move; projected constraints were not attained"};
    if (moved.nodes.size() != projected.nodes.size())
        return {ProjectionError::None,TopologyError::InternalInvariant,
                "topology result changed the number of projected nodes"};
    for (std::size_t i=0;i<moved.nodes.size();++i) {
        const auto& expected=projected.nodes[i];
        const auto& actual=moved.nodes[i];
        const double tolerance=4.0*std::numeric_limits<double>::epsilon()*
            std::max(1.0,std::max(std::abs(expected.x),std::abs(expected.y)));
        if (std::abs(actual.x-expected.x)>tolerance ||
            std::abs(actual.y-expected.y)>tolerance)
            return {ProjectionError::None,TopologyError::InternalInvariant,
                    "accepted topology position disagrees with projected target"};
    }
    for (const auto& pin:constraints.pins) {
        if (pin.node>=moved.nodes.size() ||
            moved.nodes[pin.node].x != pin.x ||
            moved.nodes[pin.node].y != pin.y)
            return {ProjectionError::None,TopologyError::NoAdmissibleStep,
                    "projected hard pin was not preserved exactly"};
    }
    out_layout=std::move(moved);
    out_routes=std::move(paths);
    if(report)*report=result;
    return {};
}

} // namespace stun::graphlayout
