#include "stun/graphlayout/tidy_tree.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace stun::graphlayout;

namespace {
void require(bool ok, const char* msg) { if (!ok) throw std::runtime_error(msg); }

void validate(const Tree& tree, const TidyTreeLayout& r) {
    require(r.nodes.size() == tree.nodes.size(), "every node is placed");
    for (std::size_t i = 0; i < r.nodes.size(); ++i) {
        const auto& a = r.nodes[i];
        require(a.parent == tree.nodes[i].parent, "parent index preserved");
        require(a.width == tree.nodes[i].width && a.height == tree.nodes[i].height,
                "variable sizes preserved");
        require(std::isfinite(a.x) && std::isfinite(a.y) && a.x >= -1e-8 && a.y >= 0,
                "finite nonnegative position");
        require(a.x+a.width <= r.width+1e-8 && a.y+a.height <= r.height+1e-8,
                "rectangle inside overall extent");
        if (a.parent != TreeNoParent) {
            const auto& p = r.nodes[a.parent];
            require(a.depth == p.depth+1, "child depth correct");
            require(a.y >= p.y+p.height, "child does not overlap parent vertically");
        }
        for (std::size_t j = i+1; j < r.nodes.size(); ++j) {
            const auto& b = r.nodes[j];
            require(a.x+a.width <= b.x+1e-8 || b.x+b.width <= a.x+1e-8 ||
                    a.y+a.height <= b.y+1e-8 || b.y+b.height <= a.y+1e-8,
                    "tree rectangles non-overlapping");
        }
    }
}

TidyTreeLayout run(const Tree& tree, const TidyTreeOptions& opt = {}) {
    TidyTreeLayout out;
    const auto status = layout_tidy_tree(tree,out,opt);
    if (!status) throw std::runtime_error("tidy tree failed: " + status.message);
    validate(tree,out);
    return out;
}

} // namespace

int main() {
    try {
        {
            TidyTreeLayout l;
            require(static_cast<bool>(layout_tidy_tree({}, l)), "empty forest accepted");
            require(l.nodes.empty() && l.width==0 && l.height==0, "empty extent zero");
        }
        {
            Tree tree{{{"root",200,40,TreeNoParent},
                       {"right",70,90,0},
                       {"left",120,20,0},
                       {"deep-a",350,50,1},
                       {"deep-b",40,60,1},
                       {"third",90,35,0},
                       {"deep-third",160,80,5}}};
            const auto layout = run(tree);
            const auto& r = layout.nodes[0];
            const double c0=layout.nodes[1].x+layout.nodes[1].width/2;
            const double c1=layout.nodes[5].x+layout.nodes[5].width/2;
            require(std::abs((c0+c1)*0.5-(r.x+r.width/2)) < 1e-9,
                    "parent centered between first and last child centers");
            require(c0 < layout.nodes[2].x+layout.nodes[2].width/2 &&
                    layout.nodes[2].x+layout.nodes[2].width/2 < c1,
                    "siblings retain input order");
            require(layout.nodes[3].y > layout.nodes[2].y, "depth grid preserves variable height");
            const auto repeated = run(tree);
            for (std::size_t i=0;i<tree.nodes.size();++i)
                require(layout.nodes[i].x == repeated.nodes[i].x &&
                        layout.nodes[i].y == repeated.nodes[i].y,
                        "deterministic identical placement");
        }
        {
            Tree forest{{{"root-a",400,40,TreeNoParent},
                          {"child-a",30,30,0},
                          {"root-b",100,110,TreeNoParent},
                          {"child-b",260,20,2},
                          {"root-c",80,25,TreeNoParent}}};
            const auto r = run(forest);
            require(r.nodes[0].x < r.nodes[2].x && r.nodes[2].x < r.nodes[4].x,
                    "forest root order retained");
            require(r.nodes[1].y == r.nodes[3].y, "forest shares global depth levels");
        }
        {
            Tree chain;
            for (std::size_t i=0;i<1000;++i)
                chain.nodes.push_back({"n"+std::to_string(i),10.0+static_cast<double>(i%5),15.0+static_cast<double>(i%3),
                                       i==0 ? TreeNoParent : i-1});
            const auto r=run(chain);
            require(r.nodes.back().depth==999, "nonrecursive deep tree");
        }
        {
            TidyTreeLayout previous;
            for (std::size_t count=4;count<45;++count) {
                Tree t;
                for (std::size_t i=0;i<count;++i)
                    t.nodes.push_back({"r"+std::to_string(i),
                                       12.0+((i*11+count)%70),
                                       12.0+((i*19+count)%40),
                                       i==0?TreeNoParent:(i-1)/3});
                const auto r=run(t);
                require(!r.nodes.empty(),"stress forest layout");
                previous=r;
            }
            require(previous.nodes.size()==44,"generated family count");
        }
        {
            Tree t{{{"a",10,20,1},{"b",10,20,0}}};
            TidyTreeLayout out;
            require(layout_tidy_tree(t,out).error==TidyTreeError::InvalidForest,
                    "cycle without root rejected");
            require(out.nodes.empty(),"failure clears output");
            t.nodes[0].parent=TreeNoParent;
            t.nodes[1].parent=3;
            require(layout_tidy_tree(t,out).error==TidyTreeError::InvalidParent,
                    "orphan parent rejected");
            t.nodes[1].parent=0;
            t.nodes[1].id="a";
            require(layout_tidy_tree(t,out).error==TidyTreeError::DuplicateId,
                    "duplicate stable identity rejected");
            t.nodes[1].id="b";
            t.nodes[1].width=-1;
            require(layout_tidy_tree(t,out).error==TidyTreeError::InvalidNode,
                    "negative size rejected");
            t.nodes[1].width=10;
            TidyTreeOptions options;
            options.max_nodes=1;
            require(layout_tidy_tree(t,out,options).error==TidyTreeError::CapacityExceeded,
                    "node budget explicit");
            options.max_nodes=10;
            options.max_contour_cells=3;
            require(layout_tidy_tree(t,out,options).error==TidyTreeError::CapacityExceeded,
                    "contour memory explicit");
            options.max_contour_cells=100;
            options.max_pair_checks=1;
            require(static_cast<bool>(layout_tidy_tree(t,out,options)),"one pair valid");
            options.max_pair_checks=0;
            require(layout_tidy_tree(t,out,options).error==TidyTreeError::InvalidOptions,
                    "invalid pair budget rejected");
        }
        std::cout<<"tidy tree: all checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "tidy tree: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
}
