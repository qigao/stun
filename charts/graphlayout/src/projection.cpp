#include "stun/graphlayout/projection.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace stun::graphlayout {
namespace {

ProjectionStatus error(ProjectionError code, const std::string& message,
                       VpscError solver_error = VpscError::None) {
    ProjectionStatus status;
    status.error = code;
    status.message = message;
    status.vpsc_error = solver_error;
    return status;
}

bool valid_axis(ProjectionAxis axis) {
    return axis == ProjectionAxis::X || axis == ProjectionAxis::Y;
}

bool valid(double d) { return std::isfinite(d); }

struct Pair {
    std::size_t first = 0;
    std::size_t second = 0;
};

bool have_clearance(const PlacedNode& a, const PlacedNode& b, double clearance) {
    // 2D rectangles cannot collide when at least one axis is separated.
    // The use of <= allows touching edges when clearance == 0.
    return a.x + a.width + clearance <= b.x ||
           b.x + b.width + clearance <= a.x ||
           a.y + a.height + clearance <= b.y ||
           b.y + b.height + clearance <= a.y;
}

ProjectionStatus validate(const Graph& graph, const Layout& desired,
                          const ProjectionConstraints& r, const ProjectionOptions& o) {
    if (!valid(o.clearance) || o.clearance < 0 || o.max_passes == 0 ||
        o.max_generated_separations == 0 || o.max_pair_checks == 0)
        return error(ProjectionError::InvalidOptions, "invalid projection clearance or budget");
    if (graph.nodes.size() != desired.nodes.size())
        return error(ProjectionError::InvalidInput, "graph and placement node counts differ");
    if (graph.nodes.size() > o.vpsc.max_variables ||
        r.pins.size() > o.vpsc.max_variables ||
        r.alignments.size() > o.vpsc.max_constraints ||
        r.separations.size() > o.vpsc.max_constraints ||
        r.alignments.size() > o.vpsc.max_constraints - r.separations.size())
        return error(ProjectionError::CapacityExceeded, "projection constraint capacity exceeded");
    std::unordered_set<std::string> ids;
    ids.reserve(graph.nodes.size());
    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        const auto& n = graph.nodes[i];
        const auto& p = desired.nodes[i];
        if (n.id.empty() || !ids.insert(n.id).second ||
            !valid(n.width) || !valid(n.height) || n.width <= 0 || n.height <= 0 ||
            !valid(p.x) || !valid(p.y) || !valid(p.width) || !valid(p.height) ||
            p.width <= 0 || p.height <= 0 ||
            p.width != n.width || p.height != n.height ||
            !valid(p.x + p.width) || !valid(p.y + p.height))
            return error(ProjectionError::InvalidInput, "invalid node ID, size, or placement geometry");
    }
    for (const auto& edge : graph.edges) {
        if (edge.source >= graph.nodes.size() || edge.target >= graph.nodes.size())
            return error(ProjectionError::InvalidInput, "invalid graph edge endpoint");
    }
    std::vector<bool> pinned(graph.nodes.size(), false);
    for (const auto& pin : r.pins) {
        if (pin.node >= graph.nodes.size() || pinned[pin.node] ||
            !valid(pin.x) || !valid(pin.y) ||
            !valid(pin.x + desired.nodes[pin.node].width) ||
            !valid(pin.y + desired.nodes[pin.node].height))
            return error(ProjectionError::InvalidInput, "invalid or duplicate hard-fixed node");
        pinned[pin.node] = true;
    }
    for (const auto& a : r.alignments) {
        if (a.first >= graph.nodes.size() || a.second >= graph.nodes.size() ||
            !valid_axis(a.axis) || !valid(a.center_offset))
            return error(ProjectionError::InvalidInput, "invalid node alignment");
    }
    for (const auto& s : r.separations) {
        if (s.before >= graph.nodes.size() || s.after >= graph.nodes.size() ||
            !valid_axis(s.axis) || !valid(s.gap) || s.gap < 0)
            return error(ProjectionError::InvalidInput, "invalid node separation");
    }
    return {};
}

