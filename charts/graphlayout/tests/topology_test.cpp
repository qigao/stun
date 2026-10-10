#include "stun/graphlayout/topology.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace stun::graphlayout;
namespace {
void check(bool ok, const char* text){if(!ok)throw std::runtime_error(text);}
void same(double a,double b,const char* text){if(a!=b)throw std::runtime_error(text);}

struct Fixture { Graph g; Layout l; std::vector<RouteRequest> ports; Routes routes; };

// Four corners with crossing diagonals or parallel horizontal connectors.
Fixture quad(bool crossed){
    Fixture f;
    f.g.nodes={{"a",10,10},{"b",10,10},{"c",10,10},{"d",10,10}};
    f.l.nodes={{0,0,10,10},{100,0,10,10},{0,100,10,10},{100,100,10,10}};
    if(crossed){
        f.g.edges={{0,3},{2,1}};
        f.ports={{{0,Side::East,0.5},{3,Side::West,0.5}},
                 {{2,Side::East,0.5},{1,Side::West,0.5}}};
        f.routes.edges={{{{10,5},{100,105}}},{{{10,105},{100,5}}}};
    }else{
        f.g.edges={{0,1},{2,3}};
        f.ports={{{0,Side::East,0.5},{1,Side::West,0.5}},
                 {{2,Side::East,0.5},{3,Side::West,0.5}}};
        f.routes.edges={{{{10,5},{100,5}}},{{{10,105},{100,105}}}};
    }
    return f;
}
TopologySignature audited(const Fixture& f){
    TopologySignature s;
    auto st=audit_topology(f.g,f.l,f.ports,f.routes,s);
    if(!st)throw std::runtime_error("audit failed: "+st.message);
    return s;
}

void run(){
    {
        const auto f=quad(true);
        const auto sig=audited(f);
        check(sig.crossing_count==1,"crossing diagonals should have exactly one crossing");
        check(sig.ordered_crossings.size()==2,"edge-oriented signatures present");
        check(sig.ordered_crossings[0].size()==1 && sig.ordered_crossings[0][0].other_edge==1,
              "first route crossing partner recorded");
        check(sig.ordered_crossings[1][0].sign==-sig.ordered_crossings[0][0].sign,
              "signed crossing orientation is anti-symmetric");
        Layout desired=f.l;
        for(auto& n:desired.nodes){n.y+=25;}
        Layout moved;Routes paths;TopologyReport report;
        const auto st=move_topology_preserving(f.g,f.l,f.ports,f.routes,desired,moved,paths,{},&report);
        check(static_cast<bool>(st),"uniform translation preserves signed crossing");
        same(report.accepted_fraction,1,"full uniform translation accepted");
        check(report.baseline_crossings==1 && report.sampled_frames>=1,"crossing audit report");
        same(paths.edges[0].points.front().y,30,"anchor follows source node");
        same(moved.nodes[0].y,25,"node translated fully");
        TopologySignature inspected;
        const auto after=audit_topology(f.g,moved,f.ports,paths,inspected);
        check(static_cast<bool>(after),"auditor accepts moved crossing");
    }
    {
        auto f=quad(false);
        check(audited(f).crossing_count==0,"parallel edges begin with no crossings");
        Layout desired=f.l;
        // Invert the two horizontal edges. Endpoint topology looks identical,
        // yet halfway through the motion the paths contact each other.
        desired.nodes[0].y+=100;desired.nodes[1].y+=100;
        desired.nodes[2].y-=100;desired.nodes[3].y-=100;
        Layout moved;Routes routes;TopologyReport report;
        auto st=move_topology_preserving(f.g,f.l,f.ports,f.routes,desired,moved,routes,{},&report);
        check(static_cast<bool>(st),"a bounded safe fraction of a swapping move is found");
        check(report.accepted_fraction<0.5 && report.accepted_fraction>0.0,
              "intermediate topology collision blocks full edge swap");
        check(audited({f.g,moved,f.ports,routes}).crossing_count==0,"result preserves embedding");
    }
    {
        auto f=quad(false);
        Layout desired=f.l;
        desired.nodes[0].y=85;
        desired.nodes[1].y=120;
        Layout moved;Routes paths;TopologyReport report;
        const auto st=move_topology_preserving(f.g,f.l,f.ports,f.routes,desired,moved,paths,{},&report);
        check(static_cast<bool>(st),"inclined line movement retains planar crossing signature");
        check(report.accepted_fraction>0 && report.accepted_fraction<=1,"bounded step");
        check(audited({f.g,moved,f.ports,paths}).crossing_count==0,"no crossing introduced");
    }
    {
        // The identity AND ordered sequence of intersecting edges is retained,
        // not merely an aggregate count of two crossings.
        Fixture f;
        f.g.nodes={{"west",10,10},{"east",10,10},
                   {"north1",10,10},{"south1",10,10},
                   {"north2",10,10},{"south2",10,10}};
        f.g.edges={{0,1},{2,3},{4,5}};
        f.l.nodes={{0,40,10,10},{200,40,10,10},
                   {55,0,10,10},{55,100,10,10},
                   {155,0,10,10},{155,100,10,10}};
        f.ports={{{0,Side::East,0.5},{1,Side::West,0.5}},
                 {{2,Side::South,0.5},{3,Side::North,0.5}},
                 {{4,Side::South,0.5},{5,Side::North,0.5}}};
        f.routes.edges={{{{10,45},{200,45}}},
                        {{{60,10},{60,100}}},
                        {{{160,10},{160,100}}}};
        const auto before=audited(f);
        check(before.crossing_count==2,"baseline two ordered crossings");
        check(before.ordered_crossings[0][0].other_edge==1 &&
              before.ordered_crossings[0][1].other_edge==2,
              "crossing order retained in signature");
        Layout target=f.l;
        target.nodes[2].x+=100;target.nodes[3].x+=100;
        target.nodes[4].x-=100;target.nodes[5].x-=100;
        Layout accepted;Routes warped;TopologyReport report;
        const auto st=move_topology_preserving(f.g,f.l,f.ports,f.routes,target,
                                               accepted,warped,{},&report);
        check(static_cast<bool>(st),"safe prefix of swapped vertical connectors found");
        check(report.accepted_fraction<0.5 && report.accepted_fraction>0,
              "crossing-order exchange requires an impossible intermediate touch");
        const auto after=audited({f.g,accepted,f.ports,warped});
        check(after.ordered_crossings[0][0].other_edge==1 &&
              after.ordered_crossings[0][1].other_edge==2,
              "new signature preserves crossing identities and ordering");
    }
    {
        // Deterministic topology stress on cyclically skewed four-corner
        // movements, including cases where only a fractional step is allowed.
        for(std::size_t i=0;i<48;++i){
            auto f=quad(i%2==0);
            Layout target=f.l;
            for(std::size_t n=0;n<target.nodes.size();++n){
                const int dx=static_cast<int>((i*13+n*19)%35)-17;
                const int dy=static_cast<int>((i*11+n*23)%37)-18;
                target.nodes[n].x+=dx;
                target.nodes[n].y+=dy;
            }
            Layout accepted;Routes warped;TopologyReport report;
            const auto st=move_topology_preserving(f.g,f.l,f.ports,f.routes,target,
                                                   accepted,warped,{},&report);
            if(!st)throw std::runtime_error("deterministic topology family: "+st.message);
            check(report.accepted_fraction>0 && report.accepted_fraction<=1,
                  "family must return a positive bounded movement fraction");
            const auto after=audited({f.g,accepted,f.ports,warped});
            const auto before=audited(f);
            check(after.crossing_count==before.crossing_count,
                  "family preserves signed crossing count");
            check(after.ordered_crossings.size()==before.ordered_crossings.size(),
                  "family edge identity count is fixed");
            for(std::size_t e=0;e<after.ordered_crossings.size();++e){
                check(after.ordered_crossings[e].size()==before.ordered_crossings[e].size(),
                      "family preserves crossing identity sequence length");
                for(std::size_t c=0;c<after.ordered_crossings[e].size();++c)
                    check(after.ordered_crossings[e][c].other_edge==before.ordered_crossings[e][c].other_edge &&
                          after.ordered_crossings[e][c].sign==before.ordered_crossings[e][c].sign,
                          "family preserves complete signed crossing sequence");
            }
        }
    }
    {
        auto f=quad(false);
        f.routes.edges[0].points={{10,5},{40,5},{40,20},{20,20},{20,5},{100,5}};
        TopologySignature signature;
        const auto st=audit_topology(f.g,f.l,f.ports,f.routes,signature);
        check(st.error==TopologyError::AmbiguousTopology,
              "a connector retracing a prior segment is topologically ambiguous");
    }
    {
        auto f=quad(false);
        f.routes.edges[0].points={{10,5},{100,5}};
        f.ports[0].source.side=Side::West;
        TopologySignature signature;
        auto st=audit_topology(f.g,f.l,f.ports,f.routes,signature);
        check(st.error==TopologyError::InvalidGeometry,
              "port compass and route anchor must agree exactly");
        f.ports[0].source.side=Side::East;
        f.routes.edges[0].points={{10,5},{-10,5},{100,5}};
        st=audit_topology(f.g,f.l,f.ports,f.routes,signature);
        check(st.error==TopologyError::InvalidGeometry,
              "a route may not depart inward from its stated compass side");
    }
    {
        auto f=quad(false);
        f.routes.edges[1].points={f.routes.edges[0].points[0],f.routes.edges[0].points[1]};
        TopologySignature s;
        const auto st=audit_topology(f.g,f.l,f.ports,f.routes,s);
        check(st.error==TopologyError::InvalidGeometry,"wrong route anchor rejected");
        check(s.ordered_crossings.empty(),"failed audit clears signature");
    }
    {
        auto f=quad(false);
        f.routes.edges[0].points.insert(f.routes.edges[0].points.begin()+1,{55,5});
        TopologyOptions small;small.max_segment_checks=1;
        TopologySignature sig;
        const auto st=audit_topology(f.g,f.l,f.ports,f.routes,sig,small);
        check(st.error==TopologyError::CapacityExceeded,"budgeted geometry auditer");
    }
    {
        auto f=quad(false);
        auto bad=f;
        bad.ports[0].source.side=Side::Auto;
        TopologySignature sig;
        auto st=audit_topology(bad.g,bad.l,bad.ports,bad.routes,sig);
        check(st.error==TopologyError::InvalidInput,"topology requires explicit ports");
        bad=f;
        bad.g.edges[0].target=bad.g.edges[0].source;
        st=audit_topology(bad.g,bad.l,bad.ports,bad.routes,sig);
        check(st.error==TopologyError::InvalidInput,"self-loops are explicitly unsupported");
    }
    {
        auto f=quad(false);
        Layout desired=f.l;
        desired.nodes[2].y=0;
        desired.nodes[3].y=0;
        TopologyOptions opts;opts.max_trials=1;
        Layout moved;Routes routes;TopologyReport report;
        moved.nodes.resize(2);routes.edges.resize(1);report.accepted_fraction=99;
        const auto st=move_topology_preserving(f.g,f.l,f.ports,f.routes,desired,moved,routes,opts,&report);
        check(st.error==TopologyError::NoAdmissibleStep,"bounded topology failure explicit");
        check(moved.nodes.empty()&&routes.edges.empty()&&report.accepted_fraction==0,
              "failed move leaves transactional empty outputs");
    }
    {
        auto f=quad(false);
        Layout desired=f.l;
        TopologyOptions opts;
        opts.max_segment_checks=5;
        Layout moved;Routes paths;TopologyReport report;
        const auto st=move_topology_preserving(f.g,f.l,f.ports,f.routes,desired,moved,paths,opts,&report);
        check(st.error==TopologyError::CapacityExceeded,"move uses global analysis budget");
    }
}
}
int main(){try{run();std::cout<<"topology: all checks passed\n";return EXIT_SUCCESS;}
catch(const std::exception& e){std::cerr<<"topology: "<<e.what()<<'\n';return EXIT_FAILURE;}}
