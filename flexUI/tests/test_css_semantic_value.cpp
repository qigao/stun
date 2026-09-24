#include <flexUI/detail/css_semantic_value.h>

#include <tinytest.hpp>

using namespace flexUI::detail;

suite("FlexUI shared CSS semantic values") {
    it("parses CSS number literals with one strict grammar") {
        const auto zero = parse_css_number_literal("0");
        const auto signed_value = parse_css_number_literal(" +.625 ");
        const auto exponent = parse_css_number_literal("-1.25e+2");

        check_true(zero.is_concrete());
        check_float_eq(zero.value, 0.0f, 0.0001f);
        check_true(signed_value.is_concrete());
        check_float_eq(signed_value.value, 0.625f, 0.0001f);
        check_true(exponent.is_concrete());
        check_float_eq(exponent.value, -125.0f, 0.0001f);
    }

    it("rejects non-CSS and malformed numeric spellings") {
        check_false(parse_css_number_literal("").is_concrete());
        check_false(parse_css_number_literal(".").is_concrete());
        check_false(parse_css_number_literal("1.").is_concrete());
        check_false(parse_css_number_literal("1e").is_concrete());
        check_false(parse_css_number_literal("0x1p-1").is_concrete());
        check_false(parse_css_number_literal("nan").is_concrete());
        check_false(parse_css_number_literal("inf").is_concrete());
        check_false(parse_css_number_literal("0.5junk").is_concrete());
    }

    it("marks computed-value forms as deferred") {
        check_true(parse_css_number_literal("50%").is_deferred());
        check_true(parse_css_number_literal("var(--opacity)").is_deferred());
        check_true(parse_css_number_literal("CALC(1 - .2)").is_deferred());
        check_true(parse_css_number_literal("env(opacity)").is_deferred());
        check_true(parse_css_number_literal("inherit").is_deferred());
        check_true(parse_css_number_literal("initial").is_deferred());
        check_true(parse_css_number_literal("unset").is_deferred());
    }

    it("parses context-independent visibility keywords") {
        const auto visible = parse_css_visibility_literal(" visible ");
        const auto hidden = parse_css_visibility_literal("HIDDEN");
        const auto collapse = parse_css_visibility_literal("collapse");

        check_true(visible.is_concrete());
        check(visible.value == Visibility::Visible);
        check_true(hidden.is_concrete());
        check(hidden.value == Visibility::Hidden);
        check_true(collapse.is_concrete());
        check(collapse.value == Visibility::Collapse);

        check_true(parse_css_visibility_literal("inherit").is_deferred());
        check_true(parse_css_visibility_literal("initial").is_deferred());
        check_true(parse_css_visibility_literal("unset").is_deferred());
        check_true(parse_css_visibility_literal("revert").is_deferred());
        check_true(
            parse_css_visibility_literal("var(--visibility)").is_deferred());
        const auto invalid = parse_css_visibility_literal("opaque");
        check_false(invalid.is_concrete());
        check_false(invalid.is_deferred());
    }

    it("parses context-independent color literals through the shared parser") {
        const auto hex = parse_css_color_literal("#336699");
        const auto rgb = parse_css_color_literal("rgb(255 0 128 / 50%)");
        const auto mixed =
            parse_css_color_literal("color-mix(in srgb, #000000 25%, #ffffff)");
        const auto light =
            parse_css_color_literal("light-dark(#112233, #ffffff)");

        check_true(hex.is_concrete());
        check_float_eq(hex.value.r, 0.2f, 0.001f);
        check_float_eq(hex.value.g, 0.4f, 0.001f);
        check_float_eq(hex.value.b, 0.6f, 0.001f);

        check_true(rgb.is_concrete());
        check_float_eq(rgb.value.r, 1.0f, 0.001f);
        check_float_eq(rgb.value.b, 128.0f / 255.0f, 0.001f);
        check_float_eq(rgb.value.a, 0.5f, 0.001f);

        check_true(mixed.is_concrete());
        check_true(light.is_concrete());
        check_float_eq(light.value.r, 0x11 / 255.0f, 0.001f);
    }

    it("parses shared context-independent CSS angles") {
        const auto deg = parse_css_angle_literal("90deg");
        const auto rad =
            parse_css_angle_literal("3.14159265358979323846rad");
        const auto turn = parse_css_angle_literal(".25turn");
        const auto grad = parse_css_angle_literal("100grad");
        const auto unitless = parse_css_angle_literal("-45");

        check_true(deg.is_concrete());
        check_float_eq(deg.value, 90.0f, 0.0001f);
        check_true(rad.is_concrete());
        check_float_eq(rad.value, 180.0f, 0.001f);
        check_true(turn.is_concrete());
        check_float_eq(turn.value, 90.0f, 0.0001f);
        check_true(grad.is_concrete());
        check_float_eq(grad.value, 90.0f, 0.0001f);
        check_true(unitless.is_concrete());
        check_float_eq(unitless.value, -45.0f, 0.0001f);

        check_true(parse_css_angle_literal("var(--angle)").is_deferred());
        check_true(parse_css_angle_literal("calc(90deg)").is_deferred());
        check_false(parse_css_angle_literal("12px").is_concrete());
    }

    it("classifies declaration-time context-independent lengths") {
        const auto px = parse_css_context_independent_length_literal(" 2px ");
        const auto unitless =
            parse_css_context_independent_length_literal("3.5");

        check_true(px.is_concrete());
        check_float_eq(px.value, 2.0f, 0.0001f);
        check_true(unitless.is_concrete());
        check_float_eq(unitless.value, 3.5f, 0.0001f);

        check_true(
            parse_css_context_independent_length_literal("50%").is_deferred());
        check_true(
            parse_css_context_independent_length_literal("2rem").is_deferred());
        check_true(
            parse_css_context_independent_length_literal("2em").is_deferred());
        check_true(
            parse_css_context_independent_length_literal("10vw").is_deferred());
        check_true(parse_css_context_independent_length_literal(
                       "calc(1px + 2px)")
                       .is_deferred());
        check_true(parse_css_context_independent_length_literal(
                       "var(--effect-width)")
                       .is_deferred());
        check_true(parse_css_context_independent_length_literal(
                       "env(safe-area-inset-top)")
                       .is_deferred());
        check_true(parse_css_context_independent_length_literal(
                       "min(1px, 2px)")
                       .is_deferred());
        check_true(parse_css_context_independent_length_literal(
                       "max(1px, 2px)")
                       .is_deferred());
        check_true(parse_css_context_independent_length_literal(
                       "clamp(1px, 2px, 3px)")
                       .is_deferred());

        const auto invalid =
            parse_css_context_independent_length_literal("2pt");
        check_false(invalid.is_concrete());
        check_false(invalid.is_deferred());
    }

    it("maps fixed border-width keywords without layout context") {
        check_float_eq(parse_css_border_width_literal("thin").value,
                       1.0f, 0.0001f);
        check_float_eq(parse_css_border_width_literal("medium").value,
                       3.0f, 0.0001f);
        check_float_eq(parse_css_border_width_literal("thick").value,
                       5.0f, 0.0001f);
        check_true(parse_css_border_width_literal("2px").is_concrete());
        check_true(parse_css_border_width_literal("1rem").is_deferred());
    }

    it("defers context-dependent color spellings") {
        check_true(parse_css_color_literal("currentColor").is_deferred());
        check_true(parse_css_color_literal("hsl(var(--primary) / .5)").is_deferred());
        check_true(parse_css_color_literal(
            "color-mix(in srgb, var(--muted) 50%, transparent)").is_deferred());
        check_true(parse_css_color_literal("inherit").is_deferred());

        const auto invalid = parse_css_color_literal("not-a-color");
        check_false(invalid.is_concrete());
        check_false(invalid.is_deferred());
    }
};
