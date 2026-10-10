#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace stun::graphlayout {

// Variable Placement with Separation Constraints, independent of rendering.
// A variable has a stable ID, a desired position and a strictly positive weight.
// All indices in the API refer to the caller's original input order.
struct VpscVariable {
    std::string id;
    double desired = 0.0;
    double weight = 1.0;
    // If true, desired is an exact, non-movable absolute coordinate.
    // It is eliminated from the free-variable objective and dual Hessian.
    // This is a hard constraint, not a large-weight approximation.
    bool fixed = false;
};

struct VpscConstraint {
    std::size_t left = 0;
    std::size_t right = 0;
    double gap = 0.0;
    // false: position[right] - position[left] >= gap
    // true:  position[right] - position[left] == gap
    bool equality = false;
};

struct VpscProblem {
    std::vector<VpscVariable> variables;
    std::vector<VpscConstraint> constraints;
};

struct VpscOptions {
    std::size_t max_variables = 4096;
    std::size_t max_constraints = 65536;
    std::size_t max_sweeps = 20000;
    std::size_t max_coordinate_updates = 8000000;
    std::size_t max_feasibility_checks = 16000000;
    // Tolerance applies to dimensionless normalized primal/KKT/duality residuals.
    // This is a numerical certificate, NOT a machine-checked Lean proof.
    double tolerance = 1e-8;
};

struct VpscCertificate {
    double objective = 0.0;
    double dual_lower_bound = 0.0;
    double relative_duality_gap = 0.0;
    double max_primal_violation = 0.0;
    double max_stationarity = 0.0;
    double max_complementarity = 0.0;
    double max_negative_multiplier = 0.0;
};

struct VpscResult {
    std::vector<double> positions;
    // Inequalities have nonnegative multipliers; equalities are unrestricted.
    std::vector<double> multipliers;
    VpscCertificate certificate;
    std::size_t sweeps = 0;
    std::size_t coordinate_updates = 0;
};

// Infeasible cycle evidence references original constraints and orientation.
// If reversed is true, the arc represents the reverse implication of an
// equality constraint. The oriented gaps around the directed cycle sum > 0.
struct VpscWitnessArc {
    std::size_t constraint_index = 0;
    bool reversed = false;
    // True indicates a fixed-position equality arc; constraint_index then
    // names an input variable instead of an input separation constraint.
    bool fixed_variable = false;
};

enum class VpscError {
    None,
    CapacityExceeded,
    InvalidVariable,
    InvalidConstraint,
    DuplicateVariableId,
    InvalidOptions,
    Infeasible,
    IterationLimit,
    InvalidNumerics,
    CertificateFailed,
    InternalInvariant,
};

struct VpscStatus {
    VpscError error = VpscError::None;
    std::string message;
    std::vector<VpscWitnessArc> infeasible_cycle;
    explicit operator bool() const { return error == VpscError::None; }
};

// Deterministic, bounded, dual coordinate ascent for convex weighted VPSC.
// Fixed variables are exact, without artificial weights. Feasibility is
// prechecked using difference constraints. A successful result
// passes an independently recomputed numerical KKT + duality-gap certificate.
// If the budget is exhausted, output is empty; there is no approximate-success
// or alternative-solver fallback.
VpscStatus solve_vpsc(const VpscProblem& problem, VpscResult& out,
                      const VpscOptions& options = {});

// Independently validate user-provided primal/dual variables and recompute the
// certificate. Does not trust the certificate embedded in VpscResult.
VpscStatus verify_vpsc(const VpscProblem& problem, const VpscResult& candidate,
                       VpscCertificate& certificate,
                       const VpscOptions& options = {});

} // namespace stun::graphlayout
