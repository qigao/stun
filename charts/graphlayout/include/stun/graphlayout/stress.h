#pragma once

#include "stun/graphlayout/projection.h"

#include <cstddef>
#include <string>
#include <vector>

namespace stun::graphlayout {

// Undirected shortest-path stress: graph edge orientation affects neither
// distances nor weights. Connected pairs at hop distance h contribute
//     0.5 / h^2 * (Euclidean(center_i, center_j) - h * ideal_length)^2.
// Disconnected pairs do not contribute. This is a NONCONVEX objective and the
// implementation claims neither global optimality nor SMACOF majorization.
struct StressOptions {
    double ideal_length = 80.0;
    double initial_step = 0.5;
    double relative_tolerance = 1e-8;
    std::size_t max_nodes = 128;
    std::size_t max_edges = 8192;
    std::size_t max_pairs = 8192;
    std::size_t max_bfs_scans = 3000000;
    std::size_t max_iterations = 96;
    std::size_t max_backtracks = 16;
    ProjectionOptions projection;
};

enum class StressError {
    None,
    InvalidInput,
    InvalidOptions,
    InvalidGeometry,
    CapacityExceeded,
    ProjectionFailed,
    InternalInvariant,
};

enum class StressTermination {
    NoPairs,
    GradientTolerance,
    LineSearchStalled,
    IterationBudget,
};

struct StressStatus {
    StressError error = StressError::None;
    std::string message;
    ProjectionError projection_error = ProjectionError::None;
    explicit operator bool() const { return error == StressError::None; }
};

struct StressEvaluation {
    double value = 0.0;
    std::size_t pairs = 0;
};

struct StressResult {
    Layout layout;
    StressEvaluation initial;
    StressEvaluation final;
    StressTermination termination = StressTermination::NoPairs;
    std::size_t iterations = 0;
    std::size_t accepted_steps = 0;
    std::size_t line_search_trials = 0;
    // Includes the post-projection initial objective, then ONLY accepted
    // monotone energies. This is an auditable descent record, not a proof of
    // optimality in the nonconvex objective or of optimal projection.
    std::vector<double> accepted_objectives;
};

// Evaluate the exact stated pair objective; this never modifies the layout.
// Budget and input errors do not change the output evaluation.
StressStatus evaluate_stress(const Graph& graph, const Layout& layout,
                             StressEvaluation& out,
                             const StressOptions& options = {});

// Deterministic, budgeted gradient descent with energy-audited backtracking.
// Optional hard pins, alignments, separations, and collision avoidance are
// projected with native VPSC at initialization and at every trial.
//
// On failure `out` is cleared; no Cola, libavoid, random jitter or provider
// fallback. The solver reports a finite feasible best iterate only if it
// passes all configured numerical/geometry checks; IterationBudget is a
// transparent finite-iterate status, NOT an optimality certificate.
StressStatus layout_stress(const Graph& graph, const Layout& seed,
                           const ProjectionConstraints& constraints,
                           StressResult& out,
                           const StressOptions& options = {});

} // namespace stun::graphlayout
