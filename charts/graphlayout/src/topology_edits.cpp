#include "stun/graphlayout/topology_edits.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <utility>

namespace stun::graphlayout {
namespace {

using Real = long double;

TopologyStatus fail(TopologyError error, const char* message, std::size_t edge = 0) {
    return {error, message, edge};
}

bool same_signature(const TopologySignature& a, const TopologySignature& b) {
    if (a.crossing_count != b.crossing_count ||
        a.ordered_crossings.size() != b.ordered_crossings.size()) return false;
    for (std::size_t e = 0; e < a.ordered_crossings.size(); ++e) {
        const auto& x = a.ordered_crossings[e];
        const auto& y = b.ordered_crossings[e];
        if (x.size() != y.size()) return false;
        for (std::size_t i = 0; i < x.size(); ++i)
            if (x[i].other_edge != y[i].other_edge || x[i].sign != y[i].sign)
                return false;
    }
    return true;
}

bool valid_options(const TopologyEditOptions& o) {
    return o.max_edits > 0 && o.max_edits <= 4096 &&
           o.max_audits > 0 && o.max_audits <= 100000 &&
           o.continuation_frames > 0 && o.continuation_frames <= 256 &&
           std::isfinite(o.collinear_tolerance) &&
           o.collinear_tolerance >= 0.0 && o.collinear_tolerance <= 1e-5;
}

bool fit(Real x, double limit) {
    return std::isfinite(x) && std::abs(x) <= static_cast<Real>(limit);
}

bool new_bend(Point a, Point b, const TopologyEdit& edit,
              double limit, Point& seed, Point& target) {
    const Real t = static_cast<Real>(edit.segment_fraction);
    const Real px = (1 - t)*static_cast<Real>(a.x) + t*b.x;
    const Real py = (1 - t)*static_cast<Real>(a.y) + t*b.y;
    const Real qx = px + static_cast<Real>(edit.displacement.x);
    const Real qy = py + static_cast<Real>(edit.displacement.y);
    if (!fit(px, limit) || !fit(py, limit) || !fit(qx, limit) || !fit(qy, limit))
        return false;
    seed = {static_cast<double>(px), static_cast<double>(py)};
    target = {static_cast<double>(qx), static_cast<double>(qy)};
    // A real bend must not duplicate a terminal nor lie on the old segment.
    const Real dx = static_cast<Real>(b.x) - a.x;
    const Real dy = static_cast<Real>(b.y) - a.y;
    const Real det = dx*(qy - static_cast<Real>(a.y)) -
                     dy*(qx - static_cast<Real>(a.x));
    const Real scale = std::max(Real(1),
        std::max(std::abs(dx),std::abs(dy))*
        std::max(std::abs(qx-static_cast<Real>(a.x)),
                 std::abs(qy-static_cast<Real>(a.y))));
    return det != 0.0L && std::abs(det) > 1e-14L*scale &&
           !(target.x == a.x && target.y == a.y) &&
           !(target.x == b.x && target.y == b.y);
}

bool redundant_bend(Point a, Point p, Point b, double tolerance) {
    const Real ux = static_cast<Real>(p.x) - a.x;
    const Real uy = static_cast<Real>(p.y) - a.y;
    const Real vx = static_cast<Real>(b.x) - p.x;
    const Real vy = static_cast<Real>(b.y) - p.y;
    const Real cross = ux*vy - uy*vx;
    const Real scale = std::max(Real(1),
        std::max(std::abs(ux),std::abs(uy))*
        std::max(std::abs(vx),std::abs(vy)));
    const Real dot = ux*vx + uy*vy;
    // Merging a u-turn would change connectivity geometry even if collinear.
    return std::abs(cross) <= static_cast<Real>(tolerance)*scale && dot > 0.0L;
}

TopologyStatus audit(const Graph& graph, const Layout& layout,
                     const std::vector<RouteRequest>& ports,
                     const Routes& candidate, const TopologySignature& initial,
                     const TopologyEditOptions& options, TopologyEditReport& report,
                     std::size_t edge) {
    if (report.audits == options.max_audits)
        return fail(TopologyError::CapacityExceeded,"topology surgery audit capacity exhausted",edge);
    ++report.audits;
    TopologySignature signature;
    const auto status = audit_topology(graph,layout,ports,candidate,signature,options.audit);
    if (!status) {
        if (status.error == TopologyError::CapacityExceeded) return status;
        return fail(TopologyError::NoAdmissibleStep,
                    "topology surgery would violate connector or obstacle geometry",edge);
    }
    if (!same_signature(initial, signature))
        return fail(TopologyError::NoAdmissibleStep,
                    "topology surgery would alter the ordered crossing signature",edge);
    return {};
}

} // namespace

TopologyStatus apply_topology_edits(
    const Graph& graph, const Layout& layout,
    const std::vector<RouteRequest>& ports, const Routes& baseline,
    const std::vector<TopologyEdit>& edits, Routes& out,
    const TopologyEditOptions& options, TopologyEditReport* report) {
    out = {};
    if (report) *report = {};
    if (!valid_options(options))
        return fail(TopologyError::InvalidOptions,"invalid topology surgery options");
    if (edits.size() > options.max_edits)
        return fail(TopologyError::CapacityExceeded,"topology surgery edit budget exceeded");
    TopologySignature initial;
    auto status = audit_topology(graph,layout,ports,baseline,initial,options.audit);
    if (!status) return status;
    TopologyEditReport result;
    result.audits = 1;
    Routes candidate = baseline;
    for (const auto& edit : edits) {
        if (edit.edge_index >= candidate.edges.size())
            return fail(TopologyError::InvalidInput,"surgery references missing edge",edit.edge_index);
        auto& pts = candidate.edges[edit.edge_index].points;
        const auto current_edge = edit.edge_index;
        if (edit.kind == TopologyEditKind::StraightToBend) {
            if (edit.point_index >= pts.size()-1 ||
                !std::isfinite(edit.segment_fraction) ||
                !(edit.segment_fraction > 0.0 && edit.segment_fraction < 1.0) ||
                !std::isfinite(edit.displacement.x) ||
                !std::isfinite(edit.displacement.y))
                return fail(TopologyError::InvalidInput,"invalid split segment/fraction/displacement",current_edge);
            if (pts.size() >= options.audit.max_points_per_route)
                return fail(TopologyError::CapacityExceeded,"split exceeds route point budget",current_edge);
            Point start, target;
            if (!new_bend(pts[edit.point_index],pts[edit.point_index+1],edit,
                          options.audit.coordinate_limit,start,target))
                return fail(TopologyError::InvalidGeometry,"split cannot create a finite noncollinear bend",current_edge);
            const auto location = pts.begin() + static_cast<std::ptrdiff_t>(edit.point_index + 1);
            pts.insert(location,start);
            const std::size_t inserted = edit.point_index + 1;
            for (std::size_t frame = 1; frame <= options.continuation_frames; ++frame) {
                const Real f = static_cast<Real>(frame)/options.continuation_frames;
                const Real nx = static_cast<Real>(start.x) + f*(static_cast<Real>(target.x)-start.x);
                const Real ny = static_cast<Real>(start.y) + f*(static_cast<Real>(target.y)-start.y);
                if (!fit(nx,options.audit.coordinate_limit) ||
                    !fit(ny,options.audit.coordinate_limit))
                    return fail(TopologyError::InvalidGeometry,"split interpolation exceeds range",current_edge);
                pts[inserted] = {static_cast<double>(nx),static_cast<double>(ny)};
                status = audit(graph,layout,ports,candidate,initial,options,result,current_edge);
                ++result.continuation_frames;
                if (!status) return status;
            }
            ++result.inserted_bends;
        } else if (edit.kind == TopologyEditKind::BendToStraight) {
            if (edit.point_index == 0 || edit.point_index >= pts.size()-1)
                return fail(TopologyError::InvalidInput,"only internal bends can be merged",current_edge);
            const std::size_t k = edit.point_index;
            if (!redundant_bend(pts[k-1],pts[k],pts[k+1],options.collinear_tolerance))
                return fail(TopologyError::NoAdmissibleStep,"bend is not straight or reverses the segment",current_edge);
            pts.erase(pts.begin()+static_cast<std::ptrdiff_t>(k));
            status = audit(graph,layout,ports,candidate,initial,options,result,current_edge);
            if (!status) return status;
            ++result.removed_bends;
        } else {
            return fail(TopologyError::InvalidInput,"unknown topology surgery event",current_edge);
        }
    }
    std::size_t total = 0;
    for (const auto& e : candidate.edges) total += e.points.size();
    result.final_route_points = total;
    out = std::move(candidate);
    if (report) *report = result;
    return {};
}

} // namespace stun::graphlayout
