/*
 * Flex Engine - Unit Tests
 *
 * Using Catch2 for testing.
 */

#include <catch2/catch_all.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <flex/flex.h>

using Catch::Matchers::WithinAbs;

// ============================================================================
// Phase 1 Tests: Types
// ============================================================================

TEST_CASE("Color::from_hex parses hex colors", "[types][color]") {
    SECTION("RGB red") {
        auto c = flex::Color::from_hex("#FF0000");
        REQUIRE_THAT(c.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(c.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(c.b, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(c.a, WithinAbs(1.0f, 0.01f));
    }

    SECTION("RGB green") {
        auto c = flex::Color::from_hex("#00FF00");
        REQUIRE_THAT(c.g, WithinAbs(1.0f, 0.01f));
    }

    SECTION("RGBA with alpha") {
        auto c = flex::Color::from_hex("#0000FF80");
        REQUIRE_THAT(c.b, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(c.a, WithinAbs(0.5f, 0.02f));
    }
}

TEST_CASE("Value::lerp interpolates correctly", "[types][value]") {
    auto a = flex::Value::from_float(0.0f);
    auto b = flex::Value::from_float(100.0f);
    auto mid = flex::Value::lerp(a, b, 0.5f);
    REQUIRE_THAT(mid.as_float(), WithinAbs(50.0f, 0.01f));
}

TEST_CASE("Easing::linear returns linear interpolation", "[types][easing]") {
    auto e = flex::Easing::linear();
    REQUIRE_THAT(e.evaluate(0.0f), WithinAbs(0.0f, 0.01f));
    REQUIRE_THAT(e.evaluate(0.5f), WithinAbs(0.5f, 0.01f));
    REQUIRE_THAT(e.evaluate(1.0f), WithinAbs(1.0f, 0.01f));
}

// ============================================================================
// Phase 1 Tests: Scene Graph
// ============================================================================

TEST_CASE("Artboard creation", "[scene][artboard]") {
    auto artboard = flex::Artboard::create(800, 600);
    REQUIRE_THAT(artboard->width(), WithinAbs(800.0f, 0.01f));
    REQUIRE_THAT(artboard->height(), WithinAbs(600.0f, 0.01f));
    REQUIRE(artboard->root() != nullptr);
}

TEST_CASE("Shape with rect geometry", "[scene][shape]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100, 50, 5);
    REQUIRE(shape->geometry_type() == flex::GeometryType::Rect);
    REQUIRE_THAT(shape->rect().width, WithinAbs(100.0f, 0.01f));
    REQUIRE_THAT(shape->rect().height, WithinAbs(50.0f, 0.01f));
    REQUIRE_THAT(shape->rect().corner_radius, WithinAbs(5.0f, 0.01f));
}

TEST_CASE("Shape with circle geometry", "[scene][shape]") {
    auto shape = flex::Shape::create();
    shape->set_circle(25);
    REQUIRE(shape->geometry_type() == flex::GeometryType::Circle);
    REQUIRE_THAT(shape->circle().radius, WithinAbs(25.0f, 0.01f));
}

TEST_CASE("Group hierarchy", "[scene][group]") {
    auto parent = flex::Group::create();
    parent->set_id("parent");

    auto child1 = flex::Shape::create();
    child1->set_id("child1");

    auto child2 = flex::Shape::create();
    child2->set_id("child2");

    parent->add_child(child1);
    parent->add_child(child2);

    REQUIRE(parent->child_count() == 2);
    REQUIRE(child1->parent() == parent.get());
    REQUIRE(child2->parent() == parent.get());

    auto* found = parent->find_child("child1");
    REQUIRE(found != nullptr);
    REQUIRE(found->id() == "child1");
}

TEST_CASE("Node tags", "[scene][node]") {
    auto shape = flex::Shape::create();
    shape->add_tag("enemy");
    shape->add_tag("flying");

    REQUIRE(shape->has_tag("enemy"));
    REQUIRE(shape->has_tag("flying"));
    REQUIRE_FALSE(shape->has_tag("player"));
}

TEST_CASE("Find nodes by tag", "[scene][group]") {
    auto group = flex::Group::create();

    auto enemy1 = flex::Shape::create();
    enemy1->set_id("enemy1");
    enemy1->add_tag("enemy");

    auto enemy2 = flex::Shape::create();
    enemy2->set_id("enemy2");
    enemy2->add_tag("enemy");

    auto player = flex::Shape::create();
    player->set_id("player");
    player->add_tag("player");

    group->add_child(enemy1);
    group->add_child(enemy2);
    group->add_child(player);

    auto enemies = group->find_by_tag("enemy");
    REQUIRE(enemies.size() == 2);
}

TEST_CASE("Node property access", "[scene][node]") {
    auto shape = flex::Shape::create();
    shape->set_position(100, 200);
    shape->set_opacity(0.5f);

    flex::Value val;
    REQUIRE(shape->get_property("x", &val));
    REQUIRE_THAT(val.as_float(), WithinAbs(100.0f, 0.01f));

    REQUIRE(shape->get_property("y", &val));
    REQUIRE_THAT(val.as_float(), WithinAbs(200.0f, 0.01f));

    REQUIRE(shape->get_property("opacity", &val));
    REQUIRE_THAT(val.as_float(), WithinAbs(0.5f, 0.01f));

    // Set via property
    shape->set_property("x", flex::Value::from_float(300.0f));
    REQUIRE_THAT(shape->x(), WithinAbs(300.0f, 0.01f));
}

TEST_CASE("Instance standalone creation", "[instance]") {
    auto instance = flex::Instance::create(1920, 1080);
    auto* artboard = instance->artboard();

    REQUIRE(artboard != nullptr);
    REQUIRE_THAT(artboard->width(), WithinAbs(1920.0f, 0.01f));
    REQUIRE_THAT(artboard->height(), WithinAbs(1080.0f, 0.01f));
}

// ============================================================================
// Phase 2 Tests: Timeline
// ============================================================================

TEST_CASE("Track samples single keyframe range", "[timeline][track]") {
    auto track = flex::Track::create("opacity");
    track->add_keyframe(0.0f, flex::Value::from_float(0.0f));
    track->add_keyframe(1.0f, flex::Value::from_float(1.0f));

    REQUIRE_THAT(track->sample(0.0f).as_float(), WithinAbs(0.0f, 0.01f));
    REQUIRE_THAT(track->sample(0.5f).as_float(), WithinAbs(0.5f, 0.01f));
    REQUIRE_THAT(track->sample(1.0f).as_float(), WithinAbs(1.0f, 0.01f));
}

TEST_CASE("Track samples multiple keyframes", "[timeline][track]") {
    auto track = flex::Track::create("x");
    track->add_keyframe(0.0f, flex::Value::from_float(0.0f));
    track->add_keyframe(1.0f, flex::Value::from_float(100.0f));
    track->add_keyframe(2.0f, flex::Value::from_float(50.0f));

    REQUIRE_THAT(track->sample(0.5f).as_float(), WithinAbs(50.0f, 0.01f));
    REQUIRE_THAT(track->sample(1.5f).as_float(), WithinAbs(75.0f, 0.01f));
    REQUIRE_THAT(track->duration(), WithinAbs(2.0f, 0.01f));
}

TEST_CASE("Timeline playback", "[timeline]") {
    auto timeline = flex::Timeline::create("FadeIn");
    auto track = timeline->add_track("opacity");
    track->add_keyframe(0.0f, flex::Value::from_float(0.0f));
    track->add_keyframe(1.0f, flex::Value::from_float(1.0f));

    REQUIRE_THAT(timeline->duration(), WithinAbs(1.0f, 0.01f));

    auto shape = flex::Shape::create();
    shape->set_opacity(0.0f);

    flex::TimelinePlayer player(timeline, shape.get());
    player.play();

    // Advance halfway
    player.advance(0.5f);
    REQUIRE_THAT(shape->opacity(), WithinAbs(0.5f, 0.01f));

    // Advance to end
    player.advance(0.5f);
    REQUIRE_THAT(shape->opacity(), WithinAbs(1.0f, 0.01f));
    REQUIRE(player.is_finished());
}

TEST_CASE("Timeline loop mode", "[timeline]") {
    auto timeline = flex::Timeline::create("Loop");
    timeline->set_loop_mode(flex::LoopMode::Loop);
    auto track = timeline->add_track("x");
    track->add_keyframe(0.0f, flex::Value::from_float(0.0f));
    track->add_keyframe(1.0f, flex::Value::from_float(100.0f));

    auto shape = flex::Shape::create();
    flex::TimelinePlayer player(timeline, shape.get());
    player.play();

    // Advance past duration
    player.advance(1.5f);
    REQUIRE(player.is_playing());  // Still playing (looping)
    REQUIRE_THAT(shape->x(), WithinAbs(50.0f, 0.01f));  // 0.5 into loop
}

TEST_CASE("Animation controller", "[timeline][instance]") {
    auto instance = flex::Instance::create(800, 600);

    // Create and add timeline
    auto timeline = flex::Timeline::create("Move");
    auto track = timeline->add_track("x");
    track->add_keyframe(0.0f, flex::Value::from_float(0.0f));
    track->add_keyframe(1.0f, flex::Value::from_float(200.0f));

    instance->add_timeline(timeline);

    // Create a shape and add to scene
    auto shape = flex::Shape::create();
    shape->set_id("box");
    instance->artboard()->add_child(shape);

    // Play animation
    auto* player = instance->play("Move", shape.get());
    REQUIRE(player != nullptr);

    // Advance
    instance->advance(0.5f);
    REQUIRE_THAT(shape->x(), WithinAbs(100.0f, 0.01f));
}

// ============================================================================
// Phase 2 Tests: State Machine
// ============================================================================

TEST_CASE("State machine basic", "[machine]") {
    auto machine = flex::Machine::create("Controller");
    auto layer = machine->add_layer("Base");

    layer->add_state("Idle");
    layer->add_state("Running");
    layer->set_initial_state("Idle");

    machine->init();
    REQUIRE(machine->current_state("Base") == "Idle");
}

TEST_CASE("State machine input transition", "[machine]") {
    auto machine = flex::Machine::create("Controller");
    auto layer = machine->add_layer("Base");

    layer->add_state("Idle");
    layer->add_state("Running");
    layer->add_transition("Idle", "Running")
        ->when_input_gt("Speed", 0.1f);

    machine->init();
    REQUIRE(machine->current_state("Base") == "Idle");

    // Update with Speed = 0 (no transition)
    machine->update(0.1f,
        [](const std::string&) { return 0.0f; },
        [](const std::string&) { return false; },
        [](const std::string&) { return false; });
    REQUIRE(machine->current_state("Base") == "Idle");

    // Update with Speed = 1 (should transition)
    machine->update(0.1f,
        [](const std::string& name) { return name == "Speed" ? 1.0f : 0.0f; },
        [](const std::string&) { return false; },
        [](const std::string&) { return false; });
    REQUIRE(machine->current_state("Base") == "Running");
}

TEST_CASE("State machine event transition", "[machine]") {
    auto machine = flex::Machine::create("Controller");
    auto layer = machine->add_layer("Base");

    layer->add_state("A");
    layer->add_state("B");
    layer->add_transition("A", "B")->when_event("Jump");

    machine->init();
    REQUIRE(machine->current_state("Base") == "A");

    // Fire event
    machine->update(0.1f,
        [](const std::string&) { return 0.0f; },
        [](const std::string& name) { return name == "Jump"; },
        [](const std::string&) { return false; });
    REQUIRE(machine->current_state("Base") == "B");
}

TEST_CASE("Instance with state machine", "[machine][instance]") {
    auto instance = flex::Instance::create(800, 600);

    auto machine = flex::Machine::create("Controller");
    auto layer = machine->add_layer("Base");
    layer->add_state("Idle");
    layer->add_state("Active");
    layer->add_transition("Idle", "Active")->when_event("Activate");

    instance->set_machine(machine);
    REQUIRE(instance->current_state("Base") == "Idle");

    // Send event
    instance->send_event("Activate");
    instance->advance(0.016f);
    REQUIRE(instance->current_state("Base") == "Active");
}

// ============================================================================
// Phase 3 Tests: Builder
// ============================================================================

TEST_CASE("Builder creates artboard from AST", "[builder]") {
    flex::ast::Document doc;

    // Create an artboard AST
    flex::ast::Artboard artboard_ast;
    artboard_ast.name = "Main";
    artboard_ast.width = 1024;
    artboard_ast.height = 768;

    doc.artboards.push_back(artboard_ast);

    flex::Builder builder;
    auto result = builder.build(doc);

    REQUIRE_FALSE(result.has_error);
    REQUIRE(result.artboard != nullptr);
    REQUIRE_THAT(result.artboard->width(), WithinAbs(1024.0f, 0.01f));
    REQUIRE_THAT(result.artboard->height(), WithinAbs(768.0f, 0.01f));
}

TEST_CASE("Builder creates nodes from AST", "[builder]") {
    flex::ast::Document doc;
    flex::ast::Artboard artboard_ast;
    artboard_ast.name = "Main";
    artboard_ast.width = 800;
    artboard_ast.height = 600;

    // Create a Shape node AST
    auto shape_ast = std::make_shared<flex::ast::Node>();
    shape_ast->type = "Shape";
    shape_ast->name = "RedBox";

    // Add position properties
    flex::ast::Property prop_x;
    prop_x.name = "x";
    prop_x.value = std::make_shared<flex::ast::Value>();
    prop_x.value->data = flex::ast::NumberValue{100, ""};
    shape_ast->properties.push_back(prop_x);

    flex::ast::Property prop_y;
    prop_y.name = "y";
    prop_y.value = std::make_shared<flex::ast::Value>();
    prop_y.value->data = flex::ast::NumberValue{50, ""};
    shape_ast->properties.push_back(prop_y);

    // Add geometry
    flex::ast::Geometry rect_geom;
    rect_geom.type = "Rect";
    flex::ast::Property width_prop;
    width_prop.name = "width";
    width_prop.value = std::make_shared<flex::ast::Value>();
    width_prop.value->data = flex::ast::NumberValue{200, ""};
    rect_geom.properties.push_back(width_prop);

    flex::ast::Property height_prop;
    height_prop.name = "height";
    height_prop.value = std::make_shared<flex::ast::Value>();
    height_prop.value->data = flex::ast::NumberValue{100, ""};
    rect_geom.properties.push_back(height_prop);
    shape_ast->geometries.push_back(rect_geom);

    // Add fill
    flex::ast::Paint fill_paint;
    fill_paint.type = "Fill";
    flex::ast::Property color_prop;
    color_prop.name = "color";
    color_prop.value = std::make_shared<flex::ast::Value>();
    color_prop.value->data = flex::ast::ColorValue{"#FF0000"};
    fill_paint.properties.push_back(color_prop);
    shape_ast->paints.push_back(fill_paint);

    artboard_ast.children.push_back(shape_ast);
    doc.artboards.push_back(artboard_ast);

    flex::Builder builder;
    auto result = builder.build(doc);

    REQUIRE(result.artboard != nullptr);
    auto* found = result.artboard->find("RedBox");
    REQUIRE(found != nullptr);
    REQUIRE_THAT(found->x(), WithinAbs(100.0f, 0.01f));
    REQUIRE_THAT(found->y(), WithinAbs(50.0f, 0.01f));

    auto* shape = dynamic_cast<flex::Shape*>(found);
    REQUIRE(shape != nullptr);
    REQUIRE(shape->geometry_type() == flex::GeometryType::Rect);
    REQUIRE_THAT(shape->rect().width, WithinAbs(200.0f, 0.01f));
    REQUIRE(shape->has_fill());
}

TEST_CASE("Builder creates timeline from AST", "[builder]") {
    flex::ast::Document doc;
    doc.artboards.push_back(flex::ast::Artboard{"Main", 800, 600, {}});

    flex::ast::Timeline timeline_ast;
    timeline_ast.name = "FadeIn";

    flex::ast::Track track_ast;
    track_ast.property = "opacity";

    // Keyframe at 0s = 0
    flex::ast::Keyframe kf1;
    kf1.time = 0;
    kf1.unit = "s";
    kf1.value = std::make_shared<flex::ast::Value>();
    kf1.value->data = flex::ast::NumberValue{0, ""};
    track_ast.keyframes.push_back(kf1);

    // Keyframe at 1s = 1
    flex::ast::Keyframe kf2;
    kf2.time = 1;
    kf2.unit = "s";
    kf2.value = std::make_shared<flex::ast::Value>();
    kf2.value->data = flex::ast::NumberValue{1, ""};
    track_ast.keyframes.push_back(kf2);

    timeline_ast.tracks.push_back(track_ast);
    doc.timelines.push_back(timeline_ast);

    flex::Builder builder;
    auto result = builder.build(doc);

    REQUIRE(result.timelines.size() == 1);
    REQUIRE(result.timelines[0]->name() == "FadeIn");
    REQUIRE_THAT(result.timelines[0]->duration(), WithinAbs(1.0f, 0.01f));
}

TEST_CASE("Builder creates machine from AST", "[builder]") {
    flex::ast::Document doc;
    doc.artboards.push_back(flex::ast::Artboard{"Main", 800, 600, {}});

    flex::ast::Machine machine_ast;
    machine_ast.name = "Controller";

    flex::ast::Layer layer_ast;
    layer_ast.name = "Base";
    layer_ast.entry_state = "Idle";

    flex::ast::State idle_state;
    idle_state.name = "Idle";
    layer_ast.states.push_back(idle_state);

    flex::ast::State running_state;
    running_state.name = "Running";
    layer_ast.states.push_back(running_state);

    flex::ast::Transition trans;
    trans.from_state = "Idle";
    trans.to_state = "Running";
    trans.condition = "Start";
    layer_ast.transitions.push_back(trans);

    machine_ast.layers.push_back(layer_ast);
    doc.machines.push_back(machine_ast);

    flex::Builder builder;
    auto result = builder.build(doc);

    REQUIRE(result.machine != nullptr);
    REQUIRE(result.machine->name() == "Controller");
    result.machine->init();
    REQUIRE(result.machine->current_state("Base") == "Idle");
}

TEST_CASE("Builder processes inputs", "[builder]") {
    flex::ast::Document doc;
    doc.artboards.push_back(flex::ast::Artboard{"Main", 800, 600, {}});

    flex::ast::Input speed_input;
    speed_input.type = "float";
    speed_input.name = "Speed";
    speed_input.default_value = std::make_shared<flex::ast::Value>();
    speed_input.default_value->data = flex::ast::NumberValue{1.5, ""};
    doc.inputs.push_back(speed_input);

    flex::ast::Input enabled_input;
    enabled_input.type = "bool";
    enabled_input.name = "Enabled";
    enabled_input.default_value = std::make_shared<flex::ast::Value>();
    enabled_input.default_value->data = flex::ast::BoolValue{true};
    doc.inputs.push_back(enabled_input);

    flex::Builder builder;
    auto result = builder.build(doc);

    REQUIRE(result.float_inputs.count("Speed") == 1);
    REQUIRE_THAT(result.float_inputs["Speed"], WithinAbs(1.5f, 0.01f));
    REQUIRE(result.bool_inputs.count("Enabled") == 1);
    REQUIRE(result.bool_inputs["Enabled"] == true);
}

// ============================================================================
// Phase 3 Tests: Scripting (QuickJS)
// ============================================================================

TEST_CASE("ScriptContext evaluates JS expressions", "[script]") {
    flex::ScriptContext ctx;
    
    auto result = ctx.eval("1 + 2");
    REQUIRE(result.is_number());
    REQUIRE_THAT(result.as_number(), WithinAbs(3.0, 0.01));
}

TEST_CASE("ScriptContext handles variables", "[script]") {
    flex::ScriptContext ctx;
    
    ctx.set_global("x", flex::ScriptValue::from_number(10));
    auto result = ctx.get_global("x");
    REQUIRE(result.is_number());
    REQUIRE_THAT(result.as_number(), WithinAbs(10.0, 0.01));
}

TEST_CASE("ScriptContext calls JS functions", "[script]") {
    flex::ScriptContext ctx;
    
    ctx.eval("function add(a, b) { return a + b; }");
    auto result = ctx.call("add", {
        flex::ScriptValue::from_number(5),
        flex::ScriptValue::from_number(3)
    });
    
    REQUIRE(result.is_number());
    REQUIRE_THAT(result.as_number(), WithinAbs(8.0, 0.01));
}

TEST_CASE("ScriptContext binds native functions", "[script]") {
    flex::ScriptContext ctx;
    
    ctx.bind_function("multiply", [](const std::vector<flex::ScriptValue>& args) {
        if (args.size() >= 2 && args[0].is_number() && args[1].is_number()) {
            return flex::ScriptValue::from_number(args[0].as_number() * args[1].as_number());
        }
        return flex::ScriptValue::from_number(0);
    });
    
    auto result = ctx.eval("multiply(6, 7)");
    REQUIRE(result.is_number());
    REQUIRE_THAT(result.as_number(), WithinAbs(42.0, 0.01));
}

TEST_CASE("ScriptEngine binds Flex API", "[script][integration]") {
    auto instance = flex::Instance::create(800, 600);
    
    // Create a shape
    auto shape = flex::Shape::create();
    shape->set_id("box");
    shape->set_position(0, 0);
    instance->artboard()->add_child(shape);
    
    // Create script context with bindings
    flex::ScriptEngine engine;
    auto ctx = engine.create_context();
    engine.bind_flex_api(ctx.get(), instance.get());
    
    // Set input via JS
    ctx->eval("flex_setInput('speed', 5.0)");
    REQUIRE_THAT(instance->get_input("speed"), WithinAbs(5.0f, 0.01f));
    
    // Modify node property via JS
    ctx->eval("flex_setProperty('box', 'x', 100)");
    auto* found = instance->artboard()->find("box");
    REQUIRE(found != nullptr);
    REQUIRE_THAT(found->x(), WithinAbs(100.0f, 0.01f));
}

TEST_CASE("ScriptContext reports errors", "[script]") {
    flex::ScriptContext ctx;
    
    ctx.eval("this is not valid javascript");
    REQUIRE(ctx.has_error());
}

// ============================================================================
// Phase 3 Tests: Physics (Box2D)
// ============================================================================

TEST_CASE("PhysicsWorld steps simulation", "[physics]") {
    flex::PhysicsWorld world(0.0f, 9.8f);
    
    // Should not crash
    world.step(1.0f / 60.0f);
}

TEST_CASE("PhysicsWorld creates and syncs bodies", "[physics]") {
    flex::PhysicsWorld world(0.0f, 0.0f); // No gravity
    
    auto node = flex::Shape::create();
    node->set_position(100, 100);
    
    auto* body = world.create_body(node.get(), flex::BodyType::Dynamic);
    REQUIRE(body != nullptr);
    
    // Verify initial pos (Box2D uses meters, we use pixels)
    float x, y;
    body->get_position(x, y);
    REQUIRE_THAT(x, WithinAbs(100.0f, 0.1f));
    REQUIRE_THAT(y, WithinAbs(100.0f, 0.1f));
    
    // Move body
    body->set_linear_velocity(100, 0); // 100 px/s = 2 m/s
    
    // Step
    world.step(1.0f); 
    
    // Node should have moved
    // Velocity 100px/s * 1s = 100px delta
    // New pos should be approx 200, 100
    REQUIRE_THAT(node->x(), WithinAbs(200.0f, 2.0f)); // Allow some tolerance
    REQUIRE_THAT(node->y(), WithinAbs(100.0f, 1.0f));
}

TEST_CASE("PhysicsWorld simulates gravity", "[physics]") {
    flex::PhysicsWorld world(0.0f, 10.0f); // 10 m/s^2 gravity down
    
    auto node = flex::Shape::create();
    node->set_position(0, 0);
    
    world.create_body(node.get(), flex::BodyType::Dynamic);
    
    // Step 1 second
    // Distance = 0.5 * g * t^2 = 0.5 * 10 * 1 = 5 meters
    // 5 meters * 50 px/m = 250 pixels
    
    // We step in small increments for stability
    for(int i=0; i<60; ++i) {
        world.step(1.0f/60.0f);
    }
    
    // Check if it fell
    REQUIRE(node->y() > 100.0f); // Should be around 250
}

TEST_CASE("PhysicsWorld collision begin callback", "[physics][collision]") {
    flex::PhysicsWorld world(0.0f, 10.0f);

    // Create two shapes that will collide
    auto floor = flex::Shape::create();
    floor->set_position(250, 500);
    floor->set_rect(500, 50);

    auto ball = flex::Shape::create();
    ball->set_position(250, 100);
    ball->set_circle(25);

    // Floor is static, ball is dynamic
    world.create_body(floor.get(), flex::BodyType::Static);
    world.create_body(ball.get(), flex::BodyType::Dynamic);

    bool collision_detected = false;
    flex::Node* collided_a = nullptr;
    flex::Node* collided_b = nullptr;

    world.on_collision_begin([&](const flex::CollisionEvent& e) {
        collision_detected = true;
        collided_a = e.body_a->node;
        collided_b = e.body_b->node;
    });

    // Run simulation until collision
    for (int i = 0; i < 120; ++i) {
        world.step(1.0f / 60.0f);
        if (collision_detected) break;
    }

    REQUIRE(collision_detected);
    // One of the bodies should be floor, one should be ball
    bool has_floor = (collided_a == floor.get() || collided_b == floor.get());
    bool has_ball = (collided_a == ball.get() || collided_b == ball.get());
    REQUIRE(has_floor);
    REQUIRE(has_ball);
}

TEST_CASE("PhysicsWorld find_body returns correct body", "[physics]") {
    flex::PhysicsWorld world(0.0f, 0.0f);

    auto node1 = flex::Shape::create();
    node1->set_position(100, 100);
    node1->set_rect(50, 50);

    auto node2 = flex::Shape::create();
    node2->set_position(200, 200);
    node2->set_rect(50, 50);

    auto* body1 = world.create_body(node1.get(), flex::BodyType::Static);
    auto* body2 = world.create_body(node2.get(), flex::BodyType::Static);

    REQUIRE(world.find_body(node1.get()) == body1);
    REQUIRE(world.find_body(node2.get()) == body2);
    REQUIRE(world.find_body(nullptr) == nullptr);
}

TEST_CASE("Definition loads from flex source", "[parser]") {
    const char* source = R"(
        Artboard "Main" (800, 600) {
            Group "Container" {
                Rect {
                    x: 10
                    y: 10
                    width: 100
                    height: 100
                    Fill { color: "#FF0000" }
                }
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());
    if (def->has_error()) {
        FAIL(def->error_message());
    }

    auto instance = flex::Instance::create(def);
    REQUIRE(instance->artboard() != nullptr);
    REQUIRE(instance->artboard()->find("Container") != nullptr);
}

// ============================================================================
// Production Hardening Tests: Input Validation
// ============================================================================

TEST_CASE("Shape geometry validates negative dimensions", "[shape][validation]") {
    auto shape = flex::Shape::create();

    SECTION("Negative rect dimensions become zero") {
        shape->set_rect(-100, -50, -5);
        REQUIRE_THAT(shape->rect().width, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(shape->rect().height, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(shape->rect().corner_radius, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Negative circle radius becomes zero") {
        shape->set_circle(-25);
        REQUIRE_THAT(shape->circle().radius, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Negative ellipse radii become zero") {
        shape->set_ellipse(-10, -20);
        REQUIRE_THAT(shape->ellipse().rx, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(shape->ellipse().ry, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Negative polygon radius becomes zero") {
        shape->set_polygon(6, -50);
        REQUIRE_THAT(shape->polygon().radius, WithinAbs(0.0f, 0.01f));
    }
}

TEST_CASE("Shape polygon validates minimum sides", "[shape][validation]") {
    auto shape = flex::Shape::create();

    SECTION("Polygon with 0 sides becomes 3") {
        shape->set_polygon(0, 50);
        REQUIRE(shape->polygon().sides == 3);
    }

    SECTION("Polygon with 1 side becomes 3") {
        shape->set_polygon(1, 50);
        REQUIRE(shape->polygon().sides == 3);
    }

    SECTION("Polygon with 2 sides becomes 3") {
        shape->set_polygon(2, 50);
        REQUIRE(shape->polygon().sides == 3);
    }

    SECTION("Polygon with 3 sides stays 3") {
        shape->set_polygon(3, 50);
        REQUIRE(shape->polygon().sides == 3);
    }

    SECTION("Polygon with negative sides becomes 3") {
        shape->set_polygon(-5, 50);
        REQUIRE(shape->polygon().sides == 3);
    }
}

// ============================================================================
// Production Hardening Tests: Value::lerp Type Safety
// ============================================================================

TEST_CASE("Value::lerp handles type mismatch", "[types][value]") {
    SECTION("Float vs Vec2 returns first value") {
        auto a = flex::Value::from_float(10.0f);
        auto b = flex::Value::from_vec2(100.0f, 200.0f);
        auto result = flex::Value::lerp(a, b, 0.5f);

        // Should return 'a' unchanged when types don't match
        REQUIRE(result.type() == flex::ValueType::Float);
        REQUIRE_THAT(result.as_float(), WithinAbs(10.0f, 0.01f));
    }

    SECTION("Vec3 vs Vec4 returns first value") {
        auto a = flex::Value::from_vec3(1.0f, 2.0f, 3.0f);
        auto b = flex::Value::from_vec4(10.0f, 20.0f, 30.0f, 40.0f);
        auto result = flex::Value::lerp(a, b, 0.5f);

        REQUIRE(result.type() == flex::ValueType::Vec3);
    }
}

TEST_CASE("Value::lerp interpolates Vec4 correctly", "[types][value]") {
    auto a = flex::Value::from_vec4(0.0f, 0.0f, 0.0f, 0.0f);
    auto b = flex::Value::from_vec4(1.0f, 1.0f, 1.0f, 1.0f);
    auto mid = flex::Value::lerp(a, b, 0.5f);

    REQUIRE(mid.type() == flex::ValueType::Vec4);
    REQUIRE_THAT(mid.data.v4[0], WithinAbs(0.5f, 0.01f));
    REQUIRE_THAT(mid.data.v4[1], WithinAbs(0.5f, 0.01f));
    REQUIRE_THAT(mid.data.v4[2], WithinAbs(0.5f, 0.01f));
    REQUIRE_THAT(mid.data.v4[3], WithinAbs(0.5f, 0.01f));
}

TEST_CASE("Value::lerp clamps t parameter", "[types][value]") {
    auto a = flex::Value::from_float(0.0f);
    auto b = flex::Value::from_float(100.0f);

    SECTION("t < 0 returns a") {
        auto result = flex::Value::lerp(a, b, -0.5f);
        REQUIRE_THAT(result.as_float(), WithinAbs(0.0f, 0.01f));
    }

    SECTION("t > 1 returns b") {
        auto result = flex::Value::lerp(a, b, 1.5f);
        REQUIRE_THAT(result.as_float(), WithinAbs(100.0f, 0.01f));
    }
}

// ============================================================================
// Production Hardening Tests: Parser Error Messages
// ============================================================================

TEST_CASE("Parser provides error location", "[parser][error]") {
    SECTION("Syntax error reports line number") {
        const char* source = R"(
            Artboard "Main" (800, 600) {
                InvalidKeyword {
                }
            }
        )";

        auto def = flex::Definition::load(source);
        REQUIRE(def->has_error());
        REQUIRE(def->error_line() > 0);
    }

    SECTION("Empty source reports error") {
        auto def = flex::Definition::load("");
        REQUIRE(def->has_error());
    }

    SECTION("Null source reports error") {
        auto def = flex::Definition::load(nullptr);
        REQUIRE(def->has_error());
    }
}

// ============================================================================
// Production Hardening Tests: Shape Property Setter
// ============================================================================

TEST_CASE("Shape property setter returns correct values", "[shape][property]") {
    auto shape = flex::Shape::create();

    SECTION("Setting radius on Rect geometry returns false") {
        shape->set_rect(100, 100);
        auto result = shape->set_property("radius", flex::Value::from_float(50.0f));
        REQUIRE_FALSE(result);
    }

    SECTION("Setting radius on Circle geometry returns true") {
        shape->set_circle(25);
        auto result = shape->set_property("radius", flex::Value::from_float(50.0f));
        REQUIRE(result);
        REQUIRE_THAT(shape->circle().radius, WithinAbs(50.0f, 0.01f));
    }

    SECTION("Setting radius on Polygon geometry returns true") {
        shape->set_polygon(6, 25);
        auto result = shape->set_property("radius", flex::Value::from_float(75.0f));
        REQUIRE(result);
        REQUIRE_THAT(shape->polygon().radius, WithinAbs(75.0f, 0.01f));
    }

    SECTION("Setting width on Rect geometry returns true") {
        shape->set_rect(100, 100);
        auto result = shape->set_property("width", flex::Value::from_float(200.0f));
        REQUIRE(result);
        REQUIRE_THAT(shape->rect().width, WithinAbs(200.0f, 0.01f));
    }

    SECTION("Setting width on Circle geometry returns false") {
        shape->set_circle(50);
        auto result = shape->set_property("width", flex::Value::from_float(200.0f));
        REQUIRE_FALSE(result);
    }
}

// ============================================================================
// Production Hardening Tests: Color Parsing Edge Cases
// ============================================================================

TEST_CASE("Color::from_hex handles edge cases", "[types][color]") {
    SECTION("Invalid hex without # returns black") {
        auto c = flex::Color::from_hex("FF0000");
        REQUIRE_THAT(c.r, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(c.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(c.b, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Null string returns black") {
        auto c = flex::Color::from_hex(nullptr);
        REQUIRE_THAT(c.r, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Short RGB (#RGB) parses correctly") {
        auto c = flex::Color::from_hex("#F00");
        REQUIRE_THAT(c.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(c.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(c.b, WithinAbs(0.0f, 0.01f));
    }

    SECTION("Lowercase hex parses correctly") {
        auto c = flex::Color::from_hex("#ff00ff");
        REQUIRE_THAT(c.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(c.g, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(c.b, WithinAbs(1.0f, 0.01f));
    }
}

// ============================================================================
// Production Hardening Tests: Easing Edge Cases
// ============================================================================

TEST_CASE("Easing::evaluate clamps input", "[types][easing]") {
    auto ease = flex::Easing::ease_in_out();

    SECTION("t < 0 returns 0") {
        REQUIRE_THAT(ease.evaluate(-0.5f), WithinAbs(0.0f, 0.01f));
    }

    SECTION("t > 1 returns 1") {
        REQUIRE_THAT(ease.evaluate(1.5f), WithinAbs(1.0f, 0.01f));
    }
}

// ============================================================================
// Production Hardening Tests: Group Operations
// ============================================================================

TEST_CASE("Group handles child removal safely", "[scene][group]") {
    auto parent = flex::Group::create();
    auto child = flex::Shape::create();
    child->set_id("child");

    parent->add_child(child);
    REQUIRE(parent->child_count() == 1);
    REQUIRE(child->parent() == parent.get());

    SECTION("Remove by pointer") {
        parent->remove_child(child.get());
        REQUIRE(parent->child_count() == 0);
        REQUIRE(child->parent() == nullptr);
    }

    SECTION("Remove by index") {
        parent->remove_child_at(0);
        REQUIRE(parent->child_count() == 0);
        REQUIRE(child->parent() == nullptr);
    }

    SECTION("Clear all children") {
        auto child2 = flex::Shape::create();
        parent->add_child(child2);
        REQUIRE(parent->child_count() == 2);

        parent->clear_children();
        REQUIRE(parent->child_count() == 0);
        REQUIRE(child->parent() == nullptr);
        REQUIRE(child2->parent() == nullptr);
    }

    SECTION("Remove non-existent child is safe") {
        auto other = flex::Shape::create();
        parent->remove_child(other.get());  // Should not crash
        REQUIRE(parent->child_count() == 1);
    }

    SECTION("Remove at invalid index is safe") {
        parent->remove_child_at(999);  // Should not crash
        REQUIRE(parent->child_count() == 1);
    }
}

// ============================================================================
// Production Hardening Tests: Timeline Edge Cases
// ============================================================================

TEST_CASE("Timeline handles empty track gracefully", "[timeline]") {
    auto track = flex::Track::create("x");

    SECTION("Duration of empty track is 0") {
        REQUIRE_THAT(track->duration(), WithinAbs(0.0f, 0.01f));
    }

    SECTION("Sample on empty track returns 0") {
        auto val = track->sample(0.5f);
        REQUIRE_THAT(val.as_float(), WithinAbs(0.0f, 0.01f));
    }
}

TEST_CASE("Timeline player seek clamps to duration", "[timeline]") {
    auto timeline = flex::Timeline::create("Test");
    auto track = timeline->add_track("x");
    track->add_keyframe(0.0f, flex::Value::from_float(0.0f));
    track->add_keyframe(1.0f, flex::Value::from_float(100.0f));

    auto shape = flex::Shape::create();
    flex::TimelinePlayer player(timeline, shape.get());

    SECTION("Seek to negative time clamps to 0") {
        player.seek(-1.0f);
        REQUIRE_THAT(player.current_time(), WithinAbs(0.0f, 0.01f));
    }

    SECTION("Seek beyond duration clamps to duration") {
        player.seek(10.0f);
        REQUIRE_THAT(player.current_time(), WithinAbs(1.0f, 0.01f));
    }
}

// ============================================================================
// Production Hardening Tests: State Machine Edge Cases
// ============================================================================

TEST_CASE("State machine handles missing state gracefully", "[machine]") {
    auto machine = flex::Machine::create("Test");
    auto layer = machine->add_layer("Base");
    layer->add_state("Idle");
    machine->init();

    SECTION("Get non-existent layer returns empty string") {
        auto& state = machine->current_state("NonExistent");
        REQUIRE(state.empty());
    }

    SECTION("Get state returns empty for null layer") {
        auto* null_layer = machine->get_layer("DoesNotExist");
        REQUIRE(null_layer == nullptr);
    }
}

TEST_CASE("Layer transition with after_time condition", "[machine]") {
    auto machine = flex::Machine::create("Test");
    auto layer = machine->add_layer("Base");
    layer->add_state("A");
    layer->add_state("B");
    layer->add_transition("A", "B")->after(0.5f);

    machine->init();
    REQUIRE(machine->current_state("Base") == "A");

    // Update for less than 0.5s - should not transition
    machine->update(0.3f,
        [](const std::string&) { return 0.0f; },
        [](const std::string&) { return false; },
        [](const std::string&) { return false; });
    REQUIRE(machine->current_state("Base") == "A");

    // Update to exceed 0.5s total - should transition
    machine->update(0.3f,
        [](const std::string&) { return 0.0f; },
        [](const std::string&) { return false; },
        [](const std::string&) { return false; });
    REQUIRE(machine->current_state("Base") == "B");
}

// ============================================================================
// Text Node Tests
// ============================================================================

TEST_CASE("Text node creation and properties", "[scene][text]") {
    auto text = flex::Text::create();
    text->set_id("label");
    text->set_content("Hello World");
    text->set_font_size(24.0f);
    text->set_font_family("Arial");
    text->set_color(flex::Color(1, 0, 0, 1));

    REQUIRE(text->type() == flex::NodeType::Text);
    REQUIRE(text->id() == "label");
    REQUIRE(text->content() == "Hello World");
    REQUIRE_THAT(text->font_size(), WithinAbs(24.0f, 0.01f));
    REQUIRE(text->font_family() == "Arial");
    REQUIRE_THAT(text->color().r, WithinAbs(1.0f, 0.01f));
}

TEST_CASE("Text node default values", "[scene][text]") {
    auto text = flex::Text::create();

    REQUIRE(text->content().empty());
    REQUIRE(text->font_family() == "sans-serif");
    REQUIRE_THAT(text->font_size(), WithinAbs(16.0f, 0.01f));
    REQUIRE(text->font_weight() == flex::FontWeight::Normal);
    REQUIRE(text->text_align() == flex::TextAlign::Left);
    REQUIRE_THAT(text->line_height(), WithinAbs(1.2f, 0.01f));
}

TEST_CASE("Text node property access", "[scene][text]") {
    auto text = flex::Text::create();

    SECTION("Set content via property") {
        text->set_property("content", flex::Value::from_string("Test"));
        REQUIRE(text->content() == "Test");
    }

    SECTION("Set fontSize via property") {
        text->set_property("fontSize", flex::Value::from_float(32.0f));
        REQUIRE_THAT(text->font_size(), WithinAbs(32.0f, 0.01f));
    }
}

// ============================================================================
// Image Node Tests
// ============================================================================

TEST_CASE("Image node creation and properties", "[scene][image]") {
    auto image = flex::Image::create();
    image->set_id("photo");
    image->set_src("/path/to/image.png");
    image->set_width(200);
    image->set_height(150);
    image->set_fit(flex::ImageFit::Cover);

    REQUIRE(image->type() == flex::NodeType::Image);
    REQUIRE(image->id() == "photo");
    REQUIRE(image->src() == "/path/to/image.png");
    REQUIRE_THAT(image->width(), WithinAbs(200.0f, 0.01f));
    REQUIRE_THAT(image->height(), WithinAbs(150.0f, 0.01f));
    REQUIRE(image->fit() == flex::ImageFit::Cover);
}

TEST_CASE("Image node default values", "[scene][image]") {
    auto image = flex::Image::create();

    REQUIRE(image->src().empty());
    REQUIRE_THAT(image->width(), WithinAbs(0.0f, 0.01f));
    REQUIRE_THAT(image->height(), WithinAbs(0.0f, 0.01f));
    REQUIRE(image->fit() == flex::ImageFit::Fill);
}

TEST_CASE("Image node property access", "[scene][image]") {
    auto image = flex::Image::create();

    SECTION("Set src via property") {
        image->set_property("src", flex::Value::from_string("test.png"));
        REQUIRE(image->src() == "test.png");
    }

    SECTION("Set dimensions via property") {
        image->set_property("width", flex::Value::from_float(100.0f));
        image->set_property("height", flex::Value::from_float(80.0f));
        REQUIRE_THAT(image->width(), WithinAbs(100.0f, 0.01f));
        REQUIRE_THAT(image->height(), WithinAbs(80.0f, 0.01f));
    }
}

// ============================================================================
// SVG Node Tests
// ============================================================================

TEST_CASE("SVG node creation with file path", "[scene][svg]") {
    auto svg = flex::Svg::create();
    svg->set_id("icon");
    svg->set_src("/path/to/icon.svg");
    svg->set_width(48);
    svg->set_height(48);

    REQUIRE(svg->type() == flex::NodeType::Svg);
    REQUIRE(svg->id() == "icon");
    REQUIRE(svg->src() == "/path/to/icon.svg");
    REQUIRE(svg->data().empty());
    REQUIRE_THAT(svg->width(), WithinAbs(48.0f, 0.01f));
    REQUIRE_THAT(svg->height(), WithinAbs(48.0f, 0.01f));
}

TEST_CASE("SVG node creation with inline data", "[scene][svg]") {
    auto svg = flex::Svg::create();
    svg->set_data("<svg><circle cx='50' cy='50' r='40'/></svg>");

    REQUIRE(svg->src().empty());
    REQUIRE(!svg->data().empty());
}

TEST_CASE("SVG node src and data are mutually exclusive", "[scene][svg]") {
    auto svg = flex::Svg::create();

    SECTION("Setting src clears data") {
        svg->set_data("<svg></svg>");
        svg->set_src("icon.svg");
        REQUIRE(svg->data().empty());
        REQUIRE(svg->src() == "icon.svg");
    }

    SECTION("Setting data clears src") {
        svg->set_src("icon.svg");
        svg->set_data("<svg></svg>");
        REQUIRE(svg->src().empty());
        REQUIRE(!svg->data().empty());
    }
}

TEST_CASE("SVG node default values", "[scene][svg]") {
    auto svg = flex::Svg::create();

    REQUIRE(svg->src().empty());
    REQUIRE(svg->data().empty());
    REQUIRE_THAT(svg->width(), WithinAbs(0.0f, 0.01f));
    REQUIRE_THAT(svg->height(), WithinAbs(0.0f, 0.01f));
}

TEST_CASE("SVG node property access", "[scene][svg]") {
    auto svg = flex::Svg::create();

    SECTION("Set src via property") {
        svg->set_property("src", flex::Value::from_string("test.svg"));
        REQUIRE(svg->src() == "test.svg");
    }

    SECTION("Set data via property") {
        svg->set_property("data", flex::Value::from_string("<svg></svg>"));
        REQUIRE(svg->data() == "<svg></svg>");
    }
}

// ============================================================================
// Path Geometry Tests
// ============================================================================

TEST_CASE("Shape with path geometry", "[scene][shape][path]") {
    auto shape = flex::Shape::create();

    SECTION("Simple triangle path") {
        shape->set_path("M 0 0 L 100 0 L 50 100 Z");
        REQUIRE(shape->geometry_type() == flex::GeometryType::Path);
        REQUIRE(shape->path().d == "M 0 0 L 100 0 L 50 100 Z");
    }

    SECTION("Rectangle path") {
        shape->set_path("M 0 0 H 100 V 50 H 0 Z");
        REQUIRE(shape->geometry_type() == flex::GeometryType::Path);
    }

    SECTION("Cubic bezier path") {
        shape->set_path("M 0 0 C 25 50 75 50 100 0");
        REQUIRE(shape->geometry_type() == flex::GeometryType::Path);
    }

    SECTION("Quadratic bezier path") {
        shape->set_path("M 0 0 Q 50 100 100 0");
        REQUIRE(shape->geometry_type() == flex::GeometryType::Path);
    }

    SECTION("Relative commands path") {
        shape->set_path("M 10 10 l 50 0 l 0 50 l -50 0 z");
        REQUIRE(shape->geometry_type() == flex::GeometryType::Path);
    }

    SECTION("Complex path with multiple subpaths") {
        shape->set_path("M 0 0 L 100 100 M 200 200 L 300 300");
        REQUIRE(shape->geometry_type() == flex::GeometryType::Path);
    }
}

TEST_CASE("Shape path with fill and stroke", "[scene][shape][path]") {
    auto shape = flex::Shape::create();
    shape->set_path("M 0 0 L 100 0 L 50 86.6 Z");
    shape->set_fill(flex::Color(1, 0, 0, 1));
    shape->set_stroke(flex::Color(0, 0, 0, 1), 2.0f);

    REQUIRE(shape->has_fill());
    REQUIRE(shape->has_stroke());
    REQUIRE_THAT(shape->fill().color.r, Catch::Matchers::WithinAbs(1.0f, 0.01f));
    REQUIRE_THAT(shape->stroke().width, Catch::Matchers::WithinAbs(2.0f, 0.01f));
}

// ============================================================================
// Font Loading Tests
// ============================================================================

// ============================================================================
// Instance Node Tests
// ============================================================================

TEST_CASE("Instance node creation and properties", "[scene][instance]") {
    auto instance = flex::InstanceNode::create();
    instance->set_id("button1");
    instance->set_source("components/Button.flex");

    REQUIRE(instance->type() == flex::NodeType::Instance);
    REQUIRE(instance->id() == "button1");
    REQUIRE(instance->source() == "components/Button.flex");
    REQUIRE(instance->is_loaded() == false);
}

TEST_CASE("Instance node input overrides", "[scene][instance]") {
    auto instance = flex::InstanceNode::create();

    SECTION("Float inputs") {
        instance->set_input("scale", 2.0f);
        REQUIRE_THAT(instance->get_float_input("scale"), WithinAbs(2.0f, 0.01f));
    }

    SECTION("String inputs") {
        instance->set_input("label", "Click Me");
        REQUIRE(instance->get_string_input("label") == "Click Me");
    }

    SECTION("Bool inputs") {
        instance->set_input("enabled", true);
        REQUIRE(instance->get_bool_input("enabled") == true);
    }

    SECTION("Default values for missing inputs") {
        REQUIRE_THAT(instance->get_float_input("missing"), WithinAbs(0.0f, 0.01f));
        REQUIRE(instance->get_string_input("missing").empty());
        REQUIRE(instance->get_bool_input("missing") == false);
    }
}

TEST_CASE("Instance node property access", "[scene][instance]") {
    auto instance = flex::InstanceNode::create();

    SECTION("Set source via property") {
        instance->set_property("source", flex::Value::from_string("test.flex"));
        REQUIRE(instance->source() == "test.flex");
    }

    SECTION("Get source via property") {
        instance->set_source("component.flex");
        flex::Value out;
        REQUIRE(instance->get_property("source", &out));
        REQUIRE(out.as_string() == "component.flex");
    }
}

TEST_CASE("Instance node load with invalid source", "[scene][instance]") {
    auto instance = flex::InstanceNode::create();

    SECTION("Empty source fails to load") {
        REQUIRE(instance->load() == false);
    }

    SECTION("Non-existent file fails to load") {
        instance->set_source("nonexistent.flex");
        REQUIRE(instance->load() == false);
        REQUIRE(instance->is_loaded() == false);
    }
}

TEST_CASE("Font loading API exists", "[font]") {
    // These tests verify the API exists and handles invalid input gracefully
    // Actual font loading requires valid font files

    SECTION("load_font with null path returns false") {
        REQUIRE(flex::load_font(nullptr) == false);
    }

    SECTION("load_font with invalid path returns false") {
        REQUIRE(flex::load_font("nonexistent_font.ttf") == false);
    }

    SECTION("load_font_data with null params returns false") {
        REQUIRE(flex::load_font_data(nullptr, nullptr, 0) == false);
        REQUIRE(flex::load_font_data("test", nullptr, 0) == false);
        REQUIRE(flex::load_font_data("test", "data", 0) == false);
    }

    SECTION("unload_font with null is safe") {
        flex::unload_font(nullptr);  // Should not crash
    }
}

// ============================================================================
// Gradient Fill Tests
// ============================================================================

TEST_CASE("LinearGradient creation and stops", "[types][gradient]") {
    flex::LinearGradient gradient(0, 0, 1, 1);  // Diagonal gradient

    SECTION("Default values") {
        flex::LinearGradient g;
        REQUIRE_THAT(g.x1, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(g.y1, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(g.x2, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(g.y2, WithinAbs(0.0f, 0.01f));
        REQUIRE(g.stops.empty());
    }

    SECTION("Add color stops") {
        gradient.add_stop(0.0f, flex::Color(1, 0, 0, 1));  // Red
        gradient.add_stop(1.0f, flex::Color(0, 0, 1, 1));  // Blue

        REQUIRE(gradient.stops.size() == 2);
        REQUIRE_THAT(gradient.stops[0].offset, WithinAbs(0.0f, 0.01f));
        REQUIRE_THAT(gradient.stops[0].color.r, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(gradient.stops[1].offset, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(gradient.stops[1].color.b, WithinAbs(1.0f, 0.01f));
    }

    SECTION("Multiple stops") {
        gradient.add_stop(0.0f, flex::Color(1, 0, 0, 1));
        gradient.add_stop(0.5f, flex::Color(0, 1, 0, 1));
        gradient.add_stop(1.0f, flex::Color(0, 0, 1, 1));

        REQUIRE(gradient.stops.size() == 3);
        REQUIRE_THAT(gradient.stops[1].offset, WithinAbs(0.5f, 0.01f));
        REQUIRE_THAT(gradient.stops[1].color.g, WithinAbs(1.0f, 0.01f));
    }
}

TEST_CASE("RadialGradient creation and stops", "[types][gradient]") {
    flex::RadialGradient gradient(0.5f, 0.5f, 0.5f);

    SECTION("Default values") {
        flex::RadialGradient g;
        REQUIRE_THAT(g.cx, WithinAbs(0.5f, 0.01f));
        REQUIRE_THAT(g.cy, WithinAbs(0.5f, 0.01f));
        REQUIRE_THAT(g.radius, WithinAbs(0.5f, 0.01f));
        REQUIRE(g.stops.empty());
    }

    SECTION("Add color stops") {
        gradient.add_stop(0.0f, flex::Color(1, 1, 1, 1));  // White center
        gradient.add_stop(1.0f, flex::Color(0, 0, 0, 1));  // Black edge

        REQUIRE(gradient.stops.size() == 2);
    }
}

TEST_CASE("Shape with linear gradient fill", "[scene][shape][gradient]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100, 100);

    flex::LinearGradient gradient(0, 0, 1, 0);  // Horizontal
    gradient.add_stop(0.0f, flex::Color(1, 0, 0, 1));
    gradient.add_stop(1.0f, flex::Color(0, 0, 1, 1));

    shape->set_fill(gradient);

    REQUIRE(shape->has_fill());
    REQUIRE(shape->fill().type == flex::FillType::LinearGradient);
    REQUIRE(shape->fill().linear_gradient.stops.size() == 2);
}

TEST_CASE("Shape with radial gradient fill", "[scene][shape][gradient]") {
    auto shape = flex::Shape::create();
    shape->set_circle(50);

    flex::RadialGradient gradient(0.5f, 0.5f, 0.5f);
    gradient.add_stop(0.0f, flex::Color(1, 1, 0, 1));  // Yellow center
    gradient.add_stop(1.0f, flex::Color(1, 0, 0, 1));  // Red edge

    shape->set_fill(gradient);

    REQUIRE(shape->has_fill());
    REQUIRE(shape->fill().type == flex::FillType::RadialGradient);
    REQUIRE(shape->fill().radial_gradient.stops.size() == 2);
}

TEST_CASE("Shape fill type changes correctly", "[scene][shape][gradient]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100, 100);

    SECTION("Solid to linear gradient") {
        shape->set_fill(flex::Color(1, 0, 0, 1));
        REQUIRE(shape->fill().type == flex::FillType::Solid);

        flex::LinearGradient gradient;
        gradient.add_stop(0.0f, flex::Color(1, 0, 0, 1));
        gradient.add_stop(1.0f, flex::Color(0, 0, 1, 1));
        shape->set_fill(gradient);
        REQUIRE(shape->fill().type == flex::FillType::LinearGradient);
    }

    SECTION("Linear gradient to radial gradient") {
        flex::LinearGradient lg;
        shape->set_fill(lg);
        REQUIRE(shape->fill().type == flex::FillType::LinearGradient);

        flex::RadialGradient rg;
        shape->set_fill(rg);
        REQUIRE(shape->fill().type == flex::FillType::RadialGradient);
    }

    SECTION("Gradient to solid") {
        flex::LinearGradient gradient;
        shape->set_fill(gradient);
        REQUIRE(shape->fill().type == flex::FillType::LinearGradient);

        shape->set_fill(flex::Color(0, 1, 0, 1));
        REQUIRE(shape->fill().type == flex::FillType::Solid);
        REQUIRE_THAT(shape->fill().color.g, WithinAbs(1.0f, 0.01f));
    }
}

TEST_CASE("ColorStop structure", "[types][gradient]") {
    flex::ColorStop stop(0.5f, flex::Color(1, 0, 0, 1));

    REQUIRE_THAT(stop.offset, WithinAbs(0.5f, 0.01f));
    REQUIRE_THAT(stop.color.r, WithinAbs(1.0f, 0.01f));
    REQUIRE_THAT(stop.color.g, WithinAbs(0.0f, 0.01f));
    REQUIRE_THAT(stop.color.b, WithinAbs(0.0f, 0.01f));
}

// ============================================================================
// Stroke Gradient Tests
// ============================================================================

TEST_CASE("Shape with linear gradient stroke", "[scene][shape][gradient]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100, 100);

    flex::LinearGradient gradient(0, 0, 1, 0);  // Horizontal
    gradient.add_stop(0.0f, flex::Color(1, 0, 0, 1));
    gradient.add_stop(1.0f, flex::Color(0, 0, 1, 1));

    shape->set_stroke(gradient, 3.0f);

    REQUIRE(shape->has_stroke());
    REQUIRE(shape->stroke().type == flex::StrokeType::LinearGradient);
    REQUIRE(shape->stroke().linear_gradient.stops.size() == 2);
    REQUIRE_THAT(shape->stroke().width, WithinAbs(3.0f, 0.01f));
}

TEST_CASE("Shape with radial gradient stroke", "[scene][shape][gradient]") {
    auto shape = flex::Shape::create();
    shape->set_circle(50);

    flex::RadialGradient gradient(0.5f, 0.5f, 0.5f);
    gradient.add_stop(0.0f, flex::Color(1, 1, 0, 1));  // Yellow center
    gradient.add_stop(1.0f, flex::Color(1, 0, 0, 1));  // Red edge

    shape->set_stroke(gradient, 2.0f);

    REQUIRE(shape->has_stroke());
    REQUIRE(shape->stroke().type == flex::StrokeType::RadialGradient);
    REQUIRE(shape->stroke().radial_gradient.stops.size() == 2);
}

TEST_CASE("Shape stroke type changes correctly", "[scene][shape][gradient]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100, 100);

    SECTION("Solid to linear gradient") {
        shape->set_stroke(flex::Color(1, 0, 0, 1), 1.0f);
        REQUIRE(shape->stroke().type == flex::StrokeType::Solid);

        flex::LinearGradient gradient;
        gradient.add_stop(0.0f, flex::Color(1, 0, 0, 1));
        gradient.add_stop(1.0f, flex::Color(0, 0, 1, 1));
        shape->set_stroke(gradient, 2.0f);
        REQUIRE(shape->stroke().type == flex::StrokeType::LinearGradient);
    }

    SECTION("Linear gradient to radial gradient") {
        flex::LinearGradient lg;
        shape->set_stroke(lg, 1.0f);
        REQUIRE(shape->stroke().type == flex::StrokeType::LinearGradient);

        flex::RadialGradient rg;
        shape->set_stroke(rg, 2.0f);
        REQUIRE(shape->stroke().type == flex::StrokeType::RadialGradient);
    }

    SECTION("Gradient to solid") {
        flex::LinearGradient gradient;
        shape->set_stroke(gradient, 1.0f);
        REQUIRE(shape->stroke().type == flex::StrokeType::LinearGradient);

        shape->set_stroke(flex::Color(0, 1, 0, 1), 3.0f);
        REQUIRE(shape->stroke().type == flex::StrokeType::Solid);
        REQUIRE_THAT(shape->stroke().color.g, WithinAbs(1.0f, 0.01f));
        REQUIRE_THAT(shape->stroke().width, WithinAbs(3.0f, 0.01f));
    }
}

TEST_CASE("Shape with both fill and stroke gradients", "[scene][shape][gradient]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100, 100);

    // Linear gradient fill
    flex::LinearGradient fillGradient(0, 0, 1, 1);
    fillGradient.add_stop(0.0f, flex::Color(1, 1, 1, 1));
    fillGradient.add_stop(1.0f, flex::Color(0, 0, 0, 1));
    shape->set_fill(fillGradient);

    // Radial gradient stroke
    flex::RadialGradient strokeGradient(0.5f, 0.5f, 0.5f);
    strokeGradient.add_stop(0.0f, flex::Color(1, 0, 0, 1));
    strokeGradient.add_stop(1.0f, flex::Color(0, 0, 1, 1));
    shape->set_stroke(strokeGradient, 4.0f);

    REQUIRE(shape->has_fill());
    REQUIRE(shape->has_stroke());
    REQUIRE(shape->fill().type == flex::FillType::LinearGradient);
    REQUIRE(shape->stroke().type == flex::StrokeType::RadialGradient);
    REQUIRE_THAT(shape->stroke().width, WithinAbs(4.0f, 0.01f));
}

// ============================================================================
// Data Binding Tests
// ============================================================================

TEST_CASE("BindingContext input management", "[binding]") {
    flex::BindingContext ctx;

    SECTION("Float inputs") {
        ctx.set_input("speed", 5.0f);
        REQUIRE_THAT(ctx.get_float_input("speed"), WithinAbs(5.0f, 0.01f));
        REQUIRE(ctx.has_input("speed"));
    }

    SECTION("String inputs") {
        ctx.set_input("name", "Player1");
        REQUIRE(ctx.get_string_input("name") == "Player1");
        REQUIRE(ctx.has_input("name"));
    }

    SECTION("Bool inputs") {
        ctx.set_input("enabled", true);
        REQUIRE(ctx.get_bool_input("enabled") == true);
        REQUIRE(ctx.has_input("enabled"));
    }

    SECTION("Missing input returns default") {
        REQUIRE_THAT(ctx.get_float_input("missing"), WithinAbs(0.0f, 0.01f));
        REQUIRE(ctx.get_string_input("missing").empty());
        REQUIRE(ctx.get_bool_input("missing") == false);
        REQUIRE_FALSE(ctx.has_input("missing"));
    }
}

TEST_CASE("Binding parsing", "[binding]") {
    SECTION("Simple input binding") {
        REQUIRE(flex::is_binding_string("$Speed"));
        auto binding = flex::parse_binding("$Speed");
        REQUIRE(binding.type == flex::BindingType::Input);
        REQUIRE(binding.input_name == "Speed");
    }

    SECTION("Expression binding") {
        REQUIRE(flex::is_binding_string("${ sin($Time) * 10 }"));
        auto binding = flex::parse_binding("${ sin($Time) * 10 }");
        REQUIRE(binding.type == flex::BindingType::Expression);
        REQUIRE(binding.expression == " sin($Time) * 10 ");
    }

    SECTION("Not a binding") {
        REQUIRE_FALSE(flex::is_binding_string("normal text"));
        REQUIRE_FALSE(flex::is_binding_string("100"));
        REQUIRE_FALSE(flex::is_binding_string(""));
    }
}

TEST_CASE("BindingContext with input binding", "[binding]") {
    flex::BindingContext ctx;
    auto shape = flex::Shape::create();
    shape->set_position(0, 0);

    // Set up input
    ctx.set_input("xPos", 100.0f);

    // Create binding for x property
    auto binding = flex::Binding::input("xPos");
    ctx.add_binding(shape.get(), "x", binding);

    // Evaluate bindings
    ctx.evaluate();

    // Check that property was updated
    REQUIRE_THAT(shape->x(), WithinAbs(100.0f, 0.01f));

    // Change input and re-evaluate
    ctx.set_input("xPos", 200.0f);
    ctx.evaluate();
    REQUIRE_THAT(shape->x(), WithinAbs(200.0f, 0.01f));
}

TEST_CASE("BindingContext remove bindings", "[binding]") {
    flex::BindingContext ctx;
    auto shape = flex::Shape::create();

    ctx.set_input("xPos", 100.0f);
    ctx.add_binding(shape.get(), "x", flex::Binding::input("xPos"));

    // Evaluate
    ctx.evaluate();
    REQUIRE_THAT(shape->x(), WithinAbs(100.0f, 0.01f));

    // Remove bindings
    ctx.remove_bindings(shape.get());

    // Change input and evaluate - should not affect shape
    ctx.set_input("xPos", 500.0f);
    ctx.evaluate();
    REQUIRE_THAT(shape->x(), WithinAbs(100.0f, 0.01f));  // Still 100
}

TEST_CASE("BindingContext time tracking", "[binding]") {
    flex::BindingContext ctx;

    REQUIRE_THAT(ctx.time(), WithinAbs(0.0f, 0.01f));

    ctx.set_time(1.5f);
    REQUIRE_THAT(ctx.time(), WithinAbs(1.5f, 0.01f));

    ctx.advance_time(0.5f);
    REQUIRE_THAT(ctx.time(), WithinAbs(2.0f, 0.01f));
}

TEST_CASE("BindingContext expression binding", "[binding][expression]") {
    flex::BindingContext ctx;
    flex::ScriptContext script_ctx;
    flex::script::bind_math(&script_ctx);
    ctx.set_script_context(&script_ctx);

    auto shape = flex::Shape::create();
    shape->set_position(0, 0);

    SECTION("Simple arithmetic expression") {
        auto binding = flex::Binding::expr("10 + 5");
        ctx.add_binding(shape.get(), "x", binding);
        ctx.evaluate();
        REQUIRE_THAT(shape->x(), WithinAbs(15.0f, 0.01f));
    }

    SECTION("Expression with input reference") {
        ctx.set_input("Speed", 50.0f);
        auto binding = flex::Binding::expr("$Speed * 2");
        ctx.add_binding(shape.get(), "x", binding);
        ctx.evaluate();
        REQUIRE_THAT(shape->x(), WithinAbs(100.0f, 0.01f));
    }

    SECTION("Expression with $Time") {
        ctx.set_time(2.0f);
        auto binding = flex::Binding::expr("$Time * 100");
        ctx.add_binding(shape.get(), "x", binding);
        ctx.evaluate();
        REQUIRE_THAT(shape->x(), WithinAbs(200.0f, 0.01f));
    }

    SECTION("Expression with math functions") {
        auto binding = flex::Binding::expr("sin(0) + 100");
        ctx.add_binding(shape.get(), "x", binding);
        ctx.evaluate();
        REQUIRE_THAT(shape->x(), WithinAbs(100.0f, 0.01f));
    }

    SECTION("Expression with lerp") {
        auto binding = flex::Binding::expr("lerp(0, 100, 0.5)");
        ctx.add_binding(shape.get(), "x", binding);
        ctx.evaluate();
        REQUIRE_THAT(shape->x(), WithinAbs(50.0f, 0.01f));
    }

    SECTION("Expression with clamp") {
        auto binding = flex::Binding::expr("clamp(150, 0, 100)");
        ctx.add_binding(shape.get(), "x", binding);
        ctx.evaluate();
        REQUIRE_THAT(shape->x(), WithinAbs(100.0f, 0.01f));
    }
}

TEST_CASE("Expression binding updates on time change", "[binding][expression]") {
    flex::BindingContext ctx;
    flex::ScriptContext script_ctx;
    flex::script::bind_math(&script_ctx);
    ctx.set_script_context(&script_ctx);

    auto shape = flex::Shape::create();
    shape->set_position(0, 0);

    // Oscillating x based on time
    auto binding = flex::Binding::expr("sin($Time) * 100");
    ctx.add_binding(shape.get(), "x", binding);

    // At time 0, sin(0) = 0
    ctx.set_time(0.0f);
    ctx.evaluate();
    REQUIRE_THAT(shape->x(), WithinAbs(0.0f, 1.0f));

    // At time PI/2, sin(PI/2) = 1
    ctx.set_time(3.14159f / 2.0f);
    ctx.evaluate();
    REQUIRE_THAT(shape->x(), WithinAbs(100.0f, 1.0f));
}

TEST_CASE("Builder creates bindings from AST", "[builder][binding]") {
    flex::ast::Document doc;

    // Add input definition
    flex::ast::Input speed_input;
    speed_input.type = "float";
    speed_input.name = "Speed";
    speed_input.default_value = std::make_shared<flex::ast::Value>();
    speed_input.default_value->data = flex::ast::NumberValue{10.0, ""};
    doc.inputs.push_back(speed_input);

    // Create artboard with a shape that has a binding
    flex::ast::Artboard artboard_ast;
    artboard_ast.name = "Main";
    artboard_ast.width = 800;
    artboard_ast.height = 600;

    auto shape_ast = std::make_shared<flex::ast::Node>();
    shape_ast->type = "Shape";
    shape_ast->name = "MovingBox";

    // x property bound to $Speed input
    flex::ast::Property prop_x;
    prop_x.name = "x";
    prop_x.value = std::make_shared<flex::ast::Value>();
    prop_x.value->data = flex::ast::BindingValue{"Speed"};
    shape_ast->properties.push_back(prop_x);

    // y property as static value
    flex::ast::Property prop_y;
    prop_y.name = "y";
    prop_y.value = std::make_shared<flex::ast::Value>();
    prop_y.value->data = flex::ast::NumberValue{50.0, ""};
    shape_ast->properties.push_back(prop_y);

    artboard_ast.children.push_back(shape_ast);
    doc.artboards.push_back(artboard_ast);

    flex::Builder builder;
    auto result = builder.build(doc);

    REQUIRE(result.bindings != nullptr);
    REQUIRE(result.artboard != nullptr);

    // Verify input was registered
    REQUIRE_THAT(result.bindings->get_float_input("Speed"), WithinAbs(10.0f, 0.01f));

    // Evaluate bindings - x should be set to Speed input value
    result.bindings->evaluate();

    auto* found = result.artboard->find("MovingBox");
    REQUIRE(found != nullptr);
    REQUIRE_THAT(found->x(), WithinAbs(10.0f, 0.01f));  // Bound to Speed
    REQUIRE_THAT(found->y(), WithinAbs(50.0f, 0.01f));  // Static value

    // Change input and re-evaluate
    result.bindings->set_input("Speed", 100.0f);
    result.bindings->evaluate();
    REQUIRE_THAT(found->x(), WithinAbs(100.0f, 0.01f));
}

// ============================================================================
// Event System Tests
// ============================================================================

TEST_CASE("Shape bounds calculation", "[scene][shape][bounds]") {
    SECTION("Rect bounds") {
        auto shape = flex::Shape::create();
        shape->set_position(10, 20);
        shape->set_rect(100, 50);

        auto b = shape->bounds();
        REQUIRE_THAT(b.x, WithinAbs(10.0f, 0.01f));
        REQUIRE_THAT(b.y, WithinAbs(20.0f, 0.01f));
        REQUIRE_THAT(b.width, WithinAbs(100.0f, 0.01f));
        REQUIRE_THAT(b.height, WithinAbs(50.0f, 0.01f));
    }

    SECTION("Circle bounds") {
        auto shape = flex::Shape::create();
        shape->set_position(50, 50);
        shape->set_circle(25);

        auto b = shape->bounds();
        // Circle is centered, so bounds should account for that
        REQUIRE_THAT(b.x, WithinAbs(25.0f, 0.01f));  // 50 - 25
        REQUIRE_THAT(b.y, WithinAbs(25.0f, 0.01f));  // 50 - 25
        REQUIRE_THAT(b.width, WithinAbs(50.0f, 0.01f));   // diameter
        REQUIRE_THAT(b.height, WithinAbs(50.0f, 0.01f));  // diameter
    }

    SECTION("Scaled bounds") {
        auto shape = flex::Shape::create();
        shape->set_position(0, 0);
        shape->set_rect(100, 100);
        shape->set_scale(2.0f);

        auto b = shape->bounds();
        REQUIRE_THAT(b.width, WithinAbs(200.0f, 0.01f));
        REQUIRE_THAT(b.height, WithinAbs(200.0f, 0.01f));
    }
}

TEST_CASE("Bounds::contains", "[types][bounds]") {
    flex::Bounds b{10, 20, 100, 50};

    SECTION("Point inside") {
        REQUIRE(b.contains(50, 40));
        REQUIRE(b.contains(10, 20));  // Top-left corner
        REQUIRE(b.contains(109, 69)); // Just inside bottom-right
    }

    SECTION("Point outside") {
        REQUIRE_FALSE(b.contains(5, 40));     // Left of bounds
        REQUIRE_FALSE(b.contains(50, 100));   // Below bounds
        REQUIRE_FALSE(b.contains(110, 40));   // Right of bounds
        REQUIRE_FALSE(b.contains(50, 15));    // Above bounds
    }
}

TEST_CASE("Node hit_test", "[scene][node][event]") {
    auto shape = flex::Shape::create();
    shape->set_position(10, 20);
    shape->set_rect(100, 50);

    SECTION("Point inside hits") {
        REQUIRE(shape->hit_test(50, 40));
    }

    SECTION("Point outside misses") {
        REQUIRE_FALSE(shape->hit_test(5, 40));
        REQUIRE_FALSE(shape->hit_test(50, 100));
    }

    SECTION("Invisible node misses") {
        shape->set_visible(false);
        REQUIRE_FALSE(shape->hit_test(50, 40));
    }
}

TEST_CASE("Node click event", "[scene][node][event]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100, 100);

    bool clicked = false;
    shape->on_click([&clicked]() {
        clicked = true;
    });

    shape->fire_click();
    REQUIRE(clicked);
}

TEST_CASE("Node pointer events", "[scene][node][event]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100, 100);

    int down_count = 0;
    int up_count = 0;
    int move_count = 0;

    shape->on_pointer_down([&down_count](flex::PointerEvent&) {
        down_count++;
    });

    shape->on_pointer_up([&up_count](flex::PointerEvent&) {
        up_count++;
    });

    shape->on_pointer_move([&move_count](flex::PointerEvent&) {
        move_count++;
    });

    flex::PointerEvent event;
    event.x = 50;
    event.y = 50;

    event.type = flex::PointerEventType::Down;
    shape->fire_pointer_down(event);
    REQUIRE(down_count == 1);

    event.type = flex::PointerEventType::Move;
    shape->fire_pointer_move(event);
    REQUIRE(move_count == 1);

    event.type = flex::PointerEventType::Up;
    shape->fire_pointer_up(event);
    REQUIRE(up_count == 1);
}

TEST_CASE("Node hover events", "[scene][node][event]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100, 100);

    bool entered = false;
    bool left = false;

    shape->on_hover_enter([&entered](flex::PointerEvent&) {
        entered = true;
    });

    shape->on_hover_leave([&left](flex::PointerEvent&) {
        left = true;
    });

    flex::PointerEvent event;
    event.x = 50;
    event.y = 50;

    event.type = flex::PointerEventType::Enter;
    shape->fire_hover_enter(event);
    REQUIRE(entered);

    event.type = flex::PointerEventType::Leave;
    shape->fire_hover_leave(event);
    REQUIRE(left);
}

TEST_CASE("Instance send_pointer_event fires click", "[instance][event]") {
    auto instance = flex::Instance::create(800, 600);

    auto shape = flex::Shape::create();
    shape->set_id("button");
    shape->set_position(100, 100);
    shape->set_rect(200, 100);
    instance->artboard()->add_child(shape);

    bool clicked = false;
    shape->on_click([&clicked]() {
        clicked = true;
    });

    // Click inside the shape
    instance->send_pointer_event(150, 150, true);   // Pointer down
    instance->send_pointer_event(150, 150, false);  // Pointer up

    REQUIRE(clicked);
}

TEST_CASE("Instance send_pointer_event tracks hover", "[instance][event]") {
    auto instance = flex::Instance::create(800, 600);

    auto shape = flex::Shape::create();
    shape->set_id("box");
    shape->set_position(100, 100);
    shape->set_rect(100, 100);
    instance->artboard()->add_child(shape);

    bool entered = false;
    bool left = false;

    shape->on_hover_enter([&entered](flex::PointerEvent&) {
        entered = true;
    });

    shape->on_hover_leave([&left](flex::PointerEvent&) {
        left = true;
    });

    // Move into the shape
    instance->send_pointer_event(150, 150, false);
    REQUIRE(entered);
    REQUIRE_FALSE(left);

    // Move out of the shape
    instance->send_pointer_event(50, 50, false);
    REQUIRE(left);
}

TEST_CASE("Instance send_pointer_event respects z-order", "[instance][event]") {
    auto instance = flex::Instance::create(800, 600);

    // Create two overlapping shapes
    auto back = flex::Shape::create();
    back->set_id("back");
    back->set_position(100, 100);
    back->set_rect(200, 200);

    auto front = flex::Shape::create();
    front->set_id("front");
    front->set_position(150, 150);
    front->set_rect(100, 100);

    // Add back first, then front (front is on top)
    instance->artboard()->add_child(back);
    instance->artboard()->add_child(front);

    std::string clicked_id;
    back->on_click([&clicked_id]() { clicked_id = "back"; });
    front->on_click([&clicked_id]() { clicked_id = "front"; });

    // Click in overlap area - should hit front
    instance->send_pointer_event(175, 175, true);
    instance->send_pointer_event(175, 175, false);

    REQUIRE(clicked_id == "front");

    // Reset and click outside front but inside back
    clicked_id.clear();
    instance->send_pointer_event(120, 120, true);
    instance->send_pointer_event(120, 120, false);

    REQUIRE(clicked_id == "back");
}

TEST_CASE("has_pointer_handlers returns correct state", "[scene][node][event]") {
    auto shape = flex::Shape::create();

    SECTION("No handlers") {
        REQUIRE_FALSE(shape->has_pointer_handlers());
    }

    SECTION("With click handler") {
        shape->on_click([]() {});
        REQUIRE(shape->has_pointer_handlers());
    }

    SECTION("With pointer_down handler") {
        shape->on_pointer_down([](flex::PointerEvent&) {});
        REQUIRE(shape->has_pointer_handlers());
    }

    SECTION("With hover handlers") {
        shape->on_hover_enter([](flex::PointerEvent&) {});
        REQUIRE(shape->has_pointer_handlers());
    }
}

TEST_CASE("Event bubbling propagates to parent", "[instance][event][bubbling]") {
    auto instance = flex::Instance::create(800, 600);

    // Create hierarchy: root -> group -> shape
    auto group = flex::Group::create();
    group->set_id("container");
    group->set_position(50, 50);
    instance->artboard()->add_child(group);

    auto shape = flex::Shape::create();
    shape->set_id("button");
    shape->set_position(50, 50);  // 100, 100 in global coords
    shape->set_rect(100, 50);
    group->add_child(shape);

    std::vector<std::string> event_order;

    // Both shape and group listen for pointer down
    shape->on_pointer_down([&event_order](flex::PointerEvent& e) {
        event_order.push_back("shape:" + std::string(e.phase == flex::EventPhase::Target ? "target" : "bubble"));
    });

    group->on_pointer_down([&event_order](flex::PointerEvent& e) {
        event_order.push_back("group:" + std::string(e.phase == flex::EventPhase::Bubble ? "bubble" : "other"));
    });

    // Click on the shape
    instance->send_pointer_event(150, 125, true);  // Inside shape bounds

    // Should fire in order: shape (target), then group (bubble)
    REQUIRE(event_order.size() == 2);
    REQUIRE(event_order[0] == "shape:target");
    REQUIRE(event_order[1] == "group:bubble");
}

TEST_CASE("Event bubbling can be stopped", "[instance][event][bubbling]") {
    auto instance = flex::Instance::create(800, 600);

    auto group = flex::Group::create();
    group->set_id("container");
    instance->artboard()->add_child(group);

    auto shape = flex::Shape::create();
    shape->set_id("button");
    shape->set_position(100, 100);
    shape->set_rect(100, 50);
    group->add_child(shape);

    bool shape_received = false;
    bool group_received = false;

    // Shape stops propagation
    shape->on_pointer_down([&shape_received](flex::PointerEvent& e) {
        shape_received = true;
        e.stop_propagation();
    });

    group->on_pointer_down([&group_received](flex::PointerEvent&) {
        group_received = true;
    });

    instance->send_pointer_event(150, 125, true);

    REQUIRE(shape_received);
    REQUIRE_FALSE(group_received);  // Should NOT receive due to stop_propagation
}

TEST_CASE("Event current_target updates during bubbling", "[instance][event][bubbling]") {
    auto instance = flex::Instance::create(800, 600);

    auto group = flex::Group::create();
    group->set_id("parent");
    instance->artboard()->add_child(group);

    auto shape = flex::Shape::create();
    shape->set_id("child");
    shape->set_position(100, 100);
    shape->set_rect(100, 50);
    group->add_child(shape);

    flex::Node* shape_current = nullptr;
    flex::Node* shape_target = nullptr;
    flex::Node* group_current = nullptr;
    flex::Node* group_target = nullptr;

    shape->on_pointer_down([&](flex::PointerEvent& e) {
        shape_current = e.current_target;
        shape_target = e.target;
    });

    group->on_pointer_down([&](flex::PointerEvent& e) {
        group_current = e.current_target;
        group_target = e.target;
    });

    instance->send_pointer_event(150, 125, true);

    // target should always be the original hit node (shape)
    REQUIRE(shape_target == shape.get());
    REQUIRE(group_target == shape.get());

    // current_target should be the node handling the event
    REQUIRE(shape_current == shape.get());
    REQUIRE(group_current == group.get());
}

// ============================================================================
// Solo Node Tests
// ============================================================================

TEST_CASE("Solo node creation", "[scene][solo]") {
    auto solo = flex::Solo::create();
    solo->set_id("tabs");

    REQUIRE(solo->type_name() == std::string("Solo"));
    REQUIRE(solo->id() == "tabs");
    REQUIRE(solo->active_index() == 0);
}

TEST_CASE("Solo shows only active child by index", "[scene][solo]") {
    auto solo = flex::Solo::create();

    auto child1 = flex::Shape::create();
    child1->set_id("tab1");
    child1->set_rect(100, 100);

    auto child2 = flex::Shape::create();
    child2->set_id("tab2");
    child2->set_rect(100, 100);

    auto child3 = flex::Shape::create();
    child3->set_id("tab3");
    child3->set_rect(100, 100);

    solo->add_child(child1);
    solo->add_child(child2);
    solo->add_child(child3);

    REQUIRE(solo->child_count() == 3);

    SECTION("Default active is first child") {
        REQUIRE(solo->active_index() == 0);
        REQUIRE(solo->active_child() == child1.get());
    }

    SECTION("Set active index") {
        solo->set_active_index(1);
        REQUIRE(solo->active_index() == 1);
        REQUIRE(solo->active_child() == child2.get());

        solo->set_active_index(2);
        REQUIRE(solo->active_child() == child3.get());
    }

    SECTION("Out of range index clamps") {
        solo->set_active_index(100);
        REQUIRE(solo->active_child() == child3.get());  // Last child

        solo->set_active_index(-5);
        REQUIRE(solo->active_child() == child1.get());  // First child
    }
}

TEST_CASE("Solo shows only active child by ID", "[scene][solo]") {
    auto solo = flex::Solo::create();

    auto child1 = flex::Shape::create();
    child1->set_id("page_home");

    auto child2 = flex::Shape::create();
    child2->set_id("page_settings");

    solo->add_child(child1);
    solo->add_child(child2);

    SECTION("Set active by ID") {
        solo->set_active_id("page_settings");
        REQUIRE(solo->active_child() == child2.get());
        REQUIRE(solo->active_id() == "page_settings");
    }

    SECTION("Invalid ID returns null") {
        solo->set_active_id("nonexistent");
        REQUIRE(solo->active_child() == nullptr);
    }
}

TEST_CASE("Solo next/previous cycling", "[scene][solo]") {
    auto solo = flex::Solo::create();

    auto c1 = flex::Shape::create();
    auto c2 = flex::Shape::create();
    auto c3 = flex::Shape::create();

    solo->add_child(c1);
    solo->add_child(c2);
    solo->add_child(c3);

    REQUIRE(solo->active_index() == 0);

    SECTION("next() cycles forward") {
        solo->next();
        REQUIRE(solo->active_index() == 1);

        solo->next();
        REQUIRE(solo->active_index() == 2);

        solo->next();
        REQUIRE(solo->active_index() == 0);  // Wraps around
    }

    SECTION("previous() cycles backward") {
        solo->previous();
        REQUIRE(solo->active_index() == 2);  // Wraps to end

        solo->previous();
        REQUIRE(solo->active_index() == 1);

        solo->previous();
        REQUIRE(solo->active_index() == 0);
    }
}

TEST_CASE("Solo property access", "[scene][solo]") {
    auto solo = flex::Solo::create();

    auto c1 = flex::Shape::create();
    auto c2 = flex::Shape::create();
    solo->add_child(c1);
    solo->add_child(c2);

    SECTION("Set active via property") {
        solo->set_property("active", flex::Value::from_float(1.0f));
        REQUIRE(solo->active_index() == 1);
    }

    SECTION("Get active via property") {
        solo->set_active_index(1);
        flex::Value val;
        REQUIRE(solo->get_property("active", &val));
        REQUIRE_THAT(val.as_float(), WithinAbs(1.0f, 0.01f));
    }

    SECTION("Set activeId via property") {
        c2->set_id("second");
        solo->set_property("activeId", flex::Value::from_string("second"));
        REQUIRE(solo->active_child() == c2.get());
    }
}

TEST_CASE("Solo with empty children", "[scene][solo]") {
    auto solo = flex::Solo::create();

    REQUIRE(solo->active_child() == nullptr);

    // These should not crash
    solo->next();
    solo->previous();
    REQUIRE(solo->active_child() == nullptr);
}

// =============================================================================
// Group Clipping Tests
// =============================================================================

TEST_CASE("Group clipping properties", "[scene][clipping]") {
    auto group = flex::Group::create();

    SECTION("Default clip state") {
        REQUIRE(group->clip() == false);
        REQUIRE(group->clip_width() == 0);
        REQUIRE(group->clip_height() == 0);
    }

    SECTION("Enable clipping") {
        group->set_clip(true);
        REQUIRE(group->clip() == true);
    }

    SECTION("Set clip size") {
        group->set_clip_size(200, 150);
        REQUIRE(group->clip_width() == 200);
        REQUIRE(group->clip_height() == 150);
    }

    SECTION("Clip via property system") {
        group->set_property("clip", flex::Value::from_float(1.0f));
        group->set_property("clipWidth", flex::Value::from_float(300.0f));
        group->set_property("clipHeight", flex::Value::from_float(250.0f));

        REQUIRE(group->clip() == true);
        REQUIRE(group->clip_width() == 300);
        REQUIRE(group->clip_height() == 250);
    }

    SECTION("Get clip via property system") {
        group->set_clip(true);
        group->set_clip_size(100, 80);

        flex::Value val;
        REQUIRE(group->get_property("clip", &val));
        REQUIRE(val.as_float() == 1.0f);

        REQUIRE(group->get_property("clipWidth", &val));
        REQUIRE(val.as_float() == 100.0f);

        REQUIRE(group->get_property("clipHeight", &val));
        REQUIRE(val.as_float() == 80.0f);
    }
}

TEST_CASE("Group bounds with clipping", "[scene][clipping]") {
    auto group = flex::Group::create();
    group->set_x(50);
    group->set_y(30);

    SECTION("Bounds with clip enabled") {
        group->set_clip(true);
        group->set_clip_size(200, 150);

        auto bounds = group->bounds();
        REQUIRE(bounds.x == 50);
        REQUIRE(bounds.y == 30);
        REQUIRE(bounds.width == 200);
        REQUIRE(bounds.height == 150);
    }

    SECTION("Bounds without clipping computes from children") {
        auto shape = flex::Shape::create();
        shape->set_rect(100, 80, 0);
        shape->set_x(10);
        shape->set_y(20);
        group->add_child(shape);

        // Without clip, bounds computed from children
        auto bounds = group->bounds();
        REQUIRE(bounds.x == 50);
        REQUIRE(bounds.y == 30);
        // Children bounds
        REQUIRE(bounds.width > 0);
        REQUIRE(bounds.height > 0);
    }
}

TEST_CASE("Group clipping with scale", "[scene][clipping]") {
    auto group = flex::Group::create();
    group->set_clip(true);
    group->set_clip_size(100, 100);
    group->set_scale(2.0f, 1.5f);

    auto bounds = group->bounds();
    REQUIRE(bounds.width == 200.0f);  // 100 * 2.0
    REQUIRE(bounds.height == 150.0f); // 100 * 1.5
}

// =============================================================================
// Timeline Trigger Tests
// =============================================================================

TEST_CASE("Timeline trigger basics", "[animation][trigger]") {
    auto timeline = flex::Timeline::create("test");

    SECTION("Add triggers") {
        timeline->add_trigger(0.5f, "midpoint");
        timeline->add_trigger(1.0f, "end");

        REQUIRE(timeline->trigger_count() == 2);
        REQUIRE(timeline->triggers()[0].time == 0.5f);
        REQUIRE(timeline->triggers()[0].event == "midpoint");
        REQUIRE(timeline->triggers()[1].time == 1.0f);
        REQUIRE(timeline->triggers()[1].event == "end");
    }

    SECTION("Triggers sorted by time") {
        timeline->add_trigger(1.0f, "end");
        timeline->add_trigger(0.25f, "quarter");
        timeline->add_trigger(0.5f, "half");

        REQUIRE(timeline->triggers()[0].time == 0.25f);
        REQUIRE(timeline->triggers()[1].time == 0.5f);
        REQUIRE(timeline->triggers()[2].time == 1.0f);
    }

    SECTION("Clear triggers") {
        timeline->add_trigger(0.5f, "test");
        timeline->clear_triggers();
        REQUIRE(timeline->trigger_count() == 0);
    }

    SECTION("Triggers affect auto duration") {
        timeline->add_trigger(2.0f, "late_event");
        REQUIRE(timeline->duration() == 2.0f);
    }
}

TEST_CASE("Timeline trigger firing during playback", "[animation][trigger]") {
    auto timeline = flex::Timeline::create("test");
    timeline->set_duration(1.0f);
    timeline->add_trigger(0.3f, "first");
    timeline->add_trigger(0.7f, "second");

    auto node = flex::Shape::create();
    flex::TimelinePlayer player(timeline, node.get());

    std::vector<std::string> fired_events;
    player.set_trigger_callback([&](const std::string& event) {
        fired_events.push_back(event);
    });

    SECTION("Triggers fire when crossed") {
        player.play();
        player.advance(0.4f);  // Cross 0.3

        REQUIRE(fired_events.size() == 1);
        REQUIRE(fired_events[0] == "first");
    }

    SECTION("Multiple triggers in one advance") {
        player.play();
        player.advance(0.8f);  // Cross both 0.3 and 0.7

        REQUIRE(fired_events.size() == 2);
        REQUIRE(fired_events[0] == "first");
        REQUIRE(fired_events[1] == "second");
    }

    SECTION("Triggers fire exactly once per crossing") {
        player.play();
        player.advance(0.35f);  // Cross 0.3
        player.advance(0.1f);   // Stay between 0.3 and 0.7

        REQUIRE(fired_events.size() == 1);
    }
}

TEST_CASE("Timeline triggers with looping", "[animation][trigger]") {
    auto timeline = flex::Timeline::create("test");
    timeline->set_duration(1.0f);
    timeline->set_loop_mode(flex::LoopMode::Loop);
    timeline->add_trigger(0.5f, "midpoint");

    auto node = flex::Shape::create();
    flex::TimelinePlayer player(timeline, node.get());

    std::vector<std::string> fired_events;
    player.set_trigger_callback([&](const std::string& event) {
        fired_events.push_back(event);
    });

    player.play();

    SECTION("Trigger fires on each loop") {
        player.advance(0.6f);  // 0 -> 0.6, crosses 0.5
        REQUIRE(fired_events.size() == 1);

        player.advance(0.6f);  // 0.6 -> 1.2, loops to 0.2 (no crossing)
        REQUIRE(fired_events.size() == 1);

        player.advance(0.4f);  // 0.2 -> 0.6, crosses 0.5 again
        REQUIRE(fired_events.size() == 2);
    }
}

TEST_CASE("AnimationController trigger callback", "[animation][trigger]") {
    flex::AnimationController controller;

    auto timeline = flex::Timeline::create("test");
    timeline->set_duration(1.0f);
    timeline->add_trigger(0.5f, "event");
    controller.add_timeline(timeline);

    auto node = flex::Shape::create();

    std::vector<std::string> fired_events;
    controller.set_trigger_callback([&](const std::string& event) {
        fired_events.push_back(event);
    });

    controller.play("test", node.get());
    controller.advance(0.6f);

    REQUIRE(fired_events.size() == 1);
    REQUIRE(fired_events[0] == "event");
}

// =============================================================================
// Math Function Tests
// =============================================================================

TEST_CASE("Script math functions", "[script][math]") {
    flex::ScriptContext ctx;
    flex::script::bind_math(&ctx);

    SECTION("Trigonometric functions") {
        auto result = ctx.eval("sin(0)");
        REQUIRE(result.is_number());
        REQUIRE_THAT(result.as_number(), WithinAbs(0.0, 0.001));

        result = ctx.eval("cos(0)");
        REQUIRE_THAT(result.as_number(), WithinAbs(1.0, 0.001));

        result = ctx.eval("sin(PI / 2)");
        REQUIRE_THAT(result.as_number(), WithinAbs(1.0, 0.001));
    }

    SECTION("lerp function") {
        auto result = ctx.eval("lerp(0, 100, 0.5)");
        REQUIRE_THAT(result.as_number(), WithinAbs(50.0, 0.001));

        result = ctx.eval("lerp(10, 20, 0)");
        REQUIRE_THAT(result.as_number(), WithinAbs(10.0, 0.001));

        result = ctx.eval("lerp(10, 20, 1)");
        REQUIRE_THAT(result.as_number(), WithinAbs(20.0, 0.001));
    }

    SECTION("clamp function") {
        auto result = ctx.eval("clamp(5, 0, 10)");
        REQUIRE_THAT(result.as_number(), WithinAbs(5.0, 0.001));

        result = ctx.eval("clamp(-5, 0, 10)");
        REQUIRE_THAT(result.as_number(), WithinAbs(0.0, 0.001));

        result = ctx.eval("clamp(15, 0, 10)");
        REQUIRE_THAT(result.as_number(), WithinAbs(10.0, 0.001));
    }

    SECTION("min/max functions") {
        auto result = ctx.eval("min(3, 7)");
        REQUIRE_THAT(result.as_number(), WithinAbs(3.0, 0.001));

        result = ctx.eval("max(3, 7)");
        REQUIRE_THAT(result.as_number(), WithinAbs(7.0, 0.001));
    }

    SECTION("abs function") {
        auto result = ctx.eval("abs(-5)");
        REQUIRE_THAT(result.as_number(), WithinAbs(5.0, 0.001));

        result = ctx.eval("abs(5)");
        REQUIRE_THAT(result.as_number(), WithinAbs(5.0, 0.001));
    }

    SECTION("floor/ceil/round") {
        auto result = ctx.eval("floor(3.7)");
        REQUIRE_THAT(result.as_number(), WithinAbs(3.0, 0.001));

        result = ctx.eval("ceil(3.2)");
        REQUIRE_THAT(result.as_number(), WithinAbs(4.0, 0.001));

        result = ctx.eval("round(3.5)");
        REQUIRE_THAT(result.as_number(), WithinAbs(4.0, 0.001));
    }

    SECTION("pow/sqrt") {
        auto result = ctx.eval("pow(2, 3)");
        REQUIRE_THAT(result.as_number(), WithinAbs(8.0, 0.001));

        result = ctx.eval("sqrt(16)");
        REQUIRE_THAT(result.as_number(), WithinAbs(4.0, 0.001));
    }

    SECTION("Constants") {
        auto result = ctx.eval("PI");
        REQUIRE_THAT(result.as_number(), WithinAbs(3.14159, 0.0001));

        result = ctx.eval("E");
        REQUIRE_THAT(result.as_number(), WithinAbs(2.71828, 0.0001));
    }

    SECTION("Complex expressions") {
        auto result = ctx.eval("sin(PI) * 100 + 50");
        REQUIRE_THAT(result.as_number(), WithinAbs(50.0, 0.01));

        result = ctx.eval("lerp(0, 100, clamp(1.5, 0, 1))");
        REQUIRE_THAT(result.as_number(), WithinAbs(100.0, 0.001));
    }
}

// =============================================================================
// State Machine Listener Tests
// =============================================================================

TEST_CASE("State machine enter/exit listeners", "[machine][listener]") {
    auto layer = flex::Layer::create("test");
    layer->add_state("Idle");
    layer->add_state("Running");
    layer->add_transition("Idle", "Running")->when_event("start");
    layer->add_transition("Running", "Idle")->when_event("stop");
    layer->set_initial_state("Idle");
    layer->init();

    std::vector<std::string> events;

    layer->on_enter("Running", [&]() {
        events.push_back("enter_running");
    });

    layer->on_exit("Running", [&]() {
        events.push_back("exit_running");
    });

    layer->on_enter("Idle", [&]() {
        events.push_back("enter_idle");
    });

    auto get_input = [](const std::string&) { return 0.0f; };
    auto is_anim_finished = [](const std::string&) { return false; };

    SECTION("Enter listener fires on transition") {
        auto is_event = [](const std::string& e) { return e == "start"; };

        layer->update(0.1f, get_input, is_event, is_anim_finished);

        REQUIRE(events.size() == 1);
        REQUIRE(events[0] == "enter_running");
    }

    SECTION("Exit listener fires on transition") {
        // First transition to Running
        auto start_event = [](const std::string& e) { return e == "start"; };
        layer->update(0.1f, get_input, start_event, is_anim_finished);
        events.clear();

        // Then transition back to Idle
        auto stop_event = [](const std::string& e) { return e == "stop"; };
        layer->update(0.1f, get_input, stop_event, is_anim_finished);

        REQUIRE(events.size() == 2);
        REQUIRE(events[0] == "exit_running");
        REQUIRE(events[1] == "enter_idle");
    }

    SECTION("Multiple listeners for same state") {
        int count = 0;
        layer->on_enter("Running", [&]() { count++; });
        layer->on_enter("Running", [&]() { count++; });

        auto is_event = [](const std::string& e) { return e == "start"; };
        layer->update(0.1f, get_input, is_event, is_anim_finished);

        // 3 total: original + 2 new
        REQUIRE(count == 2);
        REQUIRE(events.size() == 1);  // Original listener
    }
}

TEST_CASE("State change callback with listeners", "[machine][listener]") {
    auto layer = flex::Layer::create("test");
    layer->add_state("A");
    layer->add_state("B");
    layer->add_transition("A", "B")->when_event("go");
    layer->set_initial_state("A");
    layer->init();

    std::vector<std::string> order;

    layer->on_exit("A", [&]() { order.push_back("exit_A"); });
    layer->on_enter("B", [&]() { order.push_back("enter_B"); });
    layer->on_state_change([&](const std::string& from, const std::string& to) {
        order.push_back("change_" + from + "_" + to);
    });

    auto get_input = [](const std::string&) { return 0.0f; };
    auto is_event = [](const std::string& e) { return e == "go"; };
    auto is_anim_finished = [](const std::string&) { return false; };

    layer->update(0.1f, get_input, is_event, is_anim_finished);

    // Order: exit -> enter -> state_change
    REQUIRE(order.size() == 3);
    REQUIRE(order[0] == "exit_A");
    REQUIRE(order[1] == "enter_B");
    REQUIRE(order[2] == "change_A_B");
}

// =============================================================================
// Extended Easing Tests
// =============================================================================

TEST_CASE("Extended easing functions", "[easing]") {
    SECTION("Bounce easing") {
        auto bounce_out = flex::Easing::bounce_out();

        // Start and end points
        REQUIRE(bounce_out.evaluate(0.0f) == 0.0f);
        REQUIRE(bounce_out.evaluate(1.0f) == 1.0f);

        // Bounce should overshoot a bit then settle
        float mid = bounce_out.evaluate(0.5f);
        REQUIRE(mid > 0.0f);
        REQUIRE(mid < 1.0f);

        // Bounce in is reverse
        auto bounce_in = flex::Easing::bounce_in();
        REQUIRE(bounce_in.evaluate(0.0f) == 0.0f);
        REQUIRE(bounce_in.evaluate(1.0f) == 1.0f);
    }

    SECTION("Elastic easing") {
        auto elastic_out = flex::Easing::elastic_out();

        REQUIRE(elastic_out.evaluate(0.0f) == 0.0f);
        REQUIRE(elastic_out.evaluate(1.0f) == 1.0f);

        // Elastic can overshoot (go above 1.0)
        float late = elastic_out.evaluate(0.9f);
        REQUIRE(late > 0.5f);
    }

    SECTION("Back easing") {
        auto back_out = flex::Easing::back_out();

        REQUIRE(back_out.evaluate(0.0f) == 0.0f);
        REQUIRE(back_out.evaluate(1.0f) == 1.0f);

        // Back can overshoot
        float mid = back_out.evaluate(0.5f);
        REQUIRE(mid > 0.5f);  // Back out overshoots

        auto back_in = flex::Easing::back_in();
        float early = back_in.evaluate(0.2f);
        REQUIRE(early < 0.0f);  // Back in goes negative
    }

    SECTION("Expo easing") {
        auto expo_out = flex::Easing::expo_out();

        REQUIRE(expo_out.evaluate(0.0f) == 0.0f);
        REQUIRE(expo_out.evaluate(1.0f) == 1.0f);

        // Expo out starts fast
        float early = expo_out.evaluate(0.3f);
        REQUIRE(early > 0.5f);

        auto expo_in = flex::Easing::expo_in();
        float late = expo_in.evaluate(0.7f);
        REQUIRE(late < 0.5f);  // Expo in ends fast
    }

    SECTION("Circ easing") {
        auto circ_out = flex::Easing::circ_out();

        REQUIRE(circ_out.evaluate(0.0f) == 0.0f);
        REQUIRE(circ_out.evaluate(1.0f) == 1.0f);

        // Circ follows a circular curve
        float mid = circ_out.evaluate(0.5f);
        REQUIRE(mid > 0.5f);  // Circ out is fast at start
    }

    SECTION("Custom parameters") {
        // Elastic with custom amplitude and period
        auto elastic = flex::Easing::elastic_out(2.0f, 0.5f);
        float val = elastic.evaluate(0.5f);
        REQUIRE(val != 0.0f);

        // Back with custom overshoot
        auto back = flex::Easing::back_out(2.5f);
        float mid = back.evaluate(0.5f);
        REQUIRE(mid > 0.5f);
    }
}

// =============================================================================
// Parser Integration Tests: Timeline
// =============================================================================

TEST_CASE("Parser loads Timeline with tracks", "[parser][timeline]") {
    const char* source = R"(
        Artboard "Main" (800, 600) {
        }

        Timeline "FadeIn" {
            track "opacity" {
                0 -> 0
                1 -> 1
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());
    if (def->has_error()) {
        FAIL(def->error_message());
    }

    auto instance = flex::Instance::create(def);
    REQUIRE(instance->artboard() != nullptr);

    // Verify timeline was loaded
    auto* controller = instance->animation_controller();
    REQUIRE(controller != nullptr);

    // Play the timeline to verify it works
    auto shape = flex::Shape::create();
    shape->set_opacity(0.0f);
    instance->artboard()->add_child(shape);

    auto* player = instance->play("FadeIn", shape.get());
    REQUIRE(player != nullptr);

    // Advance halfway
    player->advance(0.5f);
    REQUIRE_THAT(shape->opacity(), WithinAbs(0.5f, 0.1f));
}

TEST_CASE("Parser loads Timeline with multiple tracks", "[parser][timeline]") {
    const char* source = R"(
        Artboard "Main" (800, 600) {
        }

        Timeline "Move" {
            track "x" {
                0 -> 0
                1 -> 100
            }
            track "y" {
                0 -> 0
                1 -> 200
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());

    auto instance = flex::Instance::create(def);

    auto shape = flex::Shape::create();
    shape->set_position(0, 0);
    instance->artboard()->add_child(shape);

    auto* player = instance->play("Move", shape.get());
    REQUIRE(player != nullptr);

    player->advance(1.0f);
    REQUIRE_THAT(shape->x(), WithinAbs(100.0f, 1.0f));
    REQUIRE_THAT(shape->y(), WithinAbs(200.0f, 1.0f));
}

// =============================================================================
// Parser Integration Tests: State Machine
// =============================================================================

TEST_CASE("Parser loads Machine with states", "[parser][machine]") {
    const char* source = R"(
        Artboard "Main" (800, 600) {
        }

        Machine "Controller" {
            Layer "Base" {
                State Idle {
                }
                State Running {
                }
                transition Idle -> Running {
                    when Go
                }
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());
    if (def->has_error()) {
        FAIL(def->error_message());
    }

    auto instance = flex::Instance::create(def);
    REQUIRE(instance->artboard() != nullptr);
    REQUIRE(instance->machine() != nullptr);

    // Verify initial state
    REQUIRE(instance->current_state("Base") == "Idle");

    // Trigger transition
    instance->send_event("Go");
    instance->advance(0.016f);

    REQUIRE(instance->current_state("Base") == "Running");
}

TEST_CASE("Parser loads Machine with animation playback", "[parser][machine]") {
    const char* source = R"(
        Artboard "Main" (800, 600) {
        }

        Timeline "FadeIn" {
            track "opacity" {
                0 -> 0
                0.5 -> 1
            }
        }

        Machine "Controller" {
            Layer "Base" {
                State Hidden {
                }
                State Visible {
                    play "FadeIn"
                }
                transition Hidden -> Visible {
                    when Show
                }
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());

    auto instance = flex::Instance::create(def);
    REQUIRE(instance->machine() != nullptr);
    REQUIRE(instance->current_state("Base") == "Hidden");
}

// =============================================================================
// Parser Integration Tests: Inputs
// =============================================================================

TEST_CASE("Parser loads Inputs block", "[parser][inputs]") {
    const char* source = R"(
        Inputs {
            float Speed = 10.5
            bool Enabled = true
            string Name = "Player"
        }

        Artboard "Main" (800, 600) {
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());
    if (def->has_error()) {
        FAIL(def->error_message());
    }

    auto instance = flex::Instance::create(def);
    REQUIRE(instance->artboard() != nullptr);

    // Verify inputs were loaded with default values
    REQUIRE_THAT(instance->get_input("Speed"), WithinAbs(10.5f, 0.01f));
}

// =============================================================================
// Parser Integration Tests: Full End-to-End
// =============================================================================

TEST_CASE("Parser loads complete .flex file", "[parser][e2e]") {
    const char* source = R"(
        Inputs {
            float Progress = 0
        }

        Artboard "App" (1920, 1080) {
            Group "Container" {
                x: 100
                y: 50

                Rect {
                    width: 200
                    height: 100
                    Fill { color: "#3498db" }
                }

                Circle {
                    radius: 25
                    x: 250
                    Fill { color: "#e74c3c" }
                }
            }
        }

        Timeline "Intro" {
            track "opacity" {
                0 -> 0
                1 -> 1
            }
        }

        Machine "Main" {
            Layer "UI" {
                State Initial {
                }
                State Ready {
                    play "Intro"
                }
                transition Initial -> Ready {
                    when Start
                }
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());
    if (def->has_error()) {
        FAIL(def->error_message());
    }

    auto instance = flex::Instance::create(def);

    // Verify artboard
    auto* artboard = instance->artboard();
    REQUIRE(artboard != nullptr);
    REQUIRE_THAT(artboard->width(), WithinAbs(1920.0f, 0.01f));
    REQUIRE_THAT(artboard->height(), WithinAbs(1080.0f, 0.01f));

    // Verify scene graph
    auto* container = artboard->find("Container");
    REQUIRE(container != nullptr);
    REQUIRE_THAT(container->x(), WithinAbs(100.0f, 0.01f));
    REQUIRE_THAT(container->y(), WithinAbs(50.0f, 0.01f));

    // Verify machine
    REQUIRE(instance->machine() != nullptr);
    REQUIRE(instance->current_state("UI") == "Initial");

    // Trigger state change
    instance->send_event("Start");
    instance->advance(0.016f);
    REQUIRE(instance->current_state("UI") == "Ready");
}

TEST_CASE("Parser reports syntax errors with location", "[parser][error]") {
    const char* source = R"(
        Artboard "Main" (800, 600) {
            Group "Test" {
                x: 100
                y: // Missing value
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE(def->has_error());
    REQUIRE(def->error_line() > 0);
}

TEST_CASE("Parser handles nested groups", "[parser][scene]") {
    const char* source = R"(
        Artboard "Main" (800, 600) {
            Group "Level1" {
                x: 10
                Group "Level2" {
                    x: 20
                    Group "Level3" {
                        x: 30
                        Rect {
                            width: 50
                            height: 50
                        }
                    }
                }
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());

    auto instance = flex::Instance::create(def);
    auto* artboard = instance->artboard();

    auto* level1 = artboard->find("Level1");
    REQUIRE(level1 != nullptr);
    REQUIRE_THAT(level1->x(), WithinAbs(10.0f, 0.01f));

    auto* level2 = artboard->find("Level2");
    REQUIRE(level2 != nullptr);
    REQUIRE_THAT(level2->x(), WithinAbs(20.0f, 0.01f));

    auto* level3 = artboard->find("Level3");
    REQUIRE(level3 != nullptr);
    REQUIRE_THAT(level3->x(), WithinAbs(30.0f, 0.01f));
}

// =============================================================================
// Parser Integration Tests: Instance Node
// =============================================================================

TEST_CASE("Parser loads Instance node", "[parser][instance]") {
    const char* source = R"(
        Artboard "Main" (800, 600) {
            Instance "Button" {
                source: "components/button.flex"
                x: 100
                y: 50
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());
    if (def->has_error()) {
        FAIL(def->error_message());
    }

    auto instance = flex::Instance::create(def);
    auto* artboard = instance->artboard();
    REQUIRE(artboard != nullptr);

    auto* button = artboard->find("Button");
    REQUIRE(button != nullptr);
    REQUIRE(button->type() == flex::NodeType::Instance);
    REQUIRE_THAT(button->x(), WithinAbs(100.0f, 0.01f));
    REQUIRE_THAT(button->y(), WithinAbs(50.0f, 0.01f));

    auto* inst_node = dynamic_cast<flex::InstanceNode*>(button);
    REQUIRE(inst_node != nullptr);
    REQUIRE(inst_node->source() == "components/button.flex");
}

TEST_CASE("Parser loads Instance node in Group", "[parser][instance]") {
    const char* source = R"(
        Artboard "Main" (800, 600) {
            Group "Container" {
                x: 50
                Instance "Icon" {
                    source: "icons/star.flex"
                }
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());

    auto instance = flex::Instance::create(def);
    auto* artboard = instance->artboard();

    auto* container = artboard->find("Container");
    REQUIRE(container != nullptr);

    auto* icon = artboard->find("Icon");
    REQUIRE(icon != nullptr);
    REQUIRE(icon->type() == flex::NodeType::Instance);
}

TEST_CASE("Parser loads multiple Instance nodes", "[parser][instance]") {
    const char* source = R"(
        Artboard "Main" (800, 600) {
            Instance "Header" {
                source: "header.flex"
                y: 0
            }
            Instance "Content" {
                source: "content.flex"
                y: 100
            }
            Instance "Footer" {
                source: "footer.flex"
                y: 500
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());

    auto instance = flex::Instance::create(def);
    auto* artboard = instance->artboard();

    auto* header = artboard->find("Header");
    auto* content = artboard->find("Content");
    auto* footer = artboard->find("Footer");

    REQUIRE(header != nullptr);
    REQUIRE(content != nullptr);
    REQUIRE(footer != nullptr);

    REQUIRE_THAT(dynamic_cast<flex::InstanceNode*>(header)->source(),
                 Catch::Matchers::Equals("header.flex"));
    REQUIRE_THAT(dynamic_cast<flex::InstanceNode*>(content)->source(),
                 Catch::Matchers::Equals("content.flex"));
    REQUIRE_THAT(dynamic_cast<flex::InstanceNode*>(footer)->source(),
                 Catch::Matchers::Equals("footer.flex"));
}

// =============================================================================
// Parser Integration Tests: Assets
// =============================================================================

TEST_CASE("Parser loads Assets block", "[parser][assets]") {
    const char* source = R"(
        Assets {
            Button "components/button.flex" {}
            Icon "icons/star.flex" {}
        }

        Artboard "Main" (800, 600) {
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());
    if (def->has_error()) {
        FAIL(def->error_message());
    }

    auto instance = flex::Instance::create(def);

    // Verify assets were loaded
    REQUIRE(instance->resolve_asset("Button") == "components/button.flex");
    REQUIRE(instance->resolve_asset("Icon") == "icons/star.flex");
    REQUIRE(instance->resolve_asset("NonExistent").empty());
}

TEST_CASE("Instance node resolves @asset reference", "[parser][assets][instance]") {
    const char* source = R"(
        Assets {
            MyButton "ui/button.flex" {}
        }

        Artboard "Main" (800, 600) {
            Instance "Btn" {
                source: "@MyButton"
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());
    if (def->has_error()) {
        FAIL(def->error_message());
    }

    auto instance = flex::Instance::create(def);
    auto* artboard = instance->artboard();

    auto* btn = artboard->find("Btn");
    REQUIRE(btn != nullptr);

    auto* inst_node = dynamic_cast<flex::InstanceNode*>(btn);
    REQUIRE(inst_node != nullptr);
    // Source should be resolved from @MyButton to "ui/button.flex"
    REQUIRE(inst_node->source() == "ui/button.flex");
}

TEST_CASE("Instance node with unresolved @asset keeps original", "[parser][assets][instance]") {
    const char* source = R"(
        Assets {
            Button "button.flex" {}
        }

        Artboard "Main" (800, 600) {
            Instance "Unknown" {
                source: "@NonExistent"
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());

    auto instance = flex::Instance::create(def);
    auto* artboard = instance->artboard();

    auto* unknown = artboard->find("Unknown");
    REQUIRE(unknown != nullptr);

    auto* inst_node = dynamic_cast<flex::InstanceNode*>(unknown);
    REQUIRE(inst_node != nullptr);
    // Unresolved @asset keeps the original string
    REQUIRE(inst_node->source() == "@NonExistent");
}

TEST_CASE("Multiple Instance nodes with @asset references", "[parser][assets][instance]") {
    const char* source = R"(
        Assets {
            Header "components/header.flex" {}
            Footer "components/footer.flex" {}
            Button "ui/button.flex" {}
        }

        Artboard "Main" (800, 600) {
            Instance "TopBar" {
                source: "@Header"
                y: 0
            }
            Instance "BottomBar" {
                source: "@Footer"
                y: 550
            }
            Group "Content" {
                Instance "SubmitBtn" {
                    source: "@Button"
                    x: 100
                }
            }
        }
    )";

    auto def = flex::Definition::load(source);
    REQUIRE_FALSE(def->has_error());

    auto instance = flex::Instance::create(def);
    auto* artboard = instance->artboard();

    auto* topbar = dynamic_cast<flex::InstanceNode*>(artboard->find("TopBar"));
    auto* bottombar = dynamic_cast<flex::InstanceNode*>(artboard->find("BottomBar"));
    auto* submitbtn = dynamic_cast<flex::InstanceNode*>(artboard->find("SubmitBtn"));

    REQUIRE(topbar != nullptr);
    REQUIRE(bottombar != nullptr);
    REQUIRE(submitbtn != nullptr);

    REQUIRE(topbar->source() == "components/header.flex");
    REQUIRE(bottombar->source() == "components/footer.flex");
    REQUIRE(submitbtn->source() == "ui/button.flex");
}

// ============================================================================
// Layout System (Flexbox) Tests
// ============================================================================

TEST_CASE("Group basic flex layout - row direction", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    // Add three children with fixed sizes
    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    auto child2 = flex::Group::create();
    child2->set_layout_size(60, 40);
    auto child3 = flex::Group::create();
    child3->set_layout_size(70, 50);

    group->add_child(child1);
    group->add_child(child2);
    group->add_child(child3);

    // Trigger layout
    group->perform_layout();

    // Children should be laid out left to right
    REQUIRE(child1->x() == Catch::Approx(0).margin(0.01));
    REQUIRE(child2->x() == Catch::Approx(50).margin(0.01));
    REQUIRE(child3->x() == Catch::Approx(110).margin(0.01));

    // Y should be at start (default align)
    REQUIRE(child1->y() == Catch::Approx(0).margin(0.01));
    REQUIRE(child2->y() == Catch::Approx(0).margin(0.01));
    REQUIRE(child3->y() == Catch::Approx(0).margin(0.01));
}

TEST_CASE("Group flex layout - column direction", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Column);
    group->set_clip_size(100, 300);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    auto child2 = flex::Group::create();
    child2->set_layout_size(60, 40);
    auto child3 = flex::Group::create();
    child3->set_layout_size(70, 50);

    group->add_child(child1);
    group->add_child(child2);
    group->add_child(child3);

    group->perform_layout();

    // Children should be laid out top to bottom
    REQUIRE(child1->y() == Catch::Approx(0).margin(0.01));
    REQUIRE(child2->y() == Catch::Approx(30).margin(0.01));
    REQUIRE(child3->y() == Catch::Approx(70).margin(0.01));

    // X should be at start
    REQUIRE(child1->x() == Catch::Approx(0).margin(0.01));
    REQUIRE(child2->x() == Catch::Approx(0).margin(0.01));
    REQUIRE(child3->x() == Catch::Approx(0).margin(0.01));
}

TEST_CASE("Group flex layout - justify content center", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_justify_content(flex::JustifyContent::Center);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 30);

    group->add_child(child1);
    group->add_child(child2);

    group->perform_layout();

    // Total content = 100, free space = 200, centered = offset 100
    REQUIRE(child1->x() == Catch::Approx(100).margin(0.01));
    REQUIRE(child2->x() == Catch::Approx(150).margin(0.01));
}

TEST_CASE("Group flex layout - justify content end", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_justify_content(flex::JustifyContent::End);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 30);

    group->add_child(child1);
    group->add_child(child2);

    group->perform_layout();

    // Total content = 100, free space = 200, at end
    REQUIRE(child1->x() == Catch::Approx(200).margin(0.01));
    REQUIRE(child2->x() == Catch::Approx(250).margin(0.01));
}

TEST_CASE("Group flex layout - justify content space-between", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_justify_content(flex::JustifyContent::SpaceBetween);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 30);
    auto child3 = flex::Group::create();
    child3->set_layout_size(50, 30);

    group->add_child(child1);
    group->add_child(child2);
    group->add_child(child3);

    group->perform_layout();

    // Total content = 150, free space = 150, gap = 75 each
    REQUIRE(child1->x() == Catch::Approx(0).margin(0.01));
    REQUIRE(child2->x() == Catch::Approx(125).margin(0.01));
    REQUIRE(child3->x() == Catch::Approx(250).margin(0.01));
}

TEST_CASE("Group flex layout - align items center", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_align_items(flex::AlignItems::Center);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 20);
    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 40);
    auto child3 = flex::Group::create();
    child3->set_layout_size(50, 60);

    group->add_child(child1);
    group->add_child(child2);
    group->add_child(child3);

    group->perform_layout();

    // Children should be vertically centered
    REQUIRE(child1->y() == Catch::Approx(40).margin(0.01));  // (100-20)/2
    REQUIRE(child2->y() == Catch::Approx(30).margin(0.01));  // (100-40)/2
    REQUIRE(child3->y() == Catch::Approx(20).margin(0.01));  // (100-60)/2
}

TEST_CASE("Group flex layout - align items end", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_align_items(flex::AlignItems::End);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 20);
    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 40);

    group->add_child(child1);
    group->add_child(child2);

    group->perform_layout();

    // Children should be at bottom
    REQUIRE(child1->y() == Catch::Approx(80).margin(0.01));  // 100-20
    REQUIRE(child2->y() == Catch::Approx(60).margin(0.01));  // 100-40
}

TEST_CASE("Group flex layout - flex grow", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    child1->set_flex_grow(1);

    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 30);
    child2->set_flex_grow(2);

    group->add_child(child1);
    group->add_child(child2);

    group->perform_layout();

    // Base: 100, free space: 200
    // child1 gets 200 * 1/3 = 66.67 extra -> 116.67 total
    // child2 gets 200 * 2/3 = 133.33 extra -> 183.33 total
    REQUIRE(child1->x() == Catch::Approx(0).margin(0.01));
    float child1_width = child2->x();  // child2 starts where child1 ends
    REQUIRE(child1_width == Catch::Approx(116.67).margin(0.1));
}

TEST_CASE("Group flex layout - flex shrink", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_clip_size(200, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(150, 30);
    child1->set_flex_shrink(1);

    auto child2 = flex::Group::create();
    child2->set_layout_size(150, 30);
    child2->set_flex_shrink(2);

    group->add_child(child1);
    group->add_child(child2);

    group->perform_layout();

    // Base: 300, available: 200, need to shrink 100
    // child1 shrinks 100 * 1/3 = 33.33 -> 116.67
    // child2 shrinks 100 * 2/3 = 66.67 -> 83.33
    float child1_end = child2->x();
    REQUIRE(child1_end == Catch::Approx(116.67).margin(0.1));
}

TEST_CASE("Group flex layout - gap", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_gap(10);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 30);
    auto child3 = flex::Group::create();
    child3->set_layout_size(50, 30);

    group->add_child(child1);
    group->add_child(child2);
    group->add_child(child3);

    group->perform_layout();

    // With gap=10: positions are 0, 60, 120
    REQUIRE(child1->x() == Catch::Approx(0).margin(0.01));
    REQUIRE(child2->x() == Catch::Approx(60).margin(0.01));
    REQUIRE(child3->x() == Catch::Approx(120).margin(0.01));
}

TEST_CASE("Group flex layout - padding", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_padding(20);  // All sides
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 30);

    group->add_child(child1);
    group->add_child(child2);

    group->perform_layout();

    // With padding=20 on all sides
    REQUIRE(child1->x() == Catch::Approx(20).margin(0.01));
    REQUIRE(child2->x() == Catch::Approx(70).margin(0.01));
    REQUIRE(child1->y() == Catch::Approx(20).margin(0.01));
    REQUIRE(child2->y() == Catch::Approx(20).margin(0.01));
}

TEST_CASE("Group flex layout - row reverse", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::RowReverse);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    auto child2 = flex::Group::create();
    child2->set_layout_size(60, 30);
    auto child3 = flex::Group::create();
    child3->set_layout_size(70, 30);

    group->add_child(child1);
    group->add_child(child2);
    group->add_child(child3);

    group->perform_layout();

    // In row-reverse, items go right to left
    // child1 at far right, child3 at far left
    REQUIRE(child1->x() == Catch::Approx(250).margin(0.01));  // 300-50
    REQUIRE(child2->x() == Catch::Approx(190).margin(0.01));  // 250-60
    REQUIRE(child3->x() == Catch::Approx(120).margin(0.01));  // 190-70
}

TEST_CASE("Group flex layout - column reverse", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::ColumnReverse);
    group->set_clip_size(100, 300);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 40);
    auto child3 = flex::Group::create();
    child3->set_layout_size(50, 50);

    group->add_child(child1);
    group->add_child(child2);
    group->add_child(child3);

    group->perform_layout();

    // In column-reverse, items go bottom to top
    REQUIRE(child1->y() == Catch::Approx(270).margin(0.01));  // 300-30
    REQUIRE(child2->y() == Catch::Approx(230).margin(0.01));  // 270-40
    REQUIRE(child3->y() == Catch::Approx(180).margin(0.01));  // 230-50
}

TEST_CASE("Group flex layout - align self override", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_align_items(flex::AlignItems::Start);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    // Uses parent's align_items (Start)

    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 30);
    child2->set_align_self(flex::AlignSelf::Center);

    auto child3 = flex::Group::create();
    child3->set_layout_size(50, 30);
    child3->set_align_self(flex::AlignSelf::End);

    group->add_child(child1);
    group->add_child(child2);
    group->add_child(child3);

    group->perform_layout();

    REQUIRE(child1->y() == Catch::Approx(0).margin(0.01));   // Start
    REQUIRE(child2->y() == Catch::Approx(35).margin(0.01)); // Center: (100-30)/2
    REQUIRE(child3->y() == Catch::Approx(70).margin(0.01)); // End: 100-30
}

TEST_CASE("Group flex layout - no layout mode", "[layout][flexbox]") {
    auto group = flex::Group::create();
    // Default is LayoutMode::None

    auto child1 = flex::Group::create();
    child1->set_position(10, 20);
    child1->set_layout_size(50, 30);

    auto child2 = flex::Group::create();
    child2->set_position(100, 50);
    child2->set_layout_size(50, 30);

    group->add_child(child1);
    group->add_child(child2);

    group->perform_layout();

    // Positions should be unchanged (manual positioning)
    REQUIRE(child1->x() == Catch::Approx(10).margin(0.01));
    REQUIRE(child1->y() == Catch::Approx(20).margin(0.01));
    REQUIRE(child2->x() == Catch::Approx(100).margin(0.01));
    REQUIRE(child2->y() == Catch::Approx(50).margin(0.01));
}

TEST_CASE("Group flex layout - space-around", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_justify_content(flex::JustifyContent::SpaceAround);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 30);

    group->add_child(child1);
    group->add_child(child2);

    group->perform_layout();

    // Total content = 100, free space = 200
    // Space per item = 100, half-space at edges = 50
    // child1 at 50, child2 at 50 + 50 + 100 = 200
    REQUIRE(child1->x() == Catch::Approx(50).margin(0.01));
    REQUIRE(child2->x() == Catch::Approx(200).margin(0.01));
}

TEST_CASE("Group flex layout - space-evenly", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_justify_content(flex::JustifyContent::SpaceEvenly);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 30);
    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 30);

    group->add_child(child1);
    group->add_child(child2);

    group->perform_layout();

    // Total content = 100, free space = 200
    // 3 gaps (before, between, after), each = 200/3 ≈ 66.67
    // child1 at 66.67, child2 at 66.67 + 50 + 66.67 = 183.33
    REQUIRE(child1->x() == Catch::Approx(66.67).margin(0.1));
    REQUIRE(child2->x() == Catch::Approx(183.33).margin(0.1));
}

TEST_CASE("Group flex layout - stretch alignment", "[layout][flexbox]") {
    auto group = flex::Group::create();
    group->set_layout(flex::LayoutMode::Flex);
    group->set_flex_direction(flex::FlexDirection::Row);
    group->set_align_items(flex::AlignItems::Stretch);
    group->set_clip_size(300, 100);
    group->set_clip(true);

    auto child1 = flex::Group::create();
    child1->set_layout_size(50, 0);  // Height auto
    auto child2 = flex::Group::create();
    child2->set_layout_size(50, 0);

    group->add_child(child1);
    group->add_child(child2);

    group->perform_layout();

    // Children should be stretched to container height
    REQUIRE(child1->layout_height() == Catch::Approx(100).margin(0.01));
    REQUIRE(child2->layout_height() == Catch::Approx(100).margin(0.01));
}

TEST_CASE("Group flex layout - property access", "[layout][flexbox]") {
    auto group = flex::Group::create();

    // Test layout property via string
    flex::Value val;

    group->set_property("layout", flex::Value::from_string("flex"));
    REQUIRE(group->layout() == flex::LayoutMode::Flex);

    group->set_property("flexDirection", flex::Value::from_string("column"));
    REQUIRE(group->flex_direction() == flex::FlexDirection::Column);

    group->set_property("justifyContent", flex::Value::from_string("center"));
    REQUIRE(group->justify_content() == flex::JustifyContent::Center);

    group->set_property("alignItems", flex::Value::from_string("stretch"));
    REQUIRE(group->align_items() == flex::AlignItems::Stretch);

    group->set_property("gap", flex::Value::from_float(15));
    REQUIRE(group->gap() == Catch::Approx(15).margin(0.01));

    group->set_property("padding", flex::Value::from_float(10));
    REQUIRE(group->padding_top() == Catch::Approx(10).margin(0.01));
    REQUIRE(group->padding_right() == Catch::Approx(10).margin(0.01));
    REQUIRE(group->padding_bottom() == Catch::Approx(10).margin(0.01));
    REQUIRE(group->padding_left() == Catch::Approx(10).margin(0.01));
}

TEST_CASE("Node flex properties - property access", "[layout][flexbox]") {
    auto node = flex::Group::create();

    node->set_property("layoutWidth", flex::Value::from_float(100));
    REQUIRE(node->layout_width() == Catch::Approx(100).margin(0.01));

    node->set_property("layoutHeight", flex::Value::from_float(50));
    REQUIRE(node->layout_height() == Catch::Approx(50).margin(0.01));

    node->set_property("flexGrow", flex::Value::from_float(2));
    REQUIRE(node->flex_grow() == Catch::Approx(2).margin(0.01));

    node->set_property("flexShrink", flex::Value::from_float(0.5f));
    REQUIRE(node->flex_shrink() == Catch::Approx(0.5f).margin(0.01));

    node->set_property("flexBasis", flex::Value::from_float(80));
    REQUIRE(node->flex_basis() == Catch::Approx(80).margin(0.01));

    node->set_property("alignSelf", flex::Value::from_string("center"));
    REQUIRE(node->align_self() == flex::AlignSelf::Center);
}

// ============================================================================
// Shadow and Blur Effects Tests
// ============================================================================

TEST_CASE("Shadow type creation and defaults", "[effects][shadow]") {
    flex::Shadow shadow;

    REQUIRE(shadow.offset_x == 0);
    REQUIRE(shadow.offset_y == 0);
    REQUIRE(shadow.blur == 0);
    REQUIRE(shadow.spread == 0);
    REQUIRE(shadow.inset == false);
    REQUIRE(shadow.is_none());
}

TEST_CASE("Shadow drop factory method", "[effects][shadow]") {
    flex::Color color{0.0f, 0.0f, 0.0f, 0.5f};
    auto shadow = flex::Shadow::drop(4, 4, 8, color);

    REQUIRE(shadow.offset_x == Catch::Approx(4).margin(0.01));
    REQUIRE(shadow.offset_y == Catch::Approx(4).margin(0.01));
    REQUIRE(shadow.blur == Catch::Approx(8).margin(0.01));
    REQUIRE(shadow.inset == false);
    REQUIRE_FALSE(shadow.is_none());
}

TEST_CASE("Shadow inner factory method", "[effects][shadow]") {
    flex::Color color{0.0f, 0.0f, 0.0f, 0.3f};
    auto shadow = flex::Shadow::inner(2, 2, 4, color);

    REQUIRE(shadow.offset_x == Catch::Approx(2).margin(0.01));
    REQUIRE(shadow.offset_y == Catch::Approx(2).margin(0.01));
    REQUIRE(shadow.blur == Catch::Approx(4).margin(0.01));
    REQUIRE(shadow.inset == true);
}

TEST_CASE("BlurFilter type creation", "[effects][blur]") {
    flex::BlurFilter blur;
    REQUIRE(blur.radius == 0);
    REQUIRE(blur.is_none());

    flex::BlurFilter blur2(5.0f);
    REQUIRE(blur2.radius == Catch::Approx(5).margin(0.01));
    REQUIRE_FALSE(blur2.is_none());
}

TEST_CASE("Node shadow properties", "[effects][shadow]") {
    auto node = flex::Group::create();

    // Default has no shadow
    REQUIRE_FALSE(node->has_shadow());
    REQUIRE(node->shadow().is_none());

    // Set shadow via struct
    flex::Shadow shadow(3, 4, 10, flex::Color{0, 0, 0, 0.5f});
    node->set_shadow(shadow);

    REQUIRE(node->has_shadow());
    REQUIRE(node->shadow().offset_x == Catch::Approx(3).margin(0.01));
    REQUIRE(node->shadow().offset_y == Catch::Approx(4).margin(0.01));
    REQUIRE(node->shadow().blur == Catch::Approx(10).margin(0.01));

    // Set shadow via convenience method
    node->set_shadow(5, 5, 15, flex::Color{0.2f, 0.2f, 0.2f, 0.6f});
    REQUIRE(node->shadow().offset_x == Catch::Approx(5).margin(0.01));
    REQUIRE(node->shadow().blur == Catch::Approx(15).margin(0.01));
}

TEST_CASE("Node blur properties", "[effects][blur]") {
    auto node = flex::Group::create();

    // Default has no blur
    REQUIRE_FALSE(node->has_blur());
    REQUIRE(node->blur().is_none());

    // Set blur via struct
    flex::BlurFilter blur(8.0f);
    node->set_blur(blur);

    REQUIRE(node->has_blur());
    REQUIRE(node->blur().radius == Catch::Approx(8).margin(0.01));

    // Set blur via convenience method
    node->set_blur(12.0f);
    REQUIRE(node->blur().radius == Catch::Approx(12).margin(0.01));
}

TEST_CASE("Node shadow property access via string", "[effects][shadow]") {
    auto node = flex::Group::create();

    node->set_property("shadowOffsetX", flex::Value::from_float(5));
    node->set_property("shadowOffsetY", flex::Value::from_float(7));
    node->set_property("shadowBlur", flex::Value::from_float(10));
    node->set_property("shadowSpread", flex::Value::from_float(2));

    REQUIRE(node->shadow().offset_x == Catch::Approx(5).margin(0.01));
    REQUIRE(node->shadow().offset_y == Catch::Approx(7).margin(0.01));
    REQUIRE(node->shadow().blur == Catch::Approx(10).margin(0.01));
    REQUIRE(node->shadow().spread == Catch::Approx(2).margin(0.01));

    // Get property
    flex::Value val;
    REQUIRE(node->get_property("shadowOffsetX", &val));
    REQUIRE(val.as_float() == Catch::Approx(5).margin(0.01));

    REQUIRE(node->get_property("shadowBlur", &val));
    REQUIRE(val.as_float() == Catch::Approx(10).margin(0.01));
}

TEST_CASE("Node blur property access via string", "[effects][blur]") {
    auto node = flex::Group::create();

    node->set_property("blur", flex::Value::from_float(6));
    REQUIRE(node->blur().radius == Catch::Approx(6).margin(0.01));

    node->set_property("blurRadius", flex::Value::from_float(9));
    REQUIRE(node->blur().radius == Catch::Approx(9).margin(0.01));

    // Get property
    flex::Value val;
    REQUIRE(node->get_property("blur", &val));
    REQUIRE(val.as_float() == Catch::Approx(9).margin(0.01));
}

TEST_CASE("Shadow inset property", "[effects][shadow]") {
    auto node = flex::Group::create();

    REQUIRE(node->shadow().inset == false);

    node->set_property("shadowInset", flex::Value::from_float(1));
    REQUIRE(node->shadow().inset == true);

    node->set_property("shadowInset", flex::Value::from_float(0));
    REQUIRE(node->shadow().inset == false);

    flex::Value val;
    node->set_property("shadowInset", flex::Value::from_float(1));
    REQUIRE(node->get_property("shadowInset", &val));
    REQUIRE(val.as_float() == Catch::Approx(1).margin(0.01));
}

// ============================================================================
// Keyboard Events Tests
// ============================================================================

TEST_CASE("KeyCode enum values", "[events][keyboard]") {
    // Letters should be ASCII values
    REQUIRE(static_cast<int>(flex::KeyCode::A) == 'A');
    REQUIRE(static_cast<int>(flex::KeyCode::Z) == 'Z');

    // Numbers should be ASCII values
    REQUIRE(static_cast<int>(flex::KeyCode::Num0) == '0');
    REQUIRE(static_cast<int>(flex::KeyCode::Num9) == '9');

    // Space should be ASCII
    REQUIRE(static_cast<int>(flex::KeyCode::Space) == 32);

    // Function keys should be > 255
    REQUIRE(static_cast<int>(flex::KeyCode::F1) >= 256);
}

TEST_CASE("KeyModifiers defaults and checks", "[events][keyboard]") {
    flex::KeyModifiers mods;

    REQUIRE(mods.shift == false);
    REQUIRE(mods.ctrl == false);
    REQUIRE(mods.alt == false);
    REQUIRE(mods.super == false);
    REQUIRE(mods.none());
    REQUIRE_FALSE(mods.any());

    mods.ctrl = true;
    REQUIRE_FALSE(mods.none());
    REQUIRE(mods.any());
}

TEST_CASE("KeyEvent creation and properties", "[events][keyboard]") {
    flex::KeyEvent event;

    event.type = flex::KeyEventType::Down;
    event.key = flex::KeyCode::A;
    event.modifiers.ctrl = true;

    REQUIRE(event.is_down());
    REQUIRE_FALSE(event.is_up());
    REQUIRE(event.key == flex::KeyCode::A);
    REQUIRE(event.modifiers.ctrl);

    // Test convenience methods
    REQUIRE(event.is_ctrl_key(flex::KeyCode::A));
    REQUIRE_FALSE(event.is_ctrl_key(flex::KeyCode::B));
    REQUIRE_FALSE(event.is_shift_key(flex::KeyCode::A));
}

TEST_CASE("KeyEvent propagation control", "[events][keyboard]") {
    flex::KeyEvent event;

    REQUIRE_FALSE(event.propagation_stopped());

    event.stop_propagation();
    REQUIRE(event.propagation_stopped());
}

TEST_CASE("Node keyboard event handlers", "[events][keyboard]") {
    auto node = flex::Group::create();
    bool key_down_fired = false;
    bool key_up_fired = false;
    flex::KeyCode received_key = flex::KeyCode::Unknown;

    node->on_key_down([&](flex::KeyEvent& e) {
        key_down_fired = true;
        received_key = e.key;
    });

    node->on_key_up([&](flex::KeyEvent& e) {
        key_up_fired = true;
    });

    flex::KeyEvent down_event;
    down_event.type = flex::KeyEventType::Down;
    down_event.key = flex::KeyCode::Enter;

    node->fire_key_down(down_event);
    REQUIRE(key_down_fired);
    REQUIRE(received_key == flex::KeyCode::Enter);
    REQUIRE_FALSE(key_up_fired);

    flex::KeyEvent up_event;
    up_event.type = flex::KeyEventType::Up;
    node->fire_key_up(up_event);
    REQUIRE(key_up_fired);
}

TEST_CASE("Node has_key_handlers check", "[events][keyboard]") {
    auto node = flex::Group::create();

    REQUIRE_FALSE(node->has_key_handlers());

    node->on_key_down([](flex::KeyEvent&) {});
    REQUIRE(node->has_key_handlers());
}

TEST_CASE("Node focus properties", "[events][keyboard]") {
    auto node = flex::Group::create();

    // Default not focusable
    REQUIRE_FALSE(node->focusable());
    REQUIRE_FALSE(node->focused());

    // Make focusable
    node->set_focusable(true);
    REQUIRE(node->focusable());

    // Focus state
    node->set_focused(true);
    REQUIRE(node->focused());

    node->set_focused(false);
    REQUIRE_FALSE(node->focused());
}

TEST_CASE("Node focus callback", "[events][keyboard]") {
    auto node = flex::Group::create();
    bool focus_callback_fired = false;
    bool last_focus_state = false;

    node->on_focus([&](bool gained) {
        focus_callback_fired = true;
        last_focus_state = gained;
    });

    node->set_focused(true);
    REQUIRE(focus_callback_fired);
    REQUIRE(last_focus_state == true);

    focus_callback_fired = false;
    node->set_focused(false);
    REQUIRE(focus_callback_fired);
    REQUIRE(last_focus_state == false);

    // Setting same value shouldn't trigger callback
    focus_callback_fired = false;
    node->set_focused(false);
    REQUIRE_FALSE(focus_callback_fired);
}

TEST_CASE("KeyEvent shortcut helpers", "[events][keyboard]") {
    flex::KeyEvent event;
    event.key = flex::KeyCode::S;

    // Plain key
    REQUIRE_FALSE(event.is_ctrl_key(flex::KeyCode::S));

    // Ctrl+S
    event.modifiers.ctrl = true;
    REQUIRE(event.is_ctrl_key(flex::KeyCode::S));
    REQUIRE_FALSE(event.is_shift_key(flex::KeyCode::S));

    // Ctrl+Shift+S (not a simple ctrl key)
    event.modifiers.shift = true;
    REQUIRE_FALSE(event.is_ctrl_key(flex::KeyCode::S));

    // Shift+S
    event.modifiers.ctrl = false;
    REQUIRE(event.is_shift_key(flex::KeyCode::S));

    // Alt+S
    event.modifiers.shift = false;
    event.modifiers.alt = true;
    REQUIRE(event.is_alt_key(flex::KeyCode::S));
}

// ============================================================================
// Text Enhancement Tests
// ============================================================================

TEST_CASE("Text decoration enum", "[text]") {
    REQUIRE(flex::TextDecoration::None != flex::TextDecoration::Underline);
    REQUIRE(flex::TextDecoration::Underline != flex::TextDecoration::Strikethrough);
    REQUIRE(flex::TextDecoration::Strikethrough != flex::TextDecoration::Overline);
}

TEST_CASE("Text font style enum", "[text]") {
    REQUIRE(flex::FontStyle::Normal != flex::FontStyle::Italic);
}

TEST_CASE("Text overflow enum", "[text]") {
    REQUIRE(flex::TextOverflow::Visible != flex::TextOverflow::Clip);
    REQUIRE(flex::TextOverflow::Clip != flex::TextOverflow::Ellipsis);
}

TEST_CASE("Text decoration properties", "[text]") {
    auto text = flex::Text::create();

    // Default is no decoration
    REQUIRE(text->text_decoration() == flex::TextDecoration::None);

    text->set_text_decoration(flex::TextDecoration::Underline);
    REQUIRE(text->text_decoration() == flex::TextDecoration::Underline);

    text->set_text_decoration(flex::TextDecoration::Strikethrough);
    REQUIRE(text->text_decoration() == flex::TextDecoration::Strikethrough);
}

TEST_CASE("Text font style", "[text]") {
    auto text = flex::Text::create();

    REQUIRE(text->font_style() == flex::FontStyle::Normal);

    text->set_font_style(flex::FontStyle::Italic);
    REQUIRE(text->font_style() == flex::FontStyle::Italic);
}

TEST_CASE("Text letter spacing", "[text]") {
    auto text = flex::Text::create();

    REQUIRE(text->letter_spacing() == Catch::Approx(0).margin(0.01));

    text->set_letter_spacing(2.5f);
    REQUIRE(text->letter_spacing() == Catch::Approx(2.5f).margin(0.01));
}

TEST_CASE("Text overflow", "[text]") {
    auto text = flex::Text::create();

    REQUIRE(text->text_overflow() == flex::TextOverflow::Visible);

    text->set_text_overflow(flex::TextOverflow::Ellipsis);
    REQUIRE(text->text_overflow() == flex::TextOverflow::Ellipsis);
}

TEST_CASE("Text measurement - basic", "[text][measurement]") {
    auto text = flex::Text::create();
    text->set_content("Hello");
    text->set_font_size(16);

    // Should have positive measurements
    REQUIRE(text->measured_width() > 0);
    REQUIRE(text->measured_height() > 0);

    // Height should be based on font size and line height
    REQUIRE(text->measured_height() == Catch::Approx(16 * 1.2f).margin(0.1));
}

TEST_CASE("Text measurement - empty content", "[text][measurement]") {
    auto text = flex::Text::create();
    text->set_content("");
    text->set_font_size(16);

    REQUIRE(text->measured_width() == Catch::Approx(0).margin(0.01));
}

TEST_CASE("Text measurement - font size affects width", "[text][measurement]") {
    auto text = flex::Text::create();
    text->set_content("Test");

    text->set_font_size(16);
    float width16 = text->measured_width();

    text->set_font_size(32);
    float width32 = text->measured_width();

    // Larger font should produce wider text
    REQUIRE(width32 > width16);
}

TEST_CASE("Text measurement - letter spacing affects width", "[text][measurement]") {
    auto text = flex::Text::create();
    text->set_content("Test");
    text->set_font_size(16);

    text->set_letter_spacing(0);
    float width_no_spacing = text->measured_width();

    text->set_letter_spacing(5);
    float width_with_spacing = text->measured_width();

    // Letter spacing should increase width
    REQUIRE(width_with_spacing > width_no_spacing);
}

TEST_CASE("Text bounds includes position", "[text][measurement]") {
    auto text = flex::Text::create();
    text->set_content("Hello");
    text->set_font_size(16);
    text->set_position(100, 50);

    auto bounds = text->bounds();
    REQUIRE(bounds.x == Catch::Approx(100).margin(0.01));
    REQUIRE(bounds.y == Catch::Approx(50).margin(0.01));
    REQUIRE(bounds.width > 0);
    REQUIRE(bounds.height > 0);
}

TEST_CASE("Text measurement caching", "[text][measurement]") {
    auto text = flex::Text::create();
    text->set_content("Test");
    text->set_font_size(16);

    // First call calculates
    float width1 = text->measured_width();

    // Second call should return cached value
    float width2 = text->measured_width();
    REQUIRE(width1 == width2);

    // Changing content invalidates cache
    text->set_content("Longer text here");
    float width3 = text->measured_width();
    REQUIRE(width3 > width1);
}

TEST_CASE("Text property access - decoration", "[text]") {
    auto text = flex::Text::create();

    text->set_property("textDecoration", flex::Value::from_string("underline"));
    REQUIRE(text->text_decoration() == flex::TextDecoration::Underline);

    text->set_property("textDecoration", flex::Value::from_string("strikethrough"));
    REQUIRE(text->text_decoration() == flex::TextDecoration::Strikethrough);

    text->set_property("textDecoration", flex::Value::from_string("line-through"));
    REQUIRE(text->text_decoration() == flex::TextDecoration::Strikethrough);

    text->set_property("textDecoration", flex::Value::from_string("none"));
    REQUIRE(text->text_decoration() == flex::TextDecoration::None);
}

TEST_CASE("Text property access - font style", "[text]") {
    auto text = flex::Text::create();

    text->set_property("fontStyle", flex::Value::from_string("italic"));
    REQUIRE(text->font_style() == flex::FontStyle::Italic);

    text->set_property("fontStyle", flex::Value::from_string("normal"));
    REQUIRE(text->font_style() == flex::FontStyle::Normal);
}

TEST_CASE("Text property access - letter spacing", "[text]") {
    auto text = flex::Text::create();

    text->set_property("letterSpacing", flex::Value::from_float(3.5f));
    REQUIRE(text->letter_spacing() == Catch::Approx(3.5f).margin(0.01));

    flex::Value val;
    REQUIRE(text->get_property("letterSpacing", &val));
    REQUIRE(val.as_float() == Catch::Approx(3.5f).margin(0.01));
}

TEST_CASE("Text property access - measured dimensions", "[text]") {
    auto text = flex::Text::create();
    text->set_content("Hello World");
    text->set_font_size(20);

    flex::Value width_val, height_val;
    REQUIRE(text->get_property("measuredWidth", &width_val));
    REQUIRE(text->get_property("measuredHeight", &height_val));

    REQUIRE(width_val.as_float() > 0);
    REQUIRE(height_val.as_float() > 0);
}

// ============================================================================
// Animation Blending Tests
// ============================================================================

TEST_CASE("BlendMode enum values", "[animation][blending]") {
    REQUIRE(flex::BlendMode::Override != flex::BlendMode::Additive);
    REQUIRE(flex::BlendMode::Additive != flex::BlendMode::Multiply);
    REQUIRE(flex::BlendMode::Multiply != flex::BlendMode::Override);
}

TEST_CASE("TimelinePlayer blend properties", "[animation][blending]") {
    auto timeline = flex::Timeline::create("test");
    auto track = timeline->add_track("x");
    track->add_keyframe(0.0f, flex::Value::from_float(0.0f));
    track->add_keyframe(1.0f, flex::Value::from_float(100.0f));

    auto node = flex::Shape::create();
    flex::TimelinePlayer player(timeline, node.get());

    SECTION("Default blend mode is Override") {
        REQUIRE(player.blend_mode() == flex::BlendMode::Override);
    }

    SECTION("Default blend weight is 1.0") {
        REQUIRE_THAT(player.blend_weight(), WithinAbs(1.0f, 0.01f));
    }

    SECTION("Default layer is 0") {
        REQUIRE(player.layer() == 0);
    }

    SECTION("Set blend mode") {
        player.set_blend_mode(flex::BlendMode::Additive);
        REQUIRE(player.blend_mode() == flex::BlendMode::Additive);
    }

    SECTION("Set blend weight clamps to 0-1") {
        player.set_blend_weight(0.5f);
        REQUIRE_THAT(player.blend_weight(), WithinAbs(0.5f, 0.01f));

        player.set_blend_weight(-0.5f);
        REQUIRE_THAT(player.blend_weight(), WithinAbs(0.0f, 0.01f));

        player.set_blend_weight(1.5f);
        REQUIRE_THAT(player.blend_weight(), WithinAbs(1.0f, 0.01f));
    }

    SECTION("Set layer") {
        player.set_layer(5);
        REQUIRE(player.layer() == 5);
    }
}

TEST_CASE("TimelinePlayer sample returns track value", "[animation][blending]") {
    auto timeline = flex::Timeline::create("test");
    auto track = timeline->add_track("opacity");
    track->add_keyframe(0.0f, flex::Value::from_float(0.0f));
    track->add_keyframe(1.0f, flex::Value::from_float(1.0f));

    auto node = flex::Shape::create();
    flex::TimelinePlayer player(timeline, node.get());
    player.play();
    player.seek(0.5f);

    auto val = player.sample("opacity");
    REQUIRE_THAT(val.as_float(), WithinAbs(0.5f, 0.01f));
}

TEST_CASE("blend_values interpolates correctly", "[animation][blending]") {
    auto a = flex::Value::from_float(0.0f);
    auto b = flex::Value::from_float(100.0f);

    SECTION("Weight 0 returns a") {
        auto result = flex::blend_values(a, b, 0.0f);
        REQUIRE_THAT(result.as_float(), WithinAbs(0.0f, 0.01f));
    }

    SECTION("Weight 1 returns b") {
        auto result = flex::blend_values(a, b, 1.0f);
        REQUIRE_THAT(result.as_float(), WithinAbs(100.0f, 0.01f));
    }

    SECTION("Weight 0.5 interpolates") {
        auto result = flex::blend_values(a, b, 0.5f);
        REQUIRE_THAT(result.as_float(), WithinAbs(50.0f, 0.01f));
    }

    SECTION("Weight clamps below 0") {
        auto result = flex::blend_values(a, b, -0.5f);
        REQUIRE_THAT(result.as_float(), WithinAbs(0.0f, 0.01f));
    }

    SECTION("Weight clamps above 1") {
        auto result = flex::blend_values(a, b, 1.5f);
        REQUIRE_THAT(result.as_float(), WithinAbs(100.0f, 0.01f));
    }
}

TEST_CASE("apply_blended_value Override mode", "[animation][blending]") {
    auto node = flex::Shape::create();
    node->set_x(50.0f);

    SECTION("Full weight replaces value") {
        flex::apply_blended_value(node.get(), "x",
            flex::Value::from_float(100.0f), flex::BlendMode::Override, 1.0f);
        REQUIRE_THAT(node->x(), WithinAbs(100.0f, 0.01f));
    }

    SECTION("Partial weight blends with current") {
        flex::apply_blended_value(node.get(), "x",
            flex::Value::from_float(100.0f), flex::BlendMode::Override, 0.5f);
        // Blend: current(50) + (target(100) - current(50)) * 0.5 = 75
        REQUIRE_THAT(node->x(), WithinAbs(75.0f, 0.01f));
    }

    SECTION("Zero weight has no effect") {
        flex::apply_blended_value(node.get(), "x",
            flex::Value::from_float(100.0f), flex::BlendMode::Override, 0.0f);
        REQUIRE_THAT(node->x(), WithinAbs(50.0f, 0.01f));
    }
}

TEST_CASE("apply_blended_value Additive mode", "[animation][blending]") {
    auto node = flex::Shape::create();
    node->set_x(50.0f);

    SECTION("Full weight adds value") {
        flex::apply_blended_value(node.get(), "x",
            flex::Value::from_float(30.0f), flex::BlendMode::Additive, 1.0f);
        // Additive: current(50) + value(30) * weight(1) = 80
        REQUIRE_THAT(node->x(), WithinAbs(80.0f, 0.01f));
    }

    SECTION("Partial weight adds scaled value") {
        flex::apply_blended_value(node.get(), "x",
            flex::Value::from_float(40.0f), flex::BlendMode::Additive, 0.5f);
        // Additive: current(50) + value(40) * weight(0.5) = 70
        REQUIRE_THAT(node->x(), WithinAbs(70.0f, 0.01f));
    }
}

TEST_CASE("apply_blended_value Multiply mode", "[animation][blending]") {
    auto node = flex::Shape::create();
    node->set_opacity(0.8f);

    SECTION("Full weight multiplies by value") {
        flex::apply_blended_value(node.get(), "opacity",
            flex::Value::from_float(0.5f), flex::BlendMode::Multiply, 1.0f);
        // Multiply: current * (1 + (value - 1) * weight) = 0.8 * 0.5 = 0.4
        REQUIRE_THAT(node->opacity(), WithinAbs(0.4f, 0.01f));
    }

    SECTION("Partial weight interpolates multiplication") {
        flex::apply_blended_value(node.get(), "opacity",
            flex::Value::from_float(0.5f), flex::BlendMode::Multiply, 0.5f);
        // Multiply: current * (1 + (value - 1) * weight) = 0.8 * (1 + (-0.5) * 0.5) = 0.8 * 0.75 = 0.6
        REQUIRE_THAT(node->opacity(), WithinAbs(0.6f, 0.01f));
    }
}

TEST_CASE("AnimationController play with blending options", "[animation][blending]") {
    flex::AnimationController controller;

    auto timeline = flex::Timeline::create("Anim");
    auto track = timeline->add_track("x");
    track->add_keyframe(0.0f, flex::Value::from_float(0.0f));
    track->add_keyframe(1.0f, flex::Value::from_float(100.0f));
    controller.add_timeline(timeline);

    auto node = flex::Shape::create();

    SECTION("Play with blend mode") {
        auto* player = controller.play("Anim", node.get(), flex::BlendMode::Additive, 0.5f, 2);
        REQUIRE(player != nullptr);
        REQUIRE(player->blend_mode() == flex::BlendMode::Additive);
        REQUIRE_THAT(player->blend_weight(), WithinAbs(0.5f, 0.01f));
        REQUIRE(player->layer() == 2);
    }
}

TEST_CASE("AnimationController stop_on_target", "[animation][blending]") {
    flex::AnimationController controller;

    auto timeline = flex::Timeline::create("Anim");
    timeline->set_duration(2.0f);
    controller.add_timeline(timeline);

    auto node1 = flex::Shape::create();
    auto node2 = flex::Shape::create();

    controller.play("Anim", node1.get());
    controller.play("Anim", node2.get());

    REQUIRE(controller.is_playing("Anim"));

    controller.stop_on_target(node1.get());

    // node2 should still be playing
    auto players = controller.get_players_for_target(node2.get());
    REQUIRE(players.size() == 1);
}

TEST_CASE("AnimationController get_players_for_target", "[animation][blending]") {
    flex::AnimationController controller;

    auto timeline1 = flex::Timeline::create("Anim1");
    timeline1->set_duration(2.0f);
    auto timeline2 = flex::Timeline::create("Anim2");
    timeline2->set_duration(2.0f);

    controller.add_timeline(timeline1);
    controller.add_timeline(timeline2);

    auto node = flex::Shape::create();

    controller.play("Anim1", node.get());
    controller.play("Anim2", node.get());

    auto players = controller.get_players_for_target(node.get());
    REQUIRE(players.size() == 2);
}

TEST_CASE("AnimationController layer sorting", "[animation][blending]") {
    flex::AnimationController controller;

    auto timeline = flex::Timeline::create("Anim");
    auto track = timeline->add_track("x");
    track->add_keyframe(0.0f, flex::Value::from_float(0.0f));
    track->add_keyframe(1.0f, flex::Value::from_float(100.0f));
    controller.add_timeline(timeline);

    auto node = flex::Shape::create();
    node->set_x(0.0f);

    // Play on layer 1 (applied first)
    auto* player1 = controller.play("Anim", node.get(), flex::BlendMode::Override, 1.0f, 1);
    player1->seek(1.0f);  // x -> 100

    // Play on layer 2 (applied second)
    auto* player2 = controller.play("Anim", node.get(), flex::BlendMode::Additive, 0.5f, 2);
    player2->seek(0.5f);  // adds 50 * 0.5 = 25

    controller.advance(0.0f);  // Apply all players

    // Layer 1 sets x=100, layer 2 adds 25 -> x=125
    REQUIRE_THAT(node->x(), WithinAbs(125.0f, 1.0f));
}

// ============================================================================
// Scene Optimization Tests - Dirty Flags
// ============================================================================

TEST_CASE("DirtyFlags enum values", "[scene][dirty]") {
    REQUIRE(static_cast<uint32_t>(flex::DirtyFlags::None) == 0);
    REQUIRE(static_cast<uint32_t>(flex::DirtyFlags::Transform) == 1);
    REQUIRE(static_cast<uint32_t>(flex::DirtyFlags::Visual) == 2);
    REQUIRE(static_cast<uint32_t>(flex::DirtyFlags::Content) == 4);
    REQUIRE(static_cast<uint32_t>(flex::DirtyFlags::Bounds) == 8);
    REQUIRE(static_cast<uint32_t>(flex::DirtyFlags::Layout) == 16);
    REQUIRE(static_cast<uint32_t>(flex::DirtyFlags::Children) == 32);
}

TEST_CASE("DirtyFlags bitwise operations", "[scene][dirty]") {
    auto flags = flex::DirtyFlags::Transform | flex::DirtyFlags::Visual;
    REQUIRE(flex::has_flag(flags, flex::DirtyFlags::Transform));
    REQUIRE(flex::has_flag(flags, flex::DirtyFlags::Visual));
    REQUIRE_FALSE(flex::has_flag(flags, flex::DirtyFlags::Content));

    flags |= flex::DirtyFlags::Content;
    REQUIRE(flex::has_flag(flags, flex::DirtyFlags::Content));

    flags &= ~flex::DirtyFlags::Transform;
    REQUIRE_FALSE(flex::has_flag(flags, flex::DirtyFlags::Transform));
}

TEST_CASE("Node starts dirty", "[scene][dirty]") {
    auto shape = flex::Shape::create();
    REQUIRE(shape->is_dirty());
    REQUIRE(shape->is_dirty(flex::DirtyFlags::All));
}

TEST_CASE("Node clear_dirty", "[scene][dirty]") {
    auto shape = flex::Shape::create();
    REQUIRE(shape->is_dirty());

    shape->clear_dirty();
    REQUIRE_FALSE(shape->is_dirty());
    REQUIRE(shape->dirty_flags() == flex::DirtyFlags::None);
}

TEST_CASE("Node clear_dirty specific flag", "[scene][dirty]") {
    auto shape = flex::Shape::create();
    shape->clear_dirty();

    shape->mark_dirty(flex::DirtyFlags::Transform | flex::DirtyFlags::Visual);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Transform));
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Visual));

    shape->clear_dirty(flex::DirtyFlags::Transform);
    REQUIRE_FALSE(shape->is_dirty(flex::DirtyFlags::Transform));
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Visual));
}

TEST_CASE("Node transform setters mark dirty", "[scene][dirty]") {
    auto shape = flex::Shape::create();
    shape->clear_dirty();

    shape->set_x(10.0f);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Transform));
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Bounds));

    shape->clear_dirty();
    shape->set_y(20.0f);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Transform));

    shape->clear_dirty();
    shape->set_position(30.0f, 40.0f);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Transform));

    shape->clear_dirty();
    shape->set_scale(2.0f);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Transform));

    shape->clear_dirty();
    shape->set_rotation(45.0f);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Transform));
}

TEST_CASE("Node visual setters mark dirty", "[scene][dirty]") {
    auto shape = flex::Shape::create();
    shape->clear_dirty();

    shape->set_opacity(0.5f);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Visual));

    shape->clear_dirty();
    shape->set_visible(false);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Visual));
}

