# DDF API Reference

## Overview

This document provides a comprehensive reference for the DDF (Diagram Definition Format) C++ API. The API is organized into layers, each responsible for a specific aspect of diagram functionality.

## Table of Contents

1. [Core Classes](#core-classes)
2. [Data Layer](#data-layer)
3. [Shape Layer](#shape-layer)
4. [Style Layer](#style-layer)
5. [Component Layer](#component-layer)
6. [Connector Layer](#connector-layer)
7. [Event Layer](#event-layer)
8. [Rendering](#rendering)
9. [Utilities](#utilities)

## Core Classes

### DDFDocument

The main document class that coordinates all layers.

```cpp
namespace whiteboard::ddf {

class DDFDocument {
public:
    DDFDocument();
    ~DDFDocument();
    
    // Layer access
    DataLayer& data_layer();
    ShapeLayer& shape_layer();
    ComponentLayer& component_layer();
    ConnectorLayer& connector_layer();
    EventLayer& event_layer();
    StyleLayer& style_layer();
    
    // Serialization
    bool load_from_json(const std::string& json_string);
    std::string save_to_json() const;
    bool load_from_file(const std::string& filepath);
    bool save_to_file(const std::string& filepath) const;
    
    // SVG export/import
    std::string export_to_svg() const;
    bool export_to_svg_file(const std::string& filepath) const;
    bool import_from_svg(const std::string& svg_string);
    
    // Data-driven generation
    void generate_from_data(const std::string& layout_algorithm);
    
    // Rendering
    void render(NVGcontext* ctx, const nanogui::BoundingBox& viewport);
    
    // Validation
    bool validate() const;
    std::vector<std::string> get_validation_errors() const;
    
    // Metadata
    void set_metadata(const std::string& key, const std::string& value);
    std::string get_metadata(const std::string& key) const;
    
private:
    // Implementation details
};

} // namespace whiteboard::ddf
```

#### Example Usage

```cpp
#include "whiteboard/ddf/ddf_document.h"

// Create document
whiteboard::ddf::DDFDocument doc;

// Load from file
if (doc.load_from_file("diagram.json")) {
    // Validate
    if (doc.validate()) {
        // Render
        doc.render(nvg_context, viewport);
        
        // Export to SVG
        doc.export_to_svg_file("output.svg");
    } else {
        auto errors = doc.get_validation_errors();
        for (const auto& error : errors) {
            std::cerr << "Validation error: " << error << std::endl;
        }
    }
}
```

## Data Layer

### DataNode

Represents a data node in the data layer.

```cpp
struct DataNode {
    std::string id;
    std::string type;
    std::map<std::string, std::string> properties;
};
```

### DataRelationship

Represents a relationship between data nodes.

```cpp
struct DataRelationship {
    std::string id;
    std::string type;
    std::string from_node_id;
    std::string to_node_id;
    std::map<std::string, std::string> properties;
};
```

### DataLayer

Manages data nodes and relationships.

```cpp
class DataLayer {
public:
    // Node operations
    void add_node(const DataNode& node);
    void remove_node(const std::string& id);
    void update_node(const std::string& id, 
                     const std::map<std::string, std::string>& properties);
    
    DataNode* get_node(const std::string& id);
    const DataNode* get_node(const std::string& id) const;
    std::vector<DataNode*> get_all_nodes();
    
    // Relationship operations
    void add_relationship(const DataRelationship& rel);
    void remove_relationship(const std::string& id);
    
    DataRelationship* get_relationship(const std::string& id);
    std::vector<DataRelationship> get_relationships_for_node(
        const std::string& node_id) const;
    
    // Query operations
    std::vector<DataNode*> query_nodes(const std::string& type);
    std::vector<DataNode*> query_nodes_by_property(
        const std::string& key, const std::string& value);
    
    // Serialization
    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);
};
```

#### Example Usage

```cpp
// Create data nodes
whiteboard::ddf::DataNode node1;
node1.id = "person1";
node1.type = "person";
node1.properties = {
    {"name", "John Doe"},
    {"title", "CEO"}
};

doc.data_layer().add_node(node1);

// Create relationship
whiteboard::ddf::DataRelationship rel;
rel.id = "rel1";
rel.type = "reports_to";
rel.from_node_id = "person2";
rel.to_node_id = "person1";

doc.data_layer().add_relationship(rel);

// Query nodes
auto people = doc.data_layer().query_nodes("person");
for (auto* person : people) {
    std::cout << person->properties["name"] << std::endl;
}
```

## Shape Layer

### Shape

Represents a visual shape.

```cpp
struct ConnectionPoint {
    std::string id;
    float x_ratio;  // 0-1 relative to shape bounds
    float y_ratio;  // 0-1 relative to shape bounds
};

struct DataBinding {
    std::string node_id;
    std::map<std::string, std::string> property_mappings;
};

struct Shape {
    std::string id;
    std::string type;  // rect, circle, ellipse, path, text, group
    std::optional<DataBinding> data_binding;
    
    // CSS-like styling
    std::vector<std::string> classes;
    std::map<std::string, std::string> inline_style;
    
    // Geometry
    std::map<std::string, float> geometry;
    
    // Connection points
    std::vector<ConnectionPoint> connection_points;
    
    // Transform
    nanogui::Matrix3f transform;
    
    // Hierarchy
    std::vector<std::string> child_shape_ids;
    std::optional<std::string> parent_shape_id;
    
    // Pseudo-states
    std::set<std::string> pseudo_states;
};
```

### ShapeLayer

Manages shapes and their hierarchy.

```cpp
class ShapeLayer {
public:
    // Shape operations
    void add_shape(const Shape& shape);
    void remove_shape(const std::string& id);
    void update_shape(const std::string& id, const Shape& shape);
    
    Shape* get_shape(const std::string& id);
    const Shape* get_shape(const std::string& id) const;
    std::vector<Shape*> get_all_shapes();
    
    // Data binding
    void bind_to_data(const std::string& shape_id, const DataBinding& binding);
    void update_from_data(const std::string& node_id);
    
    // Hierarchy operations
    std::string group_shapes(const std::vector<std::string>& shape_ids,
                            const std::string& group_name);
    void ungroup_shapes(const std::string& group_id);
    std::vector<std::string> get_group_children(const std::string& group_id);
    
    // Boolean operations
    enum class BooleanOperation { Union, Subtract, Intersect, Exclude };
    std::string combine_shapes(const std::vector<std::string>& shape_ids,
                               BooleanOperation op);
    
    // Rendering
    std::string render_to_svg(const std::string& shape_id) const;
    
    // Serialization
    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);
};
```

#### Example Usage

```cpp
// Create a rectangle
whiteboard::ddf::Shape rect;
rect.id = "rect1";
rect.type = "rect";
rect.geometry = {
    {"x", 100},
    {"y", 100},
    {"width", 120},
    {"height", 60}
};
rect.inline_style = {
    {"fill", "#3498db"},
    {"stroke", "#2c3e50"},
    {"stroke-width", "2"}
};
rect.classes = {"card", "primary"};

doc.shape_layer().add_shape(rect);

// Bind to data
whiteboard::ddf::DataBinding binding;
binding.node_id = "person1";
binding.property_mappings = {
    {"text", "{{name}}"},
    {"fill", "{{department | color_map}}"}
};

doc.shape_layer().bind_to_data("rect1", binding);

// Group shapes
auto group_id = doc.shape_layer().group_shapes(
    {"rect1", "text1", "circle1"}, "card_group");
```

## Style Layer

### StyleSheet

CSS-like stylesheet system.

```cpp
struct StyleRule {
    std::string selector;  // ".card", "#shape1", "rect:hover"
    std::map<std::string, std::string> properties;
    int specificity;
};

class StyleSheet {
public:
    // Rule management
    void add_rule(const std::string& selector,
                  const std::map<std::string, std::string>& properties);
    void remove_rule(const std::string& selector);
    
    std::vector<StyleRule> get_matching_rules(const RenderNode& node) const;
    std::map<std::string, std::string> compute_style(const RenderNode& node) const;
    
    // Serialization
    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);
    
private:
    bool matches_selector(const RenderNode& node, 
                         const std::string& selector) const;
    int calculate_specificity(const std::string& selector) const;
};
```

### StyleLayer

Manages stylesheets and computed styles.

```cpp
class StyleLayer {
public:
    // Stylesheet management
    void add_stylesheet(const StyleSheet& stylesheet);
    void remove_stylesheet(const std::string& id);
    StyleSheet* get_stylesheet(const std::string& id);
    
    // Style computation
    std::map<std::string, std::string> compute_style_for_shape(
        const Shape& shape) const;
    
    // Pseudo-state management
    void update_pseudo_state(const std::string& shape_id,
                            const std::string& state, bool active);
    void invalidate_computed_styles();
    
    // Serialization
    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);
};
```

#### Example Usage

```cpp
// Create stylesheet
whiteboard::ddf::StyleSheet stylesheet;

// Add rules
stylesheet.add_rule(".card", {
    {"fill", "#3498db"},
    {"stroke", "#2c3e50"},
    {"stroke-width", "2"}
});

stylesheet.add_rule(".card:hover", {
    {"fill", "#2980b9"},
    {"stroke-width", "3"}
});

doc.style_layer().add_stylesheet(stylesheet);

// Update pseudo-state
doc.style_layer().update_pseudo_state("shape1", "hover", true);
```

## Component Layer

### ComponentDefinition

Defines a reusable component.

```cpp
struct ComponentParameter {
    std::string name;
    std::string type;  // string, number, color, boolean
    std::string default_value;
};

struct ComponentDefinition {
    std::string id;
    std::string name;
    std::vector<ComponentParameter> parameters;
    std::vector<Shape> shapes;
    std::vector<ConnectionPoint> connection_points;
    std::optional<std::string> parent_component_id;
};
```

### ComponentInstance

An instance of a component.

```cpp
struct ComponentInstance {
    std::string id;
    std::string component_id;
    std::map<std::string, std::string> parameters;
    nanogui::Vector2f position;
    std::map<std::string, std::string> overrides;
    bool detached = false;
};
```

### ComponentLayer

Manages components and instances.

```cpp
class ComponentLayer {
public:
    // Component definitions
    void register_component(const ComponentDefinition& component);
    void unregister_component(const std::string& id);
    ComponentDefinition* get_component(const std::string& id);
    std::vector<ComponentDefinition*> get_all_components();
    
    // Component instances
    std::string create_instance(const std::string& component_id,
                               const std::map<std::string, std::string>& parameters,
                               const nanogui::Vector2f& position);
    void remove_instance(const std::string& id);
    void update_instance(const std::string& id,
                        const std::map<std::string, std::string>& parameters);
    
    // Instance management
    void detach_instance(const std::string& id);
    void update_all_instances(const std::string& component_id);
    
    // Component creation
    std::string create_component_from_shapes(
        const std::vector<std::string>& shape_ids,
        const std::string& component_name,
        const std::vector<ComponentParameter>& parameters);
    
    // Shape instantiation
    std::vector<Shape> instantiate_shapes(const std::string& instance_id);
    
    // Serialization
    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);
};
```

#### Example Usage

```cpp
// Define component
whiteboard::ddf::ComponentDefinition person_card;
person_card.id = "person_card";
person_card.name = "Person Card";

person_card.parameters = {
    {"name", "string", ""},
    {"title", "string", ""}
};

whiteboard::ddf::Shape rect;
rect.type = "rect";
rect.geometry = {{"x", 0}, {"y", 0}, {"width", 120}, {"height", 60}};
person_card.shapes.push_back(rect);

doc.component_layer().register_component(person_card);

// Create instance
auto instance_id = doc.component_layer().create_instance(
    "person_card",
    {{"name", "John Doe"}, {"title", "CEO"}},
    {100, 100}
);
```

## Connector Layer

### Connector

Represents a smart connector between shapes.

```cpp
enum class RoutingAlgorithm {
    Straight,
    Orthogonal,
    Bezier,
    CurvedOrthogonal
};

enum class ArrowType {
    None, Arrow, Diamond, Circle, Square
};

struct ConnectorEndpoint {
    std::string shape_id;
    std::string connection_point_id;
};

struct ConnectorRouting {
    RoutingAlgorithm algorithm;
    bool avoid_shapes;
    float padding;
    float corner_radius;
};

struct ConnectorLabel {
    std::string text;
    float position;  // 0-1 along path
    nanogui::Vector2f offset;
};

struct Connector {
    std::string id;
    ConnectorEndpoint from;
    ConnectorEndpoint to;
    ConnectorRouting routing;
    std::map<std::string, std::string> style;
    ArrowType arrow_start;
    ArrowType arrow_end;
    std::optional<ConnectorLabel> label;
    std::vector<nanogui::Vector2f> path_points;  // Computed
};
```

### ConnectorLayer

Manages connectors and routing.

```cpp
class ConnectorLayer {
public:
    // Connector operations
    void add_connector(const Connector& connector);
    void remove_connector(const std::string& id);
    void update_connector(const std::string& id, const Connector& connector);
    
    Connector* get_connector(const std::string& id);
    std::vector<Connector*> get_connectors_for_shape(const std::string& shape_id);
    
    // Routing
    void compute_path(const std::string& connector_id);
    void recompute_all_paths();
    void recompute_paths_for_shape(const std::string& shape_id);
    
    // Rendering
    std::string render_to_svg(const std::string& connector_id) const;
    
    // Serialization
    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);
    
private:
    std::vector<nanogui::Vector2f> route_straight(
        const ConnectorEndpoint& from, const ConnectorEndpoint& to);
    std::vector<nanogui::Vector2f> route_orthogonal(
        const ConnectorEndpoint& from, const ConnectorEndpoint& to,
        bool avoid_shapes);
    std::vector<nanogui::Vector2f> route_bezier(
        const ConnectorEndpoint& from, const ConnectorEndpoint& to);
};
```

#### Example Usage

```cpp
// Create connector
whiteboard::ddf::Connector conn;
conn.id = "conn1";
conn.from = {"shape1", "bottom"};
conn.to = {"shape2", "top"};
conn.routing = {
    whiteboard::ddf::RoutingAlgorithm::Orthogonal,
    true,  // avoid_shapes
    10.0f, // padding
    5.0f   // corner_radius
};
conn.arrow_end = whiteboard::ddf::ArrowType::Arrow;

doc.connector_layer().add_connector(conn);

// Compute path
doc.connector_layer().compute_path("conn1");

// Recompute when shape moves
doc.connector_layer().recompute_paths_for_shape("shape1");
```

## Event Layer

### Event

Represents an interactive event.

```cpp
enum class EventTrigger {
    Click, DoubleClick, RightClick,
    Hover, HoverEnd,
    DragStart, Drag, DragEnd,
    Select, Deselect
};

enum class ActionType {
    UpdateData, UpdateStyle, UpdateGeometry,
    ShowTooltip, Navigate, ExecuteScript, EmitCustomEvent
};

struct EventAction {
    ActionType type;
    std::map<std::string, std::string> parameters;
};

struct Event {
    std::string id;
    std::string target_id;
    EventTrigger trigger;
    std::vector<EventAction> actions;
    std::optional<std::string> condition;
};
```

### EventLayer

Manages events and handles triggers.

```cpp
class EventLayer {
public:
    // Event registration
    void register_event(const Event& event);
    void unregister_event(const std::string& id);
    
    std::vector<Event*> get_events_for_target(const std::string& target_id);
    std::vector<Event*> get_events_for_trigger(EventTrigger trigger);
    
    // Event handling
    void handle_event(const std::string& target_id, EventTrigger trigger);
    
    // Serialization
    nlohmann::json to_json() const;
    void from_json(const nlohmann::json& j);
    
private:
    void execute_action(const EventAction& action);
    bool evaluate_condition(const std::string& condition);
};
```

#### Example Usage

```cpp
// Create event
whiteboard::ddf::Event event;
event.id = "click_event";
event.target_id = "shape1";
event.trigger = whiteboard::ddf::EventTrigger::Click;

whiteboard::ddf::EventAction action;
action.type = whiteboard::ddf::ActionType::UpdateStyle;
action.parameters = {
    {"shape_id", "shape1"},
    {"property", "fill"},
    {"value", "#e74c3c"}
};
event.actions.push_back(action);

doc.event_layer().register_event(event);

// Trigger event
doc.event_layer().handle_event("shape1", 
    whiteboard::ddf::EventTrigger::Click);
```

## Rendering

### RenderNode

CSS-like render tree node.

```cpp
class RenderNode {
public:
    // Identity
    std::string id;
    std::string type;
    std::vector<std::string> classes;
    std::set<std::string> pseudo_states;
    
    // Tree structure
    RenderNode* parent = nullptr;
    std::vector<std::unique_ptr<RenderNode>> children;
    
    // Styles
    std::map<std::string, std::string> inline_style;
    std::map<std::string, std::string> computed_style;
    
    // Geometry
    std::map<std::string, float> geometry;
    
    // Transform
    nanogui::Matrix3f local_transform;
    nanogui::Matrix3f world_transform;
    
    // Bounding box
    nanogui::BoundingBox bounds;
    
    // Methods
    void compute_styles(const StyleSheet& stylesheet);
    void compute_layout();
    void compute_transforms();
    void compute_bounds();
    void render(NVGcontext* ctx);
    
    // Queries
    RenderNode* query_selector(const std::string& selector);
    std::vector<RenderNode*> query_selector_all(const std::string& selector);
};
```

### RenderingPipeline

Manages the rendering process.

```cpp
class RenderingPipeline {
public:
    RenderingPipeline(NVGcontext* ctx);
    
    void render(DDFDocument& doc);
    void set_viewport(const nanogui::BoundingBox& viewport);
    
private:
    std::unique_ptr<RenderNode> build_render_tree(DDFDocument& doc);
    std::vector<RenderNode*> cull_viewport(RenderNode* root,
                                          const nanogui::BoundingBox& viewport);
    
    NVGcontext* nvg_context_;
    nanogui::BoundingBox viewport_;
};
```

## Utilities

### ExpressionParser

Parses and evaluates expressions.

```cpp
class ExpressionParser {
public:
    static bool is_expression(const std::string& str);
    
    std::string evaluate(const std::string& expression,
                        const EvaluationContext& context);
    
    struct EvaluationContext {
        const DataNode* data_node = nullptr;
        const Shape* shape = nullptr;
        const Shape* parent_shape = nullptr;
        int index = 0;
        nanogui::Vector2f canvas_size;
        bool selected = false;
    };
};
```

### LayoutAlgorithms

Layout generation algorithms.

```cpp
class LayoutAlgorithms {
public:
    static void apply_tree_layout(DDFDocument& doc,
                                  const std::string& root_node_id,
                                  float horizontal_spacing,
                                  float vertical_spacing);
    
    static void apply_force_directed_layout(DDFDocument& doc,
                                           int iterations,
                                           float repulsion,
                                           float attraction);
    
    static void apply_grid_layout(DDFDocument& doc,
                                 int columns,
                                 float horizontal_spacing,
                                 float vertical_spacing);
};
```

### SVGRenderer

Exports DDF to SVG.

```cpp
class SVGRenderer {
public:
    std::string render(const DDFDocument& doc);
    std::string render_shape(const Shape& shape);
    
private:
    std::string generate_svg_element(const Shape& shape);
    std::string generate_svg_group(const Shape& group);
    std::string generate_svg_connector(const Connector& connector);
};
```

### SVGImporter

Imports SVG to DDF.

```cpp
class SVGImporter {
public:
    bool import(DDFDocument& doc, const std::string& svg_string);
    
private:
    Shape convert_svg_element(const pugi::xml_node& node);
    std::vector<Shape> convert_svg_group(const pugi::xml_node& group);
};
```

### Validator

Validates DDF documents.

```cpp
class Validator {
public:
    bool validate(const DDFDocument& doc);
    std::vector<std::string> get_errors() const;
    
private:
    void check_reference_integrity(const DDFDocument& doc);
    void check_circular_dependencies(const DDFDocument& doc);
    void check_data_bindings(const DDFDocument& doc);
};
```

## Error Handling

All API methods that can fail return `bool` or throw exceptions:

```cpp
try {
    if (!doc.load_from_file("diagram.json")) {
        std::cerr << "Failed to load document" << std::endl;
    }
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```

## Thread Safety

The DDF API is **not thread-safe** by default. If you need to access a `DDFDocument` from multiple threads, use external synchronization:

```cpp
std::mutex doc_mutex;

// Thread 1
{
    std::lock_guard<std::mutex> lock(doc_mutex);
    doc.data_layer().add_node(node);
}

// Thread 2
{
    std::lock_guard<std::mutex> lock(doc_mutex);
    doc.render(ctx, viewport);
}
```

## Performance Considerations

- **Caching**: Computed styles and paths are cached
- **Viewport Culling**: Only visible shapes are rendered
- **Incremental Updates**: Only affected elements are recomputed
- **Spatial Indexing**: Used for efficient collision detection

## Next Steps

- Review [User Guide](USER_GUIDE.md)
- Study [Examples](../../examples/ddf/)
- Learn about [CSS Styling](CSS_STYLING.md)
- Understand [Expression Language](EXPRESSIONS.md)
