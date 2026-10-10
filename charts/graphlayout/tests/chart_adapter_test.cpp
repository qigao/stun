#include "dotgraph_layout.h"
#include "flowchart_layout.h"
#include "stun/graphlayout/polyline.h"
#include "stun/graphlayout/nudging.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
void check(bool value, const char* description) {
    if (!value) throw std::runtime_error(description);
}

bool different(double a, double b) { return std::abs(a-b) > 1e-8; }
}

int main() {
    try {
        {
            char aid[]="A", bid[]="B", cid[]="C", dangling[]="unknown";
            DotGraphNode a{}, b{}, c{};
            a.id=aid; b.id=bid; c.id=cid;
            DotGraphEdge ab{}, bc{};
            ab.from=aid; ab.to=bid; ab.next=&bc;
            bc.from=bid; bc.to=cid;
            DotGraphDiagram diagram{};
            diagram.nodes=&a; diagram.edges=&ab; diagram.rankdir=DG_RANKDIR_TB;
            const std::vector<DotGraphNode*> nodes{&a,&b,&c};
            const std::unordered_map<std::string,std::size_t> index{{"A",0},{"B",1},{"C",2}};
            const std::vector<double> widths{20,50,30}, heights{25,15,35};
            auto tb=dotgraph::place_dot_nodes(&diagram,nodes,index,widths,heights,16,20);
            check(tb.nodes.size()==3 && tb.nodes[0].y<tb.nodes[1].y &&
                  tb.nodes[1].y<tb.nodes[2].y,"DOT top-to-bottom rank");
            diagram.rankdir=DG_RANKDIR_RL;
            auto rl=dotgraph::place_dot_nodes(&diagram,nodes,index,widths,heights,16,20);
            check(rl.nodes[0].x>rl.nodes[1].x && rl.nodes[1].x>rl.nodes[2].x,
                  "DOT right-to-left rank");
            check(rl.nodes[1].width==widths[1] && different(tb.nodes[1].y,rl.nodes[1].y),
                  "DOT stable index and shape dimensions");
            bc.to=dangling;
            bool rejected=false;
            try { (void)dotgraph::place_dot_nodes(&diagram,nodes,index,widths,heights,16,20); }
            catch(const std::invalid_argument&) {rejected=true;}
            check(rejected,"DOT dangling endpoint rejected");
        }
        {
            char aid[]="Start", bid[]="End", direction[]="TB", bad[]="Sideways";
            FlowchartNode a{},b{};
            a.id=aid; b.id=bid;
            FlowchartEdge ab{}; ab.from=aid; ab.to=bid;
            FlowchartDiagram diagram{};
            diagram.nodes=&a; diagram.edges=&ab; diagram.direction=direction;
            const std::vector<FlowchartNode*> nodes{&a,&b};
            const std::unordered_map<std::string,std::size_t> index{{"Start",0},{"End",1}};
            const std::vector<double> widths{24,48}, heights{18,30};
            auto tb=mermaid::flowchart::place_flowchart_nodes(&diagram,nodes,index,widths,heights,18,44);
            check(tb.nodes.size()==2 && tb.nodes[0].y<tb.nodes[1].y,"Mermaid top-to-bottom rank");
            char left_to_right[]="LR";
            diagram.direction=left_to_right;
            auto lr=mermaid::flowchart::place_flowchart_nodes(&diagram,nodes,index,widths,heights,18,44);
            check(lr.nodes[0].x<lr.nodes[1].x,"Mermaid left-to-right rank");
            diagram.direction=bad;
            bool rejected=false;
            try { (void)mermaid::flowchart::place_flowchart_nodes(&diagram,nodes,index,widths,heights,18,44); }
            catch(const std::invalid_argument&) {rejected=true;}
            check(rejected,"Mermaid unsupported direction rejected");
        }
        {
            char aid[]="First",bid[]="Second", label[]="first";
            DotGraphNode a{}, b{}; a.id=aid; b.id=bid; a.next=&b;
            DotGraphEdge edge{}; edge.from=aid; edge.to=bid;
            DotGraphDiagram diagram{};
            diagram.nodes=&a; diagram.edges=&edge; diagram.rankdir=DG_RANKDIR_TB;
            // Upstream parser sets these to -1 when unspecified.
            diagram.routing_shape_buffer=-1;
            diagram.routing_nudging_distance=-1;
            diagram.routing_segment_penalty=-1;
            diagram.routing_angle_penalty=-1;
            diagram.routing_crossing_penalty=-1;
            diagram.routing_nudge_orthogonal_ends=-1;
            diagram.routing_nudge_shared_paths=-1;
            std::unordered_map<std::string,std::size_t> lookup{{"First",0},{"Second",1}};
            stun::graphlayout::Layout layout;
            layout.nodes={{0,0,30,30},{140,80,30,30}};
            diagram.routing_mode=DG_ROUTE_POLYLINE;
            auto poly=dotgraph::route_dot_edges(&diagram,lookup,layout);
            check(poly.edges.size()==1 && poly.edges.front().points.size()>=4,
                  "DOT Polyline adapter returns native anchors and stubs");
            stun::graphlayout::PolylineOptions polyopt;
            check(static_cast<bool>(stun::graphlayout::validate_polyline_routes(
                layout,{{{0,stun::graphlayout::Side::South},{1,stun::graphlayout::Side::North}}},poly,polyopt)),
                "DOT native polyline geometry validated");
            diagram.routing_mode=DG_ROUTE_ORTHOGONAL;
            auto ortho=dotgraph::route_dot_edges(&diagram,lookup,layout);
            check(ortho.edges.size()==1,"DOT native orthogonal still callable");
            diagram.routing_mode=DG_ROUTE_POLYLINE;
            diagram.routing_nudging_distance=4;
            bool rejected=false;
            try { (void)dotgraph::route_dot_edges(&diagram,lookup,layout); }
            catch(const std::invalid_argument&) { rejected=true; }
            check(rejected,"DOT unsupported nudging rejected, never silently ignored");
            diagram.routing_nudging_distance=-1;
            DotGraphEdge duplicate{};
            duplicate.from=aid; duplicate.to=bid; edge.next=&duplicate;
            diagram.rankdir=DG_RANKDIR_LR;
            layout.nodes={{0,0,30,30},{170,0,30,30}};
            diagram.routing_mode=DG_ROUTE_ORTHOGONAL;
            diagram.routing_nudging_distance=12;
            auto nudged=dotgraph::route_dot_edges(&diagram,lookup,layout);
            check(nudged.edges.size()==2,"DOT paired nudging routes emitted");
            stun::graphlayout::RouteInteractions dot_audit;
            stun::graphlayout::NudgingOptions dot_options;
            check(static_cast<bool>(stun::graphlayout::audit_orthogonal_interactions(
                layout,{{{0,stun::graphlayout::Side::East},{1,stun::graphlayout::Side::West}},
                        {{0,stun::graphlayout::Side::East},{1,stun::graphlayout::Side::West}}},
                nudged,dot_audit,dot_options)),"DOT nudging geometry audited");
            check(dot_audit.shared_length<140,"DOT native nudging reduces duplicate overlap");
            diagram.routing_nudging_distance=-1;
            edge.next=nullptr;
            edge.from_port=label;
            rejected=false;
            try { (void)dotgraph::route_dot_edges(&diagram,lookup,layout); }
            catch(const std::invalid_argument&) { rejected=true; }
            check(rejected,"DOT named ports require actual measured port geometry");
        }
        {
            char aid[]="Top",bid[]="Bottom",direction[]="TB";
            FlowchartNode a{},b{}; a.id=aid; b.id=bid; a.next=&b;
            FlowchartEdge edge{}; edge.from=aid; edge.to=bid;
            FlowchartDiagram diagram{};
            diagram.nodes=&a; diagram.edges=&edge; diagram.direction=direction;
            diagram.routing_shape_buffer=-1;
            diagram.routing_nudging_distance=-1;
            diagram.routing_segment_penalty=-1;
            diagram.routing_angle_penalty=-1;
            diagram.routing_crossing_penalty=-1;
            diagram.routing_nudge_orthogonal_ends=-1;
            diagram.routing_nudge_shared_paths=-1;
            std::unordered_map<std::string,std::size_t> lookup{{"Top",0},{"Bottom",1}};
            stun::graphlayout::Layout layout;
            layout.nodes={{0,0,30,30},{110,130,30,30}};
            diagram.routing_mode=FC_ROUTE_POLYLINE;
            auto poly=mermaid::flowchart::route_flowchart_edges(&diagram,lookup,layout);
            check(poly.edges.size()==1 && poly.edges[0].points.size()>=4,
                  "Mermaid Polyline separate solver selected");
            check(static_cast<bool>(stun::graphlayout::validate_polyline_routes(
                layout,{{{0},{1}}},poly)),"Mermaid Polyline geometry validated");
            diagram.routing_mode=FC_ROUTE_ORTHOGONAL;
            auto ortho=mermaid::flowchart::route_flowchart_edges(&diagram,lookup,layout);
            check(ortho.edges.size()==1,"Mermaid orthogonal mode independent");
            diagram.routing_mode=FC_ROUTE_POLYLINE;
            diagram.routing_segment_penalty=2;
            bool rejected=false;
            try { (void)mermaid::flowchart::route_flowchart_edges(&diagram,lookup,layout); }
            catch(const std::invalid_argument&) { rejected=true; }
            check(rejected,"Mermaid polyline unsupported bend penalty rejected");
            diagram.routing_segment_penalty=-1;
            FlowchartEdge duplicate{}; duplicate.from=aid; duplicate.to=bid;
            edge.next=&duplicate;
            layout.nodes={{0,0,30,30},{170,0,30,30}};
            diagram.routing_mode=FC_ROUTE_ORTHOGONAL;
            diagram.routing_nudging_distance=12;
            auto nudged=mermaid::flowchart::route_flowchart_edges(&diagram,lookup,layout);
            check(nudged.edges.size()==2,"Mermaid paired nudging routes emitted");
            stun::graphlayout::NudgingOptions joint;
            stun::graphlayout::RouteInteractions mermaid_audit;
            check(static_cast<bool>(stun::graphlayout::audit_orthogonal_interactions(
                layout,{{{0},{1}},{{0},{1}}},nudged,mermaid_audit,joint)),
                "Mermaid nudging geometry audited");
            check(mermaid_audit.shared_length<140,"Mermaid native nudging reduces shared overlap");
        }
        std::cout<<"chart adapters: all checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "chart adapters: " << e.what() << '\n';
        return 1;
    }
}
