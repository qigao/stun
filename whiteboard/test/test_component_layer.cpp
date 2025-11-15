#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/component_layer.h>
#include <whiteboard/ddf/shape_layer.h>

using namespace whiteboard::ddf;

// ============================================================================
// Component Registration Tests
// ============================================================================

TEST_CASE("ComponentLayer registers components", "[component_layer][registration]") {
    ComponentLayer component_layer;
    
    ComponentDefinition component;
    component.id = "test_component";
    component.name = "Test Component";
    
    component_layer.register_component(component);
    
    auto* retrieved = component_layer.get_component("test_component");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->id == "test_component");
    REQUIRE(retrieved->name == "Test Component");
}

TEST_CASE("ComponentLayer unregisters components", "[component_layer][registration]") {
    ComponentLayer component_layer;
    
    ComponentDefinition component;
    component.id = "test_component";
    component.name = "Test Component";
    
    component_layer.register_component(component);
    REQUIRE(component_layer.get_component("test_component") != nullptr);
    
    component_layer.unregister_component("test_component");
    REQUIRE(component_layer.get_component("test_component") == nullptr);
}

TEST_CASE("ComponentLayer retrieves all components", "[component_layer][registration]") {
    ComponentLayer component_layer;
    
    ComponentDefinition comp1;
    comp1.id = "component1";
    comp1.name = "Component 1";
    
    ComponentDefinition comp2;
    comp2.id = "component2";
    comp2.name = "Component 2";
    
    component_layer.register_component(comp1);
    component_layer.register_component(comp2);
    
    auto components = component_layer.get_all_components();
    REQUIRE(components.size() == 2);
}

TEST_CASE("ComponentLayer handles component with parameters", "[component_layer][registration]") {
    ComponentLayer component_layer;
    
    ComponentDefinition component;
    component.id = "parameterized_component";
    component.name = "Parameterized Component";
    
    ComponentParameter param1;
    param1.name = "color";
    param1.type = "color";
    param1.default_value = "#3498db";
    
    ComponentParameter param2;
    param2.name = "size";
    param2.type = "number";
    param2.default_value = "100";
    
    component.parameters.push_back(param1);
    component.parameters.push_back(param2);
    
    component_layer.register_component(component);
    
    auto* retrieved = component_layer.get_component("parameterized_component");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->parameters.size() == 2);
    REQUIRE(retrieved->parameters[0].name == "color");
    REQUIRE(retrieved->parameters[1].name == "size");
}

// ============================================================================
// Component Instantiation Tests
// ============================================================================

TEST_CASE("ComponentLayer creates component instances", "[component_layer][instantiation]") {
    ComponentLayer component_layer;
    
    // Register a component
    ComponentDefinition component;
    component.id = "test_component";
    component.name = "Test Component";
    component_layer.register_component(component);
    
    // Create instance
    std::map<std::string, std::string> params;
    std::string instance_id = component_layer.create_instance("test_component", params, 100.0f, 200.0f);
    
    REQUIRE(!instance_id.empty());
    
    auto* instance = component_layer.get_instance(instance_id);
    REQUIRE(instance != nullptr);
    REQUIRE(instance->component_id == "test_component");
    REQUIRE(instance->position_x == 100.0f);
    REQUIRE(instance->position_y == 200.0f);
}

TEST_CASE("ComponentLayer creates instance with parameters", "[component_layer][instantiation]") {
    ComponentLayer component_layer;
    
    // Register component with parameters
    ComponentDefinition component;
    component.id = "card_component";
    component.name = "Card Component";
    
    ComponentParameter param1;
    param1.name = "title";
    param1.type = "string";
    param1.default_value = "Default Title";
    
    ComponentParameter param2;
    param2.name = "color";
    param2.type = "color";
    param2.default_value = "#ffffff";
    
    component.parameters.push_back(param1);
    component.parameters.push_back(param2);
    component_layer.register_component(component);
    
    // Create instance with custom parameters
    std::map<std::string, std::string> params;
    params["title"] = "My Card";
    params["color"] = "#3498db";
    
    std::string instance_id = component_layer.create_instance("card_component", params, 0.0f, 0.0f);
    
    auto* instance = component_layer.get_instance(instance_id);
    REQUIRE(instance != nullptr);
    REQUIRE(instance->parameters["title"] == "My Card");
    REQUIRE(instance->parameters["color"] == "#3498db");
}