ProjectionStatus from_vpsc(const VpscStatus& s, ProjectionAxis axis) {
    const std::string prefix = axis == ProjectionAxis::X ? "X: " : "Y: ";
    if (s.error == VpscError::CapacityExceeded)
        return error(ProjectionError::CapacityExceeded, prefix + s.message, s.error);
    if (s.error == VpscError::IterationLimit)
        return error(ProjectionError::IterationLimit, prefix + s.message, s.error);
    if (s.error == VpscError::Infeasible)
        return error(ProjectionError::Infeasible, prefix + s.message, s.error);
    return error(ProjectionError::InvalidGeometry, prefix + s.message, s.error);
}

// A generated axis separation needs to exceed the nominal clearance by a
// numeric guard because a dimensionless VPSC KKT tolerance is not an exact
// floating-point geometry guarantee at the box boundary.
double separation_guard(const PlacedNode& a, const PlacedNode& b,
                        const ProjectionOptions& options) {
    const double extent = std::max({1.0, a.width, a.height, b.width, b.height,
                                    options.clearance});
    return std::max(1e-6, extent * options.vpsc.tolerance * 64.0);
}

// Transitive equality links and hard pins define rigid relative coordinates.
// Detect when separating two rectangles along an axis is impossible without
// breaking one of these exact equalities, even when the equality is indirect.
struct RigidAxis {
    std::vector<std::size_t> component;
    std::vector<long double> relative;
    std::vector<long double> grounded_origin;
    std::vector<bool> grounded;

    bool prevents(std::size_t i, std::size_t j, double required) const {
        const std::size_t a = component[i], b = component[j];
        if (a == b)
            return std::abs(relative[j] - relative[i]) < static_cast<long double>(required);
        if (grounded[a] && grounded[b]) {
            const long double dx = (grounded_origin[b] + relative[j]) -
                                   (grounded_origin[a] + relative[i]);
            return std::abs(dx) < static_cast<long double>(required);
        }
        return false;
    }
};

RigidAxis build_rigidity(std::size_t n, ProjectionAxis axis,
                         const ProjectionConstraints& requirements,
                         const std::vector<bool>& pinned,
                         const std::vector<double>& pin_center) {
    struct RelativeEdge { std::size_t target; long double displacement; };
    std::vector<std::vector<RelativeEdge>> adj(n);
    for (const auto& e : requirements.alignments) {
        if (e.axis != axis) continue;
        adj[e.first].push_back({e.second, e.center_offset});
        adj[e.second].push_back({e.first, -static_cast<long double>(e.center_offset)});
    }
    RigidAxis rigid;
    rigid.component.assign(n, n);
    rigid.relative.assign(n, 0.0L);
    rigid.grounded_origin.assign(n, 0.0L);
    rigid.grounded.assign(n, false);
    for (std::size_t i = 0; i < n; ++i) {
        if (rigid.component[i] != n) continue;
        std::queue<std::size_t> q;
        rigid.component[i] = i;
        q.push(i);
        while (!q.empty()) {
            const std::size_t current = q.front(); q.pop();
            if (pinned[current] && !rigid.grounded[i]) {
                rigid.grounded[i] = true;
                rigid.grounded_origin[i] = static_cast<long double>(pin_center[current]) -
                                            rigid.relative[current];
            }
            for (const auto& e : adj[current]) {
                if (rigid.component[e.target] != n) continue;
                rigid.component[e.target] = i;
                rigid.relative[e.target] = rigid.relative[current] + e.displacement;
                q.push(e.target);
            }
        }
    }
    return rigid;
}

} // namespace

