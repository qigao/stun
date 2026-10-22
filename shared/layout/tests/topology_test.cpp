#include "stun/graphlayout/topology.h"

#include <algorithm>
#include <array>
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

Fixture cancellation_routes(double n, bool crossing){
    constexpr double size=0.125;
    Fixture f;
    f.g.nodes={{"a",size,size},{"b",size,size},{"c",size,size},{"d",size,size}};
    f.g.edges={{0,1},{2,3}};
    if(crossing) {
        f.l.nodes={{-n/2-size,-(n-1)/2,size,size},{n/2,(n-1)/2-size,size,size},
                   {-(n+1)/2-size,-n/2,size,size},{(n+1)/2-size,n/2,size,size}};
        f.ports={{{0,Side::East,0},{1,Side::West,1}},
                 {{2,Side::East,0},{3,Side::North,1}}};
        f.routes.edges={{{{-n/2,-(n-1)/2},{n/2,(n-1)/2}}},
                        {{{-(n+1)/2,-n/2},{(n+1)/2,n/2}}}};
    } else {
        f.l.nodes={{-size,0,size,size},{2*n-size,2*n-2,size,size},
                   {n+1-size,n,size,size},{n+2-size,n+1,size,size}};
        f.ports={{{0,Side::East,0},{1,Side::North,1}},
                 {{2,Side::East,0},{3,Side::North,1}}};
        f.routes.edges={{{{0,0},{2*n,2*n-2}}},{{{n+1,n},{n+2,n+1}}}};
    }
    return f;
}

void transform_cancellation(Fixture& f, double scale, std::size_t variant){
    const auto point=[&](Point& p) {
        p.x*=scale;p.y*=scale;
        if(variant&1)std::swap(p.x,p.y);
        if(variant&2)p.x=-p.x;
    };
    for(auto& n:f.g.nodes) {n.width*=scale;n.height*=scale;}
    for(auto& n:f.l.nodes) {
        Point p{n.x,n.y};point(p);n.width*=scale;n.height*=scale;
        n.x=p.x-((variant&2) ? n.width : 0);n.y=p.y;
    }
    for(auto& route:f.routes.edges)for(auto& p:route.points)point(p);
    for(auto& request:f.ports)for(auto* port:{&request.source,&request.target}) {
        if(variant&1) {
            switch(port->side) {
            case Side::East:port->side=Side::South;break;
            case Side::West:port->side=Side::North;break;
            case Side::North:port->side=Side::West;break;
            default:throw std::runtime_error("unexpected cancellation fixture side");
            }
        }
        if(variant&2) {
            if(port->side==Side::East)port->side=Side::West;
            else if(port->side==Side::West)port->side=Side::East;
            else port->offset=1-port->offset;
        }
    }
    for(std::size_t edge=0;edge<f.g.edges.size();++edge)if(variant&(4u<<edge)) {
        std::swap(f.g.edges[edge].source,f.g.edges[edge].target);
        std::swap(f.ports[edge].source,f.ports[edge].target);
        std::reverse(f.routes.edges[edge].points.begin(),f.routes.edges[edge].points.end());
    }
}

