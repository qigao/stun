#include "stress_detail.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <utility>
#include <vector>

namespace stun::graphlayout {
namespace detail {

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



} // namespace detail

StressStatus layout_stress_smacof(const Graph& graph, const Layout& seed,
                                  const ProjectionConstraints& constraints,
                                  StressResult& out, const StressOptions& opt) {
    using namespace detail;
    out = {};
    std::vector<Pair> pairs;
    std::vector<double> stiffness;
    Layout current;
    double energy = 0.0;
    StressResult candidate;
    auto status = initialize(graph, seed, constraints, opt, Method::Smacof,
                             pairs, stiffness, current, energy, candidate);
    if (!status) return status;
    if (graph.nodes.empty()) { out = std::move(candidate); return {}; }
    if (!pairs.empty()) {
        const double scale = *std::max_element(stiffness.begin(), stiffness.end());
        if (!finite(scale) || scale <= 0.0)
            return fail(StressError::InternalInvariant, "invalid stress preconditioner");
        candidate.termination = StressTermination::IterationBudget;
        for (std::size_t iteration = 0; iteration < opt.max_iterations; ++iteration) {
            candidate.iterations = iteration + 1;
            Layout majorized;
            double improvement = 0.0;
            status = majorize(graph, current, pairs, stiffness, constraints, opt,
                              majorized, candidate.linear_iterations, improvement);
            if (!status) return status;
            ++candidate.linear_solves;
            candidate.majorizer_improvements.push_back(improvement);
            if (improvement <= opt.relative_tolerance * std::max(1.0, energy)) {
                candidate.termination = StressTermination::MajorizationTolerance;
                break;
            }
            std::vector<double> move_x(current.nodes.size()), move_y(current.nodes.size());
            for (std::size_t i = 0; i < current.nodes.size(); ++i) {
                move_x[i] = majorized.nodes[i].x - current.nodes[i].x;
                move_y[i] = majorized.nodes[i].y - current.nodes[i].y;
            }
            bool accepted = false;
            status = accept_step(graph, pairs, constraints, opt, move_x, move_y,
                                 1.0, current, energy, candidate, accepted);
            if (!status) return status;
            if (!accepted) {
                candidate.termination = StressTermination::MajorizationStalled;
                break;
            }
        }
    }
    return finalize(graph, opt, pairs, std::move(current), energy, candidate, out);
}

} // namespace stun::graphlayout
