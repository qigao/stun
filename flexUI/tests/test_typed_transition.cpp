#include <flexUI/detail/style_property_registry.h>
#include <flexUI/transition.h>

#include <tinytest.hpp>

using namespace flexUI;
using namespace flexUI::detail;

suite("FlexUI typed TransitionManager core") {
    it("interpolates opacity through a descriptor-keyed float transition") {
        TransitionManager manager;
        const auto* opacity = style_property_find("opacity");
        check_not_null(opacity);

        TransitionDef def;
        def.duration_ms = 100.0f;
        def.delay_ms = 0.0f;
        def.easing = EasingType::Linear;

        check_true(manager.start_float(
            1, *opacity, 0.0f, 1.0f, def, 0.0f));
        check_true(manager.has_any_active());
        check_true(manager.has_active(1, 50.0f));
        check_float_eq(
            manager.get_float(1, *opacity, -1.0f, 50.0f),
            0.5f, 0.0001f);

        manager.update(101.0f);
        check_false(manager.has_active(1, 101.0f));
        check_false(manager.has_any_active());
    }

    it("restarts from the current interpolated opacity") {
        TransitionManager manager;
        const auto* opacity = style_property_find("opacity");
        check_not_null(opacity);

        TransitionDef def;
        def.duration_ms = 100.0f;
        def.easing = EasingType::Linear;

        check_true(manager.start_float(
            7, *opacity, 0.0f, 1.0f, def, 0.0f));
        check_float_eq(
            manager.get_float(7, *opacity, -1.0f, 50.0f),
            0.5f, 0.0001f);

        check_true(manager.start_float(
            7, *opacity, 0.0f, 0.75f, def, 50.0f));
        check_float_eq(
            manager.get_float(7, *opacity, -1.0f, 100.0f),
            0.625f, 0.0001f);
    }

    it("preserves delay and cubic bezier timing") {
        TransitionManager manager;
        const auto* opacity = style_property_find("opacity");
        check_not_null(opacity);

        TransitionDef delayed;
        delayed.duration_ms = 100.0f;
        delayed.delay_ms = 25.0f;
        delayed.easing = EasingType::Linear;
        check_true(manager.start_float(
            2, *opacity, 0.0f, 1.0f, delayed, 0.0f));
        check_float_eq(
            manager.get_float(2, *opacity, -1.0f, 10.0f),
            0.0f, 0.0001f);
        check_float_eq(
            manager.get_float(2, *opacity, -1.0f, 75.0f),
            0.5f, 0.0001f);

        TransitionDef bezier;
        bezier.duration_ms = 100.0f;
        bezier.easing = EasingType::CubicBezier;
        bezier.bezier[0] = 0.0f;
        bezier.bezier[1] = 0.0f;
        bezier.bezier[2] = 1.0f;
        bezier.bezier[3] = 1.0f;
        check_true(manager.start_float(
            3, *opacity, 0.0f, 1.0f, bezier, 0.0f));
        check_float_eq(
            manager.get_float(3, *opacity, -1.0f, 50.0f),
            0.5f, 0.002f);
    }

    it("rejects descriptor type mismatch and isolates element keys") {
        TransitionManager manager;
        const auto* opacity = style_property_find("opacity");
        const auto* background = style_property_find("background-color");
        check_not_null(opacity);
        check_not_null(background);

        TransitionDef def;
        def.duration_ms = 100.0f;
        def.easing = EasingType::Linear;

        check_false(manager.start_float(
            1, *background, 0.0f, 1.0f, def, 0.0f));
        check_true(manager.start_float(
            1, *opacity, 0.0f, 1.0f, def, 0.0f));

        check_float_eq(
            manager.get_float(2, *opacity, 0.25f, 50.0f),
            0.25f, 0.0001f);
        check_float_eq(
            manager.get_float(1, *opacity, 0.25f, 50.0f),
            0.5f, 0.0001f);
    }

    it("interpolates a whole Color including alpha") {
        TransitionManager manager;
        const auto* background = style_property_find("background-color");
        check_not_null(background);

        TransitionDef def;
        def.duration_ms = 100.0f;
        def.easing = EasingType::Linear;

        const Color from{0.2f, 0.4f, 0.6f, 1.0f};
        const Color to{0.6f, 0.8f, 0.2f, 0.5f};
        check_true(manager.start_color(
            9, *background, from, to, def, 0.0f));

        const Color mid =
            manager.get_color(9, *background, Color{}, 50.0f);
        check_float_eq(mid.r, 0.4f, 0.0001f);
        check_float_eq(mid.g, 0.6f, 0.0001f);
        check_float_eq(mid.b, 0.4f, 0.0001f);
        check_float_eq(mid.a, 0.75f, 0.0001f);
    }

    it("unions reflected impacts for active transitions") {
        TransitionManager manager;
        const auto* opacity = style_property_find("opacity");
        const auto* transform = style_property_find("transform-x");
        check_not_null(opacity);
        check_not_null(transform);

        TransitionDef def;
        def.duration_ms = 100.0f;
        def.easing = EasingType::Linear;

        check_true(manager.start_float(
            42, *opacity, 0.0f, 1.0f, def, 0.0f));
        check_true(manager.start_float(
            42, *transform, 0.0f, 10.0f, def, 0.0f));

        const std::uint32_t impact = manager.active_impact(42, 25.0f);
        check_true((impact & STYLE_IMPACT_PAINT) != 0u);
        check_true((impact & STYLE_IMPACT_COMPOSITE) != 0u);
        check_true((impact & STYLE_IMPACT_HIT_TEST) != 0u);
        check( manager.active_impact(7, 25.0f) == STYLE_IMPACT_NONE );
    }

    it("exposes typed keyframe property impact without changing legacy storage") {
        AnimationManager manager;
        const auto* opacity = style_property_find("opacity");
        check_not_null(opacity);

        AnimationDef def;
        def.duration_ms = 100.0f;
        def.easing = EasingType::Linear;

        std::vector<AnimationValuePoint> points = {
            {0.0f, 0.0f},
            {1.0f, 1.0f},
        };
        check_true(manager.start_float(
            77, *opacity, points, def, 0.0f));

        const std::uint32_t impact = manager.active_impact(77, 50.0f);
        check_true((impact & STYLE_IMPACT_PAINT) != 0u);
        check_true((impact & STYLE_IMPACT_COMPOSITE) != 0u);
        check(manager.active_impact(78, 50.0f) == STYLE_IMPACT_NONE);
    }

    it("clears typed transitions by element without touching other elements") {
        TransitionManager manager;
        const auto* opacity = style_property_find("opacity");
        check_not_null(opacity);

        TransitionDef def;
        def.duration_ms = 100.0f;
        def.easing = EasingType::Linear;

        check_true(manager.start_float(
            1, *opacity, 0.0f, 1.0f, def, 0.0f));
        check_true(manager.start_float(
            2, *opacity, 0.0f, 1.0f, def, 0.0f));

        manager.clear_element(1);
        check_false(manager.has_active(1, 25.0f));
        check_true(manager.has_active(2, 25.0f));

        manager.clear();
        check_false(manager.has_any_active());
    }
};
