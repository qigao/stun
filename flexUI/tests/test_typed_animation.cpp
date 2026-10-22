#include <flexUI/detail/style_property_registry.h>
#include <flexUI/transition.h>

#include <flex/core/cmeta_types.h>
#include <tinytest.hpp>

#include "typed_value_test_support.h"

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
    it("interpolates peer Color keyframes through a canonical property query") {
        const auto* background = style_property_find("background-color");
        check_not_null(background);
        const auto* peer = flex_test_peer_color_type();
        check_not_equal(peer, background->type);
        auto property = *background;
        property.type = peer;
        AnimationManager manager;
        const std::vector<TypedAnimationPoint> points = {
            {0.0f, {peer, flex::AnimValue{Color{1, 0, 0, 0.25f}}}},
            {1.0f, {peer, flex::AnimValue{Color{0, 0, 1, 0.75f}}}},
        };
        check_true(manager.start_typed(1, property, points, linear_animation(), 0.0f));
        const auto sampled = manager.get_color(1, *background, Color{}, 50.0f);
        check_within(sampled.r, 0.5f, 0.0001f);
        check_within(sampled.b, 0.5f, 0.0001f);
        check_within(sampled.a, 0.5f, 0.0001f);
    }

    it("rejects malformed property layouts without replacing an active animation") {
        const auto* opacity = style_property_find("opacity");
        check_not_null(opacity);
        AnimationManager manager;
        check_true(manager.start_float(
            1, *opacity, opacity_points(), linear_animation(), 0.0f));
        for (const auto& type : incompatible_native_types(*opacity->type)) {
            auto property = *opacity;
            property.type = &type;
            check_false(manager.start_float(
                1, property, opacity_points(), linear_animation(), 25.0f));
            check_within(manager.get_float(1, property, -1.0f, 50.0f),
                         -1.0f, 0.0001f);
            check_within(manager.get_float(1, *opacity, -1.0f, 50.0f),
                         0.5f, 0.0001f);
        }
    }

    it("rejects malformed keyframe layouts without replacing an active animation") {
        const auto* opacity = style_property_find("opacity");
        check_not_null(opacity);
        AnimationManager manager;
        check_true(manager.start_float(
            1, *opacity, opacity_points(), linear_animation(), 0.0f));
        for (const auto& type : incompatible_native_types(*opacity->type)) {
            const std::vector<TypedAnimationPoint> points = {
                {0.0f, {opacity->type, flex::AnimValue{0.0f}}},
                {1.0f, {&type, flex::AnimValue{1.0f}}},
            };
            check_false(manager.start_typed(
                1, *opacity, points, linear_animation(), 25.0f));
            check_within(manager.get_float(1, *opacity, -1.0f, 50.0f),
                         0.5f, 0.0001f);
        }
    }

    it("interpolates opacity by descriptor identity") {
        AnimationManager manager;
        const auto* opacity = style_property_descriptor(StylePropertyId::Opacity);
        check_not_null(opacity);
        if (!opacity) return;

        check_true(manager.start_float(
            1, *opacity, opacity_points(), linear_animation(), 0.0f));
        check_true(manager.has_effect(1, 50.0f));
        check_within(manager.get_float(1, *opacity, -1.0f, 50.0f),
                       0.5f, 0.0001f);
        check_within(manager.get(1, "opacity", -1.0f, 50.0f),
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

        check_within(manager.get_float(2, *opacity, 0.25f, 10.0f),
                       0.0f, 0.0001f);
        check_within(manager.get_float(2, *opacity, 0.25f, 75.0f),
                       0.5f, 0.0001f);
        check_within(manager.get_float(2, *opacity, 0.25f, 150.0f),
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

        check_within(manager.get_float(3, *opacity, -1.0f, 50.0f),
                       0.5f, 0.0001f);
        check_within(manager.get_float(3, *opacity, -1.0f, 150.0f),
                       0.5f, 0.0001f);

        manager.set_play_state(3, AnimationPlayState::Paused, 60.0f);
        check_within(manager.get_float(3, *opacity, -1.0f, 90.0f),
                       0.6f, 0.0001f);
        manager.set_play_state(3, AnimationPlayState::Running, 90.0f);
        check_within(manager.get_float(3, *opacity, -1.0f, 100.0f),
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

    it("interpolates whole Color values including alpha") {
        AnimationManager manager;
        const auto* background =
            style_property_descriptor(StylePropertyId::BackgroundColor);
        check_not_null(background);
        if (!background) return;

        std::vector<TypedAnimationPoint> colors = {
            {0.0f, {&flex::cmeta_type_color,
                    flex::AnimValue{Color{1.0f, 0.0f, 0.0f, 0.25f}}}},
            {1.0f, {&flex::cmeta_type_color,
                    flex::AnimValue{Color{0.0f, 0.0f, 1.0f, 0.75f}}}},
        };
        check_true(manager.start_typed(
            5, *background, colors, linear_animation(), 0.0f));

        const Color midpoint = manager.get_color(
            5, *background, Color{}, 50.0f);
        check_within(midpoint.r, 0.5f, 0.0001f);
        check_within(midpoint.g, 0.0f, 0.0001f);
        check_within(midpoint.b, 0.5f, 0.0001f);
        check_within(midpoint.a, 0.5f, 0.0001f);

        check_within(
            manager.get_float(5, *background, 0.375f, 50.0f),
            0.375f, 0.0001f);
    }

    it("keeps reflected transform component tracks independent") {
        AnimationManager manager;
        const auto* tx =
            style_property_descriptor(StylePropertyId::TransformX);
        const auto* rotate =
            style_property_descriptor(StylePropertyId::TransformRotate);
        check_not_null(tx);
        check_not_null(rotate);
        if (!tx || !rotate) return;

        const std::vector<AnimationValuePoint> x_points = {
            {0.0f, 0.0f}, {1.0f, 20.0f}};
        const std::vector<AnimationValuePoint> rotate_points = {
            {0.0f, 0.0f}, {1.0f, 90.0f}};

        check_true(manager.start_float(
            51, *tx, x_points, linear_animation(), 0.0f));
        check_true(manager.start_float(
            51, *rotate, rotate_points, linear_animation(), 0.0f));

        check_within(
            manager.get_float(51, *tx, -1.0f, 50.0f),
            10.0f, 0.0001f);
        check_within(
            manager.get_float(51, *rotate, -1.0f, 50.0f),
            45.0f, 0.0001f);

        check_within(
            manager.get(51, "transform-x", -1.0f, 50.0f),
            10.0f, 0.0001f);
    }

    it("coexists with legacy non migrated animation tracks") {
        AnimationManager manager;
        const auto* opacity = style_property_descriptor(StylePropertyId::Opacity);
        check_not_null(opacity);
        if (!opacity) return;

        check_true(manager.start_float(
            6, *opacity, opacity_points(), linear_animation(), 0.0f));

        std::vector<AnimationValuePoint> legacy = {
            {0.0f, 10.0f}, {1.0f, 20.0f}};
        manager.start(6, "transform-x", legacy, linear_animation(), 0.0f);

        check_within(manager.get_float(6, *opacity, -1.0f, 50.0f),
                       0.5f, 0.0001f);
        check_within(manager.get(6, "transform-x", -1.0f, 50.0f),
                       15.0f, 0.0001f);

        manager.clear_element(6);
        check_false(manager.has_any_effects());
    }
};
