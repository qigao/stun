#include "stun/graphlayout/topology_constraints.h"
#include "stun/graphlayout/topology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace stun::graphlayout;

namespace {
void require(bool yes, const char* message) {
    if (!yes) throw std::runtime_error(message);
}
void near(double a, double b, double epsilon, const char* message) {
    if (std::abs(a-b)>epsilon) throw std::runtime_error(message);
}
struct SpiralFixture {
    Graph graph;
    Layout baseline, desired;
    Routes routes;
    std::vector<RouteRequest> ports;
};
SpiralFixture spiral(double scale=1.0) {
    SpiralFixture f;
    f.graph.nodes={{"source",10,10},{"target",10,10}};
    f.graph.edges={{0,1}};
    f.baseline.nodes={{-10,-5,10,10},{80,15,10,10}};
    f.routes.edges={{{{0,0},{100,0},{100,100},{20,100},{20,20},{80,20}}}};
    f.ports={{{0,Side::East,0.5},{1,Side::West,0.5}}};
    for(auto& n:f.graph.nodes) {n.width*=scale;n.height*=scale;}
    for(auto& n:f.baseline.nodes) {n.x*=scale;n.y*=scale;n.width*=scale;n.height*=scale;}
    for(auto& p:f.routes.edges[0].points) {p.x*=scale;p.y*=scale;}
    f.desired=f.baseline;f.desired.nodes[1].y-=1000*scale;
    return f;
}
void spiral_frame(const SpiralFixture& f, double fraction, Layout& placement, Routes& paths) {
    constexpr std::array<double,6> lengths={0,100,200,280,360,420};
    const double displacement=f.desired.nodes[1].y-f.baseline.nodes[1].y;
    placement=f.baseline;placement.nodes[1].y+=fraction*displacement;
    paths=f.routes;
    for(std::size_t i=0;i<lengths.size();++i)
        paths.edges[0].points[i].y+=fraction*displacement*lengths[i]/lengths.back();
    paths.edges[0].points.back().y=placement.nodes[1].y+placement.nodes[1].height*0.5;
}
void run() {
    {
        auto f=spiral();
        TopologySignature signature;
        require(static_cast<bool>(audit_topology(f.graph,f.baseline,f.ports,f.routes,signature)),
                "spiral baseline has no self contact");
        // Total arclength is 420. Along the overlapping x range, the inner
        // horizontal segment is 340 arclength units after the outer one.
        // Their initial 20-unit separation closes at 20*420/(1000*340).
        constexpr double first_contact=21.0/850.0;
        constexpr double probe=0.1;
        Layout at_contact;Routes crossing;
        spiral_frame(f,probe,at_contact,crossing);
        require(audit_topology(f.graph,at_contact,f.ports,crossing,signature).error==
                TopologyError::AmbiguousTopology,"intermediate spiral genuinely self-intersects");
        TopologyOptions sampled;sampled.analytic_guards=false;sampled.samples_per_trial=1;
        Layout moved;Routes paths;TopologyReport movement;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,moved,paths,sampled,&movement)) && movement.accepted_fraction==1,
                "endpoint-only sampled motion misses the intermediate self contact");
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)),
                "same-route analytic guard can inspect the spiral");
        require(guard.segment_pair_constraints>0 && guard.max_safe_fraction>0 &&
                guard.max_safe_fraction<first_contact,"same-route guards cap motion before hidden self contact");
        sampled.analytic_guards=true;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,moved,paths,sampled,&movement)) && movement.accepted_fraction>0 &&
                movement.accepted_fraction<first_contact,"single-sample continuation respects self-contact event");
        require(static_cast<bool>(audit_topology(f.graph,moved,f.ports,paths,signature)),
                "accepted spiral prefix independently passes geometry audit");
    }
    {
        constexpr std::size_t cases=32;
        constexpr std::size_t samples=16;
        constexpr double first_contact=21.0/850.0;
        for(std::size_t i=0;i<cases;++i) {
            auto f=spiral(1.0+static_cast<double>(i)/16.0);
            if(i%2) {
                for(auto& n:f.baseline.nodes)n.y=-n.y-n.height;
                for(auto& p:f.routes.edges[0].points)p.y=-p.y;
                f.desired=f.baseline;f.desired.nodes[1].y+=1000*(1.0+static_cast<double>(i)/16.0);
            }
            TopologyGuardReport guard;
            const auto st=limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard);
            require(static_cast<bool>(st) && guard.max_safe_fraction>0 &&
                    guard.max_safe_fraction<first_contact,"scaled and reflected self-contact prefix is bounded");
            TopologyOptions single;single.samples_per_trial=1;
            Layout moved;Routes moved_paths;TopologyReport movement;
            require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                    f.desired,moved,moved_paths,single,&movement)) && movement.accepted_fraction>0 &&
                    movement.accepted_fraction<=guard.max_safe_fraction,
                    "full geometry audit may further limit the analytic self-contact prefix");
            for(std::size_t sample=0;sample<=samples;++sample) {
                Layout placement;Routes paths;TopologySignature signature;
                spiral_frame(f,movement.accepted_fraction*static_cast<double>(sample)/samples,placement,paths);
                const auto audited=audit_topology(f.graph,placement,f.ports,paths,signature);
                if(!audited)throw std::runtime_error("spiral case "+std::to_string(i)+
                    ", sample "+std::to_string(sample)+": "+audited.message);
            }
        }
    }
    {
        auto f=spiral();
        TopologyGuardReport original,reversed;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,original)),
                "forward route can be guarded");
        std::swap(f.graph.edges[0].source,f.graph.edges[0].target);
        std::swap(f.ports[0].source,f.ports[0].target);
        std::reverse(f.routes.edges[0].points.begin(),f.routes.edges[0].points.end());
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,reversed)),
                "reversed route can be guarded");
        near(original.max_safe_fraction,reversed.max_safe_fraction,1e-12,
             "self-contact event does not depend on edge orientation");
        require(original.segment_pair_constraints==reversed.segment_pair_constraints &&
                original.evaluations==reversed.evaluations,"reversing route retains guard work counts");
        TopologyOptions single;single.samples_per_trial=1;
        Layout moved;Routes paths;TopologyReport movement;TopologySignature signature;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,moved,paths,single,&movement)) && movement.accepted_fraction<21.0/850.0,
                "reversed edge also stays before self contact");
        require(static_cast<bool>(audit_topology(f.graph,moved,f.ports,paths,signature)),
                "reversed edge prefix retains exact measured ports");
        f.desired=f.baseline;
        for(auto& n:f.desired.nodes) {n.x+=37;n.y-=23;}
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,reversed)) &&
                reversed.max_safe_fraction==1,"rigid spiral translation imposes no artificial event");
    }
    {
        const auto f=spiral();
        // One node pair, four bends and six independent segment pairs with
        // four orientation evaluations each must share one bounded budget.
        constexpr std::size_t evaluations=29;
        TopologyGuardOptions limited;limited.max_constraints=evaluations;limited.max_evaluations=evaluations;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited)) &&
                guard.evaluations==evaluations,"same-route event work fits exact shared budgets");
        limited.max_constraints=evaluations-1;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0,
                "same-route constraint exhaustion publishes no partial guard report");
        limited.max_constraints=evaluations;limited.max_evaluations=evaluations-1;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0,
                "same-route evaluation exhaustion is transactional");
        TopologyOptions scarce;scarce.max_guard_constraints=evaluations-1;
        Layout moved;moved.nodes.resize(1);
        Routes paths;paths.edges.resize(1);
        TopologyReport movement;movement.accepted_fraction=1;
        require(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,f.desired,
                moved,paths,scarce,&movement).error==TopologyError::CapacityExceeded &&
                moved.nodes.empty() && paths.edges.empty() && movement.accepted_fraction==0,
                "self-contact guard budget failure clears movement output");
    }
    {
        TopologyTriConstraint c;
        c.u={0,0};c.v={10,10};c.w={0,20};c.gap=0;c.p=0.5;c.below=true;
        double fraction=-1;
        require(static_cast<bool>(limit_topology_tri(c,fraction)),"positive tri constraint solvable");
        require(fraction>0.24 && fraction<0.25,"tri analytic event computed before crossing");
        c.below=false;
        require(limit_topology_tri(c,fraction).error==TopologyGuardError::InfeasibleBaseline,
                "opposite side of separator fails initial feasibility");
        c.below=true;c.w={0,-20};
        require(static_cast<bool>(limit_topology_tri(c,fraction)) && fraction==1,
                "desired motion away from constraint admits full displacement");
    }
    {
        TopologyBendConstraint bend;
        bend.first={{0,0},{0,0}};
        bend.bend={{1,1},{1,-1}};
        bend.last={{2,2},{0,0}};
        double fraction=-1;
        require(static_cast<bool>(limit_topology_bend(bend,fraction)),"quadratic bend event detected");
        require(fraction<0.5 && fraction>0.49,"bend orientation change capped analytically");
        // A quadratic can have two roots even when its endpoints have the
        // same sign. Merely testing the endpoint orientation is insufficient.
        TopologyBendConstraint twice;
        twice.first={{0,0},{0,0}};
        twice.bend={{0,1},{1,1}};
        twice.last={{-0.1875,-0.1875},{-1,0}};
        require(static_cast<bool>(limit_topology_bend(twice,fraction)),
                "two-crossing bend trajectory can be analyzed");
        require(fraction<0.25 && fraction>0.24,
                "analytic quadratic catches hidden intermediate bend flip");
        bend.bend={{1,1},{1,2}};
        require(static_cast<bool>(limit_topology_bend(bend,fraction)) && fraction==1,
                "bend widening preserves orientation");
        bend.bend={{1,1},{0,0}};
        require(static_cast<bool>(limit_topology_bend(bend,fraction)) && fraction==1,
                "baseline-collinear triple is not a bend constraint");
    }
    {
        Graph graph;
        graph.nodes={{"A",10,10},{"B",10,10},{"obstacle",10,10}};
        graph.edges={{0,1}};
        Layout baseline;
        baseline.nodes={{0,0,10,10},{100,0,10,10},{40,25,10,10}};
        Layout desired=baseline;
        desired.nodes[2].y=-10;
        Routes routes;
        routes.edges={{{{10,5},{100,5}}}};
        TopologyGuardReport result;
        const auto s=limit_topology_movement(graph,baseline,desired,routes,result);
        if(!s)throw std::runtime_error(std::string("node-segment guard failure: ")+s.message);
        require(result.straight_constraints>0,"unrelated segment-node guard created");
        require(result.max_safe_fraction<20.0/35.0 && result.max_safe_fraction>0.55,
                "node cannot move through connector's material scan position");
        desired.nodes[2].y=40;
        require(static_cast<bool>(limit_topology_movement(graph,baseline,desired,routes,result)),
                "separating node from connector is valid");
        near(result.max_safe_fraction,1,0,"full motion when separating");
        TopologyGuardOptions limited;
        limited.max_constraints=4;
        require(static_cast<bool>(limit_topology_movement(graph,baseline,desired,routes,result,limited)),
                "three node pairs and one node-segment constraint fit exact configured count");
        limited.max_constraints=3;
        require(limit_topology_movement(graph,baseline,desired,routes,result,limited).error==
                TopologyGuardError::CapacityExceeded,"node pairs share the constraint budget");
        limited.max_constraints=4;
        limited.max_evaluations=1;
        require(limit_topology_movement(graph,baseline,desired,routes,result,limited).error==
                TopologyGuardError::CapacityExceeded,
                "scan and constraint work are cumulatively bounded");
        limited={};limited.node_segment_clearance=25;
        require(limit_topology_movement(graph,baseline,desired,routes,result,limited).error==
                TopologyGuardError::InfeasibleBaseline,
                "initial material segment clearance violation is explicit");
    }
    {
        Graph graph;
        graph.nodes={{"start",10,10},{"end",10,10},{"obstacle",10,10}};
        graph.edges={{0,1}};
        Layout layout, desired;
        layout.nodes={{0,0,10,10},{100,0,10,10},{40,25,10,10}};
        desired=layout;
        desired.nodes[2].y=-10;
        const std::vector<RouteRequest> ports{{{0,Side::East,0.5},{1,Side::West,0.5}}};
        Routes path;path.edges={{{{10,5},{100,5}}}};
        Layout result;
        Routes routed;
        TopologyReport report;
        const auto status=move_topology_preserving(graph,layout,ports,path,desired,
                                                   result,routed,{},&report);
        if(!status)throw std::runtime_error("integrated analytic guard: "+status.message);
        require(report.straight_constraints==1 && report.analytic_guard_fraction>0.55 &&
                report.analytic_guard_fraction<20.0/35.0,
                "integrated Node-Segment guard limits the attempted movement");
        require(report.accepted_fraction<=report.analytic_guard_fraction &&
                result.nodes[2].y>5.0,
                "node must stay on the original side of connector");
        TopologyOptions scarce;scarce.max_guard_evaluations=1;
        result.nodes.push_back({});routed.edges.push_back({});
        const auto exhausted=move_topology_preserving(graph,layout,ports,path,desired,
                                                      result,routed,scarce,&report);
        require(exhausted.error==TopologyError::CapacityExceeded &&
                result.nodes.empty() && routed.edges.empty(),
                "analytic guard work budget exhausts transactionally");
    }
    {
        // Two disjoint connectors exchange vertical lanes over one move.
        // A full-frame-only audit would accept both endpoints, but the
        // analytic segment-pair orientation guards catch the halfway touch.
        Graph graph;
        graph.nodes={{"left_top",10,10},{"right_top",10,10},
                     {"left_bottom",10,10},{"right_bottom",10,10}};
        graph.edges={{0,1},{2,3}};
        Layout baseline;
        baseline.nodes={{0,0,10,10},{100,0,10,10},
                        {0,100,10,10},{100,100,10,10}};
        Layout desired=baseline;
        for(std::size_t i=0;i<2;++i)desired.nodes[i].y+=100;
        for(std::size_t i=2;i<4;++i)desired.nodes[i].y-=100;
        Routes paths;
        paths.edges={{{{10,5},{100,5}}},{{{10,105},{100,105}}}};
        TopologyGuardReport analytic;
        require(static_cast<bool>(limit_topology_movement(graph,baseline,desired,paths,analytic)),
                "segment-pair swap constraints generated");
        require(analytic.segment_pair_constraints>=4 &&
                analytic.max_safe_fraction<0.5 &&
                analytic.max_safe_fraction>0,
                "quadratic segment orientations are included in safe prefix");
        TopologyOptions single;
        single.samples_per_trial=1;
        single.max_trials=8;
        std::vector<RouteRequest> ports={
            {{0,Side::East,0.5},{1,Side::West,0.5}},
            {{2,Side::East,0.5},{3,Side::West,0.5}}
        };
        Layout moved;Routes moved_paths;TopologyReport report;
        const auto safe=move_topology_preserving(graph,baseline,ports,paths,desired,
                                                 moved,moved_paths,single,&report);
        if(!safe)throw std::runtime_error("single-sample topology continuation: "+safe.message);
        require(report.segment_pair_constraints>=4 &&
                report.accepted_fraction<0.5 && report.accepted_fraction>0,
                "single-sample continuation is analytically capped");
    }
    {
        Graph graph;graph.nodes={{"moving",10,10},{"fixed",10,10}};
        Layout baseline;baseline.nodes={{0,0,10,10},{100,0,10,10}};
        Layout desired=baseline;desired.nodes[0].x=300;
        TopologyGuardReport report;
        auto st=limit_topology_movement(graph,baseline,desired,{},report);
        require(static_cast<bool>(st) && report.max_safe_fraction>0.29 &&
                report.max_safe_fraction<0.3,"continuous isolated-node collision is capped");
        require(report.evaluations==1,"one node pair consumes one guard evaluation");
        desired.nodes[0].y=100;
        st=limit_topology_movement(graph,baseline,desired,{},report);
        require(static_cast<bool>(st) && report.max_safe_fraction==1,
                "changing axis of separation permits a diagonal bypass");
        baseline.nodes[1].y=10;desired=baseline;desired.nodes[0].x=300;
        st=limit_topology_movement(graph,baseline,desired,{},report);
        require(static_cast<bool>(st) && report.max_safe_fraction==1,
                "boundary-only sliding contact is permitted");
        baseline.nodes[1]={40,60,10,10};desired=baseline;desired.nodes[0].x=100;desired.nodes[0].y=100;
        st=limit_topology_movement(graph,baseline,desired,{},report);
        require(static_cast<bool>(st) && report.max_safe_fraction==1,
                "open overlap intervals meeting at a single corner are permitted");
        baseline.nodes[1]={10,0,10,10};desired=baseline;desired.nodes[0].x=1;
        st=limit_topology_movement(graph,baseline,desired,{},report);
        require(static_cast<bool>(st) && report.max_safe_fraction==0,
                "initial touching rectangles moving inward allow no positive prefix");
        desired.nodes[0].x=-1;
        st=limit_topology_movement(graph,baseline,desired,{},report);
        require(static_cast<bool>(st) && report.max_safe_fraction==1,
                "initial touching rectangles may separate");
        baseline.nodes[1].x=9;desired=baseline;
        report.max_safe_fraction=0.5;
        st=limit_topology_movement(graph,baseline,desired,{},report);
        require(st.error==TopologyGuardError::InfeasibleBaseline && report.max_safe_fraction==1 &&
                report.evaluations==0,"overlapping baseline rejected without publishing a partial report");
        graph.nodes[0].width=-10;baseline.nodes[0].width=-10;desired=baseline;
        require(limit_topology_movement(graph,baseline,desired,{},report).error==
                TopologyGuardError::InvalidInput,"negative node extents rejected before swept math");
    }
    {
        // Fixed 10-unit rectangles exchange horizontal positions. Collision
        // begins at (initial gap)/(relative displacement), independently of sampling.
        constexpr std::size_t cases=64;
        constexpr std::size_t samples=32;
        for(std::size_t i=0;i<cases;++i) {
            const double spacing=40.0+i;
            Graph graph;graph.nodes={{"a",10,10},{"b",10,10}};
            Layout baseline;baseline.nodes={{-spacing,0,10,10},{spacing,0,10,10}};
            Layout desired=baseline;desired.nodes[0].x=spacing;desired.nodes[1].x=-spacing;
            TopologyGuardReport report;
            const auto st=limit_topology_movement(graph,baseline,desired,{},report);
            const double event=(2*spacing-10)/(4*spacing);
            require(static_cast<bool>(st) && report.max_safe_fraction>0 &&
                    report.max_safe_fraction<event,"swapped-node prefix ends before continuous collision");
            for(std::size_t sample=0;sample<=samples;++sample) {
                const double f=report.max_safe_fraction*static_cast<double>(sample)/samples;
                const double left=-spacing+f*2*spacing;
                const double right=spacing-f*2*spacing;
                require(left+10<=right,"all checked rectangle-prefix positions remain separated");
            }
        }
    }
    {
        Graph graph;graph.nodes={{"a",10,10},{"b",10,10},{"c",10,10}};
        Layout baseline;baseline.nodes={{0,0,10,10},{100,0,10,10},{200,0,10,10}};
        TopologyGuardOptions options;options.max_constraints=2;
        TopologyGuardReport report;
        auto st=limit_topology_movement(graph,baseline,baseline,{},report,options);
        require(st.error==TopologyGuardError::CapacityExceeded && report.evaluations==0,
                "node-pair constraint cap fails transactionally before publishing report");
        options={};options.max_evaluations=2;
        st=limit_topology_movement(graph,baseline,baseline,{},report,options);
        require(st.error==TopologyGuardError::CapacityExceeded && report.evaluations==0,
                "node-pair evaluation cap fails transactionally");
    }
    {
        // Large translation with unchanged relative node/segment geometry.
        Graph graph;
        graph.nodes={{"A",8,8},{"B",8,8},{"C",12,12}};
        graph.edges={{0,1}};
        Layout baseline,desired;
        baseline.nodes={{0,0,8,8},{100,0,8,8},{40,30,12,12}};
        desired=baseline;
        for(auto& n:desired.nodes){n.x+=120;n.y+=80;}
        Routes routes;
        routes.edges={{{{8,4},{100,4}}}};
        TopologyGuardReport report;
        require(static_cast<bool>(limit_topology_movement(graph,baseline,desired,routes,report)),
                "rigid translation has no topology guard events");
        require(report.max_safe_fraction==1 && report.straight_constraints>0,
                "rigid movement accepted exactly");
    }
    {
        // 96 reproducible affine separation cases: accepted prefixes must
        // remain feasible at every sampled point, including reversed sides.
        std::uint64_t state=0x8f45ab31ULL;
        auto draw=[&]() -> double {
            state=state*6364136223846793005ULL+1442695040888963407ULL;
            return static_cast<double>(static_cast<int>((state>>32)%121)-60);
        };
        for(std::size_t case_id=0;case_id<96;++case_id){
            TopologyTriConstraint c;
            c.u.initial=draw();c.v.initial=draw();
            c.u.desired=c.u.initial+draw();c.v.desired=c.v.initial+draw();
            c.p=(case_id%3+1)*0.25;
            c.gap=1.0;
            c.below=(case_id%2==0);
            const double on_line=(1.0-c.p)*c.u.initial+c.p*c.v.initial;
            const double sign=c.below?1.0:-1.0;
            c.w.initial=on_line-sign*(5.0+case_id%7);
            c.w.desired=c.w.initial+draw();
            double fraction=0;
            const auto st=limit_topology_tri(c,fraction);
            require(static_cast<bool>(st),"generated triangle starts feasible");
            require(fraction>=0 && fraction<=1,"generated analytic safe fraction bounded");
            for(std::size_t sample=0;sample<=80;++sample){
                const double alpha=fraction*static_cast<double>(sample)/80.0;
                const double u=c.u.initial+(c.u.desired-c.u.initial)*alpha;
                const double v=c.v.initial+(c.v.desired-c.v.initial)*alpha;
                const double w=c.w.initial+(c.w.desired-c.w.initial)*alpha;
                const double slack=sign*((1.0-c.p)*u+c.p*v-w)-c.gap;
                require(slack>=-1e-8,"every generated tri prefix preserves separation");
            }
        }
    }
    {
        TopologyTriConstraint bad;
        bad.u={0,0};bad.v={10,10};bad.w={0,20};bad.p=1.25;
        double fraction=123.0;
        require(limit_topology_tri(bad,fraction).error==TopologyGuardError::InvalidInput,
                "invalid material fraction rejected");
        require(fraction==123.0,"failure does not expose a partial analytic fraction");
        TopologyGuardOptions options;options.max_evaluations=0;
        require(limit_topology_tri({},fraction,options).error==TopologyGuardError::InvalidOptions,
                "invalid resource budget rejected");
    }
}
}

int main() {
    try { run();std::cout<<"topology constraints: passed\n";return EXIT_SUCCESS; }
    catch(const std::exception& e) {std::cerr<<"topology constraints: "<<e.what()<<'\n';return EXIT_FAILURE;}
}