TEST_CASE("Node layout setters mark dirty", "[scene][dirty]") {
    auto shape = flex::Shape::create();
    shape->clear_dirty();

    shape->set_layout_width(100.0f);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Layout));
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Bounds));

    shape->clear_dirty();
    shape->set_flex_grow(1.0f);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Layout));
}

TEST_CASE("Shape geometry setters mark dirty", "[scene][dirty]") {
    auto shape = flex::Shape::create();
    shape->clear_dirty();

    shape->set_rect(100.0f, 50.0f);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Content));
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Bounds));

    shape->clear_dirty();
    shape->set_circle(25.0f);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Content));
}

TEST_CASE("Shape paint setters mark dirty", "[scene][dirty]") {
    auto shape = flex::Shape::create();
    shape->clear_dirty();

    shape->set_fill(flex::Color(1, 0, 0, 1));
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Content));
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Visual));

    shape->clear_dirty();
    shape->set_stroke(flex::Color(0, 0, 1, 1), 2.0f);
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Content));

    shape->clear_dirty();
    shape->clear_fill();
    REQUIRE(shape->is_dirty(flex::DirtyFlags::Content));
}

TEST_CASE("Text setters mark dirty", "[scene][dirty]") {
    auto text = flex::Text::create();
    text->clear_dirty();

    text->set_content("Hello");
    REQUIRE(text->is_dirty(flex::DirtyFlags::Content));
    REQUIRE(text->is_dirty(flex::DirtyFlags::Bounds));

    text->clear_dirty();
    text->set_font_size(24.0f);
    REQUIRE(text->is_dirty(flex::DirtyFlags::Content));

    text->clear_dirty();
    text->set_color(flex::Color(1, 0, 0, 1));
    REQUIRE(text->is_dirty(flex::DirtyFlags::Visual));
}

