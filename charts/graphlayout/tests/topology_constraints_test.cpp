#include "stun/graphlayout/topology_constraints.h"
#include "stun/graphlayout/topology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
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
struct MovementFixture {
    Graph graph;
    Layout baseline, desired;
    Routes routes;
    std::vector<RouteRequest> ports;
};
MovementFixture spiral(double scale=1.0) {
    MovementFixture f;
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
void spiral_frame(const MovementFixture& f, double fraction, Layout& placement, Routes& paths) {
    constexpr std::array<double,6> lengths={0,100,200,280,360,420};
    const double displacement=f.desired.nodes[1].y-f.baseline.nodes[1].y;
    placement=f.baseline;placement.nodes[1].y+=fraction*displacement;
    paths=f.routes;
    for(std::size_t i=0;i<lengths.size();++i)
        paths.edges[0].points[i].y+=fraction*displacement*lengths[i]/lengths.back();
    paths.edges[0].points.back().y=placement.nodes[1].y+placement.nodes[1].height*0.5;
}
enum class SwapAxis { Horizontal, Vertical, Diagonal };
MovementFixture collinear_swap(SwapAxis axis=SwapAxis::Horizontal, double scale=1.0) {
    MovementFixture f;
    f.graph.nodes={{"a",10,10},{"b",10,10},{"c",10,10},{"d",10,10}};
    f.graph.edges={{0,1},{2,3}};
    // Opposite boundary lanes let nodes pass with no interior collision.
    f.baseline.nodes={{-10,-10,10,10},{20,-10,10,10},{90,0,10,10},{120,0,10,10}};
    f.routes.edges={{{{0,0},{20,0}}},{{{100,0},{120,0}}}};
    f.ports={{{0,Side::East,1},{1,Side::West,1}},
             {{2,Side::East,0},{3,Side::West,0}}};
    if(axis==SwapAxis::Vertical) {
        for(auto& n:f.baseline.nodes)std::swap(n.x,n.y);
        for(auto& route:f.routes.edges)for(auto& p:route.points)std::swap(p.x,p.y);
        for(auto& ports:f.ports) {ports.source.side=Side::South;ports.target.side=Side::North;}
    } else if(axis==SwapAxis::Diagonal) {
        f.baseline.nodes={{-10,0,10,10},{10,20,10,10},{100,90,10,10},{120,110,10,10}};
        for(auto& route:f.routes.edges)for(auto& p:route.points)p.y=p.x;
        f.ports={{{0,Side::East,0},{1,Side::North,1}},
                 {{2,Side::South,0},{3,Side::West,1}}};
    }
    for(auto& n:f.graph.nodes) {n.width*=scale;n.height*=scale;}
    for(auto& n:f.baseline.nodes) {n.x*=scale;n.y*=scale;n.width*=scale;n.height*=scale;}
    for(auto& route:f.routes.edges)for(auto& p:route.points) {p.x*=scale;p.y*=scale;}
    f.desired=f.baseline;
    for(std::size_t i=0;i<f.desired.nodes.size();++i) {
        const double displacement=(i<2 ? 200 : -200)*scale;
        if(axis!=SwapAxis::Vertical)f.desired.nodes[i].x+=displacement;
        if(axis!=SwapAxis::Horizontal)f.desired.nodes[i].y+=displacement;
    }
    return f;
}
Point fixture_anchor(const Layout& layout, const Port& port) {
    const auto& node=layout.nodes[port.node];
    switch(port.side) {
    case Side::North:return {node.x+node.width*port.offset,node.y};
    case Side::East:return {node.x+node.width,node.y+node.height*port.offset};
    case Side::South:return {node.x+node.width*port.offset,node.y+node.height};
    case Side::West:return {node.x,node.y+node.height*port.offset};
    default:throw std::runtime_error("invalid fixture port");
    }
}
// These fixtures translate both endpoints of each route equally.
void rigid_frame(const MovementFixture& f, double fraction, Layout& placement, Routes& paths) {
    placement=f.baseline;paths=f.routes;
    for(std::size_t i=0;i<placement.nodes.size();++i) {
        placement.nodes[i].x+=fraction*(f.desired.nodes[i].x-f.baseline.nodes[i].x);
        placement.nodes[i].y+=fraction*(f.desired.nodes[i].y-f.baseline.nodes[i].y);
    }
    for(std::size_t i=0;i<paths.edges.size();++i) {
        const auto source=f.graph.edges[i].source;
        for(auto& p:paths.edges[i].points) {
            p.x+=fraction*(f.desired.nodes[source].x-f.baseline.nodes[source].x);
            p.y+=fraction*(f.desired.nodes[source].y-f.baseline.nodes[source].y);
        }
        // Auditing requires exact measured anchors, including rounding after
        // the independently interpolated node position is computed.
        paths.edges[i].points.front()=fixture_anchor(placement,f.ports[i].source);
        paths.edges[i].points.back()=fixture_anchor(placement,f.ports[i].target);
    }
}
MovementFixture waypoint_sweep(bool vertical=false, double scale=1.0) {
    MovementFixture f;
    f.graph.nodes={{"source",10,10},{"target",10,10},{"obstacle",10,10}};
    f.graph.edges={{0,1}};
    f.baseline.nodes={{-5,100,10,10},{15,100,10,10},{80,-5,10,10}};
    f.routes.edges={{{{0,100},{0,0},{20,0},{20,100}}}};
    f.ports={{{0,Side::North,0.5},{1,Side::North,0.5}}};
    if(vertical) {
        for(auto& n:f.baseline.nodes)std::swap(n.x,n.y);
        for(auto& p:f.routes.edges[0].points)std::swap(p.x,p.y);
        f.ports[0].source.side=Side::West;f.ports[0].target.side=Side::West;
    }
    for(auto& n:f.graph.nodes) {n.width*=scale;n.height*=scale;}
    for(auto& n:f.baseline.nodes) {n.x*=scale;n.y*=scale;n.width*=scale;n.height*=scale;}
    for(auto& p:f.routes.edges[0].points) {p.x*=scale;p.y*=scale;}
    f.desired=f.baseline;
    for(std::size_t i=0;i<2;++i) {
        if(vertical)f.desired.nodes[i].y+=200*scale;
        else f.desired.nodes[i].x+=200*scale;
    }
    return f;
}
MovementFixture segment_sweep(bool vertical=false, double scale=1.0) {
    MovementFixture f;
    f.graph.nodes={{"source",10,10},{"target",10,10},{"obstacle",10,10}};
    f.graph.edges={{0,1}};
    f.baseline.nodes={{-10,-5,10,10},{100,-5,10,10},{150,-50,10,10}};
    f.routes.edges={{{{0,0},{100,0}}}};
    f.ports={{{0,Side::East,0.5},{1,Side::West,0.5}}};
    f.desired=f.baseline;
    for(std::size_t i=0;i<2;++i) {f.desired.nodes[i].x+=200;f.desired.nodes[i].y-=100;}
    if(vertical) {
        for(auto* layout:{&f.baseline,&f.desired})for(auto& n:layout->nodes)std::swap(n.x,n.y);
        for(auto& p:f.routes.edges[0].points)std::swap(p.x,p.y);
        f.ports[0].source.side=Side::South;f.ports[0].target.side=Side::North;
    }
    for(auto& n:f.graph.nodes) {n.width*=scale;n.height*=scale;}
    for(auto* layout:{&f.baseline,&f.desired})for(auto& n:layout->nodes) {
        n.x*=scale;n.y*=scale;n.width*=scale;n.height*=scale;
    }
    for(auto& p:f.routes.edges[0].points) {p.x*=scale;p.y*=scale;}
    return f;
}
MovementFixture crossing_order_sweep(bool quartic=false, bool tangent=false) {
    MovementFixture f;
    f.graph.nodes={{"west",8,8},{"east",8,8},{"southwest",8,8},
                   {"northeast",8,8},{"north",8,8},{"south",8,8}};
    f.graph.edges={{0,1},{2,3},{4,5}};
    f.baseline.nodes={{-264,-4,8,8},{256,-4,8,8},{-68,-72,8,8},
                      {60,52,8,8},{-4,-264,8,8},{-4,256,8,8}};
    f.routes.edges={{{{-256,0},{256,0}}},{{{-64,-64},{64,52}}},{{{0,-256},{0,256}}}};
    f.ports={{{0,Side::East,0.5},{1,Side::West,0.5}},
             {{2,Side::South,0.5},{3,Side::North,0.5}},
             {{4,Side::South,0.5},{5,Side::North,0.5}}};
    f.desired=f.baseline;f.desired.nodes[2].y-=64;
    f.desired.nodes[3].x+=64;f.desired.nodes[3].y+=192;
    if(tangent) {
        f.baseline.nodes[3].y-=4;f.desired.nodes[3].y-=4;
        f.routes.edges[1].points.back().y-=4;
    }
    if(quartic) {
        f.desired.nodes[0].y-=32;f.desired.nodes[1].y+=32;
        f.desired.nodes[4].x-=16;f.desired.nodes[5].x+=16;
    }
    return f;
}
void crossing_order_frame(const MovementFixture& f, double fraction, Layout& placement, Routes& paths) {
    placement=f.baseline;paths=f.routes;
    for(std::size_t node=0;node<placement.nodes.size();++node) {
        placement.nodes[node].x+=fraction*(f.desired.nodes[node].x-f.baseline.nodes[node].x);
        placement.nodes[node].y+=fraction*(f.desired.nodes[node].y-f.baseline.nodes[node].y);
    }
    // Every fixture route is a single segment: measured anchors determine
    // the whole interpolated path, independently of the production guard.
    for(std::size_t edge=0;edge<paths.edges.size();++edge) {
        paths.edges[edge].points.front()=fixture_anchor(placement,f.ports[edge].source);
        paths.edges[edge].points.back()=fixture_anchor(placement,f.ports[edge].target);
    }
}
void run() {
    {
        const auto f=crossing_order_sweep();
        TopologySignature initial, middle, final;
        require(static_cast<bool>(audit_topology(f.graph,f.baseline,f.ports,f.routes,initial)) &&
                initial.crossing_count==3,"three proper crossings form the order-sweep baseline");
        Layout placement;Routes paths;crossing_order_frame(f,0.5,placement,paths);
        require(static_cast<bool>(audit_topology(f.graph,placement,f.ports,paths,middle)) &&
                middle.crossing_count==3 &&
                middle.ordered_crossings[0][0].other_edge!=initial.ordered_crossings[0][0].other_edge,
                "crossing order changes without any pair losing a proper crossing");
        crossing_order_frame(f,1,placement,paths);
        require(static_cast<bool>(audit_topology(f.graph,placement,f.ports,paths,final)) &&
                final.ordered_crossings[0][0].other_edge==initial.ordered_crossings[0][0].other_edge,
                "final crossing order returns to its initial sequence");
        crossing_order_frame(f,0.25,placement,paths);
        require(audit_topology(f.graph,placement,f.ports,paths,middle).error==TopologyError::AmbiguousTopology,
                "independent audit rejects the exact triple intersection");
        TopologyOptions single;single.samples_per_trial=1;single.analytic_guards=false;
        TopologyReport movement;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,placement,paths,single,&movement)) && movement.accepted_fraction==1,
                "final-frame sampling misses two intermediate crossing-order exchanges");
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction>0.24 && guard.max_safe_fraction<0.25,
                "continuous crossing-order guard stops before the first triple event");
        single.analytic_guards=true;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,placement,paths,single,&movement)) && movement.accepted_fraction>0.24 &&
                movement.accepted_fraction<0.25,"single-sample continuation respects crossing-order events");
        constexpr std::size_t samples=32;
        for(std::size_t i=0;i<=samples;++i) {
            crossing_order_frame(f,movement.accepted_fraction*static_cast<double>(i)/samples,placement,paths);
            require(static_cast<bool>(audit_topology(f.graph,placement,f.ports,paths,middle)) &&
                    middle.crossing_count==initial.crossing_count,
                    "independent frames retain the accepted prefix's crossing count");
            for(std::size_t edge=0;edge<initial.ordered_crossings.size();++edge) {
                require(middle.ordered_crossings[edge].size()==initial.ordered_crossings[edge].size(),
                        "accepted prefix retains each route's crossing sequence length");
                for(std::size_t event=0;event<initial.ordered_crossings[edge].size();++event)
                    require(middle.ordered_crossings[edge][event].other_edge==
                            initial.ordered_crossings[edge][event].other_edge &&
                            middle.ordered_crossings[edge][event].sign==initial.ordered_crossings[edge][event].sign,
                            "independent frames retain every crossing identity, sign and order");
            }
        }
    }
    {
        // With all three lines rotating, the carrier's order numerator is
        // proportional to (alpha-1/4)*(alpha-3/4)*(128-alpha^2).
        // A double root instead uses (alpha-1/2)^2: it must still stop before
        // the triple contact even though the order never reverses there.
        constexpr std::array scales={0.125,1.0,8.0,1024.0};
        constexpr std::size_t variants=8;
        for(const bool tangent:{false,true})for(const double scale:scales)
            for(std::size_t variant=0;variant<variants;++variant) {
                auto f=crossing_order_sweep(true,tangent);
                for(auto& n:f.graph.nodes) {n.width*=scale;n.height*=scale;}
                for(auto* layout:{&f.baseline,&f.desired})for(auto& n:layout->nodes) {
                    n.x*=scale;n.y*=scale;n.width*=scale;n.height*=scale;
                    if(variant&1) {std::swap(n.x,n.y);std::swap(n.width,n.height);}
                    if(variant&2)n.x=-n.x-n.width;
                }
                for(auto& route:f.routes.edges)for(auto& p:route.points) {
                    p.x*=scale;p.y*=scale;if(variant&1)std::swap(p.x,p.y);
                    if(variant&2)p.x=-p.x;
                }
                for(auto& request:f.ports)for(auto* port:{&request.source,&request.target}) {
                    if(variant&1) {
                        if(port->side==Side::East)port->side=Side::South;
                        else if(port->side==Side::West)port->side=Side::North;
                        else if(port->side==Side::South)port->side=Side::East;
                        else port->side=Side::West;
                    }
                    if(variant&2) {
                        if(port->side==Side::East)port->side=Side::West;
                        else if(port->side==Side::West)port->side=Side::East;
                    }
                }
                if(variant&4)for(std::size_t edge=0;edge<f.graph.edges.size();++edge) {
                    std::swap(f.graph.edges[edge].source,f.graph.edges[edge].target);
                    std::swap(f.ports[edge].source,f.ports[edge].target);
                    std::reverse(f.routes.edges[edge].points.begin(),f.routes.edges[edge].points.end());
                }
                TopologyGuardReport guard;TopologyReport movement;Layout moved;Routes paths;
                TopologySignature signature;
                require(static_cast<bool>(audit_topology(f.graph,f.baseline,f.ports,f.routes,signature)),
                        "transformed quartic order sweep has a valid baseline");
                require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)),
                        "quartic order event can be guarded under scale, transpose, reflection and reversal");
                const double event=tangent?0.5:0.25;
                require(guard.max_safe_fraction>event-0.001 && guard.max_safe_fraction<event,
                        "quartic guard catches both exchange and boundary-only triple contact");
                TopologyOptions single;single.samples_per_trial=1;
                // Near a double root, order gaps shrink quadratically. The
                // full auditor's geometric epsilon can require a shorter
                // step than the machine-precision analytic guard, especially
                // with extended long double. Both gates must remain satisfied.
                require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                        f.desired,moved,paths,single,&movement)) && movement.accepted_fraction>0 &&
                        movement.accepted_fraction<=guard.max_safe_fraction &&
                        movement.accepted_fraction<event,"quartic order guard composes with independent full audit");
                constexpr std::size_t samples=16;
                for(std::size_t sample=0;sample<=samples;++sample) {
                    crossing_order_frame(f,movement.accepted_fraction*static_cast<double>(sample)/samples,moved,paths);
                    TopologySignature observed;
                    const auto st=audit_topology(f.graph,moved,f.ports,paths,observed);
                    if(!st)throw std::runtime_error("quartic order scale "+std::to_string(scale)+
                        ", variant "+std::to_string(variant)+", tangent "+std::to_string(tangent)+
                        ", sample "+std::to_string(sample)+": "+st.message);
                    require(observed.crossing_count==signature.crossing_count,
                            "quartic accepted prefix retains all three proper crossings");
                    for(std::size_t edge=0;edge<signature.ordered_crossings.size();++edge) {
                        require(observed.ordered_crossings[edge].size()==signature.ordered_crossings[edge].size(),
                                "quartic prefix retains per-route crossing count");
                        for(std::size_t cross=0;cross<signature.ordered_crossings[edge].size();++cross)
                            require(observed.ordered_crossings[edge][cross].other_edge==
                                    signature.ordered_crossings[edge][cross].other_edge &&
                                    observed.ordered_crossings[edge][cross].sign==signature.ordered_crossings[edge][cross].sign,
                                    "quartic accepted prefix retains every crossing identity, sign and order");
                    }
                }
            }
    }
    {
        const auto f=crossing_order_sweep(true,true);
        constexpr double near_tangency=0.5-1.0/262144.0;
        Layout placement;Routes paths;crossing_order_frame(f,near_tangency,placement,paths);
        TopologySignature signature;
        require(audit_topology(f.graph,placement,f.ports,paths,signature).error==TopologyError::AmbiguousTopology,
                "geometric epsilon rejects an unresolved near-tangent crossing order");
        TopologyOptions zero_epsilon;zero_epsilon.orientation_epsilon=0;
        require(static_cast<bool>(audit_topology(f.graph,placement,f.ports,paths,signature,zero_epsilon)) &&
                signature.crossing_count==3,
                "near-tangent crossings are still geometrically separate before the double-root contact");
    }
    {
        auto f=crossing_order_sweep();
        // 15 node pairs, 18 segment/node pairs, 10 fixed scans, 12 pair
        // orientations and three adjacent crossing-order constraints.
        constexpr std::size_t constraints=58;
        TopologyGuardReport reference, guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,reference)) &&
                reference.segment_pair_constraints==15,"three order gaps augment the twelve pair orientations");
        TopologyGuardOptions limited;limited.max_constraints=constraints;
        limited.max_evaluations=reference.evaluations;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited)) &&
                guard.evaluations==reference.evaluations,"crossing-order phase fits exact shared budgets");
        limited.max_constraints=constraints-1;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0,
                "crossing-order constraint exhaustion is transactional");
        limited.max_constraints=constraints;limited.max_evaluations=reference.evaluations-1;
        const auto exhausted=limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited);
        require(exhausted.error==TopologyGuardError::CapacityExceeded && guard.evaluations==0 &&
                exhausted.message=="crossing-order subdivision budget exceeded",
                "Bernstein subdivision work consumes the shared evaluation budget");
        TopologyOptions scarce;scarce.max_guard_evaluations=reference.evaluations-1;
        Layout moved;moved.nodes.resize(1);Routes paths;paths.edges.resize(1);
        TopologyReport movement;movement.accepted_fraction=1;
        require(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,f.desired,moved,paths,
                scarce,&movement).error==TopologyError::CapacityExceeded && moved.nodes.empty() &&
                paths.edges.empty() && movement.accepted_fraction==0,
                "order subdivision exhaustion clears composed outputs");
        for(std::size_t node=0;node<f.graph.nodes.size();++node) {
            f.desired.nodes[node]=f.baseline.nodes[node];
            f.desired.nodes[node].x+=64;f.desired.nodes[node].y-=128;
        }
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction==1,"crossing-order guards preserve rigid translation");
        f=crossing_order_sweep(true);
        constexpr double short_motion=0.125;
        for(std::size_t node=0;node<f.graph.nodes.size();++node) {
            f.desired.nodes[node].x=f.baseline.nodes[node].x+
                short_motion*(f.desired.nodes[node].x-f.baseline.nodes[node].x);
            f.desired.nodes[node].y=f.baseline.nodes[node].y+
                short_motion*(f.desired.nodes[node].y-f.baseline.nodes[node].y);
        }
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction==1,"a positive nonconstant order polynomial admits the whole safe step");
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,f.desired,
                moved,paths,{},&movement)) && movement.accepted_fraction==1,
                "nonrigid motion without an order event passes the complete topology audit");
        f=crossing_order_sweep();f.baseline.nodes[3].y=64;f.routes.edges[1].points.back().y=64;
        f.desired=f.baseline;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard).error==
                TopologyGuardError::InfeasibleBaseline && guard.evaluations==0,
                "coincident baseline crossings fail without a partial report");
    }
    {
        const auto f=segment_sweep();
        TopologySignature signature;
        require(static_cast<bool>(audit_topology(f.graph,f.baseline,f.ports,f.routes,signature)),
                "segment-interior sweep has a valid baseline");
        Layout placement;Routes paths;rigid_frame(f,0.45,placement,paths);
        require(paths.edges[0].points[0].x<placement.nodes[2].x &&
                paths.edges[0].points[1].x>placement.nodes[2].x+placement.nodes[2].width,
                "both endpoints stay outside while the segment spans the obstacle");
        require(audit_topology(f.graph,placement,f.ports,paths,signature).error==TopologyError::InvalidGeometry,
                "intermediate segment interior genuinely penetrates the obstacle");
        TopologyOptions single;single.samples_per_trial=1;single.analytic_guards=false;TopologyReport movement;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,placement,paths,single,&movement)) && movement.accepted_fraction==1,
                "endpoint-only sampling misses a hidden segment-interior sweep");
        // The line y=-100*alpha first reaches the box's y=-40 face at 0.4.
        constexpr double first_contact=0.4;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.straight_constraints==0 && guard.max_safe_fraction>0.39 &&
                guard.max_safe_fraction<first_contact,
                "continuous segment-interior guard caps a collision missed by the initial scan");
        single.analytic_guards=true;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,placement,paths,single,&movement)) && movement.accepted_fraction>0 &&
                movement.accepted_fraction<first_contact,"integrated single-sample movement respects segment events");
        require(static_cast<bool>(audit_topology(f.graph,placement,f.ports,paths,signature)),
                "accepted segment-interior prefix independently passes audit");
    }
    {
        constexpr std::array scales={0.125,1.0,8.0,1024.0};
        constexpr std::size_t axes=2, variants=4, samples=16;
        for(std::size_t axis=0;axis<axes;++axis)for(const double scale:scales)
            for(std::size_t variant=0;variant<variants;++variant) {
                const bool vertical=axis!=0;
                auto f=segment_sweep(vertical,scale);
                if(variant%2) {
                    for(auto* layout:{&f.baseline,&f.desired})for(auto& n:layout->nodes) {
                        if(vertical)n.y=-n.y-n.height;
                        else n.x=-n.x-n.width;
                    }
                    for(auto& p:f.routes.edges[0].points) {
                        if(vertical)p.y=-p.y;
                        else p.x=-p.x;
                    }
                    f.ports[0].source.side=vertical?Side::North:Side::West;
                    f.ports[0].target.side=vertical?Side::South:Side::East;
                }
                if(variant>=variants/2) {
                    std::swap(f.graph.edges[0].source,f.graph.edges[0].target);
                    std::swap(f.ports[0].source,f.ports[0].target);
                    std::reverse(f.routes.edges[0].points.begin(),f.routes.edges[0].points.end());
                }
                TopologyGuardReport guard;
                require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)),
                        "transformed full segment sweep can be guarded");
                near(guard.max_safe_fraction,0.4*(1.0-TopologyGuardOptions{}.event_backoff),1e-12,
                     "segment-interior event survives transpose, scale, reflection and reversal");
                TopologyOptions single;single.samples_per_trial=1;
                Layout placement;Routes paths;TopologyReport movement;TopologySignature signature;
                require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                        f.desired,placement,paths,single,&movement)) && movement.accepted_fraction>0 &&
                        movement.accepted_fraction<=guard.max_safe_fraction,
                        "transformed segment sweep composes with full geometry checks");
                for(std::size_t sample=0;sample<=samples;++sample) {
                    rigid_frame(f,movement.accepted_fraction*static_cast<double>(sample)/samples,placement,paths);
                    const auto status=audit_topology(f.graph,placement,f.ports,paths,signature);
                    if(!status)throw std::runtime_error("segment axis "+std::to_string(axis)+
                        ", variant "+std::to_string(variant)+", sample "+std::to_string(sample)+": "+status.message);
                }
                f.desired.nodes[2].x+=f.desired.nodes[0].x-f.baseline.nodes[0].x;
                f.desired.nodes[2].y+=f.desired.nodes[0].y-f.baseline.nodes[0].y;
                require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                        guard.max_safe_fraction==1,"full segment checks retain rigid graph translation");
            }
    }
    {
        const auto f=segment_sweep();
        // Three node pairs and three segment/node pairs. The unrelated pair
        // adds 12 polynomial evaluations and four open interval tests.
        constexpr std::size_t constraints=6, evaluations=23;
        TopologyGuardOptions limited;limited.max_constraints=constraints;limited.max_evaluations=evaluations;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited)) &&
                guard.evaluations==evaluations,"segment events fit exact shared budgets");
        limited.max_constraints=constraints-1;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0,
                "segment constraint exhaustion clears the report");
        limited.max_constraints=constraints;limited.max_evaluations=evaluations-1;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0,
                "segment interval exhaustion clears the report");
        limited.max_evaluations=10;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0,
                "segment root extraction also obeys the evaluation budget");
        TopologyOptions scarce;scarce.max_guard_evaluations=evaluations-1;
        Layout moved;moved.nodes.resize(1);Routes paths;paths.edges.resize(1);
        TopologyReport movement;movement.accepted_fraction=1;
        require(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,f.desired,
                moved,paths,scarce,&movement).error==TopologyError::CapacityExceeded &&
                moved.nodes.empty() && paths.edges.empty() && movement.accepted_fraction==0,
                "segment event failure clears all composed outputs");
    }
    {
        MovementFixture f;
        f.graph.nodes={{"source",8,8},{"target",8,8},{"obstacle",8,8}};
        f.graph.edges={{0,1}};
        f.baseline.nodes={{-4,-8,8,8},{-4,256,8,8},{-48,-128,8,8}};
        f.routes.edges={{{{0,0},{0,256}}}};
        f.ports={{{0,Side::South,0.5},{1,Side::North,0.5}}};
        f.desired=f.baseline;f.desired.nodes[1].x+=256;
        f.desired.nodes[2].x+=128;f.desired.nodes[2].y+=256;
        // Upper-left corner orientation divided by 256^2 is
        // alpha^2 - (31/32)*alpha + 3/16. The first root is on the
        // segment extension below its source; entry occurs at the second.
        const double first_contact=(31.0+std::sqrt(193.0))/64.0;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)),
                "rotating segment and translating node produce quadratic events");
        near(guard.max_safe_fraction,first_contact*(1.0-TopologyGuardOptions{}.event_backoff),1e-12,
             "quadratic segment event ignores extension roots and catches the first actual penetration");
        TopologyOptions single;single.samples_per_trial=1;single.analytic_guards=false;
        Layout placement;Routes paths;TopologyReport movement;TopologySignature signature;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,placement,paths,single,&movement)) && movement.accepted_fraction==1,
                "quadratic sweep has valid endpoints despite an intermediate penetration");
        single.analytic_guards=true;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,placement,paths,single,&movement)) && movement.accepted_fraction>0 &&
                movement.accepted_fraction<first_contact,"quadratic event caps single-sample continuation");
        require(static_cast<bool>(audit_topology(f.graph,placement,f.ports,paths,signature)),
                "quadratic accepted prefix independently passes audit");
        std::swap(f.graph.edges[0].source,f.graph.edges[0].target);
        std::swap(f.ports[0].source,f.ports[0].target);
        std::reverse(f.routes.edges[0].points.begin(),f.routes.edges[0].points.end());
        TopologyGuardReport reversed;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,reversed)),
                "reversed quadratic sweep can be guarded");
        near(reversed.max_safe_fraction,guard.max_safe_fraction,1e-12,
             "quadratic segment event is independent of edge direction");
    }
    {
        MovementFixture f;
        f.graph.nodes={{"source",8,8},{"target",8,8},{"obstacle",8,8}};
        f.graph.edges={{0,1}};
        f.baseline.nodes={{-4,-8,8,8},{-4,256,8,8},{-72,-64,8,8}};
        f.routes.edges={{{{0,0},{0,256}}}};
        f.ports={{{0,Side::South,0.5},{1,Side::North,0.5}}};
        f.desired=f.baseline;f.desired.nodes[1].x+=256;
        f.desired.nodes[2].x+=192;f.desired.nodes[2].y+=256;
        // The lower-right corner has signed area 256^2*(alpha-1/2)^2.
        // Other corners remain on the same side: the double root is a
        // boundary-only tangency, not an interval of interior penetration.
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction==1,"quadratic double-root tangency does not shorten movement");
        Layout placement=f.baseline;Routes paths=f.routes;TopologySignature signature;
        placement.nodes[1].x+=128;placement.nodes[2].x+=96;placement.nodes[2].y+=128;
        paths.edges[0].points.back().x+=128;
        require(static_cast<bool>(audit_topology(f.graph,placement,f.ports,paths,signature)),
                "independent audit permits the exact quadratic corner tangency");
        TopologyReport movement;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,placement,paths,{},&movement)) && movement.accepted_fraction==1,
                "double-root tangency composes with sampled full topology checks");
        f=segment_sweep();f.baseline.nodes[2]={50,-5,10,10};f.desired=f.baseline;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard).error==
                TopologyGuardError::InfeasibleBaseline && guard.evaluations==0,
                "baseline segment-interior penetration fails transactionally");
    }
    {
        const auto f=waypoint_sweep();
        TopologySignature signature;
        require(static_cast<bool>(audit_topology(f.graph,f.baseline,f.ports,f.routes,signature)),
                "rigid U route starts clear of the obstacle");
        Layout placement;Routes paths;
        rigid_frame(f,0.325,placement,paths);
        require(audit_topology(f.graph,placement,f.ports,paths,signature).error==TopologyError::InvalidGeometry,
                "intermediate U waypoint enters the obstacle interior");
        TopologyOptions single;single.analytic_guards=false;single.samples_per_trial=1;
        TopologyReport movement;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,placement,paths,single,&movement)) && movement.accepted_fraction==1,
                "endpoint-only sampling misses the hidden waypoint collision");
        // The right waypoint starts at x=20; the obstacle begins at x=80.
        // With 200-unit translation the first boundary contact is alpha=0.3.
        constexpr double first_contact=0.3;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction>0.29 && guard.max_safe_fraction<first_contact,
                "continuous internal-waypoint collision caps a missed scan-endpoint event");
        single.analytic_guards=true;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,placement,paths,single,&movement)) && movement.accepted_fraction>0 &&
                movement.accepted_fraction<first_contact,
                "single-sample continuation respects internal-waypoint collision");
        require(static_cast<bool>(audit_topology(f.graph,placement,f.ports,paths,signature)),
                "guarded U route independently passes audit");
    }
    {
        constexpr std::array scales={0.125,1.0,8.0,1024.0};
        constexpr std::size_t axes=2, variants=4, samples=16;
        for(std::size_t axis=0;axis<axes;++axis)for(const double scale:scales)
            for(std::size_t variant=0;variant<variants;++variant) {
                const bool vertical=axis!=0;
                auto f=waypoint_sweep(vertical,scale);
                if(variant%2) {
                    for(auto* layout:{&f.baseline,&f.desired})for(auto& n:layout->nodes) {
                        if(vertical)n.y=-n.y-n.height;
                        else n.x=-n.x-n.width;
                    }
                    for(auto& p:f.routes.edges[0].points) {
                        if(vertical)p.y=-p.y;
                        else p.x=-p.x;
                    }
                }
                if(variant>=variants/2) {
                    std::swap(f.graph.edges[0].source,f.graph.edges[0].target);
                    std::swap(f.ports[0].source,f.ports[0].target);
                    std::reverse(f.routes.edges[0].points.begin(),f.routes.edges[0].points.end());
                }
                TopologyGuardReport guard;
                require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)),
                        "transformed waypoint sweep can be guarded");
                near(guard.max_safe_fraction,0.3*(1.0-TopologyGuardOptions{}.event_backoff),1e-12,
                     "waypoint event is invariant under axis, scale, reflection and edge reversal");
                require(guard.straight_constraints==0,
                        "waypoint event is detected when all fixed scans miss their endpoints");
                Layout placement;Routes paths;TopologyReport movement;TopologySignature signature;
                TopologyOptions single;single.samples_per_trial=1;
                require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                        f.desired,placement,paths,single,&movement)) && movement.accepted_fraction>0 &&
                        movement.accepted_fraction<=guard.max_safe_fraction,
                        "transformed waypoint sweep composes with the full audit");
                for(std::size_t sample=0;sample<=samples;++sample) {
                    rigid_frame(f,movement.accepted_fraction*static_cast<double>(sample)/samples,placement,paths);
                    const auto status=audit_topology(f.graph,placement,f.ports,paths,signature);
                    if(!status)throw std::runtime_error("waypoint axis "+std::to_string(axis)+
                        ", variant "+std::to_string(variant)+", sample "+std::to_string(sample)+": "+status.message);
                }
                // Move the obstacle with the entire route: all relative
                // positions remain fixed, so the new checks must admit it.
                f.desired.nodes[2].x+=f.desired.nodes[0].x-f.baseline.nodes[0].x;
                f.desired.nodes[2].y+=f.desired.nodes[0].y-f.baseline.nodes[0].y;
                require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                        guard.max_safe_fraction==1,"rigid graph movement has no artificial waypoint event");
            }
    }
    {
        const auto f=waypoint_sweep();
        // Existing 15 constraints plus nine full segment/node pairs. Root
        // classification work depends on the number of distinct event times.
        constexpr std::size_t constraints=24;
        TopologyGuardReport reference;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,reference)),
                "reference waypoint sweep provides the full event work count");
        const std::size_t evaluations=reference.evaluations;
        TopologyGuardOptions limited;limited.max_constraints=constraints;limited.max_evaluations=evaluations;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited)) &&
                guard.evaluations==evaluations,"waypoint events fit exact shared budgets");
        limited.max_constraints=constraints-1;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0,
                "waypoint constraint budget exhaustion clears the report");
        limited.max_constraints=constraints;limited.max_evaluations=evaluations-1;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0,
                "waypoint evaluation budget exhaustion clears the report");
        TopologyOptions scarce;scarce.max_guard_evaluations=evaluations-1;
        Layout moved;moved.nodes.resize(1);Routes paths;paths.edges.resize(1);
        TopologyReport movement;movement.accepted_fraction=1;
        require(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,f.desired,
                moved,paths,scarce,&movement).error==TopologyError::CapacityExceeded &&
                moved.nodes.empty() && paths.edges.empty() && movement.accepted_fraction==0,
                "waypoint budget failure clears all composed outputs");
    }
    {
        auto f=waypoint_sweep();f.desired=f.baseline;f.desired.nodes[2].x-=200;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)),
                "a moving rectangle can sweep over stationary internal waypoints");
        near(guard.max_safe_fraction,0.3*(1.0-TopologyGuardOptions{}.event_backoff),1e-12,
             "relative motion determines the waypoint event");
        Layout moved;Routes paths;TopologyReport movement;TopologySignature signature;
        TopologyOptions single;single.samples_per_trial=1;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,moved,paths,single,&movement)) && movement.accepted_fraction<0.3,
                "moving-obstacle event caps integrated movement");
        require(static_cast<bool>(audit_topology(f.graph,moved,f.ports,paths,signature)),
                "moving-obstacle prefix independently passes audit");
        f=waypoint_sweep();f.desired=f.baseline;
        for(std::size_t i=0;i<2;++i) {f.desired.nodes[i].x+=200;f.desired.nodes[i].y+=200;}
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction==1,"diagonal waypoint bypass has disjoint axis-overlap intervals");
        f=waypoint_sweep();f.desired=f.baseline;
        for(std::size_t i=0;i<2;++i) {f.desired.nodes[i].x+=192;f.desired.nodes[i].y+=16;}
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction==1,"waypoint corner tangency has no open interior-overlap interval");
        f=waypoint_sweep();f.desired=f.baseline;
        for(std::size_t i=0;i<2;++i)f.desired.nodes[i].x+=60;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction==1,"final-frame waypoint boundary contact remains admissible");
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,moved,paths,single,&movement)) && movement.accepted_fraction==1,
                "full audit agrees with final-frame waypoint boundary policy");
    }
    {
        auto f=waypoint_sweep();f.baseline.nodes[2].x=20;
        f.desired=f.baseline;f.desired.nodes[0].x-=200;f.desired.nodes[1].x-=200;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction==1,"a boundary waypoint moving outward is admissible");
        f.desired=f.baseline;f.desired.nodes[0].y+=200;f.desired.nodes[1].y+=200;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction==1,"boundary-only waypoint sliding is admissible");
        f.desired=f.baseline;f.desired.nodes[0].x+=200;f.desired.nodes[1].x+=200;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction==0,"a boundary waypoint moving inward has no positive safe prefix");
        Layout moved;moved.nodes.resize(1);Routes paths;paths.edges.resize(1);
        TopologyReport movement;movement.accepted_fraction=1;
        require(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,f.desired,
                moved,paths,{},&movement).error==TopologyError::NoAdmissibleStep &&
                moved.nodes.empty() && paths.edges.empty() && movement.accepted_fraction==0,
                "zero waypoint prefix fails atomically through the movement API");
        f=waypoint_sweep();f.routes.edges[0].points[1]={0,105};
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard).error==
                TopologyGuardError::InfeasibleBaseline && guard.evaluations==0,
                "baseline waypoint inside its source node is rejected transactionally");
    }
    {
        MovementFixture f;
        f.graph.nodes={{"source",10,10},{"target",10,10}};f.graph.edges={{0,1}};
        f.baseline.nodes={{0,0,10,10},{-210,0,10,10}};
        f.routes.edges={{{{5,0},{5,-100},{-100,-100},{-100,5},{-200,5}}}};
        f.ports={{{0,Side::North,0.5},{1,Side::East,0.5}}};
        f.desired=f.baseline;f.desired.nodes[1].x+=400;
        // Arclength 310/410 gives the returning waypoint speed 400*310/410.
        // Its 100-unit gap from the source rectangle closes at 41/124.
        constexpr double first_contact=41.0/124.0;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)),
                "returning waypoint near an incident node can be guarded");
        near(guard.max_safe_fraction,first_contact*(1.0-TopologyGuardOptions{}.event_backoff),1e-12,
             "incident source node participates in continuous waypoint collision checks");
        Layout moved;Routes paths;TopologyReport movement;TopologySignature signature;
        TopologyOptions single;single.samples_per_trial=1;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,moved,paths,single,&movement)) && movement.accepted_fraction>0 &&
                movement.accepted_fraction<first_contact,"incident-node event composes with movement");
        require(static_cast<bool>(audit_topology(f.graph,moved,f.ports,paths,signature)),
                "accepted incident-node prefix independently passes audit");
        std::swap(f.graph.edges[0].source,f.graph.edges[0].target);
        std::swap(f.ports[0].source,f.ports[0].target);
        std::reverse(f.routes.edges[0].points.begin(),f.routes.edges[0].points.end());
        TopologyGuardReport reversed;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,reversed)),
                "reversed incident-node route can be guarded");
        near(reversed.max_safe_fraction,guard.max_safe_fraction,1e-12,
             "the same incident event is detected when the rectangle is the target node");
        require(reversed.evaluations==guard.evaluations,"incident-edge reversal retains shared work counts");
    }
    {
        const auto f=collinear_swap();
        TopologySignature signature;
        require(static_cast<bool>(audit_topology(f.graph,f.baseline,f.ports,f.routes,signature)),
                "collinear disjoint baseline is valid");
        Layout placement;Routes paths;
        rigid_frame(f,0.25,placement,paths);
        require(audit_topology(f.graph,placement,f.ports,paths,signature).error==
                TopologyError::AmbiguousTopology,"collinear segments overlap between valid endpoints");
        TopologyOptions single;single.analytic_guards=false;single.samples_per_trial=1;
        TopologyReport movement;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,placement,paths,single,&movement)) && movement.accepted_fraction==1,
                "endpoint-only sampling misses collinear overlap");
        // The 80-unit gap closes at relative speed 400: first contact = 1/5.
        constexpr double first_contact=0.2;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.segment_pair_constraints>0 && guard.max_safe_fraction>0 &&
                guard.max_safe_fraction<first_contact,"collinear motion is capped before first contact");
        single.analytic_guards=true;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,placement,paths,single,&movement)) && movement.accepted_fraction>0 &&
                movement.accepted_fraction<first_contact,"single-sample motion respects collinear event");
        require(static_cast<bool>(audit_topology(f.graph,placement,f.ports,paths,signature)),
                "collinear prefix independently passes audit");
    }
    {
        constexpr std::array axes={SwapAxis::Horizontal,SwapAxis::Vertical,SwapAxis::Diagonal};
        constexpr std::array scales={0.125,1.0,8.0,1024.0};
        constexpr std::size_t variants=4, samples=16;
        for(const auto axis:axes)for(const double scale:scales)
            for(std::size_t variant=0;variant<variants;++variant) {
                auto f=collinear_swap(axis,scale);
                if(variant%2)for(std::size_t i=0;i<f.graph.edges.size();++i) {
                    std::swap(f.graph.edges[i].source,f.graph.edges[i].target);
                    std::swap(f.ports[i].source,f.ports[i].target);
                    std::reverse(f.routes.edges[i].points.begin(),f.routes.edges[i].points.end());
                }
                if(variant>=variants/2) {
                    std::swap(f.graph.edges[0],f.graph.edges[1]);
                    std::swap(f.ports[0],f.ports[1]);
                    std::swap(f.routes.edges[0],f.routes.edges[1]);
                }
                TopologyGuardReport guard;
                require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                        guard.segment_pair_constraints==1,"one projected-gap guard covers collinear pair");
                near(guard.max_safe_fraction,0.2*(1.0-TopologyGuardOptions{}.event_backoff),1e-12,
                     "collinear event is invariant under axis, scale, edge reversal and permutation");
                Layout placement;Routes paths;TopologySignature signature;
                rigid_frame(f,1,placement,paths);
                require(static_cast<bool>(audit_topology(f.graph,placement,f.ports,paths,signature)),
                        "transformed collinear swap has valid final geometry");
                TopologyOptions single;single.samples_per_trial=1;
                TopologyReport movement;
                require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                        f.desired,placement,paths,single,&movement)) && movement.accepted_fraction>0 &&
                        movement.accepted_fraction<=guard.max_safe_fraction,
                        "transformed collinear event caps integrated movement");
                require(static_cast<bool>(audit_topology(f.graph,placement,f.ports,paths,signature)),
                        "actual transformed collinear output independently passes audit");
                // Binary fractions retain exact corner geometry in these
                // binary-scaled fixtures. Arbitrary fractions can round node
                // coordinates differently across a shared diagonal boundary.
                // The largest binary power below acceptance covers at least
                // half of its prefix, even if the full audit shortens the step.
                const double prefix=std::ldexp(1.0,std::ilogb(movement.accepted_fraction));
                for(std::size_t sample=0;sample<=samples;++sample) {
                    rigid_frame(f,prefix*static_cast<double>(sample)/samples,placement,paths);
                    const auto audited=audit_topology(f.graph,placement,f.ports,paths,signature);
                    if(!audited)throw std::runtime_error("collinear axis "+std::to_string(static_cast<int>(axis))+
                        ", scale "+std::to_string(scale)+", variant "+std::to_string(variant)+
                        ", sample "+std::to_string(sample)+": "+audited.message);
                }
                f.desired=f.baseline;
                for(auto& n:f.desired.nodes) {n.x+=37*scale;n.y-=23*scale;}
                require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                        guard.max_safe_fraction==1,"rigid collinear translation remains fully admissible");
            }
    }
    {
        auto f=collinear_swap();
        f.desired=f.baseline;
        for(std::size_t i=0;i<f.desired.nodes.size();++i)f.desired.nodes[i].x+=(i<2 ? -200 : 200);
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction==1,"separating collinear motion has no artificial event");
        f=collinear_swap();
        // Each segment tilts as its inner endpoint approaches the other route.
        // The projected guard intentionally stops when projection separation
        // is lost, even though transverse separation could permit a longer move.
        f.desired=f.baseline;f.desired.nodes[1].x+=100;f.desired.nodes[1].y-=50;
        f.desired.nodes[2].x-=100;f.desired.nodes[2].y+=50;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction>0.39 && guard.max_safe_fraction<0.4,
                "changing endpoint motion preserves a conservative projected-separation prefix");
        f=collinear_swap();f.baseline.nodes[1].y=0;f.ports[0].target.offset=0;
        f.desired=f.baseline;f.desired.nodes[0].x+=200;f.desired.nodes[1].x-=20;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)),
                "independent pair can inspect changing endpoint order");
        near(guard.max_safe_fraction,0.5*(1.0-TopologyGuardOptions{}.event_backoff),1e-12,
             "all four gaps detect the initially farther endpoint approaching after order reversal");
        f=collinear_swap();f.desired=f.baseline;
        for(std::size_t i=0;i<2;++i)f.desired.nodes[i].x+=80;
        TopologyGuardOptions exact;exact.event_backoff=0;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,exact)) &&
                guard.max_safe_fraction==std::nextafter(1.0,0.0),
                "a final-frame collinear contact is backed off even with zero configured margin");
    }
    {
        const auto f=collinear_swap();
        // Six node pairs, eight swept-separated segment/node pairs, four
        // scans, four zero orientations, one projected interval check.
        constexpr std::size_t constraints=19, evaluations=23;
        TopologyGuardOptions limited;limited.max_constraints=constraints;limited.max_evaluations=evaluations;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited)) &&
                guard.evaluations==evaluations,"collinear checks fit exact shared budgets");
        limited.max_constraints=constraints-1;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0 && guard.segment_pair_constraints==0,
                "collinear constraint exhaustion publishes no partial report");
        limited.max_constraints=constraints;limited.max_evaluations=evaluations-1;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0,
                "collinear evaluation exhaustion is transactional");
        TopologyOptions scarce;scarce.max_guard_constraints=constraints-1;
        Layout moved;moved.nodes.resize(1);Routes paths;paths.edges.resize(1);
        TopologyReport movement;movement.accepted_fraction=1;
        require(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,f.desired,
                moved,paths,scarce,&movement).error==TopologyError::CapacityExceeded &&
                moved.nodes.empty() && paths.edges.empty() && movement.accepted_fraction==0,
                "collinear budget failure clears composed movement outputs");
    }
    {
        MovementFixture f;
        f.graph.nodes={{"start",10,10},{"end",10,10}};f.graph.edges={{0,1}};
        f.baseline.nodes={{-10,-5,10,10},{120,-5,10,10}};
        f.routes.edges={{{{0,0},{20,0},{20,40},{100,40},{100,0},{120,0}}}};
        f.ports={{{0,Side::East,0.5},{1,Side::West,0.5}}};
        f.desired=f.baseline;f.desired.nodes[0].x+=200;f.desired.nodes[1].x-=200;
        // Total arclength 200 gives inner endpoints weights 0.1 and 0.9.
        // Their 80-unit projected gap closes at speed 320, hence alpha=1/4.
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction>0.24 && guard.max_safe_fraction<0.25,
                "same-route collinear nonadjacent pair is bounded");
        Layout moved;Routes paths;TopologyReport movement;TopologySignature signature;
        TopologyOptions single;single.samples_per_trial=1;
        require(static_cast<bool>(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,
                f.desired,moved,paths,single,&movement)) && movement.accepted_fraction>0 &&
                movement.accepted_fraction<0.25,"same-route projected guard composes with full audit");
        require(static_cast<bool>(audit_topology(f.graph,moved,f.ports,paths,signature)),
                "same-route collinear prefix is geometrically valid");
    }
    {
        MovementFixture f;
        f.graph.nodes={{"shared",10,10},{"northwest",10,10},{"southeast",10,10}};
        f.graph.edges={{0,1},{0,2}};
        f.baseline.nodes={{0,0,10,10},{-30,-40,10,10},{40,30,10,10}};
        f.routes.edges={{{{10,0},{-20,-30}}},{{{10,0},{40,30}}}};
        f.ports={{{0,Side::North,1},{1,Side::East,1}},
                 {{0,Side::East,0},{2,Side::West,0}}};
        f.desired=f.baseline;for(auto& n:f.desired.nodes) {n.x+=37;n.y-=23;}
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard)) &&
                guard.max_safe_fraction==1 && guard.segment_pair_constraints==0,
                "baseline shared anchor creates no strict projected-gap guard");
        Layout moved;Routes paths;TopologyReport movement;
        require(move_topology_preserving(f.graph,f.baseline,f.ports,f.routes,f.desired,
                moved,paths,{},&movement).error==TopologyError::AmbiguousTopology,
                "full audit retains its rejection of baseline collinear endpoint contacts");
    }
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
        // One node pair, eight waypoint/node checks, ten segment/node pairs,
        // four bends and six independent segment pairs share one budget.
        constexpr std::size_t constraints=47;
        TopologyGuardReport reference;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,reference)),
                "reference spiral provides the full segment event work count");
        const std::size_t evaluations=reference.evaluations;
        TopologyGuardOptions limited;limited.max_constraints=constraints;limited.max_evaluations=evaluations;
        TopologyGuardReport guard;
        require(static_cast<bool>(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited)) &&
                guard.evaluations==evaluations,"same-route event work fits exact shared budgets");
        limited.max_constraints=constraints-1;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0,
                "same-route constraint exhaustion publishes no partial guard report");
        limited.max_constraints=constraints;limited.max_evaluations=evaluations-1;
        require(limit_topology_movement(f.graph,f.baseline,f.desired,f.routes,guard,limited).error==
                TopologyGuardError::CapacityExceeded && guard.evaluations==0,
                "same-route evaluation exhaustion is transactional");
        TopologyOptions scarce;scarce.max_guard_constraints=constraints-1;
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
        {
            // n*n - (n-1)*(n+1)=1. Moving the middle point upward
            // changes that signed area to 1-(2n+1)*alpha.
            constexpr std::array magnitudes={0x1p27,0x1p38},scales={0.125,1.0};
            constexpr std::size_t variants=8;
            constexpr double relative_slack=128*std::numeric_limits<double>::epsilon();
            for(const double n:magnitudes)for(const double scale:scales)
            for(std::size_t variant=0;variant<variants;++variant)for(const bool zero_margin:{false,true}) {
                const auto transformed=[&](TopologyBendConstraint bend) {
                    for(auto* point:{&bend.first,&bend.bend,&bend.last}) {
                        for(auto* axis:{&point->x,&point->y}) {axis->initial*=scale;axis->desired*=scale;}
                        if(variant&1)std::swap(point->x,point->y);
                        if(variant&2) {point->x.initial=-point->x.initial;point->x.desired=-point->x.desired;}
                    }
                    if(variant&4)std::swap(bend.first,bend.last);
                    return bend;
                };
                TopologyBendConstraint small={{{0,0},{0,0}},
                    {{n,n},{n-1,n}},{{2*n+1,2*n+1},{2*n-1,2*n-1}}};
                TopologyGuardOptions options;options.coordinate_limit=1e12;
                if(zero_margin) {options.min_bend_area_ratio=0;options.event_backoff=0;}
                const double event=(1-options.min_bend_area_ratio)/(2*n+1);
                double fraction=1;
                require(static_cast<bool>(limit_topology_bend(transformed(small),fraction,options)),
                        "cancellation-scale bend deformation is analyzable");
                require(fraction>0 && fraction<event &&
                        fraction>=event*(1-2*options.event_backoff-relative_slack),
                        "a nonzero cancelled baseline determinant retains an accurate early bend event");
                small.bend.y.desired=n-2;
                require(static_cast<bool>(limit_topology_bend(transformed(small),fraction,options)) && fraction==1,
                        "cancellation-scale widening retains the full admissible motion");
                small.bend.y.initial=n;small.bend.y.desired=n+1;
                small.last.x={2*n,2*n};small.last.y={2*n,2*n};
                require(static_cast<bool>(limit_topology_bend(transformed(small),fraction,options)) && fraction==1,
                        "an exactly collinear baseline still imposes no bend sign guard");
            }
        }
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
        limited.max_constraints=7;
        require(static_cast<bool>(limit_topology_movement(graph,baseline,desired,routes,result,limited)),
                "three node pairs, three full segment/node pairs and one scan fit the exact count");
        limited.max_constraints=6;
        require(limit_topology_movement(graph,baseline,desired,routes,result,limited).error==
                TopologyGuardError::CapacityExceeded,"node pairs share the constraint budget");
        limited.max_constraints=7;
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
