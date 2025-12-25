/*
 * Flex Runtime System Tests
 * Tests RuntimeStateMachine, Timeline, and their integration
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "flex.h"
#include "flex/dsl/asset.h"

using namespace flex;
using namespace flex::parser;
using Catch::Matchers::WithinAbs;

// ============================================================================
// RUNTIME STATE MACHINE TESTS
// ============================================================================

TEST_CASE("RuntimeStateMachine: Basic creation", "[runtime][machine]") {
    RuntimeStateMachine machine("testMachine");

    REQUIRE(machine.name() == "testMachine");
}

TEST_CASE("RuntimeStateMachine: Layer management", "[runtime][machine]") {
    RuntimeStateMachine machine("testMachine");

    machine.add_layer("status");
    machine.add_layer("movement");

    REQUIRE(machine.get_layer("status") != nullptr);
    REQUIRE(machine.get_layer("movement") != nullptr);
    REQUIRE(machine.get_layer("nonexistent") == nullptr);
}

TEST_CASE("RuntimeStateMachine: State management", "[runtime][machine]") {
    RuntimeStateMachine machine("testMachine");
    machine.add_layer("status");

    auto* layer = machine.get_layer("status");
    REQUIRE(layer != nullptr);

    layer->add_state("idle", true, "toIdle");
    layer->add_state("active", false, "toActive");

    REQUIRE(layer->current_state() == "idle");
}

TEST_CASE("RuntimeStateMachine: Transition on input change", "[runtime][machine]") {
    RuntimeStateMachine machine("statusTracker");
    machine.add_layer("status");

    auto* layer = machine.get_layer("status");

    // Add states
    layer->add_state("neutral", true, "toNeutral");
    layer->add_state("positive", false, "toPositive");
    layer->add_state("high", false, "toHigh");

    // Add transitions
    layer->add_transition("neutral", "positive", "counter", ">", 0.0f);
    layer->add_transition("positive", "high", "counter", ">", 5.0f);
    layer->add_transition("high", "positive", "counter", "<", 5.1f);
    layer->add_transition("positive", "neutral", "counter", "<", 0.1f);

    SECTION("Initial state") {
        REQUIRE(layer->current_state() == "neutral");
    }

    SECTION("Transition to positive when counter > 0") {
        machine.set_input("counter", 1.0f);
        machine.update(0.016f);

        REQUIRE(layer->current_state() == "positive");
    }

    SECTION("Transition to high when counter > 5") {
        machine.set_input("counter", 6.0f);
        machine.update(0.016f);

        // First transition: neutral -> positive
        REQUIRE(layer->current_state() == "positive");

        machine.update(0.016f);
        // Second transition: positive -> high
        REQUIRE(layer->current_state() == "high");
    }

    SECTION("Backward transition when counter decreases") {
        // Go to high state
        machine.set_input("counter", 10.0f);
        machine.update(0.016f);
        machine.update(0.016f);
        REQUIRE(layer->current_state() == "high");

        // Decrease counter
        machine.set_input("counter", 3.0f);
        machine.update(0.016f);
        REQUIRE(layer->current_state() == "positive");

        // Decrease further
        machine.set_input("counter", 0.0f);
        machine.update(0.016f);
        REQUIRE(layer->current_state() == "neutral");
    }
}

TEST_CASE("RuntimeStateMachine: State change callback", "[runtime][machine]") {
    RuntimeStateMachine machine("testMachine");
    machine.add_layer("status");

    auto* layer = machine.get_layer("status");
    layer->add_state("idle", true, "toIdle");
    layer->add_state("active", false, "toActive");
    layer->add_transition("idle", "active", "trigger", ">", 0.0f);

    std::string callback_layer;
    std::string callback_from;
    std::string callback_to;
    std::string callback_anim;
    std::string callback_play_audio;
    std::string callback_stop_audio;

    machine.set_state_change_callback(
        [&](const std::string& layer, const std::string& from,
            const std::string& to, const std::string& anim,
            const std::string& play_audio, const std::string& stop_audio) {
            callback_layer = layer;
            callback_from = from;
            callback_to = to;
            callback_anim = anim;
            callback_play_audio = play_audio;
            callback_stop_audio = stop_audio;
        });

    machine.set_input("trigger", 1.0f);
    machine.update(0.016f);

    REQUIRE(callback_layer == "status");
    REQUIRE(callback_from == "idle");
    REQUIRE(callback_to == "active");
    REQUIRE(callback_anim == "toActive");
}

TEST_CASE("RuntimeStateMachine: Negative value transitions", "[runtime][machine]") {
    RuntimeStateMachine machine("testMachine");
    machine.add_layer("status");

    auto* layer = machine.get_layer("status");
    layer->add_state("neutral", true, "");
    layer->add_state("negative", false, "");
    layer->add_state("veryLow", false, "");

    layer->add_transition("neutral", "negative", "counter", "<", 0.0f);
    layer->add_transition("negative", "veryLow", "counter", "<", -5.0f);

    REQUIRE(layer->current_state() == "neutral");

    machine.set_input("counter", -1.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "negative");

    machine.set_input("counter", -10.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "veryLow");
}

// ============================================================================
// RUNTIME TRANSITION TESTS
// ============================================================================

TEST_CASE("RuntimeTransition: Condition operators", "[runtime][transition]") {
    std::unordered_map<std::string, float> inputs;
    inputs["x"] = 5.0f;

    SECTION("Greater than") {
        RuntimeTransition trans("a", "b", "x", ">", 3.0f);
        REQUIRE(trans.check(inputs) == true);

        RuntimeTransition trans2("a", "b", "x", ">", 5.0f);
        REQUIRE(trans2.check(inputs) == false);
    }

    SECTION("Less than") {
        RuntimeTransition trans("a", "b", "x", "<", 10.0f);
        REQUIRE(trans.check(inputs) == true);

        RuntimeTransition trans2("a", "b", "x", "<", 3.0f);
        REQUIRE(trans2.check(inputs) == false);
    }

    SECTION("Equal") {
        RuntimeTransition trans("a", "b", "x", "==", 5.0f);
        REQUIRE(trans.check(inputs) == true);

        RuntimeTransition trans2("a", "b", "x", "==", 5.1f);
        REQUIRE(trans2.check(inputs) == false);
    }

    SECTION("Not equal") {
        RuntimeTransition trans("a", "b", "x", "!=", 3.0f);
        REQUIRE(trans.check(inputs) == true);

        RuntimeTransition trans2("a", "b", "x", "!=", 5.0f);
        REQUIRE(trans2.check(inputs) == false);
    }

    SECTION("Missing input returns false") {
        RuntimeTransition trans("a", "b", "missing", ">", 0.0f);
        REQUIRE(trans.check(inputs) == false);
    }
}

// ============================================================================
// TIMELINE ANIMATION TESTS (Unified System)
// ============================================================================

TEST_CASE("Timeline: Basic creation", "[runtime][timeline]") {
    ArenaAllocator alloc{4096};
    auto timeline = Timeline::create("fadeIn", alloc);

    REQUIRE(std::string(timeline->name()) == "fadeIn");
    REQUIRE(timeline->duration() == 0.0f);  // Auto duration when no tracks
}

TEST_CASE("Timeline: Configuration", "[runtime][timeline]") {
    ArenaAllocator alloc{4096};
    auto timeline = Timeline::create("moveAndFade", alloc);

    timeline->set_duration(2.0f);
    timeline->set_loop_mode(LoopMode::Loop);

    REQUIRE(timeline->duration() == 2.0f);
    REQUIRE(timeline->loop_mode() == LoopMode::Loop);
}

TEST_CASE("TimelinePlayer: Start and advance", "[runtime][timeline]") {
    ArenaAllocator alloc{4096};
    auto timeline = Timeline::create("test", alloc);
    timeline->set_duration(1.0f);
    timeline->set_loop_mode(LoopMode::Once);

    // Create a dummy node for the player
    auto node = Group::create();
    TimelinePlayer player(timeline.get(), node.get());

    REQUIRE(player.is_playing() == false);

    player.play();
    REQUIRE(player.is_playing() == true);

    // Advance past duration
    player.advance(1.5f);
    REQUIRE(player.is_playing() == false);
    REQUIRE(player.is_finished() == true);
}

TEST_CASE("TimelinePlayer: Loop mode", "[runtime][timeline]") {
    ArenaAllocator alloc{4096};
    auto timeline = Timeline::create("looping", alloc);
    timeline->set_duration(1.0f);
    timeline->set_loop_mode(LoopMode::Loop);

    auto node = Group::create();
    TimelinePlayer player(timeline.get(), node.get());

    player.play();

    // Advance past duration multiple times
    player.advance(1.5f);
    REQUIRE(player.is_playing() == true);

    player.advance(1.5f);
    REQUIRE(player.is_playing() == true);
}

TEST_CASE("TimelinePlayer: Stop", "[runtime][timeline]") {
    ArenaAllocator alloc{4096};
    auto timeline = Timeline::create("test", alloc);
    timeline->set_duration(10.0f);

    auto node = Group::create();
    TimelinePlayer player(timeline.get(), node.get());

    player.play();
    REQUIRE(player.is_playing() == true);

    player.stop();
    REQUIRE(player.is_playing() == false);
}

// ============================================================================
// TRACK TESTS (Timeline System)
// ============================================================================

TEST_CASE("Track: Add keyframes", "[runtime][track]") {
    ArenaAllocator alloc{4096};
    auto track = Track::create("opacity", alloc);

    track->add_keyframe(0.0f, 0.0f);
    track->add_keyframe(1.0f, 1.0f);

    REQUIRE(track->keyframe_count() == 2);
}

TEST_CASE("Track: Sample interpolation", "[runtime][track]") {
    ArenaAllocator alloc{4096};
    auto track = Track::create("x", alloc);

    track->add_keyframe(0.0f, 0.0f);
    track->add_keyframe(1.0f, 100.0f);

    // Sample at midpoint
    auto value = track->sample(0.5f);
    REQUIRE(std::holds_alternative<float>(value));
    REQUIRE_THAT(std::get<float>(value), WithinAbs(50.0f, 0.1f));

    // Sample at start
    value = track->sample(0.0f);
    REQUIRE_THAT(std::get<float>(value), WithinAbs(0.0f, 0.1f));

    // Sample at end
    value = track->sample(1.0f);
    REQUIRE_THAT(std::get<float>(value), WithinAbs(100.0f, 0.1f));
}

// ============================================================================
// ANIMATION CONTROLLER TESTS (Timeline System)
// ============================================================================

TEST_CASE("AnimationController: Add and get timeline", "[runtime][controller]") {
    ArenaAllocator alloc{4096};
    AnimationController controller(alloc);

    auto timeline = Timeline::create("test", alloc);
    timeline->set_duration(1.0f);
    controller.add_timeline(timeline);

    REQUIRE(controller.get_timeline("test") != nullptr);
    REQUIRE(controller.get_timeline("nonexistent") == nullptr);
}

TEST_CASE("AnimationController: Play and stop", "[runtime][controller]") {
    ArenaAllocator alloc{4096};
    AnimationController controller(alloc);

    auto timeline = Timeline::create("test", alloc);
    timeline->set_duration(1.0f);
    controller.add_timeline(timeline);

    auto node = Group::create();
    auto* player = controller.play("test", node.get());
    REQUIRE(player != nullptr);
    REQUIRE(player->is_playing() == true);

    controller.stop("test");
    // After stop, player is marked as stopped
}

TEST_CASE("AnimationController: Advance updates players", "[runtime][controller]") {
    ArenaAllocator alloc{4096};
    AnimationController controller(alloc);

    auto timeline1 = Timeline::create("anim1", alloc);
    timeline1->set_duration(1.0f);
    timeline1->set_loop_mode(LoopMode::Once);
    controller.add_timeline(timeline1);

    auto timeline2 = Timeline::create("anim2", alloc);
    timeline2->set_duration(2.0f);
    timeline2->set_loop_mode(LoopMode::Once);
    controller.add_timeline(timeline2);

    auto node = Group::create();
    controller.play("anim1", node.get());
    controller.play("anim2", node.get());

    // Advance for 1.5 seconds
    controller.advance(1.5f);

    // anim1 (1s duration) should be finished
    REQUIRE(controller.is_playing("anim1") == false);

    // anim2 (2s duration) should still be running
    REQUIRE(controller.is_playing("anim2") == true);
}

// ============================================================================
// EASING TESTS (types.h Easing struct)
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
    REQUIRE_THAT(easing.evaluate(0.5f), WithinAbs(0.5f, 0.05f));  // Symmetric at midpoint (wider tolerance for bezier)
    REQUIRE_THAT(easing.evaluate(1.0f), WithinAbs(1.0f, 0.001f));
}

// ============================================================================
// INTEGRATION TESTS: Parser -> Runtime
// ============================================================================

TEST_CASE("Integration: Parse and create runtime state machine", "[runtime][integration]") {
    const char* source = R"(
        machine statusTracker {
            layer status {
                state neutral { initial: true, animation: "toNeutral" }
                state positive { animation: "toPositive" }
                state high { animation: "toHigh" }

                transition neutral -> positive when counter > 0
                transition positive -> high when counter > 5
                transition high -> positive when counter < 5.1
                transition positive -> neutral when counter < 0.1
            }
        }
    )";

    auto program = parse(source);
    REQUIRE(program != nullptr);
    REQUIRE(program->machines.size() == 1);

    auto& ast_machine = program->machines[0];
    REQUIRE(ast_machine->name == "statusTracker");

    // Create runtime machine from AST
    RuntimeStateMachine machine(ast_machine->name);

    for (const auto& ast_layer : ast_machine->layers) {
        machine.add_layer(ast_layer.name);
        auto* layer = machine.get_layer(ast_layer.name);

        for (const auto& ast_state : ast_layer.states) {
            layer->add_state(ast_state.name, ast_state.initial, ast_state.animation);
        }

        for (const auto& ast_trans : ast_layer.transitions) {
            layer->add_transition(
                ast_trans.from_state,
                ast_trans.to_state,
                ast_trans.condition_var,
                ast_trans.condition_op,
                ast_trans.condition_val
            );
        }
    }

    // Test the runtime machine
    auto* layer = machine.get_layer("status");
    REQUIRE(layer != nullptr);
    REQUIRE(layer->current_state() == "neutral");

    machine.set_input("counter", 1.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "positive");

    machine.set_input("counter", 10.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "high");
}

TEST_CASE("Integration: Parse and create timeline from AST", "[runtime][integration]") {
    const char* source = R"(
        anim "fadeIn" {
            duration: 2.0
            loop: once

            track "opacity" {
                keyframe 0 -> 0.0
                keyframe 2.0 -> 1.0
            }

            track "x" {
                keyframe 0 -> 0
                keyframe 1.0 -> 50
                keyframe 2.0 -> 100
            }
        }
    )";

    auto program = parse(source);
    REQUIRE(program != nullptr);
    REQUIRE(program->animations.size() == 1);

    auto& ast_anim = program->animations[0];
    REQUIRE(ast_anim->name == "fadeIn");
    REQUIRE(ast_anim->duration == 2.0f);
    REQUIRE(ast_anim->loop_mode == "once");
    REQUIRE(ast_anim->tracks.size() == 2);

    // Verify AST keyframes were parsed correctly
    REQUIRE(ast_anim->tracks[0].property == "opacity");
    REQUIRE(ast_anim->tracks[0].keyframes.size() == 2);
    REQUIRE(ast_anim->tracks[1].property == "x");
    REQUIRE(ast_anim->tracks[1].keyframes.size() == 3);

    // Create Timeline from AST using Definition::load
    auto def = Definition::load(source);
    REQUIRE(def != nullptr);
    REQUIRE(def->has_error() == false);

    // Timelines should be created by AstToRuntimeConverter
    REQUIRE(def->timelines().size() == 1);

    auto& timeline = def->timelines()[0];
    REQUIRE(std::string(timeline->name()) == "fadeIn");
    REQUIRE(timeline->duration() == 2.0f);
    REQUIRE(timeline->loop_mode() == LoopMode::Once);

    // Check tracks
    auto* opacity_track = timeline->get_track("opacity");
    REQUIRE(opacity_track != nullptr);
    REQUIRE(opacity_track->keyframe_count() == 2);

    auto* x_track = timeline->get_track("x");
    REQUIRE(x_track != nullptr);
    REQUIRE(x_track->keyframe_count() == 3);
}

TEST_CASE("Integration: State machine triggers animation via Instance", "[runtime][integration]") {
    // This tests the complete flow using the unified Timeline system:
    // 1. State machine changes state
    // 2. Callback receives the animation name
    // 3. Timeline animation is started via AnimationController

    const char* source = R"(
        scene TestScene {
            width: 400
            height: 300
        }

        machine test {
            layer status {
                state idle { initial: true, animation: "toIdle" }
                state active { animation: "toActive" }
                transition idle -> active when trigger > 0
            }
        }

        anim "toIdle" {
            duration: 0.5
            track "opacity" { keyframe 0 -> 1.0 }
        }

        anim "toActive" {
            duration: 0.5
            track "opacity" { keyframe 0 -> 0.5 }
        }
    )";

    auto def = Definition::load(source);
    REQUIRE(def != nullptr);
    REQUIRE(def->has_error() == false);

    // Verify timelines were parsed
    REQUIRE(def->timelines().size() == 2);

    auto instance = Instance::create(def);
    REQUIRE(instance != nullptr);

    // Verify timelines are in controller
    auto* controller = instance->animation_controller();
    REQUIRE(controller != nullptr);
    REQUIRE(controller->get_timeline("toIdle") != nullptr);
    REQUIRE(controller->get_timeline("toActive") != nullptr);

    // Verify initial state
    auto* machine = instance->get_machine("test");
    REQUIRE(machine != nullptr);

    auto* layer = machine->get_layer("status");
    REQUIRE(layer != nullptr);
    REQUIRE(layer->current_state() == "idle");

    // Trigger transition
    instance->set_input("trigger", 1.0f);
    instance->advance(0.016f);

    // State should change
    REQUIRE(layer->current_state() == "active");

    // Animation should be playing via AnimationController
    REQUIRE(controller->is_playing("toActive") == true);
}

// ============================================================================
// DATA BINDING EXAMPLE TEST
// ============================================================================

TEST_CASE("Integration: data_binding.flex example runtime", "[runtime][example]") {
    const char* source = R"(
        scene counter {
            width: 400
            height: 300
        }

        machine statusTracker {
            layer status {
                state neutral { initial: true, animation: "toNeutral" }
                state positive { animation: "toPositive" }
                state high { animation: "toHigh" }
                state veryHigh { animation: "toVeryHigh" }
                state negative { animation: "toNegative" }
                state veryLow { animation: "toVeryLow" }

                transition neutral -> positive when counter > 0
                transition positive -> high when counter > 5
                transition high -> veryHigh when counter > 10

                transition veryHigh -> high when counter < 10.1
                transition high -> positive when counter < 5.1
                transition positive -> neutral when counter < 0.1

                transition neutral -> negative when counter < 0
                transition negative -> veryLow when counter < -5

                transition veryLow -> negative when counter > -5.1
                transition negative -> neutral when counter > -0.1
            }
        }

        anim "toVeryHigh" { duration: 0.1, track "color" { keyframe 0 -> #ff0000 } }
        anim "toHigh" { duration: 0.1, track "color" { keyframe 0 -> #ffaa00 } }
        anim "toPositive" { duration: 0.1, track "color" { keyframe 0 -> #00ff88 } }
        anim "toNeutral" { duration: 0.1, track "color" { keyframe 0 -> #888888 } }
        anim "toNegative" { duration: 0.1, track "color" { keyframe 0 -> #ffaa00 } }
        anim "toVeryLow" { duration: 0.1, track "color" { keyframe 0 -> #ff006e } }
    )";

    // Load using Definition::load which uses the unified Timeline system
    auto def = Definition::load(source);
    REQUIRE(def != nullptr);
    REQUIRE(def->has_error() == false);
    REQUIRE(def->machines().size() == 1);
    REQUIRE(def->timelines().size() == 6);

    auto instance = Instance::create(def);
    REQUIRE(instance != nullptr);

    std::string last_animation;
    auto* machine = instance->get_machine("statusTracker");
    REQUIRE(machine != nullptr);

    // Track animation changes via state machine callback
    bool callback_called = false;
    machine->set_state_change_callback(
        [&](const std::string& layer, const std::string& from,
            const std::string& to, const std::string& anim,
            const std::string& play_audio, const std::string& stop_audio) {
            callback_called = true;
            last_animation = anim;
        });

    auto* layer = machine->get_layer("status");
    REQUIRE(layer->current_state() == "neutral");

    // Test: counter = 1 -> positive
    instance->set_input("counter", 1.0f);
    instance->advance(0.016f);
    REQUIRE(layer->current_state() == "positive");
    REQUIRE(callback_called == true);  // Check if callback was called at all
    REQUIRE(last_animation == "toPositive");

    // Test: counter = 6 -> high
    instance->set_input("counter", 6.0f);
    instance->advance(0.016f);
    REQUIRE(layer->current_state() == "high");
    REQUIRE(last_animation == "toHigh");

    // Test: counter = 15 -> veryHigh
    instance->set_input("counter", 15.0f);
    instance->advance(0.016f);
    REQUIRE(layer->current_state() == "veryHigh");
    REQUIRE(last_animation == "toVeryHigh");

    // Test: backward counter = 3 -> high -> positive
    instance->set_input("counter", 3.0f);
    instance->advance(0.016f);
    REQUIRE(layer->current_state() == "high");

    instance->advance(0.016f);
    REQUIRE(layer->current_state() == "positive");

    // Test: negative counter = -1 -> negative (via neutral)
    instance->set_input("counter", 0.0f);
    instance->advance(0.016f);
    REQUIRE(layer->current_state() == "neutral");

    instance->set_input("counter", -1.0f);
    instance->advance(0.016f);
    REQUIRE(layer->current_state() == "negative");
    REQUIRE(last_animation == "toNegative");

    // Test: counter = -10 -> veryLow
    instance->set_input("counter", -10.0f);
    instance->advance(0.016f);
    REQUIRE(layer->current_state() == "veryLow");
    REQUIRE(last_animation == "toVeryLow");
}

// ============================================================================
// ASSET MANAGER TESTS
// ============================================================================

TEST_CASE("AssetManager: Basic creation", "[runtime][assets]") {
    AssetManager manager;

    // Should start empty
    REQUIRE(manager.has("nonexistent") == false);
    REQUIRE(manager.get("nonexistent") == nullptr);
}

TEST_CASE("AssetManager: Register audio", "[runtime][assets]") {
    AssetManager manager;

    AudioOptions opts;
    opts.loop = true;
    opts.volume = 0.5f;

    manager.register_audio("bgm", "sounds/background.mp3", opts);

    REQUIRE(manager.has("bgm") == true);

    auto* entry = manager.get("bgm");
    REQUIRE(entry != nullptr);
    REQUIRE(entry->type == AssetType::Audio);
    REQUIRE(entry->path == "sounds/background.mp3");
    REQUIRE(entry->audio_opts.loop == true);
    REQUIRE(entry->audio_opts.volume == 0.5f);
}

TEST_CASE("AssetManager: Register multiple asset types", "[runtime][assets]") {
    AssetManager manager;

    manager.register_audio("click", "sounds/click.wav");
    manager.register_image("logo", "images/logo.png");
    manager.register_font("main", "fonts/roboto.ttf");

    REQUIRE(manager.has("click") == true);
    REQUIRE(manager.has("logo") == true);
    REQUIRE(manager.has("main") == true);

    REQUIRE(manager.get("click")->type == AssetType::Audio);
    REQUIRE(manager.get("logo")->type == AssetType::Image);
    REQUIRE(manager.get("main")->type == AssetType::Font);
}

TEST_CASE("AssetManager: Resolve path", "[runtime][assets]") {
    AssetManager manager;

    manager.register_audio("bgm", "sounds/music.mp3");

    REQUIRE(std::string(manager.resolve_path("bgm")) == "sounds/music.mp3");
    REQUIRE(std::string(manager.resolve_path("nonexistent")) == "");
}

TEST_CASE("AssetManager: Get all by type", "[runtime][assets]") {
    AssetManager manager;

    manager.register_audio("click", "sounds/click.wav");
    manager.register_audio("hover", "sounds/hover.wav");
    manager.register_audio("bgm", "sounds/bgm.mp3");
    manager.register_image("logo", "images/logo.png");

    auto audio_assets = manager.get_all(AssetType::Audio);
    REQUIRE(audio_assets.size() == 3);

    auto image_assets = manager.get_all(AssetType::Image);
    REQUIRE(image_assets.size() == 1);

    auto font_assets = manager.get_all(AssetType::Font);
    REQUIRE(font_assets.size() == 0);
}

TEST_CASE("AssetManager: Clear", "[runtime][assets]") {
    AssetManager manager;

    manager.register_audio("click", "sounds/click.wav");
    manager.register_image("logo", "images/logo.png");

    REQUIRE(manager.has("click") == true);
    REQUIRE(manager.has("logo") == true);

    manager.clear();

    REQUIRE(manager.has("click") == false);
    REQUIRE(manager.has("logo") == false);
}

TEST_CASE("AssetManager: Audio playback with NullBackend", "[runtime][assets]") {
    AssetManager manager;
    // Default backend is NullAudioBackend

    manager.register_audio("click", "sounds/click.wav");

    // Play returns -1 with NullBackend (no actual playback)
    int channel = manager.play_audio("click");
    REQUIRE(channel == -1);

    // Should not crash
    manager.stop_audio("click");
    manager.stop_audio_channel(0);
    manager.set_audio_volume(0, 0.5f);
    REQUIRE(manager.is_audio_playing(0) == false);
}

// ============================================================================
// STATE MACHINE WITH AUDIO ACTIONS
// ============================================================================

TEST_CASE("RuntimeStateMachine: State with play/stop audio", "[runtime][machine][audio]") {
    RuntimeStateMachine machine("audioMachine");
    machine.add_layer("main");
    auto* layer = machine.get_layer("main");

    // Add states with audio actions
    layer->add_state("idle", true, "", "", "");  // initial, no audio
    layer->add_state("playing", false, "toPlaying", "bgm", "");  // play bgm
    layer->add_state("stopped", false, "toStopped", "", "bgm");  // stop bgm

    layer->add_transition("idle", "playing", "play", ">", 0.0f);
    layer->add_transition("playing", "stopped", "stop", ">", 0.0f);

    std::string last_play_audio;
    std::string last_stop_audio;

    machine.set_state_change_callback(
        [&](const std::string& layer_name, const std::string& from,
            const std::string& to, const std::string& anim,
            const std::string& play_audio, const std::string& stop_audio) {
            last_play_audio = play_audio;
            last_stop_audio = stop_audio;
        });

    // Initial state
    REQUIRE(layer->current_state() == "idle");
    REQUIRE(last_play_audio.empty());
    REQUIRE(last_stop_audio.empty());

    // Transition to playing - should trigger play: bgm
    machine.set_input("play", 1.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "playing");
    REQUIRE(last_play_audio == "bgm");
    REQUIRE(last_stop_audio.empty());

    // Reset for next transition
    last_play_audio.clear();

    // Transition to stopped - should trigger stop: bgm
    machine.set_input("stop", 1.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "stopped");
    REQUIRE(last_play_audio.empty());
    REQUIRE(last_stop_audio == "bgm");
}

// ============================================================================
// DEFINITION + INSTANCE WITH ASSETS
// ============================================================================

TEST_CASE("Definition: Parse and convert assets", "[runtime][assets][integration]") {
    auto def = Definition::load(R"(
        assets {
            audio click: "sounds/click.wav"
            audio bgm: "sounds/background.mp3" {
                loop: true
                volume: 0.5
            }
            image logo: "images/logo.png"
        }

        scene Test {
            width: 800
            height: 600
        }
    )");

    REQUIRE(def != nullptr);
    REQUIRE(def->has_error() == false);

    // Create instance - assets should be automatically registered
    auto instance = Instance::create(def);
    REQUIRE(instance != nullptr);

    auto* manager = instance->asset_manager();
    REQUIRE(manager != nullptr);

    // Verify assets were registered
    REQUIRE(manager->has("click") == true);
    REQUIRE(manager->has("bgm") == true);
    REQUIRE(manager->has("logo") == true);

    // Verify audio options
    auto* bgm = manager->get("bgm");
    REQUIRE(bgm->audio_opts.loop == true);
    REQUIRE(bgm->audio_opts.volume == 0.5f);
}

TEST_CASE("Instance: Audio API", "[runtime][assets][integration]") {
    auto instance = Instance::create(800.0f, 600.0f);

    // Manually register audio
    instance->register_audio("click", "sounds/click.wav", false, 1.0f);
    instance->register_audio("bgm", "sounds/bgm.mp3", true, 0.5f);

    auto* manager = instance->asset_manager();
    REQUIRE(manager != nullptr);
    REQUIRE(manager->has("click") == true);
    REQUIRE(manager->has("bgm") == true);

    // Play (returns -1 with NullBackend)
    int channel = instance->play_audio("click");
    REQUIRE(channel == -1);  // NullBackend

    // Stop should not crash
    instance->stop_audio("bgm");
    instance->stop_audio_channel(0);
}

// ============================================================================
// IMPORT STATEMENT RUNTIME TESTS
// ============================================================================

TEST_CASE("Definition: Load source with imports preserves import info", "[runtime][import]") {
    // Test that import statements are parsed correctly into AST
    // Note: Definition::load() doesn't resolve imports (no base directory)
    // but it should parse them into the program structure
    const char* source = R"(
        import "animations/fade.flex"
        import "components/button.flex"

        scene MainScene {
            width: 800
            height: 600
            rect bg { fill: #000000 }
        }
    )";

    auto program = parse(source);
    REQUIRE(program != nullptr);
    REQUIRE(program->imports.size() == 2);
    REQUIRE(program->imports[0].path == "animations/fade.flex");
    REQUIRE(program->imports[1].path == "components/button.flex");
    REQUIRE(program->scene != nullptr);
    REQUIRE(program->scene->name == "MainScene");
}

TEST_CASE("Definition: Load with only imports (no scene)", "[runtime][import]") {
    // A file with only imports should parse successfully
    const char* source = R"(
        import "shared/animations.flex"
        import "shared/machines.flex"
    )";

    auto program = parse(source);
    REQUIRE(program != nullptr);
    REQUIRE(program->imports.size() == 2);
    REQUIRE(program->scene == nullptr);  // No scene in this file
}

TEST_CASE("Definition: Import with animations and machines", "[runtime][import]") {
    // Test that imports can coexist with local animations and machines
    const char* source = R"(
        import "base/common.flex"

        anim "localFade" {
            duration: 0.5
            track "opacity" {
                keyframe 0 -> 0.0
                keyframe 0.5 -> 1.0
            }
        }

        machine localMachine {
            layer main {
                state idle { initial: true }
                state active {}
                transition idle -> active when trigger > 0
            }
        }
    )";

    auto program = parse(source);
    REQUIRE(program != nullptr);
    REQUIRE(program->imports.size() == 1);
    REQUIRE(program->imports[0].path == "base/common.flex");
    REQUIRE(program->animations.size() == 1);
    REQUIRE(program->animations[0]->name == "localFade");
    REQUIRE(program->machines.size() == 1);
    REQUIRE(program->machines[0]->name == "localMachine");
}

TEST_CASE("Definition: Import line tracking", "[runtime][import]") {
    // Test that import statements track their source location
    const char* source = "import \"test.flex\"";

    auto program = parse(source);
    REQUIRE(program != nullptr);
    REQUIRE(program->imports.size() == 1);
    REQUIRE(program->imports[0].path == "test.flex");
    REQUIRE(program->imports[0].line == 1);
}

TEST_CASE("Definition: load() creates valid instance without imports", "[runtime][import]") {
    // Definition::load() parses but doesn't resolve imports
    // It should still create a valid Definition with local content
    auto def = Definition::load(R"(
        import "animations/unused.flex"

        scene Test {
            width: 400
            height: 300
            rect box { x: 10, y: 20, fill: #FF0000 }
        }

        anim "fadeIn" {
            duration: 1.0
            track "opacity" {
                keyframe 0 -> 0.0
                keyframe 1.0 -> 1.0
            }
        }
    )");

    REQUIRE(def != nullptr);
    REQUIRE(def->has_error() == false);
    REQUIRE(def->artboard() != nullptr);

    // Create instance from definition
    auto instance = Instance::create(def);
    REQUIRE(instance != nullptr);
    REQUIRE(instance->artboard() != nullptr);

    // Local animation should be available (via Timeline system)
    REQUIRE(def->timelines().size() >= 1);
    // Check timeline by iterating (timelines is a vector, not a map)
    bool found_fadeIn = false;
    for (const auto& tl : def->timelines()) {
        if (std::string(tl->name()) == "fadeIn") {
            found_fadeIn = true;
            break;
        }
    }
    REQUIRE(found_fadeIn == true);
}

TEST_CASE("Definition: Full program with all elements including imports", "[runtime][import]") {
    // Test parsing a complete program with imports, assets, scene, animations, and machines
    const char* source = R"(
        import "shared/base.flex"

        assets {
            audio click: "sounds/click.wav"
        }

        scene CompleteApp {
            width: 1024
            height: 768

            rect background {
                x: 0, y: 0
                width: 1024, height: 768
                fill: #1a1a2e
            }

            group content {
                x: 100, y: 100

                text title {
                    content: "Import Test"
                    fontSize: 24
                    color: #ffffff
                }
            }
        }

        anim "fadeTitle" {
            duration: 0.5
            track "#title/opacity" {
                keyframe 0 -> 0.0
                keyframe 0.5 -> 1.0
            }
        }

        machine uiState {
            layer visibility {
                state hidden { initial: true }
                state visible { animation: "fadeTitle" }
                transition hidden -> visible when show > 0
            }
        }
    )";

    auto program = parse(source);
    REQUIRE(program != nullptr);

    // Verify all sections parsed
    REQUIRE(program->imports.size() == 1);
    REQUIRE(program->imports[0].path == "shared/base.flex");
    REQUIRE(program->assets != nullptr);
    REQUIRE(program->assets->assets.size() == 1);
    REQUIRE(program->scene != nullptr);
    REQUIRE(program->scene->name == "CompleteApp");
    REQUIRE(program->animations.size() == 1);
    REQUIRE(program->machines.size() == 1);

    // Create Definition and Instance
    auto def = Definition::load(source);
    REQUIRE(def != nullptr);
    REQUIRE(def->has_error() == false);

    auto instance = Instance::create(def);
    REQUIRE(instance != nullptr);

    // Assets should be registered
    auto* asset_mgr = instance->asset_manager();
    REQUIRE(asset_mgr != nullptr);
    REQUIRE(asset_mgr->has("click") == true);

    // State machine should be available
    auto* machine = instance->get_machine("uiState");
    REQUIRE(machine != nullptr);
    REQUIRE(machine->get_layer("visibility") != nullptr);
    REQUIRE(machine->get_layer("visibility")->current_state() == "hidden");
}
