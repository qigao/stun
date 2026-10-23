#pragma once

#include "stun/graphlayout/projection.h"

#include <cstddef>
#include <string>
#include <vector>

namespace stun::graphlayout {

// A separately selected, renderer-neutral spring/electrostatic graph layout.
// Spring edges are undirected, deduplicated pairs (self loops do not pull).
// For node centers c_i the explicitly auditable nonconvex objective is
//
//   0.5*k*sum_{(i,j) in E} (|c_i-c_j|-L)^2
//     + alpha*L^3*sum_{i<j} 1/sqrt(|c_i-c_j|^2+softening^2).
//
// The softened Coulomb term is finite at coincidence, but its gradient there
// is zero: the solver rejects coincident centers unless explicit VPSC geometry
// constraints separate them. No random or ID-dependent jitter is introduced.
// Distinct connected components repel each other; unlike Stress, they are not
// independent. This is not the full Cola force-directed model.
struct ForceOptions {
    double ideal_length = 80.0;
    double spring_strength = 1.0;
    double repulsion_strength = 0.04; // Dimensionless alpha.
    double softening = 8.0;          // Regularizes the inverse-distance force.
    double initial_step = 0.75;
    double relative_tolerance = 1e-9;
    std::size_t max_nodes = 128;
    std::size_t max_edges = 8192;
    std::size_t max_pairs = 8192;
    std::size_t max_pair_evaluations = 20000000;
    std::size_t max_iterations = 96;
    std::size_t max_backtracks = 20;
    ProjectionOptions projection;
};

enum class ForceError {
    None,
    InvalidInput,
    InvalidOptions,
    InvalidGeometry,
    CapacityExceeded,
    ProjectionFailed,
    InternalInvariant,
};

struct ForceStatus {
    ForceError error = ForceError::None;
    std::string message;
    ProjectionError projection_error = ProjectionError::None;
    explicit operator bool() const { return error == ForceError::None; }
};

struct ForceEnergy {
    double total = 0.0;
    double spring = 0.0;
    double repulsion = 0.0;
    std::size_t spring_edges = 0;
    std::size_t repulsive_pairs = 0;
};

struct ForceGradient {
    // Derivative of the stated objective with respect to top-left node x,y.
    // Centers differ only by constants, hence have the same derivative.
    std::vector<double> dx;
    std::vector<double> dy;
};

enum class ForceTermination {
    NoInteractions,
    GradientTolerance,   // Unconstrained gradient test; not a KKT proof.
    LineSearchStalled,   // Best feasible finite iterate, not proven optimal.
    IterationBudget,     // Best feasible finite iterate, not proven optimal.
};

struct ForceResult {
    Layout layout;
    ForceEnergy initial;
    ForceEnergy final;
    ForceTermination termination = ForceTermination::NoInteractions;
    std::size_t iterations = 0;
    std::size_t accepted_steps = 0;
    std::size_t line_search_trials = 0;
    std::size_t pair_evaluations = 0;
    // Baseline followed exclusively by strictly decreasing accepted energies.
    std::vector<double> accepted_energies;
};

// Evaluate the objective (and optionally its analytic gradient) on a layout.
// The supplied output remains unchanged on error; calls are side-effect free.
ForceStatus evaluate_force(const Graph& graph, const Layout& layout,
                           ForceEnergy& out, const ForceOptions& options = {});
ForceStatus evaluate_force_gradient(const Graph& graph, const Layout& layout,
                                    ForceEnergy& energy, ForceGradient& gradient,
                                    const ForceOptions& options = {});

// Deterministic bounded descent with actual-energy backtracking. Hard pins,
// alignments, minimum gaps and rectangle collisions may be projected with
// VPSC at initialization and every trial. Accepted energies are always
// checked AFTER projection. On any error output is entirely reset; there is
// no Cola, alternate optimizer, synthetic jitter or approximate-success fallback.
ForceStatus layout_force(const Graph& graph, const Layout& seed,
                         const ProjectionConstraints& constraints,
                         ForceResult& out, const ForceOptions& options = {});

} // namespace stun::graphlayout
