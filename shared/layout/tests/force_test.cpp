#include "stun/graphlayout/force.h"

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
void require(bool ok, const char* why) {
    if (!ok) throw std::runtime_error(why);
}
void close(double a, double b, double tol, const char* why) {
    if (!std::isfinite(a) || std::abs(a - b) > tol)
        throw std::runtime_error(std::string(why) + ": got " + std::to_string(a) +
                                 ", wanted " + std::to_string(b));
}
struct Fixture { Graph g; Layout l; };
Fixture make(std::vector<Node> nodes,
             const std::vector<std::pair<double,double>>& xy,
             std::vector<Edge> edges) {
    require(nodes.size() == xy.size(), "fixture size mismatch");
    Fixture f;
    f.g.nodes = std::move(nodes);
    f.g.edges = std::move(edges);
    for (std::size_t i = 0; i < xy.size(); ++i)
        f.l.nodes.push_back({xy[i].first, xy[i].second,
                             f.g.nodes[i].width, f.g.nodes[i].height,
                             100 + i, 50 + i});
    return f;
}
ForceResult run(const Fixture& f, const ProjectionConstraints& requirements = {},
                const ForceOptions& options = {}) {
    ForceResult result;
    const auto st = layout_force(f.g, f.l, requirements, result, options);
    if (!st) throw std::runtime_error("force: " + st.message);
    require(result.layout.nodes.size() == f.l.nodes.size(), "node count and original indices");
    for (std::size_t i = 0; i < f.l.nodes.size(); ++i) {
        const auto& a = result.layout.nodes[i];
        const auto& b = f.l.nodes[i];
        require(std::isfinite(a.x) && std::isfinite(a.y), "finite output");
        require(a.width == b.width && a.height == b.height &&
                a.rank == b.rank && a.scc == b.scc,
                "identity, dimensions and topology metadata retained");
    }
    require(result.accepted_energies.size() == result.accepted_steps + 1,
            "accepted trace size");
    close(result.accepted_energies.front(), result.initial.total, 0.0,
          "initial accepted energy equals independently initialized energy");
    close(result.accepted_energies.back(), result.final.total,
          1e-9 * std::max(1.0, result.final.total), "final energy trace");
    for (std::size_t i = 1; i < result.accepted_energies.size(); ++i)
        require(result.accepted_energies[i] < result.accepted_energies[i-1],
                "each accepted force iteration strictly decreases actual energy");
    ForceEnergy independent;
    require(static_cast<bool>(evaluate_force(f.g, result.layout, independent, options)),
            "independent final objective evaluation succeeds");
    close(result.final.total, independent.total,
          1e-9 * std::max(1.0, independent.total), "independent total objective");
    close(result.final.total, result.final.spring + result.final.repulsion,
          1e-9 * std::max(1.0, result.final.total), "energy decomposes");
    return result;
}
std::map<std::string,std::pair<double,double>> positions(const Fixture& f, const ForceResult& result) {
    std::map<std::string,std::pair<double,double>> map;
    for (std::size_t i = 0; i < f.g.nodes.size(); ++i)
        map[f.g.nodes[i].id] = {result.layout.nodes[i].x, result.layout.nodes[i].y};
    return map;
}
void no_overlap(const Layout& l, double gap) {
    for (std::size_t i = 0; i < l.nodes.size(); ++i)
        for (std::size_t j = i+1; j < l.nodes.size(); ++j) {
            const auto& a=l.nodes[i]; const auto& b=l.nodes[j];
            require(a.x+a.width+gap<=b.x+1e-6 || b.x+b.width+gap<=a.x+1e-6 ||
                    a.y+a.height+gap<=b.y+1e-6 || b.y+b.height+gap<=a.y+1e-6,
                    "projected force rectangles do not overlap");
        }
}
}

