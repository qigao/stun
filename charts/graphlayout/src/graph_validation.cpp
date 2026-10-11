#include "stun/graphlayout/graph_validation.h"

#include <cmath>
#include <unordered_set>

namespace stun::graphlayout {

GraphValidationResult validate_graph(const GraphIR& graph)
{
    std::unordered_set<NodeId> nodes;

    for (const auto& node : graph.nodes) {
        if (!nodes.insert(node.id).second) {
            return {false, "duplicate node id"};
        }
        if (!std::isfinite(node.size.width) ||
            !std::isfinite(node.size.height) ||
            node.size.width < 0 ||
            node.size.height < 0) {
            return {false, "invalid node size"};
        }
    }

    for (const auto& edge : graph.edges) {
        if (!nodes.contains(edge.source) || !nodes.contains(edge.target)) {
            return {false, "edge references missing node"};
        }
    }

    return {true, {}};
}

}
