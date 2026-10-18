#include "stun/graphlayout/nudging.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <vector>

using namespace stun::graphlayout;

namespace {
void require(bool condition, const char* what) {
    if (!condition) throw std::runtime_error(what);
}
void close(double a, double b, double tolerance, const char* what) {
    if (!std::isfinite(a) || std::abs(a-b)>tolerance)
        throw std::runtime_error(what);
}
Layout rectangles(std::initializer_list<PlacedNode> nodes) {
    Layout l; l.nodes=nodes;
    for (const auto& p : l.nodes) {
        l.width=std::max(l.width,p.x+p.width);
        l.height=std::max(l.height,p.y+p.height);
    }
    return l;
}
void check_geometry(const Layout& layout,const std::vector<RouteRequest>& req,
                    const Routes& routes,const NudgingOptions& options={}) {
    const auto valid=validate_orthogonal_routes(layout,req,routes,options.routing);
    if (!valid) throw std::runtime_error("geometric validator: "+valid.message);
}
void same_routes(const Routes& a,const Routes& b) {
    require(a.edges.size()==b.edges.size(),"deterministic route count");
    for (std::size_t i=0;i<a.edges.size();++i) {
        const auto& x=a.edges[i].points; const auto& y=b.edges[i].points;
        require(x.size()==y.size(),"deterministic path length");
        for(std::size_t j=0;j<x.size();++j)
            require(x[j].x==y[j].x && x[j].y==y[j].y,"deterministic route geometry");
    }
}
}