TEST_CASE("Image setters mark dirty", "[scene][dirty]") {
    auto image = flex::Image::create();
    image->clear_dirty();

    image->set_src("test.png");
    REQUIRE(image->is_dirty(flex::DirtyFlags::Content));
    REQUIRE(image->is_dirty(flex::DirtyFlags::Visual));

    image->clear_dirty();
    image->set_width(200.0f);
    REQUIRE(image->is_dirty(flex::DirtyFlags::Content));
    REQUIRE(image->is_dirty(flex::DirtyFlags::Bounds));
}

TEST_CASE("Group children management marks dirty", "[scene][dirty]") {
    auto group = flex::Group::create();
    auto child = flex::Shape::create();
    group->clear_dirty();

    group->add_child(child);
    REQUIRE(group->is_dirty(flex::DirtyFlags::Children));
    REQUIRE(group->is_dirty(flex::DirtyFlags::Layout));
    REQUIRE(group->is_dirty(flex::DirtyFlags::Bounds));

    group->clear_dirty();
    group->remove_child(child.get());
    REQUIRE(group->is_dirty(flex::DirtyFlags::Children));

    group->add_child(child);
    group->clear_dirty();
    group->clear_children();
    REQUIRE(group->is_dirty(flex::DirtyFlags::Children));
}

