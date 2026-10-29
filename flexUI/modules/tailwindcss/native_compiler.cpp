#include <flexUI/tailwind_native.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <locale>
#include <map>
#include <new>
#include <sstream>
#include <stdexcept>

namespace flexUI::tailwind {
namespace {

using Property = flexUI::detail::StylePropertyId;

bool digit(char c) noexcept { return c >= '0' && c <= '9'; }
bool letter(char c) noexcept {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}
bool space(char c) noexcept {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f';
}
bool starts(std::string_view value, std::string_view prefix) noexcept {
  return value.substr(0, prefix.size()) == prefix;
}

NativeCompileResult failure(NativeError code, NativeSourceRange source,
                            const char* message) {
  return {nullptr, {code, source, message}};
}

bool valid_limits(const NativeCompileLimits& l) noexcept {
  return l.max_source_bytes && l.max_token_bytes && l.max_tokens &&
         l.max_rules && l.max_variant_depth && l.max_theme_tokens &&
         l.max_theme_name_bytes && l.max_program_bytes;
}

bool valid_name(std::string_view name) noexcept {
  if (name.empty() || !letter(name.front()) ||
      name.back() == '-' || name.back() == '_') return false;
  for (char c : name) {
    if (!letter(c) && !digit(c) && c != '-' && c != '_') return false;
  }
  return name != "inherit" && name != "current" && name != "transparent";
}

bool valid_color(const flex::Color& color) noexcept {
  for (float value : {color.r, color.g, color.b, color.a}) {
    if (!std::isfinite(value) || value < 0.0f || value > 1.0f) return false;
  }
  return true;
}

struct Token {
  std::string_view value;
  NativeSourceRange source;
};

// Scan before interpreting variants. Colons inside arbitrary values/selectors
// are not variant separators, even when that syntax is outside this profile.
NativeError split_candidate(std::string_view token,
                            std::vector<std::string_view>& parts) {
  std::vector<char> closing;
  char quote = 0;
  bool escaped = false;
  std::size_t begin = 0;
  for (std::size_t i = 0; i < token.size(); ++i) {
    const char c = token[i];
    const auto byte = static_cast<unsigned char>(c);
    if (byte < 0x20 || byte == 0x7f) return NativeError::MalformedSyntax;
    if (escaped) { escaped = false; continue; }
    if (c == '\\') { escaped = true; continue; }
    if (quote) {
      if (c == quote) quote = 0;
      continue;
    }
    if (c == '\'' || c == '"') {
      if (closing.empty()) return NativeError::MalformedSyntax;
      quote = c;
    } else if (c == '[' || c == '(' || c == '{') {
      closing.push_back(c == '[' ? ']' : c == '(' ? ')' : '}');
    } else if (c == ']' || c == ')' || c == '}') {
      if (closing.empty() || closing.back() != c)
        return NativeError::MalformedSyntax;
      closing.pop_back();
    } else if (c == ':' && closing.empty()) {
      if (i == begin) return NativeError::MalformedSyntax;
      parts.push_back(token.substr(begin, i - begin));
      begin = i + 1;
    }
  }
  if (quote || escaped || !closing.empty() || begin == token.size())
    return NativeError::MalformedSyntax;
  parts.push_back(token.substr(begin));
  return NativeError::None;
}

// Locale-independent finite decimal grammar, including an optional exponent.
// This never accepts a numeric prefix followed by units or other trailing text.
bool decimal(std::string_view text, double& result) {
  if (text.empty()) return false;
  std::size_t i = 0;
  bool negative = false;
  if (text[i] == '+' || text[i] == '-') {
    negative = text[i] == '-';
    if (++i == text.size()) return false;
  }
  bool has_digit = false;
  bool nonzero = false;
  while (i < text.size() && digit(text[i])) {
    has_digit = true;
    nonzero = nonzero || text[i] != '0';
    ++i;
  }
  if (i < text.size() && text[i] == '.') {
    ++i;
    while (i < text.size() && digit(text[i])) {
      has_digit = true;
      nonzero = nonzero || text[i] != '0';
      ++i;
    }
  }
  if (!has_digit) return false;
  if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
    ++i;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) {
      ++i;
    }
    if (i == text.size() || !digit(text[i])) return false;
    while (i < text.size() && digit(text[i])) {
      ++i;
    }
  }
  if (i != text.size()) return false;
  if (!nonzero) { result = negative ? -0.0 : 0.0; return true; }

