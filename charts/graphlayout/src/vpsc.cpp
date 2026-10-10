#include "stun/graphlayout/vpsc.h"

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

using Real = long double;

VpscStatus failure(VpscError error, const std::string& message) {
    VpscStatus status;
    status.error = error;
    status.message = message;
    return status;
}

bool representable(Real x) {
    return std::isfinite(x) &&
           x >= -static_cast<Real>(std::numeric_limits<double>::max()) &&
           x <= static_cast<Real>(std::numeric_limits<double>::max());
}

VpscStatus validate_input(const VpscProblem& p, const VpscOptions& o) {
    if (o.max_variables == 0 || o.max_constraints == 0 || o.max_sweeps == 0 ||
        o.max_coordinate_updates == 0 || o.max_feasibility_checks == 0 ||
        !std::isfinite(o.tolerance) || o.tolerance <= 0.0 || o.tolerance >= 1.0) {
        return failure(VpscError::InvalidOptions, "invalid VPSC capacity or tolerance");
    }
    if (p.variables.size() > o.max_variables || p.constraints.size() > o.max_constraints)
        return failure(VpscError::CapacityExceeded, "VPSC input exceeds configured capacity");

    std::unordered_set<std::string> unique_ids;
    unique_ids.reserve(p.variables.size());
    for (const auto& v : p.variables) {
        if (v.id.empty() || !std::isfinite(v.desired) ||
            !std::isfinite(v.weight) || v.weight <= 0.0)
            return failure(VpscError::InvalidVariable, "nonfinite, nonpositive or unnamed variable");
        if (!unique_ids.insert(v.id).second)
            return failure(VpscError::DuplicateVariableId, "duplicate VPSC variable ID: " + v.id);
    }
    for (const auto& c : p.constraints) {
        if (c.left >= p.variables.size() || c.right >= p.variables.size() || !std::isfinite(c.gap))
            return failure(VpscError::InvalidConstraint, "invalid VPSC endpoint or gap");
    }
    return {};
}

struct OrderedConstraint {
    std::size_t left = 0;
    std::size_t right = 0;
    std::size_t original = 0;
    Real gap = 0.0;
    bool equality = false;
};

struct Canonical {
    std::vector<std::size_t> order;
    std::vector<std::size_t> canonical_of;
    std::vector<OrderedConstraint> constraints;
};

Canonical canonicalize(const VpscProblem& p) {
    Canonical c;
    c.order.resize(p.variables.size());
    std::iota(c.order.begin(), c.order.end(), 0);
    std::sort(c.order.begin(), c.order.end(), [&](std::size_t a, std::size_t b) {
        return p.variables[a].id < p.variables[b].id;
    });
    c.canonical_of.resize(p.variables.size());
    for (std::size_t i = 0; i < c.order.size(); ++i) c.canonical_of[c.order[i]] = i;
    for (std::size_t i = 0; i < p.constraints.size(); ++i) {
        const auto& v = p.constraints[i];
        c.constraints.push_back({c.canonical_of[v.left], c.canonical_of[v.right],
                                 i, static_cast<Real>(v.gap), v.equality});
    }
    std::sort(c.constraints.begin(), c.constraints.end(), [](const auto& a, const auto& b) {
        if (a.left != b.left) return a.left < b.left;
        if (a.right != b.right) return a.right < b.right;
        if (a.gap != b.gap) return a.gap < b.gap;
        if (a.equality != b.equality) return a.equality < b.equality;
        return a.original < b.original;
    });
    return c;
}

struct Arc {
    std::size_t from = 0;
    std::size_t to = 0;
    Real gap = 0.0;
    std::size_t original = 0;
    bool reversed = false;
};