TEST_CASE("ComponentLayer fills default parameter values", "[component_layer][instantiation]") {
    ComponentLayer component_layer;
    
    // Register component with parameters
    ComponentDefinition component;
    component.id = "test_component";
    
    ComponentParameter param1;
    param1.name = "param1";
    param1.type = "string";
    param1.default_value = "default1";
    
    ComponentParameter param2;
    param2.name = "param2";
    param2.type = "string";
    param2.default_value = "default2";
    
    component.parameters.push_back(param1);
    component.parameters.push_back(param2);
    component_layer.register_component(component);
    
    // Create instance with only one parameter
    std::map<std::string, std::string> params;
    params["param1"] = "custom1";
    
    std::string instance_id = component_layer.create_instance("test_component", params, 0.0f, 0.0f);
    
    auto* instance = component_layer.get_instance(instance_id);
    REQUIRE(instance->parameters["param1"] == "custom1");
    REQUIRE(instance->parameters["param2"] == "default2");  // Should use default
}

TEST_CASE("ComponentLayer throws on invalid component", "[component_layer][instantiation]") {
    ComponentLayer component_layer;
    
    std::map<std::string, std::string> params;
    REQUIRE_THROWS_AS(
        component_layer.create_instance("nonexistent_component", params, 0.0f, 0.0f),
        std::runtime_error
    );
}

TEST_CASE("ComponentLayer removes instances", "[component_layer][instantiation]") {
    ComponentLayer component_layer;
    
    ComponentDefinition component;
    component.id = "test_component";
    component_layer.register_component(component);
    
    std::map<std::string, std::string> params;
    std::string instance_id = component_layer.create_instance("test_component", params, 0.0f, 0.0f);
    
    REQUIRE(component_layer.get_instance(instance_id) != nullptr);
    
    component_layer.remove_instance(instance_id);
    REQUIRE(component_layer.get_instance(instance_id) == nullptr);
}

TEST_CASE("ComponentLayer retrieves all instances", "[component_layer][instantiation]") {
    ComponentLayer component_layer;
    
    ComponentDefinition component;
    component.id = "test_component";
    component_layer.register_component(component);
    
    std::map<std::string, std::string> params;
    component_layer.create_instance("test_component", params, 0.0f, 0.0f);
    component_layer.create_instance("test_component", params, 100.0f, 100.0f);
    
    auto instances = component_layer.get_all_instances();
    REQUIRE(instances.size() == 2);
}

// ============================================================================
// Parameter Substitution Tests
// ============================================================================

TEST_CASE("ComponentLayer substitutes parameters in shapes", "[component_layer][parameter_substitution]") {
    ComponentLayer component_layer;
    
    // Create component with shape template
    ComponentDefinition component;
    component.id = "text_card";
    
    ComponentParameter param;
    param.name = "title";
    param.type = "string";
    param.default_value = "Default";
    component.parameters.push_back(param);
    
    Shape template_shape;
    template_shape.id = "text_shape";
    template_shape.type = "text";
    template_shape.text = "{{title}}";
    template_shape.geometry["x"] = 0.0f;
    template_shape.geometry["y"] = 0.0f;
    
    component.shapes.push_back(template_shape);
    component_layer.register_component(component);
    
    // Create instance
    std::map<std::string, std::string> params;
    params["title"] = "Hello World";
    
    std::string instance_id = component_layer.create_instance("text_card", params, 50.0f, 50.0f);
    
    // Instantiate shapes
    auto shapes = component_layer.instantiate_shapes(instance_id);
    
    REQUIRE(shapes.size() == 1);
    REQUIRE(shapes[0].text == "Hello World");
    REQUIRE(shapes[0].geometry["x"] == 50.0f);  // Position offset applied
    REQUIRE(shapes[0].geometry["y"] == 50.0f);
}

TEST_CASE("ComponentLayer substitutes parameters in styles", "[component_layer][parameter_substitution]") {
    ComponentLayer component_layer;
    
    ComponentDefinition component;
    component.id = "colored_rect";
    
    ComponentParameter param;
    param.name = "fill_color";
    param.type = "color";
    param.default_value = "#ffffff";
    component.parameters.push_back(param);
    
    Shape template_shape;
    template_shape.id = "rect_shape";
    template_shape.type = "rect";
    template_shape.inline_style["fill"] = "{{fill_color}}";
    template_shape.geometry["x"] = 0.0f;
    template_shape.geometry["y"] = 0.0f;
    template_shape.geometry["width"] = 100.0f;
    template_shape.geometry["height"] = 50.0f;
    
    component.shapes.push_back(template_shape);
    component_layer.register_component(component);
    
    // Create instance
    std::map<std::string, std::string> params;
    params["fill_color"] = "#3498db";
    
    std::string instance_id = component_layer.create_instance("colored_rect", params, 0.0f, 0.0f);
    
    // Instantiate shapes
    auto shapes = component_layer.instantiate_shapes(instance_id);
    
    REQUIRE(shapes.size() == 1);
    REQUIRE(shapes[0].inline_style["fill"] == "#3498db");
}

