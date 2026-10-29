#include <flexUI/detail/tailwind_style_apply.h>
#include <flexUI/detail/style_property_registry.h>

#include <tinytest.hpp>

#include <cstdint>
#include <limits>
#include <type_traits>
#include <vector>

using namespace flexUI;
using namespace flexUI::tailwind;
using namespace flexUI::detail;

static_assert(std::is_same_v<std::underlying_type_t<StylePropertyId>,
                             std::uint16_t>);
static_assert(static_cast<std::uint16_t>(StylePropertyId::Opacity) == 1);
static_assert(static_cast<std::uint16_t>(StylePropertyId::Visibility) == 24);

namespace {

ComputedStyle base_style() {
    ComputedStyle base;
    base.opacity = 0.37f;
    base.background_color = Color{0.1f, 0.2f, 0.3f, 0.4f};
    base.text_color = Color{0.5f, 0.6f, 0.7f, 0.8f};
    base.border_color = Color{0.9f, 0.8f, 0.7f, 0.6f};
    base.width = 123.0f;
    base.font_size = 19.0f;
    return base;
}

void check_paint_equal(const ComputedStyle& actual,
                       const ComputedStyle& expected) {
    check_within(actual.opacity, expected.opacity, 0.00001f);
    check(actual.background_color == expected.background_color);
    check(actual.text_color == expected.text_color);
    check(actual.border_color == expected.border_color);
    check_equal(actual.has_border_side_colors, expected.has_border_side_colors);
    for (std::size_t side = 0; side < 4; ++side) {
        check(actual.border_colors[side] == expected.border_colors[side]);
    }
    check_equal(actual.width, expected.width);
    check_equal(actual.font_size, expected.font_size);
}

} // namespace

