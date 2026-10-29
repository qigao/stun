#include <flexUI/tailwind_native.h>

#include <tinytest.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

namespace tw = flexUI::tailwind;
using Property = flexUI::detail::StylePropertyId;

static_assert(!std::is_copy_constructible_v<tw::NativeProgram>,
              "NativeProgram ownership must remain behind its immutable handle");
static_assert(!std::is_copy_assignable_v<tw::NativeProgram>,
              "NativeProgram must not be replaced through copy assignment");
static_assert(!std::is_move_constructible_v<tw::NativeProgram>,
              "NativeProgram must not be moved out of its published handle");
static_assert(!std::is_move_assignable_v<tw::NativeProgram>,
              "NativeProgram must not be replaced through move assignment");

bool near(float actual, float expected) {
  return std::isfinite(actual) && std::fabs(actual - expected) <= 0.000001f;
}

bool rejected(const tw::NativeCompileResult& result) {
  return !result.program && result.diagnostic.code != tw::NativeError::None &&
         !result.diagnostic.message.empty();
}

bool error_is(const tw::NativeCompileResult& result, tw::NativeError error) {
  return rejected(result) && result.diagnostic.code == error;
}

const tw::NativeRule* single_rule(const tw::NativeCompileResult& result) {
  return result.program && result.program->rules().size() == 1
             ? &result.program->rules().front()
             : nullptr;
}

// Rules are published in application order. This observer deliberately uses
// only the public rule/condition contract, not the compiler's ordering helper.
const tw::NativeRule* winner(const tw::NativeProgram& program, Property property,
                             const tw::NativeState& state = {}) {
  const tw::NativeRule* selected = nullptr;
  for (const auto& rule : program.rules()) {
    if (rule.declaration.property == property && rule.condition.matches(state)) {
      selected = &rule;
    }
  }
  return selected;
}

bool has_float(const tw::NativeRule* rule, Property property, float expected) {
  if (!rule || rule->declaration.property != property) return false;
  const auto* value = std::get_if<float>(&rule->declaration.value);
  return value && near(*value, expected);
}

bool has_color(const tw::NativeRule* rule, Property property,
               const flex::Color& expected) {
  if (!rule || rule->declaration.property != property) return false;
  const auto* value = std::get_if<flex::Color>(&rule->declaration.value);
  return value && near(value->r, expected.r) && near(value->g, expected.g) &&
         near(value->b, expected.b) && near(value->a, expected.a);
}

tw::NativeTheme theme_fixture() {
  tw::NativeTheme theme;
  theme.colors = {
      {"brand", flex::Color(0.2f, 0.4f, 0.6f, 0.8f)},
      {"A1_accent-2", flex::Color(0.8f, 0.3f, 0.1f, 1.0f)},
  };
  return theme;
}

std::vector<std::string> tokens(const tw::NativeProgram& program) {
  std::vector<std::string> result;
  for (const auto& rule : program.rules()) result.push_back(rule.token);
  return result;
}

}  // namespace

