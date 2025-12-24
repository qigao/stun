/*
 * Flex Runtime System Tests
 * Tests RuntimeStateMachine, RuntimeAnimation, and their integration
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "flex.h"

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

    machine.set_state_change_callback(
        [&](const std::string& layer, const std::string& from,
            const std::string& to, const std::string& anim) {
            callback_layer = layer;
            callback_from = from;
            callback_to = to;
            callback_anim = anim;
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
// RUNTIME ANIMATION TESTS
// ============================================================================

TEST_CASE("RuntimeAnimation: Basic creation", "[runtime][animation]") {
    RuntimeAnimation anim("fadeIn");

    REQUIRE(anim.name() == "fadeIn");
    REQUIRE(anim.is_running() == false);
}

TEST_CASE("RuntimeAnimation: Configuration", "[runtime][animation]") {
    RuntimeAnimation anim("moveAndFade");

    anim.set_duration(2.0f);
    anim.set_loop_mode("loop");

    REQUIRE(anim.duration() == 2.0f);
    REQUIRE(anim.loop_mode() == "loop");
}

TEST_CASE("RuntimeAnimation: Start and update", "[runtime][animation]") {
    RuntimeAnimation anim("test");
    anim.set_duration(1.0f);
    anim.set_loop_mode("once");

    REQUIRE(anim.is_running() == false);

    anim.start();
    REQUIRE(anim.is_running() == true);

    // Update past duration
    anim.update(1.5f);
    REQUIRE(anim.is_running() == false);
    REQUIRE(anim.is_finished() == true);
}

TEST_CASE("RuntimeAnimation: Loop mode", "[runtime][animation]") {
    RuntimeAnimation anim("looping");
    anim.set_duration(1.0f);
    anim.set_loop_mode("loop");

    anim.start();

    // Update past duration multiple times
    anim.update(1.5f);
    REQUIRE(anim.is_running() == true);

    anim.update(1.5f);
    REQUIRE(anim.is_running() == true);
}

TEST_CASE("RuntimeAnimation: Stop", "[runtime][animation]") {
    RuntimeAnimation anim("test");
    anim.set_duration(10.0f);

    anim.start();
    REQUIRE(anim.is_running() == true);

    anim.stop();
    REQUIRE(anim.is_running() == false);
}

// ============================================================================
// RUNTIME TRACK TESTS
// ============================================================================

TEST_CASE("RuntimeTrack: Add keyframes", "[runtime][track]") {
    RuntimeTrack track("opacity");

    track.add_keyframe(0.0f, AstValue(0.0f));
    track.add_keyframe(1.0f, AstValue(1.0f));

    REQUIRE(track.keyframes().size() == 2);
    REQUIRE(track.keyframes()[0].time() == 0.0f);
    REQUIRE(track.keyframes()[1].time() == 1.0f);
}

TEST_CASE("RuntimeTrack: Keyframes sorted by time", "[runtime][track]") {
    RuntimeTrack track("x");

    // Add out of order
    track.add_keyframe(1.0f, AstValue(100.0f));
    track.add_keyframe(0.0f, AstValue(0.0f));
    track.add_keyframe(0.5f, AstValue(50.0f));

    REQUIRE(track.keyframes().size() == 3);
    REQUIRE(track.keyframes()[0].time() == 0.0f);
    REQUIRE(track.keyframes()[1].time() == 0.5f);
    REQUIRE(track.keyframes()[2].time() == 1.0f);
}

// ============================================================================
// ANIMATION MANAGER TESTS
// ============================================================================

TEST_CASE("AnimationManager: Create animation from AST", "[runtime][manager]") {
    AnimationManager manager;

    // Create AST animation
    AstAnim ast_anim("fadeIn");
    ast_anim.duration = 2.0f;
    ast_anim.loop_mode = "once";

    AstTrack track("opacity");
    track.keyframes.emplace_back(0.0f, AstValue(0.0f));
    track.keyframes.emplace_back(2.0f, AstValue(1.0f));
    ast_anim.tracks.push_back(track);

    auto anim = manager.create_animation(ast_anim);

    REQUIRE(anim != nullptr);
    REQUIRE(anim->name() == "fadeIn");
    REQUIRE(anim->duration() == 2.0f);
    REQUIRE(anim->loop_mode() == "once");

    auto* rt_track = anim->get_track("opacity");
    REQUIRE(rt_track != nullptr);
    REQUIRE(rt_track->keyframes().size() == 2);
}

TEST_CASE("AnimationManager: Get animation by name", "[runtime][manager]") {
    AnimationManager manager;

    AstAnim ast_anim("test");
    ast_anim.duration = 1.0f;
    manager.create_animation(ast_anim);

    REQUIRE(manager.get_animation("test") != nullptr);
    REQUIRE(manager.get_animation("nonexistent") == nullptr);
}

TEST_CASE("AnimationManager: Start and stop animation", "[runtime][manager]") {
    AnimationManager manager;

    AstAnim ast_anim("test");
    ast_anim.duration = 1.0f;
    manager.create_animation(ast_anim);

    manager.start_animation("test");
    REQUIRE(manager.get_animation("test")->is_running() == true);

    manager.stop_animation("test");
    REQUIRE(manager.get_animation("test")->is_running() == false);
}

TEST_CASE("AnimationManager: Update all animations", "[runtime][manager]") {
    AnimationManager manager;

    AstAnim anim1("anim1");
    anim1.duration = 1.0f;
    manager.create_animation(anim1);

    AstAnim anim2("anim2");
    anim2.duration = 2.0f;
    manager.create_animation(anim2);

    manager.start_animation("anim1");
    manager.start_animation("anim2");

    // Update for 1.5 seconds
    manager.update(1.5f);

    // anim1 (1s duration) should be finished
    REQUIRE(manager.get_animation("anim1")->is_running() == false);

    // anim2 (2s duration) should still be running
    REQUIRE(manager.get_animation("anim2")->is_running() == true);
}

// ============================================================================
// EASING FUNCTION TESTS
// ============================================================================

TEST_CASE("EasingFunction: Linear", "[runtime][easing]") {
    EasingFunction easing;
    easing.type = EasingType::Linear;

    REQUIRE_THAT(easing.ease(0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(easing.ease(0.5f), WithinAbs(0.5f, 0.001f));
    REQUIRE_THAT(easing.ease(1.0f), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("EasingFunction: EaseIn", "[runtime][easing]") {
    EasingFunction easing;
    easing.type = EasingType::EaseIn;

    REQUIRE_THAT(easing.ease(0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE(easing.ease(0.5f) < 0.5f);  // EaseIn is slower at start
    REQUIRE_THAT(easing.ease(1.0f), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("EasingFunction: EaseOut", "[runtime][easing]") {
    EasingFunction easing;
    easing.type = EasingType::EaseOut;

    REQUIRE_THAT(easing.ease(0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE(easing.ease(0.5f) > 0.5f);  // EaseOut is faster at start
    REQUIRE_THAT(easing.ease(1.0f), WithinAbs(1.0f, 0.001f));
}

TEST_CASE("EasingFunction: EaseInOut", "[runtime][easing]") {
    EasingFunction easing;
    easing.type = EasingType::EaseInOut;

    REQUIRE_THAT(easing.ease(0.0f), WithinAbs(0.0f, 0.001f));
    REQUIRE_THAT(easing.ease(0.5f), WithinAbs(0.5f, 0.001f));  // Symmetric at midpoint
    REQUIRE_THAT(easing.ease(1.0f), WithinAbs(1.0f, 0.001f));
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

TEST_CASE("Integration: Parse and create runtime animation", "[runtime][integration]") {
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

    // Create runtime animation using AnimationManager
    AnimationManager manager;
    auto anim = manager.create_animation(*ast_anim);

    REQUIRE(anim != nullptr);
    REQUIRE(anim->name() == "fadeIn");
    REQUIRE(anim->duration() == 2.0f);

    auto* opacity_track = anim->get_track("opacity");
    REQUIRE(opacity_track != nullptr);
    REQUIRE(opacity_track->keyframes().size() == 2);

    auto* x_track = anim->get_track("x");
    REQUIRE(x_track != nullptr);
    REQUIRE(x_track->keyframes().size() == 3);
}

TEST_CASE("Integration: State machine triggers animation", "[runtime][integration]") {
    // This tests the complete flow:
    // 1. State machine changes state
    // 2. Callback receives the animation name
    // 3. Animation is started

    RuntimeStateMachine machine("test");
    machine.add_layer("status");

    auto* layer = machine.get_layer("status");
    layer->add_state("idle", true, "toIdle");
    layer->add_state("active", false, "toActive");
    layer->add_transition("idle", "active", "trigger", ">", 0.0f);

    AnimationManager anim_manager;

    AstAnim idle_anim("toIdle");
    idle_anim.duration = 0.5f;
    anim_manager.create_animation(idle_anim);

    AstAnim active_anim("toActive");
    active_anim.duration = 0.5f;
    anim_manager.create_animation(active_anim);

    // Connect state machine to animation manager
    machine.set_state_change_callback(
        [&](const std::string& layer_name, const std::string& from,
            const std::string& to, const std::string& anim_name) {
            if (!anim_name.empty()) {
                anim_manager.start_animation(anim_name);
            }
        });

    // Verify initial state
    REQUIRE(layer->current_state() == "idle");
    REQUIRE(anim_manager.get_animation("toActive")->is_running() == false);

    // Trigger transition
    machine.set_input("trigger", 1.0f);
    machine.update(0.016f);

    // State should change
    REQUIRE(layer->current_state() == "active");

    // Animation should be started
    REQUIRE(anim_manager.get_animation("toActive")->is_running() == true);
}

// ============================================================================
// DATA BINDING EXAMPLE TEST
// ============================================================================

TEST_CASE("Integration: data_binding.flex example runtime", "[runtime][example]") {
    const char* source = R"(
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

    auto program = parse(source);
    REQUIRE(program != nullptr);
    REQUIRE(program->machines.size() == 1);
    REQUIRE(program->animations.size() == 6);

    // Build runtime from AST
    auto& ast_machine = program->machines[0];
    RuntimeStateMachine machine(ast_machine->name);

    for (const auto& ast_layer : ast_machine->layers) {
        machine.add_layer(ast_layer.name);
        auto* layer = machine.get_layer(ast_layer.name);

        for (const auto& ast_state : ast_layer.states) {
            layer->add_state(ast_state.name, ast_state.initial, ast_state.animation);
        }

        for (const auto& ast_trans : ast_layer.transitions) {
            layer->add_transition(
                ast_trans.from_state, ast_trans.to_state,
                ast_trans.condition_var, ast_trans.condition_op,
                ast_trans.condition_val
            );
        }
    }

    AnimationManager anim_manager;
    for (const auto& ast_anim : program->animations) {
        anim_manager.create_animation(*ast_anim);
    }

    std::string last_animation;
    machine.set_state_change_callback(
        [&](const std::string& layer, const std::string& from,
            const std::string& to, const std::string& anim) {
            last_animation = anim;
            if (!anim.empty()) {
                anim_manager.start_animation(anim);
            }
        });

    auto* layer = machine.get_layer("status");
    REQUIRE(layer->current_state() == "neutral");

    // Test: counter = 1 -> positive
    machine.set_input("counter", 1.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "positive");
    REQUIRE(last_animation == "toPositive");

    // Test: counter = 6 -> high
    machine.set_input("counter", 6.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "high");
    REQUIRE(last_animation == "toHigh");

    // Test: counter = 15 -> veryHigh
    machine.set_input("counter", 15.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "veryHigh");
    REQUIRE(last_animation == "toVeryHigh");

    // Test: backward counter = 3 -> high -> positive
    machine.set_input("counter", 3.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "high");

    machine.update(0.016f);
    REQUIRE(layer->current_state() == "positive");

    // Test: negative counter = -1 -> negative (via neutral)
    machine.set_input("counter", 0.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "neutral");

    machine.set_input("counter", -1.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "negative");
    REQUIRE(last_animation == "toNegative");

    // Test: counter = -10 -> veryLow
    machine.set_input("counter", -10.0f);
    machine.update(0.016f);
    REQUIRE(layer->current_state() == "veryLow");
    REQUIRE(last_animation == "toVeryLow");
}
