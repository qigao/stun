#include "stun/graphlayout/force.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace stun::graphlayout {
namespace {

struct Interaction {
    std::size_t u = 0;
    std::size_t v = 0;
    bool spring = false;
};

ForceStatus fail(ForceError error, const std::string& message,
                 ProjectionError projection_error = ProjectionError::None) {
    return {error, message, projection_error};
}

bool finite(double x) { return std::isfinite(x); }

ForceStatus check_input(const Graph& g, const Layout& l, const ForceOptions& o) {
    if (!finite(o.ideal_length) || o.ideal_length <= 0.0 || o.ideal_length > 1e7 ||
        !finite(o.spring_strength) || o.spring_strength < 0.0 || o.spring_strength > 1e7 ||
        !finite(o.repulsion_strength) || o.repulsion_strength < 0.0 || o.repulsion_strength > 1e7 ||
        !finite(o.softening) || o.softening <= 0.0 || o.softening > 1e7 ||
        !finite(o.initial_step) || o.initial_step <= 0.0 || o.initial_step > 2.0 ||
        !finite(o.relative_tolerance) || o.relative_tolerance <= 0.0 ||
        o.relative_tolerance >= 0.1 || o.max_nodes == 0 || o.max_nodes > 256 ||
        o.max_edges == 0 || o.max_pairs == 0 || o.max_pair_evaluations == 0 ||
        o.max_iterations == 0 || o.max_iterations > 1024 || o.max_backtracks == 0 ||
        o.max_backtracks > 64)
        return fail(ForceError::InvalidOptions, "invalid force parameters or resource budgets");
    if (g.nodes.size() > o.max_nodes || g.edges.size() > o.max_edges)
        return fail(ForceError::CapacityExceeded, "force graph node or edge budget exceeded");
    if (g.nodes.size() != l.nodes.size())
        return fail(ForceError::InvalidInput, "graph and seed node count mismatch");

    const auto pair_count = g.nodes.size() < 2 ? 0 :
                            g.nodes.size() * (g.nodes.size() - 1) / 2;
    if (pair_count > o.max_pairs)
        return fail(ForceError::CapacityExceeded, "force repulsion pair budget exceeded");
    std::unordered_set<std::string> ids;
    ids.reserve(g.nodes.size());
    for (std::size_t i = 0; i < g.nodes.size(); ++i) {
        const auto& n = g.nodes[i];
        const auto& p = l.nodes[i];
        if (n.id.empty() || !ids.insert(n.id).second ||
            !finite(n.width) || !finite(n.height) || n.width <= 0.0 || n.height <= 0.0 ||
            !finite(p.x) || !finite(p.y) || p.width != n.width || p.height != n.height ||
            !finite(p.x + n.width) || !finite(p.y + n.height) ||
            !finite(p.x + n.width * 0.5) || !finite(p.y + n.height * 0.5))
            return fail(ForceError::InvalidInput, "invalid graph ID, dimension or seed geometry");
    }
    for (const auto& edge : g.edges)
        if (edge.source >= g.nodes.size() || edge.target >= g.nodes.size())
            return fail(ForceError::InvalidInput, "force edge references missing node");
    const double amp = o.repulsion_strength * o.ideal_length * o.ideal_length * o.ideal_length;
    if (!finite(amp))
        return fail(ForceError::InvalidOptions, "repulsion amplitude exceeds numeric range");
    return {};
}

std::vector<Interaction> build_interactions(const Graph& graph) {
    const auto n = graph.nodes.size();
    std::vector<std::size_t> canonical(n);
    std::iota(canonical.begin(), canonical.end(), std::size_t{0});
    std::sort(canonical.begin(), canonical.end(), [&](auto a, auto b) {
        return graph.nodes[a].id < graph.nodes[b].id;
    });
    std::vector<std::size_t> rank(n);
    for (std::size_t i = 0; i < n; ++i) rank[canonical[i]] = i;
    std::vector<std::pair<std::size_t, std::size_t>> springs;
    springs.reserve(graph.edges.size());
    for (const auto& e : graph.edges) {
        if (e.source == e.target) continue;
        auto u = rank[e.source], v = rank[e.target];
        if (u > v) std::swap(u, v);
        springs.emplace_back(u, v);
    }
    std::sort(springs.begin(), springs.end());
    springs.erase(std::unique(springs.begin(), springs.end()), springs.end());
    std::vector<Interaction> pairs;
    pairs.reserve(n * (n - 1) / 2);
    std::size_t spring_index = 0;
    for (std::size_t a = 0; a < n; ++a) {
        for (std::size_t b = a + 1; b < n; ++b) {
            const bool spring = spring_index < springs.size() &&
                                springs[spring_index] == std::make_pair(a, b);
            if (spring) ++spring_index;
            pairs.push_back({canonical[a], canonical[b], spring});
        }
    }
    return pairs;
}

ForceStatus measure(const Layout& layout, const std::vector<Interaction>& pairs,
                    const ForceOptions& options, std::size_t& visits,
                    ForceEnergy& energy, ForceGradient* gradient = nullptr,
                    double* stiffness = nullptr) {
    if (pairs.size() > options.max_pair_evaluations - visits)
        return fail(ForceError::CapacityExceeded, "force pair evaluation budget exceeded");
    visits += pairs.size();
    ForceEnergy next;
    next.repulsive_pairs = options.repulsion_strength > 0.0 ? pairs.size() : 0;
    if (gradient) {
        gradient->dx.assign(layout.nodes.size(), 0.0);
        gradient->dy.assign(layout.nodes.size(), 0.0);
    }
    std::vector<double> diagonal;
    if (stiffness) diagonal.assign(layout.nodes.size(), 0.0);
    const double amplitude = options.repulsion_strength * options.ideal_length *
                             options.ideal_length * options.ideal_length;
    for (const auto& p : pairs) {
        const auto& a = layout.nodes[p.u];
        const auto& b = layout.nodes[p.v];
        const double dx = (a.x + a.width * 0.5) - (b.x + b.width * 0.5);
        const double dy = (a.y + a.height * 0.5) - (b.y + b.height * 0.5);
        const double distance = std::hypot(dx, dy);
        const double softened = std::hypot(distance, options.softening);
        if (!finite(distance) || !finite(softened) || softened <= 0.0)
            return fail(ForceError::InvalidGeometry, "nonfinite force pair geometry");
        // The spring term is nonsmooth at distance zero and the softened
        // Coulomb gradient cannot break exact ties: never fabricate a vector.
        if (gradient && distance == 0.0)
            return fail(ForceError::InvalidGeometry, "coincident node centers require explicit geometry initialization");
        const double repulsion_energy = amplitude / softened;
        next.repulsion += repulsion_energy;
        double derivative = -repulsion_energy / (softened * softened);
        double curvature = repulsion_energy / (softened * softened);
        if (p.spring) {
            ++next.spring_edges;
            const double residual = distance - options.ideal_length;
            next.spring += 0.5 * options.spring_strength * residual * residual;
            if (gradient)
                derivative += options.spring_strength * residual / distance;
            curvature += options.spring_strength;
        }
        if (gradient) {
            const double gx = derivative * dx, gy = derivative * dy;
            gradient->dx[p.u] += gx;
            gradient->dy[p.u] += gy;
            gradient->dx[p.v] -= gx;
            gradient->dy[p.v] -= gy;
        }
        if (stiffness) {
            diagonal[p.u] += curvature;
            diagonal[p.v] += curvature;
        }
    }
    next.total = next.spring + next.repulsion;
    if (!finite(next.spring) || !finite(next.repulsion) || !finite(next.total))
        return fail(ForceError::InvalidGeometry, "force energy overflow");
    if (gradient) {
        for (std::size_t i = 0; i < gradient->dx.size(); ++i)
            if (!finite(gradient->dx[i]) || !finite(gradient->dy[i]))
                return fail(ForceError::InvalidGeometry, "nonfinite force derivative");
    }
    if (stiffness) {
        *stiffness = 1.0;
        for (auto d : diagonal) {
            if (!finite(d)) return fail(ForceError::InvalidGeometry, "nonfinite force preconditioner");
            *stiffness = std::max(*stiffness, d);
        }
    }
    energy = next;
    return {};
}

ForceStatus normalize_bounds(Layout& layout) {
    if (layout.nodes.empty()) { layout.width = 0.0; layout.height = 0.0; return {}; }
    double min_x = layout.nodes.front().x, min_y = layout.nodes.front().y;
    double max_x = min_x + layout.nodes.front().width;
    double max_y = min_y + layout.nodes.front().height;
    for (const auto& p : layout.nodes) {
        if (!finite(p.x) || !finite(p.y) || !finite(p.x + p.width) || !finite(p.y + p.height))
            return fail(ForceError::InvalidGeometry, "nonfinite force rectangle after update");
        min_x = std::min(min_x, p.x);
        min_y = std::min(min_y, p.y);
        max_x = std::max(max_x, p.x + p.width);
        max_y = std::max(max_y, p.y + p.height);
    }
    layout.width = max_x - min_x;
    layout.height = max_y - min_y;
    if (!finite(layout.width) || !finite(layout.height) || layout.width <= 0.0 || layout.height <= 0.0)
        return fail(ForceError::InvalidGeometry, "nonfinite force extent");
    return {};
}

bool has_constraints(const ProjectionConstraints& c) {
    return c.avoid_overlaps || !c.pins.empty() || !c.alignments.empty() ||
           !c.separations.empty();
}

ForceStatus apply_projection(const Graph& graph, const Layout& candidate,
                             const ProjectionConstraints& constraints,
                             const ForceOptions& opt, Layout& result) {
    if (!has_constraints(constraints)) {
        result = candidate;
        return normalize_bounds(result);
    }
    auto status = project_graph(graph, candidate, constraints, result, opt.projection);
    if (!status)
        return fail(ForceError::ProjectionFailed, "force VPSC projection: " + status.message, status.error);
    return normalize_bounds(result);
}

} // namespace

