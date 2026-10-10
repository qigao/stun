#pragma once

#include "stun/graphlayout/graph.h"

#include <cstddef>
#include <string>

namespace stun::graphlayout {

// This is a graph-placement API, independent of UI, rendering and routing.
// Node identity is stable across calls; edge endpoints are indices into nodes.
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