suite("Native Tailwind canonical style bridge") {
    it("writes opacity and all supported colors through the real registry") {
        const auto program = compile_native_class_list(
            "opacity-25 bg-[#1234] text-[#abcdef] border-[#10203080]");
        check_true(bool(program));
        if (!program) return;

        const ComputedStyle base = base_style();
        const auto result = apply_native_style(*program.program, base);
        check_true(bool(result));
        if (!result) return;

        check_within(result.style->opacity, 0.25f, 0.00001f);
        check(result.style->background_color ==
              Color(17.0f / 255, 34.0f / 255, 51.0f / 255, 68.0f / 255));
        check(result.style->text_color ==
              Color(171.0f / 255, 205.0f / 255, 239.0f / 255, 1.0f));
        check(result.style->border_color ==
              Color(16.0f / 255, 32.0f / 255, 48.0f / 255, 128.0f / 255));
        check_equal(result.impact,
                    std::uint32_t(STYLE_IMPACT_PAINT | STYLE_IMPACT_COMPOSITE));
        check(result.diagnostic.code == NativeError::None);
        check_equal(result.style->width, base.width);
        check_equal(result.style->font_size, base.font_size);
        check_paint_equal(base, base_style());
    }

    it("uses frozen theme colors and native alpha modifiers") {
        NativeTheme theme;
        theme.colors.push_back({"accent", Color{0.2f, 0.4f, 0.8f, 1.0f}});
        const auto program = compile_native_class_list(
            "bg-accent/50 text-accent/25 border-accent", theme);
        check_true(bool(program));
        if (!program) return;
        theme.colors[0].value = Color{1.0f, 0.0f, 0.0f, 1.0f};
        theme.colors.clear();

        const auto result = apply_native_style(*program.program, base_style());
        check_true(bool(result));
        if (!result) return;
        check(result.style->background_color == Color(0.2f, 0.4f, 0.8f, 0.5f));
        check(result.style->text_color == Color(0.2f, 0.4f, 0.8f, 0.25f));
        check(result.style->border_color == Color(0.2f, 0.4f, 0.8f, 1.0f));
        check_equal(result.impact, std::uint32_t(STYLE_IMPACT_PAINT));
    }

    it("replaces stale side colors through the canonical uniform border writer") {
        const auto program = compile_native_class_list("border-[#204080]");
        const auto empty = compile_native_class_list("");
        const auto opacity_only = compile_native_class_list("opacity-50");
        check_true(bool(program));
        check_true(bool(empty));
        check_true(bool(opacity_only));
        if (!program || !empty || !opacity_only) return;

        ComputedStyle base = base_style();
        base.has_border_side_colors = true;
        base.border_colors[0] = Color{1.0f, 0.0f, 0.0f, 1.0f};
        base.border_colors[1] = Color{0.0f, 1.0f, 0.0f, 1.0f};
        base.border_colors[2] = Color{0.0f, 0.0f, 1.0f, 1.0f};
        base.border_colors[3] = Color{1.0f, 1.0f, 0.0f, 1.0f};
        const ComputedStyle unchanged_base = base;
        const auto uniform = apply_native_style(*program.program, base);
        check_true(bool(uniform));
        if (!uniform) return;

        const Color color{32.0f / 255, 64.0f / 255, 128.0f / 255, 1.0f};
        check_true(uniform.style->has_border_side_colors);
        check(uniform.style->border_color == color);
        for (const auto& side : uniform.style->border_colors) check(side == color);
        check_paint_equal(base, unchanged_base);

        // Model a later base with one side overridden. The preceding uniform
        // write must have replaced all stored sides, so enabling/using the side
        // array cannot revive colors from before that write. No CSS parser or
        // new single-side utility is involved in this test.
        ComputedStyle one_side = *uniform.style;
        const Color right{0.8f, 0.2f, 0.4f, 1.0f};
        one_side.border_color = right;
        one_side.border_colors[1] = right;
        const auto preserved = apply_native_style(*empty.program, one_side);
        const auto other_property = apply_native_style(*opacity_only.program, one_side);
        check_true(bool(preserved));
        check_true(bool(other_property));
        if (!preserved || !other_property) return;
        check_paint_equal(*preserved.style, one_side);
        for (std::size_t side = 0; side < 4; ++side) {
            const Color expected = side == 1 ? right : color;
            check(preserved.style->border_colors[side] == expected);
            check(other_property.style->border_colors[side] == expected);
        }
        check_true(other_property.style->has_border_side_colors);
        check(other_property.style->border_color == right);
    }

    it("requires hover capability and restores the base when hover exits") {
        const auto program = compile_native_class_list(
            "hover:opacity-80 hover:bg-[#ffffff]");
        check_true(bool(program));
        if (!program) return;
        const ComputedStyle base = base_style();
        NativeState state;
        state.hover = true;

        const auto incapable = apply_native_style(*program.program, base, state);
        check_true(bool(incapable));
        if (!incapable) return;
        check_paint_equal(*incapable.style, base);
        check_equal(incapable.impact, std::uint32_t(0));

        state.hover_capable = true;
        const auto hovered = apply_native_style(*program.program, base, state);
        check_true(bool(hovered));
        if (!hovered) return;
        check_within(hovered.style->opacity, 0.8f, 0.00001f);
        check(hovered.style->background_color == Color(1.0f, 1.0f, 1.0f, 1.0f));

        state.hover = false;
        const auto restored = apply_native_style(*program.program, base, state);
        check_true(bool(restored));
        if (!restored) return;
        check_paint_equal(*restored.style, base);
        check_equal(restored.impact, std::uint32_t(0));
        // Independent evaluations do not mutate earlier candidates or base.
        check_within(hovered.style->opacity, 0.8f, 0.00001f);
        check_paint_equal(base, base_style());
    }

    it("removes class effects by evaluating an empty program from the base") {
        const auto styled = compile_native_class_list(
            "opacity-0 bg-transparent text-transparent border-transparent");
        const auto empty = compile_native_class_list("");
        check_true(bool(styled));
        check_true(bool(empty));
        if (!styled || !empty) return;

        const ComputedStyle base = base_style();
        const auto first = apply_native_style(*styled.program, base);
        check_true(bool(first));
        if (!first) return;
        check_equal(first.style->opacity, 0.0f);
        check_equal(first.style->background_color.a, 0.0f);
        check_equal(first.style->text_color.a, 0.0f);
        check_equal(first.style->border_color.a, 0.0f);

        const auto restored = apply_native_style(*empty.program, base);
        check_true(bool(restored));
        if (!restored) return;
        check_paint_equal(*restored.style, base);
        check_equal(restored.impact, std::uint32_t(0));
        check_equal(first.style->opacity, 0.0f);
    }

    it("honors stacked state specificity when several states are active") {
        const auto program = compile_native_class_list(
            "hover:focus:opacity-20 disabled:opacity-80");
        const auto reversed = compile_native_class_list(
            "disabled:opacity-80 hover:focus:opacity-20");
        check_true(bool(program));
        check_true(bool(reversed));
        if (!program || !reversed) return;

        NativeState state;
        state.hover_capable = true;
        state.hover = true;
        state.focus = true;
        state.disabled = true;
        const auto stacked = apply_native_style(*program.program, base_style(), state);
        const auto reordered = apply_native_style(*reversed.program, base_style(), state);
        check_true(bool(stacked));
        check_true(bool(reordered));
        if (!stacked || !reordered) return;
        check_within(stacked.style->opacity, 0.2f, 0.00001f);
        check_within(reordered.style->opacity, 0.2f, 0.00001f);

        state.focus = false;
        const auto disabled = apply_native_style(*program.program, base_style(), state);
        check_true(bool(disabled));
        if (!disabled) return;
        check_within(disabled.style->opacity, 0.8f, 0.00001f);
    }

    it("does not count dark as pseudo specificity and preserves important") {
        const auto program = compile_native_class_list(
            "hover:opacity-20 dark:opacity-80");
        const auto important = compile_native_class_list(
            "opacity-40! hover:focus:opacity-80");
        check_true(bool(program));
        check_true(bool(important));
        if (!program || !important) return;

        NativeState state;
        state.dark = true;
        state.hover_capable = true;
        state.hover = true;
        state.focus = true;
        const auto dark = apply_native_style(*program.program, base_style(), state);
        const auto forced = apply_native_style(*important.program, base_style(), state);
        check_true(bool(dark));
        check_true(bool(forced));
        if (!dark || !forced) return;
        check_within(dark.style->opacity, 0.2f, 0.00001f);
        check_within(forced.style->opacity, 0.4f, 0.00001f);
    }

    it("applies normalized arbitrary opacity without string property evaluation") {
        const auto program = compile_native_class_list("opacity-[62.5%]");
        check_true(bool(program));
        if (!program) return;
        const auto result = apply_native_style(*program.program, base_style());
        check_true(bool(result));
        if (!result) return;
        check_within(result.style->opacity, 0.625f, 0.00001f);
    }

    it("rejects unsupported utility grammar even when theme colors share its spelling") {
        NativeTheme theme;
        const Color color{0.2f, 0.4f, 0.6f, 1.0f};
        for (const auto* name : {"center", "none", "solid", "linear-to-r",
                                 "t-accent", "accent", "shadow-sm", "spacing-x-2",
                                 "blend-multiply", "gradient-to-r", "left-top",
                                 "radial"}) {
            theme.colors.push_back({name, color});
        }
        const ComputedStyle base = base_style();
        const auto valid = compile_native_class_list("opacity-25", theme);
        check_true(bool(valid));
        if (!valid) return;

        for (const auto* utility : {"text-center", "bg-none", "border-solid",
                                    "bg-linear-to-r", "border-t-accent",
                                    "text-shadow-sm", "border-spacing-x-2",
                                    "bg-blend-multiply", "bg-gradient-to-r",
                                    "bg-left-top", "bg-radial", "hover:text-center!"}) {
            const auto rejected = compile_native_class_list(utility, theme);
            check_false(bool(rejected));
            check(rejected.diagnostic.code == NativeError::UnsupportedUtility);
            // A failed replacement has no program to apply and cannot affect
            // a previously compiled program or the caller's base style.
            check_false(bool(rejected.program));
        }
        const auto unchanged = apply_native_style(*valid.program, base);
        check_true(bool(unchanged));
        if (!unchanged) return;
        check_within(unchanged.style->opacity, 0.25f, 0.00001f);
        check_paint_equal(base, base_style());
    }

    it("reserves utility grammar without globally banning useful theme names") {
        NativeTheme theme;
        const Color color{0.2f, 0.4f, 0.6f, 1.0f};
        for (const auto* name : {"center", "solid", "topaz", "shadowy", "linearity"}) {
            theme.colors.push_back({name, color});
        }
        for (const auto* utility : {"border-center", "bg-solid", "border-topaz",
                                    "text-shadowy", "bg-linearity"}) {
            const auto program = compile_native_class_list(utility, theme);
            check_true(bool(program));
            if (!program) return;
            check_equal(program.program->rules().size(), std::size_t(1));
            if (program.program->rules().empty()) return;
            const auto result = apply_native_style(*program.program, base_style());
            check_true(bool(result));
            if (!result) return;
            const auto property = program.program->rules().front().declaration.property;
            if (property == StylePropertyId::BackgroundColor)
                check(result.style->background_color == color);
            else if (property == StylePropertyId::TextColor)
                check(result.style->text_color == color);
            else {
                check(property == StylePropertyId::BorderColor);
                check(result.style->border_color == color);
            }
        }
    }

    it("rejects unknown and unsupported property IDs without changing the target") {
        const ComputedStyle base = base_style();
        ComputedStyle candidate = base;
        for (const auto id : {StylePropertyId::Unknown,
                              static_cast<StylePropertyId>(65535),
                              StylePropertyId::FontSize}) {
            const auto result = write_native_declaration({id, 0.5f}, candidate);
            check_false(bool(result));
            check(result.error == NativeError::UnsupportedValue);
            check_equal(result.impact, std::uint32_t(0));
            check_paint_equal(candidate, base);
        }
        check_paint_equal(base, base_style());
    }

    it("checks actual value storage instead of trusting the target descriptor type") {
        const ComputedStyle base = base_style();
        ComputedStyle candidate = base;
        const std::vector<NativeDeclaration> wrong_types{
            {StylePropertyId::Opacity, Color{0.8f, 0.7f, 0.6f, 0.5f}},
            {StylePropertyId::BackgroundColor, 0.5f},
            {StylePropertyId::TextColor, 0.5f},
            {StylePropertyId::BorderColor, 0.5f},
        };
        for (const auto& declaration : wrong_types) {
            const auto result = write_native_declaration(declaration, candidate);
            check_false(bool(result));
            check(result.error == NativeError::PropertyTypeMismatch);
            check_equal(result.impact, std::uint32_t(0));
            check_paint_equal(candidate, base);
        }
        check_paint_equal(base, base_style());
    }

    it("rejects nonfinite and out-of-range scalar values before writing") {
        const ComputedStyle base = base_style();
        ComputedStyle candidate = base;
        const float infinity = std::numeric_limits<float>::infinity();
        for (const float value : {std::numeric_limits<float>::quiet_NaN(),
                                   infinity, -infinity, -0.01f, 1.01f}) {
            const auto result = write_native_declaration(
                {StylePropertyId::Opacity, value}, candidate);
            check_false(bool(result));
            check(result.error == NativeError::InvalidValue);
            check_equal(result.impact, std::uint32_t(0));
            check_paint_equal(candidate, base);
        }
    }

    it("validates every color channel before the canonical writer sees it") {
        const ComputedStyle base = base_style();
        ComputedStyle candidate = base;
        const float nan = std::numeric_limits<float>::quiet_NaN();
        const float infinity = std::numeric_limits<float>::infinity();
        const std::vector<Color> invalid{
            {nan, 0.2f, 0.3f, 0.4f},
            {0.1f, infinity, 0.3f, 0.4f},
            {0.1f, 0.2f, -infinity, 0.4f},
            {0.1f, 0.2f, 0.3f, nan},
            {-0.01f, 0.2f, 0.3f, 0.4f},
            {0.1f, 1.01f, 0.3f, 0.4f},
            {0.1f, 0.2f, -0.01f, 0.4f},
            {0.1f, 0.2f, 0.3f, 1.01f},
        };
        for (const auto& value : invalid) {
            const auto result = write_native_declaration(
                {StylePropertyId::BackgroundColor, value}, candidate);
            check_false(bool(result));
            check(result.error == NativeError::InvalidValue);
            check_equal(result.impact, std::uint32_t(0));
            check_paint_equal(candidate, base);
        }
    }

    it("retains the last successful candidate when a later declaration is rejected") {
        const ComputedStyle base = base_style();
        ComputedStyle candidate = base;
        const auto applied = write_native_declaration(
            {StylePropertyId::Opacity, 0.6f}, candidate);
        check_true(bool(applied));
        check_within(candidate.opacity, 0.6f, 0.00001f);
        check_equal(applied.impact,
                    std::uint32_t(STYLE_IMPACT_PAINT | STYLE_IMPACT_COMPOSITE));
        const ComputedStyle before_failure = candidate;

        const auto rejected = write_native_declaration(
            {StylePropertyId::BackgroundColor, 0.7f}, candidate);
        check_false(bool(rejected));
        check_paint_equal(candidate, before_failure);
        check_paint_equal(base, base_style());
    }
}
