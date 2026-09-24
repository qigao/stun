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
};
