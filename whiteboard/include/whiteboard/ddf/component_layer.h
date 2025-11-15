#pragma once

#include "shape_layer.h"
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace whiteboard {
namespace ddf {

/**
 * @brief Parameter definition for a component
 */
struct ComponentParameter {
    std::string name;
    std::string type;  // string, number, color, boolean
    std::string default_value;
};

/**
 * @brief Component definition - reusable diagram component template
 * 
 * A component encapsulates shapes, connectors, and behavior as a reusable unit.
 * Components can have parameters that customize their appearance and behavior.
 */
struct ComponentDefinition {
    std::string id;
    std::string name;
    std::vector<ComponentParameter> parameters;
    std::vector<Shape> shapes;
    std::vector<ConnectionPoint> connection_points;
    std::string parent_component_id;  // For inheritance (empty string means no parent)
};

/**
 * @brief Component instance - instantiation of a component with specific parameters
 * 
 * An instance represents a specific use of a component with parameter values.
 * Instances can override specific properties and can be detached from the component.
 */
struct ComponentInstance {
    std::string id;
    std::string component_id;
    std::map<std::string, std::string> parameters;
    float position_x = 0.0f;
    float position_y = 0.0f;
    std::map<std::string, std::string> overrides;  // Override specific shape properties
    bool detached = false;  // If true, no longer linked to component
    
    // Track which shapes were generated from this instance
    std::vector<std::string> generated_shape_ids;
};

/**
 * @brief Component layer for managing reusable diagram components
 * 
 * The component layer enables creation and management of reusable diagram components
 * with parameters. Components can be instantiated multiple times with different
 * parameter values, and updates to components propagate to all instances.
 */
class ComponentLayer {
public:
    ComponentLayer() = default;
    ~ComponentLayer() = default;

    // Component definitions
    void register_component(const ComponentDefinition& component);
    void unregister_component(const std::string& id);
    ComponentDefinition* get_component(const std::string& id);
    const ComponentDefinition* get_component(const std::string& id) const;
    std::vector<ComponentDefinition*> get_all_components();
    std::vector<const ComponentDefinition*> get_all_components() const;

    // Component instances
    std::string create_instance(const std::string& component_id, 
                               const std::map<std::string, std::string>& parameters,
                               float position_x, float position_y);
    void remove_instance(const std::string& id);
    void update_instance(const std::string& id, const std::map<std::string, std::string>& parameters);
    
    ComponentInstance* get_instance(const std::string& id);
    const ComponentInstance* get_instance(const std::string& id) const;
    std::vector<ComponentInstance*> get_all_instances();
    std::vector<const ComponentInstance*> get_all_instances() const;

    // Instance management
    void detach_instance(const std::string& id);  // Break link to component
    void update_all_instances(const std::string& component_id);  // Propagate component changes

    // Combine shapes into component
    std::string create_component_from_shapes(
        const std::vector<std::string>& shape_ids,
        const std::string& component_name,
        const std::vector<ComponentParameter>& parameters
    );

    // Rendering - generate shapes from component instance
    std::vector<Shape> instantiate_shapes(const std::string& instance_id);
    
    // Clear all components and instances
    void clear();
    
    // Set shape layer for shape operations
    void set_shape_layer(ShapeLayer* shape_layer);

private:
    std::map<std::string, ComponentDefinition> components_;
    std::map<std::string, ComponentInstance> instances_;
    ShapeLayer* shape_layer_ = nullptr;
    
    // Helper to generate unique IDs
    std::string generate_id(const std::string& prefix) const;
    mutable int id_counter_ = 0;
    
    // Helper to substitute parameters in shape properties
    std::string substitute_parameter(const std::string& value, 
                                     const std::map<std::string, std::string>& parameters) const;
    
    // Helper to apply parameter substitution to a shape
    Shape substitute_shape_parameters(const Shape& template_shape,
                                     const std::map<std::string, std::string>& parameters,
                                     float offset_x, float offset_y) const;
};

} // namespace ddf
} // namespace whiteboard
