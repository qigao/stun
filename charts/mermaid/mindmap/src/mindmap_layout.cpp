#include "mindmap_layout.h"

#include <cstddef>
#include <exception>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mermaid::mindmap {
namespace {
using stun::graphlayout::TidyTreeError;
using stun::graphlayout::TidyTreeStatus;
using stun::graphlayout::TreeNoParent;

TidyTreeStatus error(TidyTreeError kind, const char* reason) {
    return {kind, reason};
}

// Every sibling chain must be finite even if a hand-built AST has cycles.
TidyTreeStatus siblings(const MindmapNode* first,
                        std::vector<const MindmapNode*>& result,
                        std::size_t budget) {
    std::unordered_set<const MindmapNode*> visited;
    for (const MindmapNode* node = first; node; node = node->next) {
        if (!visited.insert(node).second)
            return error(TidyTreeError::InvalidForest, "cyclic Mindmap sibling list");
        if (result.size() >= budget)
            return error(TidyTreeError::CapacityExceeded, "Mindmap node budget exceeded");
        result.push_back(node);
    }
    return {};
}
} // namespace

TidyTreeStatus layout_mindmap(const MindmapDiagram& diagram,
                               const MeasureNode& measure, Layout& out,
                               const stun::graphlayout::TidyTreeOptions& options) {
    out = {};
    if (!measure)
        return error(TidyTreeError::InvalidOptions, "Mindmap requires node measurement");
    if (options.max_nodes == 0)
        return error(TidyTreeError::InvalidOptions, "invalid Mindmap node budget");

    std::vector<const MindmapNode*> roots;
    auto status = siblings(diagram.root, roots, options.max_nodes);
    if (!status) return status;

    std::vector<std::pair<const MindmapNode*,std::size_t>> pending;
    for (auto it=roots.rbegin(); it!=roots.rend(); ++it)
        pending.push_back({*it, TreeNoParent});

    stun::graphlayout::Tree forest;
    std::vector<const MindmapNode*> nodes;
    std::unordered_set<const MindmapNode*> visited;
    visited.reserve(options.max_nodes);
    while (!pending.empty()) {
        const auto current=pending.back();
        pending.pop_back();
        if (!visited.insert(current.first).second)
            return error(TidyTreeError::InvalidForest, "duplicate or cyclic Mindmap node ownership");
        if (nodes.size() >= options.max_nodes)
            return error(TidyTreeError::CapacityExceeded, "Mindmap node budget exceeded");
        if (current.first->parent &&
            (current.second == TreeNoParent || nodes[current.second] != current.first->parent))
            return error(TidyTreeError::InvalidParent, "Mindmap parent pointer mismatch");
        std::pair<double,double> size;
        try {
            size = measure(*current.first);
        } catch (...) {
            return error(TidyTreeError::InvalidNode, "Mindmap measurement callback failed");
        }
        const auto index=nodes.size();
        forest.nodes.push_back({"mindmap:"+std::to_string(index),size.first,size.second,current.second});
        nodes.push_back(current.first);
        std::vector<const MindmapNode*> children;
        status=siblings(current.first->children,children,options.max_nodes);
        if (!status) return status;
        for (auto it=children.rbegin();it!=children.rend();++it)
            pending.push_back({*it,index});
    }
    stun::graphlayout::TidyTreeLayout result;
    status=stun::graphlayout::layout_tidy_tree(forest,result,options);
    if (!status) return status;
    out.nodes=std::move(nodes);
    out.geometry=std::move(result);
    return {};
}

} // namespace mermaid::mindmap
