#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace stun::graphlayout {

// This is a graph-placement API, independent of UI, rendering and routing.
// Node identity is stable across calls; edge endpoints are indices into nodes.
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

enum class Direction { TopToBottom, BottomToTop, LeftToRight, RightToLeft };

struct Options {
    Direction direction = Direction::TopToBottom;
    double node_gap = 48.0;
    double layer_gap = 76.0;
    double component_gap = 96.0;
    std::size_t crossing_sweeps = 4;
    std::size_t max_nodes = 4096;
    std::size_t max_edges = 65536;
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

enum class Error {
    None,
    CapacityExceeded,
    InvalidNode,
    InvalidEdge,
    DuplicateNodeId,
    InvalidOptions,
    InvalidGeometry,
    InternalInvariant,
};

struct Status {
    Error error = Error::None;
    std::string message;
    explicit operator bool() const { return error == Error::None; }
};

// Deterministic SCC condensation -> longest-path layering -> barycenter sweeps
// -> centered, separated variable-size rectangles. Cyclic edges within a SCC
// are deliberately not claimed to have a forward-layer orientation.
// On failure out is reset. There is no heuristic fallback or partial success.
Status layout_layered(const Graph& graph, Layout& out, const Options& options = {});

} // namespace stun::graphlayout