TEST_CASE("ComponentLayer handles multiple parameter substitutions", "[component_layer][parameter_substitution]") {
    ComponentLayer component_layer;
    
    ComponentDefinition component;
    component.id = "person_card";
    
    ComponentParameter name_param;
    name_param.name = "name";
    name_param.type = "string";
    name_param.default_value = "Unknown";
    
    ComponentParameter title_param;
    title_param.name = "title";
    title_param.type = "string";
    title_param.default_value = "Employee";
    
    ComponentParameter color_param;
    color_param.name = "color";
    color_param.type = "color";
    color_param.default_value = "#cccccc";
    
    component.parameters.push_back(name_param);
    component.parameters.push_back(title_param);
    component.parameters.push_back(color_param);
    
    // Background shape
    Shape bg_shape;
    bg_shape.id = "background";
    bg_shape.type = "rect";
    bg_shape.inline_style["fill"] = "{{color}}";
    bg_shape.geometry["x"] = 0.0f;
    bg_shape.geometry["y"] = 0.0f;
    bg_shape.geometry["width"] = 120.0f;
    bg_shape.geometry["height"] = 60.0f;
    
    // Name text
    Shape name_shape;
    name_shape.id = "name_text";
    name_shape.type = "text";
    name_shape.text = "{{name}}";
    name_shape.geometry["x"] = 60.0f;
    name_shape.geometry["y"] = 20.0f;
    
    // Title text
    Shape title_shape;
    title_shape.id = "title_text";
    title_shape.type = "text";
    title_shape.text = "{{title}}";
    title_shape.geometry["x"] = 60.0f;
    title_shape.geometry["y"] = 40.0f;
    
    component.shapes.push_back(bg_shape);
    component.shapes.push_back(name_shape);
    component.shapes.push_back(title_shape);
    component_layer.register_component(component);
    
    // Create instance
    std::map<std::string, std::string> params;
    params["name"] = "John Doe";
    params["title"] = "CEO";
    params["color"] = "#e74c3c";
    
    std::string instance_id = component_layer.create_instance("person_card", params, 100.0f, 200.0f);
    
    // Instantiate shapes
    auto shapes = component_layer.instantiate_shapes(instance_id);
    
    REQUIRE(shapes.size() == 3);
    REQUIRE(shapes[0].inline_style["fill"] == "#e74c3c");
    REQUIRE(shapes[1].text == "John Doe");
    REQUIRE(shapes[2].text == "CEO");
    
    // Check position offsets
    REQUIRE(shapes[0].geometry["x"] == 100.0f);
    REQUIRE(shapes[0].geometry["y"] == 200.0f);
    REQUIRE(shapes[1].geometry["x"] == 160.0f);  // 60 + 100
    REQUIRE(shapes[1].geometry["y"] == 220.0f);  // 20 + 200
}

// ============================================================================
// Update Propagation Tests
// ============================================================================

TEST_CASE("ComponentLayer updates instance parameters", "[component_layer][update_propagation]") {
    ComponentLayer component_layer;
    ShapeLayer shape_layer;
    component_layer.set_shape_layer(&shape_layer);
    
    // Create component
    ComponentDefinition component;
    component.id = "test_component";
    
    ComponentParameter param;
    param.name = "value";
    param.type = "string";
    param.default_value = "initial";
    component.parameters.push_back(param);
    
    Shape template_shape;
    template_shape.id = "shape";
    template_shape.type = "text";
    template_shape.text = "{{value}}";
    template_shape.geometry["x"] = 0.0f;
    template_shape.geometry["y"] = 0.0f;
    
    component.shapes.push_back(template_shape);
    component_layer.register_component(component);
    
    // Create instance
    std::map<std::string, std::string> params;
    params["value"] = "first";
    std::string instance_id = component_layer.create_instance("test_component", params, 0.0f, 0.0f);
    
    // Update instance parameters
    std::map<std::string, std::string> new_params;
    new_params["value"] = "updated";
    component_layer.update_instance(instance_id, new_params);
    
    auto* instance = component_layer.get_instance(instance_id);
    REQUIRE(instance->parameters["value"] == "updated");
}