TEST_CASE("Group layout setters mark dirty", "[scene][dirty]") {
    auto group = flex::Group::create();
    group->clear_dirty();

    group->set_layout(flex::LayoutMode::Flex);
    REQUIRE(group->is_dirty(flex::DirtyFlags::Layout));

    group->clear_dirty();
    group->set_flex_direction(flex::FlexDirection::Column);
    REQUIRE(group->is_dirty(flex::DirtyFlags::Layout));

    group->clear_dirty();
    group->set_gap(10.0f);
    REQUIRE(group->is_dirty(flex::DirtyFlags::Layout));

    group->clear_dirty();
    group->set_padding(20.0f);
    REQUIRE(group->is_dirty(flex::DirtyFlags::Layout));
}

TEST_CASE("Dirty propagates to parent", "[scene][dirty]") {
    auto parent = flex::Group::create();
    auto child = flex::Shape::create();
    parent->add_child(child);

    parent->clear_dirty();
    child->clear_dirty();

    // Layout change on child should propagate to parent
    child->set_layout_width(100.0f);
    REQUIRE(child->is_dirty(flex::DirtyFlags::Layout));
    REQUIRE(parent->is_dirty(flex::DirtyFlags::Layout));
    REQUIRE(parent->is_dirty(flex::DirtyFlags::Bounds));
}