spec("Native Tailwind paint values are compiled without a token catalog") {
  it("accepts canonical quarter-percent opacity values including unseen values") {
    struct Example {
      const char* source;
      float expected;
    };
    const Example examples[] = {
        {"opacity-0", 0.0f},
        {"opacity-0.25", 0.0025f},
        {"opacity-0.5", 0.005f},
        {"opacity-0.75", 0.0075f},
        {"opacity-12.75", 0.1275f},
        {"opacity-37.5", 0.375f},
        {"opacity-99.75", 0.9975f},
        {"opacity-100", 1.0f},
    };
    for (const auto& example : examples) {
      const auto result = tw::compile_native_class_list(example.source);
      check(result.program != nullptr);
      check(result.diagnostic.code == tw::NativeError::None);
      check(has_float(single_rule(result), Property::Opacity, example.expected));
    }
  }

  it("accepts arbitrary decimal and percent opacity without the bare-value restriction") {
    struct Example {
      const char* source;
      float expected;
    };
    const Example examples[] = {
        {"opacity-[0]", 0.0f},
        {"opacity-[1]", 1.0f},
        {"opacity-[0.123456]", 0.123456f},
        {"opacity-[00.50]", 0.5f},
        {"opacity-[0.5000]", 0.5f},
        {"opacity-[0%]", 0.0f},
        {"opacity-[100%]", 1.0f},
        {"opacity-[12.345%]", 0.12345f},
    };
    for (const auto& example : examples) {
      const auto result = tw::compile_native_class_list(example.source);
      check(has_float(single_rule(result), Property::Opacity, example.expected));
    }
  }

  it("converts a large mantissa and compensating exponent as one decimal value") {
    // The final value is one even on platforms where long double is double.
    // Accumulating this mantissa before applying its exponent would overflow.
    const std::string decimal = "1" + std::string(320, '0') + "e-320";
    const auto number =
        tw::compile_native_class_list("opacity-[" + decimal + "]");
    check(has_float(single_rule(number), Property::Opacity, 1.0f));
    const auto percent =
        tw::compile_native_class_list("opacity-[" + decimal + "%]");
    check(has_float(single_rule(percent), Property::Opacity, 0.01f));
  }

  it("accepts zero mantissas with extreme exponents after validating their syntax") {
    const char* zero_values[] = {
        "opacity-[0e9999]", "opacity-[-0e9999]", "opacity-[0e-9999]",
        "opacity-[-0e-9999]", "opacity-[000.000e9999]",
        "opacity-[0e9999%]", "opacity-[-0e9999%]",
    };
    for (const char* source : zero_values) {
      const auto result = tw::compile_native_class_list(source);
      check(has_float(single_rule(result), Property::Opacity, 0.0f));
    }
  }

  it("rejects incomplete exponents and out-of-domain exponent values") {
    const char* invalid[] = {
        "opacity-[1e]", "opacity-[1e+]", "opacity-[1e-]",
        "opacity-[1e--2]", "opacity-[1ee2]", "opacity-[0e]",
        "opacity-[0e+]", "opacity-[-0e-]", "opacity-[0e9999junk]",
        "opacity-[2e0]", "opacity-[-1e0]", "opacity-[101e0%]",
        "opacity-[1e9999]", "opacity-[1e9999%]",
    };
    for (const char* source : invalid) {
      check(rejected(tw::compile_native_class_list(source)));
    }
    const std::string too_large = "1" + std::string(320, '0') + "e-319";
    check(rejected(
        tw::compile_native_class_list("opacity-[" + too_large + "]")));
  }

  it("rejects a nonzero decimal that underflows to zero") {
    for (const char* source : {"opacity-[1e-400]", "opacity-[1e-9999]",
                               "opacity-[1e-400%]", "opacity-[1e-100]",
                               "opacity-[1e-100%]"}) {
      check(rejected(tw::compile_native_class_list(source)));
    }
  }

  it("rejects noncanonical bare numbers instead of partially parsing them") {
    const char* invalid[] = {
        "opacity-050", "opacity-25.0", "opacity-0.10", "opacity-0.1",
        "opacity-25junk", "bg-brand/05", "text-brand/50.0",
    };
    for (const char* source : invalid) {
      const auto result = tw::compile_native_class_list(source, theme_fixture());
      check(error_is(result, tw::NativeError::UnsupportedValue));
    }
  }

  it("rejects nonfinite, overflowing, out-of-domain and unit-bearing opacity") {
    const char* invalid[] = {
        "opacity-101", "opacity--1", "opacity-[1.01]", "opacity-[-0.01]",
        "opacity-[101%]", "opacity-[-1%]", "opacity-[NaN]", "opacity-[nan]",
        "opacity-[Inf]", "opacity-[Infinity]", "opacity-[1e309]",
        "opacity-[0.5junk]", "opacity-[50px]", "opacity-[0.5%junk]",
    };
    for (const char* source : invalid) {
      check(rejected(tw::compile_native_class_list(source)));
    }
  }

  it("lowers every supported hexadecimal width to straight-alpha sRGB") {
    struct Example {
      const char* source;
      Property property;
      flex::Color expected;
    };
    const Example examples[] = {
        {"bg-[#1aF]", Property::BackgroundColor,
         flex::Color(17.0f / 255.0f, 170.0f / 255.0f, 1.0f, 1.0f)},
        {"text-[#1aF8]", Property::TextColor,
         flex::Color(17.0f / 255.0f, 170.0f / 255.0f, 1.0f, 136.0f / 255.0f)},
        {"border-[#12aBcD]", Property::BorderColor,
         flex::Color(18.0f / 255.0f, 171.0f / 255.0f, 205.0f / 255.0f, 1.0f)},
        {"bg-[#12aBcD80]", Property::BackgroundColor,
         flex::Color(18.0f / 255.0f, 171.0f / 255.0f, 205.0f / 255.0f,
                     128.0f / 255.0f)},
    };
    for (const auto& example : examples) {
      const auto result = tw::compile_native_class_list(example.source);
      check(has_color(single_rule(result), example.property, example.expected));
    }
  }

  it("does not accept partial hexadecimal colors or trailing hexadecimal garbage") {
    const char* invalid[] = {
        "bg-[#]", "bg-[#1]", "bg-[#12]", "bg-[#12345]", "bg-[#1234567]",
        "bg-[#123456789]", "bg-[#ggg]", "bg-[#12x]", "bg-[#12345z]",
        "bg-[#1234567z]", "bg-[#0x1234]", "bg-[#abc]junk", "bg-[#12 3]",
    };
    for (const char* source : invalid) {
      check(rejected(tw::compile_native_class_list(source)));
    }
  }

  it("resolves typed theme colors for all three color properties") {
    const auto result = tw::compile_native_class_list(
        "bg-brand text-A1_accent-2 border-brand", theme_fixture());
    check(result.program != nullptr);
    if (!result.program) return;
    check(result.program->rules().size() == 3);
    check(has_color(winner(*result.program, Property::BackgroundColor),
                    Property::BackgroundColor, flex::Color(0.2f, 0.4f, 0.6f, 0.8f)));
    check(has_color(winner(*result.program, Property::TextColor),
                    Property::TextColor, flex::Color(0.8f, 0.3f, 0.1f, 1.0f)));
    check(has_color(winner(*result.program, Property::BorderColor),
                    Property::BorderColor, flex::Color(0.2f, 0.4f, 0.6f, 0.8f)));
  }

  it("multiplies color alpha without premultiplying RGB") {
    struct Example {
      const char* source;
      Property property;
      float expected_alpha;
    };
    const Example examples[] = {
        {"bg-brand/0", Property::BackgroundColor, 0.0f},
        {"bg-brand/12.75", Property::BackgroundColor, 0.102f},
        {"text-brand/100", Property::TextColor, 0.8f},
        {"border-brand/[0.125]", Property::BorderColor, 0.1f},
        {"bg-brand/[00.50]", Property::BackgroundColor, 0.4f},
        {"text-brand/[12.5%]", Property::TextColor, 0.1f},
    };
    for (const auto& example : examples) {
      const auto result =
          tw::compile_native_class_list(example.source, theme_fixture());
      check(has_color(single_rule(result), example.property,
                      flex::Color(0.2f, 0.4f, 0.6f, example.expected_alpha)));
    }
    const auto hex = tw::compile_native_class_list("bg-[#12abcd80]/50");
    check(has_color(single_rule(hex), Property::BackgroundColor,
                    flex::Color(18.0f / 255.0f, 171.0f / 255.0f,
                                205.0f / 255.0f, 64.0f / 255.0f)));
  }

  it("supports transparent without a theme and preserves zero alpha with modifiers") {
    for (const char* source : {"bg-transparent", "bg-transparent/50",
                               "bg-transparent/[75%]"}) {
      const auto result = tw::compile_native_class_list(source);
      check(has_color(single_rule(result), Property::BackgroundColor,
                      flex::Color(0.0f, 0.0f, 0.0f, 0.0f)));
    }
  }

  it("rejects invalid alpha instead of accepting a numeric prefix") {
    const char* invalid[] = {
        "bg-brand/101", "bg-brand/-1", "bg-brand/0.1", "bg-brand/50junk",
        "bg-brand/[1.1]", "bg-brand/[-0.1]", "bg-brand/[101%]",
        "bg-brand/[NaN]", "bg-brand/[1e309]", "bg-brand/[0.5px]",
        "bg-brand/[0.5]junk", "bg-brand/50/25",
    };
    for (const char* source : invalid) {
      check(rejected(tw::compile_native_class_list(source, theme_fixture())));
    }
  }
}

