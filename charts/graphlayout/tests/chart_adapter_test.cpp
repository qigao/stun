#include "dotgraph_layout.h"
#include "flowchart_layout.h"

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
        std::cout<<"chart adapters: all checks passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "chart adapters: " << e.what() << '\n';
        return 1;
    }
}