void run(){
    {
        constexpr std::array magnitudes={0x1p27,0x1p38},scales={0.125,1.0};
        constexpr std::size_t variants=16;
        for(const double n:magnitudes)for(const double scale:scales)
        for(std::size_t variant=0;variant<variants;++variant)for(const bool crossing:{false,true}) {
            // Disjoint: AB=(2n,2n-2), C=(n+1,n), D=(n+2,n+1),
            // so endpoint orientations are 2 and 4. Crossing: AB=(n,n-1),
            // CD=(n+1,n) have determinant 1 and intersect at both midpoints.
            auto f=cancellation_routes(n,crossing);transform_cancellation(f,scale,variant);
            TopologyOptions options;options.coordinate_limit=1e12;options.orientation_epsilon=0;
            TopologySignature signature;
            const auto status=audit_topology(f.g,f.l,f.ports,f.routes,signature,options);
            if(!status)throw std::runtime_error("cancellation-scale routes: "+status.message);
            check(signature.crossing_count==(crossing ? 1u : 0u),
                  "nonzero determinants distinguish disjoint routes from proper crossings");
            if(crossing) {
                const int sign=((variant&1)!=0)^((variant&2)!=0)^((variant&4)!=0)^((variant&8)!=0) ? -1 : 1;
                check(signature.ordered_crossings[0].size()==1 && signature.ordered_crossings[1].size()==1 &&
                      signature.ordered_crossings[0][0].other_edge==1 && signature.ordered_crossings[0][0].sign==sign &&
                      signature.ordered_crossings[1][0].other_edge==0 && signature.ordered_crossings[1][0].sign==-sign,
                      "cancellation-scale crossings retain partner identity and transformed signs");
            }
            for(const bool translate:{false,true}) {
                constexpr Point displacement{4,-8};
                auto desired=f.l;
                if(translate)for(auto& node:desired.nodes) {
                    node.x+=displacement.x*scale;node.y+=displacement.y*scale;
                }
                Layout moved;Routes paths;TopologyReport report;
                const auto movement=move_topology_preserving(f.g,f.l,f.ports,f.routes,desired,
                                                             moved,paths,options,&report);
                if(!movement)throw std::runtime_error("rigid cancellation-scale movement: "+movement.message);
                check(report.accepted_fraction==1 && report.baseline_crossings==signature.crossing_count &&
                      moved.nodes.size()==f.l.nodes.size() && paths.edges.size()==f.routes.edges.size(),
                      "audited cancellation-scale baselines enter stationary and rigid continuation");
                check(report.segment_pair_constraints==4,
                      "all four nonzero endpoint orientations retain their movement guards after cancellation");
                TopologySignature after;
                check(static_cast<bool>(audit_topology(f.g,moved,f.ports,paths,after,options)) &&
                      after.crossing_count==signature.crossing_count,
                      "independent audit retains cancellation-scale crossing count after guarded movement");
                if(crossing)check(after.ordered_crossings[0][0].sign==signature.ordered_crossings[0][0].sign &&
                                  after.ordered_crossings[1][0].sign==signature.ordered_crossings[1][0].sign,
                                  "guarded rigid continuation preserves both oriented crossing signs");
            }
            options.orientation_epsilon=TopologyOptions{}.orientation_epsilon;
            check(audit_topology(f.g,f.l,f.ports,f.routes,signature,options).error==TopologyError::AmbiguousTopology &&
                  signature.crossing_count==0 && signature.ordered_crossings.empty(),
                  "compensation preserves the configured geometric ambiguity tolerance");
            if(!crossing) {
                auto touch=cancellation_routes(n,false);
                // C is now the exact midpoint of AB: this contact must remain
                // ambiguous even with no user-specified orientation tolerance.
                touch.routes.edges[1].points.front()={n,n-1};
                touch.l.nodes[2].x=n-touch.l.nodes[2].width;touch.l.nodes[2].y=n-1;
                transform_cancellation(touch,scale,variant);options.orientation_epsilon=0;
                check(audit_topology(touch.g,touch.l,touch.ports,touch.routes,signature,options).error==
                      TopologyError::AmbiguousTopology && signature.ordered_crossings.empty(),
                      "exact finite-segment contacts remain ambiguous after compensation");
            }
        }
    }
    {
        Fixture f;
        f.g.nodes={{"a",10,10},{"b",10,10}};
        f.g.edges={{0,1}};
        f.l.nodes={{0,0,10,10},{10,10,10,10}};
        f.ports={{{0,Side::East,1},{1,Side::North,0}}};
        f.routes.edges={{{{10,10},{20,0},{10,10}}}};
        TopologySignature signature;signature.crossing_count=7;
        const auto st=audit_topology(f.g,f.l,f.ports,f.routes,signature);
        check(st.error==TopologyError::AmbiguousTopology,
              "adjacent backtracking at coincident corner ports is rejected");
        check(signature.ordered_crossings.empty() && signature.crossing_count==0,
              "backtracking failure clears the topology signature");
    }
    {
        Graph graph;graph.nodes={{"moving",10,10},{"fixed",10,10}};
        Layout baseline;baseline.nodes={{0,0,10,10},{100,0,10,10}};
        Layout desired=baseline;desired.nodes[0].x=300;
        Layout moved;Routes paths;TopologyReport report;
        const auto st=move_topology_preserving(graph,baseline,{}, {},desired,moved,paths,{},&report);
        check(static_cast<bool>(st),"a safe prefix of isolated-node movement is available");
        check(report.accepted_fraction>0 && report.accepted_fraction<0.3,
              "analytic guard prevents node tunneling between sampled frames");
        check(moved.nodes[0].x+10<100,"moving rectangle stays before the fixed rectangle");
    }
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
        const auto f=quad(false);
        TopologyOptions limited;limited.max_nodes=f.g.nodes.size()-1;
        TopologySignature signature;
        auto st=audit_topology(f.g,f.l,f.ports,f.routes,signature,limited);
        check(st.error==TopologyError::CapacityExceeded,"audit graph node limit has a capacity error");
        limited={};limited.max_edges=f.g.edges.size()-1;
        st=audit_topology(f.g,f.l,f.ports,f.routes,signature,limited);
        check(st.error==TopologyError::CapacityExceeded,"audit graph edge limit has a capacity error");
    }
    {
        auto f=quad(false);
        Layout desired=f.l;
        desired.nodes[2].y=0;
        desired.nodes[3].y=0;
        TopologyOptions opts;opts.max_trials=1;opts.analytic_guards=false;
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