spec("Native Tailwind themes are validated and frozen") {
  it("reports an unknown theme color without substituting a default") {
    const std::string source = "opacity-25 bg-missing text-brand";
    const auto result = tw::compile_native_class_list(source, theme_fixture());
    check(error_is(result, tw::NativeError::UnknownThemeToken));
    check(result.diagnostic.source.offset == source.find("bg-missing"));
    check(result.diagnostic.source.length == std::string("bg-missing").size());
  }

  it("rejects duplicate names even when the colors are not referenced") {
    auto theme = theme_fixture();
    theme.colors.push_back({"brand", flex::Color(1.0f, 0.0f, 0.0f, 1.0f)});
    const auto result = tw::compile_native_class_list("opacity-25", theme);
    check(error_is(result, tw::NativeError::InvalidTheme));
  }

  it("rejects invalid and reserved theme names") {
    const char* invalid_names[] = {
        "", "_accent", "9accent", "accent_", "accent-", "two words",
        "accent:color", "accent/name", "accent[name]", "inherit", "current",
        "transparent", "123",
    };
    for (const char* name : invalid_names) {
      tw::NativeTheme theme;
      theme.colors.push_back({name, flex::Color(0.2f, 0.3f, 0.4f, 1.0f)});
      check(error_is(tw::compile_native_class_list("", theme),
                     tw::NativeError::InvalidTheme));
    }
  }

  it("requires every RGBA channel to be finite and inside the unit interval") {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const flex::Color invalid_colors[] = {
        flex::Color(-0.01f, 0.2f, 0.3f, 1.0f),
        flex::Color(0.1f, 1.01f, 0.3f, 1.0f),
        flex::Color(0.1f, 0.2f, -0.01f, 1.0f),
        flex::Color(0.1f, 0.2f, 0.3f, 1.01f),
        flex::Color(nan, 0.2f, 0.3f, 1.0f),
        flex::Color(0.1f, infinity, 0.3f, 1.0f),
        flex::Color(0.1f, 0.2f, nan, 1.0f),
        flex::Color(0.1f, 0.2f, 0.3f, -infinity),
    };
    for (const auto& color : invalid_colors) {
      tw::NativeTheme theme;
      theme.colors.push_back({"unused", color});
      check(error_is(tw::compile_native_class_list("opacity-25", theme),
                     tw::NativeError::InvalidTheme));
    }
  }

  it("copies theme values and source text into the immutable program") {
    std::shared_ptr<const tw::NativeProgram> program;
    {
      auto theme = theme_fixture();
      std::string source = "bg-brand/50";
      const auto result = tw::compile_native_class_list(source, theme);
      check(result.program != nullptr);
      if (!result.program) return;
      program = result.program;
      theme.colors.front().name = "renamed";
      theme.colors.front().value = flex::Color(1.0f, 0.0f, 0.0f, 1.0f);
      theme.colors.clear();
      theme.colors.shrink_to_fit();
      source.assign(256, 'x');
    }
    check(program->rules().front().token == "bg-brand/50");
    check(has_color(winner(*program, Property::BackgroundColor),
                    Property::BackgroundColor, flex::Color(0.2f, 0.4f, 0.6f, 0.4f)));
  }
}