// For each inequality x_to >= x_from + gap, a strictly positive cycle
// is a certificate of infeasibility. Equalities contribute two arcs.
VpscStatus check_feasibility(std::size_t n, const Canonical& c,
                             std::size_t max_checks, double tolerance) {
    if (n == 0) return {};
    std::vector<Arc> arcs;
    if (c.constraints.size() > max_checks) // also protects doubling overflow
        return failure(VpscError::CapacityExceeded, "feasibility arc budget exceeded");
    arcs.reserve(c.constraints.size() * 2);
    for (const auto& v : c.constraints) {
        arcs.push_back({v.left, v.right, v.gap, v.original, false});
        if (v.equality) arcs.push_back({v.right, v.left, -v.gap, v.original, true});
    }
    if (arcs.empty()) return {};
    if (arcs.size() > max_checks)
        return failure(VpscError::CapacityExceeded, "feasibility arc budget exceeded");
    std::sort(arcs.begin(), arcs.end(), [](const auto& a, const auto& b) {
        if (a.from != b.from) return a.from < b.from;
        if (a.to != b.to) return a.to < b.to;
        if (a.gap != b.gap) return a.gap < b.gap;
        if (a.original != b.original) return a.original < b.original;
        return a.reversed < b.reversed;
    });

    std::vector<Real> dist(n, 0.0);
    std::vector<std::size_t> predecessor(n, arcs.size());
    std::size_t checks = 0;
    std::size_t changed_vertex = n;
    for (std::size_t round = 0; round < n; ++round) {
        changed_vertex = n;
        for (std::size_t i = 0; i < arcs.size(); ++i) {
            if (++checks > max_checks)
                return failure(VpscError::CapacityExceeded, "feasibility relaxation budget exceeded");
            const auto& a = arcs[i];
            const Real new_distance = dist[a.from] + a.gap;
            if (!std::isfinite(new_distance))
                return failure(VpscError::InvalidNumerics, "nonfinite feasibility distance");
            if (new_distance > dist[a.to]) {
                dist[a.to] = new_distance;
                predecessor[a.to] = i;
                changed_vertex = a.to;
            }
        }
        if (changed_vertex == n) return {};
    }

    // Follow predecessor edges into a directed cycle and retain the original
    // constraint indices and equality orientations as a checkable witness.
    std::size_t vertex = changed_vertex;
    for (std::size_t i = 0; i < n; ++i) {
        if (vertex >= n || predecessor[vertex] >= arcs.size())
            return failure(VpscError::InternalInvariant, "broken infeasibility predecessor chain");
        vertex = arcs[predecessor[vertex]].from;
    }
    const std::size_t cycle_begin = vertex;
    std::vector<VpscWitnessArc> witness;
    Real sum = 0.0;
    Real gap_magnitude = 0.0;
    do {
        if (vertex >= n || predecessor[vertex] >= arcs.size() || witness.size() > n)
            return failure(VpscError::InternalInvariant, "invalid positive-cycle witness");
        const Arc& a = arcs[predecessor[vertex]];
        witness.push_back({a.original, a.reversed});
        sum += a.gap;
        gap_magnitude += std::abs(a.gap);
        vertex = a.from;
    } while (vertex != cycle_begin);
    if (!std::isfinite(sum) || !std::isfinite(gap_magnitude) || sum <= 0.0)
        return failure(VpscError::InvalidNumerics, "could not validate an infeasible cycle");
    // A cycle below the requested numerical resolution must not be reported
    // as an independently confirmed infeasibility witness. Floating-point
    // roundoff in otherwise consistent equalities can create such a cycle.
    if (sum <= static_cast<Real>(tolerance) * std::max(1.0L, gap_magnitude))
        return failure(VpscError::InvalidNumerics, "positive cycle is numerically indeterminate at requested tolerance");
    std::reverse(witness.begin(), witness.end());
    VpscStatus status = failure(VpscError::Infeasible, "strictly positive separation-constraint cycle");
    status.infeasible_cycle = std::move(witness);
    return status;
}

