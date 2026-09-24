#include <flexUI/detail/css_typed_value.h>

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

    it("does not invent typed values for unsupported properties") {
        const auto* background = style_property_find("background-color");
        const auto* font_size = style_property_find("font-size");

        check_not_null(background);
        check_not_null(font_size);
        check_false(compile_css_literal(background, "#336699").has_value());
        check_false(compile_css_literal(font_size, "16px").has_value());
    }
};