ForceStatus evaluate_force(const Graph& graph, const Layout& layout,
                           ForceEnergy& out, const ForceOptions& options) {
    auto status = check_input(graph, layout, options);
    if (!status) return status;
    auto pairs = build_interactions(graph);
    std::size_t visits = 0;
    ForceEnergy result;
    status = measure(layout, pairs, options, visits, result);
    if (!status) return status;
    out = result;
    return {};
}

ForceStatus evaluate_force_gradient(const Graph& graph, const Layout& layout,
                                    ForceEnergy& energy, ForceGradient& gradient,
                                    const ForceOptions& options) {
    auto status = check_input(graph, layout, options);
    if (!status) return status;
    auto pairs = build_interactions(graph);
    std::size_t visits = 0;
    ForceEnergy result;
    ForceGradient derivative;
    status = measure(layout, pairs, options, visits, result, &derivative);
    if (!status) return status;
    energy = result;
    gradient = std::move(derivative);
    return {};
}

ForceStatus layout_force(const Graph& graph, const Layout& seed,
                         const ProjectionConstraints& constraints,
                         ForceResult& out, const ForceOptions& options) {
    out = {};
    auto status = check_input(graph, seed, options);
    if (!status) return status;
    if (graph.nodes.empty() && (!constraints.pins.empty() ||
        !constraints.alignments.empty() || !constraints.separations.empty()))
        return fail(ForceError::InvalidInput, "empty force graph has node constraints");
    auto pairs = build_interactions(graph);
    Layout current;
    status = apply_projection(graph, seed, constraints, options, current);
    if (!status) return status;
    std::size_t visits = 0;
    ForceEnergy initial;
    status = measure(current, pairs, options, visits, initial);
    if (!status) return status;
    ForceResult candidate;
    candidate.layout = current;
    candidate.initial = initial;
    candidate.final = initial;
    candidate.accepted_energies.push_back(initial.total);
    candidate.termination = ForceTermination::NoInteractions;
    if (!pairs.empty() && (options.repulsion_strength > 0.0 ||
                          (initial.spring_edges > 0 && options.spring_strength > 0.0))) {
        candidate.termination = ForceTermination::IterationBudget;
        for (std::size_t iteration = 0; iteration < options.max_iterations; ++iteration) {
            candidate.iterations = iteration + 1;
            ForceEnergy audited;
            ForceGradient grad;
            double stiffness = 1.0;
            status = measure(current, pairs, options, visits, audited, &grad, &stiffness);
            if (!status) return status;
            if (std::abs(audited.total - candidate.final.total) > 1e-8 *
                std::max(1.0, audited.total))
                return fail(ForceError::InternalInvariant, "force energy audit disagrees with accepted layout");
            double peak = 0.0;
            for (std::size_t i = 0; i < grad.dx.size(); ++i)
                peak = std::max(peak, std::hypot(grad.dx[i], grad.dy[i]));
            if (!finite(peak)) return fail(ForceError::InvalidGeometry, "force gradient norm overflow");
            if (peak <= options.relative_tolerance *
                        std::max(1.0, options.ideal_length * options.spring_strength)) {
                candidate.termination = ForceTermination::GradientTolerance;
                break;
            }
            double step = options.initial_step / stiffness;
            bool accepted = false;
            for (std::size_t trial = 0; trial < options.max_backtracks; ++trial) {
                ++candidate.line_search_trials;
                Layout tentative = current;
                bool valid = true;
                for (std::size_t i = 0; i < tentative.nodes.size(); ++i) {
                    auto& n = tentative.nodes[i];
                    n.x -= step * grad.dx[i];
                    n.y -= step * grad.dy[i];
                    if (!finite(n.x) || !finite(n.y)) { valid = false; break; }
                }
                if (valid) {
                    Layout projected;
                    auto projection_status = apply_projection(graph, tentative, constraints, options, projected);
                    if (projection_status) {
                        ForceEnergy energy;
                        status = measure(projected, pairs, options, visits, energy);
                        if (!status) return status;
                        const double threshold = options.relative_tolerance *
                                                 std::max(1.0, candidate.final.total) * 0.001;
                        if (energy.total + threshold < candidate.final.total) {
                            current = std::move(projected);
                            candidate.final = energy;
                            candidate.accepted_energies.push_back(energy.total);
                            ++candidate.accepted_steps;
                            accepted = true;
                            break;
                        }
                    } else if (projection_status.error == ForceError::InvalidInput ||
                               projection_status.error == ForceError::InvalidOptions ||
                               projection_status.error == ForceError::CapacityExceeded ||
                               (projection_status.error == ForceError::ProjectionFailed &&
                                (projection_status.projection_error == ProjectionError::InvalidInput ||
                                 projection_status.projection_error == ProjectionError::InvalidOptions ||
                                 projection_status.projection_error == ProjectionError::CapacityExceeded ||
                                 projection_status.projection_error == ProjectionError::IterationLimit))) {
                        // Resource exhaustion/invalid models are not mere
                        // line-search misses; preserve their explicit error.
                        return projection_status;
                    }
                }
                step *= 0.5;
            }
            if (!accepted) {
                candidate.termination = ForceTermination::LineSearchStalled;
                break;
            }
        }
    }
    candidate.layout = std::move(current);
    candidate.pair_evaluations = visits;
    // An independent call to the public objective uses no stored gradient,
    // history, objective cache or previous trial state.
    ForceEnergy independently_measured;
    status = evaluate_force(graph, candidate.layout, independently_measured, options);
    if (!status) return status;
    if (std::abs(independently_measured.total - candidate.final.total) >
        1e-8 * std::max(1.0, independently_measured.total))
        return fail(ForceError::InternalInvariant, "independent final force audit failed");
    out = std::move(candidate);
    return {};
}

} // namespace stun::graphlayout
