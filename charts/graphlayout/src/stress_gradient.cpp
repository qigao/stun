#include "stress_detail.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace stun::graphlayout {

StressStatus layout_stress_gradient(const Graph& graph, const Layout& seed,
                                    const ProjectionConstraints& constraints,
                                    StressResult& out, const StressOptions& opt) {
    using namespace detail;
    out = {};
    std::vector<Pair> pairs;
    std::vector<double> stiffness;
    Layout current;
    double energy = 0.0;
    StressResult candidate;
    auto status = initialize(graph, seed, constraints, opt, Method::Gradient,
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
            for (std::size_t i = 0; i < gx.size(); ++i) {
                gx[i] = -gx[i];
                gy[i] = -gy[i];
            }
            bool accepted = false;
            status = accept_step(graph, pairs, constraints, opt, gx, gy,
                                 opt.initial_step / scale,
                                 current, energy, candidate, accepted);
            if (!status) return status;
            if (!accepted) {
                candidate.termination = StressTermination::LineSearchStalled;
                break;
            }
        }
    }
    return finalize(graph, opt, pairs, std::move(current), energy, candidate, out);
}

} // namespace stun::graphlayout
