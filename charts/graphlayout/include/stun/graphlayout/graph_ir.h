#pragma once

#include "graph_types.h"

#include <string>
#include <vector>

namespace stun::graphlayout {

struct NodeAttributes {
    std::string label;
    bool fixed{false};
    bool hidden{false};
};

struct EdgeAttributes {
    bool directed{true};
};

struct GraphNode {
    NodeId id{};
    Size size{};
    Rect bounds{};
    NodeAttributes attributes;
};

struct GraphEdge {
    EdgeId id{};
    NodeId source{};
    NodeId target{};
    EdgeAttributes attributes;
};

struct GraphCluster {
    ClusterId id{};
    std::vector<NodeId> children;
    Rect bounds{};
};

struct GraphIR {
    std::vector<GraphNode> nodes;
    std::vector<GraphEdge> edges;
    std::vector<GraphCluster> clusters;
};

}
