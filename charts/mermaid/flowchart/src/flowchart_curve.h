#pragma once

#include "flowchart_renderer.h"
#include <array>
#include <string_view>

namespace mermaid::flowchart::curve {

struct Command {
    char kind; // M, L, C or Z
    std::array<Point, 3> points{}; // C: controls then endpoint; M/L: endpoint
    std::size_t size() const { return kind == 'C' ? 3 : kind == 'Z' ? 0 : 1; }
};

bool supported(std::string_view name);
std::vector<Command> interpolate(const std::vector<Point>& points, std::string_view name);

// A curve may leave the corridor certified for the original native polyline.
// Conservative convex-hull subdivision rejects collisions and uncertifiable
// geometry rather than substituting a different curve or route.
void check_obstacles(const std::vector<Command>& path, const std::vector<RenderedNode>& nodes);

} // namespace mermaid::flowchart::curve
