#include "stun/graphlayout/stress.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace stun::graphlayout;

namespace {
void require(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}
void close(double actual, double expected, double tol, const char* what) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > tol)
        throw std::runtime_error(std::string(what) + ": " + std::to_string(actual) +
                                 " vs " + std::to_string(expected));
}
struct Fixture { Graph graph; Layout seed; };
Fixture fixture(std::vector<Node> nodes,
                const std::vector<std::pair<double, double>>& centers,
                std::vector<Edge> edges) {
    require(nodes.size() == centers.size(), "fixture size mismatch");
    Fixture f;
    f.graph.nodes = std::move(nodes);
    f.graph.edges = std::move(edges);
    for (std::size_t i = 0; i < f.graph.nodes.size(); ++i)
        f.seed.nodes.push_back({centers[i].first - f.graph.nodes[i].width / 2.0,
                                centers[i].second - f.graph.nodes[i].height / 2.0,
                                f.graph.nodes[i].width, f.graph.nodes[i].height,
                                i + 10, i + 100});
    return f;
}
StressOptions smacof() {
    StressOptions o;
    o.optimizer = StressOptimizer::SmacofMajorization;
    o.max_iterations = 48;
    o.max_linear_iterations = 384;
    o.linear_relative_tolerance = 1e-11;
    return o;
}
StressResult solve(const Fixture& f, const ProjectionConstraints& c,
                   const StressOptions& o) {
    StressResult result;
    const auto status = layout_stress(f.graph, f.seed, c, result, o);
    if (!status)
        throw std::runtime_error("SMACOF failed: " + status.message);
    require(result.layout.nodes.size() == f.graph.nodes.size(), "graph size preserved");
    require(result.initial.pairs == result.final.pairs, "pair count preserved");
    require(result.linear_solves == result.majorizer_improvements.size(),
            "majorization trace matches linear solves");
    require(result.linear_iterations <= 2 * result.linear_solves * o.max_linear_iterations,
            "linear iterations are within explicit resource budget");
    require(result.accepted_objectives.size() == result.accepted_steps + 1,
            "energy trace contains accepted steps only");
    close(result.accepted_objectives.front(), result.initial.value, 0, "initial energy audit");
    close(result.accepted_objectives.back(), result.final.value,
          1e-10 * std::max(1.0, result.final.value), "final energy audit");
    for (std::size_t i = 1; i < result.accepted_objectives.size(); ++i)
        require(result.accepted_objectives[i] < result.accepted_objectives[i-1],
                "every accepted SMACOF step must decrease actual energy");
    for (const auto improvement : result.majorizer_improvements)
        require(std::isfinite(improvement) && improvement >= 0,
                "every quadratic majorizer step must be non-increasing");
    StressEvaluation external;
    require(static_cast<bool>(evaluate_stress(f.graph, result.layout, external, o)),
            "independent stress audit");
    close(external.value, result.final.value,
          1e-10 * std::max(1.0, external.value), "independent energy agreement");
    for (std::size_t i = 0; i < f.graph.nodes.size(); ++i) {
        const auto& p = result.layout.nodes[i];
        const auto& original = f.seed.nodes[i];
        require(std::isfinite(p.x) && std::isfinite(p.y), "finite geometry");
        require(p.rank == original.rank && p.scc == original.scc,
                "graph metadata remains unchanged");
        require(p.width == original.width && p.height == original.height,
                "node dimensions remain unchanged");
    }
    return result;
}
std::map<std::string, std::pair<double,double>> points(const Fixture& f,
                                                        const StressResult& result) {
    std::map<std::string, std::pair<double,double>> m;
    for (std::size_t i = 0; i < f.graph.nodes.size(); ++i)
        m[f.graph.nodes[i].id] = {result.layout.nodes[i].x, result.layout.nodes[i].y};
    return m;
}
void verify_no_overlap(const Layout& layout, double clearance) {
    for (std::size_t i = 0; i < layout.nodes.size(); ++i)
        for (std::size_t j = i + 1; j < layout.nodes.size(); ++j) {
            const auto& a = layout.nodes[i];
            const auto& b = layout.nodes[j];
            require(a.x + a.width + clearance <= b.x + 1e-6 ||
                    b.x + b.width + clearance <= a.x + 1e-6 ||
                    a.y + a.height + clearance <= b.y + 1e-6 ||
                    b.y + b.height + clearance <= a.y + 1e-6,
                    "VPSC-projected SMACOF nodes may not overlap");
        }
}
}