int main() {
 try {
    {
        const auto l=rectangles({{0,0,40,40},{140,0,40,40}});
        const std::vector<RouteRequest> req{{{0,Side::East},{1,Side::West}},
                                            {{0,Side::East},{1,Side::West}}};
        Routes baseline,result,again;
        NudgingOptions options;
        const auto st=route_orthogonal(l,req,baseline,options.routing);
        require(static_cast<bool>(st),"orthogonal starting routes");
        RouteInteractions audit;
        require(static_cast<bool>(audit_orthogonal_interactions(l,req,baseline,audit,options)),"initial interaction audit");
        close(audit.shared_length,100,1e-9,"duplicated route base overlap");
        NudgingReport report;
        const auto nudged=route_orthogonal_nudged(l,req,result,options,&report);
        if(!nudged) std::cerr<<nudged.message<<'\n';
        require(static_cast<bool>(nudged),"duplicated routes can be unbundled");
        check_geometry(l,req,result,options);
        require(report.initial.crossings==0 && report.final.crossings==0,"no newly created crossings");
        require(report.final.shared_length < report.initial.shared_length,"shared segment length reduced");
        require(report.changed_routes>=1,"an actual nudge is performed");
        require(report.accepted_moves>=1 && report.candidate_trials>=1,"search audited");
        require(result.edges[0].points.front().x==40 &&
                result.edges[1].points.back().x==140,"exact anchors preserved");
        require(static_cast<bool>(route_orthogonal_nudged(l,req,again,options)),"repeat route batch");
        same_routes(result,again);
        RouteInteractions reviewed;
        require(static_cast<bool>(audit_orthogonal_interactions(l,req,result,reviewed,options)),"independent final audit");
        close(reviewed.shared_length,report.final.shared_length,1e-9,"independent report" );
    }
    {
        // Three shared routes seek distinct deterministic lanes, without
        // pretending that their common endpoint stubs can be moved.
        const auto l=rectangles({{0,0,30,32},{210,0,30,32}});
        std::vector<RouteRequest> req(3,{{0,Side::East},{1,Side::West}});
        Routes paths; NudgingReport report;
        auto status=route_orthogonal_nudged(l,req,paths,{},&report);
        if(!status) std::cerr<<status.message<<'\n';
        require(static_cast<bool>(status),"three way edge separation");
        check_geometry(l,req,paths);
        require(report.final.shared_length<report.initial.shared_length,"three route overlap reduced");
        require(report.final.crossings<=report.initial.crossings,"no crossing regression");
    }
    {
        // Distinct, disjoint routes are not lengthened gratuitously.
        const auto l=rectangles({{0,0,30,30},{110,0,30,30},
                                 {0,120,30,30},{110,120,30,30}});
        const std::vector<RouteRequest> req{{{0,Side::East},{1,Side::West}},
                                            {{2,Side::East},{3,Side::West}}};
        Routes plain,optimized;
        require(static_cast<bool>(route_orthogonal(l,req,plain)),"disjoint initial routes");
        NudgingReport report;
        require(static_cast<bool>(route_orthogonal_nudged(l,req,optimized,{},&report)),"disjoint nudging");
        check_geometry(l,req,optimized);
        require(report.changed_routes==0 && report.final.shared_length==0,"disjoint routes unchanged");
        same_routes(plain,optimized);
    }
    {
        // Two perpendicular connector families have an unavoidable proper
        // crossing in their direct paths. The nudger cannot introduce extra
        // crossing points simply to reduce a different score.
        const auto l=rectangles({{0,30,30,30},{180,30,30,30},
                                 {90,-80,30,30},{90,140,30,30}});
        const std::vector<RouteRequest> req{
            {{0,Side::East},{1,Side::West}},
            {{2,Side::South},{3,Side::North}}};
        Routes result; NudgingReport report;
        auto status=route_orthogonal_nudged(l,req,result,{},&report);
        require(static_cast<bool>(status),"crossing geometry accepted");
        check_geometry(l,req,result);
        require(report.final.crossings<=report.initial.crossings,
                "crossing penalty never increases proper crossings");
        require(report.initial.crossings>=1,"baseline tracks genuine crossing");
    }
    {
        // Edge-input ordering does not affect the route chosen for distinct
        // endpoint tuples (same immutable graph node indices).
        const auto l=rectangles({{0,0,30,30},{180,0,30,30},{0,100,30,30}});
        const RouteRequest ab{{0,Side::East},{1,Side::West}};
        const RouteRequest cb{{2,Side::East},{1,Side::West}};
        Routes first,second;
        require(static_cast<bool>(route_orthogonal_nudged(l,{ab,cb},first)),
                "first edge ordering");
        require(static_cast<bool>(route_orthogonal_nudged(l,{cb,ab},second)),
                "permuted edge ordering");
        require(first.edges[0].points.size()==second.edges[1].points.size(),
                "stable first edge waypoint count");
        for(std::size_t k=0;k<first.edges[0].points.size();++k)
            require(first.edges[0].points[k].x==second.edges[1].points[k].x &&
                    first.edges[0].points[k].y==second.edges[1].points[k].y,
                    "stable route under distinct edge permutation");
    }
    {
        // A dense obstacle between the nodes exercises real nontrivial base
        // paths and the independent collision checker after every candidate.
        const auto l=rectangles({{0,60,35,35},{90,30,60,95},{220,60,35,35}});
        const std::vector<RouteRequest> req{{{0,Side::East},{2,Side::West}},
                                            {{0,Side::East},{2,Side::West}}};
        Routes result; NudgingReport report;
        auto status=route_orthogonal_nudged(l,req,result,{},&report);
        if(!status) std::cerr<<status.message<<'\n';
        require(static_cast<bool>(status),"joint detour around real obstacle");
        check_geometry(l,req,result);
        require(report.final.crossings<=report.initial.crossings,"detour crossings nonincreasing");
        if(report.final.crossings==report.initial.crossings)
            require(report.final.shared_length<=report.initial.shared_length+1e-9,
                    "detour overlap nonincreasing");
    }
    {
        const auto l=rectangles({{0,0,40,40},{140,0,40,40}});
        const std::vector<RouteRequest> req(2,{{0,Side::East},{1,Side::West}});
        Routes paths; paths.edges.resize(7); NudgingReport report;
        NudgingOptions o;
        o.max_candidates=1;
        auto s=route_orthogonal_nudged(l,req,paths,o,&report);
        require(s.error==RouteError::CapacityExceeded && paths.edges.empty(),
                "candidate exhaustion is transactional");
        require(report.candidate_trials==0,"failed report cleared");
        o.max_candidates=1000; o.max_segment_pair_checks=1;
        s=route_orthogonal_nudged(l,req,paths,o,&report);
        require(s.error==RouteError::CapacityExceeded && paths.edges.empty(),
                "cross-route operation capacity enforced");
        o.max_segment_pair_checks=300000; o.max_routes=1;
        s=route_orthogonal_nudged(l,req,paths,o);
        require(s.error==RouteError::CapacityExceeded && paths.edges.empty(),
                "joint route count enforced");
        o.max_routes=128; o.max_points_per_route=4;
        s=route_orthogonal_nudged(l,req,paths,o);
        require(s.error==RouteError::InvalidOptions,"minimum point capacity enforced");
        o.max_points_per_route=512; o.lane_spacing=0;
        s=route_orthogonal_nudged(l,req,paths,o);
        require(s.error==RouteError::InvalidOptions,"zero separation rejected");
    }
    {
        auto l=rectangles({{0,0,40,40},{140,0,40,40}});
        const std::vector<RouteRequest> req(2,{{0,Side::East},{1,Side::West}});
        Routes paths;
        require(static_cast<bool>(route_orthogonal(l,req,paths)),"audit base");
        paths.edges[0].points.insert(paths.edges[0].points.begin()+1,{50,10});
        RouteInteractions out{10,10};
        const auto status=audit_orthogonal_interactions(l,req,paths,out);
        require(status.error==RouteError::InternalInvariant && out.crossings==0 && out.shared_length==0,
                "audit rejects forged non-orthogonal route and clears metrics");
    }
    std::cout<<"joint orthogonal nudging: all checks passed\n";
    return EXIT_SUCCESS;
 } catch(const std::exception& e) {
    std::cerr<<"joint orthogonal nudging: "<<e.what()<<'\n';
    return EXIT_FAILURE;
 }
}