// ============================================================================
// Scene Optimization Tests - Culling
// ============================================================================

TEST_CASE("CullResult enum values", "[scene][culling]") {
    REQUIRE(static_cast<uint8_t>(flex::CullResult::Visible) == 0);
    REQUIRE(static_cast<uint8_t>(flex::CullResult::Hidden) == 1);
    REQUIRE(static_cast<uint8_t>(flex::CullResult::Transparent) == 2);
    REQUIRE(static_cast<uint8_t>(flex::CullResult::OutOfView) == 3);
}

TEST_CASE("Node should_render", "[scene][culling]") {
    auto shape = flex::Shape::create();

    REQUIRE(shape->should_render());

    shape->set_visible(false);
    REQUIRE_FALSE(shape->should_render());

    shape->set_visible(true);
    shape->set_opacity(0.0f);
    REQUIRE_FALSE(shape->should_render());

    shape->set_opacity(0.5f);
    REQUIRE(shape->should_render());
}

TEST_CASE("Node cull - hidden", "[scene][culling]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100.0f, 100.0f);
    shape->set_visible(false);

    flex::Bounds viewport{0, 0, 800, 600};
    REQUIRE(shape->cull(viewport) == flex::CullResult::Hidden);
}

TEST_CASE("Node cull - transparent", "[scene][culling]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100.0f, 100.0f);
    shape->set_opacity(0.0f);

    flex::Bounds viewport{0, 0, 800, 600};
    REQUIRE(shape->cull(viewport) == flex::CullResult::Transparent);
}