spec("Native Tailwind conditions and precedence have independent semantics") {
  it("compiles the supported states into the corresponding condition bits") {
    struct Example {
      const char* variant;
      std::uint16_t bit;
      bool tw::NativeState::*field;
    };
    const Example examples[] = {
        {"focus-within", tw::NativeFocusWithin, &tw::NativeState::focus_within},
        {"hover", tw::NativeHover, &tw::NativeState::hover},
        {"focus", tw::NativeFocus, &tw::NativeState::focus},
        {"focus-visible", tw::NativeFocusVisible, &tw::NativeState::focus_visible},
        {"active", tw::NativeActive, &tw::NativeState::active},
        {"disabled", tw::NativeDisabled, &tw::NativeState::disabled},
        {"dark", tw::NativeDark, &tw::NativeState::dark},
    };
    for (const auto& example : examples) {
      const auto result = tw::compile_native_class_list(
          std::string(example.variant) + ":opacity-25");
      const auto* rule = single_rule(result);
      check(rule != nullptr);
      if (!rule) continue;
      check(rule->condition.required == example.bit);
      check(rule->specificity == (example.bit == tw::NativeDark ? 0 : 1));
      tw::NativeState state;
      state.hover_capable = true;
      check(!rule->condition.matches(state));
      state.*(example.field) = true;
      check(rule->condition.matches(state));
    }
  }

  it("requires hover capability as well as the hover state") {
    const auto result = tw::compile_native_class_list("hover:opacity-25");
    const auto* rule = single_rule(result);
    check(rule != nullptr);
    if (!rule) return;
    tw::NativeState state;
    state.hover = true;
    check(!rule->condition.matches(state));
    state.hover_capable = true;
    check(rule->condition.matches(state));
    state.hover = false;
    check(!rule->condition.matches(state));
  }

  it("ANDs stacked conditions and does not add specificity for effective dark mode") {
    const auto result = tw::compile_native_class_list("hover:focus:dark:opacity-25");
    const auto* rule = single_rule(result);
    check(rule != nullptr);
    if (!rule) return;
    check(rule->condition.required ==
          (tw::NativeHover | tw::NativeFocus | tw::NativeDark));
    check(rule->specificity == 2);
    tw::NativeState state;
    state.hover_capable = true;
    state.hover = state.focus = state.dark = true;
    check(rule->condition.matches(state));
    state.focus = false;
    check(!rule->condition.matches(state));
    state.focus = true;
    state.dark = false;
    check(!rule->condition.matches(state));
    state.dark = true;
    state.hover_capable = false;
    check(!rule->condition.matches(state));
  }

  it("uses the host-resolved dark state and keeps hover more specific than dark") {
    const auto result =
        tw::compile_native_class_list("opacity-90 dark:opacity-20 hover:opacity-25");
    check(result.program != nullptr);
    if (!result.program) return;
    tw::NativeState state;
    check(has_float(winner(*result.program, Property::Opacity, state),
                    Property::Opacity, 0.9f));
    state.dark = true;
    check(has_float(winner(*result.program, Property::Opacity, state),
                    Property::Opacity, 0.2f));
    state.hover = state.hover_capable = true;
    check(has_float(winner(*result.program, Property::Opacity, state),
                    Property::Opacity, 0.25f));
  }

  it("lets two state pseudos outrank a later single disabled variant") {
    const auto result = tw::compile_native_class_list(
        "hover:focus:opacity-25 disabled:opacity-90");
    check(result.program != nullptr);
    if (!result.program) return;
    tw::NativeState state;
    state.hover_capable = state.hover = state.focus = state.disabled = true;
    check(has_float(winner(*result.program, Property::Opacity, state),
                    Property::Opacity, 0.25f));
    state.focus = false;
    check(has_float(winner(*result.program, Property::Opacity, state),
                    Property::Opacity, 0.9f));
  }

  it("uses the fixed variant order before the numeric value for equal specificity") {
    const auto result = tw::compile_native_class_list(
        "disabled:opacity-40 active:opacity-50 focus-visible:opacity-60 "
        "focus:opacity-70 hover:opacity-80 focus-within:opacity-90");
    check(result.program != nullptr);
    if (!result.program) return;
    tw::NativeState state;
    state.hover_capable = state.focus_within = state.hover = state.focus = true;
    state.focus_visible = state.active = state.disabled = true;
    check(has_float(winner(*result.program, Property::Opacity, state),
                    Property::Opacity, 0.4f));
    state.disabled = false;
    check(has_float(winner(*result.program, Property::Opacity, state),
                    Property::Opacity, 0.5f));
    state.active = false;
    check(has_float(winner(*result.program, Property::Opacity, state),
                    Property::Opacity, 0.6f));
    state.focus_visible = false;
    check(has_float(winner(*result.program, Property::Opacity, state),
                    Property::Opacity, 0.7f));
    state.focus = false;
    check(has_float(winner(*result.program, Property::Opacity, state),
                    Property::Opacity, 0.8f));
    state.hover = false;
    check(has_float(winner(*result.program, Property::Opacity, state),
                    Property::Opacity, 0.9f));
  }

  it("puts suffix important before specificity and still orders important variants") {
    const auto result = tw::compile_native_class_list(
        "opacity-30! hover:focus:disabled:opacity-90");
    check(result.program != nullptr);
    if (!result.program) return;
    tw::NativeState state;
    state.hover_capable = state.hover = state.focus = state.disabled = true;
    const auto* selected = winner(*result.program, Property::Opacity, state);
    check(selected && selected->important);
    check(has_float(selected, Property::Opacity, 0.3f));

    const auto variants = tw::compile_native_class_list(
        "hover:opacity-100! disabled:opacity-5!");
    check(variants.program != nullptr);
    if (!variants.program) return;
    check(has_float(winner(*variants.program, Property::Opacity, state),
                    Property::Opacity, 0.05f));
  }

  it("sorts numeric tokens naturally and ignores markup order") {
    const auto first = tw::compile_native_class_list("opacity-100 opacity-9 opacity-75");
    const auto second = tw::compile_native_class_list("opacity-75 opacity-9 opacity-100");
    check(first.program != nullptr);
    check(second.program != nullptr);
    if (!first.program || !second.program) return;
    const std::vector<std::string> expected{"opacity-9", "opacity-75", "opacity-100"};
    check(tokens(*first.program) == expected);
    check(tokens(*second.program) == expected);
    check(has_float(winner(*first.program, Property::Opacity), Property::Opacity, 1.0f));
    check(has_float(winner(*second.program, Property::Opacity), Property::Opacity, 1.0f));
  }

  it("breaks equal numeric chunks by source spelling for valid theme names") {
    tw::NativeTheme theme;
    theme.colors = {
        {"accent-02", flex::Color(1.0f, 0.0f, 0.0f, 1.0f)},
        {"accent-2", flex::Color(0.0f, 1.0f, 0.0f, 1.0f)},
        {"accent-10", flex::Color(0.0f, 0.0f, 1.0f, 1.0f)},
    };
    const auto tie = tw::compile_native_class_list("bg-accent-2 bg-accent-02", theme);
    check(tie.program != nullptr);
    if (!tie.program) return;
    check(tokens(*tie.program) ==
          std::vector<std::string>({"bg-accent-02", "bg-accent-2"}));
    check(has_color(winner(*tie.program, Property::BackgroundColor),
                    Property::BackgroundColor, flex::Color(0.0f, 1.0f, 0.0f, 1.0f)));
    const auto natural =
        tw::compile_native_class_list("bg-accent-10 bg-accent-2 bg-accent-02", theme);
    check(natural.program != nullptr);
    if (!natural.program) return;
    check(has_color(winner(*natural.program, Property::BackgroundColor),
                    Property::BackgroundColor, flex::Color(0.0f, 0.0f, 1.0f, 1.0f)));
  }
}