TEST_CASE("ComponentLayer propagates component changes to all instances", "[component_layer][update_propagation]") {
    ComponentLayer component_layer;
    ShapeLayer shape_layer;
    component_layer.set_shape_layer(&shape_layer);
    
    // Create component
    ComponentDefinition component;
    component.id = "test_component";
    
    Shape template_shape;
    template_shape.id = "shape";
    template_shape.type = "rect";
    template_shape.geometry["x"] = 0.0f;
    template_shape.geometry["y"] = 0.0f;
    template_shape.geometry["width"] = 100.0f;
    template_shape.geometry["height"] = 50.0f;
    
    component.shapes.push_back(template_shape);
    component_layer.register_component(component);
    
    // Create multiple instances
    std::map<std::string, std::string> params;
    std::string instance1_id = component_layer.create_instance("test_component", params, 0.0f, 0.0f);
    std::string instance2_id = component_layer.create_instance("test_component", params, 200.0f, 0.0f);
    
    // Modify component
    auto* comp = component_layer.get_component("test_component");
    comp->shapes[0].geometry["width"] = 150.0f;
    
    // Propagate changes
    component_layer.update_all_instances("test_component");
    
    // Verify both instances were updated
    auto* instance1 = component_layer.get_instance(instance1_id);
    auto* instance2 = component_layer.get_instance(instance2_id);
    
    REQUIRE(instance1->generated_shape_ids.size() == 1);
    REQUIRE(instance2->generated_shape_ids.size() == 1);
}

TEST_CASE("ComponentLayer does not propagate to detached instances", "[component_layer][update_propagation]") {
    ComponentLayer component_layer;
    ShapeLayer shape_layer;
    component_layer.set_shape_layer(&shape_layer);
    
    // Create component
    ComponentDefinition component;
    component.id = "test_component";
    
    Shape template_shape;
    template_shape.id = "shape";
    template_shape.type = "rect";
    template_shape.geometry["x"] = 0.0f;
    template_shape.geometry["y"] = 0.0f;
    
    component.shapes.push_back(template_shape);
    component_layer.register_component(component);
    
    // Create instances
    std::map<std::string, std::string> params;
    std::string instance1_id = component_layer.create_instance("test_component", params, 0.0f, 0.0f);
    std::string instance2_id = component_layer.create_instance("test_component", params, 200.0f, 0.0f);
    
    // Detach one instance
    component_layer.detach_instance(instance1_id);
    
    auto* instance1 = component_layer.get_instance(instance1_id);
    auto* instance2 = component_layer.get_instance(instance2_id);
    
    REQUIRE(instance1->detached == true);
    REQUIRE(instance2->detached == false);
    
    // Propagate changes
    size_t instance1_shape_count_before = instance1->generated_shape_ids.size();
    component_layer.update_all_instances("test_component");
    size_t instance1_shape_count_after = instance1->generated_shape_ids.size();
    
    // Detached instance should not be updated
    REQUIRE(instance1_shape_count_before == instance1_shape_count_after);
}

// ============================================================================
// Create Component from Shapes Tests
// ============================================================================

TEST_CASE("ComponentLayer creates component from shapes", "[component_layer][create_from_shapes]") {
    ComponentLayer component_layer;
    ShapeLayer shape_layer;
    component_layer.set_shape_layer(&shape_layer);
    
    // Create shapes
    Shape shape1;
    shape1.id = "shape1";
    shape1.type = "rect";
    shape1.geometry["x"] = 100.0f;
    shape1.geometry["y"] = 200.0f;
    shape1.geometry["width"] = 50.0f;
    shape1.geometry["height"] = 30.0f;
    
    Shape shape2;
    shape2.id = "shape2";
    shape2.type = "text";
    shape2.text = "Label";
    shape2.geometry["x"] = 125.0f;
    shape2.geometry["y"] = 215.0f;
    
    shape_layer.add_shape(shape1);
    shape_layer.add_shape(shape2);
    
    // Create component from shapes
    std::vector<std::string> shape_ids = {"shape1", "shape2"};
    std::vector<ComponentParameter> parameters;
    
    std::string component_id = component_layer.create_component_from_shapes(
        shape_ids,
        "My Component",
        parameters
    );
    
    REQUIRE(!component_id.empty());
    
    auto* component = component_layer.get_component(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->name == "My Component");
    REQUIRE(component->shapes.size() == 2);
    
    // Check that positions are relative to component origin
    REQUIRE(component->shapes[0].geometry["x"] == 0.0f);  // 100 - 100
    REQUIRE(component->shapes[0].geometry["y"] == 0.0f);  // 200 - 200
    REQUIRE(component->shapes[1].geometry["x"] == 25.0f); // 125 - 100
    REQUIRE(component->shapes[1].geometry["y"] == 15.0f); // 215 - 200
}

