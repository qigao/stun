#include "stun/graphlayout/stress.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace stun::graphlayout {
namespace {

struct Pair {
    std::size_t u = 0;
    std::size_t v = 0;
    double desired = 0.0;
    double weight = 0.0;
};

StressStatus fail(StressError error, const std::string& message,
                  ProjectionError projection_error = ProjectionError::None) {
    return {error, message, projection_error};
}

bool finite(double x) { return std::isfinite(x); }

StressStatus check_input(const Graph& graph, const Layout& seed, const StressOptions& opt) {
    if (!finite(opt.ideal_length) || opt.ideal_length <= 0.0 ||
        opt.ideal_length > 1e7 || !finite(opt.initial_step) ||
        opt.initial_step <= 0.0 || opt.initial_step > 2.0 ||
        !finite(opt.relative_tolerance) || opt.relative_tolerance <= 0.0 ||
        opt.relative_tolerance >= 0.1 || opt.max_nodes == 0 || opt.max_edges == 0 ||
        opt.max_pairs == 0 || opt.max_bfs_scans == 0 || opt.max_iterations == 0 ||
        opt.max_backtracks == 0 || opt.max_backtracks > 64 ||
        opt.max_iterations > 1024)
        return fail(StressError::InvalidOptions, "invalid stress parameters or budgets");
    if (graph.nodes.size() > opt.max_nodes || graph.edges.size() > opt.max_edges)
        return fail(StressError::CapacityExceeded, "stress graph node or edge budget exceeded");
    if (graph.nodes.size() != seed.nodes.size())
        return fail(StressError::InvalidInput, "graph and seed node counts differ");
    std::unordered_set<std::string> seen;
    seen.reserve(graph.nodes.size());
    for (std::size_t i = 0; i < graph.nodes.size(); ++i) {
        const auto& n = graph.nodes[i];
        const auto& p = seed.nodes[i];
        if (n.id.empty() || !seen.insert(n.id).second ||
            !finite(n.width) || !finite(n.height) || n.width <= 0.0 || n.height <= 0.0 ||
            !finite(p.x) || !finite(p.y) || p.width != n.width || p.height != n.height ||
            !finite(p.x + n.width) || !finite(p.y + n.height) ||
            !finite(p.x + n.width * 0.5) || !finite(p.y + n.height * 0.5))
            return fail(StressError::InvalidInput, "invalid graph ID, size, or seed geometry");
    }
    for (const auto& e : graph.edges)
        if (e.source >= graph.nodes.size() || e.target >= graph.nodes.size())
            return fail(StressError::InvalidInput, "stress edge references a missing node");
    return {};
}

// Fixed canonical ID order makes accumulation independent of node/edge input
// permutation. A BFS for each node gives unit-edge undirected path distances.
StressStatus build_pairs(const Graph& graph, const StressOptions& opt,
                         std::vector<Pair>& pairs,
                         std::vector<double>& stiffness) {
    const std::size_t n = graph.nodes.size();
    std::vector<std::size_t> canonical(n);
    std::iota(canonical.begin(), canonical.end(), std::size_t{0});
    std::sort(canonical.begin(), canonical.end(), [&](std::size_t a, std::size_t b) {
        return graph.nodes[a].id < graph.nodes[b].id;
    });
    std::vector<std::size_t> by_id(n);
    for (std::size_t i = 0; i < n; ++i) by_id[canonical[i]] = i;
    std::vector<std::vector<std::size_t>> adj(n);
    for (const auto& e : graph.edges) {
        if (e.source == e.target) continue;
        adj[e.source].push_back(e.target);
        adj[e.target].push_back(e.source);
    }
    for (auto& neighbours : adj) {
        std::sort(neighbours.begin(), neighbours.end(), [&](std::size_t a, std::size_t b) {
            return by_id[a] < by_id[b];
        });
        neighbours.erase(std::unique(neighbours.begin(), neighbours.end()), neighbours.end());
    }

    stiffness.assign(n, 0.0);
    std::vector<std::size_t> hops(n);
    std::size_t scans = 0;
    for (std::size_t ordinal = 0; ordinal < n; ++ordinal) {
        const auto source = canonical[ordinal];
        std::fill(hops.begin(), hops.end(), n);
        hops[source] = 0;
        std::queue<std::size_t> queue;
        queue.push(source);
        while (!queue.empty()) {
            const auto u = queue.front();
            queue.pop();
            for (auto v : adj[u]) {
                if (++scans > opt.max_bfs_scans)
                    return fail(StressError::CapacityExceeded, "stress BFS operation budget exceeded");
                if (hops[v] == n) {
                    hops[v] = hops[u] + 1;
                    queue.push(v);
                }
            }
        }
        for (std::size_t target = ordinal + 1; target < n; ++target) {
            const auto v = canonical[target];
            if (hops[v] == n) continue; // Independent weak components.
            if (pairs.size() == opt.max_pairs)
                return fail(StressError::CapacityExceeded, "stress pair budget exceeded");
            const double h = static_cast<double>(hops[v]);
            const double desired = h * opt.ideal_length;
            const double weight = 1.0 / (h * h);
            if (!finite(desired) || !finite(weight) || weight <= 0.0)
                return fail(StressError::InvalidGeometry, "stress distance or weight overflow");
            pairs.push_back({source, v, desired, weight});
            stiffness[source] += weight;
            stiffness[v] += weight;
        }
    }
    return {};
}

StressStatus measure(const Layout& l, const std::vector<Pair>& pairs,
                     double& energy, std::vector<double>* grad_x = nullptr,
                     std::vector<double>* grad_y = nullptr) {
    energy = 0.0;
    if (grad_x) {
        grad_x->assign(l.nodes.size(), 0.0);
        grad_y->assign(l.nodes.size(), 0.0);
    }
    // Accumulate double using fixed ID-ordered pairs. No zero-distance jitter:
    // such an input is singular and is explicitly rejected.
    for (const auto& pair : pairs) {
        const auto& a = l.nodes[pair.u];
        const auto& b = l.nodes[pair.v];
        const double ax = a.x + a.width * 0.5;
        const double ay = a.y + a.height * 0.5;
        const double bx = b.x + b.width * 0.5;
        const double by = b.y + b.height * 0.5;
        const double dx = ax - bx, dy = ay - by;
        const double length = std::hypot(dx, dy);
        if (!finite(length) || length == 0.0)
            return fail(StressError::InvalidGeometry, "coincident connected centers or nonfinite stress distance");
        const double residual = length - pair.desired;
        energy += 0.5 * pair.weight * residual * residual;
        if (!finite(energy)) return fail(StressError::InvalidGeometry, "stress objective overflow");
        if (grad_x) {
            const double scalar = pair.weight * residual / length;
            const double gx = scalar * dx, gy = scalar * dy;
            (*grad_x)[pair.u] += gx;
            (*grad_y)[pair.u] += gy;
            (*grad_x)[pair.v] -= gx;
            (*grad_y)[pair.v] -= gy;
        }
    }
    if (grad_x)
        for (std::size_t i = 0; i < l.nodes.size(); ++i)
            if (!finite((*grad_x)[i]) || !finite((*grad_y)[i]))
                return fail(StressError::InvalidGeometry, "nonfinite stress derivative");
    return {};
}

StressStatus bounds(Layout& l) {
    if (l.nodes.empty()) { l.width = 0; l.height = 0; return {}; }
    double left = l.nodes.front().x, top = l.nodes.front().y;
    double right = left + l.nodes.front().width;
    double bottom = top + l.nodes.front().height;
    for (const auto& p : l.nodes) {
        if (!finite(p.x) || !finite(p.y) || !finite(p.x + p.width) ||
            !finite(p.y + p.height))
            return fail(StressError::InvalidGeometry, "nonfinite optimized node rectangle");
        left = std::min(left, p.x);
        top = std::min(top, p.y);
        right = std::max(right, p.x + p.width);
        bottom = std::max(bottom, p.y + p.height);
    }
    l.width = right - left;
    l.height = bottom - top;
    if (!finite(l.width) || !finite(l.height) || l.width <= 0.0 || l.height <= 0.0)
        return fail(StressError::InvalidGeometry, "nonfinite stress layout extent");
    return {};
}

bool needs_projection(const ProjectionConstraints& c) {
    return c.avoid_overlaps || !c.pins.empty() || !c.alignments.empty() ||
           !c.separations.empty();
}

StressStatus project(const Graph& graph, const Layout& candidate,
                     const ProjectionConstraints& constraints,
                     const StressOptions& opt, Layout& out) {
    if (!needs_projection(constraints)) {
        out = candidate;
        return bounds(out);
    }
    const auto status = project_graph(graph, candidate, constraints, out, opt.projection);
    if (!status)
        return fail(StressError::ProjectionFailed, "stress VPSC projection: " + status.message,
                    status.error);
    return {};
}

} // namespace

