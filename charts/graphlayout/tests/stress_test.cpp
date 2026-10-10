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
void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
void near(double a, double b, double tolerance, const char* message) {
    if (!std::isfinite(a) || std::abs(a - b) > tolerance)
        throw std::runtime_error(std::string(message) + ": " + std::to_string(a) +
                                 " vs " + std::to_string(b));
}

struct Fixture { Graph graph; Layout seed; };
Fixture fixture(std::vector<Node> nodes,
                std::vector<std::pair<double,double>> positions,
                std::vector<Edge> edges) {
    check(nodes.size() == positions.size(), "fixture input sizes");
    Fixture result;
    result.graph.nodes = std::move(nodes);
    result.graph.edges = std::move(edges);
    for (std::size_t i = 0; i < positions.size(); ++i)
        result.seed.nodes.push_back({positions[i].first, positions[i].second,
                                      result.graph.nodes[i].width,
                                      result.graph.nodes[i].height, i, i});
    return result;
}
StressResult run(const Fixture& f, const ProjectionConstraints& constraints = {},
                 const StressOptions& options = {}) {
    StressResult result;
    const auto status = layout_stress(f.graph, f.seed, constraints, result, options);
    if (!status) throw std::runtime_error("layout_stress: " + status.message);
    check(result.layout.nodes.size() == f.graph.nodes.size(), "original index mapping");
    for (std::size_t i = 0; i < result.layout.nodes.size(); ++i) {
        const auto& a = result.layout.nodes[i];
        check(a.rank == f.seed.nodes[i].rank && a.scc == f.seed.nodes[i].scc,
              "node graph metadata preserved");
        check(a.width == f.seed.nodes[i].width && a.height == f.seed.nodes[i].height,
              "rectangle sizes preserved");
        check(std::isfinite(a.x) && std::isfinite(a.y), "finite node positions");
    }
    check(result.initial.pairs == result.final.pairs, "pair set invariant");
    check(result.accepted_objectives.size() == result.accepted_steps + 1,
          "energy trace includes exactly accepted steps");
    near(result.accepted_objectives.front(), result.initial.value, 0.0,
         "initial trace energy");
    near(result.accepted_objectives.back(), result.final.value, 1e-10 *
         std::max(1.0, result.final.value), "final trace energy");
    for (std::size_t i = 1; i < result.accepted_objectives.size(); ++i)
        check(result.accepted_objectives[i] < result.accepted_objectives[i - 1],
              "objective strictly decreases at each accepted step");
    StressEvaluation external;
    check(static_cast<bool>(evaluate_stress(f.graph, result.layout, external, options)),
          "separately evaluated objective must succeed");
    near(result.final.value, external.value, 1e-10 * std::max(1.0, external.value),
         "independent objective audit");
    return result;
}

void check_no_overlap(const Layout& l, double gap) {
    for (std::size_t i = 0; i < l.nodes.size(); ++i)
        for (std::size_t j = i + 1; j < l.nodes.size(); ++j) {
            const auto& a = l.nodes[i];
            const auto& b = l.nodes[j];
            check(a.x+a.width+gap <= b.x + 1e-6 || b.x+b.width+gap <= a.x + 1e-6 ||
                  a.y+a.height+gap <= b.y + 1e-6 || b.y+b.height+gap <= a.y + 1e-6,
                  "stress with projection must avoid rectangle overlap");
        }
}
std::map<std::string,std::pair<double,double>> by_id(const Fixture& f, const StressResult& r) {
    std::map<std::string,std::pair<double,double>> result;
    for (std::size_t i = 0; i < f.graph.nodes.size(); ++i)
        result[f.graph.nodes[i].id] = {r.layout.nodes[i].x, r.layout.nodes[i].y};
    return result;
}
} // namespace

