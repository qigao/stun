#pragma once

#include <string>
#include <map>
#include <vector>
#include <set>

namespace whiteboard {
namespace ddf {

/**
 * @brief Connection point for smart connectors
 */
struct ConnectionPoint {
    std::string id;
    float x_ratio;  // 0-1 relative to shape bounds
    float y_ratio;  // 0-1 relative to shape bounds
};

/**
 * @brief Data binding configuration for shapes
 */
struct DataBinding {
    std::string node_id;
    std::map<std::string, std::string> property_mappings;  // shape_prop -> data expression
};

/**
 * @brief Boolean operations for shape combination
 */
enum class BooleanOperation {
    Union,
    Subtract,
    Intersect,
    Exclude
};

/**
 * @brief Represents a visual shape in the diagram
 * 
 * Shapes are organized in a hierarchical tree structure (like DOM/CSS).
 * They support data binding, CSS-like styling, and connection points for connectors.
 */
struct Shape {
    std::string id;
    std::string type;  // rect, circle, ellipse, path, text, group, etc.
    
    // Data binding
    DataBinding data_binding;
    bool has_data_binding = false;
    
    // CSS-like styling
    std::vector<std::string> classes;  // CSS classes
    std::map<std::string, std::string> inline_style;  // Inline styles (highest priority)
    
    // Geometry (type-specific properties)
    std::map<std::string, float> geometry;
    
    // Connection points for connectors
    std::vector<ConnectionPoint> connection_points;
    
    // Transform (stored as 6 values: a, b, c, d, e, f for affine transform)
    std::vector<float> transform;
    
    // Hierarchical tree structure
    std::vector<std::string> child_shape_ids;
    std::string parent_shape_id;  // Empty string means no parent
    
    // Pseudo-states for CSS-like selectors
    std::set<std::string> pseudo_states;  // "hover", "selected", "active", "focus"
    
    // Text content (for text shapes)
    std::string text;
    
    // SVG shape support
    std::string svg_shape_id;  // Reference to shape library (e.g., "network.server")
    std::string svg_data;      // Inline SVG data
    std::map<std::string, std::string> svg_parameters;  // Parameters for parametric shapes
};

/**
 * @brief Shape layer for managing visual primitives
 * 
 * The shape layer defines visual primitives organized in a hierarchical tree structure.
 * It supports data binding, grouping, and boolean operations.
 */
class ShapeLayer {
public:
    ShapeLayer() = default;
    ~ShapeLayer() = default;

    // Shape operations
    void add_shape(const Shape& shape);
    void remove_shape(const std::string& id);
    void update_shape(const std::string& id, const Shape& shape);
    
    Shape* get_shape(const std::string& id);
    const Shape* get_shape(const std::string& id) const;
    
    std::vector<Shape*> get_all_shapes();
    std::vector<const Shape*> get_all_shapes() const;

    // Data binding
    void bind_to_data(const std::string& shape_id, const DataBinding& binding);
    void update_from_data(const std::string& node_id);

    // Shape grouping and hierarchy
    std::string group_shapes(const std::vector<std::string>& shape_ids, const std::string& group_name);
    void ungroup_shapes(const std::string& group_id);
    std::vector<std::string> get_group_children(const std::string& group_id) const;
    
    // Tree traversal
    std::vector<Shape*> get_root_shapes();
    std::vector<const Shape*> get_root_shapes() const;
    
    std::vector<Shape*> get_children(const std::string& shape_id);
    std::vector<const Shape*> get_children(const std::string& shape_id) const;

    // Boolean operations (combine shapes geometrically)
    std::string combine_shapes(const std::vector<std::string>& shape_ids, BooleanOperation op);

    // Rendering
    std::string render_to_svg(const std::string& shape_id) const;

    // Clear all shapes
    void clear();
    
    // Set data layer for data binding updates
    void set_data_layer(class DataLayer* data_layer);

private:
    std::map<std::string, Shape> shapes_;
    class DataLayer* data_layer_ = nullptr;
    
    // Helper to generate unique IDs
    std::string generate_id(const std::string& prefix);
    int id_counter_ = 0;
    
    // Helper to evaluate simple property mappings
    std::string evaluate_property_mapping(const std::string& mapping, 
                                          const std::map<std::string, std::string>& data_properties) const;
};

} // namespace ddf
} // namespace whiteboard
