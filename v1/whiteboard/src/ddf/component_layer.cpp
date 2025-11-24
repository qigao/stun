#include "whiteboard/ddf/component_layer.h"
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <limits>

namespace whiteboard {
namespace ddf {

// Component definitions

void ComponentLayer::register_component(const ComponentDefinition& component) {
    components_[component.id] = component;
}

void ComponentLayer::unregister_component(const std::string& id) {
    components_.erase(id);
}

ComponentDefinition* ComponentLayer::get_component(const std::string& id) {
    auto it = components_.find(id);
    return it != components_.end() ? &it->second : nullptr;
}

const ComponentDefinition* ComponentLayer::get_component(const std::string& id) const {
    auto it = components_.find(id);
    return it != components_.end() ? &it->second : nullptr;
}

std::vector<ComponentDefinition*> ComponentLayer::get_all_components() {
    std::vector<ComponentDefinition*> result;
    result.reserve(components_.size());
    for (auto& pair : components_) {
        result.push_back(&pair.second);
    }
    return result;
}

std::vector<const ComponentDefinition*> ComponentLayer::get_all_components() const {
    std::vector<const ComponentDefinition*> result;
    result.reserve(components_.size());
    for (const auto& pair : components_) {
        result.push_back(&pair.second);
    }
    return result;
}

// Component instances

std::string ComponentLayer::create_instance(const std::string& component_id, 
                                           const std::map<std::string, std::string>& parameters,
                                           float position_x, float position_y) {
    // Verify component exists
    auto* component = get_component(component_id);
    if (!component) {
        throw std::runtime_error("Component not found: " + component_id);
    }
    
    // Create instance
    ComponentInstance instance;
    instance.id = generate_id("instance");
    instance.component_id = component_id;
    instance.parameters = parameters;
    instance.position_x = position_x;
    instance.position_y = position_y;
    instance.detached = false;
    
    // Fill in default parameter values for missing parameters
    for (const auto& param : component->parameters) {
        if (instance.parameters.find(param.name) == instance.parameters.end()) {
            instance.parameters[param.name] = param.default_value;
        }
    }
    
    instances_[instance.id] = instance;
    
    return instance.id;
}

void ComponentLayer::remove_instance(const std::string& id) {
    // Remove generated shapes if shape layer is available
    auto it = instances_.find(id);
    if (it != instances_.end() && shape_layer_) {
        for (const auto& shape_id : it->second.generated_shape_ids) {
            shape_layer_->remove_shape(shape_id);
        }
    }
    
    instances_.erase(id);
}

void ComponentLayer::update_instance(const std::string& id, 
                                     const std::map<std::string, std::string>& parameters) {
    auto* instance = get_instance(id);
    if (!instance) {
        throw std::runtime_error("Instance not found: " + id);
    }
    
    // Update parameters
    for (const auto& [key, value] : parameters) {
        instance->parameters[key] = value;
    }
    
    // Regenerate shapes if shape layer is available
    if (shape_layer_) {
        // Remove old shapes
        for (const auto& shape_id : instance->generated_shape_ids) {
            shape_layer_->remove_shape(shape_id);
        }
        instance->generated_shape_ids.clear();
        
        // Generate new shapes
        auto shapes = instantiate_shapes(id);
        for (const auto& shape : shapes) {
            shape_layer_->add_shape(shape);
            instance->generated_shape_ids.push_back(shape.id);
        }
    }
}

ComponentInstance* ComponentLayer::get_instance(const std::string& id) {
    auto it = instances_.find(id);
    return it != instances_.end() ? &it->second : nullptr;
}

const ComponentInstance* ComponentLayer::get_instance(const std::string& id) const {
    auto it = instances_.find(id);
    return it != instances_.end() ? &it->second : nullptr;
}

std::vector<ComponentInstance*> ComponentLayer::get_all_instances() {
    std::vector<ComponentInstance*> result;
    result.reserve(instances_.size());
    for (auto& pair : instances_) {
        result.push_back(&pair.second);
    }
    return result;
}

std::vector<const ComponentInstance*> ComponentLayer::get_all_instances() const {
    std::vector<const ComponentInstance*> result;
    result.reserve(instances_.size());
    for (const auto& pair : instances_) {
        result.push_back(&pair.second);
    }
    return result;
}

// Instance management

void ComponentLayer::detach_instance(const std::string& id) {
    auto* instance = get_instance(id);
    if (instance) {
        instance->detached = true;
    }
}

void ComponentLayer::update_all_instances(const std::string& component_id) {
    if (!shape_layer_) {
        return;
    }
    
    // Find all instances of this component
    for (auto& [instance_id, instance] : instances_) {
        if (instance.component_id == component_id && !instance.detached) {
            // Regenerate shapes for this instance
            // Remove old shapes
            for (const auto& shape_id : instance.generated_shape_ids) {
                shape_layer_->remove_shape(shape_id);
            }
            instance.generated_shape_ids.clear();
            
            // Generate new shapes
            auto shapes = instantiate_shapes(instance_id);
            for (const auto& shape : shapes) {
                shape_layer_->add_shape(shape);
                instance.generated_shape_ids.push_back(shape.id);
            }
        }
    }
}

// Combine shapes into component

std::string ComponentLayer::create_component_from_shapes(
    const std::vector<std::string>& shape_ids,
    const std::string& component_name,
    const std::vector<ComponentParameter>& parameters) {
    
    if (!shape_layer_) {
        throw std::runtime_error("Shape layer not set");
    }
    
    if (shape_ids.empty()) {
        throw std::runtime_error("No shapes provided");
    }
    
    // Create component definition
    ComponentDefinition component;
    component.id = generate_id("component");
    component.name = component_name;
    component.parameters = parameters;
    
    // Calculate bounding box to determine relative positions
    float min_x = std::numeric_limits<float>::max();
    float min_y = std::numeric_limits<float>::max();
    
    std::vector<Shape*> shapes;
    for (const auto& shape_id : shape_ids) {
        Shape* shape = shape_layer_->get_shape(shape_id);
        if (shape) {
            shapes.push_back(shape);
            
            // Update bounding box
            if (shape->geometry.count("x")) {
                min_x = std::min(min_x, shape->geometry["x"]);
            }
            if (shape->geometry.count("y")) {
                min_y = std::min(min_y, shape->geometry["y"]);
            }
        }
    }
    
    if (shapes.empty()) {
        throw std::runtime_error("No valid shapes found");
    }
    
    // Convert shapes to relative positions
    for (Shape* shape : shapes) {
        Shape template_shape = *shape;
        
        // Make positions relative to component origin
        if (template_shape.geometry.count("x")) {
            template_shape.geometry["x"] -= min_x;
        }
        if (template_shape.geometry.count("y")) {
            template_shape.geometry["y"] -= min_y;
        }
        
        // Extract parameters from shape properties
        // Look for values that match parameter names and replace with {{param}}
        for (const auto& param : parameters) {
            // Check text content
            if (!template_shape.text.empty()) {
                // If text matches a parameter name, replace with template
                if (template_shape.text == param.default_value) {
                    template_shape.text = "{{" + param.name + "}}";
                }
            }
            
            // Check inline styles
            for (auto& [key, value] : template_shape.inline_style) {
                if (value == param.default_value) {
                    template_shape.inline_style[key] = "{{" + param.name + "}}";
                }
            }
        }
        
        // Copy connection points from original shape
        component.connection_points.insert(
            component.connection_points.end(),
            template_shape.connection_points.begin(),
            template_shape.connection_points.end()
        );
        
        component.shapes.push_back(template_shape);
    }
    
    // Register the component
    register_component(component);
    
    return component.id;
}

// Rendering - generate shapes from component instance

std::vector<Shape> ComponentLayer::instantiate_shapes(const std::string& instance_id) {
    auto* instance = get_instance(instance_id);
    if (!instance) {
        throw std::runtime_error("Instance not found: " + instance_id);
    }
    
    auto* component = get_component(instance->component_id);
    if (!component) {
        throw std::runtime_error("Component not found: " + instance->component_id);
    }
    
    std::vector<Shape> result;
    result.reserve(component->shapes.size());
    
    // Instantiate each shape from the component
    for (const auto& template_shape : component->shapes) {
        Shape shape = substitute_shape_parameters(
            template_shape,
            instance->parameters,
            instance->position_x,
            instance->position_y
        );
        
        // Apply overrides
        for (const auto& [key, value] : instance->overrides) {
            // Parse override key (e.g., "shape1.fill" or "geometry.x")
            size_t dot_pos = key.find('.');
            if (dot_pos != std::string::npos) {
                std::string property_type = key.substr(0, dot_pos);
                std::string property_name = key.substr(dot_pos + 1);
                
                if (property_type == "style") {
                    shape.inline_style[property_name] = value;
                } else if (property_type == "geometry") {
                    try {
                        shape.geometry[property_name] = std::stof(value);
                    } catch (...) {
                        // Ignore invalid numeric values
                    }
                }
            }
        }
        
        result.push_back(shape);
    }
    
    return result;
}

// Clear all components and instances

void ComponentLayer::clear() {
    components_.clear();
    instances_.clear();
    id_counter_ = 0;
}

// Set shape layer for shape operations

void ComponentLayer::set_shape_layer(ShapeLayer* shape_layer) {
    shape_layer_ = shape_layer;
}

// Helper methods

std::string ComponentLayer::generate_id(const std::string& prefix) const {
    return prefix + "_" + std::to_string(++id_counter_);
}

std::string ComponentLayer::substitute_parameter(const std::string& value, 
                                                 const std::map<std::string, std::string>& parameters) const {
    std::string result = value;
    
    // Look for {{parameter_name}} patterns
    size_t start_pos = 0;
    while ((start_pos = result.find("{{", start_pos)) != std::string::npos) {
        size_t end_pos = result.find("}}", start_pos);
        if (end_pos == std::string::npos) {
            break;
        }
        
        // Extract parameter name
        std::string param_name = result.substr(start_pos + 2, end_pos - start_pos - 2);
        
        // Trim whitespace
        param_name.erase(0, param_name.find_first_not_of(" \t\n\r"));
        param_name.erase(param_name.find_last_not_of(" \t\n\r") + 1);
        
        // Look up parameter value
        auto it = parameters.find(param_name);
        if (it != parameters.end()) {
            result.replace(start_pos, end_pos - start_pos + 2, it->second);
            start_pos += it->second.length();
        } else {
            // Parameter not found, leave as is
            start_pos = end_pos + 2;
        }
    }
    
    return result;
}

Shape ComponentLayer::substitute_shape_parameters(const Shape& template_shape,
                                                  const std::map<std::string, std::string>& parameters,
                                                  float offset_x, float offset_y) const {
    Shape shape = template_shape;
    
    // Generate unique ID for this shape instance
    shape.id = generate_id(template_shape.id.empty() ? "shape" : template_shape.id);
    
    // Substitute parameters in text content
    if (!shape.text.empty()) {
        shape.text = substitute_parameter(shape.text, parameters);
    }
    
    // Substitute parameters in inline styles
    for (auto& [key, value] : shape.inline_style) {
        shape.inline_style[key] = substitute_parameter(value, parameters);
    }
    
    // Apply position offset to geometry
    if (shape.geometry.count("x")) {
        shape.geometry["x"] += offset_x;
    }
    if (shape.geometry.count("y")) {
        shape.geometry["y"] += offset_y;
    }
    
    // Handle different parameter types for geometry properties
    // Look for {{parameter}} patterns in geometry that need substitution
    std::map<std::string, float> geometry_updates;
    for (const auto& [key, value] : parameters) {
        // Check if this parameter should update geometry
        // For now, we'll handle direct numeric parameters
        if (key.find("width") != std::string::npos || 
            key.find("height") != std::string::npos ||
            key.find("radius") != std::string::npos ||
            key.find("size") != std::string::npos) {
            try {
                float numeric_value = std::stof(value);
                geometry_updates[key] = numeric_value;
            } catch (...) {
                // Not a numeric value, skip
            }
        }
    }
    
    // Apply geometry updates
    for (const auto& [key, value] : geometry_updates) {
        shape.geometry[key] = value;
    }
    
    return shape;
}

} // namespace ddf
} // namespace whiteboard