  // Convert the complete value, not an unscaled mantissa: long double has
  // different ranges on MSVC and GCC. The classic locale fixes the decimal
  // point without changing process-global locale or depending on floating
  // from_chars support in older supported standard libraries.
  std::istringstream input{std::string(text)};
  input.imbue(std::locale::classic());
  input >> std::noskipws >> result;
  return input && input.eof() && std::isfinite(result) && result != 0.0;
}

// The pinned upstream bare-opacity grammar requires canonical quarter steps.
// Arbitrary [...] numbers have their own grammar and need not be canonical.
bool canonical_quarter(std::string_view text) noexcept {
  const auto dot = text.find('.');
  const auto integer = text.substr(0, dot);
  if (integer.empty() || (integer.size() > 1 && integer.front() == '0'))
    return false;
  for (char c : integer) if (!digit(c)) return false;
  if (dot == std::string_view::npos) return true;
  const auto fraction = text.substr(dot + 1);
  return fraction == "25" || fraction == "5" || fraction == "75";
}

NativeError opacity(std::string_view text, float& value) {
  if (text.empty()) return NativeError::MalformedSyntax;
  const bool arbitrary = text.front() == '[';
  double scale = 0.01;
  if (arbitrary) {
    if (text.size() < 3 || text.back() != ']')
      return NativeError::MalformedSyntax;
    text = text.substr(1, text.size() - 2);
    scale = 1.0;
    if (text.back() == '%') { text.remove_suffix(1); scale = 0.01; }
    if (starts(text, "var(") || starts(text, "calc(") ||
        starts(text, "--") || text.find(':') != std::string_view::npos)
      return NativeError::UnsupportedValue;
  } else if (!canonical_quarter(text)) {
    return NativeError::UnsupportedValue;
  }
  double parsed = 0.0;
  if (!decimal(text, parsed) || parsed < 0.0 ||
      parsed > (scale == 1.0 ? 1.0 : 100.0))
    return NativeError::InvalidValue;
  value = static_cast<float>(parsed * scale);
  if (parsed != 0.0 && value == 0.0f) return NativeError::InvalidValue;
  return NativeError::None;
}

int hex_digit(char c) noexcept {
  if (digit(c)) return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

bool hex_color(std::string_view text, flex::Color& value) noexcept {
  if (text.empty() || text.front() != '#') return false;
  text.remove_prefix(1);
  const bool short_form = text.size() == 3 || text.size() == 4;
  if (!short_form && text.size() != 6 && text.size() != 8) return false;
  unsigned channels[4] = {0, 0, 0, 255};
  const std::size_t count = short_form ? text.size() : text.size() / 2;
  for (std::size_t i = 0; i < count; ++i) {
    const int high = hex_digit(text[i * (short_form ? 1 : 2)]);
    const int low = short_form ? high : hex_digit(text[i * 2 + 1]);
    if (high < 0 || low < 0) return false;
    channels[i] = static_cast<unsigned>(high * 16 + low);
  }
  value = flex::Color(channels[0] / 255.0f, channels[1] / 255.0f,
                      channels[2] / 255.0f, channels[3] / 255.0f);
  return true;
}

using ThemeColors = std::map<std::string_view, const flex::Color*>;

NativeError color_value(std::string_view text, const ThemeColors& colors,
                        flex::Color& value) {
  std::size_t slash = std::string_view::npos;
  unsigned bracket_depth = 0;
  for (std::size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '[') ++bracket_depth;
    if (text[i] == ']') --bracket_depth;  // Structure already validated.
    if (text[i] == '/' && !bracket_depth) {
      if (slash != std::string_view::npos) return NativeError::MalformedSyntax;
      slash = i;
    }
  }
  const auto color = text.substr(0, slash);
  if (color.empty()) return NativeError::MalformedSyntax;
  if (color.front() == '[') {
    if (color.size() < 3 || color.back() != ']')
      return NativeError::MalformedSyntax;
    const auto literal = color.substr(1, color.size() - 2);
    if (literal.front() != '#') return NativeError::UnsupportedValue;
    if (!hex_color(literal, value)) return NativeError::InvalidValue;
  } else if (color == "transparent") {
    value = flex::Color(0.0f, 0.0f, 0.0f, 0.0f);
  } else {
    if (!valid_name(color)) return NativeError::UnsupportedValue;
    const auto it = colors.find(color);
    if (it == colors.end()) return NativeError::UnknownThemeToken;
    value = *it->second;
  }
  if (slash != std::string_view::npos) {
    float alpha = 1.0f;
    const auto error = opacity(text.substr(slash + 1), alpha);
    if (error != NativeError::None) return error;
    value.a *= alpha;
  }
  return NativeError::None;
}