VpscStatus certificate_for(const VpscProblem& problem,
                           const VpscResult& candidate,
                           const VpscOptions& options,
                           VpscCertificate& audit) {
    audit = {};
    if (candidate.positions.size() != problem.variables.size() ||
        candidate.multipliers.size() != problem.constraints.size())
        return failure(VpscError::CertificateFailed, "VPSC certificate vector size mismatch");
    const std::size_t n = problem.variables.size();
    const std::size_t m = problem.constraints.size();
    std::vector<Real> grad(n, 0.0);
    std::vector<Real> scale(n, 1.0);
    Real objective = 0.0;
    Real lagrangian = 0.0;
    Real primal = 0.0, complementarity = 0.0, negative_mult = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        if (!std::isfinite(candidate.positions[i]))
            return failure(VpscError::InvalidNumerics, "nonfinite primal candidate");
        const Real dx = static_cast<Real>(candidate.positions[i]) - problem.variables[i].desired;
        const Real weighted = static_cast<Real>(problem.variables[i].weight) * dx;
        grad[i] = weighted;
        scale[i] += std::abs(weighted);
        objective += 0.5L * weighted * dx;
    }
    lagrangian = objective;
    for (std::size_t i = 0; i < m; ++i) {
        const auto& c = problem.constraints[i];
        if (!std::isfinite(candidate.multipliers[i]))
            return failure(VpscError::InvalidNumerics, "nonfinite dual candidate");
        const Real lambda = candidate.multipliers[i];
        const Real slack = (static_cast<Real>(candidate.positions[c.right]) - candidate.positions[c.left]) - c.gap;
        const Real unit = std::max({1.0L, std::abs(static_cast<Real>(c.gap)),
                                     std::abs(static_cast<Real>(candidate.positions[c.left])),
                                     std::abs(static_cast<Real>(candidate.positions[c.right]))});
        const Real violation = c.equality ? std::abs(slack) : std::max(0.0L, -slack);
        primal = std::max(primal, violation / unit);
        if (!c.equality) {
            negative_mult = std::max(negative_mult, std::max(0.0L, -lambda) / (1.0L + std::abs(lambda)));
            complementarity = std::max(complementarity,
                std::abs(lambda * slack) / (1.0L + std::abs(lambda) * unit));
        }
        grad[c.left] += lambda;
        grad[c.right] -= lambda;
        scale[c.left] += std::abs(lambda);
        scale[c.right] += std::abs(lambda);
        lagrangian -= lambda * slack;
    }
    Real stationarity = 0.0;
    Real correction = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        stationarity = std::max(stationarity, std::abs(grad[i]) / scale[i]);
        correction += 0.5L * grad[i] * grad[i] / problem.variables[i].weight;
    }
    const Real dual_bound = lagrangian - correction;
    const Real dual_gap = (objective - dual_bound) / (1.0L + objective);
    if (!representable(objective) || !representable(dual_bound) || !representable(dual_gap) ||
        !representable(primal) || !representable(stationarity) ||
        !representable(complementarity) || !representable(negative_mult))
        return failure(VpscError::InvalidNumerics, "VPSC certificate arithmetic overflow");
    audit.objective = static_cast<double>(objective);
    audit.dual_lower_bound = static_cast<double>(dual_bound);
    audit.relative_duality_gap = static_cast<double>(dual_gap);
    audit.max_primal_violation = static_cast<double>(primal);
    audit.max_stationarity = static_cast<double>(stationarity);
    audit.max_complementarity = static_cast<double>(complementarity);
    audit.max_negative_multiplier = static_cast<double>(negative_mult);
    const Real tolerance = options.tolerance;
    if (primal > tolerance || stationarity > tolerance ||
        complementarity > tolerance || negative_mult > tolerance ||
        std::abs(dual_gap) > tolerance)
        return failure(VpscError::CertificateFailed, "VPSC primal/dual KKT or duality-gap residual exceeded tolerance");
    return {};
}

} // namespace