ProjectionStatus project_graph(const Graph& graph, const Layout& desired,
                               const ProjectionConstraints& requirements,
                               Layout& out, const ProjectionOptions& options,
                               ProjectionReport* report) {
    out = {};
    if (report) *report = {};
    const auto input = validate(graph, desired, requirements, options);
    if (!input) return input;
    const std::size_t n = graph.nodes.size();
    if (n == 0) return {};

    VpscProblem x, y;
    x.variables.reserve(n);
    y.variables.reserve(n);
    std::vector<bool> pinned(n, false);
    for (const auto& p : requirements.pins) pinned[p.node] = true;
    std::vector<double> pin_x(n), pin_y(n);
    for (const auto& p : requirements.pins) {
        pin_x[p.node] = p.x + desired.nodes[p.node].width / 2.0;
        pin_y[p.node] = p.y + desired.nodes[p.node].height / 2.0;
    }
    for (std::size_t i = 0; i < n; ++i) {
        const auto& node = desired.nodes[i];
        const double desired_x = pinned[i] ? pin_x[i] : node.x + node.width / 2.0;
        const double desired_y = pinned[i] ? pin_y[i] : node.y + node.height / 2.0;
        if (!valid(desired_x) || !valid(desired_y))
            return error(ProjectionError::InvalidGeometry, "center coordinate overflow");
        x.variables.push_back({graph.nodes[i].id, desired_x, 1.0, pinned[i]});
        y.variables.push_back({graph.nodes[i].id, desired_y, 1.0, pinned[i]});
    }

    auto center_offset = [&](std::size_t first, std::size_t second,
                             ProjectionAxis axis, double gap) {
        const auto& a = desired.nodes[first];
        const auto& b = desired.nodes[second];
        const double length_a = axis == ProjectionAxis::X ? a.width : a.height;
        const double length_b = axis == ProjectionAxis::X ? b.width : b.height;
        return length_a * 0.5 + length_b * 0.5 + gap;
    };

    for (const auto& a : requirements.alignments) {
        auto& cs = a.axis == ProjectionAxis::X ? x.constraints : y.constraints;
        cs.push_back({a.first, a.second, a.center_offset, true});
    }
    for (const auto& s : requirements.separations) {
        auto& cs = s.axis == ProjectionAxis::X ? x.constraints : y.constraints;
        const double gap = center_offset(s.before, s.after, s.axis, s.gap);
        if (!valid(gap))
            return error(ProjectionError::InvalidGeometry, "separation geometry overflow");
        cs.push_back({s.before, s.after, gap, false});
    }

    // Stable lexical pair traversal makes axis activation independent of input
    // node insertion ordering; constraints are never removed after activation.
    std::vector<std::size_t> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return graph.nodes[a].id < graph.nodes[b].id;
    });
    std::set<std::pair<std::size_t, std::size_t>> active_pairs;
    const RigidAxis x_rigid = build_rigidity(n, ProjectionAxis::X, requirements,
                                             pinned, pin_x);
    const RigidAxis y_rigid = build_rigidity(n, ProjectionAxis::Y, requirements,
                                             pinned, pin_y);
    ProjectionReport measured;

    for (std::size_t pass = 0; pass < options.max_passes; ++pass) {
        VpscResult px, py;
        const auto sx = solve_vpsc(x, px, options.vpsc);
        if (!sx) return from_vpsc(sx, ProjectionAxis::X);
        const auto sy = solve_vpsc(y, py, options.vpsc);
        if (!sy) return from_vpsc(sy, ProjectionAxis::Y);

        Layout current = desired;
        for (std::size_t i = 0; i < n; ++i) {
            auto& node = current.nodes[i];
            node.x = px.positions[i] - node.width / 2.0;
            node.y = py.positions[i] - node.height / 2.0;
            if (!valid(node.x) || !valid(node.y) || !valid(node.x + node.width) ||
                !valid(node.y + node.height))
                return error(ProjectionError::InvalidGeometry, "projected coordinate overflow");
            // Pins are exact even when conversion center -> top-left can round.
            if (pinned[i]) {
                for (const auto& pin : requirements.pins)
                    if (pin.node == i) {
                        if (std::abs(node.x - pin.x) > 1e-9 || std::abs(node.y - pin.y) > 1e-9)
                            return error(ProjectionError::InvalidGeometry, "hard pin coordinate conversion lost precision");
                        node.x = pin.x;
                        node.y = pin.y;
                    }
            }
        }

        double xmin = current.nodes[0].x;
        double ymin = current.nodes[0].y;
        double xmax = current.nodes[0].x + current.nodes[0].width;
        double ymax = current.nodes[0].y + current.nodes[0].height;
        for (const auto& node : current.nodes) {
            xmin = std::min(xmin, node.x);
            ymin = std::min(ymin, node.y);
            xmax = std::max(xmax, node.x + node.width);
            ymax = std::max(ymax, node.y + node.height);
        }
        current.width = xmax - xmin;
        current.height = ymax - ymin;
        if (!valid(current.width) || !valid(current.height) ||
            current.width <= 0.0 || current.height <= 0.0)
            return error(ProjectionError::InvalidGeometry, "invalid projected bounds");

        measured.passes = pass + 1;
        measured.x_certificate = px.certificate;
        measured.y_certificate = py.certificate;
        if (!requirements.avoid_overlaps) {
            out = std::move(current);
            if (report) *report = measured;
            return {};
        }

        std::vector<Pair> collisions;
        for (std::size_t oi = 0; oi < n; ++oi) {
            for (std::size_t oj = oi + 1; oj < n; ++oj) {
                if (++measured.pair_checks > options.max_pair_checks)
                    return error(ProjectionError::CapacityExceeded, "rectangle intersection budget exhausted");
                const std::size_t i = order[oi], j = order[oj];
                if (!have_clearance(current.nodes[i], current.nodes[j], options.clearance))
                    collisions.push_back({i, j});
            }
        }
        if (collisions.empty()) {
            out = std::move(current);
            if (report) *report = measured;
            return {};
        }
        if (pass + 1 == options.max_passes)
            return error(ProjectionError::IterationLimit, "non-overlap projection pass budget exhausted");

        for (const auto& pair : collisions) {
            const std::size_t i = pair.first, j = pair.second;
            const bool already_active = active_pairs.find({i, j}) != active_pairs.end();
            if (already_active)
                return error(ProjectionError::InvalidGeometry, "activated separation did not prevent overlap");
            if (active_pairs.size() == options.max_generated_separations)
                return error(ProjectionError::CapacityExceeded, "generated non-overlap pair budget exhausted");
            const auto& a = current.nodes[i];
            const auto& b = current.nodes[j];
            const double dx = (b.x + b.width / 2.0) - (a.x + a.width / 2.0);
            const double dy = (b.y + b.height / 2.0) - (a.y + a.height / 2.0);
            const double margin = separation_guard(a, b, options);
            const double req_x = (a.width + b.width) / 2.0 + options.clearance + margin;
            const double req_y = (a.height + b.height) / 2.0 + options.clearance + margin;
            if (!valid(req_x) || !valid(req_y))
                return error(ProjectionError::InvalidGeometry, "generated separation overflow");
            const bool x_viable = !x_rigid.prevents(i, j, req_x);
            const bool y_viable = !y_rigid.prevents(i, j, req_y);
            if (!x_viable && !y_viable)
                return error(ProjectionError::Infeasible, "pins/alignments force a pair of nodes to overlap");
            const double need_x = req_x - std::abs(dx);
            const double need_y = req_y - std::abs(dy);
            const ProjectionAxis axis = (x_viable && (!y_viable || need_x <= need_y))
                ? ProjectionAxis::X : ProjectionAxis::Y;
            auto& constraints = axis == ProjectionAxis::X ? x.constraints : y.constraints;
            if (constraints.size() >= options.vpsc.max_constraints)
                return error(ProjectionError::CapacityExceeded, "VPSC generated constraint capacity exceeded");
            const double distance = axis == ProjectionAxis::X ? dx : dy;
            // If centers coincide, stable lexical ID order chooses direction.
            const std::size_t before = distance < 0.0 ? j : i;
            const std::size_t after = distance < 0.0 ? i : j;
            constraints.push_back({before, after, axis == ProjectionAxis::X ? req_x : req_y});
            active_pairs.insert({i, j});
            measured.generated_separations++;
        }
    }
    return error(ProjectionError::IterationLimit, "non-overlap projection did not converge");
}

} // namespace stun::graphlayout
