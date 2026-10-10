#include "stun/graphlayout/topology_projection.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace stun::graphlayout;
namespace {
void check(bool yes,const char* msg) { if(!yes)throw std::runtime_error(msg); }
struct Fixture {Graph graph;Layout baseline;std::vector<RouteRequest> ports;Routes routes;};
Fixture fixture(){
    Fixture f;
    f.graph.nodes={{"A",10,10},{"B",10,10},{"C",10,10}};
    f.graph.edges={{0,1}};
    f.baseline.nodes={{0,0,10,10},{100,0,10,10},{45,30,10,10}};
    f.ports={{{0,Side::East,0.5},{1,Side::West,0.5}}};
    f.routes.edges={{{{10,5},{100,5}}}};
    return f;
}
}
int main(){
    try{
        {
            auto f=fixture();
            Layout desired=f.baseline;desired.nodes[0].x=15;desired.nodes[1].x=140;
            ProjectionConstraints c;c.pins={{0,0,0}};
            Layout moved;Routes paths;TopologyProjectionReport report;
            auto status=project_topology_with_edits(f.graph,f.baseline,f.ports,f.routes,
                                                     desired,c,{},moved,paths,{},&report);
            if(!status)throw std::runtime_error("projected move failed: "+status.message);
            check(moved.nodes[0].x==0 && moved.nodes[0].y==0,"hard pin exact");
            check(moved.nodes[1].x==140,"projected node reaches target");
            check(paths.edges[0].points.front().x==10 &&
                  paths.edges[0].points.back().x==140,"route terminal follows projection");
            check(report.topology.accepted_fraction==1,"full projected goal accepted");
            TopologySignature signature;
            check(static_cast<bool>(audit_topology(f.graph,moved,f.ports,paths,signature)),
                  "projected result independently topology-auditable");
        }
        {
            auto f=fixture();
            Layout desired=f.baseline;desired.nodes[1].x=150;
            TopologyEdit insert;insert.point_index=0;insert.segment_fraction=0.5;
            insert.displacement={0,-20};
            ProjectionConstraints c;c.pins={{0,0,0}};
            Layout moved;Routes routed;TopologyProjectionReport report;
            auto status=project_topology_with_edits(f.graph,f.baseline,f.ports,f.routes,
                                                     desired,c,{insert},moved,routed,{},&report);
            if(!status)throw std::runtime_error("composed edit + projection: "+status.message);
            check(routed.edges[0].points.size()==3 &&
                  routed.edges[0].points[1].y<5,"bend retained during VPSC projected move");
            check(report.edits.inserted_bends==1,"route edit applied before topology motion");
            check(moved.nodes[0].x==0 && moved.nodes[1].x==150,"exact pinned/projected geometry");
        }
        {
            auto f=fixture();
            Layout desired=f.baseline;desired.nodes[2].y=-10;
            ProjectionConstraints c;
            Layout moved;Routes paths;TopologyProjectionReport report;
            auto status=project_topology_with_edits(f.graph,f.baseline,f.ports,f.routes,
                                                     desired,c,{},moved,paths,{},&report);
            check(status.topology_error==TopologyError::NoAdmissibleStep,
                  "a partial topology move cannot falsely satisfy full projection");
            check(moved.nodes.empty() && paths.edges.empty() &&
                  report.topology.accepted_fraction==0,"fractional result is transactional failure");
        }
        {
            auto f=fixture();
            Layout desired=f.baseline;
            ProjectionConstraints c;c.pins={{0,0,0},{1,0,0}};
            Layout moved;Routes paths;
            auto status=project_topology_with_edits(f.graph,f.baseline,f.ports,f.routes,
                                                     desired,c,{},moved,paths);
            check(status.projection_error==ProjectionError::Infeasible,
                  "VPSC infeasibility propagated without moving routes");
            check(moved.nodes.empty() && paths.edges.empty(),"VPSC failure atomic");
        }
        {
            auto f=fixture();
            Layout desired=f.baseline;
            TopologyProjectionOptions opt;opt.projection.vpsc.max_variables=1;
            Layout moved;Routes paths;
            auto status=project_topology_with_edits(f.graph,f.baseline,f.ports,f.routes,
                                                     desired,{}, {},moved,paths,opt);
            check(status.projection_error==ProjectionError::CapacityExceeded,
                  "VPSC resource limits remain effective in topology composition");
            check(moved.nodes.empty() && paths.edges.empty(),"capacity failure atomic");
        }
        std::cout<<"topology/VPSC integration: all checks passed\n";
        return EXIT_SUCCESS;
    }catch(const std::exception& ex){
        std::cerr<<"topology/VPSC integration: "<<ex.what()<<'\n';return EXIT_FAILURE;
    }
}
