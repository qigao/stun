/*
 * Flex Runtime Tests - Clean Version
 *
 * Tests only implemented runtime functionality:
 * - Scene creation and properties
 * - Node hierarchy (Group, Shape, Text, Image)
 * - Timeline animation system
 * - State machine transitions
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "flex/runtime.h"

using namespace flex;
using Catch::Matchers::WithinAbs;

// ============================================================================
// ARTBOARD TESTS
// ============================================================================

TEST_CASE("Scene: Create with dimensions", "[runtime][scene]") {
    auto scene = Scene::create(800.0f, 600.0f);

    REQUIRE(scene);
    REQUIRE_THAT(scene->width(), WithinAbs(800.0f, 0.001f));
    REQUIRE_THAT(scene->height(), WithinAbs(600.0f, 0.001f));
}

TEST_CASE("Scene: Set size", "[runtime][scene]") {
    auto scene = Scene::create(100.0f, 100.0f);

    scene->set_size(1920.0f, 1080.0f);

    REQUIRE_THAT(scene->width(), WithinAbs(1920.0f, 0.001f));
    REQUIRE_THAT(scene->height(), WithinAbs(1080.0f, 0.001f));
}

TEST_CASE("Scene: Background color", "[runtime][scene]") {
    auto scene = Scene::create(800.0f, 600.0f);

    Color bg = {0.2f, 0.3f, 0.4f, 1.0f};
    scene->set_background(bg);

    const Color& result = scene->background();
    REQUIRE_THAT(result.r, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(result.g, WithinAbs(0.3f, 0.001f));
    REQUIRE_THAT(result.b, WithinAbs(0.4f, 0.001f));
    REQUIRE_THAT(result.a, WithinAbs(1.0f, 0.001f));
}

TEST_CASE("Scene: Add children to root", "[runtime][scene]") {
    auto scene = Scene::create(800.0f, 600.0f);

    auto group = Group::create();
    scene->add_child(group);

    REQUIRE(scene->root() != nullptr);
    REQUIRE(scene->root()->child_count() == 1);
}

// ============================================================================
// NODE HIERARCHY TESTS
// ============================================================================

TEST_CASE("Group: Create and add children", "[runtime][group]") {
    auto group = Group::create();

    auto child1 = Group::create();
    auto child2 = Group::create();

    group->add_child(child1);
    group->add_child(child2);

    REQUIRE(group->child_count() == 2);
    REQUIRE(group->child_at(0) == child1.get());
    REQUIRE(group->child_at(1) == child2.get());
}

TEST_CASE("Group: Remove child", "[runtime][group]") {
    auto group = Group::create();
    auto child = Group::create();

    group->add_child(child);
    REQUIRE(group->child_count() == 1);

    group->remove_child(child.get());
    REQUIRE(group->child_count() == 0);
}

TEST_CASE("Node: ID and visibility", "[runtime][node]") {
    auto node = Group::create();

    node->set_id("testNode");
    REQUIRE(node->id() == "testNode");

    REQUIRE(node->visible() == true);
    node->set_visible(false);
    REQUIRE(node->visible() == false);
}

TEST_CASE("Node: Opacity", "[runtime][node]") {
    auto node = Group::create();

    REQUIRE_THAT(node->opacity(), WithinAbs(1.0f, 0.001f));

    node->set_opacity(0.5f);
    REQUIRE_THAT(node->opacity(), WithinAbs(0.5f, 0.001f));
}

TEST_CASE("Node: Find child by ID", "[runtime][node]") {
    auto parent = Group::create();
    auto child = Group::create();
    child->set_id("target");

    parent->add_child(child);

    Node* found = parent->find_child_recursive("target");
    REQUIRE(found == child.get());

    Node* not_found = parent->find_child_recursive("nonexistent");
    REQUIRE(not_found == nullptr);
}

// ============================================================================
// SHAPE TESTS
// ============================================================================

TEST_CASE("Shape: Create with default properties", "[runtime][shape]") {
    auto shape = Shape::create();

    REQUIRE(shape);
    REQUIRE_THAT(shape->opacity(), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("Shape: Set rectangle geometry", "[runtime][shape]") {
    auto shape = Shape::create();

    shape->set_rect(100.0f, 50.0f, 5.0f);
    REQUIRE(shape->geometry_type() == GeometryType::Rect);

    auto rect = shape->rect();
    REQUIRE_THAT(rect.width, WithinAbs(100.0f, 0.001f));
    REQUIRE_THAT(rect.height, WithinAbs(50.0f, 0.001f));
    REQUIRE_THAT(rect.corner_radius, WithinAbs(5.0f, 0.001f));
}

TEST_CASE("Shape: Set circle geometry", "[runtime][shape]") {
    auto shape = Shape::create();

    shape->set_circle(25.0f);
    REQUIRE(shape->geometry_type() == GeometryType::Circle);

    auto circle = shape->circle();
    REQUIRE_THAT(circle.radius, WithinAbs(25.0f, 0.001f));
}

TEST_CASE("Shape: Fill color", "[runtime][shape]") {
    auto shape = Shape::create();

    Color red = {1.0f, 0.0f, 0.0f, 1.0f};
    shape->set_fill(red);

    REQUIRE(shape->has_fill());
    Fill fill = shape->fill();
    REQUIRE_THAT(fill.color.r, WithinAbs(1.0f, 0.001f));
    REQUIRE_THAT(fill.color.g, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(fill.color.b, WithinAbs(0.0f, 0.001f));
}

TEST_CASE("Shape: Stroke properties", "[runtime][shape]") {
    auto shape = Shape::create();

    Color blue = {0.0f, 0.0f, 1.0f, 1.0f};
    shape->set_stroke(blue, 3.0f);

    REQUIRE(shape->has_stroke());
    Stroke stroke = shape->stroke();
    REQUIRE_THAT(stroke.width, WithinAbs(3.0f, 0.001f));
    REQUIRE_THAT(stroke.color.b, WithinAbs(1.0f, 0.001f));
}

// ============================================================================
// TEXT TESTS
// ============================================================================

TEST_CASE("Text: Create with content", "[runtime][text]") {
    auto text = Text::create();

    text->set_content("Hello World");
    REQUIRE(text->content() == "Hello World");
}

TEST_CASE("Text: Font properties", "[runtime][text]") {
    auto text = Text::create();

    text->set_font_family("Arial");
    REQUIRE(text->font_family() == "Arial");

    text->set_font_size(24.0f);
    REQUIRE_THAT(text->font_size(), WithinAbs(24.0f, 0.001f));
}

TEST_CASE("Text: Color", "[runtime][text]") {
    auto text = Text::create();

    Color black = {0.0f, 0.0f, 0.0f, 1.0f};
    text->set_color(black);

    Color result = text->color();
    REQUIRE_THAT(result.r, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.g, WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(result.b, WithinAbs(0.0f, 0.001f));
}

// ============================================================================
// IMAGE TESTS
// ============================================================================

TEST_CASE("Image: Create with source", "[runtime][image]") {
    auto image = Image::create();

    image->set_src("test.png");
    REQUIRE(image->src() == "test.png");
}

TEST_CASE("Image: Dimensions", "[runtime][image]") {
    auto image = Image::create();

    image->set_width(200.0f);
    image->set_height(150.0f);

    REQUIRE_THAT(image->width(), WithinAbs(200.0f, 0.001f));
    REQUIRE_THAT(image->height(), WithinAbs(150.0f, 0.001f));
}

// ============================================================================
// TIMELINE TESTS
// ============================================================================

TEST_CASE("Timeline: Create with name", "[runtime][timeline]") {
    ArenaAllocator alloc(4096);
    auto timeline = Timeline::create("fadeIn", alloc);

    REQUIRE(std::string(timeline->name()) == "fadeIn");
}

TEST_CASE("Timeline: Set duration and loop mode", "[runtime][timeline]") {
    ArenaAllocator alloc(4096);
    auto timeline = Timeline::create("test", alloc);

    timeline->set_duration(2.0f);
    REQUIRE_THAT(timeline->duration(), WithinAbs(2.0f, 0.001f));

    REQUIRE(timeline->loop_mode() == LoopMode::Once);
    timeline->set_loop_mode(LoopMode::Loop);
    REQUIRE(timeline->loop_mode() == LoopMode::Loop);
}

TEST_CASE("Timeline: Add track", "[runtime][timeline]") {
    ArenaAllocator alloc(4096);
    auto timeline = Timeline::create("test", alloc);

    timeline->add_track("opacity");

    auto* track = timeline->get_track("opacity");
    REQUIRE(track != nullptr);
    REQUIRE(std::string(track->property()) == "opacity");
}

TEST_CASE("Timeline: Add keyframes", "[runtime][timeline]") {
    ArenaAllocator alloc(4096);
    auto timeline = Timeline::create("test", alloc);
    timeline->add_track("opacity");

    auto* track = timeline->get_track("opacity");
    track->add_keyframe(0.0f, 0.0f);
    track->add_keyframe(1.0f, 0.5f);
    track->add_keyframe(2.0f, 1.0f);

    REQUIRE(track->keyframe_count() == 3);
}

TEST_CASE("Timeline: Sample track at time", "[runtime][timeline]") {
    ArenaAllocator alloc(4096);
    auto timeline = Timeline::create("test", alloc);
    timeline->add_track("opacity");

    auto* track = timeline->get_track("opacity");
    track->add_keyframe(0.0f, 0.0f);
    track->add_keyframe(2.0f, 1.0f);

    // Sample returns AnimValue (std::variant)
    AnimValue val0 = track->sample(0.0f);
    AnimValue val1 = track->sample(1.0f);
    AnimValue val2 = track->sample(2.0f);

    REQUIRE_THAT(std::get<float>(val0), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(std::get<float>(val1), WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(std::get<float>(val2), WithinAbs(1.0f, 0.001f));
}

// ============================================================================
// STATE MACHINE TESTS
// ============================================================================

TEST_CASE("RuntimeStateMachine: Create and name", "[runtime][machine]") {
    RuntimeStateMachine machine("testMachine");

    REQUIRE(machine.name() == "testMachine");
}

TEST_CASE("RuntimeStateMachine: Add layer", "[runtime][machine]") {
    RuntimeStateMachine machine("testMachine");

    machine.add_layer("status");

    REQUIRE(machine.get_layer("status") != nullptr);
    REQUIRE(machine.get_layer("nonexistent") == nullptr);
}

TEST_CASE("RuntimeStateMachine: Layer with states", "[runtime][machine]") {
    RuntimeStateMachine machine("testMachine");
    machine.add_layer("status");

    auto* layer = machine.get_layer("status");
    layer->add_state("idle", true, "toIdle");
    layer->add_state("active", false, "toActive");

    REQUIRE(layer->current_state() == "idle");
}

TEST_CASE("RuntimeStateMachine: State transitions", "[runtime][machine]") {
    RuntimeStateMachine machine("testMachine");
    machine.add_layer("status");

    auto* layer = machine.get_layer("status");
    layer->add_state("idle", true, "");
    layer->add_state("active", false, "");

    layer->add_transition("idle", "active", "trigger", ">", 0.0f);

    REQUIRE(layer->current_state() == "idle");

    machine.set_input("trigger", 1.0f);
    machine.update(0.016f);

    REQUIRE(layer->current_state() == "active");
}

TEST_CASE("RuntimeStateMachine: Multiple transitions", "[runtime][machine]") {
    RuntimeStateMachine machine("counter");
    machine.add_layer("status");

    auto* layer = machine.get_layer("status");
    layer->add_state("low", true, "");
    layer->add_state("medium", false, "");
    layer->add_state("high", false, "");

    layer->add_transition("low", "medium", "value", ">", 10.0f);
    layer->add_transition("medium", "high", "value", ">", 50.0f);
    layer->add_transition("high", "medium", "value", "<", 50.0f);
    layer->add_transition("medium", "low", "value", "<", 10.0f);

    REQUIRE(layer->current_state() == "low");

    machine.set_input("value", 25.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "medium");

    machine.set_input("value", 75.0f);
    machine.update(0.016f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "high");

    machine.set_input("value", 30.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "medium");

    machine.set_input("value", 5.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "low");
}

// ============================================================================
// EASING TESTS
// ============================================================================

TEST_CASE("Easing: Linear", "[runtime][easing]") {
    Easing easing = Easing::linear();

    REQUIRE_THAT(easing.evaluate(0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(easing.evaluate(0.5f), WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(easing.evaluate(1.0f), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("Easing: EaseIn", "[runtime][easing]") {
    Easing easing = Easing::ease_in();

    REQUIRE_THAT(easing.evaluate(0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE(easing.evaluate(0.5f) < 0.5f);  // EaseIn is slower at start
    REQUIRE_THAT(easing.evaluate(1.0f), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("Easing: EaseOut", "[runtime][easing]") {
    Easing easing = Easing::ease_out();

    REQUIRE_THAT(easing.evaluate(0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE(easing.evaluate(0.5f) > 0.5f);  // EaseOut is faster at start
    REQUIRE_THAT(easing.evaluate(1.0f), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("Easing: EaseInOut", "[runtime][easing]") {
    Easing easing = Easing::ease_in_out();

    REQUIRE_THAT(easing.evaluate(0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(easing.evaluate(0.5f), WithinAbs(0.5f, 0.05f));  // Symmetric
    REQUIRE_THAT(easing.evaluate(1.0f), WithinAbs(1.0f, 0.001f));
}

// ============================================================================
// COLOR TESTS
// ============================================================================

TEST_CASE("Color: Create and access components", "[runtime][color]") {
    Color color = {0.2f, 0.4f, 0.6f, 0.8f};

    REQUIRE_THAT(color.r, WithinAbs(0.2f, 0.001f));
    REQUIRE_THAT(color.g, WithinAbs(0.4f, 0.001f));
    REQUIRE_THAT(color.b, WithinAbs(0.6f, 0.001f));
    REQUIRE_THAT(color.a, WithinAbs(0.8f, 0.001f));
}