std::uint16_t variant_bit(std::string_view name) noexcept {
  if (name == "focus-within") return NativeFocusWithin;
  if (name == "hover") return NativeHover;
  if (name == "focus") return NativeFocus;
  if (name == "focus-visible") return NativeFocusVisible;
  if (name == "active") return NativeActive;
  if (name == "disabled") return NativeDisabled;
  if (name == "dark") return NativeDark;
  return 0;
}

// Pinned upstream also recognizes static utilities and longer functional roots
// in these families. A same-named Theme color cannot silently erase their other
// declarations. Reserve those spellings in this limited paint profile.
bool unsupported_color_utility(std::string_view utility) noexcept {
  const auto base = utility.substr(0, utility.find('/'));
  constexpr std::string_view static_utilities[] = {
      "bg-auto", "bg-cover", "bg-contain", "bg-fixed", "bg-local", "bg-scroll",
      "bg-top", "bg-top-left", "bg-top-right", "bg-bottom", "bg-bottom-left",
      "bg-bottom-right", "bg-left", "bg-right", "bg-center", "bg-repeat",
      "bg-no-repeat", "bg-repeat-x", "bg-repeat-y", "bg-repeat-round",
      "bg-repeat-space", "bg-none", "bg-clip-text", "bg-clip-border",
      "bg-clip-padding", "bg-clip-content", "bg-origin-border",
      "bg-origin-padding", "bg-origin-content", "bg-blend-normal",
      "bg-blend-multiply", "bg-blend-screen", "bg-blend-overlay",
      "bg-blend-darken", "bg-blend-lighten", "bg-blend-color-dodge",
      "bg-blend-color-burn", "bg-blend-hard-light", "bg-blend-soft-light",
      "bg-blend-difference", "bg-blend-exclusion", "bg-blend-hue",
      "bg-blend-saturation", "bg-blend-color", "bg-blend-luminosity",
      "bg-left-top", "bg-right-top", "bg-left-bottom", "bg-right-bottom",
      "bg-gradient-to-t", "bg-gradient-to-tr", "bg-gradient-to-r",
      "bg-gradient-to-br", "bg-gradient-to-b", "bg-gradient-to-bl",
      "bg-gradient-to-l", "bg-gradient-to-tl", "text-ellipsis", "text-clip",
      "text-wrap", "text-nowrap", "text-balance", "text-pretty", "text-left",
      "text-center", "text-right", "text-justify", "text-start", "text-end",
      "text-shadow-initial", "border-collapse", "border-separate",
      "border-solid", "border-dashed", "border-dotted", "border-double",
      "border-hidden", "border-none"};
  for (auto reserved : static_utilities) if (base == reserved) return true;
  constexpr std::string_view roots[] = {
      "bg-size", "bg-position", "bg-linear", "bg-conic", "bg-radial",
      "text-shadow", "border-spacing", "border-x", "border-y", "border-s",
      "border-e", "border-t", "border-r", "border-b", "border-l"};
  for (auto root : roots) {
    if (base == root || (starts(base, root) && base.size() > root.size() &&
                         base[root.size()] == '-')) return true;
  }
  return false;
}

