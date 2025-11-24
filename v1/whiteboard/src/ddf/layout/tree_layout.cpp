#include <whiteboard/ddf/layout_algorithms.h>
#include <whiteboard/ddf/data_layer.h>
#include <algorithm>
#include <cmath>
#include <set>

namespace whiteboard {
namespace ddf {

TreeLayout::TreeLayout(const TreeLayoutParams& params)
    : params_(params) {
}

LayoutResult TreeLayout::compute(const DataLayer& data_layer, const std::string& root_node_id) {
    LayoutResult result;
    
    // Find root node
    std::string root_id = root_node_id;
    if (root_id.empty()) {
        std::vector<std::string> roots;
        find_roots(data_layer, roots);
        if (roots.empty()) {
            return result;  // No nodes or no root found
        }
        root_id = roots[0];  // Use first root
    }
    
    // Build tree structure
    auto tree = build_tree(data_layer, root_id);
    if (!tree) {
        return result;
    }
    
    // Compute layout
    compute_layout(tree.get());
    
    // Collect positions
    collect_positions(tree.get(), result);
    
    // Transform coordinates based on direction
    transform_coordinates(result);
    
    return result;
}

std::unique_ptr<TreeLayout::TreeNode> TreeLayout::build_tree(
    const DataLayer& data_layer, const std::string& root_id) {
    
    auto root = std::make_unique<TreeNode>();
    root->node_id = root_id;
    
    // Add children recursively
    add_children(root.get(), data_layer);
    
    // Compute depths
    compute_depths(root.get(), 0);
    
    return root;
}

void TreeLayout::find_roots(const DataLayer& data_layer, std::vector<std::string>& roots) {
    auto all_nodes = data_layer.get_all_nodes();
    std::set<std::string> has_parent;
    
    // Find all nodes that have incoming relationships
    for (const auto* node : all_nodes) {
        auto relationships = data_layer.get_relationships_for_node(node->id);
        for (const auto* rel : relationships) {
            if (rel->to_node_id == node->id && 
                (rel->type == "parent-child" || rel->type == "reports_to")) {
                has_parent.insert(node->id);
            }
        }
    }
    
    // Nodes without parents are roots
    for (const auto* node : all_nodes) {
        if (has_parent.find(node->id) == has_parent.end()) {
            roots.push_back(node->id);
        }
    }
}

void TreeLayout::add_children(TreeNode* node, const DataLayer& data_layer) {
    auto relationships = data_layer.get_relationships_for_node(node->node_id);
    
    for (const auto* rel : relationships) {
        // Find child relationships (where this node is the parent)
        if (rel->from_node_id == node->node_id && 
            (rel->type == "parent-child" || rel->type == "reports_to")) {
            
            auto child = std::make_unique<TreeNode>();
            child->node_id = rel->to_node_id;
            child->parent = node;
            
            // Recursively add children
            add_children(child.get(), data_layer);
            
            node->children.push_back(std::move(child));
        }
    }
}

void TreeLayout::compute_depths(TreeNode* node, int depth) {
    node->depth = depth;
    for (auto& child : node->children) {
        compute_depths(child.get(), depth + 1);
    }
}

void TreeLayout::compute_layout(TreeNode* root) {
    // Reingold-Tilford algorithm
    first_walk(root);
    second_walk(root, 0.0f);
}

void TreeLayout::first_walk(TreeNode* node) {
    if (node->children.empty()) {
        // Leaf node
        if (node->parent && !node->parent->children.empty() && 
            node->parent->children[0].get() != node) {
            // Not the leftmost child
            auto& siblings = node->parent->children;
            for (size_t i = 1; i < siblings.size(); ++i) {
                if (siblings[i].get() == node) {
                    node->prelim = siblings[i-1]->prelim + 
                                  get_node_size(siblings[i-1].get(), true) + 
                                  params_.sibling_spacing;
                    break;
                }
            }
        } else {
            node->prelim = 0.0f;
        }
    } else {
        // Internal node
        for (auto& child : node->children) {
            first_walk(child.get());
        }
        
        // Position node at midpoint of children
        float leftmost = node->children.front()->prelim;
        float rightmost = node->children.back()->prelim;
        node->prelim = (leftmost + rightmost) / 2.0f;
        
        // Adjust for left siblings
        if (node->parent && !node->parent->children.empty() && 
            node->parent->children[0].get() != node) {
            auto& siblings = node->parent->children;
            for (size_t i = 1; i < siblings.size(); ++i) {
                if (siblings[i].get() == node) {
                    float left_sibling_prelim = siblings[i-1]->prelim;
                    float spacing = get_node_size(siblings[i-1].get(), true) + 
                                   params_.subtree_spacing;
                    node->mod = node->prelim - left_sibling_prelim - spacing;
                    node->prelim = left_sibling_prelim + spacing;
                    break;
                }
            }
        }
    }
}

void TreeLayout::second_walk(TreeNode* node, float modsum) {
    node->x = node->prelim + modsum;
    node->y = node->depth * (get_node_size(node, false) + params_.vertical_spacing);
    
    for (auto& child : node->children) {
        second_walk(child.get(), modsum + node->mod);
    }
}

float TreeLayout::get_node_size(TreeNode* node, bool is_width) {
    // Default node size (can be customized based on node data)
    return is_width ? params_.horizontal_spacing : params_.vertical_spacing * 0.5f;
}

void TreeLayout::collect_positions(TreeNode* node, LayoutResult& result) {
    result.positions[node->node_id] = Vec2(node->x, node->y);
    
    // Update bounds
    if (result.positions.size() == 1) {
        result.bounds_min = Vec2(node->x, node->y);
        result.bounds_max = Vec2(node->x, node->y);
    } else {
        result.bounds_min.x = std::min(result.bounds_min.x, node->x);
        result.bounds_min.y = std::min(result.bounds_min.y, node->y);
        result.bounds_max.x = std::max(result.bounds_max.x, node->x);
        result.bounds_max.y = std::max(result.bounds_max.y, node->y);
    }
    
    for (auto& child : node->children) {
        collect_positions(child.get(), result);
    }
}

void TreeLayout::transform_coordinates(LayoutResult& result) {
    // Transform based on direction
    for (auto& [node_id, pos] : result.positions) {
        float x = pos.x;
        float y = pos.y;
        
        switch (params_.direction) {
            case TreeDirection::TopDown:
                // No transformation needed
                break;
            case TreeDirection::BottomUp:
                pos.y = -y;
                break;
            case TreeDirection::LeftRight:
                pos.x = y;
                pos.y = x;
                break;
            case TreeDirection::RightLeft:
                pos.x = -y;
                pos.y = x;
                break;
        }
    }
    
    // Recalculate bounds after transformation
    bool first = true;
    for (const auto& [node_id, pos] : result.positions) {
        if (first) {
            result.bounds_min = pos;
            result.bounds_max = pos;
            first = false;
        } else {
            result.bounds_min.x = std::min(result.bounds_min.x, pos.x);
            result.bounds_min.y = std::min(result.bounds_min.y, pos.y);
            result.bounds_max.x = std::max(result.bounds_max.x, pos.x);
            result.bounds_max.y = std::max(result.bounds_max.y, pos.y);
        }
    }
}

} // namespace ddf
} // namespace whiteboard