spec("Native Tailwind diagnostics do not create partial or fallback programs") {
  it("retains original source ranges after canonical sorting") {
    const std::string source =
        " \ttext-brand/25\nopacity-50  hover:opacity-25!\r\nbg-[#0f08] ";
    const auto result = tw::compile_native_class_list(source, theme_fixture());
    check(result.program != nullptr);
    if (!result.program) return;
    check(result.program->rules().size() == 4);
    for (const auto& rule : result.program->rules()) {
      check(rule.source.offset == source.find(rule.token));
      check(rule.source.length == rule.token.size());
      check(source.substr(rule.source.offset, rule.source.length) == rule.token);
    }
  }

  it("returns the first candidate error with its range and no preceding rules") {
    const std::string source = "opacity-25  bg-missing  flex";
    const auto result = tw::compile_native_class_list(source);
    check(error_is(result, tw::NativeError::UnknownThemeToken));
    check(result.diagnostic.source.offset == source.find("bg-missing"));
    check(result.diagnostic.source.length == std::string("bg-missing").size());
  }

  it("does not let a failed compilation mutate a previously returned program") {
    const auto active = tw::compile_native_class_list("opacity-25");
    check(active.program != nullptr);
    if (!active.program) return;
    const auto failed = tw::compile_native_class_list("opacity-75 bg-missing");
    check(error_is(failed, tw::NativeError::UnknownThemeToken));
    check(active.program->rules().size() == 1);
    check(has_float(winner(*active.program, Property::Opacity), Property::Opacity, 0.25f));
  }

  it("rejects duplicate variants and unsupported relationship variants explicitly") {
    const char* unsupported[] = {
        "hover:hover:opacity-25", "dark:dark:opacity-25",
        "group-hover:opacity-25", "peer-disabled:opacity-25",
        "data-[state=open]:opacity-25", "sm:opacity-25", "[&>span]:opacity-25",
    };
    for (const char* source : unsupported) {
      check(error_is(tw::compile_native_class_list(source),
                     tw::NativeError::UnsupportedVariant));
    }
  }

  it("rejects unsupported layout and effect utilities instead of producing CSS") {
    for (const char* source : {"flex", "w-4", "p-2", "shadow-md", "button-base"}) {
      check(error_is(tw::compile_native_class_list(source),
                     tw::NativeError::UnsupportedUtility));
    }
  }

  it("does not split a colon inside arbitrary-value brackets as a variant") {
    const std::string source = " \topacity-[var(--scope:amount)]";
    const auto result = tw::compile_native_class_list(source);
    check(error_is(result, tw::NativeError::UnsupportedValue));
    check(result.diagnostic.source.offset == 2);
    check(result.diagnostic.source.length == source.size() - 2);
    check(error_is(tw::compile_native_class_list("opacity-[calc(0.25+0.25)]"),
                   tw::NativeError::UnsupportedValue));
  }

  it("rejects malformed delimiters, legacy prefix important and trailing syntax") {
    const char* malformed[] = {
        "opacity-[0.5", "opacity-0.5]", "opacity-[]", ":opacity-25",
        "hover:", "hover::opacity-25", "!opacity-25", "hover:!opacity-25",
        "opacity-25!!", "opacity-25;", "opacity-[0.5]]",
    };
    for (const char* source : malformed) {
      check(rejected(tw::compile_native_class_list(source)));
    }
    const char embedded_nul[] = "opacity-25\0opacity-75";
    check(rejected(tw::compile_native_class_list(
        std::string_view(embedded_nul, sizeof(embedded_nul) - 1))));
  }
}