StressStatus evaluate_stress(const Graph& graph, const Layout& layout,
                             StressEvaluation& out, const StressOptions& opt) {
    const auto input_status = check_input(graph, layout, opt);
    if (!input_status) return input_status;
    std::vector<Pair> pairs;
    std::vector<double> stiffness;
    const auto pair_status = build_pairs(graph, opt, pairs, stiffness);
    if (!pair_status) return pair_status;
    StressEvaluation candidate;
    candidate.pairs = pairs.size();
    auto status = measure(layout, pairs, candidate.value);
    if (!status) return status;
    out = candidate;
    return {};
}

StressStatus layout_stress(const Graph& graph, const Layout& seed,
                           const ProjectionConstraints& constraints,
                           StressResult& out, const StressOptions& opt) {
    out = {};
    auto status = check_input(graph, seed, opt);
    if (!status) return status;
    if (graph.nodes.empty()) {
        if (!constraints.pins.empty() || !constraints.alignments.empty() ||
            !constraints.separations.empty())
            return fail(StressError::InvalidInput, "empty graph has node constraints");
        out.accepted_objectives.push_back(0.0);
        return {};
    }
    std::vector<Pair> pairs;
    std::vector<double> stiffness;
    status = build_pairs(graph, opt, pairs, stiffness);
    if (!status) return status;
    Layout current;
    status = project(graph, seed, constraints, opt, current);
    if (!status) return status;
    double energy = 0.0;
    status = measure(current, pairs, energy);
    if (!status) return status;
    StressResult candidate;
    candidate.initial = {energy, pairs.size()};
    candidate.accepted_objectives.push_back(energy);
    candidate.layout = current;
    candidate.termination = StressTermination::NoPairs;
    if (!pairs.empty()) {
        const double scale = *std::max_element(stiffness.begin(), stiffness.end());
        if (!finite(scale) || scale <= 0.0)
            return fail(StressError::InternalInvariant, "invalid stress preconditioner");
        candidate.termination = StressTermination::IterationBudget;
        for (std::size_t iteration = 0; iteration < opt.max_iterations; ++iteration) {
            candidate.iterations = iteration + 1;
            std::vector<double> gx, gy;
            status = measure(current, pairs, energy, &gx, &gy);
            if (!status) return status;
            double max_gradient = 0.0;
            for (std::size_t i = 0; i < gx.size(); ++i)
                max_gradient = std::max(max_gradient, std::hypot(gx[i], gy[i]));
            if (!finite(max_gradient))
                return fail(StressError::InvalidGeometry, "stress gradient norm overflow");
            if (max_gradient <= opt.relative_tolerance *
                                std::max(1.0, opt.ideal_length * scale)) {
                candidate.termination = StressTermination::GradientTolerance;
                break;
            }
            bool accepted = false;
            double step = opt.initial_step / scale;
            for (std::size_t trial = 0; trial < opt.max_backtracks; ++trial) {
                ++candidate.line_search_trials;
                Layout unprojected = current;
                bool valid_trial = true;
                for (std::size_t i = 0; i < gx.size(); ++i) {
                    unprojected.nodes[i].x -= step * gx[i];
                    unprojected.nodes[i].y -= step * gy[i];
                    if (!finite(unprojected.nodes[i].x) || !finite(unprojected.nodes[i].y)) {
                        valid_trial = false;
                        break;
                    }
                }
                if (valid_trial) {
                    Layout projected;
                    status = project(graph, unprojected, constraints, opt, projected);
                    if (status) {
                        double trial_energy = 0.0;
                        status = measure(projected, pairs, trial_energy);
                        if (status && trial_energy < energy &&
                            energy - trial_energy > opt.relative_tolerance *
                                                  std::max(1.0, energy)) {
                            current = std::move(projected);
                            energy = trial_energy;
                            candidate.accepted_objectives.push_back(energy);
                            ++candidate.accepted_steps;
                            accepted = true;
                            break;
                        }
                    }
                }
                step *= 0.5;
            }
            if (!accepted) {
                candidate.termination = StressTermination::LineSearchStalled;
                break;
            }
        }
    }
    candidate.layout = std::move(current);
    candidate.final = {energy, pairs.size()};
    // Independently recompute from the exposed final layout, not just the
    // descent bookkeeping, to detect silent corruption of the emitted result.
    StressEvaluation checked;
    status = evaluate_stress(graph, candidate.layout, checked, opt);
    if (!status) return status;
    if (!finite(checked.value) || checked.pairs != pairs.size() ||
        std::abs(checked.value - energy) >
            1e-10 * std::max({1.0, checked.value, energy}))
        return fail(StressError::InternalInvariant, "stress final objective audit mismatch");
    candidate.final = checked;
    out = std::move(candidate);
    return {};
}

} // namespace stun::graphlayout
