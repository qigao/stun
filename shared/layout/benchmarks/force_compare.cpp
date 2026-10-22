// Comparative geometry measurements for three different Stun graph optimizers.
// Stress vs Force energies are DISTINCT objectives and MUST NOT be compared
// against each other; every output is evaluated under both, with separate
// geometric measures. No default CI dependency or upstream vendor code.
#include "stun/graphlayout/force.h"
#include "stun/graphlayout/stress.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>

using namespace stun::graphlayout;

namespace {
struct Fixture { Graph graph; Layout seed; };
Fixture make(std::size_t n, const std::string& kind) {
    Fixture f;
    for (std::size_t i=0;i<n;++i) {
        double w=11.0+static_cast<double>(i%7),h=9.0+static_cast<double>(i%5);
        f.graph.nodes.push_back({"n"+std::to_string(i),w,h});
        const double x = kind == "crowded" ? static_cast<double>(i % 8) * 10.0
                                           : static_cast<double>(i*37+(i*i)%19);
        const double y = kind == "crowded" ? static_cast<double>(i / 8) * 10.0
                                           : static_cast<double>((i*31+7)%47)*12.0;
        f.seed.nodes.push_back({x,y,w,h,0,0});
        if (i>0) f.graph.edges.push_back({i-1,i});
    }
    if (kind=="ring" && n>2) f.graph.edges.push_back({n-1,0});
    if (kind=="sparse" || kind=="crowded")
        for (std::size_t i=0;i<n;++i) {
            if (i+3<n && i%3==0) f.graph.edges.push_back({i,i+3});
            if (i+5<n && i%5==0) f.graph.edges.push_back({i+5,i});
        }
    return f;
}
std::size_t overlaps(const Layout& l) {
    std::size_t count=0;
    for (std::size_t i=0;i<l.nodes.size();++i)
        for (std::size_t j=i+1;j<l.nodes.size();++j) {
            const auto& a=l.nodes[i]; const auto& b=l.nodes[j];
            if (a.x<b.x+b.width && b.x<a.x+a.width &&
                a.y<b.y+b.height && b.y<a.y+a.height) ++count;
        }
    return count;
}
double spring_rmse(const Graph& g,const Layout& l,double desired) {
    std::set<std::pair<std::size_t,std::size_t>> unique;
    for (const auto& e:g.edges) {
        if(e.source==e.target)continue;
        auto a=e.source,b=e.target;
        if(a>b)std::swap(a,b);
        unique.emplace(a,b);
    }
    if(unique.empty())return 0;
    double sum=0;
    for(const auto& e:unique) {
        const auto& a=l.nodes[e.first];const auto& b=l.nodes[e.second];
        const double dx=a.x+a.width/2-b.x-b.width/2;
        const double dy=a.y+a.height/2-b.y-b.height/2;
        const double delta=std::hypot(dx,dy)-desired;
        sum+=delta*delta;
    }
    return std::sqrt(sum/static_cast<double>(unique.size()));
}
void compare(const Fixture& f,const std::string& kind,const std::string& method) {
    constexpr double ideal_length=80.0;
    ProjectionConstraints constraints;constraints.avoid_overlaps=false;
    Layout layout;std::size_t iterations=0,accepted=0;
    const auto begin=std::chrono::steady_clock::now();
    if(method=="force") {
        ForceOptions opt;opt.max_iterations=64;
        ForceResult out;
        const auto status=layout_force(f.graph,f.seed,constraints,out,opt);
        if(!status){std::cerr<<"force benchmark: "<<status.message<<'\n';std::exit(EXIT_FAILURE);}
        layout=out.layout;iterations=out.iterations;accepted=out.accepted_steps;
    } else {
        StressOptions opt;opt.max_iterations=64;
        StressResult out;
        const auto status=(method=="smacof" ? layout_stress_smacof(f.graph,f.seed,constraints,out,opt)
                                       : layout_stress_gradient(f.graph,f.seed,constraints,out,opt));
        if(!status){std::cerr<<"stress benchmark: "<<status.message<<'\n';std::exit(EXIT_FAILURE);}
        layout=out.layout;iterations=out.iterations;accepted=out.accepted_steps;
    }
    const auto elapsed=std::chrono::duration<double,std::milli>(
        std::chrono::steady_clock::now()-begin).count();
    StressEvaluation stress;
    ForceEnergy force;
    if(!evaluate_stress(f.graph,layout,stress)||!evaluate_force(f.graph,layout,force)){
        std::cerr<<"benchmark cross-evaluation failed\n";std::exit(EXIT_FAILURE);
    }
    std::cout<<f.graph.nodes.size()<<','<<kind<<','<<method<<','<<iterations<<','<<accepted<<','
             <<std::setprecision(12)<<stress.value<<','<<force.total<<','
             <<spring_rmse(f.graph,layout,ideal_length)<<','<<overlaps(layout)<<','
             <<std::fixed<<std::setprecision(3)<<elapsed<<'\n';
}
}
int main() {
    std::cout<<"nodes,graph,algorithm,iterations,accepted,stress_energy,force_energy,edge_length_rmse,overlaps,elapsed_ms\n";
    for(auto n:{std::size_t{24},std::size_t{48},std::size_t{72}})
        for(const auto& kind:{std::string{"chain"},std::string{"ring"},std::string{"sparse"}}){
            const auto f=make(n,kind);
            for(const auto& alg:{std::string{"gradient"},std::string{"smacof"},std::string{"force"}})
                compare(f,kind,alg);
        }
    for (auto n:{std::size_t{24},std::size_t{48}}) {
        const auto f=make(n,"crowded");
        for (const auto& alg:{std::string{"gradient"},std::string{"smacof"},std::string{"force"}})
            compare(f,"crowded",alg);
    }
    return EXIT_SUCCESS;
}
