#include "stun/graphlayout/projection.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace stun::graphlayout;

namespace {
void check(bool ok, const char* msg) { if (!ok) throw std::runtime_error(msg); }
void near(double actual, double expected, double tol, const char* msg) {
    if (!std::isfinite(actual) || std::abs(actual-expected) > tol)
        throw std::runtime_error(std::string(msg) + " got " + std::to_string(actual) +
                                 " expected " + std::to_string(expected));
}

std::pair<Graph, Layout> graph_at(const std::vector<Node>& nodes,
                                  const std::vector<std::pair<double,double>>& xy) {
    check(nodes.size() == xy.size(), "fixture size");
    Graph graph;
    graph.nodes = nodes;
    Layout layout;
    for (std::size_t i = 0; i < xy.size(); ++i) {
        layout.nodes.push_back({xy[i].first, xy[i].second, nodes[i].width, nodes[i].height, i, i});
    }
    return {graph, layout};
}
void nonoverlap(const Layout& l, double clearance) {
    for (std::size_t i = 0; i < l.nodes.size(); ++i)
        for (std::size_t j = i+1; j < l.nodes.size(); ++j) {
            const auto& a = l.nodes[i]; const auto& b = l.nodes[j];
            check(a.x + a.width + clearance <= b.x ||
                  b.x + b.width + clearance <= a.x ||
                  a.y + a.height + clearance <= b.y ||
                  b.y + b.height + clearance <= a.y,
                  "nodes must be non-overlapping with exact clearance");
        }
}
Layout projected(const Graph& g, const Layout& desired,
                 const ProjectionConstraints& cs, ProjectionReport* report=nullptr) {
    Layout l;
    const auto status = project_graph(g, desired, cs, l, {}, report);
    if (!status) throw std::runtime_error("project_graph: " + status.message);
    check(l.nodes.size() == desired.nodes.size(), "node identity preserved");
    for (std::size_t i = 0; i < l.nodes.size(); ++i) {
        check(l.nodes[i].rank == desired.nodes[i].rank &&
              l.nodes[i].scc == desired.nodes[i].scc,
              "rank and SCC metadata preserved");
        near(l.nodes[i].width, desired.nodes[i].width, 0, "width stable");
        near(l.nodes[i].height, desired.nodes[i].height, 0, "height stable");
    }
    return l;
}
}

