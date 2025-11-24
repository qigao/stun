#include <catch2/catch_test_macros.hpp>
#include <whiteboard/ddf/event_layer.h>
#include <whiteboard/ddf/data_layer.h>
#include <whiteboard/ddf/shape_layer.h>
#include <whiteboard/ddf/expression_parser.h>

using namespace whiteboard::ddf;

// ============================================================================
// Event Registration and Retrieval Tests
// ============================================================================

TEST_CASE("EventLayer registers events", "[event_layer][registration]") {
    EventLayer event_layer;

    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::UpdateStyle;
    action.parameters["shape_id"] = "shape1";
    action.parameters["property"] = "fill";
    action.parameters["value"] = "#ff0000";
    event.actions.push_back(action);

    event_layer.register_event(event);

    auto* retrieved = event_layer.get_event("event1");
    REQUIRE(retrieved != nullptr);
    REQUIRE(retrieved->id == "event1");
    REQUIRE(retrieved->target_id == "shape1");
    REQUIRE(retrieved->trigger == EventTrigger::Click);
    REQUIRE(retrieved->actions.size() == 1);
}

TEST_CASE("EventLayer unregisters events", "[event_layer][registration]") {
    EventLayer event_layer;

    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    event_layer.register_event(event);
    REQUIRE(event_layer.get_event("event1") != nullptr);

    event_layer.unregister_event("event1");
    REQUIRE(event_layer.get_event("event1") == nullptr);
}

TEST_CASE("EventLayer retrieves all events", "[event_layer][registration]") {
    EventLayer event_layer;

    Event event1;
    event1.id = "event1";
    event1.target_id = "shape1";
    event1.trigger = EventTrigger::Click;

    Event event2;
    event2.id = "event2";
    event2.target_id = "shape2";
    event2.trigger = EventTrigger::Hover;

    event_layer.register_event(event1);
    event_layer.register_event(event2);

    auto events = event_layer.get_all_events();
    REQUIRE(events.size() == 2);
}

TEST_CASE("EventLayer retrieves events for target", "[event_layer][registration]") {
    EventLayer event_layer;

    Event event1;
    event1.id = "event1";
    event1.target_id = "shape1";
    event1.trigger = EventTrigger::Click;

    Event event2;
    event2.id = "event2";
    event2.target_id = "shape1";
    event2.trigger = EventTrigger::Hover;

    Event event3;
    event3.id = "event3";
    event3.target_id = "shape2";
    event3.trigger = EventTrigger::Click;

    event_layer.register_event(event1);
    event_layer.register_event(event2);
    event_layer.register_event(event3);

    auto events = event_layer.get_events_for_target("shape1");
    REQUIRE(events.size() == 2);
    REQUIRE(events[0]->target_id == "shape1");
    REQUIRE(events[1]->target_id == "shape1");
}

TEST_CASE("EventLayer retrieves events for trigger", "[event_layer][registration]") {
    EventLayer event_layer;

    Event event1;
    event1.id = "event1";
    event1.target_id = "shape1";
    event1.trigger = EventTrigger::Click;

    Event event2;
    event2.id = "event2";
    event2.target_id = "shape2";
    event2.trigger = EventTrigger::Click;

    Event event3;
    event3.id = "event3";
    event3.target_id = "shape3";
    event3.trigger = EventTrigger::Hover;

    event_layer.register_event(event1);
    event_layer.register_event(event2);
    event_layer.register_event(event3);

    auto events = event_layer.get_events_for_trigger(EventTrigger::Click);
    REQUIRE(events.size() == 2);
    REQUIRE(events[0]->trigger == EventTrigger::Click);
    REQUIRE(events[1]->trigger == EventTrigger::Click);
}

TEST_CASE("EventLayer clears all events", "[event_layer][registration]") {
    EventLayer event_layer;

    Event event1;
    event1.id = "event1";
    event1.target_id = "shape1";
    event1.trigger = EventTrigger::Click;

    Event event2;
    event2.id = "event2";
    event2.target_id = "shape2";
    event2.trigger = EventTrigger::Hover;

    event_layer.register_event(event1);
    event_layer.register_event(event2);

    REQUIRE(event_layer.get_all_events().size() == 2);

    event_layer.clear();

    REQUIRE(event_layer.get_all_events().size() == 0);
}

