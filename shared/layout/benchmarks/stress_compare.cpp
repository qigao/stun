// A repeatable graph-algorithm comparison; all graph models and seeds are
// deterministic. Timings describe one machine, not cross-platform guarantees.
#include "stun/graphlayout/stress.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

using namespace stun::graphlayout;

namespace {
struct Fixture { Graph graph; Layout seed; };
Fixture build(std::size_t n, const std::string& kind) {
    Fixture f;
    for (std::size_t i = 0; i < n; ++i) {
        const double width = 11.0 + static_cast<double>(i % 7);
        const double height = 9.0 + static_cast<double>(i % 5);
        f.graph.nodes.push_back({"n" + std::to_string(i), width, height});
        f.seed.nodes.push_back({static_cast<double>(i * 37 + (i * i) % 19),
                                static_cast<double>((i * 31 + 7) % 47) * 12.0,
                                width, height, 0, 0});
        if (i > 0) f.graph.edges.push_back({i - 1, i});
    }
    if (kind == "ring" && n > 2) f.graph.edges.push_back({n - 1, 0});
    if (kind == "sparse")
        for (std::size_t i = 0; i < n; ++i) {
            if (i + 3 < n && i % 3 == 0) f.graph.edges.push_back({i, i + 3});
            if (i + 5 < n && i % 5 == 0) f.graph.edges.push_back({i + 5, i});
        }
    return f;
}
std::size_t overlaps(const Layout& layout) {
    std::size_t overlap = 0;
    for (std::size_t i = 0; i < layout.nodes.size(); ++i)
        for (std::size_t j = i + 1; j < layout.nodes.size(); ++j) {
            const auto& a=layout.nodes[i];
            const auto& b=layout.nodes[j];
            if (a.x < b.x+b.width && b.x < a.x+a.width &&
                a.y < b.y+b.height && b.y < a.y+a.height) ++overlap;
        }
    return overlap;
}
void run(const Fixture& fixture, const std::string& name,
         const std::string& kind, bool majorization,
         bool collision_projection) {
    StressOptions o;
    
    o.max_iterations=64;
    o.max_pairs=8192;
    o.max_bfs_scans=3000000;
    o.max_linear_iterations=512;
    o.linear_relative_tolerance=1e-11;
    ProjectionConstraints c;
    c.avoid_overlaps=collision_projection;
    const auto start = std::chrono::steady_clock::now();
    StressResult result;
    const auto status=(majorization ? layout_stress_smacof(fixture.graph,fixture.seed,c,result,o)
                                     : layout_stress_gradient(fixture.graph,fixture.seed,c,result,o));
    const auto end = std::chrono::steady_clock::now();
    if (!status) {
        std::cerr << "bench " << name << ' ' << kind << ": " << status.message << '\n';
        std::exit(EXIT_FAILURE);
    }
    const auto ms = std::chrono::duration<double,std::milli>(end-start).count();
    std::cout << name << ',' << kind << ','
              << (majorization ? "smacof" : "gradient")
              << ',' << (collision_projection ? "true" : "false")
              << ',' << fixture.graph.nodes.size() << ',' << fixture.graph.edges.size()
              << ',' << result.initial.pairs
              << ',' << std::setprecision(12) << result.initial.value
              << ',' << result.final.value
              << ',' << result.iterations
              << ',' << result.accepted_steps
              << ',' << result.linear_iterations
              << ',' << overlaps(result.layout)
              << ',' << std::fixed << std::setprecision(3) << ms << '\n';
}
}
int main() {
    std::cout << "case,graph,algorithm,projection,nodes,edges,pairs,initial,final,iterations,accepted,linear_iterations,overlaps,elapsed_ms\n";
    for (auto size : {std::size_t{24},std::size_t{48},std::size_t{72}})
        for (const auto& kind : {std::string{"chain"},std::string{"ring"},std::string{"sparse"}}) {
            const auto f=build(size,kind);
            const auto name="n"+std::to_string(size);
            run(f,name,kind,false,false);
            run(f,name,kind,true,false);
        }
    // Separately measure the cost/quality of true hard geometry constraints.
    const auto constrained=build(24,"sparse");
    run(constrained,"n24","sparse",false,true);
    run(constrained,"n24","sparse",true,true);
    return EXIT_SUCCESS;
}