TEST_CASE("Node cull - visible in viewport", "[scene][culling]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100.0f, 100.0f);
    shape->set_position(50.0f, 50.0f);

    flex::Bounds viewport{0, 0, 800, 600};
    REQUIRE(shape->cull(viewport) == flex::CullResult::Visible);
}

TEST_CASE("Node cull - out of view", "[scene][culling]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100.0f, 100.0f);
    shape->set_position(1000.0f, 1000.0f);  // Outside viewport

    flex::Bounds viewport{0, 0, 800, 600};
    REQUIRE(shape->cull(viewport) == flex::CullResult::OutOfView);
}

TEST_CASE("Node intersects_viewport - overlap", "[scene][culling]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100.0f, 100.0f);
    shape->set_position(750.0f, 550.0f);  // Partially overlapping

    flex::Bounds viewport{0, 0, 800, 600};
    REQUIRE(shape->intersects_viewport(viewport));
}

TEST_CASE("Node intersects_viewport - no overlap", "[scene][culling]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100.0f, 100.0f);
    shape->set_position(900.0f, 700.0f);  // Completely outside

    flex::Bounds viewport{0, 0, 800, 600};
    REQUIRE_FALSE(shape->intersects_viewport(viewport));
}

TEST_CASE("Node intersects_viewport - touching edge", "[scene][culling]") {
    auto shape = flex::Shape::create();
    shape->set_rect(100.0f, 100.0f);
    shape->set_position(800.0f, 0.0f);  // Just touching the right edge

    flex::Bounds viewport{0, 0, 800, 600};
    // When exactly touching, AABB test considers them intersecting (conservative for culling)
    REQUIRE(shape->intersects_viewport(viewport));

    // Fully outside - one pixel past the edge
    shape->set_position(801.0f, 0.0f);
    REQUIRE_FALSE(shape->intersects_viewport(viewport));
}

TEST_CASE("Group bounds for culling", "[scene][culling]") {
    auto group = flex::Group::create();
    auto child1 = flex::Shape::create();
    child1->set_rect(50.0f, 50.0f);
    child1->set_position(10.0f, 10.0f);

    auto child2 = flex::Shape::create();
    child2->set_rect(50.0f, 50.0f);
    child2->set_position(100.0f, 100.0f);

    group->add_child(child1);
    group->add_child(child2);

    flex::Bounds b = group->bounds();
    // Group bounds should encompass all children
    REQUIRE(b.width > 0);
    REQUIRE(b.height > 0);
}
