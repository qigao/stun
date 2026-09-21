#include <algorithm>
#include <cctype>
#include <cmath>
#include <flexUI/box.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/textarea_widget.h>
#include <flexUI/transition.h>
#include <flexUI/detail/css_length.h>
#include <lexbor/css/css.h>
#include <lexbor/css/rule.h>
#include <lexbor/css/selectors/selectors.h>
#include <lexbor/css/stylesheet.h>
#include <map>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <flexUI/style_engine.h>
namespace flexUI {

namespace {

const Symbol& transition_property_var() {
  static const Symbol sym("__flex_transition_property");
  return sym;
}

const Symbol& transition_duration_var() {
  static const Symbol sym("__flex_transition_duration");
  return sym;
}

const Symbol& transition_delay_var() {
  static const Symbol sym("__flex_transition_delay");
  return sym;
}

const Symbol& transition_timing_var() {
  static const Symbol sym("__flex_transition_timing");
  return sym;
}

const Symbol& background_gradient_type_var() {
  static const Symbol sym("__flex_background_gradient_type");
  return sym;
}

const Symbol& background_radial_position_var() {
  static const Symbol sym("__flex_background_radial_position");
  return sym;
}

const Symbol& background_radial_size_var() {
  static const Symbol sym("__flex_background_radial_size");
  return sym;
}

const Symbol& background_position_var() {
  static const Symbol sym("__flex_background_position");
  return sym;
}

const Symbol& background_position_x_var() {
  static const Symbol sym("__flex_background_position_x");
  return sym;
}

const Symbol& background_position_y_var() {
  static const Symbol sym("__flex_background_position_y");
  return sym;
}

const Symbol& background_size_var() {
  static const Symbol sym("__flex_background_size");
  return sym;
}

const Symbol& background_repeat_var() {
  static const Symbol sym("__flex_background_repeat");
  return sym;
}

const Symbol& background_origin_var() {
  static const Symbol sym("__flex_background_origin");
  return sym;
}

const Symbol& filter_drop_shadow_offset_x_var() {
  static const Symbol sym("__flex_filter_drop_shadow_offset_x");
  return sym;
}

const Symbol& filter_drop_shadow_offset_y_var() {
  static const Symbol sym("__flex_filter_drop_shadow_offset_y");
  return sym;
}

const Symbol& filter_drop_shadow_blur_var() {
  static const Symbol sym("__flex_filter_drop_shadow_blur");
  return sym;
}

const Symbol& filter_drop_shadow_color_var() {
  static const Symbol sym("__flex_filter_drop_shadow_color");
  return sym;
}

const Symbol& filter_opacity_var() {
  static const Symbol sym("__flex_filter_opacity");
  return sym;
}

bool is_internal_transition_var(Symbol name) {
  return name == transition_property_var() ||
         name == transition_duration_var() ||
         name == transition_delay_var() ||
         name == transition_timing_var() ||
         name == background_gradient_type_var() ||
         name == background_radial_position_var() ||
         name == background_radial_size_var() ||
         name == background_position_var() ||
         name == background_position_x_var() ||
         name == background_position_y_var() ||
         name == background_size_var() ||
         name == background_repeat_var() ||
         name == background_origin_var() ||
         name == filter_drop_shadow_offset_x_var() ||
         name == filter_drop_shadow_offset_y_var() ||
         name == filter_drop_shadow_blur_var() ||
         name == filter_drop_shadow_color_var() ||
         name == filter_opacity_var();
}

bool is_non_inherited_property_projection(Symbol name) {
  static const Symbol names[] = {
      Symbol("aspect-ratio"),          Symbol("min-width"),
      Symbol("max-width"),             Symbol("min-height"),
      Symbol("max-height"),            Symbol("justify-content"),
      Symbol("align-items"),           Symbol("align-content"),
      Symbol("align-self"),            Symbol("justify-items"),
      Symbol("justify-self"),          Symbol("flex-grow"),
      Symbol("flex-shrink"),           Symbol("flex-basis"),
      Symbol("flex-wrap"),             Symbol("grid-template-columns"),
      Symbol("grid-template-rows"),    Symbol("grid-template-areas"),
      Symbol("grid-auto-flow"),        Symbol("grid-auto-columns"),
      Symbol("grid-auto-rows"),        Symbol("grid-column"),
      Symbol("grid-row"),              Symbol("grid-area"),
      Symbol("row-gap"),               Symbol("column-gap"),
      Symbol("container-type"),        Symbol("container-name"),
  };
  return std::find(std::begin(names), std::end(names), name) !=
         std::end(names);
}

bool ends_with_copy(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string transition_trim_copy(const std::string& value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }

  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }

  return value.substr(start, end - start);
}

float gradient_direction_to_angle(bool to_top, bool to_bottom, bool to_left,
                                  bool to_right) {
  const bool has_vertical = to_top || to_bottom;
  const bool has_horizontal = to_left || to_right;
  if (has_vertical && has_horizontal) {
    if (to_top && to_right) return 45.0f;
    if (to_bottom && to_right) return 135.0f;
    if (to_bottom && to_left) return 225.0f;
    if (to_top && to_left) return 315.0f;
  }
  if (to_top) return 0.0f;
  if (to_right) return 90.0f;
  if (to_bottom) return 180.0f;
  if (to_left) return 270.0f;
  return 180.0f;
}

std::vector<std::string> split_top_level_csv(const std::string& value) {
  std::vector<std::string> items;
  std::string current;
  int paren_depth = 0;

  for (char ch : value) {
    if (ch == '(') {
      ++paren_depth;
      current.push_back(ch);
      continue;
    }
    if (ch == ')') {
      if (paren_depth > 0) {
        --paren_depth;
      }
      current.push_back(ch);
      continue;
    }
    if (ch == ',' && paren_depth == 0) {
      const std::string item = transition_trim_copy(current);
      if (!item.empty()) {
        items.push_back(item);
      }
      current.clear();
      continue;
    }
    current.push_back(ch);
  }

  const std::string tail = transition_trim_copy(current);
  if (!tail.empty()) {
    items.push_back(tail);
  }
  return items;
}

std::vector<std::string> split_css_tokens(const std::string& value) {
  std::vector<std::string> tokens;
  std::string current;
  int paren_depth = 0;

  for (char ch : value) {
    if (ch == '(') {
      ++paren_depth;
    } else if (ch == ')' && paren_depth > 0) {
      --paren_depth;
    }

    if (std::isspace(static_cast<unsigned char>(ch)) && paren_depth == 0) {
      const std::string token = transition_trim_copy(current);
      if (!token.empty()) {
        tokens.push_back(token);
      }
      current.clear();
      continue;
    }

    current.push_back(ch);
  }

  const std::string tail = transition_trim_copy(current);
  if (!tail.empty()) {
    tokens.push_back(tail);
  }
  return tokens;
}

std::vector<std::string> split_background_shorthand_tokens(
    const std::string& value) {
  std::vector<std::string> tokens;
  std::string current;
  int paren_depth = 0;

  for (char ch : value) {
    if (ch == '(') {
      ++paren_depth;
    } else if (ch == ')' && paren_depth > 0) {
      --paren_depth;
    }

    if (paren_depth == 0 && ch == '/') {
      const std::string token = transition_trim_copy(current);
      if (!token.empty()) {
        tokens.push_back(token);
      }
      tokens.push_back("/");
      current.clear();
      continue;
    }

    if (std::isspace(static_cast<unsigned char>(ch)) && paren_depth == 0) {
      const std::string token = transition_trim_copy(current);
      if (!token.empty()) {
        tokens.push_back(token);
      }
      current.clear();
      continue;
    }

    current.push_back(ch);
  }

  const std::string tail = transition_trim_copy(current);
  if (!tail.empty()) {
    tokens.push_back(tail);
  }
  return tokens;
}

bool is_numeric_token(const std::string& value) {
  if (value.empty()) {
    return false;
  }

  char* end = nullptr;
  std::strtof(value.c_str(), &end);
  return end != value.c_str() && end && *end == '\0';
}

bool is_length_or_percentage_token(const std::string& value) {
  return is_numeric_token(value) || detail::is_css_length_token(value);
}

bool is_background_repeat_token(const std::string& value) {
  return value == "repeat" || value == "no-repeat" || value == "repeat-x" ||
         value == "repeat-y" || value == "space" || value == "round";
}

bool is_background_box_token(const std::string& value) {
  return value == "border-box" || value == "padding-box" ||
         value == "content-box";
}

bool is_background_position_token(const std::string& value) {
  return value == "left" || value == "right" || value == "top" ||
         value == "bottom" || value == "center" ||
         is_length_or_percentage_token(value);
}

bool is_background_size_token(const std::string& value) {
  return value == "auto" || value == "cover" || value == "contain" ||
         is_length_or_percentage_token(value);
}

bool is_background_url_token(const std::string& value) {
  return value.rfind("url(", 0) == 0 && !value.empty() && value.back() == ')';
}

bool is_background_image_token(const std::string& value) {
  return value == "none" || is_background_url_token(value) ||
         value.rfind("linear-gradient(", 0) == 0 ||
         value.rfind("radial-gradient(", 0) == 0;
}

std::string join_space_separated_tokens(const std::vector<std::string>& tokens) {
  std::string joined;
  for (const auto& token : tokens) {
    if (!joined.empty()) {
      joined.push_back(' ');
    }
    joined += token;
  }
  return joined;
}

bool is_background_horizontal_position_token(const std::string& value) {
  return value == "left" || value == "right";
}

bool is_background_vertical_position_token(const std::string& value) {
  return value == "top" || value == "bottom";
}

std::pair<std::string, std::string>
split_background_position_axes(const std::string& value) {
  std::string normalized = transition_trim_copy(value);
  std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  const auto tokens = split_css_tokens(normalized);
  std::string x_axis = "center";
  std::string y_axis = "center";
  if (tokens.empty()) {
    return {x_axis, y_axis};
  }
  if (tokens.size() == 1) {
    if (is_background_vertical_position_token(tokens[0])) {
      y_axis = tokens[0];
    } else {
      x_axis = tokens[0];
    }
    return {x_axis, y_axis};
  }
  if (tokens.size() == 2) {
    if (is_background_vertical_position_token(tokens[0]) &&
        (is_background_horizontal_position_token(tokens[1]) ||
         tokens[1] == "center")) {
      x_axis = tokens[1];
      y_axis = tokens[0];
    } else {
      x_axis = tokens[0];
      y_axis = tokens[1];
    }
    return {x_axis, y_axis};
  }

  bool has_x = false;
  bool has_y = false;
  std::vector<std::string> x_tokens;
  std::vector<std::string> y_tokens;
  for (size_t i = 0; i < tokens.size(); ++i) {
    const auto& token = tokens[i];
    if (is_background_horizontal_position_token(token)) {
      x_tokens = {token};
      has_x = true;
      if (i + 1 < tokens.size() && is_length_or_percentage_token(tokens[i + 1])) {
        x_tokens.push_back(tokens[++i]);
      }
      continue;
    }
    if (is_background_vertical_position_token(token)) {
      y_tokens = {token};
      has_y = true;
      if (i + 1 < tokens.size() && is_length_or_percentage_token(tokens[i + 1])) {
        y_tokens.push_back(tokens[++i]);
      }
      continue;
    }
    if (token == "center") {
      if (!has_x) {
        x_tokens = {token};
        has_x = true;
      } else {
        y_tokens = {token};
        has_y = true;
      }
      continue;
    }
    if (!has_x) {
      x_tokens = {token};
      has_x = true;
    } else if (!has_y) {
      y_tokens = {token};
      has_y = true;
    }
  }
  if (!x_tokens.empty()) {
    x_axis = join_space_separated_tokens(x_tokens);
  }
  if (!y_tokens.empty()) {
    y_axis = join_space_separated_tokens(y_tokens);
  }
  return {x_axis, y_axis};
}

std::string compose_background_position_axes(const std::string& x_axis,
                                             const std::string& y_axis) {
  const std::string x =
      transition_trim_copy(x_axis.empty() ? std::string("center") : x_axis);
  const std::string y =
      transition_trim_copy(y_axis.empty() ? std::string("center") : y_axis);
  return x + " " + y;
}

std::string merge_background_position_axis(const std::string& current,
                                           const std::string& axis_value,
                                           bool horizontal) {
  auto axes = split_background_position_axes(current);
  if (horizontal) {
    axes.first = transition_trim_copy(axis_value);
  } else {
    axes.second = transition_trim_copy(axis_value);
  }
  return compose_background_position_axes(axes.first, axes.second);
}

std::vector<std::string> split_transform_arguments(const std::string& value) {
  auto args = split_top_level_csv(value);
  if (args.size() > 1) {
    return args;
  }
  return split_css_tokens(value);
}

std::string transition_time_to_css(float value_ms) {
  std::ostringstream out;
  const float rounded = std::round(value_ms);
  if (std::fabs(value_ms - rounded) < 0.001f) {
    out << static_cast<int>(rounded);
  } else {
    out.setf(std::ios::fixed);
    out.precision(3);
    out << value_ms;
    std::string text = out.str();
    while (!text.empty() && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
    return text + "ms";
  }
  return out.str() + "ms";
}

std::string easing_to_css(EasingType easing) {
  switch (easing) {
  case EasingType::Linear:
    return "linear";
  case EasingType::Ease:
    return "ease";
  case EasingType::EaseIn:
    return "ease-in";
  case EasingType::EaseOut:
    return "ease-out";
  case EasingType::EaseInOut:
    return "ease-in-out";
  default:
    return "ease";
  }
}

std::string rebuild_transition_shorthand(const ComputedStyle* style) {
  auto base_defs = style->transition.empty()
                       ? std::vector<TransitionDef>{TransitionDef{}}
                       : parse_transition_list(style->transition);
  if (base_defs.empty()) {
    base_defs.push_back(TransitionDef{});
  }

  const auto property_it = style->variables.find(transition_property_var());
  const auto duration_it = style->variables.find(transition_duration_var());
  const auto delay_it = style->variables.find(transition_delay_var());
  const auto timing_it = style->variables.find(transition_timing_var());

  const auto property_items =
      property_it == style->variables.end()
          ? std::vector<std::string>{}
          : split_top_level_csv(property_it->second);
  const auto duration_items =
      duration_it == style->variables.end()
          ? std::vector<std::string>{}
          : split_top_level_csv(duration_it->second);
  const auto delay_items =
      delay_it == style->variables.end()
          ? std::vector<std::string>{}
          : split_top_level_csv(delay_it->second);
  const auto timing_items =
      timing_it == style->variables.end()
          ? std::vector<std::string>{}
          : split_top_level_csv(timing_it->second);

  const size_t count =
      !property_items.empty() ? property_items.size() : base_defs.size();

  std::ostringstream out;
  for (size_t i = 0; i < count; ++i) {
    if (i > 0) {
      out << ", ";
    }

    const auto& base = base_defs[i % base_defs.size()];
    const std::string property =
        !property_items.empty() ? property_items[i % property_items.size()]
                                : base.property;
    const std::string duration =
        !duration_items.empty()
            ? duration_items[i % duration_items.size()]
            : transition_time_to_css(base.duration_ms);
    const std::string timing =
        !timing_items.empty() ? timing_items[i % timing_items.size()]
                              : easing_to_css(base.easing);
    const std::string delay =
        !delay_items.empty() ? delay_items[i % delay_items.size()]
                             : transition_time_to_css(base.delay_ms);

    out << property << ' ' << duration << ' ' << timing << ' ' << delay;
  }

  return out.str();
}

} // namespace

class LexborCSSParser {
public:
  LexborCSSParser() {
    parser_ = lxb_css_parser_create();
    if (!parser_)
      throw std::runtime_error("Failed to create lexbor");
    if (lxb_css_parser_init(parser_, nullptr) != LXB_STATUS_OK) {
      lxb_css_parser_destroy(parser_, true);
      throw std::runtime_error("Failed to init lexbor");
    }
  }
  ~LexborCSSParser() {
    if (parser_)
      lxb_css_parser_destroy(parser_, true);
  }
  LexborCSSParser(const LexborCSSParser &) = delete;
  LexborCSSParser &operator=(const LexborCSSParser &) = delete;
  lxb_css_stylesheet_t *parse(const char *css, size_t len) {
    auto *sheet = lxb_css_stylesheet_create(nullptr);
    if (!sheet)
      throw std::runtime_error("Failed to create CSS stylesheet");
    const auto status = lxb_css_stylesheet_parse(
        sheet, parser_, reinterpret_cast<const lxb_char_t *>(css), len);
    if (status != LXB_STATUS_OK) {
      lxb_css_stylesheet_destroy(sheet, true);
      throw std::runtime_error("Failed to parse CSS");
    }
    return sheet;
  }
  lxb_css_log_t* log() const { return lxb_css_parser_log(parser_); }

private:
  lxb_css_parser_t *parser_ = nullptr;
};

struct ParsedSelector {
  struct AttributeSelector {
    Symbol name;
    std::string value;
    bool has_value = false;
  };

  Symbol type, id;
  std::unordered_set<Symbol, SymbolHash> classes;
  std::vector<AttributeSelector> attributes;
  std::vector<Symbol> pseudo_classes;
  std::vector<std::string> pseudo_functions;
  std::vector<std::vector<std::string>> is_groups;
  std::vector<std::vector<std::string>> has_groups;
  std::vector<std::vector<std::string>> where_groups;
  std::vector<std::vector<std::string>> not_groups;
};

enum class SelectorCombinator {
  None,
  Descendant,
  Child,
  AdjacentSibling,
  GeneralSibling,
};

struct SelectorStep {
  ParsedSelector selector;
  SelectorCombinator combinator_to_left = SelectorCombinator::None;
};

struct ComplexSelector {
  std::vector<SelectorStep> steps;
};

enum class MediaConstraintType {
  MinWidth,
  MinWidthExclusive,
  MaxWidth,
  MaxWidthExclusive,
  MinHeight,
  MinHeightExclusive,
  MaxHeight,
  MaxHeightExclusive,
  MinAspectRatio,
  MinAspectRatioExclusive,
  MaxAspectRatio,
  MaxAspectRatioExclusive,
  OrientationPortrait,
  OrientationLandscape,
  HoverHover,
  HoverNone,
  AnyHoverHover,
  AnyHoverNone,
  PointerNone,
  PointerCoarse,
  PointerFine,
  AnyPointerNone,
  AnyPointerCoarse,
  AnyPointerFine,
  PrefersReducedMotionReduce,
  PrefersReducedMotionNoPreference,
  PrefersColorSchemeDark,
  PrefersColorSchemeLight,
  PrefersContrastMore,
  PrefersContrastLess,
  PrefersContrastNoPreference,
  ForcedColorsActive,
  ForcedColorsNone,
};

struct MediaConstraint {
  MediaConstraintType type = MediaConstraintType::MinWidth;
  float value = 0.0f;
};

struct MediaQuery {
  std::vector<MediaConstraint> constraints;
  bool always_match = false;
};

struct MediaQueryList {
  std::vector<MediaQuery> queries;
};

enum class ContainerConstraintType {
  MinWidth,
  MinWidthExclusive,
  MaxWidth,
  MaxWidthExclusive,
};

struct ContainerConstraint {
  ContainerConstraintType type = ContainerConstraintType::MinWidth;
  float value = 0.0f;
};

struct ContainerQuery {
  std::string name;
  std::vector<ContainerConstraint> constraints;
  bool always_match = false;
};

struct ContainerQueryList {
  std::vector<ContainerQuery> queries;
};

enum class ContainerType {
  None,
  InlineSize,
};

struct ContainerRegistration {
  ContainerType type = ContainerType::None;
  std::vector<std::string> names;
};

struct CSSDeclaration {
  std::string property;
  std::string value;
  bool important = false;
  uint64_t source_order = 0;
};

using DeclarationList = std::vector<CSSDeclaration>;

struct CSSRule {
  ComplexSelector selector;
  DeclarationList properties;
  uint32_t specificity;
  std::vector<MediaQueryList> media_conditions;
  std::vector<ContainerQueryList> container_conditions;
};

static size_t find_top_level_colon(const std::string& value) {
  int paren_depth = 0;
  int bracket_depth = 0;
  for (size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if (ch == '(') {
      ++paren_depth;
    } else if (ch == ')' && paren_depth > 0) {
      --paren_depth;
    } else if (ch == '[') {
      ++bracket_depth;
    } else if (ch == ']' && bracket_depth > 0) {
      --bracket_depth;
    } else if (ch == ':' && paren_depth == 0 && bracket_depth == 0) {
      return i;
    }
  }
  return std::string::npos;
}

static std::string trim_copy(const std::string &value) {
  const size_t start = value.find_first_not_of(" \t\n\r");
  if (start == std::string::npos)
    return "";
  const size_t end = value.find_last_not_of(" \t\n\r");
  return value.substr(start, end - start + 1);
}

static std::vector<std::string> split_selector_list(const std::string &selector_text) {
  std::vector<std::string> selectors;
  std::string current;
  int paren_depth = 0;
  int bracket_depth = 0;

  bool escaped = false;
  for (char c : selector_text) {
    if (escaped) {
      current += c;
      escaped = false;
      continue;
    }
    if (c == '\\') {
      current += c;
      escaped = true;
      continue;
    }
    if (c == '(')
      paren_depth++;
    else if (c == ')' && paren_depth > 0)
      paren_depth--;
    else if (c == '[')
      bracket_depth++;
    else if (c == ']' && bracket_depth > 0)
      bracket_depth--;

    if (c == ',' && paren_depth == 0 && bracket_depth == 0) {
      auto trimmed = trim_copy(current);
      if (!trimmed.empty())
        selectors.push_back(trimmed);
      current.clear();
      continue;
    }

    current += c;
  }

  auto trimmed = trim_copy(current);
  if (!trimmed.empty())
    selectors.push_back(trimmed);
  return selectors;
}

static std::string to_lower_copy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  return value;
}

static bool is_identifier_char(char c) {
  const unsigned char uc = static_cast<unsigned char>(c);
  return std::isalnum(uc) || c == '_' || c == '-';
}

