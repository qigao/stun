/**
 * Unit Tests for CSS Animation Transforms
 *
 * Tests that CSS animations and transitions properly update element transforms:
 * - @keyframes animation with rotate
 * - @keyframes animation with scale
 * - Transition on transform property
 * - Transform-origin handling
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <nanovg.h>
#include <cssbox.h>
#include "cssbox_internal.h"
#include <cmath>

using Catch::Matchers::WithinAbs;

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Helper to check if transform is identity
static bool is_identity_transform(const float* t) {
    return t[0] == 1.0f && t[1] == 0.0f &&
           t[2] == 0.0f && t[3] == 1.0f &&
           t[4] == 0.0f && t[5] == 0.0f;
}

// Helper to check if transform has rotation component
static bool has_rotation(const float* t) {
    // Rotation matrix: [cos, sin, -sin, cos, 0, 0]
    // If t[1] != 0 or t[2] != 0, there's rotation
    return t[1] != 0.0f || t[2] != 0.0f;
}

// Helper to check if transform has scale component
static bool has_scale(const float* t) {
    // Scale changes t[0] and t[3] from 1.0
    return t[0] != 1.0f || t[3] != 1.0f;
}

// Helper to extract rotation angle from transform matrix
static float get_rotation_degrees(const float* t) {
    // For rotation matrix [cos, sin, -sin, cos, tx, ty]
    // angle = atan2(sin, cos) = atan2(t[1], t[0])
    return std::atan2(t[1], t[0]) * 180.0f / (float)M_PI;
}

// Helper to extract scale from transform matrix
static float get_scale_x(const float* t) {
    // Scale X is sqrt(t[0]^2 + t[1]^2) for combined transform
    return std::sqrt(t[0] * t[0] + t[1] * t[1]);
}

// ============================================================================
// Animation Transform Tests
// ============================================================================

TEST_CASE("CSS Animation Transform - Rotate Keyframes", "[animation][transform]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        @keyframes spin {
            from { transform: rotate(0deg); }
            to { transform: rotate(360deg); }
        }

        #spinner {
            width: 100px;
            height: 100px;
            animation: spin 1s linear infinite;
        }
    )";

    int result = cssboxParseCSS(renderer, css);
    REQUIRE(result == 1);

    cssboxElement* spinner = cssboxCreateElement(renderer, "spinner", "div");
    REQUIRE(spinner != nullptr);

    // Initial state - should be identity
    cssboxComputeLayout(renderer);
    INFO("Initial transform: [" << spinner->transform[0] << ", " << spinner->transform[1]
         << ", " << spinner->transform[2] << ", " << spinner->transform[3]
         << ", " << spinner->transform[4] << ", " << spinner->transform[5] << "]");

    SECTION("Animation starts after first update") {
        // First update to start animation
        cssboxUpdate(renderer, 0.0f);

        // Check animation state was created
        REQUIRE(spinner->animation_state != nullptr);

        AnimationState* anim_state = static_cast<AnimationState*>(spinner->animation_state);
        REQUIRE(anim_state->running_animations.size() > 0);
        REQUIRE(anim_state->running_animations[0].animation_name == "spin");
        REQUIRE(anim_state->running_animations[0].active == true);
    }

    SECTION("Transform changes during animation") {
        // Start animation
        cssboxUpdate(renderer, 0.0f);

        // Advance to 25% through animation (90 degrees)
        cssboxUpdate(renderer, 0.25f);

        INFO("After 0.25s transform: [" << spinner->transform[0] << ", " << spinner->transform[1]
             << ", " << spinner->transform[2] << ", " << spinner->transform[3]
             << ", " << spinner->transform[4] << ", " << spinner->transform[5] << "]");

        // Transform should have rotation component
        CHECK(has_rotation(spinner->transform));

        // At 25%, rotation should be ~90 degrees
        float angle = get_rotation_degrees(spinner->transform);
        INFO("Rotation angle at 25%: " << angle << " degrees");
        CHECK_THAT(angle, WithinAbs(90.0f, 5.0f));
    }

    SECTION("Transform at 50% is 180 degrees") {
        cssboxUpdate(renderer, 0.0f);
        cssboxUpdate(renderer, 0.5f);

        float angle = get_rotation_degrees(spinner->transform);
        INFO("Rotation angle at 50%: " << angle << " degrees");
        CHECK_THAT(std::abs(angle), WithinAbs(180.0f, 5.0f));
    }

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("CSS Animation Transform - Scale Keyframes", "[animation][transform]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        @keyframes pulse {
            0% { transform: scale(1); }
            50% { transform: scale(1.5); }
            100% { transform: scale(1); }
        }

        #pulse-box {
            width: 100px;
            height: 100px;
            animation: pulse 1s ease-in-out infinite;
        }
    )";

    int result = cssboxParseCSS(renderer, css);
    REQUIRE(result == 1);

    cssboxElement* box = cssboxCreateElement(renderer, "pulse-box", "div");
    REQUIRE(box != nullptr);

    cssboxComputeLayout(renderer);

    SECTION("Scale changes during animation") {
        // Start animation
        cssboxUpdate(renderer, 0.0f);

        // Advance to 50% (max scale)
        cssboxUpdate(renderer, 0.5f);

        INFO("After 0.5s transform: [" << box->transform[0] << ", " << box->transform[1]
             << ", " << box->transform[2] << ", " << box->transform[3]
             << ", " << box->transform[4] << ", " << box->transform[5] << "]");

        float scale = get_scale_x(box->transform);
        INFO("Scale at 50%: " << scale);

        // At 50%, scale should be ~1.5
        CHECK_THAT(scale, WithinAbs(1.5f, 0.2f));
    }

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("CSS Transition Transform - Scale on Hover", "[transition][transform]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        #scale-box {
            width: 100px;
            height: 100px;
            transform: scale(1);
            transition: transform 0.3s ease;
        }

        #scale-box:hover {
            transform: scale(1.3);
        }
    )";

    int result = cssboxParseCSS(renderer, css);
    REQUIRE(result == 1);

    cssboxElement* box = cssboxCreateElement(renderer, "scale-box", "div");
    REQUIRE(box != nullptr);

    cssboxComputeLayout(renderer);
    cssboxUpdate(renderer, 0.0f);

    SECTION("Initial state is scale(1)") {
        float scale = get_scale_x(box->transform);
        INFO("Initial scale: " << scale);
        CHECK_THAT(scale, WithinAbs(1.0f, 0.01f));
    }

    SECTION("Hover triggers transition") {
        // Simulate hover
        cssboxSetPseudoStateEx(renderer, box, "hover", 1);

        // First update to detect change
        cssboxUpdate(renderer, 0.0f);

        // Check transition state was created
        REQUIRE(box->transition_state != nullptr);

        TransitionState* trans_state = static_cast<TransitionState*>(box->transition_state);
        INFO("Active transitions count: " << trans_state->active_transitions.size());

        // There should be an active transition for transform
        auto it = trans_state->active_transitions.find("transform");
        if (it != trans_state->active_transitions.end()) {
            CHECK(it->second.active == true);
            CHECK(it->second.property == "transform");
        }
    }

    SECTION("Scale changes during transition") {
        // Simulate hover
        cssboxSetPseudoStateEx(renderer, box, "hover", 1);
        cssboxUpdate(renderer, 0.0f);

        // Advance through transition
        cssboxUpdate(renderer, 0.15f);  // 50% through 0.3s transition

        float scale = get_scale_x(box->transform);
        INFO("Scale at 50% transition: " << scale);

        // Scale should be between 1.0 and 1.3
        CHECK(scale > 1.0f);
        CHECK(scale < 1.4f);
    }

    SECTION("Scale reaches target after transition completes") {
        cssboxSetPseudoStateEx(renderer, box, "hover", 1);
        cssboxUpdate(renderer, 0.0f);
        cssboxUpdate(renderer, 0.35f);  // Past transition duration

        float scale = get_scale_x(box->transform);
        INFO("Scale after transition: " << scale);
        CHECK_THAT(scale, WithinAbs(1.3f, 0.1f));
    }

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Animation Transform - Element is in animated_elements set", "[animation][internal]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        @keyframes spin {
            from { transform: rotate(0deg); }
            to { transform: rotate(360deg); }
        }

        #spinner {
            width: 100px;
            height: 100px;
            animation: spin 1s linear infinite;
        }
    )";

    cssboxParseCSS(renderer, css);
    cssboxElement* spinner = cssboxCreateElement(renderer, "spinner", "div");

    // Before any update
    CHECK(renderer->animated_elements_.empty());

    // First layout + update
    cssboxComputeLayout(renderer);
    cssboxUpdate(renderer, 0.0f);

    // Element should be tracked as animated
    INFO("animated_elements_ size: " << renderer->animated_elements_.size());
    CHECK(renderer->animated_elements_.count(spinner->internal_id) > 0);

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Transform applied correctly in inline_style", "[animation][transform]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        @keyframes spin {
            from { transform: rotate(0deg); }
            to { transform: rotate(360deg); }
        }

        #spinner {
            animation: spin 1s linear infinite;
        }
    )";

    cssboxParseCSS(renderer, css);
    cssboxElement* spinner = cssboxCreateElement(renderer, "spinner", "div");

    cssboxComputeLayout(renderer);
    cssboxUpdate(renderer, 0.0f);
    cssboxUpdate(renderer, 0.25f);

    // Check if transform was applied to inline_style (fallback path)
    auto it = spinner->inline_style.find("transform");
    if (it != spinner->inline_style.end()) {
        INFO("inline_style transform: " << it->second);
        // Should contain rotate
        CHECK(it->second.find("rotate") != std::string::npos);
    }

    // Or check if transform matrix is updated directly
    INFO("Transform matrix: [" << spinner->transform[0] << ", " << spinner->transform[1]
         << ", " << spinner->transform[2] << ", " << spinner->transform[3] << "]");

    // Either inline_style has transform OR transform matrix is non-identity
    bool has_inline_transform = (it != spinner->inline_style.end());
    bool has_matrix_rotation = has_rotation(spinner->transform);

    CHECK((has_inline_transform || has_matrix_rotation));

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// interpolate_value Tests
// ============================================================================

TEST_CASE("interpolate_value for transform functions", "[animation][interpolate]") {
    SECTION("Interpolate scale") {
        std::string result = cssbox_utils::interpolate_value("scale(1)", "scale(1.5)", 0.5f);
        INFO("scale interpolation at 50%: " << result);
        CHECK(result.find("scale") != std::string::npos);
        CHECK(result.find("1.25") != std::string::npos);
    }

    SECTION("Interpolate rotate") {
        std::string result = cssbox_utils::interpolate_value("rotate(0deg)", "rotate(90deg)", 0.5f);
        INFO("rotate interpolation at 50%: " << result);
        CHECK(result.find("rotate") != std::string::npos);
        CHECK(result.find("45") != std::string::npos);
    }

    SECTION("Interpolate rotate 360") {
        std::string result = cssbox_utils::interpolate_value("rotate(0deg)", "rotate(360deg)", 0.25f);
        INFO("rotate interpolation at 25%: " << result);
        CHECK(result.find("rotate") != std::string::npos);
        CHECK(result.find("90") != std::string::npos);
    }
}

// ============================================================================
// Keyframe Parsing Tests
// ============================================================================

TEST_CASE("Keyframe animation is parsed and stored", "[animation][keyframes]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        @keyframes spin {
            from { transform: rotate(0deg); }
            to { transform: rotate(360deg); }
        }

        @keyframes pulse {
            0% { transform: scale(1); }
            50% { transform: scale(1.5); }
            100% { transform: scale(1); }
        }
    )";

    int result = cssboxParseCSS(renderer, css);
    REQUIRE(result == 1);

    // Check spin animation exists
    const KeyframeAnimation* spin = renderer->stylesheet->get_keyframe_animation("spin");
    INFO("spin animation: " << (spin ? "found" : "NOT FOUND"));
    REQUIRE(spin != nullptr);
    INFO("spin keyframes count: " << spin->keyframes.size());
    CHECK(spin->keyframes.size() == 2);

    // Check keyframe positions
    if (spin->keyframes.size() >= 2) {
        INFO("spin keyframe[0] position: " << spin->keyframes[0].position);
        INFO("spin keyframe[1] position: " << spin->keyframes[1].position);
        CHECK(spin->keyframes[0].position == 0.0f);
        CHECK(spin->keyframes[1].position == 1.0f);

        // Check properties
        auto from_it = spin->keyframes[0].properties.find("transform");
        auto to_it = spin->keyframes[1].properties.find("transform");
        if (from_it != spin->keyframes[0].properties.end()) {
            INFO("spin from transform: " << from_it->second);
        }
        if (to_it != spin->keyframes[1].properties.end()) {
            INFO("spin to transform: " << to_it->second);
        }
    }

    // Check pulse animation exists
    const KeyframeAnimation* pulse = renderer->stylesheet->get_keyframe_animation("pulse");
    INFO("pulse animation: " << (pulse ? "found" : "NOT FOUND"));
    REQUIRE(pulse != nullptr);
    INFO("pulse keyframes count: " << pulse->keyframes.size());
    CHECK(pulse->keyframes.size() == 3);

    // Check pulse keyframe values
    if (pulse->keyframes.size() >= 3) {
        INFO("pulse keyframe positions: " << pulse->keyframes[0].position
             << ", " << pulse->keyframes[1].position
             << ", " << pulse->keyframes[2].position);

        auto mid_it = pulse->keyframes[1].properties.find("transform");
        if (mid_it != pulse->keyframes[1].properties.end()) {
            INFO("pulse 50% transform: " << mid_it->second);
            CHECK(mid_it->second.find("scale") != std::string::npos);
            CHECK(mid_it->second.find("1.5") != std::string::npos);
        }
    }

    cssboxDeleteRenderer(renderer);
}

TEST_CASE("Keyframe get_properties_at interpolation", "[animation][keyframes]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        @keyframes pulse {
            0% { transform: scale(1); opacity: 1; }
            50% { transform: scale(1.5); opacity: 0.5; }
            100% { transform: scale(1); opacity: 1; }
        }
    )";

    cssboxParseCSS(renderer, css);

    const KeyframeAnimation* pulse = renderer->stylesheet->get_keyframe_animation("pulse");
    REQUIRE(pulse != nullptr);

    // Test at 0%
    auto props_0 = pulse->get_properties_at(0.0f);
    INFO("At 0%: transform=" << props_0["transform"] << " opacity=" << props_0["opacity"]);
    CHECK(props_0.find("transform") != props_0.end());
    CHECK(props_0["transform"].find("scale(1)") != std::string::npos);

    // Test at 50%
    auto props_50 = pulse->get_properties_at(0.5f);
    INFO("At 50%: transform=" << props_50["transform"] << " opacity=" << props_50["opacity"]);
    CHECK(props_50.find("transform") != props_50.end());
    CHECK(props_50["transform"].find("scale(1.5)") != std::string::npos);

    // Test at 25% (should interpolate between 0% and 50%)
    auto props_25 = pulse->get_properties_at(0.25f);
    INFO("At 25%: transform=" << props_25["transform"]);
    // Should be between scale(1) and scale(1.5), so ~scale(1.25)
    CHECK(props_25.find("transform") != props_25.end());

    cssboxDeleteRenderer(renderer);
}

// ============================================================================
// Debug Test - Print detailed animation state
// ============================================================================

TEST_CASE("Debug: Animation state inspection", "[animation][debug]") {
    cssboxRenderer* renderer = cssboxCreateRenderer(nullptr);
    cssboxSetViewport(renderer, 800, 600);

    const char* css = R"(
        @keyframes spin {
            from { transform: rotate(0deg); }
            to { transform: rotate(360deg); }
        }

        #spinner {
            width: 50px;
            height: 50px;
            animation: spin 1s linear infinite;
        }
    )";

    cssboxParseCSS(renderer, css);
    cssboxElement* spinner = cssboxCreateElement(renderer, "spinner", "div");

    INFO("=== Initial State ===");
    INFO("Element ID: " << spinner->id);
    INFO("Element internal_id: " << spinner->internal_id);

    cssboxComputeLayout(renderer);

    INFO("=== After Layout ===");
    INFO("style.animations.size(): " << spinner->style.animations.size());
    for (const auto& anim : spinner->style.animations) {
        INFO("  Animation: name=" << anim.name << " duration=" << anim.duration);
    }

    cssboxUpdate(renderer, 0.0f);

    INFO("=== After First Update ===");
    INFO("animation_state: " << (spinner->animation_state ? "created" : "null"));
    if (spinner->animation_state) {
        AnimationState* state = static_cast<AnimationState*>(spinner->animation_state);
        INFO("running_animations.size(): " << state->running_animations.size());
        for (const auto& anim : state->running_animations) {
            INFO("  Running: name=" << anim.animation_name << " active=" << anim.active);
        }
    }
    INFO("animated_elements_.size(): " << renderer->animated_elements_.size());
    INFO("animated_elements_ contains spinner: " << (renderer->animated_elements_.count(spinner->internal_id) > 0));

    // Update to 0.25s
    cssboxUpdate(renderer, 0.25f);

    INFO("=== After 0.25s ===");
    INFO("Transform: [" << spinner->transform[0] << ", " << spinner->transform[1]
         << ", " << spinner->transform[2] << ", " << spinner->transform[3]
         << ", " << spinner->transform[4] << ", " << spinner->transform[5] << "]");

    auto it = spinner->inline_style.find("transform");
    if (it != spinner->inline_style.end()) {
        INFO("inline_style[transform]: " << it->second);
    } else {
        INFO("inline_style[transform]: NOT SET");
    }

    // This test always passes - it's for debugging output
    CHECK(true);

    cssboxDeleteRenderer(renderer);
}
