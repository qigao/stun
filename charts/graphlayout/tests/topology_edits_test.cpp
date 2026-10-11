#include "stun/graphlayout/topology_edits.h"

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
Fixture hidden_split(bool segment_only=false) {
    auto f=horizontal();
    f.graph.nodes[2].width=3;f.graph.nodes[2].height=8;
    f.layout.nodes[2]={segment_only?65.0:55.0,-30,3,8};
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
            const auto f=hidden_split();
            // The new point starts at (55,5), reaches (95,-995), and lies
            // inside the obstacle at alpha=1/32. All eight sampled frames
            // occur after the local collision has already ended.
            auto intermediate=f.paths;
            intermediate.edges[0].points={{10,5},{56.25,-26.25},{100,5}};
            TopologySignature signature;
            require(audit_topology(f.graph,f.layout,f.ports,intermediate,signature).error==
                    TopologyError::InvalidGeometry,"independent frame exposes a hidden split-node collision");
            Routes out;TopologyEditReport report;
            const auto st=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{split(40,-1000)},out,{},&report);
            require(st.error==TopologyError::NoAdmissibleStep && out.edges.empty() && report.audits==0,
                    "continuous split-node guard rejects a collision before the first sampled frame");
            TopologyEditOptions sampled;sampled.audit.analytic_guards=false;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(40,-1000)},out,sampled,&report)) && report.continuation_frames==8,
                    "explicit sampled-only edits reproduce the missed intermediate collision");
            check_audited(f,out);
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(0,-27)},out,{},&report)) && out.edges[0].points[1].y==-22,
                    "continuous split guards permit final node-boundary tangency");
            check_audited(f,out);
            for(const double dx:{-50.0,50.0}) {
                const auto incident=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(dx,-2)},out,{},&report);
                require(incident.error==TopologyError::NoAdmissibleStep &&
                        incident.message=="topology surgery would sweep through a node rectangle" &&
                        out.edges.empty() && report.audits==0,
                        "continuous split guards include both incident source and target rectangles");
            }
        }
        {
            constexpr std::array scales={0.125,1.0,8.0,1024.0};
            constexpr std::size_t variants=8;
            for(const bool segment_only:{false,true})for(const double scale:scales)
                for(std::size_t variant=0;variant<variants;++variant) {
                    auto f=hidden_split(segment_only);auto edit=split(40,-1000);
                    for(auto& n:f.graph.nodes) {
                        n.width*=scale;n.height*=scale;if(variant&1)std::swap(n.width,n.height);
                    }
                    for(auto& n:f.layout.nodes) {
                        n.x*=scale;n.y*=scale;n.width*=scale;n.height*=scale;
                        if(variant&1) {std::swap(n.x,n.y);std::swap(n.width,n.height);}
                        if(variant&2)n.x=-n.x-n.width;
                    }
                    for(auto& p:f.paths.edges[0].points) {
                        p.x*=scale;p.y*=scale;if(variant&1)std::swap(p.x,p.y);
                        if(variant&2)p.x=-p.x;
                    }
                    edit.displacement.x*=scale;edit.displacement.y*=scale;
                    if(variant&1)std::swap(edit.displacement.x,edit.displacement.y);
                    if(variant&2)edit.displacement.x=-edit.displacement.x;
                    for(auto* port:{&f.ports[0].source,&f.ports[0].target}) {
                        if(variant&1)port->side=port->side==Side::East?Side::South:Side::North;
                        if(variant&2) {
                            if(port->side==Side::East)port->side=Side::West;
                            else if(port->side==Side::West)port->side=Side::East;
                        }
                    }
                    if(variant&4) {
                        std::swap(f.graph.edges[0].source,f.graph.edges[0].target);
                        std::swap(f.ports[0].source,f.ports[0].target);
                        std::reverse(f.paths.edges[0].points.begin(),f.paths.edges[0].points.end());
                    }
                    TopologySignature signature;
                    require(static_cast<bool>(audit_topology(f.graph,f.layout,f.ports,f.paths,signature)),
                            "transformed hidden edit has a valid baseline");
                    const auto a=f.paths.edges[0].points.front(), b=f.paths.edges[0].points.back();
                    const Point seed{(a.x+b.x)/2,(a.y+b.y)/2};
                    const double fraction=segment_only?5.0/128:1.0/32;
                    const Point point{seed.x+fraction*edit.displacement.x,seed.y+fraction*edit.displacement.y};
                    auto intermediate=f.paths;intermediate.edges[0].points={a,point,b};
                    if(segment_only)for(const auto& p:intermediate.edges[0].points) {
                        const auto& box=f.layout.nodes[2];
                        require(!(p.x>box.x && p.x<box.x+box.width && p.y>box.y && p.y<box.y+box.height),
                                "all waypoints remain outside during the segment-interior collision");
                    }
                    require(audit_topology(f.graph,f.layout,f.ports,intermediate,signature).error==
                            TopologyError::InvalidGeometry,"transformed independent frame exposes the edit collision");
                    Routes out;out.edges.resize(1);TopologyEditReport report;report.inserted_bends=1;
                    require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{edit},out,{},&report).error==
                            TopologyError::NoAdmissibleStep && out.edges.empty() && report.audits==0,
                            "continuous edit guards catch waypoint and segment sweeps across transformed fixtures");
                }
        }
        {
            const auto f=horizontal();Routes out;TopologyEditReport report;
            TopologyEditOptions limited;
            // Two affected segments against three stationary nodes; all six
            // pairs retain a separating axis and consume one evaluation each.
            constexpr std::size_t one_edit_work=6, batch_work=2*one_edit_work;
            limited.audit.max_guard_constraints=one_edit_work;
            limited.audit.max_guard_evaluations=one_edit_work;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(0,-10)},out,limited,&report)),"one safe edit fits exact continuous-node budgets");
            limited.audit.max_guard_constraints=one_edit_work-1;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{split(0,-10)},out,limited,&report).error==
                    TopologyError::CapacityExceeded && out.edges.empty() && report.audits==0,
                    "continuous edit constraint budget clears all outputs");
            limited.audit.max_guard_constraints=one_edit_work;limited.audit.max_guard_evaluations=one_edit_work-1;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{split(0,-10)},out,limited,&report).error==
                    TopologyError::CapacityExceeded && report.audits==0,
                    "continuous edit evaluation budget fails transactionally");
            limited.audit.max_guard_constraints=batch_work;limited.audit.max_guard_evaluations=batch_work;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(0,-10),split(0,-11)},out,limited,&report)) && report.inserted_bends==2,
                    "two safe edits share an exact cumulative continuous-node budget");
            check_audited(f,out);
            limited.audit.max_guard_constraints=batch_work-1;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(0,-10),split(0,-11)},out,limited,&report).error==TopologyError::CapacityExceeded &&
                    out.edges.empty() && report.inserted_bends==0,
                    "a later edit's constraint exhaustion rolls back the entire batch");
            limited.audit.max_guard_constraints=batch_work;limited.audit.max_guard_evaluations=batch_work-1;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(0,-10),split(0,-11)},out,limited,&report).error==TopologyError::CapacityExceeded &&
                    out.edges.empty() && report.audits==0,
                    "a later edit cannot restart the shared evaluation budget");
            const auto hidden=hidden_split(true);limited={};limited.audit.max_guard_evaluations=7;
            require(apply_topology_edits(hidden.graph,hidden.layout,hidden.ports,hidden.paths,
                    {split(40,-1000)},out,limited,&report).error==TopologyError::CapacityExceeded &&
                    out.edges.empty() && report.audits==0,
                    "split-node polynomial work obeys the same evaluation limit");
        }
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
