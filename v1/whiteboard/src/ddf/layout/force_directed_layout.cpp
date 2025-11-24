#include <whiteboard/ddf/layout_algorithms.h>
#include <whiteboard/ddf/data_layer.h>
#include <algorithm>
#include <cmath>
#include <random>

namespace whiteboard {
namespace ddf {

ForceDirectedLayout::ForceDirectedLayout(const ForceDirectedLayoutParams& params)
    : params_(params) {
}

LayoutResult ForceDirectedLayout::compute(const DataLayer& data_layer) {
    LayoutResult result;
    
    auto all_nodes = data_layer.get_all_nodes();
    if (all_nodes.empty()) {
        return result;
    }
    
    // Create force nodes
    std::vector<ForceNode> force_nodes;
    force_nodes.reserve(all_nodes.size());
    
    for (const auto* node : all_nodes) {
        ForceNode fn;
        fn.node_id = node->id;
        fn.position = Vec2(0.0f, 0.0f);
        fn.velocity = Vec2(0.0f, 0.0f);
        fn.force = Vec2(0.0f, 0.0f);
        force_nodes.push_back(fn);
    }
    
    // Initialize positions randomly
    initialize_positions(force_nodes);
    
    // Simulate forces
    float dt = 0.1f;
    for (int iter = 0; iter < params_.max_iterations; ++iter) {
        // Reset forces
        for (auto& node : force_nodes) {
            node.force = Vec2(0.0f, 0.0f);
        }
        
        // Compute forces
        compute_forces(force_nodes, data_layer);
        
        // Apply forces
        apply_forces(force_nodes, dt);
        
        // Check convergence
        if (has_converged(force_nodes)) {
            break;
        }
    }
    
    // Collect results
    bool first = true;
    for (const auto& node : force_nodes) {
        result.positions[node.node_id] = node.position;
        
        if (first) {
            result.bounds_min = node.position;
            result.bounds_max = node.position;
            first = false;
        } else {
            result.bounds_min.x = std::min(result.bounds_min.x, node.position.x);
            result.bounds_min.y = std::min(result.bounds_min.y, node.position.y);
            result.bounds_max.x = std::max(result.bounds_max.x, node.position.x);
            result.bounds_max.y = std::max(result.bounds_max.y, node.position.y);
        }
    }
    
    return result;
}

void ForceDirectedLayout::initialize_positions(std::vector<ForceNode>& nodes) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
    
    for (auto& node : nodes) {
        node.position.x = dist(gen);
        node.position.y = dist(gen);
    }
}

void ForceDirectedLayout::compute_forces(std::vector<ForceNode>& nodes, const DataLayer& data_layer) {
    // Repulsion between all pairs
    for (size_t i = 0; i < nodes.size(); ++i) {
        for (size_t j = i + 1; j < nodes.size(); ++j) {
            Vec2 repulsion = compute_repulsion(nodes[i], nodes[j]);
            nodes[i].force.x += repulsion.x;
            nodes[i].force.y += repulsion.y;
            nodes[j].force.x -= repulsion.x;
            nodes[j].force.y -= repulsion.y;
        }
    }
    
    // Attraction along edges
    for (size_t i = 0; i < nodes.size(); ++i) {
        auto relationships = data_layer.get_relationships_for_node(nodes[i].node_id);
        
        for (const auto* rel : relationships) {
            // Find the other node
            std::string other_id;
            if (rel->from_node_id == nodes[i].node_id) {
                other_id = rel->to_node_id;
            } else if (rel->to_node_id == nodes[i].node_id) {
                other_id = rel->from_node_id;
            } else {
                continue;
            }
            
            // Find the other node in our list
            for (size_t j = 0; j < nodes.size(); ++j) {
                if (nodes[j].node_id == other_id) {
                    Vec2 attraction = compute_attraction(nodes[i], nodes[j]);
                    nodes[i].force.x += attraction.x;
                    nodes[i].force.y += attraction.y;
                    break;
                }
            }
        }
    }
}

Vec2 ForceDirectedLayout::compute_repulsion(const ForceNode& n1, const ForceNode& n2) {
    Vec2 delta;
    delta.x = n1.position.x - n2.position.x;
    delta.y = n1.position.y - n2.position.y;
    
    float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    
    if (distance < 0.01f) {
        distance = 0.01f;  // Avoid division by zero
    }
    
    float force_magnitude = params_.repulsion_strength / (distance * distance);
    Vec2 result;
    result.x = (delta.x / distance) * force_magnitude;
    result.y = (delta.y / distance) * force_magnitude;
    return result;
}

Vec2 ForceDirectedLayout::compute_attraction(const ForceNode& n1, const ForceNode& n2) {
    Vec2 delta;
    delta.x = n2.position.x - n1.position.x;
    delta.y = n2.position.y - n1.position.y;
    
    float distance = std::sqrt(delta.x * delta.x + delta.y * delta.y);
    
    // Spring force: F = k * (distance - ideal_length)
    float displacement = distance - params_.ideal_edge_length;
    float force_magnitude = params_.attraction_strength * displacement;
    
    if (distance < 0.01f) {
        return Vec2(0.0f, 0.0f);
    }
    
    Vec2 result;
    result.x = (delta.x / distance) * force_magnitude;
    result.y = (delta.y / distance) * force_magnitude;
    return result;
}

void ForceDirectedLayout::apply_forces(std::vector<ForceNode>& nodes, float dt) {
    for (auto& node : nodes) {
        // Update velocity
        node.velocity.x += node.force.x * dt;
        node.velocity.y += node.force.y * dt;
        node.velocity.x *= params_.damping;
        node.velocity.y *= params_.damping;
        
        // Update position
        node.position.x += node.velocity.x * dt;
        node.position.y += node.velocity.y * dt;
    }
}

bool ForceDirectedLayout::has_converged(const std::vector<ForceNode>& nodes) {
    float max_velocity = 0.0f;
    
    for (const auto& node : nodes) {
        float velocity_magnitude = std::sqrt(node.velocity.x * node.velocity.x + 
                                            node.velocity.y * node.velocity.y);
        max_velocity = std::max(max_velocity, velocity_magnitude);
    }
    
    return max_velocity < params_.convergence_threshold;
}

} // namespace ddf
} // namespace whiteboard
