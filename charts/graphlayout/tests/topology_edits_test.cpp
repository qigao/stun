#include "stun/graphlayout/topology_edits.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace stun::graphlayout;

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
struct Fixture {
    Graph graph;
    Layout layout;
    std::vector<RouteRequest> ports;
    Routes paths;
};
Fixture horizontal() {
    Fixture f;
    f.graph.nodes = {{"A",10,10},{"B",10,10},{"obstacle",10,10}};
    f.graph.edges = {{0,1}};
    f.layout.nodes = {{0,0,10,10},{100,0,10,10},{45,30,10,10}};
    f.ports = {{{0,Side::East,0.5},{1,Side::West,0.5}}};
    f.paths.edges = {{{{10,5},{100,5}}}};
    return f;
}
Fixture parallel() {
    Fixture f;
    f.graph.nodes = {{"A",10,10},{"B",10,10},{"C",10,10},{"D",10,10}};
    f.graph.edges = {{0,1},{2,3}};
    f.layout.nodes = {{0,0,10,10},{100,0,10,10},{0,100,10,10},{100,100,10,10}};
    f.ports = {{{0,Side::East,0.5},{1,Side::West,0.5}},
               {{2,Side::East,0.5},{3,Side::West,0.5}}};
    f.paths.edges = {{{{10,5},{100,5}}},{{{10,105},{100,105}}}};
    return f;
}
TopologyEdit split(double dx, double dy) {
    TopologyEdit edit;
    edit.kind = TopologyEditKind::StraightToBend;
    edit.edge_index = 0;
    edit.point_index = 0;
    edit.segment_fraction = 0.5;
    edit.displacement = {dx,dy};
    return edit;
}
void check_audited(const Fixture& f, const Routes& r) {
    TopologySignature a,b;
    const auto start=audit_topology(f.graph,f.layout,f.ports,f.paths,a);
    const auto end=audit_topology(f.graph,f.layout,f.ports,r,b);
    require(static_cast<bool>(start) && static_cast<bool>(end),"both route geometries audited");
    require(a.crossing_count==b.crossing_count,"crossing count preserved");
    require(a.ordered_crossings.size()==b.ordered_crossings.size(),"edge count preserved");
    for(std::size_t i=0;i<a.ordered_crossings.size();++i){
        require(a.ordered_crossings[i].size()==b.ordered_crossings[i].size(),"crossings per edge preserved");
        for(std::size_t j=0;j<a.ordered_crossings[i].size();++j){
            require(a.ordered_crossings[i][j].other_edge==b.ordered_crossings[i][j].other_edge,
                    "crossing partner preserved");
            require(a.ordered_crossings[i][j].sign==b.ordered_crossings[i][j].sign,
                    "crossing sign preserved");
        }
    }
}
}