int main() {
    try {
        {
            auto f=make({}, {}, {});
            const auto result=run(f);
            require(result.termination==ForceTermination::NoInteractions &&
                    result.accepted_energies.size()==1 &&
                    result.final.total==0.0 && result.layout.nodes.empty(),
                    "empty graph is a valid empty solution");
        }
        {
            // Verify the documented formula on two nodes; duplicate edges,
            // reversals and self-loops must not duplicate spring contributions.
            auto f=make({{"a",10,10},{"b",10,10}},{{0,0},{100,0}},
                        {{0,1},{1,0},{1,1},{0,1}});
            ForceEnergy e;
            require(static_cast<bool>(evaluate_force(f.g,f.l,e)), "force objective evaluation");
            const double expected_repulsion=0.04 * 80.0*80.0*80.0 / std::hypot(100.0,8.0);
            close(e.spring,0.5*20.0*20.0,1e-10,"analytic spring energy");
            close(e.repulsion,expected_repulsion,1e-10,"analytic Coulomb energy");
            require(e.spring_edges==1 && e.repulsive_pairs==1, "canonical edge deduplication");
        }
        {
            // Check the actual derivative numerically, including the sign
            // of Coulomb repulsion and the restoring edge spring force.
            auto f=make({{"c",8,6},{"a",10,14},{"b",12,10}},
                        {{-12,43},{116,-25},{160,85}},{{0,1},{1,2}});
            ForceEnergy energy; ForceGradient gradient;
            require(static_cast<bool>(evaluate_force_gradient(f.g,f.l,energy,gradient)),
                    "analytic force gradient exists");
            const double step=1e-4;
            for (std::size_t i=0;i<f.l.nodes.size();++i) {
                for (int axis=0;axis<2;++axis) {
                    auto left=f.l, right=f.l;
                    double& a=axis==0?left.nodes[i].x:left.nodes[i].y;
                    double& b=axis==0?right.nodes[i].x:right.nodes[i].y;
                    a-=step; b+=step;
                    ForceEnergy minus,plus;
                    require(static_cast<bool>(evaluate_force(f.g,left,minus)) &&
                            static_cast<bool>(evaluate_force(f.g,right,plus)),
                            "finite difference evaluation");
                    const double numerical=(plus.total-minus.total)/(2*step);
                    const double analytic=axis==0?gradient.dx[i]:gradient.dy[i];
                    close(analytic,numerical,2e-6*std::max(1.0,std::abs(numerical)),
                          "Coulomb plus spring analytic derivative matches finite difference");
                }
            }
        }
        {
            auto f=make({{"a",10,10},{"b",10,10}},{{0,0},{300,0}},{{0,1}});
            ProjectionConstraints c; c.avoid_overlaps=false;
            const auto result=run(f,c);
            require(result.accepted_steps>=1 && result.final.total < result.initial.total,
                    "attractive spring + repulsion lower energy");
            const double distance=std::abs(result.layout.nodes[1].x-result.layout.nodes[0].x);
            require(distance>80.0 && distance<140.0, "Coulomb shifts the spring equilibrium outward");
            close(result.layout.nodes[1].x+result.layout.nodes[0].x,300,1e-7,
                  "free pair center of mass preserved");
        }
        {
            // Repulsion acts on disconnected components as well.
            auto f=make({{"a",10,10},{"b",10,10}},{{0,0},{95,0}},{});
            ProjectionConstraints c; c.avoid_overlaps=false;
            auto result=run(f,c);
            require(result.initial.spring_edges==0 && result.final.spring_edges==0,
                    "disconnected graph has no artificial spring");
            require(result.layout.nodes[1].x-result.layout.nodes[0].x>95.0,
                    "unconnected bodies repel each other");
        }
        {
            auto f=make({{"a",12,12},{"b",12,12},{"c",12,12}},
                        {{0,0},{150,60},{85,230}},{{0,1},{1,2},{2,0},{1,0},{0,0}});
            auto g=make({{"c",12,12},{"a",12,12},{"b",12,12}},
                        {{85,230},{0,0},{150,60}},{{1,2},{2,0},{0,1},{2,1},{1,1}});
            ProjectionConstraints c; c.avoid_overlaps=false;
            const auto a=run(f,c),b=run(g,c);
            const auto f_positions=positions(f,a),g_positions=positions(g,b);
            for (const auto& row : f_positions) {
                const auto& p=g_positions.at(row.first);
                close(p.first,row.second.first,1e-7,"permuted stable-id force x");
                close(p.second,row.second.second,1e-7,"permuted stable-id force y");
            }
        }
        {
            auto f=make({{"p",20,20},{"q",20,20},{"r",24,16}},
                        {{0,0},{130,0},{250,45}},{{0,1},{1,2}});
            ProjectionConstraints c;
            c.pins={{0,0,0}};
            c.alignments={{ProjectionAxis::Y,0,1,0.0}};
            c.separations={{ProjectionAxis::X,0,1,16.0}};
            const auto result=run(f,c);
            close(result.layout.nodes[0].x,0,0,"hard pinned force x");
            close(result.layout.nodes[0].y,0,0,"hard pinned force y");
            close(result.layout.nodes[1].y,0,1e-6,"hard center alignment");
            require(result.layout.nodes[1].x>=36.0-1e-6,"hard rectangle gap");
            no_overlap(result.layout,8.0);
        }
        {
            auto f=make({{"a",12,12},{"b",12,12}},{{0,0},{0,0}},{{0,1}});
            ProjectionConstraints c; c.avoid_overlaps=false;
            ForceResult out;
            auto status=layout_force(f.g,f.l,c,out);
            require(status.error==ForceError::InvalidGeometry && out.layout.nodes.empty(),
                    "coincident seed fails without fabricated jitter");
            c.avoid_overlaps=true;
            auto result=run(f,c);
            no_overlap(result.layout,8.0);
        }
        {
            auto f=make({{"a",10,10},{"b",10,10},{"c",10,10}},
                        {{0,0},{100,0},{200,0}},{{0,1},{1,2}});
            ForceResult out;
            ForceOptions opt; opt.max_pairs=2;
            require(layout_force(f.g,f.l,{},out,opt).error==ForceError::CapacityExceeded &&
                    out.layout.nodes.empty(),"repulsion capacity checked before compute");
            opt.max_pairs=3; opt.max_pair_evaluations=2;
            require(layout_force(f.g,f.l,{},out,opt).error==ForceError::CapacityExceeded &&
                    out.layout.nodes.empty(),"pair evaluation budget enforced");
            opt.max_pair_evaluations=7;
            require(layout_force(f.g,f.l,{},out,opt).error==ForceError::CapacityExceeded &&
                    out.layout.nodes.empty() && out.accepted_energies.empty(),
                    "mid-search resource budget cannot leak partial accepted state");
            opt.max_pair_evaluations=100000;
            opt.repulsion_strength=-1;
            ForceEnergy previous_energy;
            previous_energy.total=123.0;
            require(evaluate_force(f.g,f.l,previous_energy,opt).error==ForceError::InvalidOptions &&
                    previous_energy.total==123.0,
                    "independent evaluator never mutates output on invalid input");
            require(layout_force(f.g,f.l,{},out,opt).error==ForceError::InvalidOptions,
                    "negative electrostatic coefficient rejected");
            opt.repulsion_strength=0.04; opt.softening=0;
            require(layout_force(f.g,f.l,{},out,opt).error==ForceError::InvalidOptions,
                    "singular Coulomb softening rejected");
            opt.softening=8;
            f.g.edges.push_back({0,4});
            require(layout_force(f.g,f.l,{},out,opt).error==ForceError::InvalidInput &&
                    out.accepted_energies.empty(),"unknown endpoint rejected transactionally");
        }
        {
            auto f=make({{"a",20,20},{"b",20,20}},{{0,0},{50,0}},{{0,1}});
            ProjectionConstraints c; c.pins={{0,0,0},{1,0,0}};
            ForceResult out;
            auto status=layout_force(f.g,f.l,c,out);
            require(status.error==ForceError::ProjectionFailed &&
                    status.projection_error==ProjectionError::Infeasible &&
                    out.layout.nodes.empty(), "impossible hard pins fail without fallback");
        }
        {
            // Nontrivial deterministic cyclic families, different dimensions,
            // edge reversals, disconnected parts, self-loops and orientations.
            for (std::size_t t=0;t<30;++t) {
                const std::size_t n=6+t%13;
                std::vector<Node> nodes;
                std::vector<std::pair<double,double>> xy;
                std::vector<Edge> edges;
                for (std::size_t i=0;i<n;++i) {
                    nodes.push_back({"n"+std::to_string(i),8.0+i%5,9.0+i%7});
                    xy.push_back({static_cast<double>((i*43+t*11)%97)*6.0,
                                  static_cast<double>((i*59+t*19)%131)*5.0});
                    if (i>0) edges.push_back({i-1,i});
                    if ((i+t)%3==0) edges.push_back({i,(i+3)%n});
                    if ((i+t)%7==0) edges.push_back({i,i});
                }
                auto f=make(std::move(nodes),xy,std::move(edges));
                ProjectionConstraints c; c.avoid_overlaps=false;
                ForceOptions opt; opt.max_iterations=32;
                const auto result=run(f,c,opt);
                require(result.final.total<=result.initial.total,
                        "family optimization cannot increase actual potential");
                require(result.final.repulsive_pairs==n*(n-1)/2,
                        "all-pairs repulsion includes every graph node");
            }
        }
        std::cout << "force: all checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "force: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