NativeError compile_rule(const Token& token, const ThemeColors& colors,
                          const NativeCompileLimits& limits, NativeRule& rule) {
  auto input = token.value;
  if (input.back() == '!') { rule.important = true; input.remove_suffix(1); }
  if (input.empty()) return NativeError::MalformedSyntax;
  std::vector<std::string_view> parts;
  const auto syntax = split_candidate(input, parts);
  if (syntax != NativeError::None) return syntax;
  if (parts.size() - 1 > limits.max_variant_depth)
    return NativeError::ResourceLimit;
  for (std::size_t i = 0; i + 1 < parts.size(); ++i) {
    const auto bit = variant_bit(parts[i]);
    if (!bit || (rule.condition.required & bit))
      return NativeError::UnsupportedVariant;
    rule.condition.required |= bit;
    if (bit != NativeDark) ++rule.specificity;
  }
  const auto utility = parts.back();
  if (unsupported_color_utility(utility)) return NativeError::UnsupportedUtility;
  NativeError error;
  if (starts(utility, "opacity-")) {
    float value = 0.0f;
    error = opacity(utility.substr(8), value);
    rule.declaration = {Property::Opacity, value};
  } else {
    std::string_view value;
    if (starts(utility, "bg-")) {
      rule.declaration.property = Property::BackgroundColor;
      value = utility.substr(3);
    } else if (starts(utility, "text-")) {
      rule.declaration.property = Property::TextColor;
      value = utility.substr(5);
    } else if (starts(utility, "border-")) {
      rule.declaration.property = Property::BorderColor;
      value = utility.substr(7);
    } else {
      return NativeError::UnsupportedUtility;
    }
    flex::Color color;
    error = color_value(value, colors, color);
    rule.declaration.value = color;
  }
  if (error != NativeError::None) return error;
  rule.token.assign(token.value.data(), token.value.size());
  rule.source = token.source;
  return NativeError::None;
}

// Pinned upstream compares digit runs numerically, then their spelling if the
// numeric values tie. Digit-only strtod is independent of the decimal locale.
bool natural_less(std::string_view a, std::string_view b) {
  std::size_t i = 0, j = 0;
  while (i < a.size() && j < b.size()) {
    if (digit(a[i]) && digit(b[j])) {
      const auto begin_a = i, begin_b = j;
      while (i < a.size() && digit(a[i])) ++i;
      while (j < b.size() && digit(b[j])) ++j;
      const auto run_a = a.substr(begin_a, i - begin_a);
      const auto run_b = b.substr(begin_b, j - begin_b);
      const double number_a = std::strtod(std::string(run_a).c_str(), nullptr);
      const double number_b = std::strtod(std::string(run_b).c_str(), nullptr);
      if (number_a != number_b) return number_a < number_b;
      if (run_a != run_b) return run_a < run_b;
    } else {
      if (a[i] != b[j])
        return static_cast<unsigned char>(a[i]) < static_cast<unsigned char>(b[j]);
      ++i;
      ++j;
    }
  }
  return a.size() < b.size();
}

bool rule_less(const NativeRule& a, const NativeRule& b) {
  if (a.important != b.important) return !a.important;
  if (a.specificity != b.specificity) return a.specificity < b.specificity;
  if (a.condition.required != b.condition.required)
    return a.condition.required < b.condition.required;
  if (a.declaration.property != b.declaration.property)
    return a.declaration.property < b.declaration.property;
  return natural_less(a.token, b.token);
}

bool account(std::size_t& bytes, std::size_t addition, std::size_t limit) noexcept {
  if (bytes > limit || addition > limit - bytes) return false;
  bytes += addition;
  return true;
}

}  // namespace

bool NativeCondition::matches(const NativeState& state) const noexcept {
  std::uint16_t active = 0;
  if (state.focus_within) active |= NativeFocusWithin;
  if (state.hover && state.hover_capable) active |= NativeHover;
  if (state.focus) active |= NativeFocus;
  if (state.focus_visible) active |= NativeFocusVisible;
  if (state.active) active |= NativeActive;
  if (state.disabled) active |= NativeDisabled;
  if (state.dark) active |= NativeDark;
  return (active & required) == required;
}