static int hex_digit_value(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

static void append_utf8_codepoint(std::string& out, uint32_t value) {
  if (value == 0) {
    return;
  }
  if (value <= 0x7F) {
    out.push_back(static_cast<char>(value));
  } else if (value <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | ((value >> 6) & 0x1F)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3F)));
  } else if (value <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | ((value >> 12) & 0x0F)));
    out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3F)));
  } else if (value <= 0x10FFFF) {
    out.push_back(static_cast<char>(0xF0 | ((value >> 18) & 0x07)));
    out.push_back(static_cast<char>(0x80 | ((value >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((value >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (value & 0x3F)));
  }
}

static size_t skip_selector_ws(const std::string& s, size_t i) {
  while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
    ++i;
  }
  return i;
}

static std::string parse_identifier_token(const std::string& s, size_t& i) {
  std::string out;
  while (i < s.size()) {
    if (is_identifier_char(s[i])) {
      out.push_back(s[i]);
      ++i;
      continue;
    }
    if (s[i] != '\\') {
      break;
    }

    ++i;
    if (i >= s.size()) {
      break;
    }

    const int first_hex = hex_digit_value(s[i]);
    if (first_hex >= 0) {
      uint32_t value = 0;
      size_t digits = 0;
      while (i < s.size() && digits < 6) {
        const int digit = hex_digit_value(s[i]);
        if (digit < 0) {
          break;
        }
        value = (value << 4) | static_cast<uint32_t>(digit);
        ++i;
        ++digits;
      }
      if (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
        ++i;
      }
      append_utf8_codepoint(out, value);
      continue;
    }

    out.push_back(s[i]);
    ++i;
  }
  return out;
}

static bool is_selector_pseudo_name(const std::string& name) {
  return name == "active" || name == "checked" || name == "disabled" ||
         name == "empty" || name == "enabled" || name == "first-child" ||
         name == "first-of-type" || name == "focus" ||
         name == "focus-visible" || name == "focus-within" ||
         name == "has" || name == "hover" || name == "indeterminate" ||
         name == "invalid" || name == "is" || name == "last-child" ||
         name == "last-of-type" || name == "modal" || name == "not" ||
         name == "nth-child" || name == "nth-of-type" ||
         name == "open" || name == "optional" || name == "placeholder" ||
         name == "placeholder-shown" || name == "read-only" ||
         name == "read-write" || name == "required" ||
         name == "selected" || name == "valid" || name == "where";
}

static std::string parse_class_selector_token(const std::string& s, size_t& i) {
  std::string out;
  int bracket_depth = 0;
  int paren_depth = 0;

  auto append_class_char = [&](char c) {
    out.push_back(c);
    if (c == '[') {
      ++bracket_depth;
    } else if (c == ']' && bracket_depth > 0) {
      --bracket_depth;
    } else if (c == '(') {
      ++paren_depth;
    } else if (c == ')' && paren_depth > 0) {
      --paren_depth;
    }
  };

  while (i < s.size()) {
    if (is_identifier_char(s[i])) {
      append_class_char(s[i]);
      ++i;
      continue;
    }

    if (s[i] == '\\') {
      ++i;
      if (i >= s.size()) {
        break;
      }

      const int first_hex = hex_digit_value(s[i]);
      if (first_hex >= 0) {
        uint32_t value = 0;
        size_t digits = 0;
        while (i < s.size() && digits < 6) {
          const int digit = hex_digit_value(s[i]);
          if (digit < 0) {
            break;
          }
          value = (value << 4) | static_cast<uint32_t>(digit);
          ++i;
          ++digits;
        }
        if (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) {
          ++i;
        }
        const size_t before = out.size();
        append_utf8_codepoint(out, value);
        if (out.size() == before + 1) {
          const char decoded = out.back();
          out.pop_back();
          append_class_char(decoded);
        }
        continue;
      }

      append_class_char(s[i]);
      ++i;
      continue;
    }

    if (s[i] == '[' && (out.empty() || out.back() == '-')) {
      append_class_char(s[i]);
      ++i;
      continue;
    }

    if (bracket_depth == 0 && paren_depth == 0 && s[i] == ':') {
      size_t name_start = i + 1;
      if (name_start < s.size() && s[name_start] == ':') {
        break;
      }
      size_t name_end = name_start;
      while (name_end < s.size() && is_identifier_char(s[name_end])) {
        ++name_end;
      }
      if (name_end == name_start ||
          is_selector_pseudo_name(s.substr(name_start, name_end - name_start))) {
        break;
      }
      append_class_char(s[i]);
      ++i;
      continue;
    }

    if (bracket_depth == 0 && paren_depth == 0) {
      break;
    }

    append_class_char(s[i]);
    ++i;
  }

  return out;
}

static std::string parse_balanced_token(const std::string& s, size_t& i,
                                        char open_ch, char close_ch) {
  if (i >= s.size() || s[i] != open_ch) {
    return "";
  }

  const size_t content_start = ++i;
  int depth = 1;
  while (i < s.size()) {
    if (s[i] == open_ch) {
      ++depth;
    } else if (s[i] == close_ch) {
      --depth;
      if (depth == 0) {
        const std::string result = s.substr(content_start, i - content_start);
        ++i;
        return result;
      }
    }
    ++i;
  }

  return "";
}

static std::string strip_quotes_copy(const std::string& value) {
  if (value.size() >= 2 &&
      ((value.front() == '"' && value.back() == '"') ||
       (value.front() == '\'' && value.back() == '\''))) {
    return value.substr(1, value.size() - 2);
  }
  return value;
}

static float parse_media_length(const std::string& value) {
  const std::string trimmed = trim_copy(to_lower_copy(value));
  if (trimmed.empty()) {
    return 0.0f;
  }

  std::string num = trimmed;
  if (num.size() >= 3 && num.substr(num.size() - 3) == "rem") {
    return std::stof(num.substr(0, num.size() - 3)) * 16.0f;
  }
  if (num.size() >= 2 && num.substr(num.size() - 2) == "em") {
    return std::stof(num.substr(0, num.size() - 2)) * 16.0f;
  }
  if (num.size() >= 2 && num.substr(num.size() - 2) == "px") {
    return std::stof(num.substr(0, num.size() - 2));
  }
  return std::stof(num);
}

static float parse_media_ratio(const std::string& value) {
  const std::string trimmed = trim_copy(to_lower_copy(value));
  if (trimmed.empty()) {
    return 0.0f;
  }

  const size_t slash = trimmed.find('/');
  if (slash == std::string::npos) {
    return std::stof(trimmed);
  }

  const float numerator = std::stof(trimmed.substr(0, slash));
  const float denominator = std::stof(trimmed.substr(slash + 1));
  if (std::fabs(denominator) <= 0.0001f) {
    return 0.0f;
  }
  return numerator / denominator;
}

static std::vector<std::string> split_media_query_terms(const std::string& query) {
  std::vector<std::string> terms;
  std::string current;
  int paren_depth = 0;
  const std::string lowered = to_lower_copy(query);

  auto flush_current = [&]() {
    const std::string trimmed = trim_copy(current);
    if (!trimmed.empty()) {
      terms.push_back(trimmed);
    }
    current.clear();
  };

  for (size_t i = 0; i < lowered.size(); ++i) {
    const char ch = lowered[i];
    if (ch == '(') {
      ++paren_depth;
      current.push_back(query[i]);
      continue;
    }
    if (ch == ')') {
      if (paren_depth > 0) {
        --paren_depth;
      }
      current.push_back(query[i]);
      continue;
    }

    if (paren_depth == 0 && lowered.compare(i, 3, "and") == 0) {
      const bool left_ok =
          i == 0 || std::isspace(static_cast<unsigned char>(lowered[i - 1]));
      const bool right_ok =
          i + 3 >= lowered.size() ||
          std::isspace(static_cast<unsigned char>(lowered[i + 3])) ||
          lowered[i + 3] == '(';
      if (left_ok && right_ok) {
        flush_current();
        i += 2;
        continue;
      }
    }

    current.push_back(query[i]);
  }

  flush_current();
  return terms;
}

static std::vector<std::string> split_media_feature_tokens(
    const std::string& value) {
  std::vector<std::string> tokens;
  std::string current;

  auto flush_current = [&]() {
    const std::string trimmed = trim_copy(current);
    if (!trimmed.empty()) {
      tokens.push_back(trimmed);
    }
    current.clear();
  };

  for (size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if (std::isspace(static_cast<unsigned char>(ch))) {
      flush_current();
      continue;
    }

    if (ch == '<' || ch == '>' || ch == '=') {
      flush_current();
      std::string op(1, ch);
      if (i + 1 < value.size() && value[i + 1] == '=') {
        op.push_back('=');
        ++i;
      }
      tokens.push_back(op);
      continue;
    }

    current.push_back(ch);
  }

  flush_current();
  return tokens;
}

static bool is_ignorable_media_term(const std::string& term) {
  const std::string lowered = trim_copy(to_lower_copy(term));
  return lowered.empty() || lowered == "all" || lowered == "screen" ||
         lowered == "only" || lowered == "only all" ||
         lowered == "only screen";
}

static bool is_range_media_feature_name(const std::string& feature) {
  return feature == "width" || feature == "height" || feature == "aspect-ratio";
}

static bool append_range_media_constraint(const std::string& feature,
                                          const std::string& op,
                                          const std::string& value,
                                          std::vector<MediaConstraint>& out) {
  MediaConstraint constraint;
  if (feature == "width") {
    if (op == ">=") {
      constraint.type = MediaConstraintType::MinWidth;
    } else if (op == ">") {
      constraint.type = MediaConstraintType::MinWidthExclusive;
    } else if (op == "<=") {
      constraint.type = MediaConstraintType::MaxWidth;
    } else if (op == "<") {
      constraint.type = MediaConstraintType::MaxWidthExclusive;
    } else {
      return false;
    }
    constraint.value = parse_media_length(value);
  } else if (feature == "height") {
    if (op == ">=") {
      constraint.type = MediaConstraintType::MinHeight;
    } else if (op == ">") {
      constraint.type = MediaConstraintType::MinHeightExclusive;
    } else if (op == "<=") {
      constraint.type = MediaConstraintType::MaxHeight;
    } else if (op == "<") {
      constraint.type = MediaConstraintType::MaxHeightExclusive;
    } else {
      return false;
    }
    constraint.value = parse_media_length(value);
  } else if (feature == "aspect-ratio") {
    if (op == ">=") {
      constraint.type = MediaConstraintType::MinAspectRatio;
    } else if (op == ">") {
      constraint.type = MediaConstraintType::MinAspectRatioExclusive;
    } else if (op == "<=") {
      constraint.type = MediaConstraintType::MaxAspectRatio;
    } else if (op == "<") {
      constraint.type = MediaConstraintType::MaxAspectRatioExclusive;
    } else {
      return false;
    }
    constraint.value = parse_media_ratio(value);
  } else {
    return false;
  }

  out.push_back(constraint);
  return true;
}

static std::string invert_range_operator(const std::string& op) {
  if (op == "<") return ">";
  if (op == "<=") return ">=";
  if (op == ">") return "<";
  if (op == ">=") return "<=";
  return "";
}

static bool parse_media_feature(const std::string& term,
                                std::vector<MediaConstraint>& out) {
  const std::string trimmed = trim_copy(term);
  if (trimmed.size() < 2 || trimmed.front() != '(' || trimmed.back() != ')') {
    return false;
  }

  const std::string inner =
      trim_copy(to_lower_copy(trimmed.substr(1, trimmed.size() - 2)));
  const size_t colon = inner.find(':');
  if (colon == std::string::npos) {
    const auto tokens = split_media_feature_tokens(inner);
    if (tokens.size() == 3 && is_range_media_feature_name(tokens[0])) {
      return append_range_media_constraint(tokens[0], tokens[1], tokens[2], out);
    }
    if (tokens.size() == 5 && is_range_media_feature_name(tokens[2])) {
      const std::string left_op = invert_range_operator(tokens[1]);
      if (left_op.empty() ||
          !append_range_media_constraint(tokens[2], left_op, tokens[0], out) ||
          !append_range_media_constraint(tokens[2], tokens[3], tokens[4], out)) {
        return false;
      }
      return true;
    }
    return false;
  }

  const std::string feature = trim_copy(inner.substr(0, colon));
  const std::string value = trim_copy(inner.substr(colon + 1));
  if (value.empty()) {
    return false;
  }

  MediaConstraint constraint;
  if (feature == "min-width") {
    constraint.type = MediaConstraintType::MinWidth;
  } else if (feature == "max-width") {
    constraint.type = MediaConstraintType::MaxWidth;
  } else if (feature == "min-height") {
    constraint.type = MediaConstraintType::MinHeight;
  } else if (feature == "max-height") {
    constraint.type = MediaConstraintType::MaxHeight;
  } else if (feature == "min-aspect-ratio") {
    constraint.type = MediaConstraintType::MinAspectRatio;
  } else if (feature == "max-aspect-ratio") {
    constraint.type = MediaConstraintType::MaxAspectRatio;
  } else if (feature == "orientation") {
    if (value == "portrait") {
      constraint.type = MediaConstraintType::OrientationPortrait;
      constraint.value = 0.0f;
    } else if (value == "landscape") {
      constraint.type = MediaConstraintType::OrientationLandscape;
      constraint.value = 0.0f;
    } else {
      return false;
    }
  } else if (feature == "hover") {
    if (value == "hover") {
      constraint.type = MediaConstraintType::HoverHover;
      constraint.value = 0.0f;
    } else if (value == "none") {
      constraint.type = MediaConstraintType::HoverNone;
      constraint.value = 0.0f;
    } else {
      return false;
    }
  } else if (feature == "any-hover") {
    if (value == "hover") {
      constraint.type = MediaConstraintType::AnyHoverHover;
      constraint.value = 0.0f;
    } else if (value == "none") {
      constraint.type = MediaConstraintType::AnyHoverNone;
      constraint.value = 0.0f;
    } else {
      return false;
    }
  } else if (feature == "pointer") {
    if (value == "none") {
      constraint.type = MediaConstraintType::PointerNone;
      constraint.value = 0.0f;
    } else if (value == "coarse") {
      constraint.type = MediaConstraintType::PointerCoarse;
      constraint.value = 0.0f;
    } else if (value == "fine") {
      constraint.type = MediaConstraintType::PointerFine;
      constraint.value = 0.0f;
    } else {
      return false;
    }
  } else if (feature == "any-pointer") {
    if (value == "none") {
      constraint.type = MediaConstraintType::AnyPointerNone;
      constraint.value = 0.0f;
    } else if (value == "coarse") {
      constraint.type = MediaConstraintType::AnyPointerCoarse;
      constraint.value = 0.0f;
    } else if (value == "fine") {
      constraint.type = MediaConstraintType::AnyPointerFine;
      constraint.value = 0.0f;
    } else {
      return false;
    }
  } else if (feature == "prefers-reduced-motion") {
    if (value == "reduce") {
      constraint.type = MediaConstraintType::PrefersReducedMotionReduce;
      constraint.value = 0.0f;
    } else if (value == "no-preference") {
      constraint.type = MediaConstraintType::PrefersReducedMotionNoPreference;
      constraint.value = 0.0f;
    } else {
      return false;
    }
  } else if (feature == "prefers-color-scheme") {
    if (value == "dark") {
      constraint.type = MediaConstraintType::PrefersColorSchemeDark;
      constraint.value = 0.0f;
    } else if (value == "light") {
      constraint.type = MediaConstraintType::PrefersColorSchemeLight;
      constraint.value = 0.0f;
    } else {
      return false;
    }
  } else if (feature == "prefers-contrast") {
    if (value == "more") {
      constraint.type = MediaConstraintType::PrefersContrastMore;
      constraint.value = 0.0f;
    } else if (value == "less") {
      constraint.type = MediaConstraintType::PrefersContrastLess;
      constraint.value = 0.0f;
    } else if (value == "no-preference") {
      constraint.type = MediaConstraintType::PrefersContrastNoPreference;
      constraint.value = 0.0f;
    } else {
      return false;
    }
  } else if (feature == "forced-colors") {
    if (value == "active") {
      constraint.type = MediaConstraintType::ForcedColorsActive;
      constraint.value = 0.0f;
    } else if (value == "none") {
      constraint.type = MediaConstraintType::ForcedColorsNone;
      constraint.value = 0.0f;
    } else {
      return false;
    }
  } else {
    return false;
  }

  if (constraint.type == MediaConstraintType::MinWidth ||
      constraint.type == MediaConstraintType::MinWidthExclusive ||
      constraint.type == MediaConstraintType::MaxWidth ||
      constraint.type == MediaConstraintType::MaxWidthExclusive ||
      constraint.type == MediaConstraintType::MinHeight ||
      constraint.type == MediaConstraintType::MinHeightExclusive ||
      constraint.type == MediaConstraintType::MaxHeight ||
      constraint.type == MediaConstraintType::MaxHeightExclusive) {
    constraint.value = parse_media_length(value);
  } else if (constraint.type == MediaConstraintType::MinAspectRatio ||
             constraint.type == MediaConstraintType::MinAspectRatioExclusive ||
             constraint.type == MediaConstraintType::MaxAspectRatio ||
             constraint.type == MediaConstraintType::MaxAspectRatioExclusive) {
    constraint.value = parse_media_ratio(value);
  }
  out.push_back(constraint);
  return true;
}

static bool parse_single_media_query(const std::string& query, MediaQuery& out) {
  const std::string trimmed = trim_copy(query);
  if (trimmed.empty()) {
    return false;
  }

  bool saw_supported_feature = false;
  for (const auto& term : split_media_query_terms(trimmed)) {
    if (is_ignorable_media_term(term)) {
      continue;
    }

    std::vector<MediaConstraint> constraints;
    if (!parse_media_feature(term, constraints)) {
      return false;
    }

    out.constraints.insert(out.constraints.end(), constraints.begin(),
                           constraints.end());
    saw_supported_feature = true;
  }

  out.always_match = !saw_supported_feature;
  return true;
}

static MediaQueryList parse_media_query_list(const std::string& prelude) {
  MediaQueryList list;
  for (const auto& query_text : split_top_level_csv(prelude)) {
    MediaQuery query;
    if (parse_single_media_query(query_text, query)) {
      list.queries.push_back(std::move(query));
    }
  }
  return list;
}

static bool parse_container_feature(const std::string& raw,
                                    std::vector<ContainerConstraint>& out) {
  std::string expr = trim_copy(raw);
  if (expr.size() >= 2 && expr.front() == '(' && expr.back() == ')') {
    expr = trim_copy(expr.substr(1, expr.size() - 2));
  }
  const std::string lowered = to_lower_copy(expr);
  if (lowered.empty()) {
    return false;
  }

  auto push_constraint = [&](ContainerConstraintType type,
                             const std::string& value_text) {
    out.push_back({type, parse_media_length(value_text)});
  };

  const size_t colon = lowered.find(':');
  if (colon != std::string::npos) {
    const std::string feature = trim_copy(lowered.substr(0, colon));
    const std::string value = trim_copy(lowered.substr(colon + 1));
    if (value.empty()) {
      return false;
    }
    if (feature == "min-width" || feature == "min-inline-size") {
      push_constraint(ContainerConstraintType::MinWidth, value);
      return true;
    }
    if (feature == "max-width" || feature == "max-inline-size") {
      push_constraint(ContainerConstraintType::MaxWidth, value);
      return true;
    }
    return false;
  }

  const auto tokens = split_media_feature_tokens(lowered);
  const auto is_inline_size_feature = [](const std::string& token) {
    return token == "width" || token == "inline-size";
  };
  if (tokens.size() == 3 && is_inline_size_feature(tokens[0])) {
    if (tokens[1] == ">=") {
      push_constraint(ContainerConstraintType::MinWidth, tokens[2]);
      return true;
    }
    if (tokens[1] == ">") {
      push_constraint(ContainerConstraintType::MinWidthExclusive, tokens[2]);
      return true;
    }
    if (tokens[1] == "<=") {
      push_constraint(ContainerConstraintType::MaxWidth, tokens[2]);
      return true;
    }
    if (tokens[1] == "<") {
      push_constraint(ContainerConstraintType::MaxWidthExclusive, tokens[2]);
      return true;
    }
  }
  if (tokens.size() == 3 && is_inline_size_feature(tokens[2])) {
    if (tokens[1] == "<=") {
      push_constraint(ContainerConstraintType::MinWidth, tokens[0]);
      return true;
    }
    if (tokens[1] == "<") {
      push_constraint(ContainerConstraintType::MinWidthExclusive, tokens[0]);
      return true;
    }
    if (tokens[1] == ">=") {
      push_constraint(ContainerConstraintType::MaxWidth, tokens[0]);
      return true;
    }
    if (tokens[1] == ">") {
      push_constraint(ContainerConstraintType::MaxWidthExclusive, tokens[0]);
      return true;
    }
  }
  if (tokens.size() == 5 && is_inline_size_feature(tokens[2])) {
    if (tokens[1] == "<=") {
      push_constraint(ContainerConstraintType::MinWidth, tokens[0]);
    } else if (tokens[1] == "<") {
      push_constraint(ContainerConstraintType::MinWidthExclusive, tokens[0]);
    } else if (tokens[1] == ">=") {
      push_constraint(ContainerConstraintType::MaxWidth, tokens[0]);
    } else if (tokens[1] == ">") {
      push_constraint(ContainerConstraintType::MaxWidthExclusive, tokens[0]);
    } else {
      return false;
    }

    if (tokens[3] == ">=") {
      push_constraint(ContainerConstraintType::MinWidth, tokens[4]);
      return true;
    }
    if (tokens[3] == ">") {
      push_constraint(ContainerConstraintType::MinWidthExclusive, tokens[4]);
      return true;
    }
    if (tokens[3] == "<=") {
      push_constraint(ContainerConstraintType::MaxWidth, tokens[4]);
      return true;
    }
    if (tokens[3] == "<") {
      push_constraint(ContainerConstraintType::MaxWidthExclusive, tokens[4]);
      return true;
    }
    return false;
  }

  return false;
}

static bool parse_single_container_query(const std::string& query_text,
                                         ContainerQuery& out) {
  const std::string trimmed = trim_copy(query_text);
  if (trimmed.empty()) {
    return false;
  }

  const size_t first_paren = trimmed.find('(');
  if (first_paren == std::string::npos) {
    return false;
  }

  out.name = strip_quotes_copy(trim_copy(trimmed.substr(0, first_paren)));
  bool saw_supported_feature = false;
  for (const auto& term : split_media_query_terms(trimmed.substr(first_paren))) {
    if (is_ignorable_media_term(term)) {
      continue;
    }
    std::vector<ContainerConstraint> constraints;
    if (!parse_container_feature(term, constraints)) {
      return false;
    }
    out.constraints.insert(out.constraints.end(), constraints.begin(),
                           constraints.end());
    saw_supported_feature = true;
  }

  out.always_match = !saw_supported_feature;
  return true;
}

static ContainerQueryList parse_container_query_list(
    const std::string& prelude) {
  ContainerQueryList list;
  for (const auto& query_text : split_top_level_csv(prelude)) {
    ContainerQuery query;
    if (parse_single_container_query(query_text, query)) {
      list.queries.push_back(std::move(query));
    }
  }
  return list;
}

static float element_content_inline_size(const Element* elem) {
  if (!elem) {
    return 0.0f;
  }

  float width = elem->layout_width();
  if (width <= 0.0f && elem->computed_style &&
      !elem->computed_style->width_is_percent) {
    width = elem->computed_style->width;
  }
  if (auto* style = elem->computed_style) {
    width -= style->padding[1] + style->padding[3] + style->border_width[1] +
             style->border_width[3];
  }
  return std::max(width, 0.0f);
}

static bool is_inline_size_container(const ContainerRegistration& registration) {
  return registration.type == ContainerType::InlineSize;
}

static bool container_name_matches(const ContainerRegistration& registration,
                                   const std::string& name) {
  if (name.empty()) {
    return true;
  }
  for (const auto& token : registration.names) {
    if (token == name) {
      return true;
    }
  }
  return false;
}

static const Element* find_matching_container_ancestor(
    const Element* elem, const ContainerQuery& query,
    const std::unordered_map<const Element*, ContainerRegistration>&
        container_registrations) {
  for (auto* ancestor = elem ? elem->parent_elem() : nullptr; ancestor;
       ancestor = ancestor->parent_elem()) {
    const auto it = container_registrations.find(ancestor);
    if (it == container_registrations.end() ||
        !is_inline_size_container(it->second)) {
      continue;
    }
    if (container_name_matches(it->second, query.name)) {
      return ancestor;
    }
  }
  return nullptr;
}

static bool container_query_matches(
    const ContainerQuery& query, const Element* elem,
    const std::unordered_map<const Element*, ContainerRegistration>&
        container_registrations) {
  if (query.always_match) {
    return true;
  }

  const Element* container =
      find_matching_container_ancestor(elem, query, container_registrations);
  if (!container) {
    return false;
  }

  const float inline_size = element_content_inline_size(container);
  for (const auto& constraint : query.constraints) {
    switch (constraint.type) {
    case ContainerConstraintType::MinWidth:
      if (inline_size < constraint.value) {
        return false;
      }
      break;
    case ContainerConstraintType::MinWidthExclusive:
      if (inline_size <= constraint.value) {
        return false;
      }
      break;
    case ContainerConstraintType::MaxWidth:
      if (inline_size > constraint.value) {
        return false;
      }
      break;
    case ContainerConstraintType::MaxWidthExclusive:
      if (inline_size >= constraint.value) {
        return false;
      }
      break;
    }
  }

  return true;
}

static bool container_query_list_matches(
    const ContainerQueryList& list, const Element* elem,
    const std::unordered_map<const Element*, ContainerRegistration>&
        container_registrations) {
  for (const auto& query : list.queries) {
    if (container_query_matches(query, elem, container_registrations)) {
      return true;
    }
  }
  return false;
}

static bool media_query_matches(const MediaQuery& query, float viewport_width,
                                float viewport_height,
                                const MediaEnvironment& env) {
  if (query.always_match) {
    return true;
  }

  const float viewport_ratio =
      viewport_height > 0.0f ? viewport_width / viewport_height : 0.0f;
  for (const auto& constraint : query.constraints) {
    switch (constraint.type) {
    case MediaConstraintType::MinWidth:
      if (viewport_width < constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::MinWidthExclusive:
      if (viewport_width <= constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::MaxWidth:
      if (viewport_width > constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::MaxWidthExclusive:
      if (viewport_width >= constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::MinHeight:
      if (viewport_height < constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::MinHeightExclusive:
      if (viewport_height <= constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::MaxHeight:
      if (viewport_height > constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::MaxHeightExclusive:
      if (viewport_height >= constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::MinAspectRatio:
      if (viewport_ratio < constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::MinAspectRatioExclusive:
      if (viewport_ratio <= constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::MaxAspectRatio:
      if (viewport_ratio > constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::MaxAspectRatioExclusive:
      if (viewport_ratio >= constraint.value) {
        return false;
      }
      break;
    case MediaConstraintType::OrientationPortrait:
      if (viewport_height < viewport_width) {
        return false;
      }
      break;
    case MediaConstraintType::OrientationLandscape:
      if (viewport_width < viewport_height) {
        return false;
      }
      break;
    case MediaConstraintType::HoverHover:
      if (!env.hover_available) {
        return false;
      }
      break;
    case MediaConstraintType::HoverNone:
      if (env.hover_available) {
        return false;
      }
      break;
    case MediaConstraintType::AnyHoverHover:
      if (!env.any_hover_available) {
        return false;
      }
      break;
    case MediaConstraintType::AnyHoverNone:
      if (env.any_hover_available) {
        return false;
      }
      break;
    case MediaConstraintType::PointerNone:
      if (env.pointer_precision != PointerPrecision::None) {
        return false;
      }
      break;
    case MediaConstraintType::PointerCoarse:
      if (env.pointer_precision != PointerPrecision::Coarse) {
        return false;
      }
      break;
    case MediaConstraintType::PointerFine:
      if (env.pointer_precision != PointerPrecision::Fine) {
        return false;
      }
      break;
    case MediaConstraintType::AnyPointerNone:
      if (env.any_pointer_precision != PointerPrecision::None) {
        return false;
      }
      break;
    case MediaConstraintType::AnyPointerCoarse:
      if (env.any_pointer_precision != PointerPrecision::Coarse) {
        return false;
      }
      break;
    case MediaConstraintType::AnyPointerFine:
      if (env.any_pointer_precision != PointerPrecision::Fine) {
        return false;
      }
      break;
    case MediaConstraintType::PrefersReducedMotionReduce:
      if (!env.prefers_reduced_motion) {
        return false;
      }
      break;
    case MediaConstraintType::PrefersReducedMotionNoPreference:
      if (env.prefers_reduced_motion) {
        return false;
      }
      break;
    case MediaConstraintType::PrefersColorSchemeDark:
      if (!env.prefers_dark_scheme) {
        return false;
      }
      break;
    case MediaConstraintType::PrefersColorSchemeLight:
      if (env.prefers_dark_scheme) {
        return false;
      }
      break;
    case MediaConstraintType::PrefersContrastMore:
      if (env.contrast_preference != ContrastPreference::More) {
        return false;
      }
      break;
    case MediaConstraintType::PrefersContrastLess:
      if (env.contrast_preference != ContrastPreference::Less) {
        return false;
      }
      break;
    case MediaConstraintType::PrefersContrastNoPreference:
      if (env.contrast_preference != ContrastPreference::NoPreference) {
        return false;
      }
      break;
    case MediaConstraintType::ForcedColorsActive:
      if (!env.forced_colors_active) {
        return false;
      }
      break;
    case MediaConstraintType::ForcedColorsNone:
      if (env.forced_colors_active) {
        return false;
      }
      break;
    }
  }

  return true;
}

static bool media_query_list_matches(const MediaQueryList& list,
                                     float viewport_width,
                                     float viewport_height,
                                     const MediaEnvironment& env) {
  if (list.queries.empty()) {
    return false;
  }

  for (const auto& query : list.queries) {
    if (media_query_matches(query, viewport_width, viewport_height, env)) {
      return true;
    }
  }

  return false;
}


static bool serialize_rule_to_string(const lxb_css_rule_t* rule,
                                     std::string& out) {
  if (!rule) {
    return false;
  }

  lexbor_str_t str = {0};
  const auto status = lxb_css_rule_serialize(
      rule,
      [](const lxb_char_t* data, size_t len, void* ctx) -> lxb_status_t {
        auto* s = static_cast<lexbor_str_t*>(ctx);
        const size_t current = s->length ? s->length : 0;
        const size_t new_len = current + len;
        auto* new_data =
            static_cast<lxb_char_t*>(realloc(s->data, new_len + 1));
        if (!new_data) {
          return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
        }
        memcpy(new_data + current, data, len);
        new_data[new_len] = 0;
        s->data = new_data;
        s->length = new_len;
        return LXB_STATUS_OK;
      },
      &str);
  if (status != LXB_STATUS_OK || !str.data) {
    if (str.data) {
      free(str.data);
    }
    return false;
  }

  out.assign(reinterpret_cast<char*>(str.data), str.length);
  free(str.data);
  return true;
}

static bool extract_media_prelude_and_block(const lxb_css_rule_at_t* at_rule,
                                            const std::string& source_css,
                                            std::string& prelude,
                                            std::string& block) {
  std::string serialized;
  const char* source_begin = source_css.data();
  const char* source_end = source_begin + source_css.size();
  const size_t start_offset =
      at_rule != nullptr ? at_rule->name_begin : source_css.size();

  if (start_offset < source_css.size()) {
    size_t media_pos = source_css.find("@media", start_offset);
    if (media_pos != std::string::npos) {
      size_t brace_pos = source_css.find('{', media_pos + 6);
      if (brace_pos != std::string::npos) {
        int brace_depth = 1;
        size_t i = brace_pos + 1;
        for (; i < source_css.size(); ++i) {
          if (source_css[i] == '{') {
            ++brace_depth;
          } else if (source_css[i] == '}') {
            --brace_depth;
            if (brace_depth == 0) {
              serialized = source_css.substr(media_pos, i - media_pos + 1);
              break;
            }
          }
        }
      }
    }
  }

  if (serialized.empty() && !serialize_rule_to_string(&at_rule->rule, serialized)) {
    return false;
  }

  const std::string lowered = to_lower_copy(serialized);
  const size_t media_pos = lowered.find("@media");
  if (media_pos == std::string::npos) {
    return false;
  }

  const size_t brace_pos = serialized.find('{', media_pos + 6);
  if (brace_pos == std::string::npos) {
    return false;
  }

  prelude = trim_copy(serialized.substr(media_pos + 6, brace_pos - (media_pos + 6)));

  int brace_depth = 1;
  const size_t block_start = brace_pos + 1;
  size_t i = block_start;
  for (; i < serialized.size(); ++i) {
    if (serialized[i] == '{') {
      ++brace_depth;
    } else if (serialized[i] == '}') {
      --brace_depth;
      if (brace_depth == 0) {
        break;
      }
    }
  }

  if (brace_depth != 0 || i <= block_start) {
    return false;
  }

  block = serialized.substr(block_start, i - block_start);
  return true;
}

static bool is_color_value_token(const std::string& value) {
  if (value.empty()) {
    return false;
  }
  if (value[0] == '#') {
    return true;
  }
  if (value.find("rgb(") == 0 || value.find("rgba(") == 0 ||
      value.find("hsl(") == 0 || value.find("hsla(") == 0 ||
      value.find("oklch(") == 0 || value.find("oklab(") == 0 ||
      value.find("color-mix(") == 0 || value.find("light-dark(") == 0) {
    return true;
  }
  return value == "currentcolor" || value == "currentColor" ||
         value == "transparent" || value == "black" || value == "white" ||
         value == "red" || value == "green" || value == "blue" ||
         value == "yellow" || value == "gray" || value == "grey";
}

static std::string color_to_css_variable_value(const Color& color) {
  const auto clamp_u8 = [](float channel) {
    const float scaled = std::round(std::clamp(channel, 0.0f, 1.0f) * 255.0f);
    return static_cast<int>(scaled);
  };
  const auto format_alpha = [](float alpha) {
    const float clamped = std::clamp(alpha, 0.0f, 1.0f);
    if (clamped <= 0.0f) {
      return std::string("0");
    }
    if (clamped >= 1.0f) {
      return std::string("255");
    }

    std::ostringstream out;
    out.setf(std::ios::fixed);
    out.precision(3);
    out << clamped;
    std::string text = out.str();
    while (!text.empty() && text.back() == '0') {
      text.pop_back();
    }
    if (!text.empty() && text.back() == '.') {
      text.pop_back();
    }
    return text.empty() ? std::string("0") : text;
  };

  std::ostringstream out;
  out << clamp_u8(color.r) << ", " << clamp_u8(color.g) << ", "
      << clamp_u8(color.b) << ", " << format_alpha(color.a);
  return out.str();
}

std::string normalize_css_for_parser(std::string css) {
  struct ReplacementRule {
    const char* needle;
    const char* replacement;
  };

  static const ReplacementRule replacements[] = {
      {":focus-visible", "[__flex_state_focus_visible]"},
      {":focus-within", "[__flex_state_focus_within]"},
      {":selected", "[__flex_state_selected]"},
      {":open", "[__flex_state_open]"},
      {":modal", "[__flex_state_modal]"},
      {":indeterminate", "[__flex_state_indeterminate]"},
      {":placeholder-shown", "[__flex_state_placeholder_shown]"},
      {":required", "[__flex_state_required]"},
      {":optional", "[__flex_state_optional]"},
      {":invalid", "[__flex_state_invalid]"},
      {":valid", "[__flex_state_valid]"},
      {":read-only", "[__flex_state_read_only]"},
      {":read-write", "[__flex_state_read_write]"},
      {"::before", "[__flex_pseudo_before]"},
      {":before", "[__flex_pseudo_before]"},
      {"::after", "[__flex_pseudo_after]"},
      {":after", "[__flex_pseudo_after]"},
      {"::placeholder", "[__flex_pseudo_placeholder]"},
      {"::selection", "[__flex_pseudo_selection]"},
      {":selection", "[__flex_pseudo_selection]"},
  };

  for (const auto& replacement_rule : replacements) {
    const std::string needle = replacement_rule.needle;
    const std::string replacement = replacement_rule.replacement;
    size_t pos = 0;
    while ((pos = css.find(needle, pos)) != std::string::npos) {
      css.replace(pos, needle.size(), replacement);
      pos += replacement.size();
    }
  }

  const auto normalize_color_slash_alpha = [&](const char* function_name) {
    const std::string needle = function_name;
    size_t pos = 0;
    while ((pos = css.find(needle, pos)) != std::string::npos) {
      const size_t open = pos + needle.size() - 1;
      if (open >= css.size() || css[open] != '(') {
        pos += needle.size();
        continue;
      }

      int depth = 1;
      for (size_t i = open + 1; i < css.size(); ++i) {
        if (css[i] == '(') {
          ++depth;
        } else if (css[i] == ')' && depth > 0) {
          --depth;
          if (depth == 0) {
            pos = i + 1;
            break;
          }
        } else if (css[i] == '/' && depth == 1) {
          css[i] = ',';
        }
      }
      if (depth != 0) {
        break;
      }
    }
  };

  normalize_color_slash_alpha("rgb(");
  normalize_color_slash_alpha("rgba(");
  normalize_color_slash_alpha("hsl(");
  normalize_color_slash_alpha("hsla(");
  normalize_color_slash_alpha("oklch(");
  normalize_color_slash_alpha("oklab(");
  return css;
}

static ComplexSelector parse_complex_selector(const std::string& selector);
static uint32_t calculate_specificity(const ComplexSelector& sel);
static bool matches_selector(const ComplexSelector& sel, const Element* elem);
static bool matches_relative_has_selector(const std::string& selector,
                                          const Element* elem);
static bool rule_matches_media_conditions(const CSSRule& rule,
                                          const Element* elem);
static bool rule_matches_container_conditions(
    const CSSRule& rule, const Element* elem,
    const std::unordered_map<const Element*, ContainerRegistration>&
        container_registrations);

static ParsedSelector parse_selector_simple(const std::string &selector) {
  ParsedSelector result;
  const std::string s = trim_copy(selector);
  size_t i = 0;
  while (i < s.size()) {
    i = skip_selector_ws(s, i);
    if (i >= s.size()) {
      break;
    }

    char c = s[i];
    if (c == '#') {
      ++i;
      result.id = Symbol(parse_identifier_token(s, i));
    } else if (c == '.') {
      ++i;
      result.classes.insert(Symbol(parse_class_selector_token(s, i)));
    } else if (c == ':') {
      ++i;
      const std::string pseudo = parse_identifier_token(s, i);
      if (pseudo.empty()) {
        continue;
      }

      if (i < s.size() && s[i] == '(') {
        const std::string inner = parse_balanced_token(s, i, '(', ')');
        auto inner_selectors = split_selector_list(inner);
        std::vector<std::string> parsed_group;
        parsed_group.reserve(inner_selectors.size());
        for (const auto& inner_selector : inner_selectors) {
          const std::string trimmed_inner = trim_copy(inner_selector);
          if (!trimmed_inner.empty()) {
            parsed_group.push_back(trimmed_inner);
          }
        }

        if (pseudo == "not") {
          if (!parsed_group.empty()) {
            result.not_groups.push_back(std::move(parsed_group));
          }
        } else if (pseudo == "has") {
          if (!parsed_group.empty()) {
            result.has_groups.push_back(std::move(parsed_group));
          }
        } else if (pseudo == "is") {
          if (!parsed_group.empty()) {
            result.is_groups.push_back(std::move(parsed_group));
          }
        } else if (pseudo == "where") {
          if (!parsed_group.empty()) {
            result.where_groups.push_back(std::move(parsed_group));
          }
        } else if (pseudo == "nth-child" || pseudo == "nth-of-type") {
          result.pseudo_functions.push_back(pseudo + "(" + trim_copy(inner) + ")");
        }
      } else {
        result.pseudo_classes.push_back(Symbol(pseudo));
      }
    } else if (c == '[') {
      ++i;
      ParsedSelector::AttributeSelector attr;
      i = skip_selector_ws(s, i);
      attr.name = Symbol(parse_identifier_token(s, i));
      i = skip_selector_ws(s, i);
      if (i < s.size() && s[i] == '=') {
        ++i;
        i = skip_selector_ws(s, i);
        size_t value_start = i;
        if (i < s.size() && (s[i] == '"' || s[i] == '\'')) {
          const char quote = s[i++];
          while (i < s.size() && s[i] != quote) {
            ++i;
          }
          if (i < s.size() && s[i] == quote) {
            ++i;
          }
        } else {
          while (i < s.size() && s[i] != ']') {
            ++i;
          }
        }
        attr.value = trim_copy(strip_quotes_copy(s.substr(value_start, i - value_start)));
        attr.has_value = true;
      }
      while (i < s.size() && s[i] != ']') {
        ++i;
      }
      if (i < s.size() && s[i] == ']') {
        ++i;
      }
      if (attr.name.id != 0) {
        result.attributes.push_back(std::move(attr));
      }
    } else if (c == '*') {
      result.type = Symbol("*");
      ++i;
    } else if (std::isalpha(static_cast<unsigned char>(c))) {
      result.type = Symbol(parse_identifier_token(s, i));
    } else {
      ++i;
    }
  }
  return result;
}

static const Element* previous_element_sibling(const Element* elem);
static const Element* next_element_sibling(const Element* elem);

static bool parse_nth_pseudo_argument(const std::string& value, int& out_index,
                                      bool& out_odd, bool& out_even,
                                      int& out_step) {
  const std::string lowered = to_lower_copy(trim_copy(value));
  if (lowered == "odd") {
    out_odd = true;
    out_even = false;
    out_index = 0;
    out_step = 0;
    return true;
  }
  if (lowered == "even") {
    out_odd = false;
    out_even = true;
    out_index = 0;
    out_step = 0;
    return true;
  }
  if (lowered.empty()) {
    return false;
  }

  const size_t n_pos = lowered.find('n');
  if (n_pos == std::string::npos) {
    size_t parsed = 0;
    try {
      out_index = std::stoi(lowered, &parsed);
    } catch (...) {
      return false;
    }
    if (parsed != lowered.size()) {
      return false;
    }

    out_odd = false;
    out_even = false;
    out_step = 0;
    return true;
  }

  const std::string a_text = lowered.substr(0, n_pos);
  const std::string b_text = lowered.substr(n_pos + 1);
  auto parse_coeff = [](const std::string& text, int default_value,
                        int& out_value) -> bool {
    if (text.empty() || text == "+") {
      out_value = default_value;
      return true;
    }
    if (text == "-") {
      out_value = -default_value;
      return true;
    }

    size_t parsed = 0;
    try {
      out_value = std::stoi(text, &parsed);
    } catch (...) {
      return false;
    }
    return parsed == text.size();
  };

  int step = 0;
  if (!parse_coeff(a_text, 1, step)) {
    return false;
  }

  int offset = 0;
  if (!b_text.empty() && !parse_coeff(b_text, 0, offset)) {
    return false;
  }

  out_odd = false;
  out_even = false;
  out_index = offset;
  out_step = step;
  return true;
}

static int child_index_in_parent(const Element* elem, bool same_type_only) {
  const Element* parent = elem ? elem->parent_elem() : nullptr;
  if (!parent || !elem) {
    return -1;
  }

  int index = 0;
  for (size_t i = 0; i < parent->child_count(); ++i) {
    const auto* child = parent->child_at(i);
    if (!child) {
      continue;
    }
    if (!same_type_only || child->tag_name() == elem->tag_name()) {
      ++index;
    }
    if (child == elem) {
      return index;
    }
  }
  return -1;
}

static bool matches_nth_position(int index, const std::string& argument) {
  if (index <= 0) {
    return false;
  }

  int target = 0;
  bool odd = false;
  bool even = false;
  int step = 0;
  if (!parse_nth_pseudo_argument(argument, target, odd, even, step)) {
    return false;
  }
  if (odd) {
    return (index % 2) == 1;
  }
  if (even) {
    return (index % 2) == 0;
  }
  if (step == 0) {
    return index == target;
  }

  const int delta = index - target;
  if ((step > 0 && delta < 0) || (step < 0 && delta > 0)) {
    return false;
  }
  return delta % step == 0;
}

static bool matches_structural_pseudo_class(Symbol pseudo, const Element* elem) {
  if (!elem) {
    return false;
  }

  static const Symbol first_child("first-child");
  static const Symbol last_child("last-child");
  static const Symbol only_child("only-child");
  static const Symbol empty("empty");

  if (pseudo == first_child) {
    return previous_element_sibling(elem) == nullptr;
  }
  if (pseudo == last_child) {
    return next_element_sibling(elem) == nullptr;
  }
  if (pseudo == only_child) {
    return previous_element_sibling(elem) == nullptr &&
           next_element_sibling(elem) == nullptr;
  }
  if (pseudo == empty) {
    return elem->child_count() == 0 && elem->text().empty();
  }

  return false;
}

static bool matches_parameterized_pseudo_class(const std::string& pseudo,
                                               const Element* elem) {
  if (!elem) {
    return false;
  }

  static const std::string nth_child_prefix = "nth-child(";
  static const std::string nth_of_type_prefix = "nth-of-type(";
  if (pseudo.size() > nth_child_prefix.size() &&
      pseudo.rfind(nth_child_prefix, 0) == 0 && pseudo.back() == ')') {
    return matches_nth_position(
        child_index_in_parent(elem, false),
        pseudo.substr(nth_child_prefix.size(),
                      pseudo.size() - nth_child_prefix.size() - 1));
  }
  if (pseudo.size() > nth_of_type_prefix.size() &&
      pseudo.rfind(nth_of_type_prefix, 0) == 0 && pseudo.back() == ')') {
    return matches_nth_position(
        child_index_in_parent(elem, true),
        pseudo.substr(nth_of_type_prefix.size(),
                      pseudo.size() - nth_of_type_prefix.size() - 1));
  }

  return false;
}

static ComplexSelector parse_complex_selector(const std::string& selector) {
  ComplexSelector result;
  std::vector<std::string> parts;
  std::vector<SelectorCombinator> combinators;
  std::string current;
  int paren_depth = 0;
  int bracket_depth = 0;

  auto flush_current = [&]() {
    const std::string trimmed = trim_copy(current);
    if (!trimmed.empty()) {
      parts.push_back(trimmed);
    }
    current.clear();
  };

  bool escaped = false;
  for (size_t i = 0; i < selector.size(); ++i) {
    const char c = selector[i];
    if (escaped) {
      current.push_back(c);
      escaped = false;
      continue;
    }
    if (c == '\\') {
      current.push_back(c);
      escaped = true;
      continue;
    }
    if (c == '(') {
      ++paren_depth;
      current.push_back(c);
      continue;
    }
    if (c == ')') {
      if (paren_depth > 0) --paren_depth;
      current.push_back(c);
      continue;
    }
    if (c == '[') {
      ++bracket_depth;
      current.push_back(c);
      continue;
    }
    if (c == ']') {
      if (bracket_depth > 0) --bracket_depth;
      current.push_back(c);
      continue;
    }

    if (paren_depth == 0 && bracket_depth == 0) {
      if (c == '>' || c == '+' || c == '~') {
        flush_current();
        if (!parts.empty()) {
          if (c == '>') combinators.push_back(SelectorCombinator::Child);
          else if (c == '+') combinators.push_back(SelectorCombinator::AdjacentSibling);
          else combinators.push_back(SelectorCombinator::GeneralSibling);
        }
        continue;
      }

      if (std::isspace(static_cast<unsigned char>(c))) {
        const bool had_current = !trim_copy(current).empty();
        flush_current();
        size_t j = i + 1;
        while (j < selector.size() &&
               std::isspace(static_cast<unsigned char>(selector[j]))) {
          ++j;
        }
        if (had_current && !parts.empty() && j < selector.size() &&
            selector[j] != '>' && selector[j] != '+' && selector[j] != '~') {
          combinators.push_back(SelectorCombinator::Descendant);
        }
        i = j - 1;
        continue;
      }
    }

    current.push_back(c);
  }

  flush_current();

  if (parts.empty()) {
    return result;
  }

  result.steps.reserve(parts.size());
  for (size_t i = 0; i < parts.size(); ++i) {
    SelectorStep step;
    step.selector = parse_selector_simple(parts[i]);
    step.combinator_to_left =
        i == 0 ? SelectorCombinator::None : combinators[i - 1];
    result.steps.push_back(std::move(step));
  }
  return result;
}

static bool matches_pseudo_class(Symbol pseudo, const Element* elem) {
  static const Symbol disabled("disabled");
  static const Symbol checked("checked");
  static const Symbol selected("selected");
  static const Symbol open("open");
  static const Symbol modal("modal");
  static const Symbol indeterminate("indeterminate");
  static const Symbol focusvisible("focusvisible");
  static const Symbol focuswithin("focuswithin");
  static const Symbol focus_within("focus-within");
  static const Symbol placeholdershown("placeholdershown");
  static const Symbol placeholder_shown("placeholder-shown");
  static const Symbol required("required");
  static const Symbol optional("optional");
  static const Symbol valid("valid");
  static const Symbol invalid("invalid");
  static const Symbol readonly_compact("readonly");
  static const Symbol read_only("read-only");
  static const Symbol readwrite_compact("readwrite");
  static const Symbol read_write("read-write");
  if (matches_structural_pseudo_class(pseudo, elem)) {
    return true;
  }
  if (pseudo == disabled) {
    if (elem->has_state(disabled) || elem->has_attribute(disabled)) {
      return true;
    }
    const std::string* aria_disabled = elem->attribute(Symbol("aria-disabled"));
    return aria_disabled != nullptr && *aria_disabled == "true";
  }
  if (pseudo == checked) {
    if (elem->has_state(checked)) {
      return true;
    }
    const std::string* aria_checked = elem->attribute(Symbol("aria-checked"));
    return aria_checked != nullptr && *aria_checked == "true";
  }
  if (pseudo == selected) {
    if (elem->has_state(selected)) {
      return true;
    }
    const std::string* aria_selected = elem->attribute(Symbol("aria-selected"));
    if (aria_selected != nullptr && *aria_selected == "true") {
      return true;
    }
    const std::string* data_state = elem->attribute(Symbol("data-state"));
    return data_state != nullptr && *data_state == "selected";
  }
  if (pseudo == open) {
    const std::string* data_state = elem->attribute(Symbol("data-state"));
    if (data_state != nullptr && *data_state == "open") {
      return true;
    }
    const std::string* aria_expanded = elem->attribute(Symbol("aria-expanded"));
    return aria_expanded != nullptr && *aria_expanded == "true";
  }
  if (pseudo == modal) {
    const std::string* aria_modal = elem->attribute(Symbol("aria-modal"));
    return aria_modal != nullptr && *aria_modal == "true";
  }
  if (pseudo == indeterminate) {
    return elem->has_state(indeterminate);
  }
  if (pseudo == focusvisible) {
    return elem->has_state(Symbol("focus-visible"));
  }
  if (pseudo == focuswithin || pseudo == focus_within) {
    return elem->has_state(Symbol("focus-within"));
  }
  if (pseudo == placeholdershown || pseudo == placeholder_shown) {
    if (const auto* input = elem->widget_as<InputWidget>()) {
      return input->text().empty() && !input->placeholder().empty();
    }
    if (const auto* textarea = elem->widget_as<TextAreaWidget>()) {
      return textarea->text().empty() && !textarea->placeholder().empty();
    }
    return false;
  }
  const auto is_required = [&]() {
    if (elem->has_state(required) || elem->has_attribute(required)) {
      return true;
    }
    const std::string* aria_required = elem->attribute(Symbol("aria-required"));
    return aria_required != nullptr && *aria_required == "true";
  };
  const auto is_invalid = [&]() {
    if (elem->has_state(invalid) || elem->has_attribute(invalid) ||
        elem->has_attribute(Symbol("data-invalid"))) {
      return true;
    }
    const std::string* aria_invalid = elem->attribute(Symbol("aria-invalid"));
    return aria_invalid != nullptr && *aria_invalid != "false";
  };
  const auto is_read_only = [&]() {
    if (elem->has_state(read_only) || elem->has_state(readonly_compact) ||
        elem->has_attribute(Symbol("readonly")) ||
        elem->has_attribute(Symbol("read-only"))) {
      return true;
    }
    const std::string* aria_readonly = elem->attribute(Symbol("aria-readonly"));
    return aria_readonly != nullptr && *aria_readonly == "true";
  };
  if (pseudo == required) {
    return is_required();
  }
  if (pseudo == optional) {
    return !is_required();
  }
  if (pseudo == invalid) {
    return is_invalid();
  }
  if (pseudo == valid) {
    return !is_invalid();
  }
  if (pseudo == readonly_compact || pseudo == read_only) {
    return is_read_only();
  }
  if (pseudo == readwrite_compact || pseudo == read_write) {
    return !is_read_only() && !matches_pseudo_class(disabled, elem);
  }
  return elem->has_state(pseudo);
}

static bool matches_compound_selector(const ParsedSelector &sel,
                                      const Element *elem) {
  if (sel.id.id != 0 && sel.id != elem->element_id())
    return false;
  static const Symbol wildcard("*");
  if (sel.type.id != 0 && sel.type != wildcard && sel.type != elem->tag_name())
    return false;

  for (const auto &cls : sel.classes) {
    if (!elem->has_class(cls))
      return false;
  }

  for (const auto& attr : sel.attributes) {
    static const Symbol focus_visible_attr("__flex_state_focus_visible");
    static const Symbol focus_within_attr("__flex_state_focus_within");
    static const Symbol selected_attr("__flex_state_selected");
    static const Symbol open_attr("__flex_state_open");
    static const Symbol modal_attr("__flex_state_modal");
    static const Symbol indeterminate_attr("__flex_state_indeterminate");
    static const Symbol placeholder_shown_attr("__flex_state_placeholder_shown");
    static const Symbol required_attr("__flex_state_required");
    static const Symbol optional_attr("__flex_state_optional");
    static const Symbol invalid_attr("__flex_state_invalid");
    static const Symbol valid_attr("__flex_state_valid");
    static const Symbol read_only_attr("__flex_state_read_only");
    static const Symbol read_write_attr("__flex_state_read_write");
    if (attr.name == focus_visible_attr) {
      const bool matched = elem->has_state(Symbol("focus-visible"));
      if (!matched) {
        return false;
      }
      if (attr.has_value && attr.value != "true") {
        return false;
      }
      continue;
    }
    if (attr.name == focus_within_attr) {
      const bool matched = elem->has_state(Symbol("focus-within"));
      if (!matched) {
        return false;
      }
      if (attr.has_value && attr.value != "true") {
        return false;
      }
      continue;
    }
    if (attr.name == selected_attr) {
      const bool matched = matches_pseudo_class(Symbol("selected"), elem);
      if (!matched) {
        return false;
      }
      if (attr.has_value && attr.value != "true") {
        return false;
      }
      continue;
    }
    if (attr.name == open_attr) {
      const bool matched = matches_pseudo_class(Symbol("open"), elem);
      if (!matched) {
        return false;
      }
      if (attr.has_value && attr.value != "true") {
        return false;
      }
      continue;
    }
    if (attr.name == modal_attr) {
      const bool matched = matches_pseudo_class(Symbol("modal"), elem);
      if (!matched) {
        return false;
      }
      if (attr.has_value && attr.value != "true") {
        return false;
      }
      continue;
    }
    if (attr.name == indeterminate_attr) {
      const bool matched = matches_pseudo_class(Symbol("indeterminate"), elem);
      if (!matched) {
        return false;
      }
      if (attr.has_value && attr.value != "true") {
        return false;
      }
      continue;
    }
    if (attr.name == placeholder_shown_attr) {
      bool matched = false;
      if (const auto* input = elem->widget_as<InputWidget>()) {
        matched = input->text().empty() && !input->placeholder().empty();
      } else if (const auto* textarea = elem->widget_as<TextAreaWidget>()) {
        matched = textarea->text().empty() && !textarea->placeholder().empty();
      }
      if (!matched) {
        return false;
      }
      if (attr.has_value && attr.value != "true") {
        return false;
      }
      continue;
    }
    if (attr.name == required_attr || attr.name == optional_attr ||
        attr.name == invalid_attr || attr.name == valid_attr ||
        attr.name == read_only_attr || attr.name == read_write_attr) {
      Symbol pseudo("required");
      if (attr.name == optional_attr) pseudo = Symbol("optional");
      else if (attr.name == invalid_attr) pseudo = Symbol("invalid");
      else if (attr.name == valid_attr) pseudo = Symbol("valid");
      else if (attr.name == read_only_attr) pseudo = Symbol("read-only");
      else if (attr.name == read_write_attr) pseudo = Symbol("read-write");

      const bool matched = matches_pseudo_class(pseudo, elem);
      if (!matched) {
        return false;
      }
      if (attr.has_value && attr.value != "true") {
        return false;
      }
      continue;
    }
    const std::string* value = elem->attribute(attr.name);
    if (!value) {
      return false;
    }
    if (attr.has_value && *value != attr.value) {
      return false;
    }
  }

  for (const auto& pseudo : sel.pseudo_classes) {
    if (!matches_pseudo_class(pseudo, elem)) {
      return false;
    }
  }
  for (const auto& pseudo : sel.pseudo_functions) {
    if (!matches_parameterized_pseudo_class(pseudo, elem)) {
      return false;
    }
  }

  for (const auto& group : sel.is_groups) {
    bool matched = false;
    for (const auto& option : group) {
      if (matches_selector(parse_complex_selector(option), elem)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      return false;
    }
  }

  for (const auto& group : sel.where_groups) {
    bool matched = false;
    for (const auto& option : group) {
      if (matches_selector(parse_complex_selector(option), elem)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      return false;
    }
  }

  for (const auto& group : sel.has_groups) {
    bool matched = false;
    for (const auto& option : group) {
      if (matches_relative_has_selector(option, elem)) {
        matched = true;
        break;
      }
    }
    if (!matched) {
      return false;
    }
  }

  for (const auto& group : sel.not_groups) {
    for (const auto& option : group) {
      if (matches_selector(parse_complex_selector(option), elem)) {
        return false;
      }
    }
  }

  return true;
}

static const Element* previous_element_sibling(const Element* elem) {
  const Element* parent = elem ? elem->parent_elem() : nullptr;
  if (!parent) {
    return nullptr;
  }

  for (size_t i = 0; i < parent->child_count(); ++i) {
    if (parent->child_at(i) == elem) {
      return i > 0 ? parent->child_at(i - 1) : nullptr;
    }
  }
  return nullptr;
}

static const Element* next_element_sibling(const Element* elem) {
  const Element* parent = elem ? elem->parent_elem() : nullptr;
  if (!parent) {
    return nullptr;
  }

  for (size_t i = 0; i < parent->child_count(); ++i) {
    if (parent->child_at(i) == elem) {
      return i + 1 < parent->child_count() ? parent->child_at(i + 1) : nullptr;
    }
  }
  return nullptr;
}

static bool subtree_has_match(const Element* elem, const ComplexSelector& selector) {
  if (!elem) {
    return false;
  }

  for (size_t i = 0; i < elem->child_count(); ++i) {
    const auto* child = elem->child_at(i);
    if (!child) {
      continue;
    }
    if (matches_selector(selector, child) || subtree_has_match(child, selector)) {
      return true;
    }
  }

  return false;
}

static bool matches_relative_has_selector(const std::string& selector,
                                          const Element* elem) {
  const std::string trimmed = trim_copy(selector);
  if (trimmed.empty() || !elem) {
    return false;
  }

  const char first = trimmed.front();
  if (first == '>') {
    const auto child_selector =
        parse_complex_selector(trim_copy(trimmed.substr(1)));
    for (size_t i = 0; i < elem->child_count(); ++i) {
      const auto* child = elem->child_at(i);
      if (child && matches_selector(child_selector, child)) {
        return true;
      }
    }
    return false;
  }

  if (first == '+') {
    const auto sibling_selector =
        parse_complex_selector(trim_copy(trimmed.substr(1)));
    return matches_selector(sibling_selector, next_element_sibling(elem));
  }

  if (first == '~') {
    const auto sibling_selector =
        parse_complex_selector(trim_copy(trimmed.substr(1)));
    for (auto* sibling = next_element_sibling(elem); sibling;
         sibling = next_element_sibling(sibling)) {
      if (matches_selector(sibling_selector, sibling)) {
        return true;
      }
    }
    return false;
  }

  return subtree_has_match(elem, parse_complex_selector(trimmed));
}

static bool matches_selector_step(const ComplexSelector& sel, size_t step_index,
                                  const Element* elem) {
  if (!elem || step_index >= sel.steps.size()) {
    return false;
  }
  if (!matches_compound_selector(sel.steps[step_index].selector, elem)) {
    return false;
  }
  if (step_index == 0) {
    return true;
  }

  switch (sel.steps[step_index].combinator_to_left) {
  case SelectorCombinator::Child:
    return matches_selector_step(sel, step_index - 1, elem->parent_elem());
  case SelectorCombinator::Descendant:
    for (auto* parent = elem->parent_elem(); parent; parent = parent->parent_elem()) {
      if (matches_selector_step(sel, step_index - 1, parent)) {
        return true;
      }
    }
    return false;
  case SelectorCombinator::AdjacentSibling:
    return matches_selector_step(sel, step_index - 1, previous_element_sibling(elem));
  case SelectorCombinator::GeneralSibling:
    for (auto* sibling = previous_element_sibling(elem); sibling;
         sibling = previous_element_sibling(sibling)) {
      if (matches_selector_step(sel, step_index - 1, sibling)) {
        return true;
      }
    }
    return false;
  case SelectorCombinator::None:
  default:
    return false;
  }
}

static bool matches_selector(const ComplexSelector& sel, const Element* elem) {
  if (sel.steps.empty()) {
    return false;
  }
  return matches_selector_step(sel, sel.steps.size() - 1, elem);
}

static bool rule_matches_media_conditions(const CSSRule& rule,
                                          const Element* elem) {
  if (rule.media_conditions.empty()) {
    return true;
  }

  const Box* box = elem ? elem->owner_box_ : nullptr;
  if (!box) {
    return false;
  }

  const float viewport_width = box->viewport_width();
  const float viewport_height = box->viewport_height();
  const MediaEnvironment& media_environment = box->media_environment();
  for (const auto& media_list : rule.media_conditions) {
    if (!media_query_list_matches(media_list, viewport_width,
                                  viewport_height, media_environment)) {
      return false;
    }
  }

  return true;
}

static bool rule_matches_container_conditions(
    const CSSRule& rule, const Element* elem,
    const std::unordered_map<const Element*, ContainerRegistration>&
        container_registrations) {
  if (rule.container_conditions.empty()) {
    return true;
  }

  for (const auto& container_list : rule.container_conditions) {
    if (!container_query_list_matches(container_list, elem,
                                      container_registrations)) {
      return false;
    }
  }

  return true;
}

static uint32_t calculate_specificity(const ParsedSelector &sel) {
  uint32_t spec = 0;
  if (sel.id.id != 0)
    spec += 100;
  spec += static_cast<uint32_t>(sel.classes.size()) * 10;
  spec += static_cast<uint32_t>(sel.attributes.size()) * 10;
  spec += static_cast<uint32_t>(sel.pseudo_classes.size()) * 10;
  spec += static_cast<uint32_t>(sel.pseudo_functions.size()) * 10;
  for (const auto& group : sel.is_groups) {
    uint32_t group_spec = 0;
    for (const auto& option : group) {
      group_spec = std::max(group_spec,
                            calculate_specificity(parse_complex_selector(option)));
    }
    spec += group_spec;
  }
  for (const auto& group : sel.has_groups) {
    uint32_t group_spec = 0;
    for (const auto& option : group) {
      group_spec = std::max(group_spec,
                            calculate_specificity(parse_complex_selector(option)));
    }
    spec += group_spec;
  }
  for (const auto& group : sel.not_groups) {
    uint32_t group_spec = 0;
    for (const auto& option : group) {
      group_spec = std::max(group_spec,
                            calculate_specificity(parse_complex_selector(option)));
    }
    spec += group_spec;
  }
  static const Symbol wildcard("*");
  if (sel.type.id != 0 && sel.type != wildcard)
    spec += 1;
  return spec;
}

static uint32_t calculate_specificity(const ComplexSelector& sel) {
  uint32_t spec = 0;
  for (const auto& step : sel.steps) {
    spec += calculate_specificity(step.selector);
  }
  return spec;
}

static void collect_selector_pseudo_classes(
    const ComplexSelector& sel,
    std::unordered_set<Symbol, SymbolHash>& out);

static void collect_selector_pseudo_classes(
    const ParsedSelector& sel,
    std::unordered_set<Symbol, SymbolHash>& out) {
  for (const auto& pseudo : sel.pseudo_classes) {
    out.insert(pseudo);
  }
  for (const auto& attr : sel.attributes) {
    static const std::pair<Symbol, Symbol> state_attrs[] = {
        {Symbol("__flex_state_focus_visible"), Symbol("focus-visible")},
        {Symbol("__flex_state_focus_within"), Symbol("focus-within")},
        {Symbol("__flex_state_selected"), Symbol("selected")},
        {Symbol("__flex_state_open"), Symbol("open")},
        {Symbol("__flex_state_modal"), Symbol("modal")},
        {Symbol("__flex_state_indeterminate"), Symbol("indeterminate")},
        {Symbol("__flex_state_placeholder_shown"), Symbol("placeholder-shown")},
        {Symbol("__flex_state_required"), Symbol("required")},
        {Symbol("__flex_state_optional"), Symbol("optional")},
        {Symbol("__flex_state_invalid"), Symbol("invalid")},
        {Symbol("__flex_state_valid"), Symbol("valid")},
        {Symbol("__flex_state_read_only"), Symbol("read-only")},
        {Symbol("__flex_state_read_write"), Symbol("read-write")},
    };
    for (const auto& [attr_name, pseudo] : state_attrs) {
      if (attr.name == attr_name) {
        out.insert(pseudo);
        break;
      }
    }
  }

  const auto collect_group =
      [&](const std::vector<std::vector<std::string>>& groups) {
        for (const auto& group : groups) {
          for (const auto& option : group) {
            collect_selector_pseudo_classes(parse_complex_selector(option), out);
          }
        }
      };

  collect_group(sel.is_groups);
  collect_group(sel.has_groups);
  collect_group(sel.where_groups);
  collect_group(sel.not_groups);
}

static void collect_selector_pseudo_classes(
    const ComplexSelector& sel,
    std::unordered_set<Symbol, SymbolHash>& out) {
  for (const auto& step : sel.steps) {
    collect_selector_pseudo_classes(step.selector, out);
  }
}

static void collect_selector_text_pseudo_classes(
    const std::string& selector,
    std::unordered_set<Symbol, SymbolHash>& out) {
  static const char* names[] = {
      "hover", "active", "focus", "focus-visible", "focus-within",
      "disabled", "checked", "selected", "open", "modal", "indeterminate",
      "placeholder-shown", "required", "optional", "invalid", "valid",
      "read-only", "read-write", "readonly", "readwrite"};

  for (const char* name : names) {
    const std::string token = std::string(":") + name;
    if (selector.find(token) != std::string::npos) {
      out.insert(Symbol(name));
    }
  }
}

class StyleEngine::Impl {
public:
  void parse_css(const std::string &css) {
    stylesheets_.clear();
    const StylesheetId id = next_stylesheet_id_++;
    stylesheets_.push_back({id, css, "<inline>"});
    rebuild_stylesheets();
  }

  void append_css(const std::string& css) {
    const StylesheetId id = next_stylesheet_id_++;
    stylesheets_.push_back({id, css, "<inline>"});
    const std::string normalized_css = normalize_css_for_parser(css);
    parse_css_fragment(normalized_css, {});
  }

  CssLoadResult load_stylesheet(const std::string& css,
                                const CssLoadOptions& options) {
    const StylesheetId id = next_stylesheet_id_++;
    CssLoadResult result;
    result.stylesheet_id = id;
    stylesheets_.push_back({id, css, options.source});
    collect_diagnostics_for_ = id;
    active_diagnostics_ = &result.diagnostics;
    try {
      rebuild_stylesheets();
    } catch (const std::exception& error) {
      active_source_css_ = nullptr;
      add_diagnostic(CssDiagnosticSeverity::Error, {}, {}, {}, error.what());
    }
    active_diagnostics_ = nullptr;
    collect_diagnostics_for_ = 0;

    if ((options.strict && !result.diagnostics.empty()) ||
        result.has_errors()) {
      stylesheets_.pop_back();
      rebuild_stylesheets();
      return result;
    }
    result.applied = true;
    return result;
  }

  CssLoadResult replace_stylesheet(StylesheetId stylesheet_id,
                                   const std::string& css,
                                   const CssLoadOptions& options) {
    CssLoadResult result;
    result.stylesheet_id = stylesheet_id;
    auto it = std::find_if(stylesheets_.begin(), stylesheets_.end(),
                           [stylesheet_id](const StylesheetRecord& sheet) {
                             return sheet.id == stylesheet_id;
                           });
    if (it == stylesheets_.end()) {
      result.diagnostics.push_back(
          {CssDiagnosticSeverity::Error, options.source, 0, 0, {}, {}, {},
           "unknown stylesheet id"});
      return result;
    }

    const StylesheetRecord previous = *it;
    it->css = css;
    it->source = options.source;
    collect_diagnostics_for_ = stylesheet_id;
    active_diagnostics_ = &result.diagnostics;
    try {
      rebuild_stylesheets();
    } catch (const std::exception& error) {
      active_source_css_ = nullptr;
      add_diagnostic(CssDiagnosticSeverity::Error, {}, {}, {}, error.what());
    }
    active_diagnostics_ = nullptr;
    collect_diagnostics_for_ = 0;

    if ((options.strict && !result.diagnostics.empty()) ||
        result.has_errors()) {
      *it = previous;
      rebuild_stylesheets();
      return result;
    }
    result.applied = true;
    return result;
  }

  bool remove_stylesheet(StylesheetId stylesheet_id) {
    auto it = std::find_if(stylesheets_.begin(), stylesheets_.end(),
                           [stylesheet_id](const StylesheetRecord& sheet) {
                             return sheet.id == stylesheet_id;
                           });
    if (it == stylesheets_.end()) {
      return false;
    }
    stylesheets_.erase(it);
    rebuild_stylesheets();
    return true;
  }

  void apply_styles(Element *elem) {
    if (!elem || !elem->computed_style)
      return;

    current_element_ = elem;
    container_registrations_[elem] = ContainerRegistration{};

    auto [baseline_it, inserted] = baseline_styles_.emplace(elem, *elem->computed_style);
    *elem->computed_style = baseline_it->second;

    // Step 1: Inherit properties from parent
    if (elem->parent_elem() && elem->parent_elem()->computed_style) {
      auto* parent_style = elem->parent_elem()->computed_style;
      auto* style = elem->computed_style;

      // Inherit variables
      for (const auto &[name, value] : parent_style->variables) {
        if (!is_internal_transition_var(name) &&
            !is_non_inherited_property_projection(name)) {
          style->variables[name] = value;
        }
      }

      // Inherit typographic properties
      style->text_color = parent_style->text_color;
      style->font_family = parent_style->font_family;
      style->font_size = parent_style->font_size;
      style->font_weight = parent_style->font_weight;
      style->font_style = parent_style->font_style;
      style->direction = parent_style->direction;
      style->text_align = parent_style->text_align;
      style->has_text_shadow = parent_style->has_text_shadow;
      style->text_shadow = parent_style->text_shadow;
      style->text_shadows = parent_style->text_shadows;
      style->visibility = parent_style->visibility;
    }

    struct MatchedDeclaration {
      uint32_t specificity = 0;
      const CSSDeclaration* declaration = nullptr;
    };

    // The cascade is declaration-based because !important can differ inside a rule.
    std::vector<MatchedDeclaration> matched_declarations;
    for (const auto &rule : rules_) {
      if (rule_matches_media_conditions(rule, elem) &&
          rule_matches_container_conditions(rule, elem, container_registrations_) &&
          matches_selector(rule.selector, elem)) {
        for (const auto& declaration : rule.properties) {
          matched_declarations.push_back({rule.specificity, &declaration});
        }
      }
    }
    std::stable_sort(
        matched_declarations.begin(), matched_declarations.end(),
        [](const MatchedDeclaration& lhs, const MatchedDeclaration& rhs) {
          if (lhs.declaration->important != rhs.declaration->important) {
            return !lhs.declaration->important;
          }
          if (lhs.specificity != rhs.specificity) {
            return lhs.specificity < rhs.specificity;
          }
          return lhs.declaration->source_order < rhs.declaration->source_order;
        });

    // Step 3: Pass 1 - Apply CSS Variables (--*)
    for (const auto& matched : matched_declarations) {
      const auto& declaration = *matched.declaration;
      if (declaration.property.size() > 2 && declaration.property[0] == '-' &&
          declaration.property[1] == '-') {
        apply_property(declaration.property, declaration.value,
                       elem->computed_style);
      }
    }

    // Element custom properties are the inline declaration layer consumed by
    // C++ bindings and designers. They remain separate from ComputedStyle so
    // recomputing the cascade cannot discard their source values.
    for (const auto& [name, value] : elem->custom_properties()) {
      elem->computed_style->variables[name] = value;
    }

    // Pass 1b: direction must be stable before logical properties map to
    // physical sides, regardless of declaration order.
    for (const auto& matched : matched_declarations) {
      const auto& declaration = *matched.declaration;
      if (declaration.property == "direction") {
        apply_property(declaration.property, declaration.value,
                       elem->computed_style);
      }
    }

    // Pass 2a: Resolve final text color before properties that consume currentColor.
    for (const auto& matched : matched_declarations) {
      const auto& declaration = *matched.declaration;
      if (declaration.property == "color") {
        apply_property(declaration.property, declaration.value,
                       elem->computed_style);
      }
    }

    // Pass 2b: Apply other properties after currentColor has a stable source.
    for (const auto& matched : matched_declarations) {
      const auto& declaration = *matched.declaration;
      const auto& prop = declaration.property;
      if (!(prop.size() > 2 && prop[0] == '-' && prop[1] == '-') &&
          prop != "color" && prop != "direction") {
        apply_property(prop, declaration.value, elem->computed_style);
      }
    }

    // Composite controls paint their visible control surface through stable
    // child parts. Project author-facing host box properties into inherited
    // internal tokens so normal CSS remains useful without overriding a
    // directly styled part or a legacy widget variable.
    const std::string* role = elem->attribute("role");
    if (role && (*role == "checkbox" || *role == "switch")) {
      bool has_background = false;
      bool has_border_color = false;
      bool has_border_width = false;
      bool has_border_radius = false;
      for (const auto& matched : matched_declarations) {
        const auto& prop = matched.declaration->property;
        has_background = has_background || prop == "background" ||
                         prop == "background-color";
        has_border_color = has_border_color || prop == "border" ||
                           prop == "border-color" ||
                           prop == "border-top-color" ||
                           prop == "border-right-color" ||
                           prop == "border-bottom-color" ||
                           prop == "border-left-color";
        has_border_width = has_border_width || prop == "border" ||
                           prop == "border-width" ||
                           prop == "border-top-width" ||
                           prop == "border-right-width" ||
                           prop == "border-bottom-width" ||
                           prop == "border-left-width";
        has_border_radius = has_border_radius || prop == "border-radius" ||
                            prop == "border-top-left-radius" ||
                            prop == "border-top-right-radius" ||
                            prop == "border-bottom-right-radius" ||
                            prop == "border-bottom-left-radius";
      }
      auto* style = elem->computed_style;
      if (has_background) {
        style->variables[Symbol("--host-control-background")] =
            color_to_css_variable_value(style->background_color);
      }
      if (has_border_color) {
        style->variables[Symbol("--host-control-border-color")] =
            color_to_css_variable_value(style->border_color);
      }
      if (has_border_width) {
        style->variables[Symbol("--host-control-border-width")] =
            std::to_string(*std::max_element(std::begin(style->border_width),
                                             std::end(style->border_width))) +
            "px";
      }
      if (has_border_radius) {
        style->variables[Symbol("--host-control-border-radius")] =
            std::to_string(style->border_radius[0]) + "px";
      }
    }

    current_element_ = nullptr;
  }

  void clear() {
    stylesheets_.clear();
    rules_.clear();
    pseudo_dependencies_.clear();
    keyframes_.clear();
    container_registrations_.clear();
  }

  void clear_baseline_styles() {
    baseline_styles_.clear();
  }

  const std::vector<AnimationKeyframeStep>* keyframes(
      const std::string& name) const {
    auto it = keyframes_.find(name);
    return it != keyframes_.end() ? &it->second : nullptr;
  }

  bool uses_pseudo_class(Symbol pseudo) const {
    return pseudo_dependencies_.count(pseudo) > 0;
  }

  bool matches(const Element* elem, const std::string& selector) const {
    if (!elem) {
      return false;
    }
    const std::string trimmed = trim_copy(selector);
    if (trimmed.empty()) {
      return false;
    }
    for (const auto& item : split_selector_list(trimmed)) {
      const std::string candidate = trim_copy(item);
      if (!candidate.empty() &&
          matches_selector(parse_complex_selector(candidate), elem)) {
        return true;
      }
    }
    return false;
  }

private:
  struct StylesheetRecord {
    StylesheetId id = 0;
    std::string css;
    std::string source;
  };

  std::vector<StylesheetRecord> stylesheets_;
  std::vector<CSSRule> rules_;
  std::unordered_set<Symbol, SymbolHash> pseudo_dependencies_;
  std::unordered_map<std::string, std::vector<AnimationKeyframeStep>> keyframes_;
  std::unordered_map<const Element *, ComputedStyle> baseline_styles_;
  std::unordered_map<const Element*, ContainerRegistration> container_registrations_;
  const Element* current_element_ = nullptr;
  uint64_t next_declaration_order_ = 0;
  StylesheetId next_stylesheet_id_ = 1;
  StylesheetId active_stylesheet_id_ = 0;
  StylesheetId collect_diagnostics_for_ = 0;
  std::string active_source_ = "<inline>";
  const std::string* active_source_css_ = nullptr;
  std::vector<CssDiagnostic>* active_diagnostics_ = nullptr;

  void rebuild_stylesheets() {
    rules_.clear();
    keyframes_.clear();
    pseudo_dependencies_.clear();
    container_registrations_.clear();
    next_declaration_order_ = 0;
    for (const auto& sheet : stylesheets_) {
      active_stylesheet_id_ = sheet.id;
      active_source_ = sheet.source.empty() ? "<inline>" : sheet.source;
      const std::string normalized_css = normalize_css_for_parser(sheet.css);
      active_source_css_ = &normalized_css;
      parse_css_fragment(normalized_css, {});
      active_source_css_ = nullptr;
    }
    active_stylesheet_id_ = 0;
  }

  std::pair<size_t, size_t> source_position(const lxb_char_t* position) const {
    if (!active_source_css_ || !position || active_source_css_->empty()) {
      return {0, 0};
    }
    const auto* begin = reinterpret_cast<const lxb_char_t*>(active_source_css_->data());
    const auto* end = begin + active_source_css_->size();
    if (position < begin || position > end) {
      return {0, 0};
    }
    const size_t offset = static_cast<size_t>(position - begin);
    size_t line = 1;
    size_t column = 1;
    for (size_t i = 0; i < offset; ++i) {
      if ((*active_source_css_)[i] == '\n') {
        ++line;
        column = 1;
      } else {
        ++column;
      }
    }
    return {line, column};
  }

  void add_diagnostic(CssDiagnosticSeverity severity,
                      const std::string& selector,
                      const std::string& property,
                      const std::string& value,
                      const std::string& message,
                      const lxb_char_t* position = nullptr) {
    if (!active_diagnostics_ || active_stylesheet_id_ != collect_diagnostics_for_) {
      return;
    }
    const auto [line, column] = source_position(position);
    active_diagnostics_->push_back({severity, active_source_, line, column,
                                    selector, property, value, message});
  }

  static bool starts_with_at_rule_keyword(const std::string& css, size_t pos,
                                          const std::string& keyword) {
    if (pos + keyword.size() > css.size()) {
      return false;
    }
    for (size_t i = 0; i < keyword.size(); ++i) {
      if (std::tolower(static_cast<unsigned char>(css[pos + i])) !=
          std::tolower(static_cast<unsigned char>(keyword[i]))) {
        return false;
      }
    }
    return true;
  }

  static bool starts_with_media_at_rule(const std::string& css, size_t pos) {
    return starts_with_at_rule_keyword(css, pos, "@media");
  }

  static bool starts_with_container_at_rule(const std::string& css, size_t pos) {
    return starts_with_at_rule_keyword(css, pos, "@container");
  }

  static bool starts_with_supports_at_rule(const std::string& css, size_t pos) {
    return starts_with_at_rule_keyword(css, pos, "@supports");
  }

  static bool starts_with_keyframes_at_rule(const std::string& css, size_t pos) {
    return starts_with_at_rule_keyword(css, pos, "@keyframes") ||
           starts_with_at_rule_keyword(css, pos, "@-webkit-keyframes");
  }

  static bool has_outer_parentheses(const std::string& expr) {
    if (expr.size() < 2 || expr.front() != '(' || expr.back() != ')') {
      return false;
    }
    int depth = 0;
    for (size_t i = 0; i < expr.size(); ++i) {
      if (expr[i] == '(') {
        ++depth;
      } else if (expr[i] == ')') {
        --depth;
        if (depth == 0 && i + 1 < expr.size()) {
          return false;
        }
      }
      if (depth < 0) {
        return false;
      }
    }
    return depth == 0;
  }

  static bool split_supports_expression(const std::string& expr,
                                        const std::string& op,
                                        std::vector<std::string>& parts) {
    const std::string lowered = to_lower_copy(expr);
    const std::string needle = " " + op + " ";
    std::string current;
    int paren_depth = 0;

    for (size_t i = 0; i < expr.size(); ++i) {
      const char ch = expr[i];
      if (ch == '(') {
        ++paren_depth;
      } else if (ch == ')' && paren_depth > 0) {
        --paren_depth;
      }

      if (paren_depth == 0 &&
          i + needle.size() <= lowered.size() &&
          lowered.compare(i, needle.size(), needle) == 0) {
        const std::string trimmed = trim_copy(current);
        if (trimmed.empty()) {
          return false;
        }
        parts.push_back(trimmed);
        current.clear();
        i += needle.size() - 1;
        continue;
      }

      current.push_back(ch);
    }

    const std::string tail = trim_copy(current);
    if (tail.empty()) {
      return false;
    }
    parts.push_back(tail);
    return parts.size() > 1;
  }

  static bool supports_property_name(std::string property) {
    property = to_lower_copy(trim_copy(property));
    if (property.empty()) {
      return false;
    }
    if (property.rfind("--", 0) == 0) {
      return true;
    }
    if (property == "-webkit-backdrop-filter") {
      property = "backdrop-filter";
    } else if (property == "-webkit-appearance") {
      property = "appearance";
    }

    static const std::unordered_set<std::string> supported = {
        "width",        "height",        "inline-size",     "block-size",
        "aspect-ratio", "min-width",     "max-width",       "min-height",
        "max-height",   "min-inline-size","max-inline-size", "min-block-size",
        "max-block-size","padding",
        "padding-top",  "padding-right", "padding-bottom",  "padding-left",
        "padding-inline", "padding-inline-start", "padding-inline-end",
        "padding-block", "padding-block-start", "padding-block-end",
        "margin",       "margin-top",    "margin-right",    "margin-bottom",
        "margin-left",  "margin-inline", "margin-inline-start",
        "margin-inline-end", "margin-block", "margin-block-start",
        "margin-block-end", "border",        "border-top",      "border-right",
        "border-bottom","border-left",   "border-inline",   "border-inline-start",
        "border-inline-end","border-block","border-block-start","border-block-end",
        "border-style", "border-width",  "border-top-width","border-right-width",
        "border-bottom-width","border-left-width","border-top-style","border-right-style",
        "border-bottom-style","border-left-style","border-top-color","border-right-color",
        "border-bottom-color","border-left-color","border-inline-width",
        "border-inline-start-width","border-inline-end-width","border-block-width",
        "border-block-start-width","border-block-end-width","border-inline-style",
        "border-inline-start-style","border-inline-end-style","border-block-style",
        "border-block-start-style","border-block-end-style","border-inline-color",
        "border-inline-start-color","border-inline-end-color","border-block-color",
        "border-block-start-color","border-block-end-color","border-radius",
        "border-top-left-radius","border-top-right-radius",
        "border-bottom-right-radius","border-bottom-left-radius",
        "border-start-start-radius","border-start-end-radius",
        "border-end-end-radius","border-end-start-radius",
        "border-color","box-sizing",   "display",
        "position",     "inset",         "inset-x",         "inset-y",
        "inset-inline", "inset-inline-start", "inset-inline-end",
        "inset-block", "inset-block-start", "inset-block-end",
        "top",          "right",         "bottom",          "left",
        "z-index",      "flex-direction","justify-content", "align-items",
        "align-content","align-self",    "justify-items",   "justify-self",
        "place-content","place-items",   "place-self",      "gap",
        "flex-grow",    "flex-shrink",   "flex-basis",      "flex-wrap",
        "flex",         "grid-template-columns",            "grid-template-rows",
        "grid-template-areas", "grid-auto-flow","grid-auto-columns", "grid-auto-rows",
        "grid-column",  "grid-row",      "grid-area",       "row-gap",         "column-gap",
        "font-size",    "font-family",   "font-weight",     "font-style",
        "line-height",  "direction",     "unicode-bidi",    "text-align",
        "text-align-last",
        "text-decoration",
        "text-decoration-line",
        "text-decoration-color","text-decoration-style","text-decoration-thickness",
        "text-underline-offset","text-transform",
        "letter-spacing","word-spacing","text-indent","tab-size","text-shadow","font-variant-numeric","vertical-align","white-space",    "text-wrap",    "text-wrap-mode",    "text-overflow",
        "overflow-wrap","word-break",   "max-lines",    "line-clamp",
        "-webkit-line-clamp",
        "object-fit",   "object-position","caret-color",    "accent-color",
        "color",        "background-color","background-image","background-position",
        "background-position-x","background-position-y",
        "background-size","background-repeat","background-clip","background-origin",
        "background", "clip-path", "filter",
        "backdrop-filter","-webkit-backdrop-filter","cursor",      "appearance",
        "-webkit-appearance","user-select","-webkit-user-select","touch-action","color-scheme",
        "-webkit-color-scheme","opacity",
        "visibility",   "pointer-events","overflow",        "overflow-x",
        "overflow-y",   "scrollbar-width","scrollbar-color","translate",
        "scale",        "rotate",       "transform",       "transform-origin","box-shadow",
        "outline",      "outline-width", "outline-style", "outline-color",
        "outline-offset", "content",
        "ring",         "ring-width",    "ring-color",      "ring-offset",
        "ring-offset-color",
        "scroll-padding","scroll-padding-top","scroll-padding-right",
        "scroll-padding-bottom","scroll-padding-left","scroll-padding-inline",
        "scroll-padding-inline-start","scroll-padding-inline-end",
        "scroll-padding-block","scroll-padding-block-start",
        "scroll-padding-block-end","scroll-margin","scroll-margin-top",
        "scroll-margin-right","scroll-margin-bottom","scroll-margin-left",
        "scroll-margin-inline","scroll-margin-inline-start",
        "scroll-margin-inline-end","scroll-margin-block",
        "scroll-margin-block-start","scroll-margin-block-end",
        "transition",   "transition-property","transition-duration",
        "transition-delay","transition-timing-function",    "animation",
        "animation-name","animation-duration","animation-delay",
        "animation-timing-function","animation-iteration-count",
        "animation-fill-mode","animation-direction","animation-play-state",
        "container-type","container-name"};
    return supported.count(property) > 0;
  }

  static bool supports_selector_query(const std::string& query) {
    const std::string trimmed = trim_copy(query);
    if (trimmed.empty()) {
      return false;
    }
    return !parse_complex_selector(trimmed).steps.empty();
  }

  static bool evaluate_supports_condition(const std::string& raw) {
    std::string expr = trim_copy(raw);
    if (expr.empty()) {
      return false;
    }

    while (has_outer_parentheses(expr)) {
      expr = trim_copy(expr.substr(1, expr.size() - 2));
    }

    const std::string lowered = to_lower_copy(expr);
    if (lowered.rfind("not ", 0) == 0) {
      return !evaluate_supports_condition(expr.substr(4));
    }

    std::vector<std::string> parts;
    if (split_supports_expression(expr, "or", parts)) {
      for (const auto& part : parts) {
        if (evaluate_supports_condition(part)) {
          return true;
        }
      }
      return false;
    }

    parts.clear();
    if (split_supports_expression(expr, "and", parts)) {
      for (const auto& part : parts) {
        if (!evaluate_supports_condition(part)) {
          return false;
        }
      }
      return true;
    }

    const std::string atom = trim_copy(expr);
    if (atom.size() > 10 &&
        to_lower_copy(atom).rfind("selector(", 0) == 0 &&
        atom.back() == ')') {
      return supports_selector_query(atom.substr(9, atom.size() - 10));
    }

    const size_t colon = find_top_level_colon(atom);
    if (colon == std::string::npos) {
      return false;
    }
    const std::string property = trim_copy(atom.substr(0, colon));
    const std::string value = trim_copy(atom.substr(colon + 1));
    return !value.empty() && supports_property_name(property);
  }

  static bool extract_at_rule_block(const std::string& css, size_t at_pos,
                                    size_t keyword_len, std::string& prelude,
                                    std::string& block, size_t& rule_end) {
    const size_t prelude_start = at_pos + keyword_len;
    const size_t block_open = css.find('{', prelude_start);
    if (block_open == std::string::npos) {
      return false;
    }

    int block_depth = 1;
    size_t block_close = block_open + 1;
    for (; block_close < css.size(); ++block_close) {
      if (css[block_close] == '{') {
        ++block_depth;
      } else if (css[block_close] == '}') {
        --block_depth;
        if (block_depth == 0) {
          break;
        }
      }
    }
    if (block_depth != 0 || block_close >= css.size()) {
      return false;
    }

    prelude = trim_copy(css.substr(prelude_start, block_open - prelude_start));
    block = css.substr(block_open + 1, block_close - block_open - 1);
    rule_end = block_close + 1;
    return true;
  }

  static void parse_declarations_from_block(
      const std::string& block, DeclarationList& props) {
    std::string current;
    int paren_depth = 0;

    auto flush_current = [&]() {
      const std::string decl = trim_copy(current);
      current.clear();
      if (decl.empty()) {
        return;
      }

      size_t colon = std::string::npos;
      int nested_paren_depth = 0;
      for (size_t i = 0; i < decl.size(); ++i) {
        if (decl[i] == '(') {
          ++nested_paren_depth;
        } else if (decl[i] == ')' && nested_paren_depth > 0) {
          --nested_paren_depth;
        } else if (decl[i] == ':' && nested_paren_depth == 0) {
          colon = i;
          break;
        }
      }
      if (colon == std::string::npos) {
        return;
      }

      const std::string name = trim_copy(decl.substr(0, colon));
      const std::string value = trim_copy(decl.substr(colon + 1));
      if (!name.empty() && !value.empty()) {
        props.push_back({name, value, false, 0});
      }
    };

    for (char ch : block) {
      if (ch == '(') {
        ++paren_depth;
      } else if (ch == ')' && paren_depth > 0) {
        --paren_depth;
      }

      if (ch == ';' && paren_depth == 0) {
        flush_current();
        continue;
      }

      current.push_back(ch);
    }

    flush_current();
  }

  static void parse_declarations_from_block(
      const std::string& block, std::map<std::string, std::string>& props) {
    std::string current;
    int paren_depth = 0;

    auto flush_current = [&]() {
      const std::string decl = trim_copy(current);
      current.clear();
      if (decl.empty()) {
        return;
      }

      size_t colon = std::string::npos;
      int nested_paren_depth = 0;
      for (size_t i = 0; i < decl.size(); ++i) {
        if (decl[i] == '(') {
          ++nested_paren_depth;
        } else if (decl[i] == ')' && nested_paren_depth > 0) {
          --nested_paren_depth;
        } else if (decl[i] == ':' && nested_paren_depth == 0) {
          colon = i;
          break;
        }
      }
      if (colon == std::string::npos) {
        return;
      }

      const std::string name = trim_copy(decl.substr(0, colon));
      const std::string value = trim_copy(decl.substr(colon + 1));
      if (!name.empty() && !value.empty()) {
        props[name] = value;
      }
    };

    for (char ch : block) {
      if (ch == '(') {
        ++paren_depth;
      } else if (ch == ')' && paren_depth > 0) {
        --paren_depth;
      }

      if (ch == ';' && paren_depth == 0) {
        flush_current();
        continue;
      }

      current.push_back(ch);
    }

    flush_current();
  }

  static std::vector<float> parse_keyframe_offsets(
      const std::string& selector_text) {
    std::vector<float> offsets;
    for (const auto& token : split_selector_list(selector_text)) {
      const std::string lowered = to_lower_copy(trim_copy(token));
      if (lowered == "from") {
        offsets.push_back(0.0f);
      } else if (lowered == "to") {
        offsets.push_back(1.0f);
      } else if (!lowered.empty() && lowered.back() == '%') {
        offsets.push_back(std::stof(lowered.substr(0, lowered.size() - 1)) /
                          100.0f);
      }
    }
    return offsets;
  }

  void parse_keyframes_rule(const std::string& name, const std::string& block) {
    if (name.empty() || block.empty()) {
      return;
    }

    std::vector<AnimationKeyframeStep> frames;
    size_t segment_start = 0;
    size_t i = 0;
    int brace_depth = 0;

    while (i < block.size()) {
      if (block[i] == '{') {
        if (brace_depth == 0) {
          const std::string selector_text =
              trim_copy(block.substr(segment_start, i - segment_start));
          int inner_depth = 1;
          const size_t content_start = i + 1;
          size_t block_close = content_start;
          for (; block_close < block.size(); ++block_close) {
            if (block[block_close] == '{') {
              ++inner_depth;
            } else if (block[block_close] == '}') {
              --inner_depth;
              if (inner_depth == 0) {
                break;
              }
            }
          }
          if (inner_depth != 0 || block_close >= block.size()) {
            break;
          }

          std::map<std::string, std::string> props;
          parse_declarations_from_block(
              block.substr(content_start, block_close - content_start), props);
          if (!props.empty()) {
            for (float offset : parse_keyframe_offsets(selector_text)) {
              frames.push_back(AnimationKeyframeStep{offset, props});
            }
          }

          i = block_close + 1;
          segment_start = i;
          continue;
        }
        ++brace_depth;
      } else if (block[i] == '}' && brace_depth > 0) {
        --brace_depth;
      }
      ++i;
    }

    if (frames.empty()) {
      return;
    }

    std::stable_sort(frames.begin(), frames.end(),
                     [](const auto& lhs, const auto& rhs) {
                       return lhs.offset < rhs.offset;
                     });
    keyframes_[name] = std::move(frames);
  }

  void parse_plain_css_fragment(
      const std::string& css,
      const std::vector<MediaQueryList>& media_conditions,
      const std::vector<ContainerQueryList>& container_conditions) {
    if (trim_copy(css).empty()) {
      return;
    }

    LexborCSSParser parser;
    auto* sheet = parser.parse(css.c_str(), css.size());
    extract_rules(sheet, media_conditions, container_conditions, css);
    if (auto* log = parser.log()) {
      for (size_t i = 0; i < lxb_css_log_length(log); ++i) {
        auto* message = static_cast<lxb_css_log_message_t*>(
            lexbor_array_obj_get(&log->messages, i));
        if (!message || !message->text.data) {
          continue;
        }
        const CssDiagnosticSeverity severity =
            message->type == LXB_CSS_LOG_ERROR ||
                    message->type == LXB_CSS_LOG_SYNTAX_ERROR
                ? CssDiagnosticSeverity::Error
                : CssDiagnosticSeverity::Warning;
        add_diagnostic(
            severity, {}, {}, {},
            std::string(reinterpret_cast<const char*>(message->text.data),
                        message->text.length));
      }
    }
    lxb_css_stylesheet_destroy(sheet, true);
  }

  void parse_css_fragment(const std::string& css,
                          const std::vector<MediaQueryList>& media_conditions,
                          const std::vector<ContainerQueryList>&
                              container_conditions = {}) {
    size_t segment_start = 0;
    size_t i = 0;
    int brace_depth = 0;

    while (i < css.size()) {
      const char ch = css[i];
      if (ch == '{') {
        ++brace_depth;
        ++i;
        continue;
      }
      if (ch == '}') {
        if (brace_depth > 0) {
          --brace_depth;
        }
        ++i;
        continue;
      }

      if (brace_depth == 0 && starts_with_media_at_rule(css, i)) {
        parse_plain_css_fragment(css.substr(segment_start, i - segment_start),
                                 media_conditions, container_conditions);

        std::string prelude;
        std::string block;
        size_t rule_end = i;
        if (!extract_at_rule_block(css, i, 6, prelude, block, rule_end)) {
          break;
        }
        MediaQueryList media_list = parse_media_query_list(prelude);
        if (!media_list.queries.empty()) {
          std::vector<MediaQueryList> nested_media_conditions =
              media_conditions;
          nested_media_conditions.push_back(std::move(media_list));
          parse_css_fragment(block, nested_media_conditions, container_conditions);
        }

        i = rule_end;
        segment_start = i;
        continue;
      }

      if (brace_depth == 0 && starts_with_container_at_rule(css, i)) {
        parse_plain_css_fragment(css.substr(segment_start, i - segment_start),
                                 media_conditions, container_conditions);

        std::string prelude;
        std::string block;
        size_t rule_end = i;
        if (!extract_at_rule_block(css, i, 10, prelude, block, rule_end)) {
          break;
        }
        ContainerQueryList container_list = parse_container_query_list(prelude);
        if (!container_list.queries.empty()) {
          std::vector<ContainerQueryList> nested_container_conditions =
              container_conditions;
          nested_container_conditions.push_back(std::move(container_list));
          parse_css_fragment(block, media_conditions, nested_container_conditions);
        }

        i = rule_end;
        segment_start = i;
        continue;
      }

      if (brace_depth == 0 && starts_with_supports_at_rule(css, i)) {
        parse_plain_css_fragment(css.substr(segment_start, i - segment_start),
                                 media_conditions, container_conditions);

        std::string prelude;
        std::string block;
        size_t rule_end = i;
        if (!extract_at_rule_block(css, i, 9, prelude, block, rule_end)) {
          break;
        }
        if (evaluate_supports_condition(prelude)) {
          parse_css_fragment(block, media_conditions, container_conditions);
        }

        i = rule_end;
        segment_start = i;
        continue;
      }

      if (brace_depth == 0 && starts_with_keyframes_at_rule(css, i)) {
        parse_plain_css_fragment(css.substr(segment_start, i - segment_start),
                                 media_conditions, container_conditions);

        const size_t keyword_len =
            starts_with_at_rule_keyword(css, i, "@-webkit-keyframes") ? 18 : 10;
        std::string prelude;
        std::string block;
        size_t rule_end = i;
        if (!extract_at_rule_block(css, i, keyword_len, prelude, block,
                                   rule_end)) {
          break;
        }

        parse_keyframes_rule(prelude, block);
        i = rule_end;
        segment_start = i;
        continue;
      }

      ++i;
    }

    parse_plain_css_fragment(css.substr(segment_start), media_conditions,
                             container_conditions);
  }

  void extract_rules(lxb_css_stylesheet_t *sheet,
                     const std::vector<MediaQueryList>& media_conditions = {},
                     const std::vector<ContainerQueryList>&
                         container_conditions = {},
                     const std::string& source_css = "") {
    if (!sheet || !sheet->root)
      return;
    lxb_css_rule_t *rule = nullptr;
    if (sheet->root->type == LXB_CSS_RULE_LIST) {
      rule = lxb_css_rule_list(sheet->root)->first;
    } else {
      rule = sheet->root;
    }
    while (rule) {
      if (rule->type == LXB_CSS_RULE_STYLE) {
        extract_style_rule(lxb_css_rule_style(rule), media_conditions,
                           container_conditions, source_css);
      } else if (rule->type == LXB_CSS_RULE_AT_RULE) {
        extract_at_rule(lxb_css_rule_at(rule), media_conditions,
                        container_conditions, source_css);
      }
      rule = rule->next;
    }
  }

  void extract_style_rule(
      lxb_css_rule_style_t *style_rule,
      const std::vector<MediaQueryList>& media_conditions,
      const std::vector<ContainerQueryList>& container_conditions,
      const std::string& source_css) {
    if (!style_rule)
      return;
    std::string selector_text = get_selector_text(style_rule);
    if (selector_text.empty())
      return;

    DeclarationList props;
    extract_declarations(style_rule, props);
    props.erase(std::remove_if(props.begin(), props.end(),
                               [&](const CSSDeclaration& declaration) {
      if (!supports_property_name(declaration.property)) {
        add_diagnostic(CssDiagnosticSeverity::Warning, selector_text,
                       declaration.property, declaration.value,
                       "unsupported CSS property",
                       style_rule->prelude_begin < source_css.size()
                           ? reinterpret_cast<const lxb_char_t*>(
                                 source_css.data() + style_rule->prelude_begin)
                           : nullptr);
      }
      const std::string property = to_lower_copy(declaration.property);
      const std::string value = to_lower_copy(trim_copy(declaration.value));
      const bool is_size = property == "width" || property == "height" ||
                           property == "inline-size" || property == "block-size";
      if (is_size && value != "auto" && value.find("var(") == std::string::npos &&
          std::isnan(detail::parse_css_length(value))) {
        add_diagnostic(CssDiagnosticSeverity::Warning, selector_text,
                       declaration.property, declaration.value,
                       "invalid CSS size value",
                       style_rule->prelude_begin < source_css.size()
                           ? reinterpret_cast<const lxb_char_t*>(
                                 source_css.data() + style_rule->prelude_begin)
                           : nullptr);
        return true;
      }
      return false;
    }), props.end());
    if (!props.empty()) {
      for (const auto& selector : split_selector_list(selector_text)) {
        collect_selector_text_pseudo_classes(selector, pseudo_dependencies_);
        collect_selector_pseudo_classes(parse_complex_selector(selector),
                                        pseudo_dependencies_);
        std::string normalized_selector = selector;
        DeclarationList normalized_props = props;
        normalize_selector_props(normalized_selector, normalized_props);
        collect_selector_text_pseudo_classes(normalized_selector,
                                             pseudo_dependencies_);
        if (normalized_selector.empty() || normalized_props.empty()) {
          continue;
        }
        ComplexSelector parsed_sel = parse_complex_selector(normalized_selector);
        CSSRule rule{parsed_sel, normalized_props,
                     calculate_specificity(parsed_sel), media_conditions,
                     container_conditions};
        collect_selector_pseudo_classes(parsed_sel, pseudo_dependencies_);
        rules_.push_back(rule);
      }
    }
  }

  void extract_at_rule(
      lxb_css_rule_at_t* at_rule,
      const std::vector<MediaQueryList>& parent_media_conditions,
      const std::vector<ContainerQueryList>& parent_container_conditions,
      const std::string& source_css) {
    if (!at_rule || at_rule->type != LXB_CSS_AT_RULE_MEDIA) {
      return;
    }

    std::string prelude;
    std::string block;
    if (!extract_media_prelude_and_block(at_rule, source_css, prelude, block)) {
      return;
    }

    MediaQueryList media_list = parse_media_query_list(prelude);
    if (media_list.queries.empty()) {
      return;
    }

    std::vector<MediaQueryList> nested_media_conditions = parent_media_conditions;
    nested_media_conditions.push_back(std::move(media_list));

    LexborCSSParser parser;
    const std::string normalized_block = normalize_css_for_parser(block);
    auto* nested_sheet =
        parser.parse(normalized_block.c_str(), normalized_block.size());
    extract_rules(nested_sheet, nested_media_conditions,
                  parent_container_conditions, normalized_block);
    lxb_css_stylesheet_destroy(nested_sheet, true);
  }

  std::string get_selector_text(lxb_css_rule_style_t *style_rule) {
    if (!style_rule || !style_rule->selector)
      return "";
    lexbor_str_t str = {0};
    auto append = [](const lxb_char_t *data, size_t len, void *ctx) -> lxb_status_t {
      auto *s = static_cast<lexbor_str_t *>(ctx);
      size_t new_len = (s->length ? s->length : 0) + len;
      auto *new_data = static_cast<lxb_char_t *>(realloc(s->data, new_len + 1));
      if (!new_data)
        return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
      memcpy(new_data + (s->length ? s->length : 0), data, len);
      new_data[new_len] = 0;
      s->data = new_data;
      s->length = new_len;
      return LXB_STATUS_OK;
    };

    size_t selector_count = 0;
    for (auto *list = style_rule->selector; list != nullptr && selector_count < 64;
         list = list->next, ++selector_count) {
      if (selector_count > 0) {
        append(reinterpret_cast<const lxb_char_t *>(", "), 2, &str);
      }
      lxb_css_selector_serialize_list(list, append, &str);
    }

    std::string result;
    if (str.data) {
      result = std::string(reinterpret_cast<char *>(str.data), str.length);
      free(str.data);
    }
    return result;
  }

  void extract_declarations(lxb_css_rule_style_t *style_rule,
                            DeclarationList &props) {
    if (!style_rule || !style_rule->declarations)
      return;
    lxb_css_rule_t *rule = style_rule->declarations->first;
    while (rule) {
      if (rule->type == LXB_CSS_RULE_DECLARATION) {
        auto *decl = lxb_css_rule_declaration(rule);
        std::string serialized = serialize_declaration(decl);
        size_t colon = serialized.find(':');
        if (colon != std::string::npos) {
          std::string name = serialized.substr(0, colon);
          std::string value = serialized.substr(colon + 1);
          auto trim = [](std::string &s) {
            size_t start = s.find_first_not_of(" \t\n\r");
            size_t end = s.find_last_not_of(" \t\n\r");
            if (start != std::string::npos)
              s = s.substr(start, end - start + 1);
          };
          trim(name);
          trim(value);
          if (!name.empty() && !value.empty()) {
            if (decl->important) {
              const std::string marker = "!important";
              const std::string lowered = to_lower_copy(value);
              if (lowered.size() >= marker.size() &&
                  lowered.compare(lowered.size() - marker.size(), marker.size(),
                                  marker) == 0) {
                value = trim_copy(value.substr(0, value.size() - marker.size()));
              }
            }
            props.push_back(
                {name, value, decl->important, next_declaration_order_++});
          }
        }
      }
      rule = rule->next;
    }
  }

  void normalize_selector_props(std::string& selector,
                                DeclarationList& props) {
    enum class PseudoKind {
      None,
      Placeholder,
      Before,
      After,
      Selection,
    };

    auto detect_pseudo = [&](const std::string* tokens, size_t count,
                             PseudoKind kind, size_t& out_pos,
                             size_t& out_len) -> PseudoKind {
      for (size_t i = 0; i < count; ++i) {
        out_pos = selector.find(tokens[i]);
        if (out_pos != std::string::npos) {
          out_len = tokens[i].size();
          return kind;
        }
      }
      return PseudoKind::None;
    };

    const std::string placeholder_tokens[] = {
        "[__flex_pseudo_placeholder]", "::placeholder", ":placeholder"};
    const std::string before_tokens[] = {"[__flex_pseudo_before]", "::before", ":before"};
    const std::string after_tokens[] = {"[__flex_pseudo_after]", "::after", ":after"};
    const std::string selection_tokens[] = {
        "[__flex_pseudo_selection]", "::selection", ":selection"};

    size_t pseudo_pos = std::string::npos;
    size_t pseudo_len = 0;
    PseudoKind pseudo = detect_pseudo(placeholder_tokens,
                                      sizeof(placeholder_tokens) /
                                          sizeof(placeholder_tokens[0]),
                                      PseudoKind::Placeholder, pseudo_pos,
                                      pseudo_len);
    if (pseudo == PseudoKind::None) {
      pseudo = detect_pseudo(before_tokens,
                             sizeof(before_tokens) / sizeof(before_tokens[0]),
                             PseudoKind::Before, pseudo_pos, pseudo_len);
    }
    if (pseudo == PseudoKind::None) {
      pseudo = detect_pseudo(after_tokens,
                             sizeof(after_tokens) / sizeof(after_tokens[0]),
                             PseudoKind::After, pseudo_pos, pseudo_len);
    }
    if (pseudo == PseudoKind::None) {
      pseudo = detect_pseudo(selection_tokens,
                             sizeof(selection_tokens) / sizeof(selection_tokens[0]),
                             PseudoKind::Selection, pseudo_pos, pseudo_len);
    }
    if (pseudo == PseudoKind::None) {
      return;
    }

    selector.erase(pseudo_pos, pseudo_len);
    selector = trim_copy(selector);

    DeclarationList remapped;
    for (const auto& declaration : props) {
      const auto& prop = declaration.property;
      const auto& value = declaration.value;
      const auto remap = [&](std::string name, std::string mapped_value) {
        remapped.push_back({std::move(name), std::move(mapped_value),
                            declaration.important, declaration.source_order});
      };
      if (pseudo == PseudoKind::Placeholder) {
        if (prop == "color") {
          const auto serialized = color_to_css_variable_value(parse_color(value));
          remap("--input-placeholder", serialized);
          remap("--textarea-placeholder", serialized);
        }
        continue;
      }

      if (pseudo == PseudoKind::Selection) {
        if (prop == "color") {
          const auto serialized = color_to_css_variable_value(parse_color(value));
          remap("--selection-color", serialized);
          remap("--input-selection-color", serialized);
          remap("--textarea-selection-color", serialized);
        } else if (prop == "background-color" || prop == "background") {
          const auto serialized = color_to_css_variable_value(parse_color(value));
          remap("--selection-bg", serialized);
          remap("--input-selection-bg", serialized);
          remap("--textarea-selection-bg", serialized);
        }
        continue;
      }

      const std::string prefix =
          pseudo == PseudoKind::Before ? "--before-" : "--after-";
      if (prop == "color" || prop == "background-color" || prop == "background") {
        remap(prefix + prop, color_to_css_variable_value(parse_color(value)));
      } else if (prop == "border-color") {
        remap(prefix + prop, color_to_css_variable_value(parse_color(value)));
      } else if (prop == "border") {
        bool hidden_style = false;
        for (const auto& token : split_css_tokens(value)) {
          if (is_border_style_token(token)) {
            hidden_style = hidden_style || is_border_style_hidden(token);
            continue;
          }
          if (is_color_value_token(token)) {
            remap(prefix + "border-color",
                  color_to_css_variable_value(parse_color(token)));
            continue;
          }
          remap(prefix + "border-width", token);
        }
        if (hidden_style) {
          remap(prefix + "border-width", "0");
        }
      } else if (prop == "content") {
        remap(prefix + prop, strip_quotes_copy(value));
      } else if (prop == "width" || prop == "height" || prop == "top" ||
                 prop == "left" || prop == "right" || prop == "bottom" ||
                 prop == "font-size" || prop == "display" ||
                 prop == "border-width" || prop == "border-radius" ||
                 prop == "opacity" || prop == "inset" ||
                 prop == "inset-x" || prop == "inset-y") {
        remap(prefix + prop, value);
      }
    }
    props = std::move(remapped);
  }

  std::string serialize_declaration(lxb_css_rule_declaration_t *decl) {
    if (!decl)
      return "";
    lexbor_str_t str = {0};
    lxb_css_rule_declaration_serialize(
        decl,
        [](const lxb_char_t *data, size_t len, void *ctx) -> lxb_status_t {
          auto *s = static_cast<lexbor_str_t *>(ctx);
          size_t new_len = (s->length ? s->length : 0) + len;
          auto *new_data = static_cast<lxb_char_t *>(realloc(s->data, new_len + 1));
          if (!new_data)
            return LXB_STATUS_ERROR_MEMORY_ALLOCATION;
          memcpy(new_data + (s->length ? s->length : 0), data, len);
          new_data[new_len] = 0;
          s->data = new_data;
          s->length = new_len;
          return LXB_STATUS_OK;
        },
        &str);
    std::string result;
    if (str.data) {
      result = std::string(reinterpret_cast<char *>(str.data), str.length);
      free(str.data);
    }
    return result;
  }

  static ContainerType parse_container_type_value(const std::string& value) {
    return to_lower_copy(trim_copy(value)) == "inline-size"
               ? ContainerType::InlineSize
               : ContainerType::None;
  }

  static std::vector<std::string> parse_container_name_value(
      const std::string& value) {
    std::vector<std::string> names;
    for (const auto& token : split_css_tokens(value)) {
      const std::string normalized = strip_quotes_copy(trim_copy(token));
      if (normalized.empty()) {
        continue;
      }
      if (to_lower_copy(normalized) == "none") {
        names.clear();
        return names;
      }
      names.push_back(normalized);
    }
    return names;
  }

  void apply_container_property(const Symbol& prop_sym,
                                const std::string& value,
                                ComputedStyle* style) {
    if (!current_element_) {
      return;
    }

    auto& registration = container_registrations_[current_element_];
    static const Symbol PROP_CONTAINER_TYPE("container-type");
    static const Symbol PROP_CONTAINER_NAME("container-name");
    if (prop_sym == PROP_CONTAINER_TYPE) {
      registration.type = parse_container_type_value(value);
      if (style) {
        style->variables[prop_sym] = to_lower_copy(trim_copy(value));
      }
    } else if (prop_sym == PROP_CONTAINER_NAME) {
      registration.names = parse_container_name_value(value);
      if (style) {
        style->variables[prop_sym] = trim_copy(value);
      }
    }
  }

  void apply_property(const std::string &prop, const std::string &raw_value, ComputedStyle *style) {
    // CSS Variables (--xxx)
    if (prop.size() > 2 && prop[0] == '-' && prop[1] == '-') {
      style->variables[Symbol(prop)] = raw_value;
      return;
    }

    // Intern common property names as Symbols for fast comparison
    static const Symbol PROP_WIDTH("width");
    static const Symbol PROP_HEIGHT("height");
    static const Symbol PROP_ASPECT_RATIO("aspect-ratio");
    static const Symbol PROP_MIN_WIDTH("min-width");
    static const Symbol PROP_MAX_WIDTH("max-width");
    static const Symbol PROP_MIN_HEIGHT("min-height");
    static const Symbol PROP_MAX_HEIGHT("max-height");
    static const Symbol PROP_PADDING("padding");
    static const Symbol PROP_PADDING_TOP("padding-top");
    static const Symbol PROP_PADDING_RIGHT("padding-right");
    static const Symbol PROP_PADDING_BOTTOM("padding-bottom");
    static const Symbol PROP_PADDING_LEFT("padding-left");
    static const Symbol PROP_PADDING_INLINE("padding-inline");
    static const Symbol PROP_PADDING_INLINE_START("padding-inline-start");
    static const Symbol PROP_PADDING_INLINE_END("padding-inline-end");
    static const Symbol PROP_PADDING_BLOCK("padding-block");
    static const Symbol PROP_PADDING_BLOCK_START("padding-block-start");
    static const Symbol PROP_PADDING_BLOCK_END("padding-block-end");
    static const Symbol PROP_MARGIN("margin");
    static const Symbol PROP_MARGIN_TOP("margin-top");
    static const Symbol PROP_MARGIN_RIGHT("margin-right");
    static const Symbol PROP_MARGIN_BOTTOM("margin-bottom");
    static const Symbol PROP_MARGIN_LEFT("margin-left");
    static const Symbol PROP_MARGIN_INLINE("margin-inline");
    static const Symbol PROP_MARGIN_INLINE_START("margin-inline-start");
    static const Symbol PROP_MARGIN_INLINE_END("margin-inline-end");
    static const Symbol PROP_MARGIN_BLOCK("margin-block");
    static const Symbol PROP_MARGIN_BLOCK_START("margin-block-start");
    static const Symbol PROP_MARGIN_BLOCK_END("margin-block-end");
    static const Symbol PROP_SCROLL_PADDING("scroll-padding");
    static const Symbol PROP_SCROLL_PADDING_TOP("scroll-padding-top");
    static const Symbol PROP_SCROLL_PADDING_RIGHT("scroll-padding-right");
    static const Symbol PROP_SCROLL_PADDING_BOTTOM("scroll-padding-bottom");
    static const Symbol PROP_SCROLL_PADDING_LEFT("scroll-padding-left");
    static const Symbol PROP_SCROLL_PADDING_INLINE("scroll-padding-inline");
    static const Symbol PROP_SCROLL_PADDING_INLINE_START("scroll-padding-inline-start");
    static const Symbol PROP_SCROLL_PADDING_INLINE_END("scroll-padding-inline-end");
    static const Symbol PROP_SCROLL_PADDING_BLOCK("scroll-padding-block");
    static const Symbol PROP_SCROLL_PADDING_BLOCK_START("scroll-padding-block-start");
    static const Symbol PROP_SCROLL_PADDING_BLOCK_END("scroll-padding-block-end");
    static const Symbol PROP_SCROLL_MARGIN("scroll-margin");
    static const Symbol PROP_SCROLL_MARGIN_TOP("scroll-margin-top");
    static const Symbol PROP_SCROLL_MARGIN_RIGHT("scroll-margin-right");
    static const Symbol PROP_SCROLL_MARGIN_BOTTOM("scroll-margin-bottom");
    static const Symbol PROP_SCROLL_MARGIN_LEFT("scroll-margin-left");
    static const Symbol PROP_SCROLL_MARGIN_INLINE("scroll-margin-inline");
    static const Symbol PROP_SCROLL_MARGIN_INLINE_START("scroll-margin-inline-start");
    static const Symbol PROP_SCROLL_MARGIN_INLINE_END("scroll-margin-inline-end");
    static const Symbol PROP_SCROLL_MARGIN_BLOCK("scroll-margin-block");
    static const Symbol PROP_SCROLL_MARGIN_BLOCK_START("scroll-margin-block-start");
    static const Symbol PROP_SCROLL_MARGIN_BLOCK_END("scroll-margin-block-end");
    static const Symbol PROP_BORDER("border");
    static const Symbol PROP_BORDER_TOP("border-top");
    static const Symbol PROP_BORDER_RIGHT("border-right");
    static const Symbol PROP_BORDER_BOTTOM("border-bottom");
    static const Symbol PROP_BORDER_LEFT("border-left");
    static const Symbol PROP_BORDER_INLINE("border-inline");
    static const Symbol PROP_BORDER_INLINE_START("border-inline-start");
    static const Symbol PROP_BORDER_INLINE_END("border-inline-end");
    static const Symbol PROP_BORDER_BLOCK("border-block");
    static const Symbol PROP_BORDER_BLOCK_START("border-block-start");
    static const Symbol PROP_BORDER_BLOCK_END("border-block-end");
    static const Symbol PROP_BORDER_STYLE("border-style");
    static const Symbol PROP_BORDER_WIDTH("border-width");
    static const Symbol PROP_BORDER_TOP_WIDTH("border-top-width");
    static const Symbol PROP_BORDER_RIGHT_WIDTH("border-right-width");
    static const Symbol PROP_BORDER_BOTTOM_WIDTH("border-bottom-width");
    static const Symbol PROP_BORDER_LEFT_WIDTH("border-left-width");
    static const Symbol PROP_BORDER_TOP_STYLE("border-top-style");
    static const Symbol PROP_BORDER_RIGHT_STYLE("border-right-style");
    static const Symbol PROP_BORDER_BOTTOM_STYLE("border-bottom-style");
    static const Symbol PROP_BORDER_LEFT_STYLE("border-left-style");
    static const Symbol PROP_BORDER_TOP_COLOR("border-top-color");
    static const Symbol PROP_BORDER_RIGHT_COLOR("border-right-color");
    static const Symbol PROP_BORDER_BOTTOM_COLOR("border-bottom-color");
    static const Symbol PROP_BORDER_LEFT_COLOR("border-left-color");
    static const Symbol PROP_BORDER_RADIUS("border-radius");
    static const Symbol PROP_BORDER_TOP_LEFT_RADIUS("border-top-left-radius");
    static const Symbol PROP_BORDER_TOP_RIGHT_RADIUS("border-top-right-radius");
    static const Symbol PROP_BORDER_BOTTOM_RIGHT_RADIUS("border-bottom-right-radius");
    static const Symbol PROP_BORDER_BOTTOM_LEFT_RADIUS("border-bottom-left-radius");
    static const Symbol PROP_BORDER_START_START_RADIUS("border-start-start-radius");
    static const Symbol PROP_BORDER_START_END_RADIUS("border-start-end-radius");
    static const Symbol PROP_BORDER_END_END_RADIUS("border-end-end-radius");
    static const Symbol PROP_BORDER_END_START_RADIUS("border-end-start-radius");
    static const Symbol PROP_BOX_SIZING("box-sizing");
    static const Symbol PROP_DISPLAY("display");
    static const Symbol PROP_POSITION("position");
    static const Symbol PROP_INSET("inset");
    static const Symbol PROP_INSET_X("inset-x");
    static const Symbol PROP_INSET_Y("inset-y");
    static const Symbol PROP_INSET_INLINE("inset-inline");
    static const Symbol PROP_INSET_INLINE_START("inset-inline-start");
    static const Symbol PROP_INSET_INLINE_END("inset-inline-end");
    static const Symbol PROP_INSET_BLOCK("inset-block");
    static const Symbol PROP_INSET_BLOCK_START("inset-block-start");
    static const Symbol PROP_INSET_BLOCK_END("inset-block-end");
    static const Symbol PROP_TOP("top");
    static const Symbol PROP_RIGHT("right");
    static const Symbol PROP_BOTTOM("bottom");
    static const Symbol PROP_LEFT("left");
    static const Symbol PROP_Z_INDEX("z-index");
    static const Symbol PROP_FLEX_DIRECTION("flex-direction");
    static const Symbol PROP_JUSTIFY_CONTENT("justify-content");
    static const Symbol PROP_ALIGN_ITEMS("align-items");
    static const Symbol PROP_ALIGN_CONTENT("align-content");
    static const Symbol PROP_ALIGN_SELF("align-self");
    static const Symbol PROP_JUSTIFY_ITEMS("justify-items");
    static const Symbol PROP_JUSTIFY_SELF("justify-self");
    static const Symbol PROP_PLACE_CONTENT("place-content");
    static const Symbol PROP_PLACE_ITEMS("place-items");
    static const Symbol PROP_PLACE_SELF("place-self");
    static const Symbol PROP_GAP("gap");
    static const Symbol PROP_FLEX_GROW("flex-grow");
    static const Symbol PROP_FLEX_SHRINK("flex-shrink");
    static const Symbol PROP_FLEX_BASIS("flex-basis");
    static const Symbol PROP_FLEX_WRAP("flex-wrap");
    static const Symbol PROP_FLEX("flex");
    static const Symbol PROP_GRID_TEMPLATE_COLUMNS("grid-template-columns");
    static const Symbol PROP_GRID_TEMPLATE_ROWS("grid-template-rows");
    static const Symbol PROP_GRID_TEMPLATE_AREAS("grid-template-areas");
    static const Symbol PROP_GRID_AUTO_FLOW("grid-auto-flow");
    static const Symbol PROP_GRID_AUTO_COLUMNS("grid-auto-columns");
    static const Symbol PROP_GRID_AUTO_ROWS("grid-auto-rows");
    static const Symbol PROP_GRID_COLUMN("grid-column");
    static const Symbol PROP_GRID_ROW("grid-row");
    static const Symbol PROP_GRID_AREA("grid-area");
    static const Symbol PROP_ROW_GAP("row-gap");
    static const Symbol PROP_COLUMN_GAP("column-gap");
    static const Symbol PROP_FONT_SIZE("font-size");
    static const Symbol PROP_FONT_FAMILY("font-family");
    static const Symbol PROP_FONT_WEIGHT("font-weight");
    static const Symbol PROP_FONT_STYLE("font-style");
    static const Symbol PROP_DIRECTION("direction");
    static const Symbol PROP_UNICODE_BIDI("unicode-bidi");
    static const Symbol PROP_LINE_HEIGHT("line-height");
    static const Symbol PROP_TEXT_ALIGN("text-align");
    static const Symbol PROP_TEXT_ALIGN_LAST("text-align-last");
    static const Symbol PROP_TEXT_DECORATION("text-decoration");
    static const Symbol PROP_TEXT_DECORATION_LINE("text-decoration-line");
    static const Symbol PROP_TEXT_DECORATION_COLOR("text-decoration-color");
    static const Symbol PROP_TEXT_DECORATION_STYLE("text-decoration-style");
    static const Symbol PROP_TEXT_DECORATION_THICKNESS("text-decoration-thickness");
    static const Symbol PROP_TEXT_UNDERLINE_OFFSET("text-underline-offset");
    static const Symbol PROP_TEXT_TRANSFORM("text-transform");
    static const Symbol PROP_LETTER_SPACING("letter-spacing");
    static const Symbol PROP_WORD_SPACING("word-spacing");
    static const Symbol PROP_TEXT_INDENT("text-indent");
    static const Symbol PROP_TAB_SIZE("tab-size");
    static const Symbol PROP_TEXT_SHADOW("text-shadow");
    static const Symbol PROP_FONT_VARIANT_NUMERIC("font-variant-numeric");
    static const Symbol PROP_VERTICAL_ALIGN("vertical-align");
    static const Symbol PROP_WHITE_SPACE("white-space");
    static const Symbol PROP_TEXT_WRAP("text-wrap");
    static const Symbol PROP_TEXT_WRAP_MODE("text-wrap-mode");
    static const Symbol PROP_TEXT_OVERFLOW("text-overflow");
    static const Symbol PROP_OVERFLOW_WRAP("overflow-wrap");
    static const Symbol PROP_WORD_BREAK("word-break");
    static const Symbol PROP_MAX_LINES("max-lines");
    static const Symbol PROP_LINE_CLAMP("line-clamp");
    static const Symbol PROP_WEBKIT_LINE_CLAMP("-webkit-line-clamp");
    static const Symbol PROP_OBJECT_FIT("object-fit");
    static const Symbol PROP_OBJECT_POSITION("object-position");
    static const Symbol PROP_CARET_COLOR("caret-color");
    static const Symbol PROP_ACCENT_COLOR("accent-color");
    static const Symbol PROP_COLOR("color");
    static const Symbol PROP_BACKGROUND_COLOR("background-color");
    static const Symbol PROP_BACKGROUND_IMAGE("background-image");
    static const Symbol PROP_BACKGROUND_POSITION("background-position");
    static const Symbol PROP_BACKGROUND_POSITION_X("background-position-x");
    static const Symbol PROP_BACKGROUND_POSITION_Y("background-position-y");
    static const Symbol PROP_BACKGROUND_SIZE("background-size");
    static const Symbol PROP_BACKGROUND_REPEAT("background-repeat");
    static const Symbol PROP_BACKGROUND_CLIP("background-clip");
    static const Symbol PROP_BACKGROUND_ORIGIN("background-origin");
    static const Symbol PROP_BACKGROUND("background");
    static const Symbol PROP_CLIP_PATH("clip-path");
    static const Symbol PROP_FILTER("filter");
    static const Symbol PROP_BACKDROP_FILTER("backdrop-filter");
    static const Symbol PROP_WEBKIT_BACKDROP_FILTER("-webkit-backdrop-filter");
    static const Symbol PROP_CURSOR("cursor");
    static const Symbol PROP_APPEARANCE("appearance");
    static const Symbol PROP_WEBKIT_APPEARANCE("-webkit-appearance");
    static const Symbol PROP_USER_SELECT("user-select");
    static const Symbol PROP_WEBKIT_USER_SELECT("-webkit-user-select");
    static const Symbol PROP_TOUCH_ACTION("touch-action");
    static const Symbol PROP_COLOR_SCHEME("color-scheme");
    static const Symbol PROP_WEBKIT_COLOR_SCHEME("-webkit-color-scheme");
    static const Symbol PROP_BORDER_COLOR("border-color");
    static const Symbol PROP_OPACITY("opacity");
    static const Symbol PROP_VISIBILITY("visibility");
    static const Symbol PROP_POINTER_EVENTS("pointer-events");
    static const Symbol PROP_OVERFLOW("overflow");
    static const Symbol PROP_OVERFLOW_X("overflow-x");
    static const Symbol PROP_OVERFLOW_Y("overflow-y");
    static const Symbol PROP_SCROLLBAR_WIDTH("scrollbar-width");
    static const Symbol PROP_SCROLLBAR_COLOR("scrollbar-color");
    static const Symbol PROP_TRANSLATE("translate");
    static const Symbol PROP_SCALE("scale");
    static const Symbol PROP_ROTATE("rotate");
    static const Symbol PROP_TRANSFORM("transform");
    static const Symbol PROP_TRANSFORM_ORIGIN("transform-origin");
    static const Symbol PROP_BOX_SHADOW("box-shadow");
    static const Symbol PROP_OUTLINE("outline");
    static const Symbol PROP_OUTLINE_STYLE("outline-style");
    static const Symbol PROP_OUTLINE_WIDTH("outline-width");
    static const Symbol PROP_OUTLINE_COLOR("outline-color");
    static const Symbol PROP_OUTLINE_OFFSET("outline-offset");
    static const Symbol PROP_RING("ring");
    static const Symbol PROP_RING_WIDTH("ring-width");
    static const Symbol PROP_RING_COLOR("ring-color");
    static const Symbol PROP_RING_OFFSET("ring-offset");
    static const Symbol PROP_RING_OFFSET_COLOR("ring-offset-color");
    static const Symbol PROP_TRANSITION("transition");
    static const Symbol PROP_TRANSITION_PROPERTY("transition-property");
    static const Symbol PROP_TRANSITION_DURATION("transition-duration");
    static const Symbol PROP_TRANSITION_DELAY("transition-delay");
    static const Symbol PROP_TRANSITION_TIMING_FUNCTION("transition-timing-function");
    static const Symbol PROP_ANIMATION("animation");
    static const Symbol PROP_ANIMATION_NAME("animation-name");
    static const Symbol PROP_ANIMATION_DURATION("animation-duration");
    static const Symbol PROP_ANIMATION_DELAY("animation-delay");
    static const Symbol PROP_ANIMATION_TIMING_FUNCTION("animation-timing-function");
    static const Symbol PROP_ANIMATION_ITERATION_COUNT("animation-iteration-count");
    static const Symbol PROP_ANIMATION_FILL_MODE("animation-fill-mode");
    static const Symbol PROP_ANIMATION_DIRECTION("animation-direction");
    static const Symbol PROP_ANIMATION_PLAY_STATE("animation-play-state");
    static const Symbol PROP_CONTAINER_TYPE("container-type");
    static const Symbol PROP_CONTAINER_NAME("container-name");

    Symbol prop_sym(prop);

    // Resolve var() references
    std::string value = style->resolve_variable_value(raw_value);

    if (prop.rfind("--", 0) == 0) {
      style->variables[prop_sym] = value;
      return;
    }

    if (prop_sym == PROP_CONTAINER_TYPE || prop_sym == PROP_CONTAINER_NAME) {
      apply_container_property(prop_sym, value, style);
      return;
    }

    const auto apply_shared_text_property = [&]() -> bool {
      if (prop_sym == PROP_DIRECTION) {
        style->variables[Symbol("--direction")] = value;
        style->direction = value == "rtl" ? Direction::Rtl : Direction::Ltr;
        return true;
      }
      if (prop_sym == PROP_UNICODE_BIDI) {
        const std::string lowered = to_lower_copy(trim_copy(value));
        style->variables[Symbol("--unicode-bidi")] = lowered;
        if (lowered == "plaintext") {
          style->unicode_bidi = UnicodeBidi::Plaintext;
        } else if (lowered == "isolate") {
          style->unicode_bidi = UnicodeBidi::Isolate;
        } else {
          style->unicode_bidi = UnicodeBidi::Normal;
        }
        return true;
      }
      if (prop_sym == PROP_LINE_HEIGHT) {
        style->variables[Symbol("--line-height")] = value;
        return true;
      }
      if (prop_sym == PROP_TEXT_ALIGN) {
        style->variables[Symbol("--text-align")] = value;
        if (value == "left")
          style->text_align = TextAlign::Left;
        else if (value == "center")
          style->text_align = TextAlign::Center;
        else if (value == "right")
          style->text_align = TextAlign::Right;
        else if (value == "justify")
          style->text_align = TextAlign::Justify;
        else if (value == "start")
          style->text_align = TextAlign::Start;
        else if (value == "end")
          style->text_align = TextAlign::End;
        return true;
      }
      if (prop_sym == PROP_TEXT_ALIGN_LAST) {
        style->variables[Symbol("--text-align-last")] =
            to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_TEXT_DECORATION) {
        parse_text_decoration(value, style);
        return true;
      }
      if (prop_sym == PROP_TEXT_DECORATION_LINE) {
        assign_text_decoration_line(value, style);
        return true;
      }
      if (prop_sym == PROP_TEXT_DECORATION_COLOR) {
        style->variables[Symbol("--text-decoration-color")] =
            color_to_css_variable_value(parse_style_color(value, style));
        return true;
      }
      if (prop_sym == PROP_TEXT_DECORATION_STYLE) {
        style->variables[Symbol("--text-decoration-style")] =
            to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_TEXT_DECORATION_THICKNESS) {
        style->variables[Symbol("--text-decoration-thickness")] =
            to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_TEXT_UNDERLINE_OFFSET) {
        style->variables[Symbol("--text-underline-offset")] =
            to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_TEXT_TRANSFORM) {
        const std::string lowered = to_lower_copy(trim_copy(value));
        style->variables[Symbol("--text-transform")] = lowered;
        if (lowered == "uppercase") {
          style->text_transform = TextTransform::Uppercase;
        } else if (lowered == "lowercase") {
          style->text_transform = TextTransform::Lowercase;
        } else if (lowered == "capitalize") {
          style->text_transform = TextTransform::Capitalize;
        } else {
          style->text_transform = TextTransform::None;
        }
        return true;
      }
      if (prop_sym == PROP_LETTER_SPACING) {
        const std::string lowered = to_lower_copy(trim_copy(value));
        style->variables[Symbol("--letter-spacing")] = lowered;
        style->letter_spacing =
            lowered.empty() || lowered == "normal" ? 0.0f : parse_length(lowered);
        return true;
      }
      if (prop_sym == PROP_WORD_SPACING) {
        const std::string lowered = to_lower_copy(trim_copy(value));
        style->variables[Symbol("--word-spacing")] = lowered;
        style->word_spacing =
            lowered.empty() || lowered == "normal" ? 0.0f : parse_length(lowered);
        return true;
      }
      if (prop_sym == PROP_TEXT_INDENT) {
        const std::string lowered = to_lower_copy(trim_copy(value));
        style->variables[Symbol("--text-indent")] = lowered;
        style->text_indent = lowered.empty() ? 0.0f : parse_length(lowered);
        return true;
      }
      if (prop_sym == PROP_TAB_SIZE) {
        const std::string lowered = to_lower_copy(trim_copy(value));
        style->variables[Symbol("--tab-size")] = lowered;
        style->tab_size = lowered.empty() ? 8.0f : std::max(parse_length(lowered), 0.0f);
        return true;
      }
      if (prop_sym == PROP_FONT_VARIANT_NUMERIC) {
        style->variables[Symbol("--font-variant-numeric")] =
            to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_VERTICAL_ALIGN) {
        style->variables[Symbol("--vertical-align")] = value;
        return true;
      }
      if (prop_sym == PROP_WHITE_SPACE) {
        style->variables[Symbol("--white-space")] = value;
        return true;
      }
      if (prop_sym == PROP_TEXT_WRAP) {
        style->variables[Symbol("--text-wrap")] = to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_TEXT_WRAP_MODE) {
        style->variables[Symbol("--text-wrap-mode")] =
            to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_TEXT_OVERFLOW) {
        style->variables[Symbol("--text-overflow")] = value;
        return true;
      }
      if (prop_sym == PROP_OVERFLOW_WRAP) {
        style->variables[Symbol("--overflow-wrap")] = value;
        return true;
      }
      if (prop_sym == PROP_WORD_BREAK) {
        style->variables[Symbol("--word-break")] = value;
        return true;
      }
      if (prop_sym == PROP_MAX_LINES) {
        style->variables[Symbol("--max-lines")] = value;
        return true;
      }
      if (prop_sym == PROP_LINE_CLAMP || prop_sym == PROP_WEBKIT_LINE_CLAMP) {
        style->variables[Symbol("--line-clamp")] = value;
        return true;
      }
      return false;
    };

    const auto apply_bridge_visual_property = [&]() -> bool {
      if (prop_sym == PROP_OBJECT_FIT) {
        style->variables[Symbol("--object-fit")] = value;
        return true;
      }
      if (prop_sym == PROP_OBJECT_POSITION) {
        style->variables[Symbol("--object-position")] = value;
        return true;
      }
      if (prop_sym == PROP_BACKGROUND_POSITION) {
        assign_background_layer_metadata(value, style, "position");
        style->variables.erase(background_position_x_var());
        style->variables.erase(background_position_y_var());
        return true;
      }
      if (prop_sym == PROP_BACKGROUND_POSITION_X) {
        assign_background_position_axis_metadata(value, style, true);
        return true;
      }
      if (prop_sym == PROP_BACKGROUND_POSITION_Y) {
        assign_background_position_axis_metadata(value, style, false);
        return true;
      }
      if (prop_sym == PROP_BACKGROUND_SIZE) {
        assign_background_layer_metadata(value, style, "size");
        return true;
      }
      if (prop_sym == PROP_BACKGROUND_REPEAT) {
        assign_background_layer_metadata(value, style, "repeat");
        return true;
      }
      if (prop_sym == PROP_BACKGROUND_ORIGIN) {
        const std::string lowered = to_lower_copy(trim_copy(value));
        const auto items = split_top_level_csv(lowered);
        bool valid = !items.empty();
        for (const auto& item : items) {
          if (!is_background_box_token(transition_trim_copy(item))) {
            valid = false;
            break;
          }
        }
        if (valid) {
          assign_background_layer_metadata(value, style, "origin");
        }
        return true;
      }
      if (prop_sym == PROP_BACKGROUND_CLIP) {
        const std::string lowered = to_lower_copy(trim_copy(value));
        const auto items = split_top_level_csv(lowered);
        bool valid = !items.empty();
        for (const auto& item : items) {
          if (!is_background_box_token(transition_trim_copy(item))) {
            valid = false;
            break;
          }
        }
        if (valid) {
          assign_background_layer_metadata(value, style, "clip");
        }
        return true;
      }
      if (prop_sym == PROP_CARET_COLOR) {
        const auto serialized =
            color_to_css_variable_value(parse_style_color(value, style, style->text_color));
        style->variables[Symbol("--caret-color")] = serialized;
        style->variables[Symbol("--input-cursor")] = serialized;
        style->variables[Symbol("--textarea-cursor")] = serialized;
        return true;
      }
      if (prop_sym == PROP_ACCENT_COLOR) {
        const auto serialized =
            color_to_css_variable_value(parse_style_color(value, style, style->text_color));
        style->variables[Symbol("--accent-color")] = serialized;
        style->variables[Symbol("--checkbox-bg-checked")] = serialized;
        style->variables[Symbol("--radio-bg-checked")] = serialized;
        style->variables[Symbol("--switch-bg-on")] = serialized;
        style->variables[Symbol("--progress-fill")] = serialized;
        style->variables[Symbol("--track-fill")] = serialized;
        return true;
      }
      if (prop_sym == PROP_CLIP_PATH) {
        style->variables[Symbol("--clip-path")] =
            to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_FILTER) {
        parse_filter_effect(value, style, Symbol("--filter-blur"), true);
        return true;
      }
      if (prop_sym == PROP_BACKDROP_FILTER ||
          prop_sym == PROP_WEBKIT_BACKDROP_FILTER) {
        parse_filter_effect(value, style, Symbol("--backdrop-blur"), false);
        return true;
      }
      if (prop_sym == PROP_CURSOR) {
        style->variables[Symbol("--cursor")] = to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_APPEARANCE ||
          prop_sym == PROP_WEBKIT_APPEARANCE) {
        style->variables[Symbol("--appearance")] = to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_USER_SELECT ||
          prop_sym == PROP_WEBKIT_USER_SELECT) {
        style->variables[Symbol("--user-select")] = to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_TOUCH_ACTION) {
        style->variables[Symbol("--touch-action")] = to_lower_copy(trim_copy(value));
        return true;
      }
      if (prop_sym == PROP_COLOR_SCHEME ||
          prop_sym == PROP_WEBKIT_COLOR_SCHEME) {
        style->variables[Symbol("--color-scheme")] = to_lower_copy(trim_copy(value));
        return true;
      }
      return false;
    };
    const auto apply_logical_pair = [&](const std::string& raw,
                                        float& start_value,
                                        float& end_value,
                                        auto&& parser) {
      const auto values = split_css_tokens(raw);
      if (values.empty()) {
        return;
      }
      start_value = parser(values[0]);
      end_value = parser(values.size() > 1 ? values[1] : values[0]);
    };
    const auto inline_start_side = [&]() -> int {
      return style->direction == Direction::Rtl ? 1 : 3;
    };
    const auto inline_end_side = [&]() -> int {
      return style->direction == Direction::Rtl ? 3 : 1;
    };
    const auto start_start_radius_index = [&]() -> int {
      return style->direction == Direction::Rtl ? 1 : 0;
    };
    const auto start_end_radius_index = [&]() -> int {
      return style->direction == Direction::Rtl ? 0 : 1;
    };
    const auto end_end_radius_index = [&]() -> int {
      return style->direction == Direction::Rtl ? 3 : 2;
    };
    const auto end_start_radius_index = [&]() -> int {
      return style->direction == Direction::Rtl ? 2 : 3;
    };
    const auto assign_scroll_edges = [&](const std::string& raw,
                                         Symbol top_var, Symbol right_var,
                                         Symbol bottom_var, Symbol left_var) {
      const auto values = split_css_tokens(raw);
      if (values.empty()) {
        return;
      }
      if (values.size() == 1) {
        style->variables[top_var] = values[0];
        style->variables[right_var] = values[0];
        style->variables[bottom_var] = values[0];
        style->variables[left_var] = values[0];
      } else if (values.size() == 2) {
        style->variables[top_var] = values[0];
        style->variables[bottom_var] = values[0];
        style->variables[right_var] = values[1];
        style->variables[left_var] = values[1];
      } else if (values.size() == 3) {
        style->variables[top_var] = values[0];
        style->variables[right_var] = values[1];
        style->variables[left_var] = values[1];
        style->variables[bottom_var] = values[2];
      } else {
        style->variables[top_var] = values[0];
        style->variables[right_var] = values[1];
        style->variables[bottom_var] = values[2];
        style->variables[left_var] = values[3];
      }
    };
    const auto assign_scroll_logical_pair = [&](const std::string& raw,
                                                Symbol start_var, Symbol end_var) {
      const auto values = split_css_tokens(raw);
      if (values.empty()) {
        return;
      }
      style->variables[start_var] = values[0];
      style->variables[end_var] = values.size() > 1 ? values[1] : values[0];
    };
    const auto apply_scroll_reveal_property = [&]() -> bool {
      if (prop_sym == PROP_SCROLL_PADDING) {
        assign_scroll_edges(value, Symbol("--scroll-padding-top"),
                            Symbol("--scroll-padding-right"),
                            Symbol("--scroll-padding-bottom"),
                            Symbol("--scroll-padding-left"));
        return true;
      }
      if (prop_sym == PROP_SCROLL_PADDING_TOP) {
        style->variables[Symbol("--scroll-padding-top")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_PADDING_RIGHT) {
        style->variables[Symbol("--scroll-padding-right")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_PADDING_BOTTOM) {
        style->variables[Symbol("--scroll-padding-bottom")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_PADDING_LEFT) {
        style->variables[Symbol("--scroll-padding-left")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_PADDING_INLINE) {
        assign_scroll_logical_pair(
            value,
            style->direction == Direction::Rtl ? Symbol("--scroll-padding-right")
                                               : Symbol("--scroll-padding-left"),
            style->direction == Direction::Rtl ? Symbol("--scroll-padding-left")
                                               : Symbol("--scroll-padding-right"));
        return true;
      }
      if (prop_sym == PROP_SCROLL_PADDING_INLINE_START) {
        style->variables[style->direction == Direction::Rtl
                             ? Symbol("--scroll-padding-right")
                             : Symbol("--scroll-padding-left")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_PADDING_INLINE_END) {
        style->variables[style->direction == Direction::Rtl
                             ? Symbol("--scroll-padding-left")
                             : Symbol("--scroll-padding-right")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_PADDING_BLOCK) {
        assign_scroll_logical_pair(value, Symbol("--scroll-padding-top"),
                                   Symbol("--scroll-padding-bottom"));
        return true;
      }
      if (prop_sym == PROP_SCROLL_PADDING_BLOCK_START) {
        style->variables[Symbol("--scroll-padding-top")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_PADDING_BLOCK_END) {
        style->variables[Symbol("--scroll-padding-bottom")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_MARGIN) {
        assign_scroll_edges(value, Symbol("--scroll-margin-top"),
                            Symbol("--scroll-margin-right"),
                            Symbol("--scroll-margin-bottom"),
                            Symbol("--scroll-margin-left"));
        return true;
      }
      if (prop_sym == PROP_SCROLL_MARGIN_TOP) {
        style->variables[Symbol("--scroll-margin-top")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_MARGIN_RIGHT) {
        style->variables[Symbol("--scroll-margin-right")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_MARGIN_BOTTOM) {
        style->variables[Symbol("--scroll-margin-bottom")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_MARGIN_LEFT) {
        style->variables[Symbol("--scroll-margin-left")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_MARGIN_INLINE) {
        assign_scroll_logical_pair(
            value,
            style->direction == Direction::Rtl ? Symbol("--scroll-margin-right")
                                               : Symbol("--scroll-margin-left"),
            style->direction == Direction::Rtl ? Symbol("--scroll-margin-left")
                                               : Symbol("--scroll-margin-right"));
        return true;
      }
      if (prop_sym == PROP_SCROLL_MARGIN_INLINE_START) {
        style->variables[style->direction == Direction::Rtl
                             ? Symbol("--scroll-margin-right")
                             : Symbol("--scroll-margin-left")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_MARGIN_INLINE_END) {
        style->variables[style->direction == Direction::Rtl
                             ? Symbol("--scroll-margin-left")
                             : Symbol("--scroll-margin-right")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_MARGIN_BLOCK) {
        assign_scroll_logical_pair(value, Symbol("--scroll-margin-top"),
                                   Symbol("--scroll-margin-bottom"));
        return true;
      }
      if (prop_sym == PROP_SCROLL_MARGIN_BLOCK_START) {
        style->variables[Symbol("--scroll-margin-top")] = value;
        return true;
      }
      if (prop_sym == PROP_SCROLL_MARGIN_BLOCK_END) {
        style->variables[Symbol("--scroll-margin-bottom")] = value;
        return true;
      }
      return false;
    };

    if (apply_shared_text_property() || apply_bridge_visual_property() ||
        apply_scroll_reveal_property()) {
      return;
    }

    if (apply_logical_size_property(prop, value, style) ||
        apply_logical_border_property(prop, value, style)) {
      return;
    }

    // Box Model
    if (prop_sym == PROP_WIDTH) {
      assign_size(value, style->width_size, style->width,
                  style->width_is_percent);
    } else if (prop_sym == PROP_HEIGHT) {
      assign_size(value, style->height_size, style->height,
                  style->height_is_percent);
    } else if (prop_sym == PROP_ASPECT_RATIO) {
      style->variables[PROP_ASPECT_RATIO] = value;
    } else if (prop_sym == PROP_MIN_WIDTH)
      style->variables[PROP_MIN_WIDTH] = value;
    else if (prop_sym == PROP_MAX_WIDTH)
      style->variables[PROP_MAX_WIDTH] = value;
    else if (prop_sym == PROP_MIN_HEIGHT)
      style->variables[PROP_MIN_HEIGHT] = value;
    else if (prop_sym == PROP_MAX_HEIGHT)
      style->variables[PROP_MAX_HEIGHT] = value;
    else if (prop_sym == PROP_PADDING)
      parse_box_values(value, style->padding);
    else if (prop_sym == PROP_PADDING_TOP)
      style->padding[0] = parse_length(value);
    else if (prop_sym == PROP_PADDING_RIGHT)
      style->padding[1] = parse_length(value);
    else if (prop_sym == PROP_PADDING_BOTTOM)
      style->padding[2] = parse_length(value);
    else if (prop_sym == PROP_PADDING_LEFT)
      style->padding[3] = parse_length(value);
    else if (prop_sym == PROP_PADDING_INLINE)
      apply_logical_pair(value, style->padding[inline_start_side()],
                         style->padding[inline_end_side()],
                         [this](const std::string& token) { return parse_length(token); });
    else if (prop_sym == PROP_PADDING_INLINE_START)
      style->padding[inline_start_side()] = parse_length(value);
    else if (prop_sym == PROP_PADDING_INLINE_END)
      style->padding[inline_end_side()] = parse_length(value);
    else if (prop_sym == PROP_PADDING_BLOCK)
      apply_logical_pair(value, style->padding[0], style->padding[2],
                         [this](const std::string& token) { return parse_length(token); });
    else if (prop_sym == PROP_PADDING_BLOCK_START)
      style->padding[0] = parse_length(value);
    else if (prop_sym == PROP_PADDING_BLOCK_END)
      style->padding[2] = parse_length(value);
    else if (prop_sym == PROP_MARGIN)
      parse_box_values(value, style->margin);
    else if (prop_sym == PROP_MARGIN_TOP)
      style->margin[0] = parse_length(value);
    else if (prop_sym == PROP_MARGIN_RIGHT)
      style->margin[1] = parse_length(value);
    else if (prop_sym == PROP_MARGIN_BOTTOM)
      style->margin[2] = parse_length(value);
    else if (prop_sym == PROP_MARGIN_LEFT)
      style->margin[3] = parse_length(value);
    else if (prop_sym == PROP_MARGIN_INLINE)
      apply_logical_pair(value, style->margin[inline_start_side()],
                         style->margin[inline_end_side()],
                         [this](const std::string& token) { return parse_length(token); });
    else if (prop_sym == PROP_MARGIN_INLINE_START)
      style->margin[inline_start_side()] = parse_length(value);
    else if (prop_sym == PROP_MARGIN_INLINE_END)
      style->margin[inline_end_side()] = parse_length(value);
    else if (prop_sym == PROP_MARGIN_BLOCK)
      apply_logical_pair(value, style->margin[0], style->margin[2],
                         [this](const std::string& token) { return parse_length(token); });
    else if (prop_sym == PROP_MARGIN_BLOCK_START)
      style->margin[0] = parse_length(value);
    else if (prop_sym == PROP_MARGIN_BLOCK_END)
      style->margin[2] = parse_length(value);
    else if (prop_sym == PROP_BORDER)
      parse_border_shorthand(value, style);
    else if (prop_sym == PROP_BORDER_TOP)
      parse_border_shorthand(value, style, 0);
    else if (prop_sym == PROP_BORDER_RIGHT)
      parse_border_shorthand(value, style, 1);
    else if (prop_sym == PROP_BORDER_BOTTOM)
      parse_border_shorthand(value, style, 2);
    else if (prop_sym == PROP_BORDER_LEFT)
      parse_border_shorthand(value, style, 3);
    else if (prop_sym == PROP_BORDER_INLINE)
      apply_border_to_sides({inline_start_side(), inline_end_side()}, [&](int side) {
        parse_border_shorthand(value, style, side);
      });
    else if (prop_sym == PROP_BORDER_INLINE_START)
      parse_border_shorthand(value, style, inline_start_side());
    else if (prop_sym == PROP_BORDER_INLINE_END)
      parse_border_shorthand(value, style, inline_end_side());
    else if (prop_sym == PROP_BORDER_BLOCK)
      apply_border_to_sides({0, 2}, [&](int side) {
        parse_border_shorthand(value, style, side);
      });
    else if (prop_sym == PROP_BORDER_BLOCK_START)
      parse_border_shorthand(value, style, 0);
    else if (prop_sym == PROP_BORDER_BLOCK_END)
      parse_border_shorthand(value, style, 2);
    else if (prop_sym == PROP_BORDER_STYLE)
      parse_border_style(value, style);
    else if (prop_sym == PROP_BORDER_WIDTH)
      parse_border_width_values(value, style->border_width);
    else if (prop_sym == PROP_BORDER_TOP_WIDTH)
      set_border_width_for_side(style->border_width, 0,
                                parse_border_width_token(to_lower_copy(trim_copy(value))));
    else if (prop_sym == PROP_BORDER_RIGHT_WIDTH)
      set_border_width_for_side(style->border_width, 1,
                                parse_border_width_token(to_lower_copy(trim_copy(value))));
    else if (prop_sym == PROP_BORDER_BOTTOM_WIDTH)
      set_border_width_for_side(style->border_width, 2,
                                parse_border_width_token(to_lower_copy(trim_copy(value))));
    else if (prop_sym == PROP_BORDER_LEFT_WIDTH)
      set_border_width_for_side(style->border_width, 3,
                                parse_border_width_token(to_lower_copy(trim_copy(value))));
    else if (prop_sym == PROP_BORDER_TOP_STYLE)
      set_border_style_for_side(style->border_style, 0,
                                parse_border_style_token(to_lower_copy(trim_copy(value))));
    else if (prop_sym == PROP_BORDER_RIGHT_STYLE)
      set_border_style_for_side(style->border_style, 1,
                                parse_border_style_token(to_lower_copy(trim_copy(value))));
    else if (prop_sym == PROP_BORDER_BOTTOM_STYLE)
      set_border_style_for_side(style->border_style, 2,
                                parse_border_style_token(to_lower_copy(trim_copy(value))));
    else if (prop_sym == PROP_BORDER_LEFT_STYLE)
      set_border_style_for_side(style->border_style, 3,
                                parse_border_style_token(to_lower_copy(trim_copy(value))));
    else if (prop_sym == PROP_BORDER_TOP_COLOR ||
             prop_sym == PROP_BORDER_RIGHT_COLOR ||
             prop_sym == PROP_BORDER_BOTTOM_COLOR ||
             prop_sym == PROP_BORDER_LEFT_COLOR) {
      int side = 0;
      if (prop_sym == PROP_BORDER_RIGHT_COLOR) {
        side = 1;
      } else if (prop_sym == PROP_BORDER_BOTTOM_COLOR) {
        side = 2;
      } else if (prop_sym == PROP_BORDER_LEFT_COLOR) {
        side = 3;
      }
      set_border_color_for_side(
          style, side, parse_style_color(value, style, style->border_color));
    }
    else if (prop_sym == PROP_BORDER_RADIUS)
      parse_box_values(value, style->border_radius);
    else if (prop_sym == PROP_BORDER_TOP_LEFT_RADIUS)
      style->border_radius[0] = parse_length(value);
    else if (prop_sym == PROP_BORDER_TOP_RIGHT_RADIUS)
      style->border_radius[1] = parse_length(value);
    else if (prop_sym == PROP_BORDER_BOTTOM_RIGHT_RADIUS)
      style->border_radius[2] = parse_length(value);
    else if (prop_sym == PROP_BORDER_BOTTOM_LEFT_RADIUS)
      style->border_radius[3] = parse_length(value);
    else if (prop_sym == PROP_BORDER_START_START_RADIUS)
      style->border_radius[start_start_radius_index()] = parse_length(value);
    else if (prop_sym == PROP_BORDER_START_END_RADIUS)
      style->border_radius[start_end_radius_index()] = parse_length(value);
    else if (prop_sym == PROP_BORDER_END_END_RADIUS)
      style->border_radius[end_end_radius_index()] = parse_length(value);
    else if (prop_sym == PROP_BORDER_END_START_RADIUS)
      style->border_radius[end_start_radius_index()] = parse_length(value);
    else if (prop_sym == PROP_BOX_SIZING) {
      if (value == "border-box")
        style->box_sizing = BoxSizing::BorderBox;
      else
        style->box_sizing = BoxSizing::ContentBox;
    }

    // Display & Position
    else if (prop_sym == PROP_DISPLAY) {
      if (value == "none")
        style->display = Display::None;
      else if (value == "block")
        style->display = Display::Block;
      else if (value == "inline-block")
        style->display = Display::Block;
      else if (value == "inline")
        style->display = Display::Inline;
      else if (value == "inline-flex")
        style->display = Display::Flex;
      else if (value == "flex")
        style->display = Display::Flex;
      else if (value == "inline-grid")
        style->display = Display::Grid;
      else if (value == "grid")
        style->display = Display::Grid;
    } else if (prop_sym == PROP_POSITION) {
      if (value == "static")
        style->position = Position::Static;
      else if (value == "relative")
        style->position = Position::Relative;
      else if (value == "absolute")
        style->position = Position::Absolute;
      else if (value == "fixed")
        style->position = Position::Fixed;
      else if (value == "sticky")
        style->position = Position::Sticky;
    } else if (prop_sym == PROP_INSET) {
      const auto values = split_css_tokens(value);
      if (values.size() == 1) {
        const float inset = parse_position_offset(values[0]);
        style->top = inset;
        style->right = inset;
        style->bottom = inset;
        style->left = inset;
      } else if (values.size() == 2) {
        style->top = parse_position_offset(values[0]);
        style->bottom = style->top;
        style->right = parse_position_offset(values[1]);
        style->left = style->right;
      } else if (values.size() == 3) {
        style->top = parse_position_offset(values[0]);
        style->right = parse_position_offset(values[1]);
        style->left = style->right;
        style->bottom = parse_position_offset(values[2]);
      } else if (values.size() >= 4) {
        style->top = parse_position_offset(values[0]);
        style->right = parse_position_offset(values[1]);
        style->bottom = parse_position_offset(values[2]);
        style->left = parse_position_offset(values[3]);
      }
    } else if (prop_sym == PROP_INSET_X) {
      const auto values = split_css_tokens(value);
      if (values.size() == 1) {
        const float inset = parse_position_offset(values[0]);
        style->left = inset;
        style->right = inset;
      } else if (values.size() >= 2) {
        style->left = parse_position_offset(values[0]);
        style->right = parse_position_offset(values[1]);
      }
    } else if (prop_sym == PROP_INSET_Y) {
      const auto values = split_css_tokens(value);
      if (values.size() == 1) {
        const float inset = parse_position_offset(values[0]);
        style->top = inset;
        style->bottom = inset;
      } else if (values.size() >= 2) {
        style->top = parse_position_offset(values[0]);
        style->bottom = parse_position_offset(values[1]);
      }
    } else if (prop_sym == PROP_INSET_INLINE) {
      apply_logical_pair(value,
                         style->direction == Direction::Rtl ? style->right : style->left,
                         style->direction == Direction::Rtl ? style->left : style->right,
                         [this](const std::string& token) {
                           return parse_position_offset(token);
                         });
    } else if (prop_sym == PROP_INSET_INLINE_START) {
      if (style->direction == Direction::Rtl)
        style->right = parse_position_offset(value);
      else
        style->left = parse_position_offset(value);
    } else if (prop_sym == PROP_INSET_INLINE_END) {
      if (style->direction == Direction::Rtl)
        style->left = parse_position_offset(value);
      else
        style->right = parse_position_offset(value);
    } else if (prop_sym == PROP_INSET_BLOCK) {
      apply_logical_pair(value, style->top, style->bottom,
                         [this](const std::string& token) {
                           return parse_position_offset(token);
                         });
    } else if (prop_sym == PROP_INSET_BLOCK_START) {
      style->top = parse_position_offset(value);
    } else if (prop_sym == PROP_INSET_BLOCK_END) {
      style->bottom = parse_position_offset(value);
    } else if (prop_sym == PROP_TOP)
      style->top = parse_position_offset(value);
    else if (prop_sym == PROP_RIGHT)
      style->right = parse_position_offset(value);
    else if (prop_sym == PROP_BOTTOM)
      style->bottom = parse_position_offset(value);
    else if (prop_sym == PROP_LEFT)
      style->left = parse_position_offset(value);
    else if (prop_sym == PROP_Z_INDEX)
      style->z_index = std::stoi(value);

    // Flexbox
    else if (prop_sym == PROP_FLEX_DIRECTION) {
      if (value == "row")
        style->flex_direction = FlexDirection::Row;
      else if (value == "row-reverse")
        style->flex_direction = FlexDirection::RowReverse;
      else if (value == "column")
        style->flex_direction = FlexDirection::Column;
      else if (value == "column-reverse")
        style->flex_direction = FlexDirection::ColumnReverse;
    } else if (prop_sym == PROP_JUSTIFY_CONTENT) {
      if (value == "flex-start" || value == "start")
        style->justify_content = JustifyContent::Start;
      else if (value == "flex-end" || value == "end")
        style->justify_content = JustifyContent::End;
      else if (value == "center")
        style->justify_content = JustifyContent::Center;
      else if (value == "space-between")
        style->justify_content = JustifyContent::SpaceBetween;
      else if (value == "space-around")
        style->justify_content = JustifyContent::SpaceAround;
      else if (value == "space-evenly")
        style->justify_content = JustifyContent::SpaceEvenly;
      style->variables[PROP_JUSTIFY_CONTENT] = value;
    } else if (prop_sym == PROP_ALIGN_ITEMS) {
      if (value == "flex-start" || value == "start")
        style->align_items = AlignItems::Start;
      else if (value == "flex-end" || value == "end")
        style->align_items = AlignItems::End;
      else if (value == "center")
        style->align_items = AlignItems::Center;
      else if (value == "stretch")
        style->align_items = AlignItems::Stretch;
      style->variables[PROP_ALIGN_ITEMS] = value;
    } else if (prop_sym == PROP_ALIGN_CONTENT) {
      style->variables[PROP_ALIGN_CONTENT] = value;
    } else if (prop_sym == PROP_ALIGN_SELF) {
      style->variables[PROP_ALIGN_SELF] = value;
    } else if (prop_sym == PROP_JUSTIFY_ITEMS) {
      style->variables[PROP_JUSTIFY_ITEMS] = value;
    } else if (prop_sym == PROP_JUSTIFY_SELF) {
      style->variables[PROP_JUSTIFY_SELF] = value;
    } else if (prop_sym == PROP_PLACE_CONTENT) {
      auto parts = split_css_tokens(value);
      if (!parts.empty()) {
        style->variables[PROP_ALIGN_CONTENT] = parts[0];
        const std::string justify_value =
            parts.size() > 1 ? parts[1] : parts[0];
        style->variables[PROP_JUSTIFY_CONTENT] = justify_value;
        if (justify_value == "flex-start" || justify_value == "start")
          style->justify_content = JustifyContent::Start;
        else if (justify_value == "flex-end" || justify_value == "end")
          style->justify_content = JustifyContent::End;
        else if (justify_value == "center")
          style->justify_content = JustifyContent::Center;
        else if (justify_value == "space-between")
          style->justify_content = JustifyContent::SpaceBetween;
        else if (justify_value == "space-around")
          style->justify_content = JustifyContent::SpaceAround;
        else if (justify_value == "space-evenly")
          style->justify_content = JustifyContent::SpaceEvenly;
      }
    } else if (prop_sym == PROP_PLACE_ITEMS) {
      auto parts = split_css_tokens(value);
      if (!parts.empty()) {
        const std::string align_value = parts[0];
        style->variables[PROP_ALIGN_ITEMS] = align_value;
        if (align_value == "flex-start" || align_value == "start")
          style->align_items = AlignItems::Start;
        else if (align_value == "flex-end" || align_value == "end")
          style->align_items = AlignItems::End;
        else if (align_value == "center")
          style->align_items = AlignItems::Center;
        else if (align_value == "stretch")
          style->align_items = AlignItems::Stretch;
        style->variables[PROP_JUSTIFY_ITEMS] =
            parts.size() > 1 ? parts[1] : align_value;
      }
    } else if (prop_sym == PROP_PLACE_SELF) {
      auto parts = split_css_tokens(value);
      if (!parts.empty()) {
        style->variables[PROP_ALIGN_SELF] = parts[0];
        style->variables[PROP_JUSTIFY_SELF] =
            parts.size() > 1 ? parts[1] : parts[0];
      }
    } else if (prop_sym == PROP_GAP)
      style->gap = parse_length(value);
    else if (prop_sym == PROP_FLEX_GROW)
      style->variables[PROP_FLEX_GROW] = value;
    else if (prop_sym == PROP_FLEX_SHRINK)
      style->variables[PROP_FLEX_SHRINK] = value;
    else if (prop_sym == PROP_FLEX_BASIS)
      style->variables[PROP_FLEX_BASIS] = value;
    else if (prop_sym == PROP_FLEX_WRAP)
      style->variables[PROP_FLEX_WRAP] = value;
    else if (prop_sym == PROP_GRID_TEMPLATE_COLUMNS)
      style->variables[PROP_GRID_TEMPLATE_COLUMNS] = value;
    else if (prop_sym == PROP_GRID_TEMPLATE_ROWS)
      style->variables[PROP_GRID_TEMPLATE_ROWS] = value;
    else if (prop_sym == PROP_GRID_TEMPLATE_AREAS)
      style->variables[PROP_GRID_TEMPLATE_AREAS] = value;
    else if (prop_sym == PROP_GRID_AUTO_FLOW)
      style->variables[PROP_GRID_AUTO_FLOW] = value;
    else if (prop_sym == PROP_GRID_AUTO_COLUMNS)
      style->variables[PROP_GRID_AUTO_COLUMNS] = value;
    else if (prop_sym == PROP_GRID_AUTO_ROWS)
      style->variables[PROP_GRID_AUTO_ROWS] = value;
    else if (prop_sym == PROP_GRID_COLUMN)
      style->variables[PROP_GRID_COLUMN] = value;
    else if (prop_sym == PROP_GRID_ROW)
      style->variables[PROP_GRID_ROW] = value;
    else if (prop_sym == PROP_GRID_AREA)
      style->variables[PROP_GRID_AREA] = value;
    else if (prop_sym == PROP_ROW_GAP)
      style->variables[PROP_ROW_GAP] = value;
    else if (prop_sym == PROP_COLUMN_GAP)
      style->variables[PROP_COLUMN_GAP] = value;
    else if (prop_sym == PROP_FLEX) {
      // Shorthand: flex: <grow> [<shrink>] [<basis>]
      // Common cases: flex: 1, flex: 1 1 0, flex: none, flex: auto
      if (value == "none") {
        style->variables[PROP_FLEX_GROW] = "0";
        style->variables[PROP_FLEX_SHRINK] = "0";
      } else if (value == "auto") {
        style->variables[PROP_FLEX_GROW] = "1";
        style->variables[PROP_FLEX_SHRINK] = "1";
      } else {
        std::istringstream ss(value);
        std::string grow, shrink, basis;
        ss >> grow;
        if (ss >> shrink) {
          ss >> basis;
        }
        style->variables[PROP_FLEX_GROW] = grow;
        if (!shrink.empty()) {
          style->variables[PROP_FLEX_SHRINK] = shrink;
        } else {
          style->variables[PROP_FLEX_SHRINK] = "1";
          style->variables[PROP_FLEX_BASIS] = "0";
        }
        if (!basis.empty()) {
          style->variables[PROP_FLEX_BASIS] = basis;
        }
      }
    }

    // Typography
    if (prop_sym == PROP_FONT_SIZE)
      style->font_size = parse_font_size_value(value, style->font_size);
    else if (prop_sym == PROP_FONT_FAMILY)
      style->font_family = parse_font_family_value(value);
    else if (prop_sym == PROP_FONT_WEIGHT) {
      style->font_weight = parse_font_weight_value(value, style->font_weight);
    } else if (prop_sym == PROP_FONT_STYLE) {
      const std::string lowered = to_lower_copy(trim_copy(value));
      if (lowered == "normal")
        style->font_style = FontStyle::Normal;
      else if (lowered == "italic")
        style->font_style = FontStyle::Italic;
      else if (lowered == "oblique" || lowered.rfind("oblique ", 0) == 0)
        style->font_style = FontStyle::Oblique;
    }

    // Colors
    if (prop_sym == PROP_COLOR)
      style->text_color = parse_style_color(value, style, style->text_color);
    else if (prop_sym == PROP_BACKGROUND_COLOR || prop_sym == PROP_BACKGROUND ||
             prop_sym == PROP_BACKGROUND_IMAGE) {
      if (prop_sym == PROP_BACKGROUND &&
          parse_background_shorthand(value, style)) {
        // Parsed shorthand image/color/position/size/repeat.
      } else if ((prop_sym == PROP_BACKGROUND || prop_sym == PROP_BACKGROUND_IMAGE) &&
                 parse_background_image_layers(value, style)) {
        // Parsed multi-layer gradient background-image list.
      } else if ((prop_sym == PROP_BACKGROUND || prop_sym == PROP_BACKGROUND_IMAGE) &&
                 (parse_linear_gradient(value, style) ||
                  parse_radial_gradient(value, style))) {
        if (style->background_layers.empty()) {
          BackgroundImageLayer layer;
          layer.has_gradient = style->has_gradient;
          layer.gradient = style->gradient;
          layer.gradient_type =
              style->get_variable(background_gradient_type_var(), "linear");
          layer.radial_position =
              style->get_variable(background_radial_position_var(), "center");
          layer.radial_size =
              style->get_variable(background_radial_size_var(),
                                  "farthest-corner");
          layer.position = style->get_variable(background_position_var(), "");
          layer.size = style->get_variable(background_size_var(), "");
          layer.repeat = style->get_variable(background_repeat_var(), "");
          layer.origin = style->get_variable(background_origin_var(), "");
          layer.clip = style->get_variable(Symbol("--background-clip"), "");
          style->background_layers = {layer};
        } else {
          sync_primary_background_layer(style);
        }
        // Keep any previously resolved background color as a fallback.
      } else if (prop_sym == PROP_BACKGROUND_IMAGE) {
        if (to_lower_copy(trim_copy(value)) == "none") {
          clear_background_gradient_metadata(style);
          sync_primary_background_layer(style);
        }
      } else if (value.find("gradient") == std::string::npos) {
        if (prop_sym == PROP_BACKGROUND) {
          clear_background_gradient_metadata(style);
          sync_primary_background_layer(style);
        }
        style->background_color =
            parse_style_color(value, style, style->background_color);
      }
    } else if (prop_sym == PROP_BORDER_COLOR)
      parse_border_color_list(value, style);
    else if (prop_sym == PROP_OUTLINE_COLOR)
      style->outline_color = parse_style_color(value, style, style->outline_color);
    else if (prop_sym == PROP_RING_COLOR)
      style->ring_color = parse_style_color(value, style, style->ring_color);
    else if (prop_sym == PROP_RING_OFFSET_COLOR)
      style->ring_offset_color =
          parse_style_color(value, style, style->ring_offset_color);

    // Visibility & Overflow
    if (prop_sym == PROP_OPACITY)
      style->opacity = std::stof(value);
    else if (prop_sym == PROP_VISIBILITY) {
      if (value == "visible")
        style->visibility = Visibility::Visible;
      else if (value == "hidden")
        style->visibility = Visibility::Hidden;
      else if (value == "collapse")
        style->visibility = Visibility::Collapse;
    } else if (prop_sym == PROP_POINTER_EVENTS) {
      style->variables[PROP_POINTER_EVENTS] = value;
    } else if (prop_sym == PROP_OVERFLOW) {
      const std::string lowered = to_lower_copy(trim_copy(value));
      Overflow ov = Overflow::Visible;
      if (lowered == "hidden" || lowered == "clip")
        ov = Overflow::Hidden;
      else if (lowered == "scroll")
        ov = Overflow::Scroll;
      else if (lowered == "auto")
        ov = Overflow::Auto;
      style->overflow_x = style->overflow_y = ov;
    } else if (prop_sym == PROP_OVERFLOW_X) {
      const std::string lowered = to_lower_copy(trim_copy(value));
      if (lowered == "hidden" || lowered == "clip")
        style->overflow_x = Overflow::Hidden;
      else if (lowered == "scroll")
        style->overflow_x = Overflow::Scroll;
      else if (lowered == "auto")
        style->overflow_x = Overflow::Auto;
      else
        style->overflow_x = Overflow::Visible;
    } else if (prop_sym == PROP_OVERFLOW_Y) {
      const std::string lowered = to_lower_copy(trim_copy(value));
      if (lowered == "hidden" || lowered == "clip")
        style->overflow_y = Overflow::Hidden;
      else if (lowered == "scroll")
        style->overflow_y = Overflow::Scroll;
      else if (lowered == "auto")
        style->overflow_y = Overflow::Auto;
      else
        style->overflow_y = Overflow::Visible;
    } else if (prop_sym == PROP_SCROLLBAR_WIDTH) {
      std::string lowered = to_lower_copy(trim_copy(value));
      if (lowered == "auto") {
        lowered = "8px";
      } else if (lowered == "thin") {
        lowered = "6px";
      } else if (lowered == "none") {
        lowered = "0px";
      }
      style->variables[Symbol("--scrollbar-width")] = lowered;
      style->variables[Symbol("--listview-scrollbar-width")] = lowered;
    } else if (prop_sym == PROP_SCROLLBAR_COLOR) {
      const std::string lowered = to_lower_copy(trim_copy(value));
      if (lowered == "auto") {
        style->variables.erase(Symbol("--scrollbar-thumb"));
        style->variables.erase(Symbol("--scrollbar-bg"));
        return;
      }

      const auto parts = split_css_tokens(value);
      if (!parts.empty()) {
        style->variables[Symbol("--scrollbar-thumb")] =
            color_to_css_variable_value(
                parse_style_color(parts[0], style, style->text_color));
      }
      if (parts.size() > 1) {
        style->variables[Symbol("--scrollbar-bg")] =
            color_to_css_variable_value(
                parse_style_color(parts[1], style, style->background_color));
      }
    }

    // Transform
    if (prop_sym == PROP_TRANSLATE) {
      parse_individual_translate(value, style);
    } else if (prop_sym == PROP_SCALE) {
      parse_individual_scale(value, style);
    } else if (prop_sym == PROP_ROTATE) {
      parse_individual_rotate(value, style);
    } else if (prop_sym == PROP_TRANSFORM) {
      parse_transform(value, style);
    } else if (prop_sym == PROP_TRANSFORM_ORIGIN) {
      parse_transform_origin(value, style);
    }

    // Effects
    if (prop_sym == PROP_BOX_SHADOW)
      parse_box_shadow(value, style);
    else if (prop_sym == PROP_TEXT_SHADOW)
      parse_text_shadow(value, style);
    else if (prop_sym == PROP_OUTLINE)
      parse_outline(value, style);
    else if (prop_sym == PROP_OUTLINE_STYLE)
      parse_outline_style(value, style);
    else if (prop_sym == PROP_OUTLINE_WIDTH)
      style->outline_width =
          parse_border_width_token(to_lower_copy(trim_copy(value)));
    else if (prop_sym == PROP_OUTLINE_OFFSET)
      style->outline_offset = parse_length(value);
    else if (prop_sym == PROP_RING)
      parse_ring(value, style);
    else if (prop_sym == PROP_RING_WIDTH)
      style->ring_width = parse_length(value);
    else if (prop_sym == PROP_RING_OFFSET)
      style->ring_offset = parse_length(value);
    else if (prop_sym == PROP_TRANSITION) {
      style->transition = value;
      style->variables.erase(transition_property_var());
      style->variables.erase(transition_duration_var());
      style->variables.erase(transition_delay_var());
      style->variables.erase(transition_timing_var());
    } else if (prop_sym == PROP_TRANSITION_PROPERTY) {
      style->variables[transition_property_var()] = value;
      style->transition = rebuild_transition_shorthand(style);
    } else if (prop_sym == PROP_TRANSITION_DURATION) {
      style->variables[transition_duration_var()] = value;
      style->transition = rebuild_transition_shorthand(style);
    } else if (prop_sym == PROP_TRANSITION_DELAY) {
      style->variables[transition_delay_var()] = value;
      style->transition = rebuild_transition_shorthand(style);
    } else if (prop_sym == PROP_TRANSITION_TIMING_FUNCTION) {
      style->variables[transition_timing_var()] = value;
      style->transition = rebuild_transition_shorthand(style);
    } else if (prop_sym == PROP_ANIMATION) {
      parse_animation_shorthand(value, style);
    } else if (prop_sym == PROP_ANIMATION_NAME) {
      style->animation_name = value == "none" ? "" : value;
      apply_animation_longhand_list(
          style, value, [](AnimationStyleEntry& animation, const std::string& item) {
            animation.name = to_lower_copy(item) == "none" ? "" : item;
          });
    } else if (prop_sym == PROP_ANIMATION_DURATION) {
      style->animation_duration_ms = parse_time_ms(value);
      apply_animation_longhand_list(
          style, value, [this](AnimationStyleEntry& animation, const std::string& item) {
            animation.duration_ms = parse_time_ms(item);
          });
    } else if (prop_sym == PROP_ANIMATION_DELAY) {
      style->animation_delay_ms = parse_time_ms(value);
      apply_animation_longhand_list(
          style, value, [this](AnimationStyleEntry& animation, const std::string& item) {
            animation.delay_ms = parse_time_ms(item);
          });
    } else if (prop_sym == PROP_ANIMATION_TIMING_FUNCTION) {
      style->animation_timing = parse_easing_type(value);
      apply_animation_longhand_list(
          style, value, [this](AnimationStyleEntry& animation, const std::string& item) {
            animation.timing = parse_easing_type(item);
          });
    } else if (prop_sym == PROP_ANIMATION_ITERATION_COUNT) {
      parse_animation_iteration_count(value, style->animation_iteration_count,
                                      style->animation_infinite);
      apply_animation_longhand_list(
          style, value, [this](AnimationStyleEntry& animation, const std::string& item) {
            parse_animation_iteration_count(item, animation.iteration_count,
                                            animation.infinite);
          });
    } else if (prop_sym == PROP_ANIMATION_FILL_MODE) {
      style->animation_fill_mode = parse_animation_fill_mode(value);
      apply_animation_longhand_list(
          style, value, [this](AnimationStyleEntry& animation, const std::string& item) {
            animation.fill_mode = parse_animation_fill_mode(item);
          });
    } else if (prop_sym == PROP_ANIMATION_DIRECTION) {
      style->animation_direction = parse_animation_direction(value);
      apply_animation_longhand_list(
          style, value, [this](AnimationStyleEntry& animation, const std::string& item) {
            animation.direction = parse_animation_direction(item);
          });
    } else if (prop_sym == PROP_ANIMATION_PLAY_STATE) {
      style->animation_play_state = parse_animation_play_state(value);
      apply_animation_longhand_list(
          style, value, [this](AnimationStyleEntry& animation, const std::string& item) {
            animation.play_state = parse_animation_play_state(item);
          });
    }
  }

  detail::CssLengthContext length_context(
      float percent_reference = detail::css_nan()) const {
    detail::CssLengthContext context;
    context.percent_reference = percent_reference;
    if (current_element_ && current_element_->owner_box_) {
      context.viewport_width = current_element_->owner_box_->viewport_width();
      context.viewport_height = current_element_->owner_box_->viewport_height();
    }
    return context;
  }

  float parse_length(const std::string &value,
                     float percent_reference = detail::css_nan()) {
    const std::string trimmed = trim_copy(value);
    if (trimmed.empty() || trimmed == "auto")
      return 0;
    const float parsed =
        detail::parse_css_length(trimmed, length_context(percent_reference));
    return std::isnan(parsed) ? 0.0f : parsed;
  }

  bool assign_size(const std::string& value, CssSize& size, float& projection,
                   bool& percent_projection) {
    const std::string trimmed = to_lower_copy(trim_copy(value));
    if (trimmed == "auto") {
      size = {};
      projection = 0.0f;
      percent_projection = false;
      return true;
    }
    const float parsed = detail::parse_css_length(trimmed, length_context());
    if (std::isnan(parsed)) {
      return false;
    }
    size.kind = detail::is_css_math_length(trimmed)
                    ? CssSizeKind::Expression
                    : (!trimmed.empty() && trimmed.back() == '%'
                           ? CssSizeKind::Percentage
                           : CssSizeKind::Length);
    size.value = parsed;
    size.expression = size.kind == CssSizeKind::Expression ? trimmed : "";
    projection = parsed;
    percent_projection = size.kind == CssSizeKind::Percentage;
    return true;
  }

  float parse_font_size_value(const std::string& value, float inherited_size) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered == "xx-small") return 9.0f;
    if (lowered == "x-small") return 10.0f;
    if (lowered == "small") return 13.0f;
    if (lowered == "medium") return 16.0f;
    if (lowered == "large") return 18.0f;
    if (lowered == "x-large") return 24.0f;
    if (lowered == "xx-large") return 32.0f;
    if (lowered == "xxx-large") return 48.0f;
    if (lowered == "smaller") return inherited_size / 1.2f;
    if (lowered == "larger") return inherited_size * 1.2f;
    return parse_length(lowered);
  }

  FontWeight parse_font_weight_value(const std::string& value,
                                     FontWeight inherited_weight) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered == "normal") return FontWeight::Normal;
    if (lowered == "bold") return FontWeight::Bold;
    if (lowered == "light" || lowered == "300") return FontWeight::Light;
    if (lowered == "medium" || lowered == "500") return FontWeight::Medium;
    if (lowered == "600") return FontWeight::SemiBold;
    if (lowered == "800") return FontWeight::ExtraBold;
    if (lowered == "900") return FontWeight::Black;
    if (lowered == "bolder") {
      const int current = static_cast<int>(inherited_weight);
      if (current < 350) return FontWeight::Normal;
      if (current < 600) return FontWeight::Bold;
      return FontWeight::Black;
    }
    if (lowered == "lighter") {
      const int current = static_cast<int>(inherited_weight);
      if (current < 600) return static_cast<FontWeight>(100);
      if (current < 800) return FontWeight::Normal;
      return FontWeight::Bold;
    }
    return static_cast<FontWeight>(std::stoi(lowered));
  }

  std::string parse_font_family_value(const std::string& value) {
    bool in_string = false;
    char quote_char = '\0';
    int paren_depth = 0;
    for (size_t i = 0; i < value.size(); ++i) {
      const char ch = value[i];
      if (in_string) {
        if (ch == quote_char) {
          in_string = false;
        }
        continue;
      }
      if (ch == '"' || ch == '\'') {
        in_string = true;
        quote_char = ch;
        continue;
      }
      if (ch == '(') {
        ++paren_depth;
        continue;
      }
      if (ch == ')') {
        if (paren_depth > 0) {
          --paren_depth;
        }
        continue;
      }
      if (ch == ',' && paren_depth == 0) {
        return strip_quotes_copy(trim_copy(value.substr(0, i)));
      }
    }
    return strip_quotes_copy(trim_copy(value));
  }

  float parse_position_offset(const std::string &value) {
    if (value.empty() || value == "auto")
      return NAN;
    return parse_length(value);
  }

  float parse_time_ms(const std::string& value) {
    const std::string trimmed = trim_copy(value);
    if (trimmed.empty()) {
      return 0.0f;
    }
    if (trimmed.size() >= 2 &&
        trimmed.compare(trimmed.size() - 2, 2, "ms") == 0) {
      return std::stof(trimmed.substr(0, trimmed.size() - 2));
    }
    if (!trimmed.empty() && trimmed.back() == 's') {
      return std::stof(trimmed.substr(0, trimmed.size() - 1)) * 1000.0f;
    }
    return std::stof(trimmed);
  }

  bool is_time_value_token(const std::string& value) {
    const std::string trimmed = trim_copy(value);
    if (trimmed.empty()) {
      return false;
    }

    size_t numeric_end = 0;
    if (trimmed.size() >= 2 &&
        trimmed.compare(trimmed.size() - 2, 2, "ms") == 0) {
      numeric_end = trimmed.size() - 2;
    } else if (trimmed.back() == 's') {
      numeric_end = trimmed.size() - 1;
    } else {
      return false;
    }

    if (numeric_end == 0) {
      return false;
    }

    bool saw_digit = false;
    for (size_t i = 0; i < numeric_end; ++i) {
      const char ch = trimmed[i];
      if (std::isdigit(static_cast<unsigned char>(ch))) {
        saw_digit = true;
        continue;
      }
      if (ch == '.' || ch == '+' || ch == '-') {
        continue;
      }
      return false;
    }

    return saw_digit;
  }

  EasingType parse_easing_type(const std::string& value) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered == "linear") {
      return EasingType::Linear;
    }
    if (lowered == "ease-in") {
      return EasingType::EaseIn;
    }
    if (lowered == "ease-out") {
      return EasingType::EaseOut;
    }
    if (lowered == "ease-in-out") {
      return EasingType::EaseInOut;
    }
    if (lowered.find("cubic-bezier") == 0) {
      return EasingType::CubicBezier;
    }
    return EasingType::Ease;
  }

  void parse_animation_iteration_count(const std::string& value,
                                       float& iteration_count,
                                       bool& infinite) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered == "infinite") {
      infinite = true;
      iteration_count = 1.0f;
      return;
    }
    infinite = false;
    iteration_count = std::max(std::stof(lowered), 0.0f);
  }

  AnimationFillMode parse_animation_fill_mode(const std::string& value) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered == "forwards") {
      return AnimationFillMode::Forwards;
    }
    if (lowered == "backwards") {
      return AnimationFillMode::Backwards;
    }
    if (lowered == "both") {
      return AnimationFillMode::Both;
    }
    return AnimationFillMode::None;
  }

  AnimationDirection parse_animation_direction(const std::string& value) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered == "reverse") {
      return AnimationDirection::Reverse;
    }
    if (lowered == "alternate") {
      return AnimationDirection::Alternate;
    }
    if (lowered == "alternate-reverse") {
      return AnimationDirection::AlternateReverse;
    }
    return AnimationDirection::Normal;
  }

  AnimationPlayState parse_animation_play_state(const std::string& value) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered == "paused") {
      return AnimationPlayState::Paused;
    }
    return AnimationPlayState::Running;
  }

  AnimationStyleEntry make_default_animation_entry() {
    return AnimationStyleEntry{};
  }

  void apply_animation_entry_to_primary_fields(const AnimationStyleEntry& entry,
                                               ComputedStyle* style) {
    if (!style) {
      return;
    }
    style->animation_name = entry.name;
    style->animation_duration_ms = entry.duration_ms;
    style->animation_delay_ms = entry.delay_ms;
    style->animation_timing = entry.timing;
    style->animation_iteration_count = entry.iteration_count;
    style->animation_infinite = entry.infinite;
    style->animation_fill_mode = entry.fill_mode;
    style->animation_direction = entry.direction;
    style->animation_play_state = entry.play_state;
  }

  void sync_single_animation_entry(ComputedStyle* style) {
    if (!style) {
      return;
    }

    AnimationStyleEntry entry;
    entry.name = style->animation_name;
    entry.duration_ms = style->animation_duration_ms;
    entry.delay_ms = style->animation_delay_ms;
    entry.timing = style->animation_timing;
    entry.iteration_count = style->animation_iteration_count;
    entry.infinite = style->animation_infinite;
    entry.fill_mode = style->animation_fill_mode;
    entry.direction = style->animation_direction;
    entry.play_state = style->animation_play_state;

    style->animations.clear();
    style->animations.push_back(entry);
  }

  template <typename Setter>
  void apply_animation_longhand_list(ComputedStyle* style,
                                     const std::string& value,
                                     Setter&& setter) {
    if (!style) {
      return;
    }

    auto items = split_top_level_csv(value);
    if (items.empty()) {
      items.push_back(value);
    }

    const size_t target_count = std::max(style->animations.size(), items.size());
    if (target_count == 0) {
      style->animations.push_back(make_default_animation_entry());
    } else if (style->animations.size() < target_count) {
      style->animations.resize(target_count, make_default_animation_entry());
    }

    for (size_t i = 0; i < style->animations.size(); ++i) {
      const std::string& item = items[std::min(i, items.size() - 1)];
      setter(style->animations[i], trim_copy(item));
    }

    apply_animation_entry_to_primary_fields(style->animations.front(), style);
  }

  AnimationStyleEntry parse_single_animation_shorthand_item(const std::string& value) {
    AnimationStyleEntry entry = make_default_animation_entry();
    const auto tokens = split_css_tokens(value);
    bool saw_duration = false;
    bool saw_delay = false;

    for (const auto& token : tokens) {
      const std::string lowered = to_lower_copy(trim_copy(token));
      if (lowered.empty()) {
        continue;
      }

      if (is_time_value_token(lowered)) {
        const float time_ms = parse_time_ms(lowered);
        if (!saw_duration) {
          entry.duration_ms = time_ms;
          saw_duration = true;
        } else if (!saw_delay) {
          entry.delay_ms = time_ms;
          saw_delay = true;
        }
        continue;
      }

      if (lowered == "linear" || lowered == "ease" ||
          lowered == "ease-in" || lowered == "ease-out" ||
          lowered == "ease-in-out" ||
          lowered.find("cubic-bezier") == 0) {
        entry.timing = parse_easing_type(lowered);
        continue;
      }

      if (lowered == "none" || lowered == "forwards" ||
          lowered == "backwards" || lowered == "both") {
        if (lowered == "none") {
          entry.name.clear();
        } else {
          entry.fill_mode = parse_animation_fill_mode(lowered);
        }
        continue;
      }

      if (lowered == "normal" || lowered == "reverse" ||
          lowered == "alternate" || lowered == "alternate-reverse") {
        entry.direction = parse_animation_direction(lowered);
        continue;
      }

      if (lowered == "running" || lowered == "paused") {
        entry.play_state = parse_animation_play_state(lowered);
        continue;
      }

      if (lowered == "infinite" ||
          std::isdigit(static_cast<unsigned char>(lowered.front()))) {
        parse_animation_iteration_count(lowered, entry.iteration_count,
                                        entry.infinite);
        continue;
      }

      entry.name = token;
    }

    return entry;
  }

  void parse_animation_shorthand(const std::string& value,
                                 ComputedStyle* style) {
    if (!style) {
      return;
    }

    const auto items = split_top_level_csv(value);
    if (items.empty()) {
      apply_animation_entry_to_primary_fields(make_default_animation_entry(), style);
      style->animations.clear();
      return;
    }

    style->animations.clear();
    for (const auto& item : items) {
      style->animations.push_back(parse_single_animation_shorthand_item(item));
    }

    if (!style->animations.empty()) {
      apply_animation_entry_to_primary_fields(style->animations.front(), style);
    } else {
      apply_animation_entry_to_primary_fields(make_default_animation_entry(), style);
    }
  }

  Color parse_color(const std::string &value) {
    const std::string input = to_lower_copy(trim_copy(value));
    if (input.empty())
      return Color(0.0f, 0.0f, 0.0f, 0.0f);

    // Widget bridge variables historically store RGBA as four comma-separated
    // components. Accept that representation after var() resolution so the
    // same token can feed standard CSS color properties on semantic parts.
    const auto serialized = split_top_level_csv(input);
    if (serialized.size() == 4) {
      float channels[4] = {};
      bool valid = true;
      for (size_t i = 0; i < serialized.size(); ++i) {
        const std::string token = trim_copy(serialized[i]);
        char* end = nullptr;
        channels[i] = std::strtof(token.c_str(), &end);
        if (end == token.c_str() || (end && *end != '\0')) {
          valid = false;
          break;
        }
      }
      if (valid) {
        constexpr float channel_max = 255.0f;
        const auto rgb = [channel_max](float channel) {
          return std::clamp(channel, 0.0f, channel_max) / channel_max;
        };
        const float alpha = channels[3] > 1.0f
                                ? rgb(channels[3])
                                : std::clamp(channels[3], 0.0f, 1.0f);
        return Color(rgb(channels[0]), rgb(channels[1]), rgb(channels[2]),
                     alpha);
      }
    }

    // Hex color
    if (input[0] == '#') {
      std::string hex = input.substr(1);
      if (hex.size() == 3) {
        // #RGB -> #RRGGBB
        hex = std::string(2, hex[0]) + std::string(2, hex[1]) + std::string(2, hex[2]);
      }
      if (hex.size() == 6) {
        unsigned int r, g, b;
        sscanf(hex.c_str(), "%2x%2x%2x", &r, &g, &b);
        return Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
      }
      if (hex.size() == 8) {
        unsigned int r, g, b, a;
        sscanf(hex.c_str(), "%2x%2x%2x%2x", &r, &g, &b, &a);
        return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
      }
    }

    const auto parse_alpha_token = [](const std::string& token) {
      if (token.empty()) {
        return 1.0f;
      }
      if (token.back() == '%') {
        return std::clamp(
            std::strtof(token.substr(0, token.size() - 1).c_str(), nullptr) /
                100.0f,
            0.0f, 1.0f);
      }
      return std::clamp(std::strtof(token.c_str(), nullptr), 0.0f, 1.0f);
    };

    const auto normalize_color_function_tokens = [](std::string inner) {
      for (char& c : inner) {
        if (c == ',' || c == '/') {
          c = ' ';
        }
      }
      return split_css_tokens(inner);
    };

    if (input.rfind("light-dark(", 0) == 0 && input.back() == ')') {
      const std::string inner = input.substr(11, input.size() - 12);
      const auto items = split_top_level_csv(inner);
      if (!items.empty()) {
        // flexUI currently has a stable light default unless explicit media rules
        // override theme tokens.
        return parse_color(items.front());
      }
    }

    if (input.rfind("color-mix(", 0) == 0 && input.back() == ')') {
      struct MixPart {
        Color color;
        float weight = NAN;
      };

      const auto parse_mix_part = [&](const std::string& raw) -> MixPart {
        auto tokens = split_css_tokens(trim_copy(raw));
        float weight = NAN;
        if (!tokens.empty() && !tokens.back().empty() && tokens.back().back() == '%') {
          weight = std::clamp(
              std::strtof(tokens.back().substr(0, tokens.back().size() - 1).c_str(),
                          nullptr) /
                  100.0f,
              0.0f, 1.0f);
          tokens.pop_back();
        }
        return {parse_color(join_space_separated_tokens(tokens)), weight};
      };

      const std::string inner = input.substr(10, input.size() - 11);
      const auto items = split_top_level_csv(inner);
      if (items.size() >= 3) {
        MixPart first = parse_mix_part(items[1]);
        MixPart second = parse_mix_part(items[2]);
        if (std::isnan(first.weight) && std::isnan(second.weight)) {
          first.weight = 0.5f;
          second.weight = 0.5f;
        } else if (std::isnan(first.weight)) {
          first.weight = 1.0f - second.weight;
        } else if (std::isnan(second.weight)) {
          second.weight = 1.0f - first.weight;
        }

        const float sum = first.weight + second.weight;
        if (sum > 0.000001f) {
          first.weight /= sum;
          second.weight /= sum;
        }

        return Color(first.color.r * first.weight + second.color.r * second.weight,
                     first.color.g * first.weight + second.color.g * second.weight,
                     first.color.b * first.weight + second.color.b * second.weight,
                     first.color.a * first.weight + second.color.a * second.weight);
      }
    }

    const auto parse_rgb_component = [](const std::string& token) {
      if (!token.empty() && token.back() == '%') {
        return std::clamp(
            std::strtof(token.substr(0, token.size() - 1).c_str(), nullptr) /
                100.0f,
            0.0f, 1.0f);
      }
      return std::clamp(std::strtof(token.c_str(), nullptr) / 255.0f, 0.0f,
                        1.0f);
    };

    // rgb()/rgba() support both comma and modern slash-alpha syntaxes.
    if (input.rfind("rgb", 0) == 0) {
      const size_t start = input.find('(');
      const size_t end = input.rfind(')');
      if (start != std::string::npos && end != std::string::npos && end > start) {
        const auto tokens =
            normalize_color_function_tokens(input.substr(start + 1, end - start - 1));
        if (tokens.size() >= 3) {
          return Color(parse_rgb_component(tokens[0]), parse_rgb_component(tokens[1]),
                       parse_rgb_component(tokens[2]),
                       tokens.size() >= 4 ? parse_alpha_token(tokens[3]) : 1.0f);
        }
      }
    }

    const auto parse_percentage_token = [](const std::string& token) {
      if (!token.empty() && token.back() == '%') {
        return std::clamp(
            std::strtof(token.substr(0, token.size() - 1).c_str(), nullptr) /
                100.0f,
            0.0f, 1.0f);
      }
      return std::clamp(std::strtof(token.c_str(), nullptr), 0.0f, 1.0f);
    };

    const auto parse_oklch_lightness_token = [](const std::string& token) {
      if (!token.empty() && token.back() == '%') {
        return std::clamp(
            std::strtof(token.substr(0, token.size() - 1).c_str(), nullptr) /
                100.0f,
            0.0f, 1.0f);
      }
      return std::clamp(std::strtof(token.c_str(), nullptr), 0.0f, 1.0f);
    };

    const auto parse_angle_degrees = [](std::string token) {
      if (ends_with_copy(token, "deg")) {
        token = token.substr(0, token.size() - 3);
      } else if (ends_with_copy(token, "rad")) {
        return std::strtof(token.substr(0, token.size() - 3).c_str(), nullptr) *
               180.0f / 3.14159265358979323846f;
      } else if (ends_with_copy(token, "turn")) {
        return std::strtof(token.substr(0, token.size() - 4).c_str(), nullptr) *
               360.0f;
      }
      return std::strtof(token.c_str(), nullptr);
    };

    const auto hue_to_rgb = [](float p, float q, float t) {
      if (t < 0.0f) t += 1.0f;
      if (t > 1.0f) t -= 1.0f;
      if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
      if (t < 1.0f / 2.0f) return q;
      if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
      return p;
    };

    // hsl()/hsla() is required for shadcn-style hsl(var(--token) / alpha).
    if (input.rfind("hsl", 0) == 0) {
      const size_t start = input.find('(');
      const size_t end = input.rfind(')');
      if (start != std::string::npos && end != std::string::npos && end > start) {
        const auto tokens =
            normalize_color_function_tokens(input.substr(start + 1, end - start - 1));
        if (tokens.size() >= 3) {
          float h = std::fmod(std::strtof(tokens[0].c_str(), nullptr), 360.0f);
          if (h < 0.0f) h += 360.0f;
          h /= 360.0f;
          const float s = parse_percentage_token(tokens[1]);
          const float l = parse_percentage_token(tokens[2]);
          const float a = tokens.size() >= 4 ? parse_alpha_token(tokens[3]) : 1.0f;

          if (s <= 0.0f) {
            return Color(l, l, l, a);
          }

          const float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
          const float p = 2.0f * l - q;
          return Color(hue_to_rgb(p, q, h + 1.0f / 3.0f),
                       hue_to_rgb(p, q, h),
                       hue_to_rgb(p, q, h - 1.0f / 3.0f), a);
        }
      }
    }

    const auto linear_to_srgb = [](float value) {
      value = std::clamp(value, 0.0f, 1.0f);
      if (value <= 0.0031308f) {
        return 12.92f * value;
      }
      return 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    };

    // OKLCH is used by modern Tailwind/shadcn theme tokens.
    if (input.rfind("oklch", 0) == 0) {
      const size_t start = input.find('(');
      const size_t end = input.rfind(')');
      if (start != std::string::npos && end != std::string::npos && end > start) {
        const auto tokens =
            normalize_color_function_tokens(input.substr(start + 1, end - start - 1));
        if (tokens.size() >= 3) {
          const float l = parse_oklch_lightness_token(tokens[0]);
          const float c = std::max(std::strtof(tokens[1].c_str(), nullptr), 0.0f);
          const float h = parse_angle_degrees(tokens[2]) *
                          3.14159265358979323846f / 180.0f;
          const float a = tokens.size() >= 4 ? parse_alpha_token(tokens[3]) : 1.0f;

          const float ok_a = c * std::cos(h);
          const float ok_b = c * std::sin(h);
          const float l_ = l + 0.3963377774f * ok_a + 0.2158037573f * ok_b;
          const float m_ = l - 0.1055613458f * ok_a - 0.0638541728f * ok_b;
          const float s_ = l - 0.0894841775f * ok_a - 1.2914855480f * ok_b;
          const float l3 = l_ * l_ * l_;
          const float m3 = m_ * m_ * m_;
          const float s3 = s_ * s_ * s_;

          const float r_linear =
              4.0767416621f * l3 - 3.3077115913f * m3 + 0.2309699292f * s3;
          const float g_linear =
              -1.2684380046f * l3 + 2.6097574011f * m3 - 0.3413193965f * s3;
          const float b_linear =
              -0.0041960863f * l3 - 0.7034186147f * m3 + 1.7076147010f * s3;
          return Color(linear_to_srgb(r_linear), linear_to_srgb(g_linear),
                       linear_to_srgb(b_linear), a);
        }
      }
    }

    if (input.rfind("oklab", 0) == 0) {
      const size_t start = input.find('(');
      const size_t end = input.rfind(')');
      if (start != std::string::npos && end != std::string::npos && end > start) {
        const auto tokens =
            normalize_color_function_tokens(input.substr(start + 1, end - start - 1));
        if (tokens.size() >= 3) {
          const float l = parse_oklch_lightness_token(tokens[0]);
          const float ok_a = std::strtof(tokens[1].c_str(), nullptr);
          const float ok_b = std::strtof(tokens[2].c_str(), nullptr);
          const float alpha = tokens.size() >= 4 ? parse_alpha_token(tokens[3]) : 1.0f;

          const float l_ = l + 0.3963377774f * ok_a + 0.2158037573f * ok_b;
          const float m_ = l - 0.1055613458f * ok_a - 0.0638541728f * ok_b;
          const float s_ = l - 0.0894841775f * ok_a - 1.2914855480f * ok_b;
          const float l3 = l_ * l_ * l_;
          const float m3 = m_ * m_ * m_;
          const float s3 = s_ * s_ * s_;

          const float r_linear =
              4.0767416621f * l3 - 3.3077115913f * m3 + 0.2309699292f * s3;
          const float g_linear =
              -1.2684380046f * l3 + 2.6097574011f * m3 - 0.3413193965f * s3;
          const float b_linear =
              -0.0041960863f * l3 - 0.7034186147f * m3 + 1.7076147010f * s3;
          return Color(linear_to_srgb(r_linear), linear_to_srgb(g_linear),
                       linear_to_srgb(b_linear), alpha);
        }
      }
    }

    // Named colors (common ones) - now using float [0.0-1.0]
    if (input == "transparent")
      return Color(0.0f, 0.0f, 0.0f, 0.0f);
    if (input == "black")
      return Color(0.0f, 0.0f, 0.0f, 1.0f);
    if (input == "white")
      return Color(1.0f, 1.0f, 1.0f, 1.0f);
    if (input == "red")
      return Color(1.0f, 0.0f, 0.0f, 1.0f);
    if (input == "green")
      return Color(0.0f, 0.5f, 0.0f, 1.0f); // CSS green is #008000
    if (input == "blue")
      return Color(0.0f, 0.0f, 1.0f, 1.0f);
    if (input == "yellow")
      return Color(1.0f, 1.0f, 0.0f, 1.0f);
    if (input == "gray" || input == "grey")
      return Color(0.5f, 0.5f, 0.5f, 1.0f);

    return Color(0.0f, 0.0f, 0.0f, 1.0f); // Default: black
  }

  Color parse_style_color(const std::string& value, const ComputedStyle* style,
                          const Color& fallback = Color(0.0f, 0.0f, 0.0f, 1.0f)) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered == "currentcolor") {
      return style ? style->text_color : fallback;
    }
    if (lowered == "inherit" || lowered == "unset") {
      return fallback;
    }
    if (lowered == "initial") {
      return Color(0.0f, 0.0f, 0.0f, 1.0f);
    }
    return parse_color(value);
  }

  void clear_background_gradient_metadata(ComputedStyle* style) {
    if (!style) {
      return;
    }
    style->background_layers.clear();
    style->variables.erase(background_gradient_type_var());
    style->variables.erase(background_radial_position_var());
    style->variables.erase(background_radial_size_var());
  }

  void sync_primary_background_layer(ComputedStyle* style) {
    if (!style) {
      return;
    }

    if (style->background_layers.empty()) {
      style->has_gradient = false;
      style->gradient = LinearGradient{};
      style->variables.erase(background_gradient_type_var());
      style->variables.erase(background_radial_position_var());
      style->variables.erase(background_radial_size_var());
      style->variables.erase(background_position_var());
      style->variables.erase(background_position_x_var());
      style->variables.erase(background_position_y_var());
      style->variables.erase(background_size_var());
      style->variables.erase(background_repeat_var());
      style->variables.erase(background_origin_var());
      return;
    }

    const auto& layer = style->background_layers.front();
    if (!layer.origin.empty()) {
      if (layer.origin == "padding-box") {
        style->background_origin = BackgroundClip::PaddingBox;
      } else if (layer.origin == "content-box") {
        style->background_origin = BackgroundClip::ContentBox;
      } else {
        style->background_origin = BackgroundClip::BorderBox;
      }
      style->variables[background_origin_var()] = layer.origin;
    } else {
      style->variables.erase(background_origin_var());
    }

    if (!layer.clip.empty()) {
      if (layer.clip == "padding-box") {
        style->background_clip = BackgroundClip::PaddingBox;
      } else if (layer.clip == "content-box") {
        style->background_clip = BackgroundClip::ContentBox;
      } else {
        style->background_clip = BackgroundClip::BorderBox;
      }
      style->variables[Symbol("--background-clip")] = layer.clip;
    } else {
      style->variables.erase(Symbol("--background-clip"));
    }

    style->has_gradient = layer.has_gradient;
    if (layer.has_gradient) {
      style->gradient = layer.gradient;
      style->variables[background_gradient_type_var()] = layer.gradient_type;
      if (layer.gradient_type == "radial") {
        style->variables[background_radial_position_var()] = layer.radial_position;
        style->variables[background_radial_size_var()] = layer.radial_size;
      } else {
        style->variables.erase(background_radial_position_var());
        style->variables.erase(background_radial_size_var());
      }
    } else {
      style->gradient = LinearGradient{};
      style->variables.erase(background_gradient_type_var());
      style->variables.erase(background_radial_position_var());
      style->variables.erase(background_radial_size_var());
    }

    if (!layer.position.empty()) {
      style->variables[background_position_var()] = layer.position;
    } else {
      style->variables.erase(background_position_var());
    }
    style->variables.erase(background_position_x_var());
    style->variables.erase(background_position_y_var());
    if (!layer.size.empty()) {
      style->variables[background_size_var()] = layer.size;
    } else {
      style->variables.erase(background_size_var());
    }
    if (!layer.repeat.empty()) {
      style->variables[background_repeat_var()] = layer.repeat;
    } else {
      style->variables.erase(background_repeat_var());
    }
  }

  bool parse_gradient_layer_value(const std::string& value,
                                  BackgroundImageLayer* layer) {
    if (!layer) {
      return false;
    }

    ComputedStyle scratch;
    if (!(parse_linear_gradient(value, &scratch) ||
          parse_radial_gradient(value, &scratch))) {
      return false;
    }

    layer->has_gradient = scratch.has_gradient;
    layer->has_image_url = false;
    layer->gradient = scratch.gradient;
    layer->gradient_type =
        scratch.get_variable(background_gradient_type_var(), "linear");
    layer->image_url.clear();
    layer->radial_position =
        scratch.get_variable(background_radial_position_var(), "center");
    layer->radial_size =
        scratch.get_variable(background_radial_size_var(), "farthest-corner");
    layer->origin = scratch.get_variable(background_origin_var(), "");
    layer->clip = scratch.get_variable(Symbol("--background-clip"), "");
    return layer->has_gradient;
  }

  bool parse_background_url_layer_value(const std::string& value,
                                        BackgroundImageLayer* layer) {
    if (!layer) {
      return false;
    }

    const std::string trimmed = transition_trim_copy(value);
    const std::string lowered = to_lower_copy(trimmed);
    static const std::string prefix = "url(";
    if (lowered.rfind(prefix, 0) != 0 || trimmed.empty() || trimmed.back() != ')') {
      return false;
    }

    std::string image_url =
        transition_trim_copy(trimmed.substr(prefix.size(), trimmed.size() - prefix.size() - 1));
    if (image_url.size() >= 2) {
      const char quote = image_url.front();
      if ((quote == '"' || quote == '\'') && image_url.back() == quote) {
        image_url = image_url.substr(1, image_url.size() - 2);
      }
    }

    layer->has_gradient = false;
    layer->has_image_url = true;
    layer->gradient = LinearGradient{};
    layer->gradient_type = "linear";
    layer->image_url = std::move(image_url);
    layer->radial_position = "center";
    layer->radial_size = "farthest-corner";
    layer->origin.clear();
    layer->clip.clear();
    return true;
  }

  bool parse_background_image_layer_value(const std::string& value,
                                          BackgroundImageLayer* layer) {
    return parse_gradient_layer_value(value, layer) ||
           parse_background_url_layer_value(value, layer);
  }

  void apply_background_layer_metadata_list(const std::string& value,
                                            std::vector<BackgroundImageLayer>* layers,
                                            const char* kind) {
    if (!layers || !kind || layers->empty()) {
      return;
    }

    const auto items = split_top_level_csv(to_lower_copy(trim_copy(value)));
    if (items.empty()) {
      return;
    }

    const auto apply_to_layer = [&](BackgroundImageLayer& layer,
                                    const std::string& item) {
      if (std::string(kind) == "position") {
        layer.position = item;
      } else if (std::string(kind) == "size") {
        layer.size = item;
      } else if (std::string(kind) == "repeat") {
        layer.repeat = item;
      } else if (std::string(kind) == "origin") {
        layer.origin = item;
      } else if (std::string(kind) == "clip") {
        layer.clip = item;
      }
    };

    for (size_t i = 0; i < layers->size(); ++i) {
      const std::string item =
          transition_trim_copy(items[std::min(i, items.size() - 1)]);
      apply_to_layer((*layers)[i], item);
    }
  }

  void hydrate_background_layer_metadata(ComputedStyle* style) {
    if (!style || style->background_layers.empty()) {
      return;
    }

    const auto apply_from_var = [&](Symbol variable, const char* kind) {
      const auto it = style->variables.find(variable);
      if (it == style->variables.end()) {
        return;
      }
      apply_background_layer_metadata_list(it->second, &style->background_layers,
                                           kind);
    };

    apply_from_var(background_position_var(), "position");
    apply_from_var(background_size_var(), "size");
    apply_from_var(background_repeat_var(), "repeat");
    apply_from_var(background_origin_var(), "origin");
    apply_from_var(Symbol("--background-clip"), "clip");
  }

  bool parse_background_image_layers(const std::string& value,
                                     ComputedStyle* style) {
    if (!style) {
      return false;
    }

    const auto items = split_top_level_csv(value);
    if (items.empty()) {
      return false;
    }

    std::vector<BackgroundImageLayer> layers;
    for (const auto& item : items) {
      const std::string trimmed = transition_trim_copy(item);
      const std::string lowered = to_lower_copy(trimmed);
      if (trimmed.empty()) {
        continue;
      }
      if (lowered == "none") {
        continue;
      }

      BackgroundImageLayer layer;
      if (!parse_background_image_layer_value(trimmed, &layer)) {
        return false;
      }
      layers.push_back(std::move(layer));
    }

    style->background_layers = std::move(layers);
    hydrate_background_layer_metadata(style);
    sync_primary_background_layer(style);
    return true;
  }

  void assign_background_layer_metadata(const std::string& value,
                                        ComputedStyle* style,
                                        const char* kind) {
    if (!style || !kind) {
      return;
    }

    const std::string normalized = to_lower_copy(trim_copy(value));

    if (!style->background_layers.empty()) {
      apply_background_layer_metadata_list(normalized, &style->background_layers,
                                           kind);
      sync_primary_background_layer(style);
      return;
    }

    if (std::string(kind) == "position") {
      style->variables[background_position_var()] = normalized;
    } else if (std::string(kind) == "size") {
      style->variables[background_size_var()] = normalized;
    } else if (std::string(kind) == "repeat") {
      style->variables[background_repeat_var()] = normalized;
    } else if (std::string(kind) == "origin") {
      const auto items = split_top_level_csv(normalized);
      const std::string box =
          items.empty() ? normalized : transition_trim_copy(items.front());
      if (box == "padding-box") {
        style->background_origin = BackgroundClip::PaddingBox;
      } else if (box == "content-box") {
        style->background_origin = BackgroundClip::ContentBox;
      } else {
        style->background_origin = BackgroundClip::BorderBox;
      }
      style->variables[background_origin_var()] = normalized;
    } else if (std::string(kind) == "clip") {
      const auto items = split_top_level_csv(normalized);
      const std::string box =
          items.empty() ? normalized : transition_trim_copy(items.front());
      if (box == "padding-box") {
        style->background_clip = BackgroundClip::PaddingBox;
      } else if (box == "content-box") {
        style->background_clip = BackgroundClip::ContentBox;
      } else {
        style->background_clip = BackgroundClip::BorderBox;
      }
      style->variables[Symbol("--background-clip")] = normalized;
    }
  }

  void assign_background_position_axis_metadata(const std::string& value,
                                                ComputedStyle* style,
                                                bool horizontal) {
    if (!style) {
      return;
    }

    const std::string normalized = to_lower_copy(trim_copy(value));
    const auto items = split_top_level_csv(normalized);
    if (items.empty()) {
      return;
    }

    if (!style->background_layers.empty()) {
      for (size_t i = 0; i < style->background_layers.size(); ++i) {
        const std::string item =
            transition_trim_copy(items[std::min(i, items.size() - 1)]);
        auto& layer = style->background_layers[i];
        layer.position =
            merge_background_position_axis(layer.position, item, horizontal);
      }
      sync_primary_background_layer(style);
      return;
    }

    const Symbol axis_var =
        horizontal ? background_position_x_var() : background_position_y_var();
    const Symbol other_axis_var =
        horizontal ? background_position_y_var() : background_position_x_var();
    style->variables[axis_var] = normalized;

    const auto other_it = style->variables.find(other_axis_var);
    const bool has_other_axis =
        other_it != style->variables.end() && !other_it->second.empty();
    const auto other_items = has_other_axis
                                 ? split_top_level_csv(other_it->second)
                                 : std::vector<std::string>{};
    const auto current_items =
        split_top_level_csv(style->get_variable(background_position_var(), ""));

    std::vector<std::string> merged_items;
    const size_t count =
        std::max(items.size(), has_other_axis ? other_items.size()
                                              : std::max<size_t>(current_items.size(), 1));
    merged_items.reserve(count);
    for (size_t i = 0; i < count; ++i) {
      const std::string axis_item =
          transition_trim_copy(items[std::min(i, items.size() - 1)]);
      std::string base;
      if (has_other_axis) {
        const std::string other_item = transition_trim_copy(
            other_items[std::min(i, other_items.size() - 1)]);
        base = horizontal ? compose_background_position_axes(axis_item, other_item)
                          : compose_background_position_axes(other_item, axis_item);
      } else if (!current_items.empty()) {
        base = transition_trim_copy(
            current_items[std::min(i, current_items.size() - 1)]);
        base = merge_background_position_axis(base, axis_item, horizontal);
      } else {
        base = merge_background_position_axis("", axis_item, horizontal);
      }
      merged_items.push_back(base);
    }

    std::string joined;
    for (const auto& item : merged_items) {
      if (!joined.empty()) {
        joined += ", ";
      }
      joined += item;
    }
    style->variables[background_position_var()] = joined;
  }

  bool parse_background_shorthand(const std::string& value, ComputedStyle* style) {
    if (!style) {
      return false;
    }

    const auto items = split_top_level_csv(value);
    if (items.empty()) {
      return false;
    }

    ComputedStyle next = *style;
    next.has_gradient = false;
    next.background_color = Color(0.0f, 0.0f, 0.0f, 0.0f);
    next.background_clip = BackgroundClip::BorderBox;
    next.background_origin = BackgroundClip::PaddingBox;
    clear_background_gradient_metadata(&next);
    next.variables.erase(background_position_var());
    next.variables.erase(background_position_x_var());
    next.variables.erase(background_position_y_var());
    next.variables.erase(background_size_var());
    next.variables.erase(background_repeat_var());
    next.variables.erase(background_origin_var());
    next.variables.erase(Symbol("--background-clip"));

    std::vector<BackgroundImageLayer> layers;
    bool saw_supported_token = false;

    for (size_t item_index = 0; item_index < items.size(); ++item_index) {
      const auto tokens = split_background_shorthand_tokens(items[item_index]);
      if (tokens.empty()) {
        return false;
      }

      std::string image_value;
      std::string color_value;
      std::vector<std::string> position_tokens;
      std::vector<std::string> size_tokens;
      std::vector<std::string> repeat_tokens;
      std::vector<std::string> box_tokens;
      bool in_size_clause = false;
      bool saw_layer_token = false;

      for (const auto& raw_token : tokens) {
        const std::string token = transition_trim_copy(raw_token);
        const std::string lowered = to_lower_copy(token);
        if (lowered.empty()) {
          continue;
        }

        if (lowered == "/") {
          in_size_clause = true;
          continue;
        }

        if (image_value.empty() && is_background_image_token(lowered)) {
          image_value = token;
          saw_layer_token = true;
          continue;
        }

        if (is_background_repeat_token(lowered)) {
          repeat_tokens.push_back(lowered);
          saw_layer_token = true;
          continue;
        }

        if (is_background_box_token(lowered)) {
          box_tokens.push_back(lowered);
          saw_layer_token = true;
          continue;
        }

        if (color_value.empty() && is_color_value_token(lowered)) {
          color_value = token;
          saw_layer_token = true;
          continue;
        }

        if (!in_size_clause && is_background_position_token(lowered)) {
          position_tokens.push_back(lowered);
          saw_layer_token = true;
          continue;
        }

        if (in_size_clause && is_background_size_token(lowered)) {
          size_tokens.push_back(lowered);
          saw_layer_token = true;
          continue;
        }

        return false;
      }

      if (!saw_layer_token) {
        return false;
      }
      if (box_tokens.size() > 2) {
        return false;
      }

      if (!color_value.empty()) {
        if (item_index + 1 != items.size()) {
          return false;
        }
        next.background_color =
            parse_style_color(color_value, &next, next.background_color);
      }

      std::string layer_origin;
      std::string layer_clip;
      if (!box_tokens.empty()) {
        layer_origin = box_tokens[0];
        layer_clip = box_tokens.size() >= 2 ? box_tokens[1] : box_tokens[0];

        if (layer_origin == "content-box") {
          next.background_origin = BackgroundClip::ContentBox;
        } else if (layer_origin == "border-box") {
          next.background_origin = BackgroundClip::BorderBox;
        } else {
          next.background_origin = BackgroundClip::PaddingBox;
        }

        if (layer_clip == "content-box") {
          next.background_clip = BackgroundClip::ContentBox;
        } else if (layer_clip == "padding-box") {
          next.background_clip = BackgroundClip::PaddingBox;
        } else {
          next.background_clip = BackgroundClip::BorderBox;
        }
        next.variables[background_origin_var()] = layer_origin;
        next.variables[Symbol("--background-clip")] = layer_clip;
      }

      if (!image_value.empty() && to_lower_copy(image_value) != "none") {
        BackgroundImageLayer layer;
        if (!parse_background_image_layer_value(image_value, &layer)) {
          return false;
        }
        layer.position = join_space_separated_tokens(position_tokens);
        layer.size = join_space_separated_tokens(size_tokens);
        layer.repeat = join_space_separated_tokens(repeat_tokens);
        layer.origin = layer_origin;
        layer.clip = layer_clip;
        layers.push_back(std::move(layer));
      }

      saw_supported_token = true;
    }

    if (!saw_supported_token) {
      return false;
    }

    next.background_layers = std::move(layers);
    sync_primary_background_layer(&next);
    *style = std::move(next);
    return true;
  }

  void clear_drop_shadow_filter_vars(ComputedStyle* style) {
    if (!style) {
      return;
    }
    style->variables.erase(filter_drop_shadow_offset_x_var());
    style->variables.erase(filter_drop_shadow_offset_y_var());
    style->variables.erase(filter_drop_shadow_blur_var());
    style->variables.erase(filter_drop_shadow_color_var());
  }

  void clear_filter_effect_vars(ComputedStyle* style) {
    if (!style) {
      return;
    }
    style->variables.erase(Symbol("--filter-blur"));
    style->variables.erase(filter_opacity_var());
    clear_drop_shadow_filter_vars(style);
  }

  bool parse_opacity_filter(const std::string& token, ComputedStyle* style) {
    if (!style) {
      return false;
    }

    const std::string lowered = to_lower_copy(transition_trim_copy(token));
    static const std::string prefix = "opacity(";
    if (lowered.rfind(prefix, 0) != 0 || lowered.back() != ')') {
      return false;
    }

    std::string inner =
        transition_trim_copy(token.substr(prefix.size(), token.size() - prefix.size() - 1));
    if (inner.empty()) {
      return false;
    }

    float alpha = 1.0f;
    const std::string lowered_inner = to_lower_copy(inner);
    try {
      if (!lowered_inner.empty() && lowered_inner.back() == '%') {
        alpha = std::stof(lowered_inner.substr(0, lowered_inner.size() - 1)) / 100.0f;
      } else {
        alpha = std::stof(lowered_inner);
      }
    } catch (...) {
      return false;
    }
    alpha = std::clamp(alpha, 0.0f, 1.0f);

    std::ostringstream out;
    out << alpha;
    style->variables[filter_opacity_var()] = out.str();
    return true;
  }

  bool parse_drop_shadow_filter(const std::string& token, ComputedStyle* style) {
    if (!style) {
      return false;
    }

    const std::string lowered = to_lower_copy(transition_trim_copy(token));
    static const std::string prefix = "drop-shadow(";
    if (lowered.rfind(prefix, 0) != 0 || lowered.back() != ')') {
      return false;
    }

    const std::string inner =
        transition_trim_copy(token.substr(prefix.size(), token.size() - prefix.size() - 1));
    const auto parts = split_css_tokens(inner);
    if (parts.size() < 2) {
      return false;
    }

    std::vector<std::string> lengths;
    lengths.reserve(3);
    Color color{0.0f, 0.0f, 0.0f, 1.0f};
    bool has_color = false;
    for (const auto& part : parts) {
      const std::string trimmed = transition_trim_copy(part);
      const std::string lowered_part = to_lower_copy(trimmed);
      if (is_color_value_token(lowered_part)) {
        if (has_color) {
          return false;
        }
        color = parse_style_color(trimmed, style, color);
        has_color = true;
        continue;
      }
      try {
        parse_length(lowered_part);
      } catch (...) {
        return false;
      }
      lengths.push_back(lowered_part);
    }

    if (lengths.size() < 2 || lengths.size() > 3) {
      return false;
    }

    style->variables[filter_drop_shadow_offset_x_var()] = lengths[0];
    style->variables[filter_drop_shadow_offset_y_var()] = lengths[1];
    style->variables[filter_drop_shadow_blur_var()] =
        lengths.size() >= 3 ? lengths[2] : "0px";
    style->variables[filter_drop_shadow_color_var()] =
        color_to_css_variable_value(has_color ? color
                                              : Color{0.0f, 0.0f, 0.0f, 1.0f});
    return true;
  }

  void parse_filter_effect(const std::string& value, ComputedStyle* style,
                           Symbol blur_variable_name, bool allow_drop_shadow) {
    if (!style) {
      return;
    }

    const std::string trimmed = to_lower_copy(transition_trim_copy(value));
    if (trimmed.empty() || trimmed == "none") {
      style->variables.erase(blur_variable_name);
      if (allow_drop_shadow) {
        clear_filter_effect_vars(style);
      }
      return;
    }

    style->variables.erase(blur_variable_name);
    if (allow_drop_shadow) {
      clear_drop_shadow_filter_vars(style);
      style->variables.erase(filter_opacity_var());
    }

    for (const auto& token : split_css_tokens(value)) {
      const std::string lowered_token = to_lower_copy(transition_trim_copy(token));
      if (lowered_token.rfind("blur(", 0) == 0 && lowered_token.back() == ')') {
        const std::string radius =
            transition_trim_copy(token.substr(5, token.size() - 6));
        if (!radius.empty()) {
          style->variables[blur_variable_name] = radius;
        }
        continue;
      }
      if (allow_drop_shadow) {
        parse_opacity_filter(token, style);
        parse_drop_shadow_filter(token, style);
      }
    }
  }

  bool parse_gradient_stops(const std::vector<std::string>& items, size_t stop_start,
                            ComputedStyle* style, LinearGradient* gradient) {
    if (!gradient) {
      return false;
    }

    const size_t stop_count = items.size() - stop_start;
    if (stop_count < 2 || stop_count > 8) {
      return false;
    }

    std::vector<bool> explicit_offsets(stop_count, false);
    std::vector<float> offsets(stop_count, NAN);

    for (size_t i = 0; i < stop_count; ++i) {
      const auto stop_tokens = split_css_tokens(items[stop_start + i]);
      if (stop_tokens.empty()) {
        return false;
      }

      std::string color_token = stop_tokens.front();
      if (stop_tokens.size() > 1) {
        const std::string maybe_offset = transition_trim_copy(stop_tokens.back());
        if (maybe_offset.empty() || maybe_offset.back() != '%') {
          return false;
        }
        try {
          offsets[i] = std::clamp(
              std::stof(maybe_offset.substr(0, maybe_offset.size() - 1)) / 100.0f,
              0.0f, 1.0f);
        } catch (...) {
          return false;
        }
        explicit_offsets[i] = true;

        color_token.clear();
        for (size_t token_index = 0; token_index + 1 < stop_tokens.size();
             ++token_index) {
          if (!color_token.empty()) {
            color_token.push_back(' ');
          }
          color_token += stop_tokens[token_index];
        }
      }

      gradient->stops[i].color =
          parse_style_color(color_token, style, style ? style->text_color
                                                      : Color{0.0f, 0.0f, 0.0f, 1.0f});
    }

    if (!explicit_offsets.front()) {
      offsets.front() = 0.0f;
    }
    if (!explicit_offsets.back()) {
      offsets.back() = 1.0f;
    }

    size_t index = 0;
    while (index < stop_count) {
      if (explicit_offsets[index] || index == 0 || index + 1 == stop_count) {
        ++index;
        continue;
      }

      size_t run_end = index;
      while (run_end < stop_count - 1 && !explicit_offsets[run_end]) {
        ++run_end;
      }

      const float start_offset = offsets[index - 1];
      const float end_offset = offsets[run_end];
      const size_t gap = run_end - index + 1;
      for (size_t step = 0; step < gap; ++step) {
        offsets[index + step] =
            start_offset + (end_offset - start_offset) *
                               static_cast<float>(step + 1) /
                               static_cast<float>(gap + 1);
      }
      index = run_end + 1;
    }

    for (size_t i = 0; i < stop_count; ++i) {
      gradient->stops[i].offset = offsets[i];
    }

    gradient->stop_count = static_cast<int>(stop_count);
    return true;
  }

  bool parse_linear_gradient(const std::string& value, ComputedStyle* style) {
    if (!style) {
      return false;
    }

    const std::string trimmed = transition_trim_copy(value);
    const std::string lowered = to_lower_copy(trimmed);
    static const std::string prefix = "linear-gradient(";
    if (lowered.rfind(prefix, 0) != 0 || trimmed.empty() || trimmed.back() != ')') {
      return false;
    }

    const std::string inner =
        trimmed.substr(prefix.size(), trimmed.size() - prefix.size() - 1);
    const auto items = split_top_level_csv(inner);
    if (items.size() < 2) {
      return false;
    }

    LinearGradient gradient;
    gradient.angle = 180.0f;
    size_t stop_start = 0;

    const std::string first = transition_trim_copy(items.front());
    const std::string first_lower = to_lower_copy(first);
    if (first_lower.rfind("to ", 0) == 0) {
      bool to_top = false;
      bool to_bottom = false;
      bool to_left = false;
      bool to_right = false;
      for (const auto& token : split_css_tokens(first_lower.substr(3))) {
        if (token == "top") to_top = true;
        else if (token == "bottom") to_bottom = true;
        else if (token == "left") to_left = true;
        else if (token == "right") to_right = true;
      }
      gradient.angle =
          gradient_direction_to_angle(to_top, to_bottom, to_left, to_right);
      stop_start = 1;
    } else if (ends_with_copy(first_lower, "deg")) {
      try {
        gradient.angle =
            std::stof(first_lower.substr(0, first_lower.size() - 3));
        stop_start = 1;
      } catch (...) {
        return false;
      }
    }

    if (!parse_gradient_stops(items, stop_start, style, &gradient)) {
      return false;
    }

    style->has_gradient = true;
    style->gradient = gradient;
    style->variables[background_gradient_type_var()] = "linear";
    style->variables.erase(background_radial_position_var());
    style->variables.erase(background_radial_size_var());
    return true;
  }

  bool parse_radial_gradient(const std::string& value, ComputedStyle* style) {
    if (!style) {
      return false;
    }

    const std::string trimmed = transition_trim_copy(value);
    const std::string lowered = to_lower_copy(trimmed);
    static const std::string prefix = "radial-gradient(";
    if (lowered.rfind(prefix, 0) != 0 || trimmed.empty() || trimmed.back() != ')') {
      return false;
    }

    const std::string inner =
        trimmed.substr(prefix.size(), trimmed.size() - prefix.size() - 1);
    const auto items = split_top_level_csv(inner);
    if (items.size() < 2) {
      return false;
    }

    LinearGradient gradient;
    size_t stop_start = 0;
    std::string radial_position = "center";
    std::string radial_size = "farthest-corner";

    const std::string first = transition_trim_copy(items.front());
    const std::string first_lower = to_lower_copy(first);
    const bool has_descriptor =
        first_lower.rfind("at ", 0) == 0 ||
        first_lower.find(" at ") != std::string::npos ||
        first_lower == "circle" || first_lower == "ellipse" ||
        first_lower == "closest-side" || first_lower == "closest-corner" ||
        first_lower == "farthest-side" || first_lower == "farthest-corner";
    if (has_descriptor) {
      stop_start = 1;

      std::string descriptor = first_lower;
      if (descriptor.rfind("at ", 0) == 0) {
        radial_position = transition_trim_copy(descriptor.substr(3));
        descriptor.clear();
      } else {
        const size_t at_pos = descriptor.find(" at ");
        if (at_pos != std::string::npos) {
          radial_position = transition_trim_copy(descriptor.substr(at_pos + 4));
          descriptor = transition_trim_copy(descriptor.substr(0, at_pos));
        }
      }

      radial_size.clear();
      const auto descriptor_tokens = split_css_tokens(descriptor);
      for (const auto& token : descriptor_tokens) {
        if (token == "circle" || token == "ellipse") {
          continue;
        }
        if (!radial_size.empty()) {
          radial_size.push_back(' ');
        }
        radial_size += token;
      }
      radial_size = transition_trim_copy(radial_size);
      if (radial_size.empty()) {
        radial_size = "farthest-corner";
      }
    }

    if (!parse_gradient_stops(items, stop_start, style, &gradient)) {
      return false;
    }

    style->has_gradient = true;
    style->gradient = gradient;
    style->variables[background_gradient_type_var()] = "radial";
    style->variables[background_radial_position_var()] = radial_position;
    style->variables[background_radial_size_var()] = radial_size;
    return true;
  }

  void parse_box_shadow(const std::string &value, ComputedStyle *style) {
    if (!style) {
      return;
    }

    if (value == "none" || value.empty()) {
      style->has_shadow = false;
      style->shadow = BoxShadow{};
      style->shadows.clear();
      return;
    }

    auto parse_shadow_layer = [&](const std::string& layer,
                                  BoxShadow& shadow) -> bool {
      shadow = BoxShadow{};
      const auto parts = split_css_tokens(layer);
      if (parts.empty()) {
        return false;
      }

      std::vector<float> lengths;
      lengths.reserve(4);
      for (const auto& raw_part : parts) {
        const std::string part = trim_copy(raw_part);
        if (part.empty()) {
          continue;
        }

        const std::string lowered = to_lower_copy(part);
        if (lowered == "inset") {
          shadow.inset = true;
          continue;
        }
        if (is_color_value_token(lowered)) {
          shadow.color = parse_style_color(part, style, shadow.color);
          continue;
        }

        try {
          lengths.push_back(parse_length(part));
        } catch (...) {
          return false;
        }
      }

      if (lengths.size() < 2) {
        return false;
      }
      shadow.offset_x = lengths[0];
      shadow.offset_y = lengths[1];
      if (lengths.size() >= 3) {
        shadow.blur_radius = lengths[2];
      }
      if (lengths.size() >= 4) {
        shadow.spread_radius = lengths[3];
      }
      return true;
    };

    style->shadows.clear();
    for (const auto& item : split_top_level_csv(value)) {
      BoxShadow shadow;
      if (parse_shadow_layer(item, shadow)) {
        style->shadows.push_back(shadow);
      }
    }

    style->has_shadow = !style->shadows.empty();
    style->shadow = style->has_shadow ? style->shadows.front() : BoxShadow{};
  }

  void parse_text_shadow(const std::string& value, ComputedStyle* style) {
    if (!style) {
      return;
    }

    const std::string lowered_value = to_lower_copy(trim_copy(value));
    if (lowered_value == "none" || lowered_value.empty()) {
      style->has_text_shadow = false;
      style->text_shadow = TextShadow{};
      style->text_shadows.clear();
      return;
    }

    auto parse_shadow_layer = [&](const std::string& layer,
                                  TextShadow& shadow) -> bool {
      shadow = TextShadow{};
      shadow.color = style->text_color;
      const auto parts = split_css_tokens(layer);
      if (parts.empty()) {
        return false;
      }

      std::vector<float> lengths;
      lengths.reserve(3);
      bool saw_color = false;
      for (const auto& raw_part : parts) {
        const std::string part = trim_copy(raw_part);
        if (part.empty()) {
          continue;
        }

        const std::string lowered = to_lower_copy(part);
        if (is_color_value_token(lowered)) {
          if (saw_color) {
            return false;
          }
          shadow.color = parse_style_color(part, style, style->text_color);
          saw_color = true;
          continue;
        }

        if (lengths.size() >= 3) {
          return false;
        }
        try {
          lengths.push_back(parse_length(part));
        } catch (...) {
          return false;
        }
      }

      if (lengths.size() < 2 || lengths.size() > 3) {
        return false;
      }
      shadow.offset_x = lengths[0];
      shadow.offset_y = lengths[1];
      if (lengths.size() >= 3) {
        shadow.blur_radius = lengths[2];
      }
      return true;
    };

    style->text_shadows.clear();
    for (const auto& item : split_top_level_csv(value)) {
      TextShadow shadow;
      if (parse_shadow_layer(item, shadow)) {
        style->text_shadows.push_back(shadow);
      }
    }

    style->has_text_shadow = !style->text_shadows.empty();
    style->text_shadow =
        style->has_text_shadow ? style->text_shadows.front() : TextShadow{};
  }

  void parse_transform(const std::string& value, ComputedStyle* style) {
    style->transform_x = 0.0f;
    style->transform_y = 0.0f;
    style->transform_scale = 1.0f;
    style->transform_scale_x = 1.0f;
    style->transform_scale_y = 1.0f;
    style->transform_rotate = 0.0f;
    style->has_transform_matrix = false;
    style->transform_matrix = flex::Transform{};

    size_t i = 0;
    while (i < value.size()) {
      i = skip_selector_ws(value, i);
      if (i >= value.size()) {
        break;
      }

      const std::string function = parse_identifier_token(value, i);
      i = skip_selector_ws(value, i);
      if (function.empty() || i >= value.size() || value[i] != '(') {
        ++i;
        continue;
      }

      const std::string inner = parse_balanced_token(value, i, '(', ')');
      const auto args = split_transform_arguments(inner);
      if (args.empty()) {
        continue;
      }

      const std::string function_name = to_lower_copy(function);
      if (function_name == "translate") {
        style->transform_x = parse_length(args[0]);
        style->transform_y = args.size() > 1 ? parse_length(args[1]) : 0.0f;
      } else if (function_name == "translate3d") {
        style->transform_x = parse_length(args[0]);
        style->transform_y = args.size() > 1 ? parse_length(args[1]) : 0.0f;
      } else if (function_name == "translatex") {
        style->transform_x = parse_length(args[0]);
      } else if (function_name == "translatey") {
        style->transform_y = parse_length(args[0]);
      } else if (function_name == "scale") {
        const float sx = std::stof(args[0]);
        const float sy = args.size() > 1 ? std::stof(args[1]) : sx;
        style->transform_scale = sx == sy ? sx : 1.0f;
        style->transform_scale_x = sx;
        style->transform_scale_y = sy;
      } else if (function_name == "scale3d") {
        const float sx = std::stof(args[0]);
        const float sy = args.size() > 1 ? std::stof(args[1]) : sx;
        style->transform_scale = sx == sy ? sx : 1.0f;
        style->transform_scale_x = sx;
        style->transform_scale_y = sy;
      } else if (function_name == "scalex") {
        style->transform_scale_x = std::stof(args[0]);
      } else if (function_name == "scaley") {
        style->transform_scale_y = std::stof(args[0]);
      } else if (function_name == "rotate" || function_name == "rotatez") {
        style->transform_rotate = parse_angle_degrees(args[0]);
      } else if (function_name == "matrix" && args.size() == 6) {
        flex::Transform matrix;
        matrix.data[0] = std::stof(args[0]);
        matrix.data[1] = std::stof(args[2]);
        matrix.data[2] = parse_length(args[4]);
        matrix.data[3] = std::stof(args[1]);
        matrix.data[4] = std::stof(args[3]);
        matrix.data[5] = parse_length(args[5]);
        append_transform_matrix(style, matrix);
      } else if (function_name == "skewx") {
        append_transform_matrix(style, make_skew_transform(args[0], ""));
      } else if (function_name == "skewy") {
        append_transform_matrix(style, make_skew_transform("", args[0]));
      } else if (function_name == "skew") {
        append_transform_matrix(
            style,
            make_skew_transform(args[0], args.size() > 1 ? args[1] : ""));
      }
    }

    sync_uniform_transform_scale(style);
  }

  flex::Transform make_skew_transform(const std::string& x_angle,
                                      const std::string& y_angle) {
    constexpr float kPi = 3.14159265358979323846f;
    flex::Transform transform;
    if (!trim_copy(x_angle).empty()) {
      transform.data[1] =
          std::tan(parse_angle_degrees(x_angle) * kPi / 180.0f);
    }
    if (!trim_copy(y_angle).empty()) {
      transform.data[3] =
          std::tan(parse_angle_degrees(y_angle) * kPi / 180.0f);
    }
    return transform;
  }

  void append_transform_matrix(ComputedStyle* style,
                               const flex::Transform& transform) {
    using flex::operator*;
    style->transform_matrix =
        style->has_transform_matrix ? style->transform_matrix * transform
                                    : transform;
    style->has_transform_matrix = true;
  }

  float parse_angle_degrees(std::string token) {
    token = to_lower_copy(trim_copy(token));
    if (token.size() >= 3 && token.compare(token.size() - 3, 3, "deg") == 0) {
      return std::stof(token.substr(0, token.size() - 3));
    }
    if (token.size() >= 3 && token.compare(token.size() - 3, 3, "rad") == 0) {
      return std::stof(token.substr(0, token.size() - 3)) *
             180.0f / 3.14159265f;
    }
    if (token.size() >= 4 &&
        token.compare(token.size() - 4, 4, "turn") == 0) {
      return std::stof(token.substr(0, token.size() - 4)) * 360.0f;
    }
    if (token.size() >= 4 &&
        token.compare(token.size() - 4, 4, "grad") == 0) {
      return std::stof(token.substr(0, token.size() - 4)) * 0.9f;
    }
    return std::stof(token);
  }

  void sync_uniform_transform_scale(ComputedStyle* style) {
    if (style->transform_scale == 1.0f &&
        std::abs(style->transform_scale_x - style->transform_scale_y) <
            0.0001f) {
      style->transform_scale = style->transform_scale_x;
    }
  }

  void parse_individual_translate(const std::string& value,
                                  ComputedStyle* style) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered.empty() || lowered == "none") {
      style->transform_x = 0.0f;
      style->transform_y = 0.0f;
      return;
    }

    const auto args = split_transform_arguments(value);
    if (args.empty()) {
      return;
    }
    style->transform_x = parse_length(args[0]);
    style->transform_y = args.size() > 1 ? parse_length(args[1]) : 0.0f;
  }

  void parse_individual_scale(const std::string& value, ComputedStyle* style) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered.empty() || lowered == "none") {
      style->transform_scale = 1.0f;
      style->transform_scale_x = 1.0f;
      style->transform_scale_y = 1.0f;
      return;
    }

    const auto args = split_transform_arguments(value);
    if (args.empty()) {
      return;
    }
    const float sx = std::stof(args[0]);
    const float sy = args.size() > 1 ? std::stof(args[1]) : sx;
    style->transform_scale = sx == sy ? sx : 1.0f;
    style->transform_scale_x = sx;
    style->transform_scale_y = sy;
    sync_uniform_transform_scale(style);
  }

  void parse_individual_rotate(const std::string& value, ComputedStyle* style) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered.empty() || lowered == "none") {
      style->transform_rotate = 0.0f;
      return;
    }

    const auto args = split_transform_arguments(value);
    if (args.empty()) {
      return;
    }
    style->transform_rotate = parse_angle_degrees(args.back());
  }

  void parse_transform_origin(const std::string& value, ComputedStyle* style) {
    style->transform_origin_x = 0.5f;
    style->transform_origin_y = 0.5f;
    style->transform_origin_x_percent = true;
    style->transform_origin_y_percent = true;

    const auto tokens = split_css_tokens(value);
    if (tokens.empty()) {
      return;
    }

    const auto apply_component = [this](const std::string& token, bool horizontal,
                                        float& out_value, bool& out_percent) -> bool {
      const std::string lowered = to_lower_copy(trim_copy(token));
      if (lowered == "center") {
        out_value = 0.5f;
        out_percent = true;
        return true;
      }
      if (horizontal) {
        if (lowered == "left") {
          out_value = 0.0f;
          out_percent = true;
          return true;
        }
        if (lowered == "right") {
          out_value = 1.0f;
          out_percent = true;
          return true;
        }
      } else {
        if (lowered == "top") {
          out_value = 0.0f;
          out_percent = true;
          return true;
        }
        if (lowered == "bottom") {
          out_value = 1.0f;
          out_percent = true;
          return true;
        }
      }
      if (!lowered.empty() && lowered.back() == '%') {
        out_value =
            std::clamp(std::stof(lowered.substr(0, lowered.size() - 1)) / 100.0f,
                       0.0f, 1.0f);
        out_percent = true;
        return true;
      }
      out_value = parse_length(lowered);
      out_percent = false;
      return true;
    };

    if (tokens.size() == 1) {
      const std::string lowered = to_lower_copy(trim_copy(tokens[0]));
      if (lowered == "top" || lowered == "bottom") {
        apply_component(tokens[0], false, style->transform_origin_y,
                        style->transform_origin_y_percent);
      } else {
        apply_component(tokens[0], true, style->transform_origin_x,
                        style->transform_origin_x_percent);
      }
      return;
    }

    apply_component(tokens[0], true, style->transform_origin_x,
                    style->transform_origin_x_percent);
    apply_component(tokens[1], false, style->transform_origin_y,
                    style->transform_origin_y_percent);
  }

  void parse_outline(const std::string& value, ComputedStyle* style) {
    if (value.empty() || value == "none") {
      style->outline_width = 0.0f;
      style->outline_offset = 0.0f;
      style->outline_color = Color(0.0f, 0.0f, 0.0f, 0.0f);
      style->outline_style = BorderStyle::None;
      return;
    }

    for (const auto& token : split_css_tokens(value)) {
      if (is_border_style_token(token)) {
        style->outline_style = parse_border_style_token(token);
        continue;
      }
      if (is_color_value_token(token)) {
        style->outline_color =
            parse_style_color(token, style, style->outline_color);
      } else {
        style->outline_width =
            parse_border_width_token(to_lower_copy(trim_copy(token)));
      }
    }
  }

  void parse_outline_style(const std::string& value, ComputedStyle* style) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered.empty()) {
      return;
    }
    style->outline_style = parse_border_style_token(lowered);
    if (lowered == "none" || lowered == "hidden") {
      style->outline_width = 0.0f;
      style->outline_offset = 0.0f;
      style->outline_color = Color(0.0f, 0.0f, 0.0f, 0.0f);
    }
  }

  void parse_ring(const std::string& value, ComputedStyle* style) {
    if (value.empty() || value == "none") {
      style->ring_width = 0.0f;
      style->ring_offset = 0.0f;
      style->ring_color = Color(0.0f, 0.0f, 0.0f, 0.0f);
      return;
    }

    bool width_set = false;
    for (const auto& token : split_css_tokens(value)) {
      if (is_color_value_token(token)) {
        style->ring_color = parse_style_color(token, style, style->ring_color);
      } else if (!width_set) {
        style->ring_width = parse_length(token);
        width_set = true;
      } else {
        style->ring_offset = parse_length(token);
      }
    }
  }

  bool is_text_decoration_line_token(const std::string& token) const {
    return token == "none" || token == "underline" || token == "line-through" ||
           token == "overline";
  }

  bool is_text_decoration_thickness_token(const std::string& token) const {
    if (token == "auto" || token == "from-font") {
      return true;
    }
    if (!token.empty() &&
        (std::isdigit(static_cast<unsigned char>(token.front())) ||
         token.front() == '-' || token.front() == '.')) {
      return true;
    }
    return false;
  }

  bool is_text_decoration_style_token(const std::string& token) const {
    return token == "solid" || token == "dashed" || token == "dotted" ||
           token == "double" || token == "wavy";
  }

  void assign_text_decoration_line(const std::string& value, ComputedStyle* style) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered.empty()) {
      return;
    }
    if (lowered == "none") {
      style->variables[Symbol("--text-decoration")] = "none";
      return;
    }

    std::vector<std::string> lines;
    for (const auto& token : split_css_tokens(lowered)) {
      if (!is_text_decoration_line_token(token) || token == "none") {
        continue;
      }
      if (std::find(lines.begin(), lines.end(), token) == lines.end()) {
        lines.push_back(token);
      }
    }
    style->variables[Symbol("--text-decoration")] =
        lines.empty() ? "none" : join_space_separated_tokens(lines);
  }

  void parse_text_decoration(const std::string& value, ComputedStyle* style) {
    const std::string lowered = to_lower_copy(trim_copy(value));
    if (lowered.empty()) {
      return;
    }
    if (lowered == "none") {
      style->variables[Symbol("--text-decoration")] = "none";
      style->variables.erase(Symbol("--text-decoration-color"));
      style->variables.erase(Symbol("--text-decoration-style"));
      style->variables.erase(Symbol("--text-decoration-thickness"));
      return;
    }

    std::vector<std::string> lines;
    std::string color_value;
    std::string style_value;
    std::string thickness_value;
    for (const auto& token : split_css_tokens(lowered)) {
      if (is_text_decoration_line_token(token)) {
        if (token != "none" &&
            std::find(lines.begin(), lines.end(), token) == lines.end()) {
          lines.push_back(token);
        }
        continue;
      }
      if (color_value.empty() && is_color_value_token(token)) {
        color_value = color_to_css_variable_value(parse_style_color(token, style));
        continue;
      }
      if (style_value.empty() && is_text_decoration_style_token(token)) {
        style_value = token;
        continue;
      }
      if (thickness_value.empty() && is_text_decoration_thickness_token(token)) {
        thickness_value = token;
      }
    }

    style->variables[Symbol("--text-decoration")] =
        lines.empty() ? "none" : join_space_separated_tokens(lines);
    if (!color_value.empty()) {
      style->variables[Symbol("--text-decoration-color")] = color_value;
    }
    if (!style_value.empty()) {
      style->variables[Symbol("--text-decoration-style")] = style_value;
    }
    if (!thickness_value.empty()) {
      style->variables[Symbol("--text-decoration-thickness")] = thickness_value;
    }
  }

  bool is_border_style_token(const std::string& token) const {
    return token == "none" || token == "hidden" || token == "solid" ||
           token == "dashed" || token == "dotted" || token == "double" ||
           token == "groove" || token == "ridge" || token == "inset" ||
           token == "outset";
  }

  bool is_border_style_hidden(const std::string& token) const {
    return token == "none" || token == "hidden";
  }

  float parse_border_width_token(const std::string& token) {
    if (token == "thin") return 1.0f;
    if (token == "medium") return 3.0f;
    if (token == "thick") return 5.0f;
    return parse_length(token);
  }

  BorderStyle parse_border_style_token(const std::string& token) {
    if (token == "none" || token == "hidden") return BorderStyle::None;
    if (token == "dashed") return BorderStyle::Dashed;
    if (token == "dotted") return BorderStyle::Dotted;
    if (token == "double") return BorderStyle::Double;
    if (token == "groove") return BorderStyle::Groove;
    if (token == "ridge") return BorderStyle::Ridge;
    if (token == "inset") return BorderStyle::Inset;
    if (token == "outset") return BorderStyle::Outset;
    return BorderStyle::Solid;
  }

  void set_border_width_for_side(float widths[4], int side, float value) {
    if (side >= 0 && side < 4) {
      widths[side] = value;
      return;
    }

    widths[0] = widths[1] = widths[2] = widths[3] = value;
  }

  void set_border_style_for_side(BorderStyle styles[4], int side,
                                 BorderStyle value) {
    if (side >= 0 && side < 4) {
      styles[side] = value;
      return;
    }

    styles[0] = styles[1] = styles[2] = styles[3] = value;
  }

  void set_border_color_for_side(ComputedStyle* style, int side,
                                 const Color& value) {
    if (!style) {
      return;
    }

    style->has_border_side_colors = true;
    if (side >= 0 && side < 4) {
      style->border_color = value;
      style->border_colors[side] = value;
      return;
    }

    style->border_color = value;
    style->border_colors[0] = value;
    style->border_colors[1] = value;
    style->border_colors[2] = value;
    style->border_colors[3] = value;
  }

  void parse_border_color_list(const std::string& value, ComputedStyle* style) {
    if (!style) {
      return;
    }

    // flexUI custom properties historically serialize a single RGBA color as
    // four comma-separated channels. Treat that form as one color before
    // applying the standard one-to-four border-color list grammar.
    if (split_top_level_csv(value).size() == 4) {
      const Color color = parse_style_color(value, style, style->border_color);
      style->has_border_side_colors = true;
      style->border_color = color;
      for (Color& side_color : style->border_colors) {
        side_color = color;
      }
      return;
    }

    const auto tokens = split_css_tokens(value);
    if (tokens.empty() || tokens.size() > 4) {
      return;
    }
    for (const auto& token : tokens) {
      if (!is_color_value_token(token)) {
        return;
      }
    }

    Color colors[4];
    colors[0] = parse_style_color(tokens[0], style, style->border_color);
    if (tokens.size() == 1) {
      colors[1] = colors[2] = colors[3] = colors[0];
    } else if (tokens.size() == 2) {
      colors[1] = parse_style_color(tokens[1], style, style->border_color);
      colors[2] = colors[0];
      colors[3] = colors[1];
    } else if (tokens.size() == 3) {
      colors[1] = parse_style_color(tokens[1], style, style->border_color);
      colors[2] = parse_style_color(tokens[2], style, style->border_color);
      colors[3] = colors[1];
    } else {
      colors[1] = parse_style_color(tokens[1], style, style->border_color);
      colors[2] = parse_style_color(tokens[2], style, style->border_color);
      colors[3] = parse_style_color(tokens[3], style, style->border_color);
    }

    style->has_border_side_colors = true;
    style->border_color = colors[0];
    for (size_t i = 0; i < 4; ++i) {
      style->border_colors[i] = colors[i];
    }
  }

  template <typename Fn>
  void apply_border_to_sides(std::initializer_list<int> sides, Fn&& fn) {
    for (const int side : sides) {
      fn(side);
    }
  }

  void parse_logical_border_width_pair(const std::string& value,
                                       ComputedStyle* style, int start_side,
                                       int end_side) {
    if (!style) {
      return;
    }
    const auto values = split_css_tokens(value);
    if (values.empty()) {
      return;
    }
    set_border_width_for_side(
        style->border_width, start_side,
        parse_border_width_token(to_lower_copy(trim_copy(values[0]))));
    set_border_width_for_side(
        style->border_width, end_side,
        parse_border_width_token(to_lower_copy(trim_copy(
            values.size() > 1 ? values[1] : values[0]))));
  }

  void parse_logical_border_style_single(const std::string& value,
                                         ComputedStyle* style, int side) {
    if (!style) {
      return;
    }
    const std::string lowered = to_lower_copy(trim_copy(value));
    set_border_style_for_side(style->border_style, side,
                              parse_border_style_token(lowered));
    if (is_border_style_hidden(lowered)) {
      set_border_width_for_side(style->border_width, side, 0.0f);
    }
  }

  void parse_logical_border_style_pair(const std::string& value,
                                       ComputedStyle* style, int start_side,
                                       int end_side) {
    const auto values = split_css_tokens(value);
    if (values.empty()) {
      return;
    }
    parse_logical_border_style_single(values[0], style, start_side);
    parse_logical_border_style_single(values.size() > 1 ? values[1] : values[0],
                                      style, end_side);
  }

  void parse_logical_border_color_pair(const std::string& value,
                                       ComputedStyle* style, int start_side,
                                       int end_side) {
    if (!style) {
      return;
    }
    const auto values = split_css_tokens(value);
    if (values.empty()) {
      return;
    }
    set_border_color_for_side(
        style, start_side,
        parse_style_color(values[0], style, style->border_color));
    set_border_color_for_side(
        style, end_side,
        parse_style_color(values.size() > 1 ? values[1] : values[0], style,
                          style->border_color));
  }

  bool apply_logical_size_property(const std::string& property,
                                   const std::string& value,
                                   ComputedStyle* style) {
    if (!style) {
      return false;
    }

    if (property == "inline-size") {
      assign_size(value, style->width_size, style->width,
                  style->width_is_percent);
      return true;
    }
    if (property == "block-size") {
      assign_size(value, style->height_size, style->height,
                  style->height_is_percent);
      return true;
    }
    if (property == "min-inline-size") {
      style->variables[Symbol("min-width")] = value;
      return true;
    }
    if (property == "max-inline-size") {
      style->variables[Symbol("max-width")] = value;
      return true;
    }
    if (property == "min-block-size") {
      style->variables[Symbol("min-height")] = value;
      return true;
    }
    if (property == "max-block-size") {
      style->variables[Symbol("max-height")] = value;
      return true;
    }

    return false;
  }

  bool apply_logical_border_property(const std::string& property,
                                     const std::string& value,
                                     ComputedStyle* style) {
    if (!style) {
      return false;
    }

    const int inline_start = style->direction == Direction::Rtl ? 1 : 3;
    const int inline_end = style->direction == Direction::Rtl ? 3 : 1;

    if (property == "border-inline-width") {
      parse_logical_border_width_pair(value, style, inline_start, inline_end);
      return true;
    }
    if (property == "border-inline-start-width") {
      set_border_width_for_side(
          style->border_width, inline_start,
          parse_border_width_token(to_lower_copy(trim_copy(value))));
      return true;
    }
    if (property == "border-inline-end-width") {
      set_border_width_for_side(
          style->border_width, inline_end,
          parse_border_width_token(to_lower_copy(trim_copy(value))));
      return true;
    }
    if (property == "border-block-width") {
      parse_logical_border_width_pair(value, style, 0, 2);
      return true;
    }
    if (property == "border-block-start-width") {
      set_border_width_for_side(
          style->border_width, 0,
          parse_border_width_token(to_lower_copy(trim_copy(value))));
      return true;
    }
    if (property == "border-block-end-width") {
      set_border_width_for_side(
          style->border_width, 2,
          parse_border_width_token(to_lower_copy(trim_copy(value))));
      return true;
    }

    if (property == "border-inline-style") {
      parse_logical_border_style_pair(value, style, inline_start, inline_end);
      return true;
    }
    if (property == "border-inline-start-style") {
      parse_logical_border_style_single(value, style, inline_start);
      return true;
    }
    if (property == "border-inline-end-style") {
      parse_logical_border_style_single(value, style, inline_end);
      return true;
    }
    if (property == "border-block-style") {
      parse_logical_border_style_pair(value, style, 0, 2);
      return true;
    }
    if (property == "border-block-start-style") {
      parse_logical_border_style_single(value, style, 0);
      return true;
    }
    if (property == "border-block-end-style") {
      parse_logical_border_style_single(value, style, 2);
      return true;
    }

    if (property == "border-inline-color") {
      parse_logical_border_color_pair(value, style, inline_start, inline_end);
      return true;
    }
    if (property == "border-inline-start-color") {
      set_border_color_for_side(
          style, inline_start,
          parse_style_color(value, style, style->border_color));
      return true;
    }
    if (property == "border-inline-end-color") {
      set_border_color_for_side(
          style, inline_end,
          parse_style_color(value, style, style->border_color));
      return true;
    }
    if (property == "border-block-color") {
      parse_logical_border_color_pair(value, style, 0, 2);
      return true;
    }
    if (property == "border-block-start-color") {
      set_border_color_for_side(
          style, 0, parse_style_color(value, style, style->border_color));
      return true;
    }
    if (property == "border-block-end-color") {
      set_border_color_for_side(
          style, 2, parse_style_color(value, style, style->border_color));
      return true;
    }

    return false;
  }

  void parse_border_shorthand(const std::string& value, ComputedStyle* style,
                              int side = -1) {
    if (value.empty()) {
      return;
    }

    float parsed_width = 0.0f;
    bool width_set = false;
    bool hidden_style = false;
    bool saw_style = false;
    BorderStyle parsed_style = BorderStyle::Solid;

    for (const auto& token : split_css_tokens(value)) {
      if (is_border_style_token(token)) {
        parsed_style = parse_border_style_token(token);
        saw_style = true;
        hidden_style = hidden_style || is_border_style_hidden(token);
        continue;
      }

      if (is_color_value_token(token)) {
        set_border_color_for_side(
            style, side, parse_style_color(token, style, style->border_color));
        continue;
      }

      parsed_width = parse_border_width_token(token);
      width_set = true;
    }

    if (hidden_style) {
      set_border_width_for_side(style->border_width, side, 0.0f);
      set_border_style_for_side(style->border_style, side, BorderStyle::None);
      return;
    }

    if (width_set) {
      set_border_width_for_side(style->border_width, side, parsed_width);
    }
    if (saw_style) {
      set_border_style_for_side(style->border_style, side, parsed_style);
    }
  }

  void parse_border_style(const std::string& value, ComputedStyle* style) {
    const auto tokens = split_css_tokens(value);
    if (tokens.empty()) {
      return;
    }

    std::string sides[4];
    if (tokens.size() == 1) {
      sides[0] = sides[1] = sides[2] = sides[3] = tokens[0];
    } else if (tokens.size() == 2) {
      sides[0] = sides[2] = tokens[0];
      sides[1] = sides[3] = tokens[1];
    } else if (tokens.size() == 3) {
      sides[0] = tokens[0];
      sides[1] = sides[3] = tokens[1];
      sides[2] = tokens[2];
    } else {
      sides[0] = tokens[0];
      sides[1] = tokens[1];
      sides[2] = tokens[2];
      sides[3] = tokens[3];
    }

    for (int i = 0; i < 4; ++i) {
      style->border_style[i] = parse_border_style_token(sides[i]);
      if (is_border_style_hidden(sides[i])) {
        style->border_width[i] = 0.0f;
      }
    }
  }

  void parse_box_values(const std::string &value, float out[4]) {
    std::vector<float> values;
    for (const auto& token : split_css_tokens(value)) {
      values.push_back(parse_length(token));
    }
    if (values.empty())
      return;
    if (values.size() == 1) {
      out[0] = out[1] = out[2] = out[3] = values[0];
    } else if (values.size() == 2) {
      out[0] = out[2] = values[0];
      out[1] = out[3] = values[1];
    } else if (values.size() == 3) {
      out[0] = values[0];
      out[1] = out[3] = values[1];
      out[2] = values[2];
    } else {
      out[0] = values[0];
      out[1] = values[1];
      out[2] = values[2];
      out[3] = values[3];
    }
  }

  void parse_border_width_values(const std::string& value, float out[4]) {
    std::vector<float> values;
    for (const auto& token : split_css_tokens(value)) {
      values.push_back(parse_border_width_token(to_lower_copy(trim_copy(token))));
    }
    if (values.empty())
      return;
    if (values.size() == 1) {
      out[0] = out[1] = out[2] = out[3] = values[0];
    } else if (values.size() == 2) {
      out[0] = out[2] = values[0];
      out[1] = out[3] = values[1];
    } else if (values.size() == 3) {
      out[0] = values[0];
      out[1] = out[3] = values[1];
      out[2] = values[2];
    } else {
      out[0] = values[0];
      out[1] = values[1];
      out[2] = values[2];
      out[3] = values[3];
    }
  }
};

StyleEngine::StyleEngine() : impl_(new Impl()) {}
StyleEngine::~StyleEngine() = default;
void StyleEngine::parse_css(const std::string &css) { impl_->parse_css(css); }
void StyleEngine::append_css(const std::string& css) { impl_->append_css(css); }
bool StyleEngine::matches(const Element* elem, const std::string& selector) const {
  return impl_->matches(elem, selector);
}
CssLoadResult StyleEngine::load_stylesheet(const std::string& css,
                                           const CssLoadOptions& options) {
  return impl_->load_stylesheet(css, options);
}
CssLoadResult StyleEngine::replace_stylesheet(
    StylesheetId stylesheet_id, const std::string& css,
    const CssLoadOptions& options) {
  return impl_->replace_stylesheet(stylesheet_id, css, options);
}
bool StyleEngine::remove_stylesheet(StylesheetId stylesheet_id) {
  return impl_->remove_stylesheet(stylesheet_id);
}
void StyleEngine::apply_styles(Element *elem) { impl_->apply_styles(elem); }
const std::vector<AnimationKeyframeStep>* StyleEngine::keyframes(
    const std::string& name) const {
  return impl_->keyframes(name);
}
bool StyleEngine::uses_pseudo_class(Symbol pseudo) const {
  return impl_->uses_pseudo_class(pseudo);
}
void StyleEngine::clear() { impl_->clear(); }
void StyleEngine::clear_baseline_styles() { impl_->clear_baseline_styles(); }

bool CssLoadResult::has_errors() const {
  return std::any_of(diagnostics.begin(), diagnostics.end(),
                     [](const CssDiagnostic& diagnostic) {
                       return diagnostic.severity == CssDiagnosticSeverity::Error;
                     });
}

} // namespace flexUI
