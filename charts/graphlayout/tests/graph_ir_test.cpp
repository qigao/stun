#include "stun/graphlayout/graph_validation.h"

#include <cassert>

using namespace stun::graphlayout;

int main()
{
    GraphIR graph;
    graph.nodes.push_back({1, {10, 10}, {}, {}});
    graph.nodes.push_back({2, {10, 10}, {}, {}});
    graph.edges.push_back({1, 1, 2, {}});

    assert(validate_graph(graph).valid);

    graph.edges.push_back({2, 1, 99, {}});
    assert(!validate_graph(graph).valid);

    return 0;
}
