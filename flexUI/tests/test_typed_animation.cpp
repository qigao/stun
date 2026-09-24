#include <flexUI/detail/style_property_registry.h>
#include <flexUI/transition.h>

#include <flex/core/cmeta_types.h>
#include <tinytest.hpp>

using namespace flexUI;
using namespace flexUI::detail;

namespace {

AnimationDef linear_animation(float duration_ms = 100.0f) {
    AnimationDef def;
    def.duration_ms = duration_ms;
    def.easing = EasingType::Linear;
    return def;
}

std::vector<AnimationValuePoint> opacity_points() {
    return {{0.0f, 0.0f}, {1.0f, 1.0f}};
}

} // namespace

suite("FlexUI typed AnimationManager core") {
    it("interpolates opacity by descriptor identity") {
        AnimationManager manager;
        const auto* opacity = style_property_descriptor(StylePropertyId::Opacity);
        check_not_null(opacity);
        if (!opacity) return;

        check_true(manager.start_float(
            1, *opacity, opacity_points(), linear_animation(), 0.0f));
        check_true(manager.has_effect(1, 50.0f));
        check_float_eq(manager.get_float(1, *opacity, -1.0f, 50.0f),
                       0.5f, 0.0001f);
        check_float_eq(manager.get(1, "opacity", -1.0f, 50.0f),
                       0.5f, 0.0001f);
    }

    it("preserves delay and fill modes") {
        AnimationManager manager;
        const auto* opacity = style_property_descriptor(StylePropertyId::Opacity);
        check_not_null(opacity);
        if (!opacity) return;

        AnimationDef def = linear_animation();
        def.delay_ms = 25.0f;
        def.fill_mode = AnimationFillMode::Both;
        check_true(manager.start_float(2, *opacity, opacity_points(), def, 0.0f));

        check_float_eq(manager.get_float(2, *opacity, 0.25f, 10.0f),
                       0.0f, 0.0001f);
        check_float_eq(manager.get_float(2, *opacity, 0.25f, 75.0f),
                       0.5f, 0.0001f);
        check_float_eq(manager.get_float(2, *opacity, 0.25f, 150.0f),
                       1.0f, 0.0001f);
        manager.update(150.0f);
        check_true(manager.has_effect(2, 150.0f));
    }

    it("preserves alternate direction and pause resume") {
        AnimationManager manager;
        const auto* opacity = style_property_descriptor(StylePropertyId::Opacity);
        check_not_null(opacity);
        if (!opacity) return;

        AnimationDef def = linear_animation();
        def.iteration_count = 2.0f;
        def.direction = AnimationDirection::Alternate;
        check_true(manager.start_float(3, *opacity, opacity_points(), def, 0.0f));

        check_float_eq(manager.get_float(3, *opacity, -1.0f, 50.0f),
                       0.5f, 0.0001f);
        check_float_eq(manager.get_float(3, *opacity, -1.0f, 150.0f),
                       0.5f, 0.0001f);

        manager.set_play_state(3, AnimationPlayState::Paused, 60.0f);
        check_float_eq(manager.get_float(3, *opacity, -1.0f, 90.0f),
                       0.6f, 0.0001f);
        manager.set_play_state(3, AnimationPlayState::Running, 90.0f);
        check_float_eq(manager.get_float(3, *opacity, -1.0f, 100.0f),
                       0.7f, 0.0001f);
    }

    it("rejects mixed type and unsorted typed keyframes") {
        AnimationManager manager;
        const auto* opacity = style_property_descriptor(StylePropertyId::Opacity);
        check_not_null(opacity);
        if (!opacity) return;

        std::vector<TypedAnimationPoint> mixed = {
            {0.0f, {&cmeta_type_float, flex::AnimValue{0.0f}}},
            {1.0f, {&flex::cmeta_type_color,
                    flex::AnimValue{Color{1.0f, 1.0f, 1.0f, 1.0f}}}},
        };
        check_false(manager.start_typed(
            4, *opacity, mixed, linear_animation(), 0.0f));

        std::vector<TypedAnimationPoint> unsorted = {
            {1.0f, {&cmeta_type_float, flex::AnimValue{1.0f}}},
            {0.0f, {&cmeta_type_float, flex::AnimValue{0.0f}}},
        };
        check_false(manager.start_typed(
            4, *opacity, unsorted, linear_animation(), 0.0f));
        check_false(manager.has_any_effects());
    }

    it("interpolates a whole Color keyframe track including alpha") {
        AnimationManager manager;
        const auto* background =
            style_property_descriptor(StylePropertyId::BackgroundColor);
        check_not_null(background);
        if (!background) return;

        const std::vector<TypedAnimationPoint> points = {
            {0.0f, {&flex::cmeta_type_color,
                    flex::AnimValue{Color{0.2f, 0.4f, 0.6f, 1.0f}}}},
            {1.0f, {&flex::cmeta_type_color,
                    flex::AnimValue{Color{0.6f, 0.8f, 0.2f, 0.5f}}}},
        };
        check_true(manager.start_typed(
            41, *background, points, linear_animation(), 0.0f));

        const Color mid = manager.get_color(
            41, *background, Color{}, 50.0f);
        check_float_eq(mid.r, 0.4f, 0.0001f);
        check_float_eq(mid.g, 0.6f, 0.0001f);
        check_float_eq(mid.b, 0.4f, 0.0001f);
        check_float_eq(mid.a, 0.75f, 0.0001f);

        // Historical component pseudo-keys are not runtime identities for
        // typed Color tracks.
        check_float_eq(
            manager.get(41, "background-color-r", 0.125f, 50.0f),
            0.125f, 0.0001f);
    }

    it("coexists with legacy non migrated animation tracks") {
        AnimationManager manager;
        const auto* opacity = style_property_descriptor(StylePropertyId::Opacity);
        check_not_null(opacity);
        if (!opacity) return;

        check_true(manager.start_float(
            5, *opacity, opacity_points(), linear_animation(), 0.0f));

        std::vector<AnimationValuePoint> legacy = {
            {0.0f, 10.0f}, {1.0f, 20.0f}};
        manager.start(5, "transform-x", legacy, linear_animation(), 0.0f);

        check_float_eq(manager.get_float(5, *opacity, -1.0f, 50.0f),
                       0.5f, 0.0001f);
        check_float_eq(manager.get(5, "transform-x", -1.0f, 50.0f),
                       15.0f, 0.0001f);

        manager.clear_element(5);
        check_false(manager.has_any_effects());
    }
};
