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
            constexpr double offset=0x1p-100;
            Fixture f;
            f.graph.nodes={{"A",10,10},{"B",10,10},{"C",10,10},{"D",10,10}};
            f.graph.edges={{0,1},{2,3}};
            f.baseline.nodes={{-42,-5,10,10},{32,-5,10,10},{-5,-110,10,10},{35,40,10,10}};
            f.ports={{{0,Side::East,0.5},{1,Side::West,0.5}},
                     {{2,Side::South,0.5},{3,Side::North,0.5}}};
            f.routes.edges={{{{-32,0},{0,0x1p-20},{32,0}}},
                            {{{0,-100},{-2*offset,-offset},{40,40}}}};
            TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
            TopologyProjectionOptions options;
            options.edits.collinear_tolerance=1e-5;options.edits.audit.orientation_epsilon=0;
            Layout moved;moved.nodes.resize(1);Routes paths;paths.edges.resize(1);
            TopologyProjectionReport report;report.projection.passes=1;report.edits.removed_bends=1;
            const auto status=project_topology_with_edits(f.graph,f.baseline,f.ports,f.routes,
                f.baseline,{}, {merge},moved,paths,options,&report);
            check(status.topology_error==TopologyError::NoAdmissibleStep,
                  "projection rejects a collapse contact whose interior event fraction rounds to one");
            check(moved.nodes.empty() && paths.edges.empty() && report.projection.passes==0 &&
                  report.edits.removed_bends==0,"near-terminal contact failure rolls back successful VPSC projection");
        }
        {
            for(const bool route_contact:{false,true}) {
                auto f=fixture();f.routes.edges[0].points={{10,5},{55,5+1e-4},{100,5}};
                if(route_contact) {
                    constexpr double offset=5e-5;
                    f.graph.nodes.push_back({"D",10,10});f.graph.edges.push_back({2,3});
                    f.baseline.nodes[2]={15,-40+offset,10,10};f.baseline.nodes.push_back({85,40+offset,10,10});
                    f.ports.push_back({{2,Side::South,0.5},{3,Side::North,0.5}});
                    f.routes.edges.push_back({{{20,f.baseline.nodes[2].y+10},{90,f.baseline.nodes[3].y}}});
                } else {
                    f.graph.nodes[2].width=2e-6;f.graph.nodes[2].height=2e-6;
                    f.baseline.nodes[2]={55-1e-6,5+4e-5,2e-6,2e-6};
                }
                TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
                TopologyProjectionOptions options;options.edits.collinear_tolerance=1e-5;
                Layout moved;moved.nodes.resize(1);Routes paths;paths.edges.resize(1);
                TopologyProjectionReport report;report.edits.removed_bends=1;
                const auto status=project_topology_with_edits(f.graph,f.baseline,f.ports,f.routes,
                    f.baseline,{}, {merge},moved,paths,options,&report);
                check(status.topology_error==TopologyError::NoAdmissibleStep,
                      "projection rejects node and route contacts during near-collinear collapse");
                check(moved.nodes.empty() && paths.edges.empty() && report.edits.removed_bends==0,
                      "continuous collapse failure clears the entire projected result");
            }
        }
        {
            auto f=fixture();
            f.graph.nodes.push_back({"D",10,10});f.graph.edges.push_back({2,3});
            f.baseline.nodes[2]={51,-1010,10,10};f.baseline.nodes.push_back({51,20,10,10});
            f.ports.push_back({{2,Side::South,0.5},{3,Side::North,0.5}});
            f.routes.edges.push_back({{{56,-1000},{56,20}}});
            TopologyEdit insert;insert.displacement={40,-1000};
            Layout moved;moved.nodes.resize(1);Routes paths;paths.edges.resize(1);
            TopologyProjectionReport report;report.edits.inserted_bends=1;
            const auto status=project_topology_with_edits(f.graph,f.baseline,f.ports,f.routes,
                f.baseline,{}, {insert},moved,paths,{},&report);
            check(status.topology_error==TopologyError::NoAdmissibleStep,
                  "projection composition rejects an isolated unsampled split-route contact");
            check(moved.nodes.empty() && paths.edges.empty() && report.edits.inserted_bends==0,
                  "route-contact edit failure clears the entire projected result");
        }
        {
            auto f=fixture();f.graph.nodes[2].width=3;f.graph.nodes[2].height=8;
            f.baseline.nodes[2]={65,-30,3,8};
            TopologyEdit insert;insert.displacement={40,-1000};
            Layout moved;moved.nodes.resize(1);Routes paths;paths.edges.resize(1);
            TopologyProjectionReport report;report.edits.inserted_bends=1;
            const auto status=project_topology_with_edits(f.graph,f.baseline,f.ports,f.routes,
                f.baseline,{}, {insert},moved,paths,{},&report);
            check(status.topology_error==TopologyError::NoAdmissibleStep,
                  "projection composition rejects an unsampled split-segment collision");
            check(moved.nodes.empty() && paths.edges.empty() && report.edits.inserted_bends==0,
                  "continuous edit failure publishes neither projected nodes nor edited routes");
        }
        {
            Graph graph;graph.nodes={{"pinned",10,10}};
            Layout baseline;baseline.nodes={{1e7,1e7,10,10}};
            Layout desired=baseline;
            ProjectionConstraints constraints;constraints.pins={{0,1e-9,-1e-9}};
            Layout moved;Routes paths;TopologyProjectionReport report;
            const auto status=project_topology_with_edits(graph,baseline,{}, {},
                desired,constraints,{},moved,paths,{},&report);
            if(!status)throw std::runtime_error("small exact pin after large displacement: "+status.message);
            check(moved.nodes[0].x==1e-9 && moved.nodes[0].y==-1e-9,
                  "full topology displacement preserves small projected hard pins exactly");
            check(report.topology.accepted_fraction==1,"large pinned displacement is fully attained");
        }
        {
            Graph graph;graph.nodes={{"moving",10,10},{"fixed",10,10}};
            Layout baseline;baseline.nodes={{0,0,10,10},{100,0,10,10}};
            ProjectionConstraints constraints;constraints.pins={{0,300,0},{1,100,0}};
            Layout moved;Routes paths;TopologyProjectionReport report;
            moved.nodes.resize(1);paths.edges.resize(1);report.topology.accepted_fraction=1;
            const auto status=project_topology_with_edits(graph,baseline,{}, {},
                baseline,constraints,{},moved,paths,{},&report);
            check(status.topology_error==TopologyError::NoAdmissibleStep,
                  "continuous isolated-node collision prevents full projected hard pins");
            check(moved.nodes.empty() && paths.edges.empty() && report.topology.accepted_fraction==0,
                  "continuous collision failure clears composed output and report");
        }
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
