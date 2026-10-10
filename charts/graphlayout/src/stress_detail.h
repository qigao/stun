#pragma once

#include "stun/graphlayout/stress.h"

#include <cstddef>
#include <string>
#include <vector>

namespace stun::graphlayout::detail {

struct Pair {
    std::size_t u = 0;
    std::size_t v = 0;
    double desired = 0.0;
    double weight = 0.0;
};

enum class Method { Evaluate, Gradient, Smacof };

StressStatus fail(StressError error, const std::string& message,
                  ProjectionError projection_error = ProjectionError::None);
bool finite(double x);
StressStatus check_input(const Graph& graph, const Layout& seed,
                         const StressOptions& options, Method method);
StressStatus build_pairs(const Graph& graph, const StressOptions& options,
                         std::vector<Pair>& pairs, std::vector<double>& stiffness);
StressStatus measure(const Layout& layout, const std::vector<Pair>& pairs,
                     double& energy, std::vector<double>* grad_x = nullptr,
                     std::vector<double>* grad_y = nullptr);
StressStatus bounds(Layout& layout);
StressStatus project(const Graph& graph, const Layout& candidate,
                     const ProjectionConstraints& constraints,
                     const StressOptions& options, Layout& out);
StressStatus initialize(const Graph& graph, const Layout& seed,
                        const ProjectionConstraints& constraints,
                        const StressOptions& options, Method method,
                        std::vector<Pair>& pairs, std::vector<double>& stiffness,
                        Layout& current, double& energy, StressResult& result);
StressStatus accept_step(const Graph& graph, const std::vector<Pair>& pairs,
                         const ProjectionConstraints& constraints,
                         const StressOptions& options,
                         const std::vector<double>& move_x,
                         const std::vector<double>& move_y,
                         double initial_scale, Layout& current,
                         double& energy, StressResult& result, bool& accepted);
StressStatus finalize(const Graph& graph, const StressOptions& options,
                      const std::vector<Pair>& pairs, Layout&& layout,
                      double energy, StressResult& result, StressResult& out);

} // namespace stun::graphlayout::detail