NativeCompileResult compile_native_class_list(std::string_view classes,
                                             const NativeTheme& theme,
                                             const NativeCompileLimits& limits) {
  try {
    if (!valid_limits(limits))
      return failure(NativeError::InvalidLimits, {}, "limits must be non-zero");
    if (classes.size() > limits.max_source_bytes ||
        theme.colors.size() > limits.max_theme_tokens)
      return failure(NativeError::ResourceLimit, {}, "source or theme count limit");

    ThemeColors colors;
    for (const auto& token : theme.colors) {
      if (token.name.size() > limits.max_theme_name_bytes)
        return failure(NativeError::ResourceLimit, {}, "theme name limit");
      if (!valid_name(token.name) || !valid_color(token.value) ||
          !colors.emplace(token.name, &token.value).second)
        return failure(NativeError::InvalidTheme, {}, "invalid or duplicate color token");
    }

    std::vector<Token> tokens;
    for (std::size_t i = 0; i < classes.size();) {
      if (space(classes[i])) { ++i; continue; }
      const auto begin = i;
      while (i < classes.size() && !space(classes[i])) ++i;
      const NativeSourceRange source{begin, i - begin};
      if (source.length > limits.max_token_bytes || tokens.size() >= limits.max_tokens)
        return failure(NativeError::ResourceLimit, source, "token length or count limit");
      tokens.push_back({classes.substr(begin, source.length), source});
    }

    // Keep the first source location for an exact duplicate without keeping a
    // global token cache or borrowing the caller's input after compilation.
    std::sort(tokens.begin(), tokens.end(), [](const Token& a, const Token& b) {
      return a.value != b.value ? a.value < b.value : a.source.offset < b.source.offset;
    });
    tokens.erase(std::unique(tokens.begin(), tokens.end(), [](const Token& a, const Token& b) {
      return a.value == b.value;
    }), tokens.end());
    if (tokens.size() > limits.max_rules)
      return failure(NativeError::ResourceLimit, {}, "rule count limit");

    std::size_t estimate = sizeof(NativeProgram);
    if (estimate > limits.max_program_bytes ||
        tokens.size() > (limits.max_program_bytes - estimate) / sizeof(NativeRule))
      return failure(NativeError::ResourceLimit, {}, "program object limit");
    estimate += tokens.size() * sizeof(NativeRule);
    for (const auto& token : tokens) {
      if (token.value.size() == std::numeric_limits<std::size_t>::max() ||
          !account(estimate, token.value.size() + 1, limits.max_program_bytes))
        return failure(NativeError::ResourceLimit, token.source, "program token limit");
    }

    std::vector<NativeRule> rules;
    rules.reserve(tokens.size());
    for (const auto& token : tokens) {
      NativeRule rule;
      const auto error = compile_rule(token, colors, limits, rule);
      if (error != NativeError::None)
        return failure(error, token.source, "candidate outside native paint profile");
      rules.push_back(std::move(rule));
    }
    std::sort(rules.begin(), rules.end(), rule_less);

    std::size_t retained = sizeof(NativeProgram);
    if (rules.capacity() > (limits.max_program_bytes - retained) / sizeof(NativeRule))
      return failure(NativeError::ResourceLimit, {}, "program capacity limit");
    retained += rules.capacity() * sizeof(NativeRule);
    for (const auto& rule : rules) {
      if (rule.token.capacity() == std::numeric_limits<std::size_t>::max() ||
          !account(retained, rule.token.capacity() + 1, limits.max_program_bytes))
        return failure(NativeError::ResourceLimit, rule.source, "program string capacity limit");
    }
    return {std::shared_ptr<const NativeProgram>(new NativeProgram(std::move(rules), retained)), {}};
  } catch (const std::bad_alloc&) {
    return {nullptr, {NativeError::AllocationFailure, {}, {}}};
  } catch (const std::length_error&) {
    return {nullptr, {NativeError::ResourceLimit, {}, {}}};
  }
}

}  // namespace flexUI::tailwind
