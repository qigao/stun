#pragma once

#include "stun/graphlayout/projection.h"

#include <cstddef>
#include <string>
#include <vector>

namespace stun::graphlayout {

// Undirected shortest-path stress: graph edge orientation affects neither
// distances nor weights. Connected pairs at hop distance h contribute
//     0.5 / h^2 * (Euclidean(center_i, center_j) - h * ideal_length)^2.
// Disconnected pairs do not contribute. This is a NONCONVEX objective:
// neither standalone algorithm claims global optimality. SMACOF uses a
// quadratic majorizer and graph-Laplacian normal equations; Gradient descent
// uses the same objective but does not link the SMACOF implementation.
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
    // For SMACOF, at most this many preconditioned conjugate-gradient
    // iterations PER AXIS and majorization step. Exceeding this limit is a
    // typed failure, never a silently accepted inexact majorizer solve.
    std::size_t max_linear_iterations = 384;
    double linear_relative_tolerance = 1e-11;
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
    LinearSolveLimit,
};

enum class StressTermination {
    NoPairs,
    GradientTolerance,
    LineSearchStalled,
    IterationBudget,
    MajorizationTolerance,
    MajorizationStalled,
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
    std::size_t linear_solves = 0; // Number of SMACOF X/Y Laplacian solve pairs.
    std::size_t linear_iterations = 0; // Sum of CG iterations over both axes.
    // Q(current|current) - Q(majorized|current), prior to any geometric
    // projection. Values are auditable *numerical* majorizer improvements.
    std::vector<double> majorizer_improvements;
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

// Deterministic, separately linked gradient descent and Laplacian-based SMACOF
// majorization. SMACOF exactly
// eliminates declared hard pins from the quadratic step and pins one stable
// reference per unpinned component to remove the translation nullspace.
// Optional VPSC projection does NOT itself minimize the Laplacian majorizer;
// candidates after projection/backtracking are accepted only after measuring
// actual Stress, so accepted objective values remain monotone.
// Optional hard pins, alignments, separations, and collision avoidance are
// projected with native VPSC at initialization and at every trial.
//
// On failure `out` is cleared; no Cola, libavoid, random jitter or provider
// fallback. The solver reports a finite feasible best iterate only if it
// passes all configured numerical/geometry checks; IterationBudget is a
// transparent finite-iterate status, NOT an optimality certificate.
// Explicitly linked independent algorithms: consumers decide at build time.
// No runtime optimizer selector or internal alternative-solver fallback.
StressStatus layout_stress_gradient(const Graph& graph, const Layout& seed,
                                    const ProjectionConstraints& constraints,
                                    StressResult& out,
                                    const StressOptions& options = {});
StressStatus layout_stress_smacof(const Graph& graph, const Layout& seed,
                                  const ProjectionConstraints& constraints,
                                  StressResult& out,
                                  const StressOptions& options = {});

} // namespace stun::graphlayout
