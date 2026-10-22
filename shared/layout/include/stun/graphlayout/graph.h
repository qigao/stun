#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace stun::graphlayout {

// Shared, renderer-neutral graph geometry; no parser or algorithm dependency.
// Input identities are stable, and output positions keep caller index order.
struct Node {
    std::string id;
    double width = 0.0;
    double height = 0.0;
};

struct Edge {
    std::size_t source = 0;
    std::size_t target = 0;
};

struct Graph {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
};

struct PlacedNode {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;
    std::size_t rank = 0;
    std::size_t scc = 0;
};

struct Layout {
    // Index is the input node index. Never reordered by the solver.
    std::vector<PlacedNode> nodes;
    double width = 0.0;
    double height = 0.0;
};

} // namespace stun::graphlayout