// ============================================================================
// Event Triggering Tests
// ============================================================================

TEST_CASE("EventLayer triggers events for target and trigger type", "[event_layer][triggering]") {
    EventLayer event_layer;
    ShapeLayer shape_layer;
    event_layer.set_shape_layer(&shape_layer);

    // Create a shape
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    shape.inline_style["fill"] = "#0000ff";
    shape_layer.add_shape(shape);

    // Create event
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::UpdateStyle;
    action.parameters["shape_id"] = "shape1";
    action.parameters["property"] = "fill";
    action.parameters["value"] = "#ff0000";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event
    event_layer.handle_event("shape1", EventTrigger::Click);

    // Verify the action was executed
    auto* updated_shape = shape_layer.get_shape("shape1");
    REQUIRE(updated_shape != nullptr);
    REQUIRE(updated_shape->inline_style["fill"] == "#ff0000");
}

TEST_CASE("EventLayer does not trigger events for wrong trigger type", "[event_layer][triggering]") {
    EventLayer event_layer;
    ShapeLayer shape_layer;
    event_layer.set_shape_layer(&shape_layer);

    // Create a shape
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    shape.inline_style["fill"] = "#0000ff";
    shape_layer.add_shape(shape);

    // Create event for Click
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::UpdateStyle;
    action.parameters["shape_id"] = "shape1";
    action.parameters["property"] = "fill";
    action.parameters["value"] = "#ff0000";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger with Hover (should not execute)
    event_layer.handle_event("shape1", EventTrigger::Hover);

    // Verify the action was NOT executed
    auto* updated_shape = shape_layer.get_shape("shape1");
    REQUIRE(updated_shape != nullptr);
    REQUIRE(updated_shape->inline_style["fill"] == "#0000ff"); // Still original color
}

TEST_CASE("EventLayer triggers multiple events for same target", "[event_layer][triggering]") {
    EventLayer event_layer;
    ShapeLayer shape_layer;
    event_layer.set_shape_layer(&shape_layer);

    // Create a shape
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    shape.inline_style["fill"] = "#0000ff";
    shape.inline_style["stroke"] = "#000000";
    shape_layer.add_shape(shape);

    // Create first event
    Event event1;
    event1.id = "event1";
    event1.target_id = "shape1";
    event1.trigger = EventTrigger::Click;

    EventAction action1;
    action1.type = ActionType::UpdateStyle;
    action1.parameters["shape_id"] = "shape1";
    action1.parameters["property"] = "fill";
    action1.parameters["value"] = "#ff0000";
    event1.actions.push_back(action1);

    // Create second event
    Event event2;
    event2.id = "event2";
    event2.target_id = "shape1";
    event2.trigger = EventTrigger::Click;

    EventAction action2;
    action2.type = ActionType::UpdateStyle;
    action2.parameters["shape_id"] = "shape1";
    action2.parameters["property"] = "stroke";
    action2.parameters["value"] = "#00ff00";
    event2.actions.push_back(action2);

    event_layer.register_event(event1);
    event_layer.register_event(event2);

    // Trigger the events
    event_layer.handle_event("shape1", EventTrigger::Click);

    // Verify both actions were executed
    auto* updated_shape = shape_layer.get_shape("shape1");
    REQUIRE(updated_shape != nullptr);
    REQUIRE(updated_shape->inline_style["fill"] == "#ff0000");
    REQUIRE(updated_shape->inline_style["stroke"] == "#00ff00");
}

// ============================================================================
// Action Execution Tests
// ============================================================================

TEST_CASE("EventLayer executes UpdateData action", "[event_layer][actions]") {
    EventLayer event_layer;
    DataLayer data_layer;
    event_layer.set_data_layer(&data_layer);

    // Create a data node
    DataNode node;
    node.id = "node1";
    node.type = "person";
    node.properties["name"] = "John";
    node.properties["selected"] = "false";
    data_layer.add_node(node);

    // Create event with UpdateData action
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::UpdateData;
    action.parameters["node_id"] = "node1";
    action.parameters["property"] = "selected";
    action.parameters["value"] = "true";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event
    event_layer.handle_event("shape1", EventTrigger::Click);

    // Verify the data was updated
    auto* updated_node = data_layer.get_node("node1");
    REQUIRE(updated_node != nullptr);
    REQUIRE(updated_node->properties["selected"] == "true");
}

