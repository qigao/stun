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
constexpr const char* route_contact_message="topology surgery would pass through a route contact";
constexpr std::array cancellation_magnitudes={0x1p27,0x1p38};
constexpr std::size_t cancellation_variants=8;

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
Fixture hidden_merge() {
    auto f=horizontal();
    f.graph.nodes[2].width=2e-6;f.graph.nodes[2].height=2e-6;
    f.layout.nodes[2]={55-1e-6,5+4e-5,2e-6,2e-6};
    f.paths.edges[0].points={{10,5},{55,5+1e-4},{100,5}};
    return f;
}
Fixture cancellation_merge(double n) {
    Fixture f;
    f.graph.nodes={{"A",10,10},{"B",10,10}};f.graph.edges={{0,1}};
    f.layout.nodes={{-n-10,-n-5,10,10},{n+1,n-6,10,10}};
    f.ports={{{0,Side::East,0.5},{1,Side::West,0.5}}};
    f.paths.edges={{{{-n,-n},{0,-1},{n+1,n-1}}}};
    return f;
}
Fixture merge_crossing(double offset=5e-5) {
    auto f=parallel();
    f.paths.edges[0].points={{10,5},{55,5+1e-4},{100,5}};
    f.layout.nodes[2]={15,-40+offset,10,10};f.layout.nodes[3]={85,40+offset,10,10};
    f.ports[1]={{2,Side::South,0.5},{3,Side::North,0.5}};
    f.paths.edges[1].points={{20,f.layout.nodes[2].y+10},{90,f.layout.nodes[3].y}};
    return f;
}
Fixture terminal_merge_contact(double offset) {
    Fixture f;
    f.graph.nodes={{"A",10,10},{"B",10,10},{"C",10,10},{"D",10,10}};
    f.graph.edges={{0,1},{2,3}};
    f.layout.nodes={{-42,-5,10,10},{32,-5,10,10},{-5,-110,10,10},{35,40,10,10}};
    f.ports={{{0,Side::East,0.5},{1,Side::West,0.5}},
             {{2,Side::South,0.5},{3,Side::North,0.5}}};
    f.paths.edges={{{{-32,0},{0,0x1p-20},{32,0}}},
                   {{{0,-100},{-2*offset,-offset},{40,40}}}};
    return f;
}
Fixture split_extension() {
    Fixture f;
    f.graph.nodes={{"A",10,10},{"B",10,10},{"C",10,10},{"D",10,10}};
    f.graph.edges={{0,1},{2,3}};
    f.layout.nodes={{-42,-5,10,10},{32,-5,10,10},{-4,30,10,10},{-4,100,10,10}};
    f.ports={{{0,Side::East,0.5},{1,Side::West,0.5}},
             {{2,Side::South,0.5},{3,Side::North,0.5}}};
    f.paths.edges={{{{-32,0},{-16,0},{16,0},{32,0}}},{{{1,40},{1,100}}}};
    return f;
}
Fixture hidden_route_contact() {
    auto f=parallel();
    f.layout.nodes[2]={51,-1010,10,10};
    f.layout.nodes[3]={51,20,10,10};
    f.ports[1]={{2,Side::South,0.5},{3,Side::North,0.5}};
    f.paths.edges[1].points={{56,-1000},{56,20}};
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
void transform(Fixture& f, TopologyEdit& edit, double scale, std::size_t variant) {
    const auto point=[&](Point& p) {
        p.x*=scale;p.y*=scale;if(variant&1)std::swap(p.x,p.y);
        if(variant&2)p.x=-p.x;
        if(variant&8) {p.x+=1e7;p.y-=1e7;}
    };
    for(auto& node:f.graph.nodes) {
        node.width*=scale;node.height*=scale;
        if(variant&1)std::swap(node.width,node.height);
    }
    for(auto& node:f.layout.nodes) {
        Point origin{node.x,node.y};point(origin);
        node.width*=scale;node.height*=scale;
        if(variant&1)std::swap(node.width,node.height);
        node.x=origin.x-(variant&2 ? node.width : 0);node.y=origin.y;
    }
    for(auto& route:f.paths.edges)for(auto& p:route.points)point(p);
    edit.displacement.x*=scale;edit.displacement.y*=scale;
    if(variant&1)std::swap(edit.displacement.x,edit.displacement.y);
    if(variant&2)edit.displacement.x=-edit.displacement.x;
    for(auto& request:f.ports)for(auto* port:{&request.source,&request.target}) {
        if(variant&1) {
            switch(port->side) {
            case Side::East: port->side=Side::South;break;
            case Side::West: port->side=Side::North;break;
            case Side::North: port->side=Side::West;break;
            case Side::South: port->side=Side::East;break;
            default: throw std::runtime_error("transform needs an explicit port side");
            }
        }
        if(variant&2) {
            if(port->side==Side::East)port->side=Side::West;
            else if(port->side==Side::West)port->side=Side::East;
            else port->offset=1-port->offset;
        }
    }
    if(variant&4) {
        const auto removed=edit.kind==TopologyEditKind::BendToStraight ? 1 : 2;
        edit.point_index=f.paths.edges[edit.edge_index].points.size()-removed-edit.point_index;
        edit.segment_fraction=1-edit.segment_fraction;
        for(std::size_t edge=0;edge<f.paths.edges.size();++edge) {
            std::swap(f.graph.edges[edge].source,f.graph.edges[edge].target);
            std::swap(f.ports[edge].source,f.ports[edge].target);
            std::reverse(f.paths.edges[edge].points.begin(),f.paths.edges[edge].points.end());
        }
    }
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
            constexpr std::array displacements={0x1p30,0x1p38};
            constexpr std::array scales={0.125,1.0};
            // Four nodes and four original segments: 2*4 + 2*(4-1).
            constexpr std::size_t variants=16,guard_constraints=14;
            for(const double displacement:displacements)for(const double scale:scales)
            for(std::size_t variant=0;variant<variants;++variant) {
                auto f=split_extension();
                auto contact=f;contact.layout.nodes[2].y=-110;contact.paths.edges[1].points.front().y=-100;
                auto intermediate=f;
                intermediate.paths.edges[0].points.insert(intermediate.paths.edges[0].points.begin()+2,{1,-1});
                auto contact_frame=contact;
                contact_frame.paths.edges[0].points.insert(contact_frame.paths.edges[0].points.begin()+2,{1,-1});
                auto edit=split(displacement,-displacement);edit.point_index=1;
                auto frame_edit=edit;transform(intermediate,frame_edit,scale,variant);
                auto contact_edit=edit;transform(contact,contact_edit,scale,variant);
                frame_edit=edit;transform(contact_frame,frame_edit,scale,variant);
                transform(f,edit,scale,variant);
                TopologyEditOptions options;options.audit.coordinate_limit=1e12;
                TopologySignature signature;
                require(static_cast<bool>(audit_topology(intermediate.graph,intermediate.layout,intermediate.ports,
                        intermediate.paths,signature,options.audit)),
                        "the supporting-line event lies below the finite vertical route");
                require(audit_topology(contact_frame.graph,contact_frame.layout,contact_frame.ports,
                        contact_frame.paths,signature,options.audit).error==TopologyError::AmbiguousTopology,
                        "the same event is a real contact when the finite vertical interval includes it");
                Routes out;TopologyEditReport report;
                auto unchecked=options;unchecked.audit.analytic_guards=false;
                require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{edit},out,unchecked)),
                        "large legal split passes independent continuation audits");
                require(static_cast<bool>(apply_topology_edits(contact.graph,contact.layout,contact.ports,
                        contact.paths,{contact_edit},out,unchecked)),
                        "sampled-only split misses the early real contact at the same displacement scale");
                options.audit.max_guard_constraints=guard_constraints;
                const auto status=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{edit},out,options,&report);
                if(!status)throw std::runtime_error("large supporting-line extension must remain legal: "+status.message);
                require(report.inserted_bends==1,"event-local contact classification accepts the complete legal split");
                const auto guarded=apply_topology_edits(contact.graph,contact.layout,contact.ports,
                        contact.paths,{contact_edit},out,options,&report);
                require(guarded.error==TopologyError::NoAdmissibleStep && guarded.message==route_contact_message &&
                        out.edges.empty() && report.audits==0,"event-local margins still reject true finite-segment contact");
                options.audit.max_guard_constraints=guard_constraints-1;
                require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{edit},out,options,&report).error==
                        TopologyError::CapacityExceeded && out.edges.empty() && report.inserted_bends==0,
                        "event-local classification retains exact guard constraints and atomic budget failure");
            }
        }
        {
            constexpr double offset=0x1p-1060,displacement=0x1p38;
            constexpr std::array scales={0.125,1.0};
            constexpr std::size_t variants=8;
            for(const double scale:scales)for(std::size_t variant=0;variant<variants;++variant) {
                auto f=terminal_merge_contact(offset);
                f.layout.nodes[3]={-5,100,10,10};
                f.ports[1].target={3,Side::North,0.5};
                f.paths.edges[0].points={{-32,0},{-16,0},{16,0},{32,0}};
                f.paths.edges[1].points={{0,-100},{offset,-90},{offset,90},{0,100}};
                auto edit=split(displacement,-displacement);edit.point_index=1;
                auto intermediate=f;
                intermediate.paths.edges[0].points.insert(intermediate.paths.edges[0].points.begin()+2,{offset,-offset});
                auto frame_edit=edit;transform(intermediate,frame_edit,scale,variant);
                transform(f,edit,scale,variant);
                TopologyEditOptions options;options.audit.coordinate_limit=1e12;
                TopologySignature signature;
                require(audit_topology(intermediate.graph,intermediate.layout,intermediate.ports,
                        intermediate.paths,signature,options.audit).error==TopologyError::AmbiguousTopology,
                        "independent frame exposes contact just after insertion begins");
                Routes out;TopologyEditReport report;
                auto unchecked=options;unchecked.audit.analytic_guards=false;
                const auto final_status=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{edit},out,unchecked);
                if(!final_status)throw std::runtime_error("near-initial contact endpoints: "+final_status.message);
                const auto guarded=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{edit},out,options,&report);
                require(guarded.error==TopologyError::NoAdmissibleStep && guarded.message==route_contact_message &&
                        out.edges.empty() && report.audits==0,
                        "a positive insertion event cannot become an allowed initial seed contact through root underflow");
            }
        }
        {
            // The contact at p.y=2^-100 occurs before the end of the 2^-20
            // collapse, but its fraction is about 1-2^-80 and rounds to one in Real.
            constexpr double offset=0x1p-100;
            constexpr std::array scales={0.125,1.0,8.0,1024.0};
            constexpr std::size_t variants=4;
            for(const double scale:scales)for(std::size_t variant=0;variant<variants;++variant) {
                auto f=terminal_merge_contact(offset);
                TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
                auto intermediate=f;intermediate.paths.edges[0].points[1]={0,offset};
                auto frame_edit=merge;transform(intermediate,frame_edit,scale,variant);
                transform(f,merge,scale,variant);
                TopologyEditOptions options;options.audit.orientation_epsilon=0;options.collinear_tolerance=1e-5;
                TopologySignature signature;
                require(audit_topology(intermediate.graph,intermediate.layout,intermediate.ports,
                        intermediate.paths,signature,options.audit).error==TopologyError::AmbiguousTopology,
                        "independent frame exposes contact just before collapse ends");
                Routes out;TopologyEditReport report;
                auto unchecked=options;unchecked.audit.analytic_guards=false;
                const auto final_status=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,unchecked);
                if(!final_status)throw std::runtime_error("near-terminal contact endpoints: "+final_status.message);
                for(const double backoff:{0.0,TopologyOptions{}.guard_backoff}) {
                    options.audit.guard_backoff=backoff;
                    const auto guarded=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options,&report);
                    require(guarded.error==TopologyError::NoAdmissibleStep && guarded.message==route_contact_message &&
                            out.edges.empty() && report.audits==0,
                            "an interior contact cannot become an allowed final seed contact through root rounding");
                }
            }
        }
        {
            // u=(n,n-1), v=(n+1,n): n*n-(n-1)*(n+1)=1, although
            // independently rounded products can be identical in Real.
            for(const double n:cancellation_magnitudes)
            for(std::size_t variant=0;variant<cancellation_variants;++variant) {
                auto f=cancellation_merge(n);
                TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
                transform(f,merge,1,variant);
                TopologyEditOptions options;options.audit.coordinate_limit=2*n;
                TopologySignature signature;
                require(static_cast<bool>(audit_topology(f.graph,f.layout,f.ports,f.paths,signature,options.audit)),
                        "cancellation fixture has valid baseline geometry");
                Routes out;TopologyEditReport report;
                options.collinear_tolerance=0;
                require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options,&report).error==
                        TopologyError::NoAdmissibleStep && out.edges.empty() && report.audits==0,
                        "rounded product cancellation must not prune a nonzero-area bend at zero tolerance");
                options.collinear_tolerance=TopologyEditOptions{}.collinear_tolerance;
                options.audit.max_guard_constraints=1;
                require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options,&report).error==
                        TopologyError::CapacityExceeded && out.edges.empty() && report.removed_bends==0,
                        "a cancellation-prone near-collinear collapse must consume continuous guard work");
                options.audit.max_guard_constraints=TopologyEditOptions{}.audit.max_guard_constraints;
                require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options)),
                        "an unobstructed cancellation-prone collapse remains admissible");
                auto exact=cancellation_merge(n);
                exact.layout.nodes[1]={n,n-5,10,10};
                exact.paths.edges[0].points={{-n,-n},{0,0},{n,n}};
                auto exact_merge=merge;transform(exact,exact_merge,1,variant);
                options.collinear_tolerance=0;options.audit.max_guard_constraints=1;
                options.audit.max_guard_evaluations=1;
                require(static_cast<bool>(apply_topology_edits(exact.graph,exact.layout,exact.ports,
                        exact.paths,{exact_merge},out,options)),
                        "compensated area retains motion-free pruning of exactly collinear diagonal bends");
            }
        }
        {
            for(const double n:cancellation_magnitudes)
            for(std::size_t variant=0;variant<cancellation_variants;++variant) {
                auto f=cancellation_merge(n);
                // Area is one. Project in bend-local coordinates: q=p+(-dy,dx)/|AB|^2.
                const double dx=2*n+1,dy=2*n-1,denominator=dx*dx+dy*dy;
                const Point mid{-dy/(2*denominator),-1+dx/(2*denominator)};
                const double extent=dx/(8*denominator);
                f.graph.nodes.push_back({"thin",extent,extent});
                f.layout.nodes.push_back({mid.x-extent/2,mid.y-extent/2,extent,extent});
                TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
                TopologyEditOptions options;options.audit.coordinate_limit=2*n;
                auto intermediate=f;intermediate.paths.edges[0].points[1]=mid;
                auto intermediate_edit=merge;transform(intermediate,intermediate_edit,1,variant);
                transform(f,merge,1,variant);
                TopologySignature signature;
                require(audit_topology(intermediate.graph,intermediate.layout,intermediate.ports,
                        intermediate.paths,signature,options.audit).error==
                        TopologyError::InvalidGeometry,"independent midpoint exposes a cancellation-scale collapse collision");
                Routes out;TopologyEditReport report;
                auto unchecked=options;unchecked.audit.analytic_guards=false;
                require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,unchecked)),
                        "cancellation-scale collapse has admissible baseline and final geometry");
                require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options,&report).error==
                        TopologyError::NoAdmissibleStep && out.edges.empty() && report.audits==0,
                        "bend-local projection must preserve a tiny collapse through a node interior");
            }
        }
        {
            const auto f=hidden_merge();
            TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
            TopologyEditOptions options;options.collinear_tolerance=1e-5;
            auto intermediate=f.paths;intermediate.edges[0].points[1].y=5+4.1e-5;
            TopologySignature signature;
            require(audit_topology(f.graph,f.layout,f.ports,intermediate,signature).error==
                    TopologyError::InvalidGeometry,"independent frame exposes a near-collinear bend-collapse collision");
            Routes out;TopologyEditReport report;
            auto sampled=options;sampled.audit.analytic_guards=false;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {merge},out,sampled)),"unchecked bend removal misses its swept node interior");
            check_audited(f,out);
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options,&report).error==
                    TopologyError::NoAdmissibleStep && out.edges.empty() && report.audits==0,
                    "continuous bend-collapse guard rejects an intermediate node collision");
        }
        {
            const auto f=merge_crossing(0);
            TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
            TopologyEditOptions options;options.collinear_tolerance=1e-5;
            constexpr std::array scales={0.125,1.0,8.0,1024.0};
            constexpr std::size_t variants=16;
            for(const double scale:scales)for(std::size_t variant=0;variant<variants;++variant) {
                auto candidate=f;auto edit=merge;transform(candidate,edit,scale,variant);
                Routes out;
                require(static_cast<bool>(apply_topology_edits(candidate.graph,candidate.layout,
                        candidate.ports,candidate.paths,{edit},out,options)),
                        "the final removed collapse seed may lie at an existing proper crossing");
                check_audited(candidate,out);
            }
        }
        {
            constexpr std::array scales={0.125,1.0,8.0,1024.0};
            constexpr std::size_t variants=16;
            for(const double scale:scales)for(std::size_t variant=0;variant<variants;++variant) {
                auto f=hidden_merge();TopologyEdit merge;
                merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
                transform(f,merge,scale,variant);
                auto intermediate=f.paths;
                const auto a=f.paths.edges[0].points.front(),b=f.paths.edges[0].points.back();
                const Point seed{(a.x+b.x)/2,(a.y+b.y)/2};
                const auto p=f.paths.edges[0].points[1];
                constexpr double event=0.59;
                intermediate.edges[0].points[1]={p.x+event*(seed.x-p.x),p.y+event*(seed.y-p.y)};
                TopologySignature signature;
                require(audit_topology(f.graph,f.layout,f.ports,intermediate,signature).error==
                        TopologyError::InvalidGeometry,"transformed independent frame exposes a bend-collapse collision");
                TopologyEditOptions options;options.collinear_tolerance=1e-5;
                Routes out;TopologyEditReport report;
                require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options,&report).error==
                        TopologyError::NoAdmissibleStep && out.edges.empty() && report.audits==0,
                        "continuous collapse guards survive scale, axis, reflection, reversal and translation");
                options.audit.analytic_guards=false;
                require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options)),
                        "transformed unchecked removals miss the swept interior");
                check_audited(f,out);
            }
        }
        {
            constexpr std::array scales={0.125,1.0,8.0,1024.0};
            constexpr std::size_t variants=16;
            for(const double scale:scales)for(std::size_t variant=0;variant<variants;++variant) {
                auto f=merge_crossing();TopologyEdit merge;
                merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
                transform(f,merge,scale,variant);
                auto intermediate=f.paths;
                const auto c=f.paths.edges[1].points.front(),d=f.paths.edges[1].points.back();
                intermediate.edges[0].points[1]={(c.x+d.x)/2,(c.y+d.y)/2};
                TopologySignature signature;
                require(audit_topology(f.graph,f.layout,f.ports,intermediate,signature).error==
                        TopologyError::AmbiguousTopology,"independent frame exposes an isolated collapse-route contact");
                TopologyEditOptions options;options.collinear_tolerance=1e-5;
                Routes out;TopologyEditReport report;
                const auto status=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options,&report);
                require(status.error==TopologyError::NoAdmissibleStep && status.message==route_contact_message &&
                        out.edges.empty() && report.audits==0,"continuous collapse guards reject isolated route contact");
                options.audit.analytic_guards=false;
                require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options)),
                        "unchecked near-collinear removal misses a transient crossing-ownership transfer");
                check_audited(f,out);
            }
        }
        {
            auto f=hidden_merge();
            f.graph.nodes[2].height=2e-11;f.layout.nodes[2].height=2e-11;
            f.layout.nodes[2].y=5+4e-10;f.paths.edges[0].points[1].y=5+1e-9;
            TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
            Routes out;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out).error==
                    TopologyError::NoAdmissibleStep,"default collinearity tolerance also guards thin swept obstacles");
            TopologyEditOptions unchecked;unchecked.audit.analytic_guards=false;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,unchecked)),
                    "default-tolerance unchecked collapse has valid endpoint geometry");
            check_audited(f,out);
        }
        {
            auto f=horizontal();f.paths.edges[0].points={{10,5},{55,5+1e-4},{100,5}};
            TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
            TopologyEditOptions limited;limited.collinear_tolerance=1e-5;
            constexpr std::size_t one_edit=6,batch_work=2*one_edit;
            limited.audit.max_guard_constraints=one_edit;limited.audit.max_guard_evaluations=one_edit;
            Routes out;TopologyEditReport report;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {merge},out,limited,&report)) && report.removed_bends==1 && report.continuation_frames==0,
                    "safe near-collinear collapse fits exact guard budgets without extra sampled frames");
            check_audited(f,out);
            limited.audit.max_guard_constraints=batch_work;limited.audit.max_guard_evaluations=batch_work;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {merge,split(0,-10)},out,limited,&report)) && report.removed_bends==1 && report.inserted_bends==1,
                    "collapse and split share the same cumulative motion budget");
            for(const bool evaluation_limit:{false,true}) {
                limited.audit.max_guard_constraints=evaluation_limit ? batch_work : batch_work-1;
                limited.audit.max_guard_evaluations=evaluation_limit ? batch_work-1 : batch_work;
                require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge,split(0,-10)},out,limited,&report).error==
                        TopologyError::CapacityExceeded && out.edges.empty() && report.removed_bends==0,
                        "a later split cannot reset work used by collapse and rolls back the batch");
            }
            f.paths.edges[0].points[1].y=5;
            limited.audit.max_guard_constraints=1;limited.audit.max_guard_evaluations=1;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,limited)),
                    "exact collinear pruning needs no geometric motion guard work");
        }
        {
            auto f=parallel();f.graph.edges[1]={0,2};f.layout.nodes[2]={100,-100,10,10};
            f.ports[1]={{0,Side::East,0.5},{2,Side::West,0.5}};
            f.paths.edges[0].points={{10,5},{55,5+1e-4},{100,5}};
            f.paths.edges[1].points={{10,5},{100,-95}};
            TopologyEdit merge;merge.kind=TopologyEditKind::BendToStraight;merge.point_index=1;
            TopologyEditOptions options;options.collinear_tolerance=1e-5;
            for(std::size_t variant=0;variant<8;++variant) {
                auto candidate=f;auto edit=merge;transform(candidate,edit,1,variant);
                Routes out;
                require(static_cast<bool>(apply_topology_edits(candidate.graph,candidate.layout,
                        candidate.ports,candidate.paths,{edit},out,options)),"collapse preserves qualified shared graph anchors");
                check_audited(candidate,out);
            }
            f=horizontal();f.paths.edges[0].points={{10,5},{40,5+1e-4},{70,5},{100,5}};
            // Six node tests, eight orientation tests and two tests of a
            // permanently collinear fixed anchor outside the adjacent segment.
            options.audit.max_guard_constraints=8;options.audit.max_guard_evaluations=16;
            Routes out;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options)),
                    "collapse skips both affected segments and retains its fixed adjacent segment at exact budgets");
            check_audited(f,out);
            options.audit.max_guard_evaluations=15;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{merge},out,options).error==
                    TopologyError::CapacityExceeded && out.edges.empty(),"collapse route work has an independent cumulative evaluation limit");
        }
        {
            const auto f=hidden_route_contact();
            // The moving bend reaches edge 1 at alpha=1/40. Before and
            // after this instant the same signed crossing exists, but changes
            // from the right child segment to the left child segment.
            auto intermediate=f.paths;
            intermediate.edges[0].points={{10,5},{56,-20},{100,5}};
            TopologySignature signature;
            require(audit_topology(f.graph,f.layout,f.ports,intermediate,signature).error==
                    TopologyError::AmbiguousTopology,"independent frame exposes an isolated bend-route contact");
            Routes out;TopologyEditReport report;
            TopologyEditOptions sampled;sampled.audit.analytic_guards=false;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(40,-1000)},out,sampled,&report)),"sampled split misses isolated route contact");
            check_audited(f,out);
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(40,-1000)},out,{},&report).error==TopologyError::NoAdmissibleStep &&
                    out.edges.empty() && report.audits==0,"continuous split guard rejects isolated route contact");
        }
        {
            constexpr std::array scales={0.125,1.0,8.0,1024.0};
            constexpr std::size_t variants=16;
            for(const double scale:scales)for(std::size_t variant=0;variant<variants;++variant) {
                auto f=hidden_route_contact();auto edit=split(40,-1000);
                transform(f,edit,scale,variant);
                TopologySignature signature;
                require(static_cast<bool>(audit_topology(f.graph,f.layout,f.ports,f.paths,signature)),
                        "transformed route-contact baseline audited");
                const auto a=f.paths.edges[0].points.front(),b=f.paths.edges[0].points.back();
                constexpr double event=1.0/40;
                const Point p{(a.x+b.x)/2+event*edit.displacement.x,
                              (a.y+b.y)/2+event*edit.displacement.y};
                auto intermediate=f.paths;intermediate.edges[0].points={a,p,b};
                require(audit_topology(f.graph,f.layout,f.ports,intermediate,signature).error==
                        TopologyError::AmbiguousTopology,"transformed independent frame exposes route contact");
                Routes out;TopologyEditReport report;
                require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,{edit},out,{},&report).error==
                        TopologyError::NoAdmissibleStep && out.edges.empty() && report.audits==0,
                        "continuous contact guards survive scale, axis, reflection, reversal and translation");
                TopologyEditOptions sampled;sampled.audit.analytic_guards=false;
                require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                        {edit},out,sampled)),"transformed sampled-only edits still miss isolated contacts");
                check_audited(f,out);
            }
        }
        {
            auto f=hidden_route_contact();
            f.layout.nodes[2].x=50;f.layout.nodes[3].x=50;
            for(auto& p:f.paths.edges[1].points)p.x=55;
            Routes out;TopologyEditReport report;
            for(const double dx:{-5.0,5.0}) {
                require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                        {split(dx,-10)},out,{},&report)),"splitting at an existing crossing may immediately leave it");
                check_audited(f,out);
            }
            const auto persistent=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(0,-1000)},out,{},&report);
            require(persistent.error==TopologyError::NoAdmissibleStep &&
                    persistent.message==route_contact_message &&
                    out.edges.empty() && report.audits==0,"persistent collinear bend-route contact is rejected continuously");
            f=hidden_route_contact();
            f.layout.nodes[2].y=30;f.layout.nodes[3].y=1010;
            f.paths.edges[1].points={{56,40},{56,1010}};
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(40,-1000)},out)),"supporting-line events outside finite routes remain legal");
            check_audited(f,out);
        }
        {
            auto f=parallel();
            f.graph.edges[1]={0,2};f.layout.nodes[2]={100,-100,10,10};
            f.ports[1]={{0,Side::East,0.5},{2,Side::West,0.5}};
            f.paths.edges[1].points={{10,5},{100,-95}};
            Routes out;TopologyEditReport report;
            for(std::size_t variant=0;variant<8;++variant) {
                auto transformed=f;auto edit=split(0,-10);transform(transformed,edit,1,variant);
                require(static_cast<bool>(apply_topology_edits(transformed.graph,transformed.layout,
                        transformed.ports,transformed.paths,{edit},out)),"qualified shared graph anchors remain legal");
                check_audited(transformed,out);
                edit.displacement.x*=100;edit.displacement.y*=100;
                const auto status=apply_topology_edits(transformed.graph,transformed.layout,
                        transformed.ports,transformed.paths,{edit},out,{},&report);
                require(status.error==TopologyError::NoAdmissibleStep &&
                        status.message==route_contact_message &&
                        out.edges.empty() && report.audits==0,"shared-terminal collinear overlap is not exempt");
            }
        }
        {
            auto f=horizontal();f.paths.edges[0].points={{10,5},{30,-100},{60,-100},{60,-10},{100,5}};
            Routes out;TopologyEditReport report;
            const auto status=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(80,0)},out,{},&report);
            require(status.error==TopologyError::NoAdmissibleStep &&
                    status.message==route_contact_message &&
                    out.edges.empty() && report.audits==0,"continuous contact guard includes nonadjacent segments of its own route");
        }
        {
            auto f=horizontal();f.paths.edges[0].points={{10,5},{55,-5},{100,5}};
            Routes out;TopologyEditReport report;TopologyEditOptions limited;
            constexpr std::size_t constraints=8,evaluations=20;
            limited.audit.max_guard_constraints=constraints;limited.audit.max_guard_evaluations=evaluations;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(0,-11)},out,limited)),"adjacent fixed segments fit exact node/route work budgets");
            check_audited(f,out);
            for(const std::size_t limit:{std::size_t(6),std::size_t(10),std::size_t(11),evaluations-1}) {
                limited.audit.max_guard_evaluations=limit;
                const auto status=apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                        {split(0,-11)},out,limited,&report);
                require(status.error==TopologyError::CapacityExceeded &&
                        status.message=="topology surgery continuous route guard budget exhausted" &&
                        out.edges.empty() && report.audits==0,
                        "route orientation, polynomial and classification work consume the cumulative budget");
            }
        }
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
                    transform(f,edit,scale,variant);
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
            constexpr std::size_t one_edit_work=6;
            // The second split also tests its two children against the
            // remaining fixed segment: four orientations each, plus three
            // supporting-line events outside the finite segments (two tests
            // each). Those events must consume work without rejecting edits.
            constexpr std::size_t batch_constraints=2*one_edit_work+2;
            constexpr std::size_t batch_evaluations=2*one_edit_work+8+6;
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
            limited.audit.max_guard_constraints=batch_constraints;limited.audit.max_guard_evaluations=batch_evaluations;
            require(static_cast<bool>(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(0,-10),split(0,-11)},out,limited,&report)) && report.inserted_bends==2,
                    "two safe edits share exact cumulative node and route budgets");
            check_audited(f,out);
            limited.audit.max_guard_constraints=batch_constraints-1;
            require(apply_topology_edits(f.graph,f.layout,f.ports,f.paths,
                    {split(0,-10),split(0,-11)},out,limited,&report).error==TopologyError::CapacityExceeded &&
                    out.edges.empty() && report.inserted_bends==0,
                    "a later edit's constraint exhaustion rolls back the entire batch");
            limited.audit.max_guard_constraints=batch_constraints;limited.audit.max_guard_evaluations=batch_evaluations-1;
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