spec("Native Tailwind compilation has explicit bounded resource use") {
  it("allows an empty program while still accounting for its retained object") {
    const auto result = tw::compile_native_class_list(" \t\n\r\f ");
    check(result.program != nullptr);
    if (!result.program) return;
    check(result.program->rules().empty());
    check(result.program->retained_bytes() >= sizeof(tw::NativeProgram));
    tw::NativeCompileLimits limits;
    limits.max_program_bytes = result.program->retained_bytes();
    check(tw::compile_native_class_list("", {}, limits).program != nullptr);
    --limits.max_program_bytes;
    check(error_is(tw::compile_native_class_list("", {}, limits),
                   tw::NativeError::ResourceLimit));
  }

  it("rejects zero in each limit rather than interpreting it as unlimited") {
    const std::array<std::size_t tw::NativeCompileLimits::*, 8> fields = {
        &tw::NativeCompileLimits::max_source_bytes,
        &tw::NativeCompileLimits::max_token_bytes,
        &tw::NativeCompileLimits::max_tokens,
        &tw::NativeCompileLimits::max_rules,
        &tw::NativeCompileLimits::max_variant_depth,
        &tw::NativeCompileLimits::max_theme_tokens,
        &tw::NativeCompileLimits::max_theme_name_bytes,
        &tw::NativeCompileLimits::max_program_bytes,
    };
    for (auto field : fields) {
      tw::NativeCompileLimits limits;
      limits.*field = 0;
      check(error_is(tw::compile_native_class_list("", {}, limits),
                     tw::NativeError::InvalidLimits));
    }
  }

  it("checks raw source and individual token byte limits at their exact boundary") {
    const std::string token = "opacity-[0.375]";
    const std::string source = " \t" + token + "\n";
    tw::NativeCompileLimits limits;
    limits.max_source_bytes = source.size();
    limits.max_token_bytes = token.size();
    check(tw::compile_native_class_list(source, {}, limits).program != nullptr);
    --limits.max_source_bytes;
    check(error_is(tw::compile_native_class_list(source, {}, limits),
                   tw::NativeError::ResourceLimit));
    limits.max_source_bytes = source.size();
    --limits.max_token_bytes;
    check(error_is(tw::compile_native_class_list(source, {}, limits),
                   tw::NativeError::ResourceLimit));
  }

  it("counts raw tokens before deduplication and limits emitted rules separately") {
    tw::NativeCompileLimits limits;
    limits.max_tokens = 2;
    limits.max_rules = 1;
    const auto repeated =
        tw::compile_native_class_list("opacity-25 opacity-25", {}, limits);
    check(repeated.program != nullptr);
    if (repeated.program) check(repeated.program->rules().size() == 1);
    limits.max_tokens = 1;
    check(error_is(tw::compile_native_class_list("opacity-25 opacity-25", {}, limits),
                   tw::NativeError::ResourceLimit));

    limits.max_tokens = 2;
    check(error_is(tw::compile_native_class_list("opacity-25 opacity-75", {}, limits),
                   tw::NativeError::ResourceLimit));
    limits.max_rules = 2;
    const auto distinct =
        tw::compile_native_class_list("opacity-25 opacity-75", {}, limits);
    check(distinct.program != nullptr);
    if (distinct.program) check(distinct.program->rules().size() == 2);
  }

  it("limits stacked variants before publishing a rule") {
    tw::NativeCompileLimits limits;
    limits.max_variant_depth = 2;
    check(tw::compile_native_class_list("hover:focus:opacity-25", {}, limits).program !=
          nullptr);
    limits.max_variant_depth = 1;
    check(error_is(
        tw::compile_native_class_list("hover:focus:opacity-25", {}, limits),
        tw::NativeError::ResourceLimit));
  }

  it("counts all supplied theme tokens and their names even when not referenced") {
    tw::NativeTheme theme;
    theme.colors = {
        {"accent-2", flex::Color(0.1f, 0.2f, 0.3f, 1.0f)},
        {"brand", flex::Color(0.3f, 0.2f, 0.1f, 1.0f)},
    };
    tw::NativeCompileLimits limits;
    limits.max_theme_tokens = 2;
    limits.max_theme_name_bytes = std::string("accent-2").size();
    check(tw::compile_native_class_list("opacity-25", theme, limits).program != nullptr);
    limits.max_theme_tokens = 1;
    check(error_is(tw::compile_native_class_list("opacity-25", theme, limits),
                   tw::NativeError::ResourceLimit));
    limits.max_theme_tokens = 2;
    --limits.max_theme_name_bytes;
    check(error_is(tw::compile_native_class_list("opacity-25", theme, limits),
                   tw::NativeError::ResourceLimit));
  }

  it("enforces retained program bytes with an exact success and failure boundary") {
    const std::string source = "focus:opacity-[0.375] bg-[#12ab34cd]/[62.5%]";
    const auto baseline = tw::compile_native_class_list(source);
    check(baseline.program != nullptr);
    if (!baseline.program) return;
    const std::size_t retained = baseline.program->retained_bytes();
    check(retained > sizeof(tw::NativeProgram));
    tw::NativeCompileLimits limits;
    limits.max_program_bytes = retained;
    const auto exact = tw::compile_native_class_list(source, {}, limits);
    check(exact.program != nullptr);
    if (exact.program) check(exact.program->retained_bytes() == retained);
    limits.max_program_bytes = retained - 1;
    check(error_is(tw::compile_native_class_list(source, {}, limits),
                   tw::NativeError::ResourceLimit));
  }
}