TEST_CASE("EventLayer executes UpdateStyle action", "[event_layer][actions]") {
    EventLayer event_layer;
    ShapeLayer shape_layer;
    event_layer.set_shape_layer(&shape_layer);

    // Create a shape
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    shape.inline_style["fill"] = "#0000ff";
    shape_layer.add_shape(shape);

    // Create event with UpdateStyle action
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::UpdateStyle;
    action.parameters["shape_id"] = "shape1";
    action.parameters["property"] = "fill";
    action.parameters["value"] = "#ff0000";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event
    event_layer.handle_event("shape1", EventTrigger::Click);

    // Verify the style was updated
    auto* updated_shape = shape_layer.get_shape("shape1");
    REQUIRE(updated_shape != nullptr);
    REQUIRE(updated_shape->inline_style["fill"] == "#ff0000");
}

TEST_CASE("EventLayer executes UpdateGeometry action", "[event_layer][actions]") {
    EventLayer event_layer;
    ShapeLayer shape_layer;
    event_layer.set_shape_layer(&shape_layer);

    // Create a shape
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    shape.geometry["x"] = 100.0f;
    shape.geometry["y"] = 100.0f;
    shape_layer.add_shape(shape);

    // Create event with UpdateGeometry action
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Drag;

    EventAction action;
    action.type = ActionType::UpdateGeometry;
    action.parameters["shape_id"] = "shape1";
    action.parameters["property"] = "x";
    action.parameters["value"] = "200.5";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event
    event_layer.handle_event("shape1", EventTrigger::Drag);

    // Verify the geometry was updated
    auto* updated_shape = shape_layer.get_shape("shape1");
    REQUIRE(updated_shape != nullptr);
    REQUIRE(updated_shape->geometry["x"] == 200.5f);
}

TEST_CASE("EventLayer executes multiple actions", "[event_layer][actions]") {
    EventLayer event_layer;
    DataLayer data_layer;
    ShapeLayer shape_layer;
    event_layer.set_data_layer(&data_layer);
    event_layer.set_shape_layer(&shape_layer);

    // Create a data node
    DataNode node;
    node.id = "node1";
    node.type = "person";
    node.properties["selected"] = "false";
    data_layer.add_node(node);

    // Create a shape
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    shape.inline_style["fill"] = "#0000ff";
    shape.geometry["x"] = 100.0f;
    shape_layer.add_shape(shape);

    // Create event with multiple actions
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action1;
    action1.type = ActionType::UpdateData;
    action1.parameters["node_id"] = "node1";
    action1.parameters["property"] = "selected";
    action1.parameters["value"] = "true";
    event.actions.push_back(action1);

    EventAction action2;
    action2.type = ActionType::UpdateStyle;
    action2.parameters["shape_id"] = "shape1";
    action2.parameters["property"] = "fill";
    action2.parameters["value"] = "#ff0000";
    event.actions.push_back(action2);

    EventAction action3;
    action3.type = ActionType::UpdateGeometry;
    action3.parameters["shape_id"] = "shape1";
    action3.parameters["property"] = "x";
    action3.parameters["value"] = "150.0";
    event.actions.push_back(action3);

    event_layer.register_event(event);

    // Trigger the event
    event_layer.handle_event("shape1", EventTrigger::Click);

    // Verify all actions were executed
    auto* updated_node = data_layer.get_node("node1");
    REQUIRE(updated_node != nullptr);
    REQUIRE(updated_node->properties["selected"] == "true");

    auto* updated_shape = shape_layer.get_shape("shape1");
    REQUIRE(updated_shape != nullptr);
    REQUIRE(updated_shape->inline_style["fill"] == "#ff0000");
    REQUIRE(updated_shape->geometry["x"] == 150.0f);
}

TEST_CASE("EventLayer handles missing data layer gracefully", "[event_layer][actions]") {
    EventLayer event_layer;
    // No data layer set

    // Create event with UpdateData action
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::UpdateData;
    action.parameters["node_id"] = "node1";
    action.parameters["property"] = "selected";
    action.parameters["value"] = "true";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event (should not crash)
    REQUIRE_NOTHROW(event_layer.handle_event("shape1", EventTrigger::Click));
}