VpscStatus verify_vpsc(const VpscProblem& problem, const VpscResult& candidate,
                       VpscCertificate& certificate, const VpscOptions& options) {
    certificate = {};
    const auto validated = validate_input(problem, options);
    if (!validated) return validated;
    return certificate_for(problem, candidate, options, certificate);
}

VpscStatus solve_vpsc(const VpscProblem& problem, VpscResult& out,
                      const VpscOptions& options) {
    out = {};
    const auto validated = validate_input(problem, options);
    if (!validated) return validated;
    const auto c = canonicalize(problem);
    const std::size_t n = c.order.size();
    const std::size_t m = c.constraints.size();
    const auto feasible = check_feasibility(n, c, options.max_feasibility_checks,
                                             options.tolerance);
    if (!feasible) return feasible;

    std::vector<Real> x(n, 0.0), inverse_weight(n, 0.0);
    std::vector<Real> dual(m, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        const auto& v = problem.variables[c.order[i]];
        x[i] = v.desired;
        inverse_weight[i] = 1.0L / v.weight;
    }
    VpscResult trial;
    trial.positions.resize(n);
    trial.multipliers.resize(m);
    auto produce_trial = [&]() -> bool {
        for (std::size_t i = 0; i < n; ++i) {
            if (!representable(x[i])) return false;
            trial.positions[c.order[i]] = static_cast<double>(x[i]);
        }
        for (std::size_t j = 0; j < m; ++j) {
            if (!representable(dual[j])) return false;
            trial.multipliers[c.constraints[j].original] = static_cast<double>(dual[j]);
        }
        return true;
    };
    if (!produce_trial()) return failure(VpscError::InvalidNumerics, "VPSC initial values overflow");
    VpscCertificate audit;
    VpscStatus cert_status = certificate_for(problem, trial, options, audit);
    if (cert_status) {
        trial.certificate = audit;
        out = std::move(trial);
        return {};
    }
    if (cert_status.error == VpscError::InvalidNumerics) return cert_status;

    std::size_t updates = 0;
    for (std::size_t sweep = 0; sweep < options.max_sweeps; ++sweep) {
        for (std::size_t j = 0; j < m; ++j) {
            if (updates >= options.max_coordinate_updates)
                return failure(VpscError::IterationLimit, "VPSC coordinate update budget exhausted");
            ++updates;
            const auto& q = c.constraints[j];
            if (q.left == q.right) continue; // already checked feasible above
            const Real norm = inverse_weight[q.left] + inverse_weight[q.right];
            const Real residual = q.gap - (x[q.right] - x[q.left]);
            const Real candidate = dual[j] + residual / norm;
            const Real new_multiplier = q.equality ? candidate : std::max(0.0L, candidate);
            const Real delta = new_multiplier - dual[j];
            if (!std::isfinite(new_multiplier) || !std::isfinite(delta))
                return failure(VpscError::InvalidNumerics, "VPSC dual update overflow");
            dual[j] = new_multiplier;
            x[q.left] -= delta * inverse_weight[q.left];
            x[q.right] += delta * inverse_weight[q.right];
            if (!std::isfinite(x[q.left]) || !std::isfinite(x[q.right]))
                return failure(VpscError::InvalidNumerics, "VPSC primal update overflow");
        }
        if (!produce_trial())
            return failure(VpscError::InvalidNumerics, "VPSC projected output overflow");
        cert_status = certificate_for(problem, trial, options, audit);
        if (cert_status) {
            trial.certificate = audit;
            trial.sweeps = sweep + 1;
            trial.coordinate_updates = updates;
            out = std::move(trial);
            return {};
        }
        if (cert_status.error == VpscError::InvalidNumerics) return cert_status;
    }
    return failure(VpscError::IterationLimit, "VPSC did not meet its independently checked KKT certificate within budget");
}

} // namespace stun::graphlayout
