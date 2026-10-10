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
        opt.ideal_length > 1e7 ||
        (opt.optimizer == StressOptimizer::GradientDescent &&
         (!finite(opt.initial_step) || opt.initial_step <= 0.0 ||
          opt.initial_step > 2.0)) ||
        (opt.optimizer != StressOptimizer::GradientDescent &&
         opt.optimizer != StressOptimizer::SmacofMajorization) ||
        !finite(opt.linear_relative_tolerance) ||
        opt.linear_relative_tolerance <= 0.0 ||
        opt.linear_relative_tolerance >= 0.1 ||
        opt.max_linear_iterations == 0 || opt.max_linear_iterations > 8192 ||
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

// A Laplacian majorization step solves V X = B(Z) Z, where V is the
// weighted graph Laplacian and off-diagonal B(Z) = -w*d/||z_i-z_j||.
// We eliminate hard-pinned coordinates exactly and gauge-fix one stable ID
// per unpinned component. For positive edge weights the resulting reduced
// Laplacian is SPD; Jacobi-preconditioned CG computes both coordinate axes.
// This is the classical SMACOF/Guttman transform, with a separate energy
// audit after optional VPSC geometry projection.
StressStatus majorize(const Graph& graph, const Layout& current,
                      const std::vector<Pair>& pairs,
                      const std::vector<double>& stiffness,
                      const ProjectionConstraints& constraints,
                      const StressOptions& opt, Layout& target,
                      std::size_t& linear_iterations,
                      double& improvement) {
    const std::size_t n = graph.nodes.size();
    std::vector<std::size_t> parent(n), canonical(n);
    std::iota(parent.begin(), parent.end(), std::size_t{0});
    std::iota(canonical.begin(), canonical.end(), std::size_t{0});
    std::sort(canonical.begin(), canonical.end(), [&](std::size_t u, std::size_t v) {
        return graph.nodes[u].id < graph.nodes[v].id;
    });
    const auto root = [&](std::size_t node) {
        while (parent[node] != node) node = parent[node];
        return node;
    };
    for (const auto& p : pairs) {
        const auto u = root(p.u), v = root(p.v);
        if (u != v) parent[v] = u;
    }
    for (std::size_t i = 0; i < n; ++i) parent[i] = root(i);

    std::vector<bool> fixed(n, false), component_pinned(n, false);
    for (const auto& pin : constraints.pins) {
        if (pin.node >= n)
            return fail(StressError::InvalidInput, "SMACOF pin references a missing node");
        fixed[pin.node] = true;
        component_pinned[parent[pin.node]] = true;
    }
    // Stable component gauge anchor only for components without hard pins.
    std::vector<bool> anchor_selected(n, false);
    for (const auto u : canonical) {
        const auto component = parent[u];
        if (!component_pinned[component] && !anchor_selected[component]) {
            fixed[u] = true;
            anchor_selected[component] = true;
        }
    }

    std::vector<double> old_x(n), old_y(n), rhs_x(n, 0.0), rhs_y(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        old_x[i] = current.nodes[i].x + current.nodes[i].width * 0.5;
        old_y[i] = current.nodes[i].y + current.nodes[i].height * 0.5;
    }
    for (const auto& p : pairs) {
        const double dx = old_x[p.u] - old_x[p.v];
        const double dy = old_y[p.u] - old_y[p.v];
        const double len = std::hypot(dx, dy);
        if (!finite(len) || len <= 0.0)
            return fail(StressError::InvalidGeometry,
                        "SMACOF is undefined at coincident stress-pair centers");
        // Solve for DELTAS from the current layout. The shifted right-hand
        // side is B(Z)Z - VZ, not the absolute-coordinate B(Z)Z. This avoids
        // catastrophic cancellation when graph coordinates are far from zero
        // and makes the linear system translation-independent. Hard pins have
        // exactly zero displacement; no large artificial fixed weights.
        const double scale = p.weight * (p.desired / len - 1.0);
        if (!finite(scale))
            return fail(StressError::InvalidGeometry, "nonfinite SMACOF majorizer");
        rhs_x[p.u] += scale * dx;
        rhs_x[p.v] -= scale * dx;
        rhs_y[p.u] += scale * dy;
        rhs_y[p.v] -= scale * dy;
    }

    const auto apply = [&](const std::vector<double>& x, std::vector<double>& y) {
        std::fill(y.begin(), y.end(), 0.0);
        for (const auto& p : pairs) {
            const bool a = !fixed[p.u], b = !fixed[p.v];
            if (a && b) {
                const double delta = p.weight * (x[p.u] - x[p.v]);
                y[p.u] += delta;
                y[p.v] -= delta;
            } else if (a) {
                y[p.u] += p.weight * x[p.u];
            } else if (b) {
                y[p.v] += p.weight * x[p.v];
            }
        }
    };
    const auto solve_axis = [&](const std::vector<double>& rhs,
                                std::vector<double>& x) -> StressStatus {
        x.assign(n, 0.0); // This is displacement, not absolute position.
        std::vector<double> ax(n), residual(n, 0.0), z(n, 0.0), direction(n, 0.0), ad(n);
        apply(x, ax);
        double rhs_norm2 = 0.0, residual2 = 0.0, rz = 0.0;
        for (const auto i : canonical) {
            if (fixed[i]) continue;
            if (!finite(stiffness[i]) || stiffness[i] <= 0.0)
                return fail(StressError::InternalInvariant,
                            "free SMACOF variable lacks positive diagonal stiffness");
            residual[i] = rhs[i] - ax[i];
            z[i] = residual[i] / stiffness[i];
            direction[i] = z[i];
            rhs_norm2 += rhs[i] * rhs[i];
            residual2 += residual[i] * residual[i];
            rz += residual[i] * z[i];
        }
        if (!finite(rhs_norm2) || !finite(residual2) || !finite(rz))
            return fail(StressError::InvalidGeometry, "SMACOF linear-system overflow");
        const double tol = opt.linear_relative_tolerance *
                           std::max(1.0, std::sqrt(rhs_norm2));
        if (std::sqrt(residual2) <= tol) return {};
        for (std::size_t iter = 0; iter < opt.max_linear_iterations; ++iter) {
            apply(direction, ad);
            double p_ap = 0.0;
            for (const auto i : canonical)
                if (!fixed[i]) p_ap += direction[i] * ad[i];
            if (!(p_ap > 0.0) || !finite(p_ap) || !(rz > 0.0))
                return fail(StressError::InvalidGeometry,
                            "SMACOF reduced Laplacian lost positive definiteness");
            const double alpha = rz / p_ap;
            if (!finite(alpha))
                return fail(StressError::InvalidGeometry, "SMACOF CG step overflow");
            residual2 = 0.0;
            for (const auto i : canonical) {
                if (fixed[i]) continue;
                x[i] += alpha * direction[i];
                residual[i] -= alpha * ad[i];
                residual2 += residual[i] * residual[i];
            }
            ++linear_iterations;
            if (!finite(residual2))
                return fail(StressError::InvalidGeometry, "SMACOF CG residual overflow");
            if (std::sqrt(residual2) <= tol) return {};
            double rz_new = 0.0;
            for (const auto i : canonical) {
                if (fixed[i]) continue;
                z[i] = residual[i] / stiffness[i];
                rz_new += residual[i] * z[i];
            }
            if (!(rz_new > 0.0) || !finite(rz_new))
                return fail(StressError::InvalidGeometry, "SMACOF CG preconditioner breakdown");
            const double beta = rz_new / rz;
            if (!finite(beta))
                return fail(StressError::InvalidGeometry, "SMACOF CG direction overflow");
            for (const auto i : canonical)
                if (!fixed[i]) direction[i] = z[i] + beta * direction[i];
            rz = rz_new;
        }
        return fail(StressError::LinearSolveLimit, "SMACOF reduced Laplacian CG budget exceeded");
    };
    std::vector<double> dx_step, dy_step;
    auto status = solve_axis(rhs_x, dx_step);
    if (!status) return status;
    status = solve_axis(rhs_y, dy_step);
    if (!status) return status;

    // Translation is unobservable in unpinned components. Zero their mean
    // displacement to keep the original center of mass, while never moving
    // any truly pinned component as a whole.
    std::vector<double> avg_dx(n, 0.0), avg_dy(n, 0.0);
    std::vector<std::size_t> count(n, 0);
    for (const auto i : canonical) {
        const auto c = parent[i];
        if (component_pinned[c]) continue;
        avg_dx[c] += dx_step[i];
        avg_dy[c] += dy_step[i];
        ++count[c];
    }
    for (const auto i : canonical) {
        const auto c = parent[i];
        if (!component_pinned[c] && count[c] != 0) {
            dx_step[i] -= avg_dx[c] / static_cast<double>(count[c]);
            dy_step[i] -= avg_dy[c] / static_cast<double>(count[c]);
        }
    }
    std::vector<double> next_x(n), next_y(n);
    for (std::size_t i = 0; i < n; ++i) {
        next_x[i] = old_x[i] + dx_step[i];
        next_y[i] = old_y[i] + dy_step[i];
    }
    target = current;
    for (std::size_t i = 0; i < n; ++i) {
        target.nodes[i].x = next_x[i] - 0.5 * target.nodes[i].width;
        target.nodes[i].y = next_y[i] - 0.5 * target.nodes[i].height;
    }
    status = bounds(target);
    if (!status) return status;
    long double q_delta = 0.0;
    for (const auto& p : pairs) {
        const double dx = old_x[p.u] - old_x[p.v];
        const double dy = old_y[p.u] - old_y[p.v];
        const double nx = next_x[p.u] - next_x[p.v];
        const double ny = next_y[p.u] - next_y[p.v];
        const double old_len = std::hypot(dx, dy);
        q_delta += 0.5L * static_cast<long double>(p.weight) *
                   (static_cast<long double>(nx)*nx + static_cast<long double>(ny)*ny -
                    static_cast<long double>(dx)*dx - static_cast<long double>(dy)*dy -
                    2.0L * static_cast<long double>(p.desired) *
                    (static_cast<long double>(dx)*(nx-dx) +
                     static_cast<long double>(dy)*(ny-dy)) / old_len);
    }
    double old_energy = 0.0;
    status = measure(current, pairs, old_energy);
    if (!status) return status;
    if (!std::isfinite(q_delta) ||
        q_delta > 1e-8L * std::max(1.0, old_energy))
        return fail(StressError::InternalInvariant,
                    "SMACOF quadratic majorizer did not decrease");
    improvement = std::max(0.0, -static_cast<double>(q_delta));
    if (!finite(improvement))
        return fail(StressError::InvalidGeometry, "SMACOF majorizer improvement overflow");
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
            Layout majorized;
            double majorizer_improvement = 0.0;
            if (opt.optimizer == StressOptimizer::SmacofMajorization) {
                status = majorize(graph, current, pairs, stiffness, constraints, opt,
                                  majorized, candidate.linear_iterations,
                                  majorizer_improvement);
                if (!status) return status;
                ++candidate.linear_solves;
                candidate.majorizer_improvements.push_back(majorizer_improvement);
                if (majorizer_improvement <= opt.relative_tolerance *
                                             std::max(1.0, energy)) {
                    candidate.termination = StressTermination::MajorizationTolerance;
                    break;
                }
            } else {
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
            }
            bool accepted = false;
            double step = opt.optimizer == StressOptimizer::SmacofMajorization
                            ? 1.0 : opt.initial_step / scale;
            for (std::size_t trial = 0; trial < opt.max_backtracks; ++trial) {
                ++candidate.line_search_trials;
                Layout unprojected = current;
                bool valid_trial = true;
                for (std::size_t i = 0; i < current.nodes.size(); ++i) {
                    if (opt.optimizer == StressOptimizer::SmacofMajorization) {
                        unprojected.nodes[i].x += step *
                            (majorized.nodes[i].x - current.nodes[i].x);
                        unprojected.nodes[i].y += step *
                            (majorized.nodes[i].y - current.nodes[i].y);
                    } else {
                        unprojected.nodes[i].x -= step * gx[i];
                        unprojected.nodes[i].y -= step * gy[i];
                    }
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
                candidate.termination = opt.optimizer == StressOptimizer::SmacofMajorization
                    ? StressTermination::MajorizationStalled
                    : StressTermination::LineSearchStalled;
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