TEST_CASE("EventLayer handles missing shape layer gracefully", "[event_layer][actions]") {
    EventLayer event_layer;
    // No shape layer set

    // Create event with UpdateStyle action
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::UpdateStyle;
    action.parameters["shape_id"] = "shape1";
    action.parameters["property"] = "fill";
    action.parameters["value"] = "#ff0000";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event (should not crash)
    REQUIRE_NOTHROW(event_layer.handle_event("shape1", EventTrigger::Click));
}

TEST_CASE("EventLayer handles missing node gracefully", "[event_layer][actions]") {
    EventLayer event_layer;
    DataLayer data_layer;
    event_layer.set_data_layer(&data_layer);

    // Create event with UpdateData action for non-existent node
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::UpdateData;
    action.parameters["node_id"] = "nonexistent";
    action.parameters["property"] = "selected";
    action.parameters["value"] = "true";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event (should not crash)
    REQUIRE_NOTHROW(event_layer.handle_event("shape1", EventTrigger::Click));
}

TEST_CASE("EventLayer handles missing shape gracefully", "[event_layer][actions]") {
    EventLayer event_layer;
    ShapeLayer shape_layer;
    event_layer.set_shape_layer(&shape_layer);

    // Create event with UpdateStyle action for non-existent shape
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::UpdateStyle;
    action.parameters["shape_id"] = "nonexistent";
    action.parameters["property"] = "fill";
    action.parameters["value"] = "#ff0000";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event (should not crash)
    REQUIRE_NOTHROW(event_layer.handle_event("shape1", EventTrigger::Click));
}

// ============================================================================
// Conditional Event Tests
// ============================================================================

TEST_CASE("EventLayer evaluates condition with expression parser", "[event_layer][conditions]") {
    EventLayer event_layer;
    ShapeLayer shape_layer;
    ExpressionParser expression_parser;
    event_layer.set_shape_layer(&shape_layer);
    event_layer.set_expression_parser(&expression_parser);

    // Create a shape
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    shape.inline_style["fill"] = "#0000ff";
    shape_layer.add_shape(shape);

    // Create event with condition that evaluates to true (1 is truthy)
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;
    event.condition = "1";

    EventAction action;
    action.type = ActionType::UpdateStyle;
    action.parameters["shape_id"] = "shape1";
    action.parameters["property"] = "fill";
    action.parameters["value"] = "#ff0000";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event
    event_layer.handle_event("shape1", EventTrigger::Click);

    // Verify the action was executed
    auto* updated_shape = shape_layer.get_shape("shape1");
    REQUIRE(updated_shape != nullptr);
    REQUIRE(updated_shape->inline_style["fill"] == "#ff0000");
}

TEST_CASE("EventLayer skips action when condition is false", "[event_layer][conditions]") {
    EventLayer event_layer;
    ShapeLayer shape_layer;
    ExpressionParser expression_parser;
    event_layer.set_shape_layer(&shape_layer);
    event_layer.set_expression_parser(&expression_parser);

    // Create a shape
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    shape.inline_style["fill"] = "#0000ff";
    shape_layer.add_shape(shape);

    // Create event with condition that evaluates to false (0 is falsy)
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;
    event.condition = "0";

    EventAction action;
    action.type = ActionType::UpdateStyle;
    action.parameters["shape_id"] = "shape1";
    action.parameters["property"] = "fill";
    action.parameters["value"] = "#ff0000";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event
    event_layer.handle_event("shape1", EventTrigger::Click);

    // Verify the action was NOT executed
    auto* updated_shape = shape_layer.get_shape("shape1");
    REQUIRE(updated_shape != nullptr);
    REQUIRE(updated_shape->inline_style["fill"] == "#0000ff"); // Still original color
}

