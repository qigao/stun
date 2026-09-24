#include <flexUI/detail/style_property_registry.h>
#include <flexUI/element.h>

#include <flex/core/cmeta_types.h>
#include <tinytest.hpp>

using namespace flexUI;
using namespace flexUI::detail;

suite("FlexUI CMeta style property registry") {
    it("resolves canonical names and migration aliases") {
        const auto* opacity = style_property_find("opacity");
        const auto* visibility = style_property_find("visibility");
        const auto* color = style_property_find("color");
        const auto* text_color = style_property_find("text-color");
        const auto* font_size = style_property_find("font-size");
        const auto* font_size_alias = style_property_find("fontSize");

        check_not_null(opacity);
        check_not_null(visibility);
        check_true(cmeta_type_desc_valid(visibility->type));
        check(visibility->type->kind == CMETA_T_INTEGER);
        check_false(cmeta_type_equal(visibility->type, &cmeta_type_int));
        check_true((visibility->flags & STYLE_PROPERTY_INHERITED) != 0u);
        check_true((visibility->flags & STYLE_PROPERTY_DISCRETE) != 0u);
        check_false(
            (visibility->flags & STYLE_PROPERTY_TRANSITIONABLE) != 0u);
        check_true((visibility->impact & STYLE_IMPACT_PAINT) != 0u);
        check_true((visibility->impact & STYLE_IMPACT_HIT_TEST) != 0u);
        check_not_null(color);
        check(color == text_color);
        check(font_size == font_size_alias);
        check_null(style_property_find("does-not-exist"));

        check_true(cmeta_type_equal(opacity->type, &cmeta_type_float));
        check_true(cmeta_type_equal(color->type, &flex::cmeta_type_color));
    }

    it("publishes transition groups and dirty impact metadata") {
        const auto* tx = style_property_find("transform-x");
        const auto* shadow = style_property_find("box-shadow-color");
        const auto* font_size = style_property_find("font-size");
        const auto* opacity = style_property_find("opacity");

        check_not_null(tx);
        check_str_eq(tx->transition_group, "transform");
        check_true((tx->impact & STYLE_IMPACT_HIT_TEST) != 0u);

        check_not_null(shadow);
        check_str_eq(shadow->transition_group, "box-shadow");

        check_true((font_size->impact & STYLE_IMPACT_TEXT_LAYOUT) != 0u);
        check_true((font_size->impact & STYLE_IMPACT_LAYOUT) != 0u);
        check_true((font_size->flags & STYLE_PROPERTY_INHERITED) != 0u);

        check_true((opacity->impact & STYLE_IMPACT_COMPOSITE) != 0u);
        check_true((opacity->flags & STYLE_PROPERTY_TRANSITIONABLE) != 0u);
    }

    it("reads and writes through CMeta-validated accessors") {
        ComputedStyle style;
        style.opacity = 0.25f;
        style.background_color = Color{0.1f, 0.2f, 0.3f, 1.0f};

        const auto* opacity = style_property_find("opacity");
        const auto* background = style_property_find("background-color");

        float read_opacity = 0.0f;
        check_true(style_property_read(
            *opacity, style, &cmeta_type_float, &read_opacity));
        check_float_eq(read_opacity, 0.25f, 0.0001f);

        const float new_opacity = 0.75f;
        check_true(style_property_write(
            *opacity, style, &cmeta_type_float, &new_opacity));
        check_float_eq(style.opacity, 0.75f, 0.0001f);

        Color read_color{};
        check_true(style_property_read(
            *background, style, &flex::cmeta_type_color, &read_color));
        check_float_eq(read_color.g, 0.2f, 0.0001f);

        const Color new_color{0.8f, 0.7f, 0.6f, 1.0f};
        check_true(style_property_write(
            *background, style, &flex::cmeta_type_color, &new_color));
        check_float_eq(style.background_color.r, 0.8f, 0.0001f);

        check_false(style_property_write(
            *background, style, &cmeta_type_float, &new_opacity));

        const auto* visibility = style_property_find("visibility");
        const Visibility hidden = Visibility::Hidden;
        check_true(style_property_write(
            *visibility, style, visibility->type, &hidden));
        check(style.visibility == Visibility::Hidden);

        Visibility read_visibility = Visibility::Visible;
        check_true(style_property_read(
            *visibility, style, visibility->type, &read_visibility));
        check(read_visibility == Visibility::Hidden);

        check_false(style_property_write(
            *visibility, style, &cmeta_type_int, &hidden));
    }

    it("maps style impact metadata onto element dirty stages") {
        Element element;
        element.clear_dirty();

        const auto* opacity = style_property_find("opacity");
        const auto* transform = style_property_find("transform-x");
        const auto* font_size = style_property_find("font-size");
        check_not_null(opacity);
        check_not_null(transform);
        check_not_null(font_size);

        mark_style_property_dirty(element, *opacity);
        check_true(element.is_dirty(flex::DirtyFlags::Visual));
        check_false(element.is_dirty(flex::DirtyFlags::Layout));

        element.clear_dirty();
        mark_style_property_dirty(element, *transform);
        check_true(element.is_dirty(flex::DirtyFlags::Visual));
        check_true(element.is_dirty(flex::DirtyFlags::Bounds));
        check_true(element.is_dirty(flex::DirtyFlags::WorldBounds));
        check_false(element.is_dirty(flex::DirtyFlags::Layout));

        element.clear_dirty();
        mark_style_property_dirty(element, *font_size);
        check_true(element.is_dirty(flex::DirtyFlags::Content));
        check_true(element.is_dirty(flex::DirtyFlags::Bounds));
        check_true(element.is_dirty(flex::DirtyFlags::Layout));
        check_true(element.is_dirty(flex::DirtyFlags::Visual));
    }

    it("matches the legacy transition selector surface through registry metadata") {
        const auto* opacity = style_property_find("opacity");
        const auto* background = style_property_find("background-color");
        const auto* transform_x = style_property_find("transform-x");
        const auto* shadow_blur = style_property_find("box-shadow-blur");
        const auto* font_size = style_property_find("font-size");
        const auto* color = style_property_find("color");
        const auto* visibility = style_property_find("visibility");

        check_not_null(opacity);
        check_not_null(background);
        check_not_null(transform_x);
        check_not_null(shadow_blur);
        check_not_null(font_size);
        check_not_null(color);
        check_not_null(visibility);

        check_true(style_property_matches_transition(*opacity, "opacity"));
        check_true(style_property_matches_transition(*opacity, "all"));
        check_true(style_property_matches_transition(*background, "background-color"));

        check_true(style_property_matches_transition(*transform_x, "transform"));
        check_true(style_property_matches_transition(*transform_x, "all"));
        check_false(style_property_matches_transition(*transform_x, "transform-x"));

        check_true(style_property_matches_transition(*shadow_blur, "box-shadow"));
        check_true(style_property_matches_transition(*shadow_blur, "all"));
        check_false(style_property_matches_transition(*shadow_blur, "box-shadow-blur"));

        check_true((font_size->flags & STYLE_PROPERTY_ANIMATABLE) != 0u);
        check_false((font_size->flags & STYLE_PROPERTY_TRANSITIONABLE) != 0u);
        check_false(style_property_matches_transition(*font_size, "all"));

        check_true((color->flags & STYLE_PROPERTY_ANIMATABLE) != 0u);
        check_false((color->flags & STYLE_PROPERTY_TRANSITIONABLE) != 0u);
        check_false(style_property_matches_transition(*color, "all"));

        check_false(style_property_matches_transition(*visibility, "all"));
    }

    it("covers the current stable transition property subset") {
        check(style_property_count() >= static_cast<std::size_t>(24));
        check_not_null(style_property_find("border-color"));
        check_not_null(style_property_find("outline-width"));
        check_not_null(style_property_find("ring-offset-color"));
        check_not_null(style_property_find("transform-scale-y"));
        check_not_null(style_property_find("box-shadow-spread"));
    }
};
