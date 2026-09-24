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