int main() {
    try {
        {
            const Fixture f = fixture({}, {}, {});
            const auto r = run(f);
            check(r.layout.nodes.empty() && r.termination == StressTermination::NoPairs,
                  "empty graph succeeds without fabricated geometry");
        }
        {
            auto f = fixture({{"a",10,10},{"b",10,10}}, {{0,0},{300,0}}, {{0,1}});
            ProjectionConstraints c;
            c.avoid_overlaps = false;
            const auto result = run(f,c);
            check(result.accepted_steps >= 1, "long spring must be shortened by descent");
            check(result.final.value < 1e-5 * result.initial.value,
                  "two-node stress approaches analytic optimum");
            near(result.layout.nodes[1].x - result.layout.nodes[0].x, 80.0, 0.1,
                 "two-node equilibrium distance");
            near(result.layout.nodes[0].x + result.layout.nodes[1].x, 300.0, 1e-7,
                 "unconstrained center of mass preserved");
        }
        {
            // A->B->C shortest-path stress includes the A-C distance 2*L.
            auto f=fixture({{"A",8,8},{"B",10,8},{"C",8,8}},
                           {{0,0},{400,0},{15,300}},{{0,1},{1,2}});
            ProjectionConstraints c; c.avoid_overlaps=false;
            auto r=run(f,c);
            check(r.initial.pairs==3,"shortest-path all-pairs objective is used");
            check(r.final.value < r.initial.value * 0.02,
                  "multi-hop chain stress improves significantly");
        }
        {
            // Edge orientation does not influence the undirected graph stress.
            auto f=fixture({{"a",12,9},{"b",12,9},{"c",12,9}},
                           {{0,0},{200,40},{100,230}},{{0,1},{1,2},{2,0}});
            auto g=fixture({{"c",12,9},{"a",12,9},{"b",12,9}},
                           {{100,230},{0,0},{200,40}},{{1,0},{2,1},{0,2}});
            ProjectionConstraints c; c.avoid_overlaps=false;
            const auto a=run(f,c), b=run(g,c);
            check(a.initial.pairs==3 && a.final.value < a.initial.value,
                  "three-cycle stress descent");
            const auto pa=by_id(f,a), pb=by_id(g,b);
            for (const auto& entry:pa) {
                near(entry.second.first,pb.at(entry.first).first,1e-7,
                     "input node/edge permutation stable x");
                near(entry.second.second,pb.at(entry.first).second,1e-7,
                     "input node/edge permutation stable y");
            }
        }
        {
            // Connected nodes can be pinned and constrained without fake
            // infinite weights; every line search trial uses native VPSC.
            auto f=fixture({{"left",20,20},{"right",20,20},{"tail",20,20}},
                           {{0,0},{110,0},{210,25}},{{0,1},{1,2}});
            ProjectionConstraints cs;
            cs.pins.push_back({0,0,0});
            cs.alignments.push_back({ProjectionAxis::Y,0,1,0.0});
            cs.separations.push_back({ProjectionAxis::X,0,1,14.0});
            auto r=run(f,cs);
            near(r.layout.nodes[0].x,0,0,"pinned top-left x exact");
            near(r.layout.nodes[0].y,0,0,"pinned top-left y exact");
            near(r.layout.nodes[1].y,0,1e-6,"aligned centers remain exact");
            check(r.layout.nodes[1].x>=34-1e-6,"hard min separation maintained");
            check_no_overlap(r.layout,8);
        }
        {
            // Disconnected pairs have no synthetic spring, preserving isolated
            // nodes absent externally requested geometry constraints.
            auto f=fixture({{"a",8,8},{"z",8,8},{"b",8,8}},
                           {{0,0},{300,450},{200,0}},{{0,2}});
            ProjectionConstraints c; c.avoid_overlaps=false;
            auto r=run(f,c);
            near(r.layout.nodes[1].x,300,0,"disconnected isolated x stable");
            near(r.layout.nodes[1].y,450,0,"disconnected isolated y stable");
        }
        {
            auto f=fixture({{"a",12,12},{"b",12,12}},{{0,0},{0,0}},{{0,1}});
            ProjectionConstraints c; c.avoid_overlaps=false;
            StressResult rejected;
            check(layout_stress(f.graph,f.seed,c,rejected).error==StressError::InvalidGeometry,
                  "degenerate seed must fail, not jitter randomly");
            check(rejected.layout.nodes.empty(),"failure clears output");
            c.avoid_overlaps=true;
            auto r=run(f,c);
            check_no_overlap(r.layout,8);
            check(r.initial.pairs==1, "initial projection resolves exact overlap");
        }
        {
            auto f=fixture({{"a",10,10},{"b",10,10},{"c",10,10}},
                           {{0,0},{100,0},{210,0}}, {{0,1},{1,2}});
            StressResult rejected;
            StressOptions options; options.max_pairs=1;
            check(layout_stress(f.graph,f.seed,{},rejected,options).error==StressError::CapacityExceeded,
                  "all-pair memory budget enforced");
            check(rejected.layout.nodes.empty(),"pair budget failure transactional");
            options.max_pairs=10; options.max_bfs_scans=1;
            check(layout_stress(f.graph,f.seed,{},rejected,options).error==StressError::CapacityExceeded,
                  "BFS visit budget enforced");
            options.max_bfs_scans=100; options.initial_step=0;
            check(layout_stress(f.graph,f.seed,{},rejected,options).error==StressError::InvalidOptions,
                  "step budget must be positive");
            options.initial_step=0.5;
            auto broken=f;
            broken.graph.edges.push_back({2,4});
            check(layout_stress(broken.graph,broken.seed,{},rejected,options).error==StressError::InvalidInput,
                  "invalid endpoints rejected");
        }
        {
            auto f=fixture({{"a",10,10},{"b",10,10}},{{0,0},{30,0}},{{0,1}});
            ProjectionConstraints cs;
            cs.pins={{0,0,0},{1,0,0}};
            StressResult rejected;
            auto status=layout_stress(f.graph,f.seed,cs,rejected);
            check(status.error==StressError::ProjectionFailed &&
                  status.projection_error==ProjectionError::Infeasible,
                  "impossible hard pins propagate exact infeasibility");
            check(rejected.layout.nodes.empty(),"failure must be atomic");
        }
        {
            // Deterministic graph-family qualification: edge reversals,
            // self-loops, multiple cycles and varied initial geometries.
            // Every accepted iterate must be independently energy audited.
            std::size_t successes = 0;
            for (std::size_t case_id = 0; case_id < 40; ++case_id) {
                const std::size_t n = 6 + case_id % 15;
                std::vector<Node> nodes;
                std::vector<std::pair<double,double>> xy;
                std::vector<Edge> edges;
                for (std::size_t i = 0; i < n; ++i) {
                    nodes.push_back({"test" + std::to_string(i),
                                     8.0 + i % 3, 10.0 + i % 5});
                    xy.emplace_back(static_cast<double>(i * 29 + (i * i) % 17),
                                    static_cast<double>((i * 23 + case_id * 17) % 59) * 7.0);
                    edges.push_back({i, (i + 1) % n});
                    if ((i + case_id) % 3 == 0) edges.push_back({i, (i + 3) % n});
                    if ((i + case_id) % 5 == 0) edges.push_back({i, i});
                }
                auto f = fixture(std::move(nodes), std::move(xy), std::move(edges));
                ProjectionConstraints c; c.avoid_overlaps = false;
                StressOptions opt;
                opt.max_iterations = 60;
                const auto result = run(f, c, opt);
                check(result.final.value <= result.initial.value,
                      "graph family energy cannot increase");
                check(result.final.pairs == n * (n-1) / 2,
                      "cycle-connected graph must use all pairs");
                ++successes;
            }
            check(successes == 40, "deterministic graph-family count");
        }
        std::cout << "stress: all checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "stress: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
