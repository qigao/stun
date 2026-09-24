#include <flexUI/detail/css_typed_value.h>

#include <flex/core/cmeta_types.h>
#include <tinytest.hpp>

#include <variant>

using namespace flexUI;
using namespace flexUI::detail;

suite("FlexUI typed CSS declaration literals") {
    it("compiles context-independent opacity once") {
        const auto* opacity = style_property_find("opacity");
        const auto compiled = compile_css_literal(opacity, " 0.625 ");

        check_true(compiled.has_value());
        check_true(cmeta_type_equal(compiled.type, &cmeta_type_float));
        check_true(std::holds_alternative<float>(compiled.value));
        check_float_eq(std::get<float>(compiled.value), 0.625f, 0.0001f);
    }

    it("keeps deferred opacity values on the legacy path") {
        const auto* opacity = style_property_find("opacity");

        check_false(compile_css_literal(opacity, "var(--opacity)").has_value());
        check_false(compile_css_literal(opacity, "calc(1 - 0.2)").has_value());
        check_false(compile_css_literal(opacity, "50%").has_value());
        check_false(compile_css_literal(opacity, "0.5junk").has_value());
    }

    it("compiles the visibility discrete enum with CMeta identity") {
        const auto* visibility = style_property_find("visibility");
        check_not_null(visibility);
        check_true(cmeta_type_desc_valid(visibility->type));

        const auto compiled = compile_css_literal(visibility, "hidden");
        check_true(compiled.has_value());
        check_true(cmeta_type_equal(compiled.type, visibility->type));
        check_true(std::holds_alternative<Visibility>(compiled.value));
        check(std::get<Visibility>(compiled.value) == Visibility::Hidden);

        check_false(compile_css_literal(visibility, "inherit").has_value());
        check_false(
            compile_css_literal(visibility, "var(--visibility)").has_value());
    }

    it("compiles safe context-independent color literals") {
        const auto* background = style_property_find("background-color");
        const auto* outline = style_property_find("outline-color");
        const auto* ring = style_property_find("ring-color");
        const auto* ring_offset = style_property_find("ring-offset-color");

        for (const auto* property : {background, outline, ring, ring_offset}) {
            check_not_null(property);
        }

        const auto hex = compile_css_literal(background, "#336699");
        const auto rgb = compile_css_literal(outline, "rgb(255 0 128 / 50%)");
        const auto hsl = compile_css_literal(ring, "hsl(120 100% 25%)");
        const auto oklch =
            compile_css_literal(ring_offset, "oklch(62% 0.1 250)");
        const auto mixed = compile_css_literal(
            background, "color-mix(in srgb, #000000 25%, #ffffff)");

        for (const auto* compiled : {&hex, &rgb, &hsl, &oklch, &mixed}) {
            check_true(compiled->has_value());
            check_true(cmeta_type_equal(compiled->type,
                                        &flex::cmeta_type_color));
            check_true(std::holds_alternative<Color>(compiled->value));
        }

        const auto color = std::get<Color>(hex.value);
        check_float_eq(color.r, 0.2f, 0.001f);
        check_float_eq(color.g, 0.4f, 0.001f);
        check_float_eq(color.b, 0.6f, 0.001f);
    }

    it("compiles context-independent effect lengths") {
        const auto* outline_width = style_property_find("outline-width");
        const auto* outline_offset = style_property_find("outline-offset");
        const auto* ring_width = style_property_find("ring-width");
        const auto* ring_offset = style_property_find("ring-offset");

        const auto thin = compile_css_literal(outline_width, "thin");
        const auto px = compile_css_literal(outline_offset, "2px");
        const auto ring = compile_css_literal(ring_width, "3.5");
        const auto offset = compile_css_literal(ring_offset, "-1px");

        for (const auto* compiled : {&thin, &px, &ring, &offset}) {
            check_true(compiled->has_value());
            check_true(cmeta_type_equal(compiled->type, &cmeta_type_float));
            check_true(std::holds_alternative<float>(compiled->value));
        }
        check_float_eq(std::get<float>(thin.value), 1.0f, 0.0001f);
        check_float_eq(std::get<float>(px.value), 2.0f, 0.0001f);
    }

    it("defers context-dependent effect lengths") {
        const auto* outline = style_property_find("outline-offset");
        const auto* ring = style_property_find("ring-width");

        check_false(compile_css_literal(outline, "10%").has_value());
        check_false(compile_css_literal(outline, "2rem").has_value());
        check_false(compile_css_literal(ring, "10vw").has_value());
        check_false(
            compile_css_literal(ring, "calc(1px + 2px)").has_value());
        check_false(
            compile_css_literal(ring, "var(--ring-width)").has_value());
    }

    it("compiles individual transforms into bounded typed writes") {
        const auto translate =
            compile_css_property_writes("translate", "12px -4px");
        check(translate.size() == static_cast<std::size_t>(2));
        check(translate[0].property->id == StylePropertyId::TransformX);
        check(translate[1].property->id == StylePropertyId::TransformY);
        check_float_eq(std::get<float>(translate[0].value.value),
                       12.0f, 0.0001f);
        check_float_eq(std::get<float>(translate[1].value.value),
                       -4.0f, 0.0001f);

        const auto uniform = compile_css_property_writes("scale", "2");
        check(uniform.size() == static_cast<std::size_t>(3));
        check(uniform[0].property->id == StylePropertyId::TransformScale);
        check_float_eq(std::get<float>(uniform[0].value.value),
                       2.0f, 0.0001f);

        const auto non_uniform =
            compile_css_property_writes("scale", "2 3");
        check(non_uniform.size() == static_cast<std::size_t>(3));
        check_float_eq(std::get<float>(non_uniform[0].value.value),
                       1.0f, 0.0001f);
        check_float_eq(std::get<float>(non_uniform[1].value.value),
                       2.0f, 0.0001f);
        check_float_eq(std::get<float>(non_uniform[2].value.value),
                       3.0f, 0.0001f);

        const auto rotate =
            compile_css_property_writes("rotate", ".25turn");
        check(rotate.size() == static_cast<std::size_t>(1));
        check(rotate[0].property->id == StylePropertyId::TransformRotate);
        check_float_eq(std::get<float>(rotate[0].value.value),
                       90.0f, 0.0001f);
    }

    it("keeps dependent and compound transforms on fallback") {
        check_true(
            compile_css_property_writes("translate", "50% 2px").empty());
        check_true(
            compile_css_property_writes("translate", "var(--x) 2px").empty());
        check_true(
            compile_css_property_writes("scale", "calc(1 + .5)").empty());
        check_true(
            compile_css_property_writes("rotate", "var(--angle)").empty());
        check_true(
            compile_css_property_writes(
                "transform", "translate(1px, 2px) scale(2)")
                .empty());
    }

    it("keeps dependent and unsafe color properties on the fallback path") {
        const auto* background = style_property_find("background-color");
        const auto* border = style_property_find("border-color");
        const auto* text = style_property_find("color");
        const auto* shadow = style_property_find("box-shadow-color");
        const auto* font_size = style_property_find("font-size");

        check_false(
            compile_css_literal(background, "var(--surface)").has_value());
        check_false(
            compile_css_literal(background, "currentColor").has_value());
        check_false(compile_css_literal(border, "#336699").has_value());
        check_false(compile_css_literal(text, "#336699").has_value());
        check_false(compile_css_literal(shadow, "#336699").has_value());
        check_false(compile_css_literal(font_size, "16px").has_value());
    }
};