TEST_CASE("ComponentLayer creates component with parameters from shapes", "[component_layer][create_from_shapes]") {
    ComponentLayer component_layer;
    ShapeLayer shape_layer;
    component_layer.set_shape_layer(&shape_layer);
    
    // Create shape with text
    Shape shape;
    shape.id = "text_shape";
    shape.type = "text";
    shape.text = "Default Text";
    shape.geometry["x"] = 0.0f;
    shape.geometry["y"] = 0.0f;
    
    shape_layer.add_shape(shape);
    
    // Create component with parameter
    std::vector<std::string> shape_ids = {"text_shape"};
    std::vector<ComponentParameter> parameters;
    
    ComponentParameter param;
    param.name = "label";
    param.type = "string";
    param.default_value = "Default Text";
    parameters.push_back(param);
    
    std::string component_id = component_layer.create_component_from_shapes(
        shape_ids,
        "Text Component",
        parameters
    );
    
    auto* component = component_layer.get_component(component_id);
    REQUIRE(component != nullptr);
    REQUIRE(component->parameters.size() == 1);
    REQUIRE(component->parameters[0].name == "label");
    
    // Check that text was parameterized
    REQUIRE(component->shapes[0].text == "{{label}}");
}

TEST_CASE("ComponentLayer throws when creating component with no shapes", "[component_layer][create_from_shapes]") {
    ComponentLayer component_layer;
    ShapeLayer shape_layer;
    component_layer.set_shape_layer(&shape_layer);
    
    std::vector<std::string> shape_ids;
    std::vector<ComponentParameter> parameters;
    
    REQUIRE_THROWS_AS(
        component_layer.create_component_from_shapes(shape_ids, "Empty Component", parameters),
        std::runtime_error
    );
}

TEST_CASE("ComponentLayer throws when creating component without shape layer", "[component_layer][create_from_shapes]") {
    ComponentLayer component_layer;
    // No shape layer set
    
    std::vector<std::string> shape_ids = {"shape1"};
    std::vector<ComponentParameter> parameters;
    
    REQUIRE_THROWS_AS(
        component_layer.create_component_from_shapes(shape_ids, "Component", parameters),
        std::runtime_error
    );
}

// ============================================================================
// Clear and Utility Tests
// ============================================================================

TEST_CASE("ComponentLayer clears all data", "[component_layer][utility]") {
    ComponentLayer component_layer;
    
    // Add components and instances
    ComponentDefinition component;
    component.id = "test_component";
    component_layer.register_component(component);
    
    std::map<std::string, std::string> params;
    component_layer.create_instance("test_component", params, 0.0f, 0.0f);
    
    REQUIRE(component_layer.get_all_components().size() == 1);
    REQUIRE(component_layer.get_all_instances().size() == 1);
    
    component_layer.clear();
    
    REQUIRE(component_layer.get_all_components().size() == 0);
    REQUIRE(component_layer.get_all_instances().size() == 0);
}

TEST_CASE("ComponentLayer generates unique IDs", "[component_layer][utility]") {
    ComponentLayer component_layer;
    
    ComponentDefinition component;
    component.id = "test_component";
    component_layer.register_component(component);
    
    std::map<std::string, std::string> params;
    std::string id1 = component_layer.create_instance("test_component", params, 0.0f, 0.0f);
    std::string id2 = component_layer.create_instance("test_component", params, 0.0f, 0.0f);
    std::string id3 = component_layer.create_instance("test_component", params, 0.0f, 0.0f);
    
    REQUIRE(id1 != id2);
    REQUIRE(id2 != id3);
    REQUIRE(id1 != id3);
}

TEST_CASE("ComponentLayer handles connection points", "[component_layer][utility]") {
    ComponentLayer component_layer;
    
    ComponentDefinition component;
    component.id = "test_component";
    
    ConnectionPoint cp1;
    cp1.id = "top";
    cp1.x_ratio = 0.5f;
    cp1.y_ratio = 0.0f;
    
    ConnectionPoint cp2;
    cp2.id = "bottom";
    cp2.x_ratio = 0.5f;
    cp2.y_ratio = 1.0f;
    
    component.connection_points.push_back(cp1);
    component.connection_points.push_back(cp2);
    
    component_layer.register_component(component);
    
    auto* retrieved = component_layer.get_component("test_component");
    REQUIRE(retrieved->connection_points.size() == 2);
    REQUIRE(retrieved->connection_points[0].id == "top");
    REQUIRE(retrieved->connection_points[1].id == "bottom");
}