int main() {
    try {
        {
            auto [g, base] = graph_at({{"a",10,10},{"b",10,10}}, {{0,0},{30,0}});
            auto l = projected(g, base, {});
            near(l.nodes[0].x, 0, 0, "already valid layout unchanged");
            near(l.nodes[1].x, 30, 0, "already separated layout unchanged");
            nonoverlap(l, 8);
        }
        {
            auto [g, base] = graph_at({{"a",10,10},{"b",10,10}}, {{0,0},{1,0}});
            ProjectionReport report;
            auto l = projected(g, base, {}, &report);
            nonoverlap(l, 8);
            check(report.generated_separations == 1 && report.passes == 2,
                  "overlapping pair generates exactly one axis constraint");
        }
        {
            auto [g, base] = graph_at({{"anchor",10,10},{"free",10,10},{"far",10,10}},
                                      {{0,0},{1,0},{0,100}});
            ProjectionConstraints c;
            c.pins.push_back({0,0,0});
            auto l = projected(g, base, c);
            near(l.nodes[0].x, 0, 0, "pinned x exact");
            near(l.nodes[0].y, 0, 0, "pinned y exact");
            near(l.nodes[2].y, 100, 0, "isolated node retains desired center");
            nonoverlap(l, 8);
        }
        {
            // X alignment forbids horizontal separation; non-overlap must
            // separate the nodes vertically instead.
            auto [g, base] = graph_at({{"a",12,12},{"b",12,12}}, {{0,0},{1,0}});
            ProjectionConstraints c;
            c.alignments.push_back({ProjectionAxis::X,0,1,0});
            auto l = projected(g, base, c);
            near(l.nodes[0].x, l.nodes[1].x, 1e-7, "center x alignment");
            nonoverlap(l, 8);
        }
        {
            // Indirect alignment: A == B == C on X must not create a
            // spurious X separation between A and C. The independent Y
            // projection resolves the disjunctive non-overlap safely.
            auto [g, base] = graph_at({{"a",10,10},{"b",10,10},{"c",10,10}},
                                       {{0,0},{0,1},{0,2}});
            ProjectionConstraints c;
            c.alignments.push_back({ProjectionAxis::X,0,1,0});
            c.alignments.push_back({ProjectionAxis::X,1,2,0});
            auto l = projected(g, base, c);
            for (std::size_t i = 1; i < 3; ++i)
                near(l.nodes[i].x,l.nodes[0].x,1e-7,"transitive x rigidity");
            nonoverlap(l,8);
        }
        {
            // A hard pin plus an offset-aligned node creates an indirectly
            // pinned coordinate; avoid impossible-axis separation.
            auto [g, base] = graph_at({{"a",10,10},{"b",10,10},{"c",10,10}},
                                       {{0,0},{0,0},{1,0}});
            ProjectionConstraints c;
            c.pins.push_back({0,0,0});
            c.alignments.push_back({ProjectionAxis::X,0,1,0});
            c.alignments.push_back({ProjectionAxis::X,1,2,0});
            auto l = projected(g, base, c);
            near(l.nodes[0].x,0,0,"hard pin origin x");
            near(l.nodes[0].y,0,0,"hard pin origin y");
            near(l.nodes[2].x,0,1e-7,"propagated fixed x");
            nonoverlap(l,8);
        }
        {
            // Alignment and separation are independent of the box widths.
            auto [g, base] = graph_at({{"left",20,10},{"right",40,20}}, {{0,0},{0,40}});
            ProjectionConstraints c;
            c.avoid_overlaps = false;
            c.alignments.push_back({ProjectionAxis::Y,0,1,0});
            c.separations.push_back({ProjectionAxis::X,0,1,12});
            auto l = projected(g, base, c);
            near((l.nodes[1].y+l.nodes[1].height/2) -
                 (l.nodes[0].y+l.nodes[0].height/2), 0, 1e-6, "center alignment");
            check(l.nodes[1].x - (l.nodes[0].x+l.nodes[0].width) >= 12-1e-6,
                  "explicit minimum rectangle gap enforced");
        }
        {
            auto [g, base] = graph_at({{"a",10,10},{"b",10,10}}, {{0,0},{0,0}});
            ProjectionConstraints c;
            c.pins = {{0,0,0},{1,0,0}};
            Layout l;
            const auto status = project_graph(g,base,c,l);
            check(status.error == ProjectionError::Infeasible, "pinned collision explicitly infeasible");
            check(l.nodes.empty(), "failed projection is atomic");
            c.pins.clear();
            c.alignments={{ProjectionAxis::X,0,1,0},{ProjectionAxis::Y,0,1,0}};
            check(project_graph(g,base,c,l).error == ProjectionError::Infeasible,
                  "both-axis alignment cannot be nonoverlapping");
            check(l.nodes.empty(), "alignment failure is atomic");
        }
        {
            auto [g, base] = graph_at({{"a",10,10},{"b",10,10}}, {{0,0},{30,0}});
            ProjectionConstraints c;
            c.pins={{0,7,3}};
            auto l = projected(g,base,c);
            near(l.nodes[0].x, 7, 0, "pinned x offset exact");
            near(l.nodes[0].y, 3, 0, "pinned y offset exact");
            nonoverlap(l,8);
            c.pins={{0,0,0},{0,0,0}};
            Layout failed;
            check(project_graph(g,base,c,failed).error == ProjectionError::InvalidInput,
                  "duplicate pin rejected even at same position");
            c.pins.clear();
            g.edges.push_back({1, 2});
            check(project_graph(g,base,c,failed).error == ProjectionError::InvalidInput,
                  "dangling graph edge rejected");
            g.edges.clear();
            c.separations={{ProjectionAxis::X,0,2,0}};
            check(project_graph(g,base,c,failed).error == ProjectionError::InvalidInput,
                  "dangling axis constraint rejected");
        }
        {
            // Failure to allocate the required pair budget is not a partial success.
            auto [g, base] = graph_at({{"a",10,10},{"b",10,10}}, {{0,0},{0,0}});
            ProjectionOptions opt;
            opt.max_passes=1;
            Layout l;
            check(project_graph(g,base,{},l,opt).error == ProjectionError::IterationLimit,
                  "projection pass capacity enforced");
            check(l.nodes.empty(), "limited projection resets output");
            opt.max_passes=2; opt.max_pair_checks=1;
            check(project_graph(g,base,{},l,opt).error == ProjectionError::CapacityExceeded,
                  "total pair checks bound");
        }
        {
            // Repeatable lexical ID order should not depend on graph storage order.
            auto [ga, la] = graph_at({{"a",10,10},{"b",15,10},{"c",10,12}},
                                     {{0,0},{3,3},{8,8}});
            auto [gb, lb] = graph_at({{"c",10,12},{"a",10,10},{"b",15,10}},
                                     {{8,8},{0,0},{3,3}});
            const auto a = projected(ga,la,{});
            const auto b = projected(gb,lb,{});
            for (std::size_t i=0; i<ga.nodes.size(); ++i)
                for (std::size_t j=0; j<gb.nodes.size(); ++j)
                    if (ga.nodes[i].id==gb.nodes[j].id) {
                        near(a.nodes[i].x,b.nodes[j].x,1e-7,"permuted x");
                        near(a.nodes[i].y,b.nodes[j].y,1e-7,"permuted y");
                    }
        }
        std::cout << "projection: all checks passed\n";
        return EXIT_SUCCESS;
    } catch(const std::exception& e) {
        std::cerr << "projection: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