TEST_CASE("EventLayer executes action when no condition is specified", "[event_layer][conditions]") {
    EventLayer event_layer;
    ShapeLayer shape_layer;
    event_layer.set_shape_layer(&shape_layer);

    // Create a shape
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    shape.inline_style["fill"] = "#0000ff";
    shape_layer.add_shape(shape);

    // Create event without condition
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;
    // No condition specified

    EventAction action;
    action.type = ActionType::UpdateStyle;
    action.parameters["shape_id"] = "shape1";
    action.parameters["property"] = "fill";
    action.parameters["value"] = "#ff0000";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event
    event_layer.handle_event("shape1", EventTrigger::Click);

    // Verify the action was executed
    auto* updated_shape = shape_layer.get_shape("shape1");
    REQUIRE(updated_shape != nullptr);
    REQUIRE(updated_shape->inline_style["fill"] == "#ff0000");
}

TEST_CASE("EventLayer defaults to true when no expression parser is set", "[event_layer][conditions]") {
    EventLayer event_layer;
    ShapeLayer shape_layer;
    event_layer.set_shape_layer(&shape_layer);
    // No expression parser set

    // Create a shape
    Shape shape;
    shape.id = "shape1";
    shape.type = "rect";
    shape.inline_style["fill"] = "#0000ff";
    shape_layer.add_shape(shape);

    // Create event with condition
    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;
    event.condition = "some_condition";

    EventAction action;
    action.type = ActionType::UpdateStyle;
    action.parameters["shape_id"] = "shape1";
    action.parameters["property"] = "fill";
    action.parameters["value"] = "#ff0000";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event
    event_layer.handle_event("shape1", EventTrigger::Click);

    // Verify the action was executed (defaults to true)
    auto* updated_shape = shape_layer.get_shape("shape1");
    REQUIRE(updated_shape != nullptr);
    REQUIRE(updated_shape->inline_style["fill"] == "#ff0000");
}

// ============================================================================
// Event Trigger Type Tests
// ============================================================================

TEST_CASE("EventLayer handles all trigger types", "[event_layer][triggers]") {
    EventLayer event_layer;

    std::vector<EventTrigger> triggers = {
        EventTrigger::Click,
        EventTrigger::DoubleClick,
        EventTrigger::RightClick,
        EventTrigger::Hover,
        EventTrigger::HoverEnd,
        EventTrigger::DragStart,
        EventTrigger::Drag,
        EventTrigger::DragEnd,
        EventTrigger::Select,
        EventTrigger::Deselect
    };

    for (size_t i = 0; i < triggers.size(); ++i) {
        Event event;
        event.id = "event" + std::to_string(i);
        event.target_id = "shape1";
        event.trigger = triggers[i];

        event_layer.register_event(event);

        auto* retrieved = event_layer.get_event(event.id);
        REQUIRE(retrieved != nullptr);
        REQUIRE(retrieved->trigger == triggers[i]);
    }
}

// ============================================================================
// Action Type Tests
// ============================================================================

TEST_CASE("EventLayer handles ShowTooltip action", "[event_layer][actions]") {
    EventLayer event_layer;

    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Hover;

    EventAction action;
    action.type = ActionType::ShowTooltip;
    action.parameters["content"] = "This is a tooltip";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event (should not crash, just logs)
    REQUIRE_NOTHROW(event_layer.handle_event("shape1", EventTrigger::Hover));
}

TEST_CASE("EventLayer handles Navigate action", "[event_layer][actions]") {
    EventLayer event_layer;

    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::Navigate;
    action.parameters["target"] = "https://example.com";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event (should not crash, just logs)
    REQUIRE_NOTHROW(event_layer.handle_event("shape1", EventTrigger::Click));
}

TEST_CASE("EventLayer handles EmitCustomEvent action", "[event_layer][actions]") {
    EventLayer event_layer;

    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::EmitCustomEvent;
    action.parameters["event_name"] = "custom_event";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event (should not crash, just logs)
    REQUIRE_NOTHROW(event_layer.handle_event("shape1", EventTrigger::Click));
}

TEST_CASE("EventLayer handles ExecuteScript action", "[event_layer][actions]") {
    EventLayer event_layer;

    Event event;
    event.id = "event1";
    event.target_id = "shape1";
    event.trigger = EventTrigger::Click;

    EventAction action;
    action.type = ActionType::ExecuteScript;
    action.parameters["script"] = "console.log('test')";
    event.actions.push_back(action);

    event_layer.register_event(event);

    // Trigger the event (should not crash, logs error)
    REQUIRE_NOTHROW(event_layer.handle_event("shape1", EventTrigger::Click));
}
