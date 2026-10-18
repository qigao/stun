#include "mindmap_layout.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace mermaid::mindmap;

int main() {
    try {
        MindmapNode root{},a{},b{},grandchild{},other{};
        root.label=const_cast<char*>("Root");
        a.label=const_cast<char*>("a");
        b.label=const_cast<char*>("b");
        grandchild.label=const_cast<char*>("grandchild");
        other.label=const_cast<char*>("other root");
        root.children=&a;
        a.next=&b;
        a.parent=&root;
        b.parent=&root;
        b.children=&grandchild;
        grandchild.parent=&b;
        root.next=&other;
        MindmapDiagram diagram{};
        diagram.root=&root;
        MeasureNode measured=[](const MindmapNode& n) {
            return std::make_pair(static_cast<double>(30+std::string(n.label).size()*7),
                                  static_cast<double>(20+std::string(n.label).size()));
        };
        Layout l;
        const auto status=layout_mindmap(diagram,measured,l);
        if (!status) throw std::runtime_error(status.message);
        if (l.nodes.size()!=5 || l.geometry.nodes.size()!=5)
            throw std::runtime_error("Mindmap retains all descendants and roots");
        if (l.nodes[0]!=&root || l.nodes[1]!=&a || l.nodes[2]!=&b ||
            l.nodes[3]!=&grandchild || l.nodes[4]!=&other)
            throw std::runtime_error("Mindmap ordered preorder identity lost");
        if (l.geometry.nodes[3].parent!=2 || l.geometry.nodes[4].parent!=stun::graphlayout::TreeNoParent)
            throw std::runtime_error("Mindmap parent mapping incorrect");
        if (l.geometry.nodes[3].y<=l.geometry.nodes[2].y)
            throw std::runtime_error("grandchild must be lower");
        Layout repeat;
        if (!layout_mindmap(diagram,measured,repeat))
            throw std::runtime_error("repeat Mindmap failed");
        for (std::size_t i=0;i<l.nodes.size();++i)
            if (l.geometry.nodes[i].x!=repeat.geometry.nodes[i].x)
                throw std::runtime_error("Mindmap must be deterministic");
        root.next=&root;
        if (layout_mindmap(diagram,measured,l).error!=stun::graphlayout::TidyTreeError::InvalidForest ||
            !l.nodes.empty())
            throw std::runtime_error("cyclic root sibling list rejected atomically");
        root.next=nullptr;
        b.parent=&a;
        if (layout_mindmap(diagram,measured,l).error!=stun::graphlayout::TidyTreeError::InvalidParent)
            throw std::runtime_error("invalid source parent rejected");
        std::cout<<"mindmap adapter: all checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "mindmap adapter: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