int main() {
    try {
        {
            auto f = fixture({}, {}, {});
            auto result = solve(f, {}, smacof());
            require(result.layout.nodes.empty() && result.linear_solves == 0,
                    "empty graph has no linear system");
        }
        {
            // The exact two-node SMACOF step reaches target distance in one
            // Laplacian solve. Match its common objective to gradient descent.
            auto f = fixture({{"a",10,10},{"b",10,10}}, {{5,5},{305,5}}, {{0,1}});
            ProjectionConstraints c; c.avoid_overlaps = false;
            StressOptions o = smacof(); o.max_iterations = 1;
            const auto majorized = solve(f, c, o);
            o.optimizer = StressOptimizer::GradientDescent;
            const auto gradient = solve(f, c, o);
            close(majorized.final.value, 0.0, 1e-12, "two-node SMACOF optimum");
            require(majorized.final.value <= gradient.final.value + 1e-9,
                    "SMACOF one-step result should beat one gradient step on 2 nodes");
            close(majorized.layout.nodes[1].x - majorized.layout.nodes[0].x, 80.0,
                  1e-9, "analytic inter-node separation");
            close(majorized.layout.nodes[0].x + majorized.layout.nodes[1].x,
                  f.seed.nodes[0].x + f.seed.nodes[1].x, 1e-9,
                  "unpinned component centroid preserved");
            require(majorized.linear_solves == 1 && majorized.accepted_steps == 1,
                    "one majorization step was accepted");
        }
        {
            // Three-node chain uses 3 shortest-path pairs, including the
            // two-hop A-C term. Translation must not change the objective.
            auto f = fixture({{"a",8,8},{"b",14,9},{"c",12,10}},
                             {{10,0},{400,60},{80,290}}, {{0,1},{1,2}});
            ProjectionConstraints c; c.avoid_overlaps = false;
            const auto a = solve(f,c,smacof());
            require(a.initial.pairs == 3 && a.final.value < a.initial.value * 0.02,
                    "three-node graph-distance stress converges");
            for (auto& p : f.seed.nodes) {p.x+=200; p.y-=300;}
            const auto b=solve(f,c,smacof());
            close(a.final.value, b.final.value, 1e-8 * std::max(1.0,a.final.value),
                  "translation does not alter stress energy");
        }
        {
            auto f = fixture({{"a",10,10},{"b",10,10},{"c",10,10}},
                             {{10,10},{270,60},{150,220}}, {{0,1},{1,2},{2,0}});
            auto g = fixture({{"c",10,10},{"a",10,10},{"b",10,10}},
                             {{150,220},{10,10},{270,60}}, {{1,0},{2,1},{0,2}});
            ProjectionConstraints c; c.avoid_overlaps=false;
            auto a=solve(f,c,smacof()), b=solve(g,c,smacof());
            const auto by_a=points(f,a), by_b=points(g,b);
            for (const auto& kv : by_a) {
                close(kv.second.first, by_b.at(kv.first).first, 1e-7,
                      "permutation stable x");
                close(kv.second.second, by_b.at(kv.first).second, 1e-7,
                      "permutation stable y");
            }
        }
        {
            // Exact Dirichlet pins are retained in the Laplacian solve; VPSC
            // must still enforce alignment/separation and nonoverlap.
            auto f = fixture({{"p",20,20},{"mid",20,20},{"last",20,20}},
                             {{10,10},{180,35},{290,120}}, {{0,1},{1,2}});
            ProjectionConstraints c;
            c.pins.push_back({0,0,0});
            c.alignments.push_back({ProjectionAxis::Y,0,1,0});
            c.separations.push_back({ProjectionAxis::X,0,1,14});
            const auto r=solve(f,c,smacof());
            close(r.layout.nodes[0].x,0.0,0,"exact hard pin x");
            close(r.layout.nodes[0].y,0.0,0,"exact hard pin y");
            close(r.layout.nodes[1].y,0.0,1e-6,"hard center alignment");
            require(r.layout.nodes[1].x >= 34.0-1e-6, "hard minimum gap");
            verify_no_overlap(r.layout,8.0);
        }
        {
            auto f = fixture({{"a",8,8},{"alone",8,8},{"b",8,8}},
                             {{8,8},{350,430},{208,8}}, {{0,2}});
            ProjectionConstraints c; c.avoid_overlaps=false;
            const auto r=solve(f,c,smacof());
            close(r.layout.nodes[1].x,f.seed.nodes[1].x,0,"isolated node stable x");
            close(r.layout.nodes[1].y,f.seed.nodes[1].y,0,"isolated node stable y");
        }
        {
            auto f = fixture({{"a",8,8},{"b",8,8}},{{15,15},{15,15}},{{0,1}});
            ProjectionConstraints c; c.avoid_overlaps=false;
            StressResult rejected;
            require(layout_stress(f.graph,f.seed,c,rejected,smacof()).error ==
                    StressError::InvalidGeometry,
                    "undefined coincident centers fail rather than jitter");
            require(rejected.layout.nodes.empty(), "failure returns empty result");
            c.avoid_overlaps=true;
            const auto r=solve(f,c,smacof());
            verify_no_overlap(r.layout,8.0);
        }
        {
            // A three-dimensional free subspace with a nontrivial Laplacian
            // needs more than one CG iteration; budget exhaustion is typed.
            auto f = fixture({{"a",10,10},{"b",11,10},{"c",12,10},
                              {"d",13,10},{"e",14,10}},
                             {{10,10},{200,50},{340,220},{40,340},{520,110}},
                             {{0,1},{1,2},{2,3},{3,4}});
            StressOptions o=smacof(); o.max_linear_iterations=1;
            o.linear_relative_tolerance=1e-14;
            ProjectionConstraints c; c.avoid_overlaps=false;
            StressResult rejected;
            const auto failed=layout_stress(f.graph,f.seed,c,rejected,o);
            require(failed.error==StressError::LinearSolveLimit,
                    "linear solver budget exhaustion must be typed");
            require(rejected.accepted_objectives.empty() && rejected.layout.nodes.empty(),
                    "linear solver exhaustion is transactional");
            o.max_linear_iterations=384;
            const auto ok=solve(f,c,o);
            require(ok.final.value <= ok.initial.value, "budgeted CG recovers when expanded");
        }
        {
            // Deterministic cyclic and noncyclic graph stress qualification,
            // with varied dimensions, pinned components, and self loops.
            for (std::size_t seed=0; seed<32; ++seed) {
                const std::size_t n=6+(seed%13);
                std::vector<Node> nodes;
                std::vector<std::pair<double,double>> xy;
                std::vector<Edge> edges;
                for (std::size_t i=0; i<n; ++i) {
                    nodes.push_back({"n"+std::to_string(i),9.0+(i%4),8.0+(i%5)});
                    xy.push_back({40.0 + static_cast<double>(i)*47.0 + static_cast<double>((i*i)%11),
                                  25.0 + 19.0 * static_cast<double>((i*19+seed*7)%17)});
                    edges.push_back({i,(i+1)%n});
                    if ((seed+i)%3==0) edges.push_back({i,(i+3)%n});
                    if ((seed+i)%7==0) edges.push_back({i,i});
                }
                auto f=fixture(std::move(nodes),xy,std::move(edges));
                ProjectionConstraints c; c.avoid_overlaps=false;
                StressOptions o=smacof(); o.max_iterations=30;
                const auto r=solve(f,c,o);
                require(r.final.value <= r.initial.value + 1e-8,
                        "bounded random family may not increase stress");
                require(r.final.pairs==n*(n-1)/2,
                        "all connected pairs should contribute");
            }
        }
        std::cout << "smacof: all checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "smacof: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
