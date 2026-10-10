#include "stun/graphlayout/polyline_nudging.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace stun::graphlayout;

namespace {
void check(bool value,const char* msg){if(!value)throw std::runtime_error(msg);}
Layout boxes(std::initializer_list<PlacedNode> nodes){Layout l;l.nodes=nodes;return l;}
bool same(const Routes& a,const Routes& b){
    if(a.edges.size()!=b.edges.size())return false;
    for(std::size_t i=0;i<a.edges.size();++i){
        const auto& x=a.edges[i].points;const auto& y=b.edges[i].points;
        if(x.size()!=y.size())return false;
        for(std::size_t j=0;j<x.size();++j)
            if(x[j].x!=y[j].x||x[j].y!=y[j].y)return false;
    }
    return true;
}
void validate(const Layout& l,const std::vector<RouteRequest>& req,const Routes& r,
              const PolylineNudgingOptions& options={}) {
    auto s=validate_polyline_routes(l,req,r,options.routing);
    if(!s)throw std::runtime_error("polyline nudging validity: "+s.message);
}
}

int main(){
 try{
    {
        const Layout l=boxes({{0,0,40,40},{200,50,40,40}});
        const RouteRequest r{{0,Side::East},{1,Side::West}};
        const std::vector<RouteRequest> req{r,r};
        Routes plain,opt,repeat;
        auto s=route_polyline(l,req,plain);
        check(static_cast<bool>(s),"unoptimized polyline exists");
        PolylineNudgingReport report;
        s=route_polyline_nudged(l,req,opt,{},&report);
        if(!s)throw std::runtime_error("nudging duplicated polylines: "+s.message);
        validate(l,req,opt);
        check(report.initial.proper_crossings==0 && report.final.proper_crossings==0,
              "parallel edges do not gain crossings");
        check(report.final.shared_length<report.initial.shared_length,
              "joint polyline removes some shared segment length");
        check(report.changed_routes>=1 && report.accepted_moves>=1,
              "optimizer actually changes a route");
        check(report.final.proper_crossings<=report.initial.proper_crossings,
              "proper crossings never increase");
        check(static_cast<bool>(route_polyline_nudged(l,req,repeat)),"deterministic rerun");
        check(same(opt,repeat),"same input yields exact same output geometry");
        PolylineInteractions independent;
        check(static_cast<bool>(audit_polyline_interactions(l,req,opt,independent)),
              "independent final audit");
        check(independent.shared_length==report.final.shared_length &&
              independent.proper_crossings==report.final.proper_crossings,
              "independent audit matches final report");
    }
    {
        const Layout l=boxes({{0,10,35,35},{160,10,35,35},
                              {0,120,35,35},{160,120,35,35}});
        const std::vector<RouteRequest> req{
            {{0,Side::East},{1,Side::West}},{{2,Side::East},{3,Side::West}}};
        Routes plain,optimized;
        check(static_cast<bool>(route_polyline(l,req,plain)),"separate baseline routes");
        PolylineNudgingReport report;
        check(static_cast<bool>(route_polyline_nudged(l,req,optimized,{},&report)),
              "separate routing returns valid result");
        validate(l,req,optimized);
        check(same(plain,optimized),"already disjoint routes remain shortest");
        check(report.changed_routes==0 && report.final.shared_length==0,
              "no gratuitous geometric changes");
    }
    {
        // Nearby obstacles force real corner-visibility detours. All emitted
        // geometry remains checkable regardless of whether a lane fits.
        const Layout l=boxes({{0,60,30,30},{85,25,60,95},{220,60,30,30}});
        const std::vector<RouteRequest> req(2,{{0,Side::East},{2,Side::West}});
        Routes optimized;
        PolylineNudgingReport report;
        auto s=route_polyline_nudged(l,req,optimized,{},&report);
        if(!s)throw std::runtime_error("obstacle lane: "+s.message);
        validate(l,req,optimized);
        check(report.final.proper_crossings<=report.initial.proper_crossings,
              "complex geometry proper crossings nonincreasing");
        if(report.final.proper_crossings==report.initial.proper_crossings)
            check(report.final.shared_length<=report.initial.shared_length,
                  "complex geometry overlap nonincreasing");
    }
    {
        const Layout l=boxes({{0,40,30,30},{200,40,30,30},
                              {100,-65,30,30},{100,155,30,30}});
        const std::vector<RouteRequest> req{
            {{0,Side::East},{1,Side::West}},{{2,Side::South},{3,Side::North}}};
        Routes baseline,result;
        check(static_cast<bool>(route_polyline(l,req,baseline)),"crossing baseline");
        PolylineInteractions first;
        check(static_cast<bool>(audit_polyline_interactions(l,req,baseline,first)),
              "crossing audit exists");
        check(first.proper_crossings>0,"crossing audit counts proper crossings");
        auto s=route_polyline_nudged(l,req,result);
        if(!s)throw std::runtime_error(s.message);
        PolylineInteractions final;
        check(static_cast<bool>(audit_polyline_interactions(l,req,result,final)),
              "crossing final audit");
        check(final.proper_crossings<=first.proper_crossings,
              "lane search cannot increase proper crossing count");
    }
    {
        const Layout l=boxes({{0,0,40,40},{200,50,40,40}});
        const std::vector<RouteRequest> req(2,{{0,Side::East},{1,Side::West}});
        Routes result;result.edges.resize(4);
        PolylineNudgingReport report;
        PolylineNudgingOptions options;
        options.max_candidates=1;
        auto s=route_polyline_nudged(l,req,result,options,&report);
        check(s.error==RouteError::CapacityExceeded && result.edges.empty(),
              "candidate exhaustion must be transactional");
        check(report.accepted_moves==0,"failed report not published");
        options.max_candidates=6000; options.max_segment_pair_checks=1;
        s=route_polyline_nudged(l,req,result,options);
        check(s.error==RouteError::CapacityExceeded && result.edges.empty(),
              "interaction checks bounded");
        options.max_segment_pair_checks=2000000; options.max_routes=1;
        s=route_polyline_nudged(l,req,result,options);
        check(s.error==RouteError::CapacityExceeded,"route count bounded");
        options.max_routes=128; options.lane_spacing=0;
        s=route_polyline_nudged(l,req,result,options);
        check(s.error==RouteError::InvalidOptions,"zero lane spacing invalid");
        options.lane_spacing=12; options.max_points_per_route=3;
        s=route_polyline_nudged(l,req,result,options);
        check(s.error==RouteError::InvalidOptions,"invalid waypoint options rejected");
    }
    std::cout << "polyline-nudging: all checks passed\n";
    return EXIT_SUCCESS;
 }catch(const std::exception& e){std::cerr << "polyline-nudging: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
