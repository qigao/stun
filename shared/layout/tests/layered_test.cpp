#include "stun/graphlayout/layered.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>

using namespace stun::graphlayout;

namespace {
void check(bool ok, const char* text) {
    if (!ok) throw std::runtime_error(text);
}

void check_geometry(const Graph& g, const Layout& l) {
    check(l.nodes.size() == g.nodes.size(), "node count must remain stable");
    for (std::size_t u = 0; u < g.nodes.size(); ++u) {
        const auto& a = l.nodes[u];
        check(std::isfinite(a.x) && std::isfinite(a.y), "finite position");
        check(a.x >= 0 && a.y >= 0, "non-negative position");
        check(a.x + a.width <= l.width + 1e-6 && a.y + a.height <= l.height + 1e-6, "bounds contained");
        for (std::size_t v = u + 1; v < g.nodes.size(); ++v) {
            const auto& b = l.nodes[v];
            bool separated = a.x + a.width <= b.x + 1e-6 || b.x + b.width <= a.x + 1e-6 ||
                             a.y + a.height <= b.y + 1e-6 || b.y + b.height <= a.y + 1e-6;
            check(separated, "non-overlapping node rectangles");
        }
    }
    for (const auto& e : g.edges) {
        const auto& a = l.nodes[e.source];
        const auto& b = l.nodes[e.target];
        if (a.scc == b.scc) check(a.rank == b.rank, "same SCC has same layer");
        else check(a.rank < b.rank, "condensed DAG advances rank");
    }
}

Graph sample() {
    Graph g;
    g.nodes = {{"root", 140, 80}, {"left", 80, 70}, {"right", 180, 40}, {"end", 110, 60}};
    g.edges = {{0,1}, {0,2}, {1,3}, {2,3}};
    return g;
}

std::map<std::string, std::pair<double,double>> positions(const Graph& g, const Layout& l) {
    std::map<std::string, std::pair<double,double>> result;
    for (std::size_t i = 0; i < g.nodes.size(); ++i)
        result[g.nodes[i].id] = {l.nodes[i].x, l.nodes[i].y};
    return result;
}
}

