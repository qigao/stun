#include <whiteboard/ddf/shape_layer.h>
#include <whiteboard/ddf/data_layer.h>
#include <algorithm>

namespace whiteboard {
namespace ddf {

void ShapeLayer::add_shape(const Shape& shape) {
    shapes_[shape.id] = shape;
    // If the shape has a data binding with a node_id, mark it as having data binding
    if (!shape.data_binding.node_id.empty()) {
        shapes_[shape.id].has_data_binding = true;
    }
}

void ShapeLayer::remove_shape(const std::string& id) {
    shapes_.erase(id);
}

void ShapeLayer::update_shape(const std::string& id, const Shape& shape) {
    shapes_[id] = shape;
}

Shape* ShapeLayer::get_shape(const std::string& id) {
    auto it = shapes_.find(id);
    return it != shapes_.end() ? &it->second : nullptr;
}

const Shape* ShapeLayer::get_shape(const std::string& id) const {
    auto it = shapes_.find(id);
    return it != shapes_.end() ? &it->second : nullptr;
}

std::vector<Shape*> ShapeLayer::get_all_shapes() {
    std::vector<Shape*> result;
    result.reserve(shapes_.size());
    for (auto& [id, shape] : shapes_) {
        result.push_back(&shape);
    }
    return result;
}

std::vector<const Shape*> ShapeLayer::get_all_shapes() const {
    std::vector<const Shape*> result;
    result.reserve(shapes_.size());
    for (const auto& [id, shape] : shapes_) {
        result.push_back(&shape);
    }
    return result;
}

void ShapeLayer::bind_to_data(const std::string& shape_id, const DataBinding& binding) {
    auto* shape = get_shape(shape_id);
    if (shape) {
        shape->data_binding = binding;
        shape->has_data_binding = true;
    }
}

void ShapeLayer::update_from_data(const std::string& node_id) {
    if (!data_layer_) return;
    
    // Find all shapes bound to this data node
    for (auto& [id, shape] : shapes_) {
        if (shape.has_data_binding && 
            shape.data_binding.node_id == node_id) {
            
            // Get data node
            auto* data_node = data_layer_->get_node(node_id);
            if (!data_node) continue;
            
            // Update shape properties from data
            for (const auto& [shape_prop, mapping] : shape.data_binding.property_mappings) {
                std::string value = evaluate_property_mapping(mapping, data_node->properties);
                
                // Update different shape properties based on the mapping
                if (shape_prop == "text") {
                    shape.text = value;
                } else if (shape_prop == "fill" || shape_prop == "stroke" || 
                          shape_prop == "stroke-width" || shape_prop == "opacity") {
                    shape.inline_style[shape_prop] = value;
                } else {
                    // Try to parse as geometry property
                    try {
                        shape.geometry[shape_prop] = std::stof(value);
                    } catch (...) {
                        // If not a number, store as inline style
                        shape.inline_style[shape_prop] = value;
                    }
                }
            }
        }
    }
}

std::string ShapeLayer::group_shapes(const std::vector<std::string>& shape_ids, const std::string& group_name) {
    std::string group_id = generate_id("group");
    
    Shape group;
    group.id = group_id;
    group.type = "group";
    group.child_shape_ids = shape_ids;
    
    // Update parent references for children
    for (const auto& child_id : shape_ids) {
        auto* child = get_shape(child_id);
        if (child) {
            child->parent_shape_id = group_id;
        }
    }
    
    add_shape(group);
    return group_id;
}

void ShapeLayer::ungroup_shapes(const std::string& group_id) {
    auto* group = get_shape(group_id);
    if (!group) return;
    
    // Clear parent references for children
    for (const auto& child_id : group->child_shape_ids) {
        auto* child = get_shape(child_id);
        if (child) {
            child->parent_shape_id = "";
        }
    }
    
    remove_shape(group_id);
}

std::vector<std::string> ShapeLayer::get_group_children(const std::string& group_id) const {
    auto* group = get_shape(group_id);
    return group ? group->child_shape_ids : std::vector<std::string>();
}

std::vector<Shape*> ShapeLayer::get_root_shapes() {
    std::vector<Shape*> result;
    for (auto& [id, shape] : shapes_) {
        if (shape.parent_shape_id.empty()) {
            result.push_back(&shape);
        }
    }
    return result;
}

std::vector<const Shape*> ShapeLayer::get_root_shapes() const {
    std::vector<const Shape*> result;
    for (const auto& [id, shape] : shapes_) {
        if (shape.parent_shape_id.empty()) {
            result.push_back(&shape);
        }
    }
    return result;
}

std::vector<Shape*> ShapeLayer::get_children(const std::string& shape_id) {
    std::vector<Shape*> result;
    auto* shape = get_shape(shape_id);
    if (shape) {
        for (const auto& child_id : shape->child_shape_ids) {
            auto* child = get_shape(child_id);
            if (child) {
                result.push_back(child);
            }
        }
    }
    return result;
}

std::vector<const Shape*> ShapeLayer::get_children(const std::string& shape_id) const {
    std::vector<const Shape*> result;
    auto* shape = get_shape(shape_id);
    if (shape) {
        for (const auto& child_id : shape->child_shape_ids) {
            auto* child = get_shape(child_id);
            if (child) {
                result.push_back(child);
            }
        }
    }
    return result;
}

std::string ShapeLayer::combine_shapes(const std::vector<std::string>& shape_ids, BooleanOperation op) {
    // TODO: Implement boolean operations
    return generate_id("combined");
}

std::string ShapeLayer::render_to_svg(const std::string& shape_id) const {
    // TODO: Implement SVG rendering
    return "";
}

void ShapeLayer::clear() {
    shapes_.clear();
}

std::string ShapeLayer::generate_id(const std::string& prefix) {
    return prefix + "_" + std::to_string(id_counter_++);
}

void ShapeLayer::set_data_layer(DataLayer* data_layer) {
    data_layer_ = data_layer;
}

std::string ShapeLayer::evaluate_property_mapping(
    const std::string& mapping, 
    const std::map<std::string, std::string>& data_properties) const {
    
    // Simple template evaluation: {{property_name}}
    // For now, we'll do basic string replacement
    // A full implementation would use the expression parser
    
    std::string result = mapping;
    
    // Check if it's a template expression
    if (result.size() >= 4 && result.substr(0, 2) == "{{" && result.substr(result.size() - 2) == "}}") {
        // Extract property name
        std::string prop_name = result.substr(2, result.size() - 4);
        
        // Trim whitespace
        prop_name.erase(0, prop_name.find_first_not_of(" \t"));
        prop_name.erase(prop_name.find_last_not_of(" \t") + 1);
        
        // Look up in data properties
        auto it = data_properties.find(prop_name);
        if (it != data_properties.end()) {
            result = it->second;
        } else {
            result = "";  // Property not found
        }
    }
    
    return result;
}

} // namespace ddf
} // namespace whiteboard