int main(){
    try{
        {
            auto f=horizontal();
            Routes output;TopologyEditReport report;
            const auto st=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{split(0,-24)},output,{},&report);
            if(!st) throw std::runtime_error("valid split failed: "+st.message);
            require(output.edges.size()==1 && output.edges[0].points.size()==3,
                    "explicit segment converted into two segments");
            require(output.edges[0].points[0].x==10 && output.edges[0].points.back().x==100,
                    "exact ports preserved");
            require(output.edges[0].points[1].x==55 && output.edges[0].points[1].y==-19,
                    "bend inserted at requested geometric position");
            require(report.inserted_bends==1 && report.removed_bends==0 &&
                    report.audits==9 && report.continuation_frames==8,
                    "every intermediate bend geometry audited");
            check_audited(f,output);
        }
        {
            auto f=horizontal();
            f.paths.edges[0].points={{10,5},{55,5},{100,5}};
            Routes out;TopologyEditReport report;
            TopologyEdit merge;
            merge.kind=TopologyEditKind::BendToStraight;
            merge.point_index=1;
            auto st=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,{},&report);
            require(static_cast<bool>(st),"redundant straight connector point can be pruned");
            require(out.edges[0].points.size()==2 && out.edges[0].points[0].x==10 &&
                    out.edges[0].points[1].x==100,"merge preserves exact route geometry");
            require(report.removed_bends==1 && report.audits==2 &&
                    report.continuation_frames==0,"merge audited transactionally");
            check_audited(f,out);
        }
        {
            auto f=horizontal();
            Routes out;out.edges.resize(2);
            TopologyEditReport report;report.inserted_bends=8;
            const auto st=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{split(0,0)},out,{},&report);
            require(st.error==TopologyError::InvalidGeometry,"zero bend is not a StraightToBend event");
            require(out.edges.empty() && report.inserted_bends==0,"failure clears all outputs");
        }
        {
            auto f=horizontal();
            Routes out;
            // Moving the bend into obstacle C is rejected by geometry validator.
            const auto st=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{split(0,32)},out);
            require(st.error==TopologyError::NoAdmissibleStep,"node-through-route split forbidden");
            require(out.edges.empty(),"forbidden split never exposes partial route");
        }
        {
            auto f=parallel();
            Routes out;
            // This detour would introduce two new proper crossings with edge 1.
            const auto st=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{split(0,130)},out);
            require(st.error==TopologyError::NoAdmissibleStep,"new crossings must fail");
            require(out.edges.empty(),"new crossing rollback");
        }
        {
            auto f=horizontal();
            f.paths.edges[0].points={{10,5},{55,-15},{100,5}};
            TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
            Routes out;
            const auto st=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out);
            require(st.error==TopologyError::NoAdmissibleStep,"nonstraight bend may not be silently pruned");
        }
        {
            auto f=horizontal();
            Routes out;TopologyEditReport report;
            TopologyEditOptions limited;limited.max_edits=1;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                   {split(0,-10),split(0,-11)},out,limited,&report).error==
                   TopologyError::CapacityExceeded,"batch edit count bounded");
            require(out.edges.empty() && report.audits==0,"preflight budget does not publish candidate");
            limited={};limited.max_audits=2;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                   {split(0,-10)},out,limited).error==TopologyError::CapacityExceeded,
                   "cumulative audit budget enforced");
            limited={};limited.audit.max_points_per_route=2;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                   {split(0,-10)},out,limited).error==TopologyError::CapacityExceeded,
                   "single-edge waypoint budget enforced");
            limited={};limited.continuation_frames=0;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                   {split(0,-10)},out,limited).error==TopologyError::InvalidOptions,
                   "invalid continuation-frame configuration rejected");
        }
        {
            auto f=horizontal();
            Routes out;
            auto e=split(0,-7);e.segment_fraction=1.0;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{e},out).error==
                    TopologyError::InvalidInput,"endpoint fractions cannot add duplicate vertices");
            e=split(0,-7);e.point_index=99;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{e},out).error==
                    TopologyError::InvalidInput,"invalid route segment index rejected");
            TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=0;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out).error==
                    TopologyError::InvalidInput,"terminal anchors cannot be removed");
            require(out.edges.empty(),"invalid commands leave empty output");
        }
        {
            // 50 deterministic trials varying the explicit bend magnitude.
            // The exact same request always produces exactly the same points.
            auto f=horizontal();
            for(std::size_t k=0;k<50;++k){
                const double dx=static_cast<double>(k%9)-4;
                const double dy=-static_cast<double>(1+k%21);
                Routes a,b;
                const auto edit=split(dx,dy);
                auto one=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{edit},a);
                auto two=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{edit},b);
                require(static_cast<bool>(one) && static_cast<bool>(two),"deterministic safe edits succeed");
                require(a.edges[0].points.size()==3 &&
                        a.edges[0].points[1].x==b.edges[0].points[1].x &&
                        a.edges[0].points[1].y==b.edges[0].points[1].y,
                        "edits are deterministic");
                check_audited(f,a);
            }
        }
        std::cout<<"topology event surgery: all checks passed\n";
        return EXIT_SUCCESS;
    }catch(const std::exception& ex){
        std::cerr<<"topology event surgery: "<<ex.what()<<'\n';
        return EXIT_FAILURE;
    }
}