int main() {
    try {
        {
            Layout l;
            check(static_cast<bool>(layout_layered({}, l)), "empty graph success");
            check(l.nodes.empty() && l.width == 0.0 && l.height == 0.0, "empty layout");
        }
        {
            auto g = sample();
            Layout l;
            for (auto d : {Direction::TopToBottom, Direction::BottomToTop,
                           Direction::LeftToRight, Direction::RightToLeft}) {
                Options opts;
                opts.direction = d;
                check(static_cast<bool>(layout_layered(g, l, opts)), "DAG layout success");
                check_geometry(g, l);
                if (d == Direction::TopToBottom) check(l.nodes[0].y < l.nodes[3].y, "TB orientation");
                if (d == Direction::BottomToTop) check(l.nodes[0].y > l.nodes[3].y, "BT orientation");
                if (d == Direction::LeftToRight) check(l.nodes[0].x < l.nodes[3].x, "LR orientation");
                if (d == Direction::RightToLeft) check(l.nodes[0].x > l.nodes[3].x, "RL orientation");
            }
        }
        {
            Graph g;
            g.nodes = {{"one", 40, 50}, {"two", 100, 60}, {"three", 60, 90}, {"tail", 50, 70}};
            g.edges = {{0,1}, {1,2}, {2,0}, {2,3}, {1,1}};
            Layout l;
            check(static_cast<bool>(layout_layered(g, l)), "cycle layout success");
            check_geometry(g, l);
            check(l.nodes[0].scc == l.nodes[1].scc && l.nodes[1].scc == l.nodes[2].scc,
                  "cycle nodes form SCC");
            check(l.nodes[3].rank == 1, "SCC edge advances rank");
        }
        {
            Graph g;
            g.nodes = {{"z", 120, 35}, {"a", 80, 75}, {"isolated", 60, 80}, {"b", 40, 60}};
            g.edges = {{1,3}, {1,3}, {3,0}};
            Layout l;
            check(static_cast<bool>(layout_layered(g, l)), "disconnected layout success");
            check_geometry(g, l);
        }
        {
            // Lexically sorted B->C and A->D would cross without barycenter ordering.
            Graph g;
            g.nodes = {{"A", 80, 50}, {"B", 80, 50}, {"C", 80, 50}, {"D", 80, 50}};
            g.edges = {{0,3}, {1,2}};
            Layout l;
            check(static_cast<bool>(layout_layered(g, l)), "two-layer crossing case");
            check_geometry(g, l);
            const double top_delta = l.nodes[0].x - l.nodes[1].x;
            const double bottom_delta = l.nodes[3].x - l.nodes[2].x;
            check(top_delta * bottom_delta > 0.0, "barycenter removes obvious crossing");
        }
        {
            // Iterative SCC traversal must handle a deep path without recursion.
            Graph g;
            for (std::size_t i = 0; i < 1250; ++i) {
                g.nodes.push_back({"chain" + std::to_string(i), 10, 10});
                if (i > 0) g.edges.push_back({i - 1, i});
            }
            Layout l;
            check(static_cast<bool>(layout_layered(g, l)), "deep graph layout");
            check(l.nodes.front().rank == 0 && l.nodes.back().rank == 1249, "deep ranks preserved");
        }
        {
            Graph a = sample();
            Graph b;
            b.nodes = {a.nodes[2], a.nodes[0], a.nodes[3], a.nodes[1]};
            b.edges = {{1,3}, {1,0}, {3,2}, {0,2}};
            Layout aa, bb;
            check(static_cast<bool>(layout_layered(a, aa)), "first stable layout");
            check(static_cast<bool>(layout_layered(b, bb)), "second stable layout");
            check(positions(a, aa) == positions(b, bb), "input permutation must not change output");
        }
        {
            Graph g;
            const std::size_t nodes = 190;
            for (std::size_t i = 0; i < nodes; ++i)
                g.nodes.push_back({"n" + std::to_string(i), 60.0 + (i % 9), 40.0 + (i % 11)});
            for (std::size_t i = 0; i < nodes * 4; ++i)
                g.edges.push_back({(i * 31 + 17) % nodes, (i * 43 + 11) % nodes});
            Layout a, b;
            check(static_cast<bool>(layout_layered(g, a)), "bounded cyclic stress graph");
            check_geometry(g, a);
            check(static_cast<bool>(layout_layered(g, b)), "repeat cyclic stress graph");
            for (std::size_t i = 0; i < nodes; ++i) {
                check(a.nodes[i].x == b.nodes[i].x && a.nodes[i].y == b.nodes[i].y,
                      "bit-for-bit deterministic repeat");
            }
        }
        {
            Graph g;
            g.nodes = {{"dup", 1, 1}, {"dup", 2, 2}};
            Layout l;
            check(layout_layered(g, l).error == Error::DuplicateNodeId, "duplicate ID rejected");
            check(l.nodes.empty(), "failure is transactional");
            g.nodes[1].id = "ok";
            g.nodes[1].width = -1;
            check(layout_layered(g, l).error == Error::InvalidNode, "invalid size rejected");
            g.nodes[1].width = 2;
            g.edges.push_back({1, 3});
            check(layout_layered(g, l).error == Error::InvalidEdge, "dangling edge rejected");
            g.edges.clear();
            Options opts; opts.max_nodes = 1;
            check(layout_layered(g, l, opts).error == Error::CapacityExceeded, "capacity rejected");
            opts.max_nodes = 10; opts.layer_gap = -1;
            check(layout_layered(g, l, opts).error == Error::InvalidOptions, "invalid options rejected");
        }
        std::cout << "graphlayout: all checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "graphlayout: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
