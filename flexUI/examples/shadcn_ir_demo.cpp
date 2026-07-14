#ifndef WINDOWS_LEAN_AND_MEAN
#define WINDOWS_LEAN_AND_MEAN
#endif

#include <nlohmann/json.hpp>

#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/shadcn_ir.h>
#include <flexUI/widgets/label_widget.h>
#include "glfw_app.h"
#include "host_input_bridge.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;
using Json = json;

namespace {

namespace fs = std::filesystem;

json load_json_file(const fs::path& path) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("unable to open json file: " + path.string());
  }
  json value;
  input >> value;
  return value;
}

std::string load_text_file(const fs::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("unable to open text file: " + path.string());
  }
  return std::string(std::istreambuf_iterator<char>(input),
                     std::istreambuf_iterator<char>());
}

fs::path repo_root() {
  return fs::path(__FILE__).parent_path().parent_path().parent_path();
}

fs::path shadcn_sample_dir() {
  return repo_root() / "tools" / "shadcn-ir" / "samples";
}

std::string trim_for_panel(const std::string& text, size_t limit = 420);

struct SampleBundle {
  std::string name;
  json component;
  json style;
  json bridge;
  std::string component_text;
  std::string style_text;
  std::string bridge_text;
};

SampleBundle load_sample_bundle(const fs::path& sample_dir,
                                const std::string& name) {
  SampleBundle bundle;
  bundle.name = name;
  bundle.component = load_json_file(sample_dir / (name + ".component.json"));
  bundle.style = load_json_file(sample_dir / (name + ".style.json"));
  bundle.bridge = load_json_file(sample_dir / (name + ".bridge.json"));
  bundle.component_text =
      trim_for_panel(load_text_file(sample_dir / (name + ".component.json")));
  bundle.style_text =
      trim_for_panel(load_text_file(sample_dir / (name + ".style.json")));
  bundle.bridge_text =
      trim_for_panel(load_text_file(sample_dir / (name + ".bridge.json")));
  return bundle;
}

std::string summarize_runtime_config(const Json& props, const Json& active_states) {
  std::string out = "props:\n";
  out += props.empty() ? "{}" : props.dump(2);
  out += "\n\nbridge_states:\n";
  out += active_states.empty() ? "{}" : active_states.dump(2);
  return trim_for_panel(out, 520);
}

std::string summarize_css_targets(const Json& component_ir) {
  std::string out = "scope: " + flexUI::shadcn_ir::component_scope(component_ir) + "\n";
  out += "dom ids:\n";
  if (component_ir.contains("nodes")) {
    for (const auto& node : component_ir.at("nodes")) {
      const std::string node_id = node.value("id", std::string());
      if (node_id.empty()) {
        continue;
      }
      out += "- " + node_id + " -> #" +
             flexUI::shadcn_ir::node_dom_id(component_ir, node_id) + "\n";
    }
  }
  return trim_for_panel(out, 520);
}

std::string summarize_emitted_css(const Json& component_ir, const Json& style_ir,
                                  const Json& resolved_variants = Json::object()) {
  return trim_for_panel(
      flexUI::shadcn_ir::emit_css(component_ir, style_ir, resolved_variants), 520);
}

Json merge_demo_variants(const Json& component_ir, const Json& resolved_variants) {
  Json merged = Json::object();
  if (component_ir.contains("defaults")) {
    merged = component_ir.at("defaults");
  }
  for (auto it = resolved_variants.begin(); it != resolved_variants.end(); ++it) {
    merged[it.key()] = it.value();
  }
  return merged;
}

std::string format_float_compact(float value) {
  std::ostringstream out;
  out.setf(std::ios::fixed);
  out.precision(3);
  out << value;
  std::string text = out.str();
  while (!text.empty() && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
  if (text.empty()) {
    return "0";
  }
  return text;
}

std::string format_px(float value) {
  return format_float_compact(value) + "px";
}

std::string format_optional_px(float value) {
  if (std::isnan(value)) {
    return "auto";
  }
  return format_px(value);
}

std::string format_color_rgba(const flexUI::Color& color) {
  return "rgba(" + format_float_compact(color.r) + ", " +
         format_float_compact(color.g) + ", " + format_float_compact(color.b) + ", " +
         format_float_compact(color.a) + ")";
}

std::string format_box_shorthand(const float values[4]) {
  if (values[0] == values[1] && values[0] == values[2] && values[0] == values[3]) {
    return format_px(values[0]);
  }
  if (values[0] == values[2] && values[1] == values[3]) {
    return format_px(values[0]) + " " + format_px(values[1]);
  }
  if (values[1] == values[3]) {
    return format_px(values[0]) + " " + format_px(values[1]) + " " + format_px(values[2]);
  }
  return format_px(values[0]) + " " + format_px(values[1]) + " " + format_px(values[2]) +
         " " + format_px(values[3]);
}

std::string display_to_string(flexUI::Display display) {
  switch (display) {
    case flexUI::Display::Block:
      return "block";
    case flexUI::Display::Inline:
      return "inline";
    case flexUI::Display::Flex:
      return "flex";
    case flexUI::Display::Grid:
      return "grid";
    case flexUI::Display::None:
      return "none";
  }
  return "block";
}

std::string position_to_string(flexUI::Position position) {
  switch (position) {
    case flexUI::Position::Static:
      return "static";
    case flexUI::Position::Relative:
      return "relative";
    case flexUI::Position::Absolute:
      return "absolute";
    case flexUI::Position::Fixed:
      return "fixed";
    case flexUI::Position::Sticky:
      return "sticky";
  }
  return "static";
}

std::string text_align_to_string(flexUI::TextAlign align) {
  switch (align) {
    case flexUI::TextAlign::Left:
      return "left";
    case flexUI::TextAlign::Center:
      return "center";
    case flexUI::TextAlign::Right:
      return "right";
    case flexUI::TextAlign::Start:
      return "start";
    case flexUI::TextAlign::End:
      return "end";
    case flexUI::TextAlign::Justify:
      return "justify";
  }
  return "left";
}

std::string font_weight_to_string(flexUI::FontWeight weight) {
  switch (weight) {
    case flexUI::FontWeight::Normal:
      return "400";
    case flexUI::FontWeight::Bold:
      return "700";
    case flexUI::FontWeight::Light:
      return "300";
    case flexUI::FontWeight::Medium:
      return "500";
    case flexUI::FontWeight::SemiBold:
      return "600";
    case flexUI::FontWeight::ExtraBold:
      return "800";
    case flexUI::FontWeight::Black:
      return "900";
  }
  return "400";
}

std::string trim_copy_local(const std::string& value) {
  size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }
  size_t end = value.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(begin, end - begin);
}

std::string to_lower_copy_local(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return value;
}

std::string collapse_spaces_local(const std::string& value) {
  std::string out;
  out.reserve(value.size());
  bool in_space = false;
  for (char ch : value) {
    if (std::isspace(static_cast<unsigned char>(ch))) {
      if (!out.empty() && !in_space) {
        out.push_back(' ');
      }
      in_space = true;
      continue;
    }
    out.push_back(ch);
    in_space = false;
  }
  return trim_copy_local(out);
}

std::vector<std::string> split_space_tokens_local(const std::string& value) {
  std::vector<std::string> tokens;
  std::istringstream in(value);
  std::string token;
  while (in >> token) {
    tokens.push_back(token);
  }
  return tokens;
}

bool try_parse_float_local(const std::string& value, float& out) {
  char* end = nullptr;
  const float parsed = std::strtof(value.c_str(), &end);
  if (end == value.c_str() || (end && *end != '\0')) {
    return false;
  }
  out = parsed;
  return true;
}

bool is_color_property_name(const std::string& property) {
  return property.find("color") != std::string::npos ||
         property == "background";
}

bool is_length_property_name(const std::string& property) {
  return property == "width" || property == "height" || property == "top" ||
         property == "right" || property == "bottom" || property == "left" ||
         property == "gap" || property == "font-size" ||
         property.find("padding") == 0 || property.find("margin") == 0 ||
         property.find("border") == 0 || property.find("outline") == 0 ||
         property.find("ring") == 0 || property == "border-radius";
}

bool string_ends_with_local(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::optional<std::string> resolve_variable_references_local(flexUI::Element* elem,
                                                             const std::string& value) {
  if (!elem || !elem->computed_style) {
    return std::nullopt;
  }
  const std::string trimmed = trim_copy_local(value);
  if (trimmed.find("var(") == std::string::npos) {
    return std::nullopt;
  }
  const std::string resolved = elem->computed_style->resolve_variable_value(trimmed);
  if (resolved.empty() || resolved == trimmed) {
    return std::nullopt;
  }
  return resolved;
}

bool contains_dynamic_css_function_local(const std::string& value) {
  const std::string lowered = to_lower_copy_local(value);
  return lowered.find("var(") != std::string::npos ||
         lowered.find("color-mix(") != std::string::npos ||
         lowered.find("light-dark(") != std::string::npos ||
         lowered.find("calc(") != std::string::npos ||
         lowered.find("clamp(") != std::string::npos ||
         lowered.find("min(") != std::string::npos ||
         lowered.find("max(") != std::string::npos;
}

std::string property_group_for_diff(const std::string& property) {
  if (property == "width" || property == "height" || property == "min-width" ||
      property == "max-width" || property == "min-height" ||
      property == "max-height" || property == "padding" || property == "margin" ||
      property == "border-width" || property == "border-radius" ||
      property.rfind("padding-", 0) == 0 || property.rfind("margin-", 0) == 0 ||
      property.rfind("border-", 0) == 0) {
    return "box-model";
  }
  if (property == "font-family" || property == "font-size" ||
      property == "font-weight" || property == "text-align" ||
      property.rfind("text-", 0) == 0) {
    return "text";
  }
  if (property.find("color") != std::string::npos || property == "background") {
    return "color";
  }
  if (property == "opacity" || property == "box-shadow" || property == "transition" ||
      property == "animation-name" || property == "animation-duration" ||
      property.rfind("outline-", 0) == 0 || property.rfind("ring-", 0) == 0) {
    return "effect";
  }
  if (property == "display" || property == "position" || property == "top" ||
      property == "right" || property == "bottom" || property == "left" ||
      property == "gap") {
    return "positioning";
  }
  return "other";
}

std::optional<flexUI::Color> parse_color_literal_local(const std::string& value) {
  const std::string trimmed = to_lower_copy_local(trim_copy_local(value));
  if (trimmed.empty()) {
    return std::nullopt;
  }
  if (trimmed == "transparent") {
    return flexUI::Color{0.0f, 0.0f, 0.0f, 0.0f};
  }
  if (trimmed[0] == '#') {
    auto hex_digit = [](char ch) -> int {
      if (ch >= '0' && ch <= '9') return ch - '0';
      if (ch >= 'a' && ch <= 'f') return 10 + (ch - 'a');
      return -1;
    };
    const std::string hex = trimmed.substr(1);
    auto to_channel = [&](int hi, int lo) {
      return static_cast<float>((hi << 4) | lo) / 255.0f;
    };
    if (hex.size() == 3 || hex.size() == 4) {
      const int r = hex_digit(hex[0]);
      const int g = hex_digit(hex[1]);
      const int b = hex_digit(hex[2]);
      const int a = hex.size() == 4 ? hex_digit(hex[3]) : 15;
      if (r >= 0 && g >= 0 && b >= 0 && a >= 0) {
        return flexUI::Color{to_channel(r, r), to_channel(g, g),
                             to_channel(b, b), to_channel(a, a)};
      }
    }
    if (hex.size() == 6 || hex.size() == 8) {
      const int r0 = hex_digit(hex[0]);
      const int r1 = hex_digit(hex[1]);
      const int g0 = hex_digit(hex[2]);
      const int g1 = hex_digit(hex[3]);
      const int b0 = hex_digit(hex[4]);
      const int b1 = hex_digit(hex[5]);
      const int a0 = hex.size() == 8 ? hex_digit(hex[6]) : 15;
      const int a1 = hex.size() == 8 ? hex_digit(hex[7]) : 15;
      if (r0 >= 0 && r1 >= 0 && g0 >= 0 && g1 >= 0 &&
          b0 >= 0 && b1 >= 0 && a0 >= 0 && a1 >= 0) {
        return flexUI::Color{to_channel(r0, r1), to_channel(g0, g1),
                             to_channel(b0, b1), to_channel(a0, a1)};
      }
    }
    return std::nullopt;
  }
  auto parse_function_args = [](const std::string& inner) {
    std::vector<std::string> parts;
    std::string current;
    int slash_count = 0;
    for (char ch : inner) {
      if (ch == ',') {
        parts.push_back(trim_copy_local(current));
        current.clear();
        continue;
      }
      if (ch == '/') {
        ++slash_count;
        parts.push_back(trim_copy_local(current));
        current.clear();
        continue;
      }
      current.push_back(ch);
    }
    if (!current.empty()) {
      parts.push_back(trim_copy_local(current));
    }
    if (parts.size() == 3 && slash_count == 0) {
      const auto ws_parts = split_space_tokens_local(inner);
      if (ws_parts.size() == 3 || ws_parts.size() == 4) {
        return ws_parts;
      }
    }
    return parts;
  };
  const auto parse_channel = [](const std::string& token, bool alpha) -> std::optional<float> {
    std::string part = trim_copy_local(token);
    if (part.empty()) return std::nullopt;
    if (part.back() == '%') {
      float parsed = 0.0f;
      if (!try_parse_float_local(part.substr(0, part.size() - 1), parsed)) {
        return std::nullopt;
      }
      return alpha ? parsed / 100.0f : parsed * 255.0f / 100.0f;
    }
    float parsed = 0.0f;
    if (!try_parse_float_local(part, parsed)) {
      return std::nullopt;
    }
    return parsed;
  };
  auto parse_rgb_like = [&](const std::string& prefix) -> std::optional<flexUI::Color> {
    if (trimmed.rfind(prefix, 0) != 0 || trimmed.back() != ')') {
      return std::nullopt;
    }
    const std::string inner = trimmed.substr(prefix.size(), trimmed.size() - prefix.size() - 1);
    const auto parts = parse_function_args(inner);
    if (parts.size() < 3 || parts.size() > 4) {
      return std::nullopt;
    }
    const auto r = parse_channel(parts[0], false);
    const auto g = parse_channel(parts[1], false);
    const auto b = parse_channel(parts[2], false);
    const auto a = parts.size() == 4 ? parse_channel(parts[3], true)
                                     : std::optional<float>(1.0f);
    if (!r || !g || !b || !a) {
      return std::nullopt;
    }
    return flexUI::Color{*r > 1.0f ? *r / 255.0f : *r,
                         *g > 1.0f ? *g / 255.0f : *g,
                         *b > 1.0f ? *b / 255.0f : *b, *a};
  };
  if (auto rgba = parse_rgb_like("rgba(")) {
    return rgba;
  }
  if (auto rgb = parse_rgb_like("rgb(")) {
    return rgb;
  }
  return std::nullopt;
}

std::optional<std::string> normalize_length_token_local(const std::string& token) {
  const std::string trimmed = to_lower_copy_local(trim_copy_local(token));
  if (trimmed.empty()) {
    return std::nullopt;
  }
  if (trimmed == "auto") {
    return std::string("auto");
  }
  if (trimmed.back() == '%') {
    float parsed = 0.0f;
    if (!try_parse_float_local(trimmed.substr(0, trimmed.size() - 1), parsed)) {
      return std::nullopt;
    }
    return format_float_compact(parsed) + "%";
  }
  if (trimmed.size() > 2 &&
      (string_ends_with_local(trimmed, "px") ||
       string_ends_with_local(trimmed, "ms") ||
       string_ends_with_local(trimmed, "rem"))) {
    const std::string unit = trimmed.substr(trimmed.size() - 2);
    std::string number_part = trimmed.substr(0, trimmed.size() - 2);
    std::string suffix = unit;
    if (string_ends_with_local(trimmed, "rem")) {
      number_part = trimmed.substr(0, trimmed.size() - 3);
      suffix = "rem";
    }
    float parsed = 0.0f;
    if (!try_parse_float_local(number_part, parsed)) {
      return std::nullopt;
    }
    if (suffix == "rem") {
      return format_px(parsed * 16.0f);
    }
    return format_float_compact(parsed) + suffix;
  }
  float bare = 0.0f;
  if (try_parse_float_local(trimmed, bare)) {
    if (bare == 0.0f) {
      return std::string("0px");
    }
    return std::nullopt;
  }
  return std::nullopt;
}

std::string normalize_property_value_for_compare(flexUI::Element* elem,
                                                 const std::string& property,
                                                 const std::string& raw_value) {
  std::string value = trim_copy_local(raw_value);
  if (value.empty()) {
    return value;
  }
  if (auto resolved = resolve_variable_references_local(elem, value)) {
    value = *resolved;
  }
  if (property == "font-weight") {
    const std::string lowered = to_lower_copy_local(value);
    if (lowered == "normal") return "400";
    if (lowered == "bold") return "700";
    if (lowered == "light") return "300";
    if (lowered == "medium") return "500";
    if (lowered == "semibold") return "600";
    if (lowered == "extrabold") return "800";
    if (lowered == "black") return "900";
  }
  if (is_color_property_name(property)) {
    if (auto parsed = parse_color_literal_local(value)) {
      return format_color_rgba(*parsed);
    }
  }
  if (property == "padding" || property == "margin" || property == "border-width" ||
      property == "border-radius") {
    const auto tokens = split_space_tokens_local(value);
    if (!tokens.empty()) {
      std::vector<std::string> normalized;
      normalized.reserve(tokens.size());
      for (const auto& token : tokens) {
        if (auto length = normalize_length_token_local(token)) {
          normalized.push_back(*length);
        } else {
          normalized.push_back(to_lower_copy_local(token));
        }
      }
      std::ostringstream out;
      for (size_t i = 0; i < normalized.size(); ++i) {
        if (i) out << ' ';
        out << normalized[i];
      }
      return out.str();
    }
  }
  if (is_length_property_name(property)) {
    if (auto length = normalize_length_token_local(value)) {
      return *length;
    }
  }
  return collapse_spaces_local(value);
}

std::optional<std::string> runtime_style_value_for_property(flexUI::Element* elem,
                                                            const std::string& property) {
  if (!elem || !elem->computed_style) {
    return std::nullopt;
  }
  const auto* style = elem->computed_style;
  const std::string value_from_vars = style->get_variable(flexUI::Symbol(property), "");

  if (property == "display") return display_to_string(style->display);
  if (property == "position") return position_to_string(style->position);
  if (property == "width") return style->width > 0.0f ? (style->width_is_percent
                                                            ? format_float_compact(style->width) + "%"
                                                            : format_px(style->width))
                                                       : std::string("auto");
  if (property == "height") return style->height > 0.0f ? (style->height_is_percent
                                                              ? format_float_compact(style->height) + "%"
                                                              : format_px(style->height))
                                                         : std::string("auto");
  if (property == "max-width" || property == "min-width" || property == "max-height" ||
      property == "min-height" || property == "transition" || property == "animation-name" ||
      property == "animation-duration") {
    if (!value_from_vars.empty()) return value_from_vars;
    if (property == "transition" && !style->transition.empty()) return style->transition;
    if (property == "animation-name" && !style->animation_name.empty()) return style->animation_name;
    if (property == "animation-duration" && style->animation_duration_ms > 0.0f)
      return format_float_compact(style->animation_duration_ms) + "ms";
    return std::nullopt;
  }
  if (property == "top") return format_optional_px(style->top);
  if (property == "right") return format_optional_px(style->right);
  if (property == "bottom") return format_optional_px(style->bottom);
  if (property == "left") return format_optional_px(style->left);
  if (property == "gap") return format_px(style->gap);
  if (property == "font-family") return style->font_family;
  if (property == "font-size") return format_px(style->font_size);
  if (property == "font-weight") return font_weight_to_string(style->font_weight);
  if (property == "text-align") return text_align_to_string(style->text_align);
  if (property == "opacity") return format_float_compact(style->opacity);
  if (property == "background-color") return format_color_rgba(style->background_color);
  if (property == "color") return format_color_rgba(style->text_color);
  if (property == "border-color") return format_color_rgba(style->border_color);
  if (property == "outline-width") return format_px(style->outline_width);
  if (property == "outline-offset") return format_px(style->outline_offset);
  if (property == "outline-color") return format_color_rgba(style->outline_color);
  if (property == "ring-width") return format_px(style->ring_width);
  if (property == "ring-offset") return format_px(style->ring_offset);
  if (property == "ring-color") return format_color_rgba(style->ring_color);
  if (property == "ring-offset-color") return format_color_rgba(style->ring_offset_color);
  if (property == "padding") return format_box_shorthand(style->padding);
  if (property == "margin") return format_box_shorthand(style->margin);
  if (property == "border-width") return format_box_shorthand(style->border_width);
  if (property == "border-radius") return format_box_shorthand(style->border_radius);
  if (property == "padding-top") return format_px(style->padding[0]);
  if (property == "padding-right") return format_px(style->padding[1]);
  if (property == "padding-bottom") return format_px(style->padding[2]);
  if (property == "padding-left") return format_px(style->padding[3]);
  if (property == "margin-top") return format_px(style->margin[0]);
  if (property == "margin-right") return format_px(style->margin[1]);
  if (property == "margin-bottom") return format_px(style->margin[2]);
  if (property == "margin-left") return format_px(style->margin[3]);
  if (property == "border-top-width") return format_px(style->border_width[0]);
  if (property == "border-right-width") return format_px(style->border_width[1]);
  if (property == "border-bottom-width") return format_px(style->border_width[2]);
  if (property == "border-left-width") return format_px(style->border_width[3]);
  if (property == "background" && style->has_gradient) return std::string("<gradient>");
  if (property == "box-shadow") return style->has_shadow ? std::string("<shadow>") : std::string("none");
  if (property == "accent-color") {
    const auto color = style->get_variable_color("--accent-color", flexUI::Color{0, 0, 0, 0});
    return format_color_rgba(color);
  }
  if (property == "caret-color") {
    const auto color = style->get_variable_color("--caret-color", flexUI::Color{0, 0, 0, 0});
    return format_color_rgba(color);
  }
  if (!value_from_vars.empty()) {
    return value_from_vars;
  }
  return std::nullopt;
}

std::string summarize_bridge_decisions(
    const Json& bridge_ir, const Json& active_states,
    const std::unordered_map<std::string, flexUI::Element*>& nodes) {
  auto condition_result = [](const Json& condition, const Json& states,
                             std::string& reason_out) -> bool {
    const std::string type = condition.value("type", std::string());
    const std::string name = condition.value("name", std::string());
    if (type == "state" || type == "prop" || type == "variant") {
      const auto it = states.find(name);
      if (it == states.end()) {
        reason_out = type + "(" + name + ") missing";
        return false;
      }
      const Json expected =
          condition.contains("equals") ? condition.at("equals")
                                        : condition.value("value", Json(true));
      if (*it != expected) {
        reason_out = type + "(" + name + ") expected " + expected.dump() +
                     ", got " + it->dump();
        return false;
      }
      reason_out = type + "(" + name + ") matched " + expected.dump();
      return true;
    }
    reason_out = "condition type " + type + " treated as passthrough";
    return true;
  };

  if (!bridge_ir.contains("bridges") || !bridge_ir.at("bridges").is_array()) {
    return "no bridge rules";
  }

  std::ostringstream out;
  size_t index = 0;
  for (const auto& bridge : bridge_ir.at("bridges")) {
    ++index;
    const std::string target = bridge.value("target", std::string("<missing-target>"));
    out << "#" << index << " target=" << target;
    const auto node_it = nodes.find(target);
    if (node_it != nodes.end() && node_it->second && !node_it->second->id().empty()) {
      out << " (#" << node_it->second->id() << ")";
    }
    out << "\n";

    bool active = true;
    std::vector<std::string> checks;

    if (bridge.contains("state")) {
      const std::string state_name = bridge.at("state").get<std::string>();
      const auto it = active_states.find(state_name);
      if (it == active_states.end()) {
        active = false;
        checks.push_back("state(" + state_name + ") missing");
      } else if (!it->is_boolean() || !it->get<bool>()) {
        active = false;
        checks.push_back("state(" + state_name + ") inactive: " + it->dump());
      } else {
        checks.push_back("state(" + state_name + ") active");
      }
    }

    if (bridge.contains("when")) {
      for (const auto& condition : bridge.at("when")) {
        std::string reason;
        const bool matched = condition_result(condition, active_states, reason);
        checks.push_back(reason);
        if (!matched) {
          active = false;
        }
      }
    }

    out << "  decision: " << (active ? "active" : "inactive") << "\n";
    out << "  checks:\n";
    if (checks.empty()) {
      out << "    - unconditional\n";
    } else {
      for (const auto& check : checks) {
        out << "    - " << check << "\n";
      }
    }

    if (bridge.contains("attributes")) {
      out << "  attributes:\n";
      for (auto it = bridge.at("attributes").begin(); it != bridge.at("attributes").end();
           ++it) {
        out << "    - " << it.key() << " = "
            << (it->is_string() ? it->get<std::string>() : it->dump()) << "\n";
      }
    }
    if (bridge.contains("missing_attributes") && bridge.at("missing_attributes").is_array() &&
        !bridge.at("missing_attributes").empty()) {
      out << "  missing_attributes:\n";
      for (const auto& attr : bridge.at("missing_attributes")) {
        out << "    - " << attr.get<std::string>() << "\n";
      }
    }
    if (bridge.contains("pseudo_states") && bridge.at("pseudo_states").is_array() &&
        !bridge.at("pseudo_states").empty()) {
      out << "  pseudo_states:\n";
      for (const auto& state : bridge.at("pseudo_states")) {
        out << "    - :" << state.get<std::string>() << "\n";
      }
    }
    if (bridge.contains("properties")) {
      out << "  properties:\n";
      for (auto it = bridge.at("properties").begin(); it != bridge.at("properties").end();
           ++it) {
        out << "    - " << it.key() << " = " << it.value().dump() << "\n";
      }
    }
    out << "\n";
  }

  return trim_for_panel(out.str(), 520);
}

std::string summarize_bridge_host_snapshot(
    const Json& bridge_ir, const Json& active_states,
    const std::unordered_map<std::string, flexUI::Element*>& nodes) {
  struct TargetProjection {
    std::unordered_map<std::string, std::string> attribute_payloads;
    std::vector<std::string> managed_attributes;
    std::unordered_map<std::string, Json> property_payloads;
    std::vector<std::string> managed_properties;
    std::vector<std::string> managed_pseudo_states;
  };

  auto append_unique = [](std::vector<std::string>& out, const std::string& value) {
    if (std::find(out.begin(), out.end(), value) == out.end()) {
      out.push_back(value);
    }
  };

  auto condition_matches = [](const Json& condition, const Json& states) {
    const std::string type = condition.value("type", std::string());
    const std::string name = condition.value("name", std::string());
    if (type == "state" || type == "prop" || type == "variant") {
      const auto it = states.find(name);
      if (it == states.end()) {
        return false;
      }
      const Json expected =
          condition.contains("equals") ? condition.at("equals")
                                        : condition.value("value", Json(true));
      return *it == expected;
    }
    return true;
  };

  auto bridge_is_active = [&](const Json& bridge) {
    if (bridge.contains("state")) {
      const auto it = active_states.find(bridge.at("state").get<std::string>());
      if (it == active_states.end() || !it->is_boolean() || !it->get<bool>()) {
        return false;
      }
    }
    if (bridge.contains("when")) {
      for (const auto& condition : bridge.at("when")) {
        if (!condition_matches(condition, active_states)) {
          return false;
        }
      }
    }
    return true;
  };

  std::unordered_map<std::string, TargetProjection> projections;
  if (bridge_ir.contains("bridges")) {
    for (const auto& bridge : bridge_ir.at("bridges")) {
      const std::string target = bridge.value("target", std::string());
      if (target.empty()) {
        continue;
      }
      auto& projection = projections[target];
      if (bridge.contains("attributes")) {
        for (auto it = bridge.at("attributes").begin(); it != bridge.at("attributes").end();
             ++it) {
          append_unique(projection.managed_attributes, it.key());
          if (bridge_is_active(bridge)) {
            projection.attribute_payloads[it.key()] =
                it.value().is_string() ? it.value().get<std::string>() : it.value().dump();
          }
        }
      }
      if (bridge.contains("missing_attributes")) {
        for (const auto& attr : bridge.at("missing_attributes")) {
          append_unique(projection.managed_attributes, attr.get<std::string>());
        }
      }
      if (bridge.contains("pseudo_states")) {
        for (const auto& state : bridge.at("pseudo_states")) {
          append_unique(projection.managed_pseudo_states, state.get<std::string>());
        }
      }
      if (bridge.contains("properties")) {
        for (auto it = bridge.at("properties").begin(); it != bridge.at("properties").end();
             ++it) {
          append_unique(projection.managed_properties, it.key());
          if (bridge_is_active(bridge)) {
            projection.property_payloads[it.key()] = it.value();
          }
        }
      }
    }
  }

  if (projections.empty()) {
    return "no managed bridge targets";
  }

  std::vector<std::string> targets;
  for (const auto& entry : projections) {
    targets.push_back(entry.first);
  }
  std::sort(targets.begin(), targets.end());

  std::ostringstream out;
  bool first_target = true;
  for (const auto& target_name : targets) {
    const auto proj_it = projections.find(target_name);
    if (proj_it == projections.end()) {
      continue;
    }
    const auto node_it = nodes.find(target_name);
    flexUI::Element* elem = node_it != nodes.end() ? node_it->second : nullptr;
    if (!first_target) {
      out << "\n";
    }
    first_target = false;

    out << target_name;
    if (elem && !elem->id().empty()) {
      out << " (#" << elem->id() << ")";
    }
    out << "\n";

    auto attrs = proj_it->second.managed_attributes;
    std::sort(attrs.begin(), attrs.end());
    out << "  attrs:\n";
    for (const auto& attr : attrs) {
      const std::string* value = elem ? elem->attribute(attr) : nullptr;
      out << "    " << attr << " = " << (value ? *value : "<missing>") << "\n";
    }

    auto states = proj_it->second.managed_pseudo_states;
    std::sort(states.begin(), states.end());
    out << "  pseudo_states:\n";
    for (const auto& state : states) {
      const bool active = elem ? elem->has_state(flexUI::Symbol(state)) : false;
      out << "    :" << state << " = " << (active ? "true" : "false") << "\n";
    }

    auto properties = proj_it->second.managed_properties;
    std::sort(properties.begin(), properties.end());
    if (!properties.empty()) {
      out << "  widget_properties:\n";
      for (const auto& property : properties) {
        const auto value_it = proj_it->second.property_payloads.find(property);
        if (value_it != proj_it->second.property_payloads.end()) {
          out << "    " << property << " = " << value_it->second.dump() << "\n";
        } else {
          out << "    " << property << " = <unset>\n";
        }
      }
    }
  }
  return trim_for_panel(out.str(), 520);
}

std::string summarize_resolved_node_tree(
    const Json& component_ir,
    const std::unordered_map<std::string, flexUI::Element*>& nodes) {
  std::unordered_map<std::string, Json> node_specs;
  std::unordered_map<std::string, std::vector<std::string>> children;
  if (component_ir.contains("nodes")) {
    for (const auto& node : component_ir.at("nodes")) {
      const std::string id = node.value("id", std::string());
      if (!id.empty()) {
        node_specs.emplace(id, node);
      }
    }
  }
  if (component_ir.contains("edges")) {
    for (const auto& edge : component_ir.at("edges")) {
      const std::string parent = edge.value("parent", std::string());
      const std::string child = edge.value("child", std::string());
      if (!parent.empty() && !child.empty()) {
        children[parent].push_back(child);
      }
    }
  }
  for (auto& entry : children) {
    std::sort(entry.second.begin(), entry.second.end());
  }

  const std::string root_id = component_ir.value("root", std::string());
  if (root_id.empty()) {
    return "component root missing";
  }

  std::ostringstream out;
  out << "root: " << root_id << "\n";

  std::function<void(const std::string&, int)> dump_node =
      [&](const std::string& node_id, int depth) {
        auto spec_it = node_specs.find(node_id);
        if (spec_it == node_specs.end()) {
          return;
        }

        const Json& node = spec_it->second;
        const std::string indent(static_cast<size_t>(depth) * 2, ' ');
        out << indent << "- " << node_id;
        out << " [" << node.value("type", std::string("unknown")) << "]";
        if (node.contains("widget")) {
          out << " widget=" << node.value("widget", std::string());
        }
        if (node.contains("slot")) {
          out << " slot=" << node.value("slot", std::string());
        }
        if (node.contains("tag")) {
          out << " tag=" << node.value("tag", std::string());
        }
        if (node.contains("role")) {
          out << " role=" << node.value("role", std::string());
        }
        const auto elem_it = nodes.find(node_id);
        if (elem_it != nodes.end() && elem_it->second && !elem_it->second->id().empty()) {
          out << " dom=#" << elem_it->second->id();
        } else {
          out << " dom=#"
              << flexUI::shadcn_ir::node_dom_id(component_ir, node_id);
        }
        out << "\n";

        auto child_it = children.find(node_id);
        if (child_it == children.end()) {
          return;
        }
        for (const auto& child_id : child_it->second) {
          dump_node(child_id, depth + 1);
        }
      };

  dump_node(root_id, 0);
  return trim_for_panel(out.str(), 520);
}

std::string summarize_resolved_style_hits(
    const Json& component_ir, const Json& style_ir, const Json& resolved_variants,
    const std::unordered_map<std::string, flexUI::Element*>& nodes) {
  const Json variants = merge_demo_variants(component_ir, resolved_variants);
  auto condition_result = [&](const Json& condition, const std::string& target,
                              std::string& reason_out) -> bool {
    const std::string type = condition.value("type", std::string());
    const std::string name = condition.value("name", std::string());

    if (type == "variant") {
      const auto it = variants.find(name);
      if (it == variants.end()) {
        reason_out = "variant(" + name + ") missing";
        return false;
      }
      const Json expected =
          condition.contains("value") ? condition.at("value") : condition.at("equals");
      if (*it != expected) {
        reason_out =
            "variant(" + name + ") expected " + expected.dump() + ", got " + it->dump();
        return false;
      }
      reason_out = "variant(" + name + ") matched " + expected.dump();
      return true;
    }

    const auto node_it = nodes.find(target);
    flexUI::Element* elem = node_it != nodes.end() ? node_it->second : nullptr;
    if (!elem) {
      reason_out = "target(" + target + ") missing";
      return false;
    }

    if (type == "state") {
      const bool active = elem->has_state(flexUI::Symbol(name));
      const Json expected =
          condition.contains("value") ? condition.at("value")
                                       : condition.value("equals", Json(true));
      const bool expected_bool = expected.is_boolean() ? expected.get<bool>() : true;
      if (active != expected_bool) {
        reason_out = "state(" + name + ") expected " + expected.dump() + ", got " +
                     (active ? "true" : "false");
        return false;
      }
      reason_out = "state(" + name + ") matched " + expected.dump();
      return true;
    }

    if (type == "attr") {
      const std::string* value = elem->attribute(name);
      if (condition.contains("equals") || condition.contains("value")) {
        const Json expected_json =
            condition.contains("equals") ? condition.at("equals") : condition.at("value");
        const std::string expected =
            expected_json.is_string() ? expected_json.get<std::string>() : expected_json.dump();
        if (!value) {
          reason_out = "attr(" + name + ") missing, expected " + expected;
          return false;
        }
        if (*value != expected) {
          reason_out = "attr(" + name + ") expected " + expected + ", got " + *value;
          return false;
        }
        reason_out = "attr(" + name + ") matched " + expected;
        return true;
      }
      if (!value) {
        reason_out = "attr(" + name + ") missing";
        return false;
      }
      reason_out = "attr(" + name + ") present";
      return true;
    }

    if (type == "media" || type == "container") {
      reason_out = type + " query not evaluated in demo";
      return false;
    }

    reason_out = "condition type " + type + " treated as passthrough";
    return true;
  };

  if (!style_ir.contains("rules") || !style_ir.at("rules").is_array()) {
    return "no style rules";
  }

  std::ostringstream out;
  size_t rule_index = 0;
  for (const auto& rule : style_ir.at("rules")) {
    ++rule_index;
    const std::string target = rule.value("target", std::string("<missing-target>"));
    const std::string selector = rule.value("selector", std::string("&"));

    bool active = true;
    std::vector<std::string> checks;
    if (rule.contains("when")) {
      for (const auto& condition : rule.at("when")) {
        std::string reason;
        const bool matched = condition_result(condition, target, reason);
        checks.push_back(reason);
        if (!matched) {
          active = false;
        }
      }
    }

    out << "#" << rule_index << " target=" << target << " selector=\"" << selector << "\"\n";
    out << "  decision: " << (active ? "hit" : "miss") << "\n";
    out << "  checks:\n";
    if (checks.empty()) {
      out << "    - unconditional\n";
    } else {
      for (const auto& check : checks) {
        out << "    - " << check << "\n";
      }
    }
    out << "  decls:\n";
    for (auto it = rule.at("decls").begin(); it != rule.at("decls").end(); ++it) {
      out << "    - " << it.key() << ": " << it.value().get<std::string>() << "\n";
    }
    out << "\n";
  }

  if (style_ir.contains("media") && style_ir.at("media").is_array() &&
      !style_ir.at("media").empty()) {
    out << "media blocks:\n";
    for (const auto& block : style_ir.at("media")) {
      out << "  - @media " << block.value("query", std::string()) << " (not evaluated)\n";
    }
  }
  if (style_ir.contains("container") && style_ir.at("container").is_array() &&
      !style_ir.at("container").empty()) {
    out << "container blocks:\n";
    for (const auto& block : style_ir.at("container")) {
      out << "  - @container ";
      if (block.contains("name") && !block.at("name").get<std::string>().empty()) {
        out << block.at("name").get<std::string>() << " ";
      }
      out << block.value("query", std::string()) << " (not evaluated)\n";
    }
  }
  return trim_for_panel(out.str(), 520);
}

std::string summarize_resolved_style_provenance(
    const Json& component_ir, const Json& style_ir, const Json& resolved_variants,
    const std::unordered_map<std::string, flexUI::Element*>& nodes) {
  struct AppliedDecl {
    size_t rule_index = 0;
    std::string selector;
    std::string value;
    std::vector<std::string> checks;
  };

  const Json variants = merge_demo_variants(component_ir, resolved_variants);
  auto evaluate_rule = [&](const Json& rule, std::vector<std::string>& checks_out) -> bool {
    const std::string target = rule.value("target", std::string());
    const auto node_it = nodes.find(target);
    flexUI::Element* elem = node_it != nodes.end() ? node_it->second : nullptr;
    bool active = true;

    if (rule.contains("when")) {
      for (const auto& condition : rule.at("when")) {
        const std::string type = condition.value("type", std::string());
        const std::string name = condition.value("name", std::string());

        if (type == "variant") {
          const auto it = variants.find(name);
          if (it == variants.end()) {
            checks_out.push_back("variant(" + name + ") missing");
            active = false;
            continue;
          }
          const Json expected =
              condition.contains("value") ? condition.at("value") : condition.at("equals");
          if (*it != expected) {
            checks_out.push_back("variant(" + name + ") expected " + expected.dump() +
                                 ", got " + it->dump());
            active = false;
          } else {
            checks_out.push_back("variant(" + name + ") matched " + expected.dump());
          }
          continue;
        }

        if (!elem) {
          checks_out.push_back("target(" + target + ") missing");
          active = false;
          continue;
        }

        if (type == "state") {
          const bool actual = elem->has_state(flexUI::Symbol(name));
          const Json expected =
              condition.contains("value") ? condition.at("value")
                                          : condition.value("equals", Json(true));
          const bool expected_bool = expected.is_boolean() ? expected.get<bool>() : true;
          if (actual != expected_bool) {
            checks_out.push_back("state(" + name + ") expected " + expected.dump() +
                                 ", got " + (actual ? "true" : "false"));
            active = false;
          } else {
            checks_out.push_back("state(" + name + ") matched " + expected.dump());
          }
          continue;
        }

        if (type == "attr") {
          const std::string* value = elem->attribute(name);
          if (condition.contains("equals") || condition.contains("value")) {
            const Json expected_json =
                condition.contains("equals") ? condition.at("equals") : condition.at("value");
            const std::string expected = expected_json.is_string()
                                             ? expected_json.get<std::string>()
                                             : expected_json.dump();
            if (!value) {
              checks_out.push_back("attr(" + name + ") missing, expected " + expected);
              active = false;
            } else if (*value != expected) {
              checks_out.push_back("attr(" + name + ") expected " + expected + ", got " +
                                   *value);
              active = false;
            } else {
              checks_out.push_back("attr(" + name + ") matched " + expected);
            }
          } else if (!value) {
            checks_out.push_back("attr(" + name + ") missing");
            active = false;
          } else {
            checks_out.push_back("attr(" + name + ") present");
          }
          continue;
        }

        if (type == "media" || type == "container") {
          checks_out.push_back(type + " query not evaluated in demo");
          active = false;
          continue;
        }

        checks_out.push_back("condition type " + type + " treated as passthrough");
      }
    } else {
      checks_out.push_back("unconditional");
    }

    return active;
  };

  std::map<std::string, std::map<std::string, std::vector<AppliedDecl>>> provenance;
  if (style_ir.contains("rules") && style_ir.at("rules").is_array()) {
    size_t rule_index = 0;
    for (const auto& rule : style_ir.at("rules")) {
      ++rule_index;
      std::vector<std::string> checks;
      if (!evaluate_rule(rule, checks)) {
        continue;
      }
      const std::string target = rule.value("target", std::string("<missing-target>"));
      const std::string selector = rule.value("selector", std::string("&"));
      for (auto it = rule.at("decls").begin(); it != rule.at("decls").end(); ++it) {
        provenance[target][it.key()].push_back(
            AppliedDecl{rule_index, selector, it.value().get<std::string>(), checks});
      }
    }
  }

  std::ostringstream out;
  if (provenance.empty()) {
    out << "no active declarations\n";
  } else {
    for (const auto& [target, properties] : provenance) {
      out << target << "\n";
      for (const auto& [property, applied] : properties) {
        const AppliedDecl& winner = applied.back();
        out << "  " << property << " = " << winner.value << "\n";
        out << "    winner: #" << winner.rule_index << " selector=\"" << winner.selector
            << "\"\n";
        out << "    checks:\n";
        for (const auto& check : winner.checks) {
          out << "      - " << check << "\n";
        }
        if (applied.size() > 1) {
          out << "    overridden:\n";
          for (size_t i = 0; i + 1 < applied.size(); ++i) {
            out << "      - #" << applied[i].rule_index << " selector=\""
                << applied[i].selector << "\" value=" << applied[i].value << "\n";
          }
        }
      }
      out << "\n";
    }
  }

  if (style_ir.contains("media") && style_ir.at("media").is_array() &&
      !style_ir.at("media").empty()) {
    out << "media blocks skipped:\n";
    for (const auto& block : style_ir.at("media")) {
      out << "  - @media " << block.value("query", std::string()) << "\n";
    }
  }
  if (style_ir.contains("container") && style_ir.at("container").is_array() &&
      !style_ir.at("container").empty()) {
    out << "container blocks skipped:\n";
    for (const auto& block : style_ir.at("container")) {
      out << "  - @container ";
      if (block.contains("name") && !block.at("name").get<std::string>().empty()) {
        out << block.at("name").get<std::string>() << " ";
      }
      out << block.value("query", std::string()) << "\n";
    }
  }

  return trim_for_panel(out.str(), 520);
}

std::string summarize_style_provenance_runtime_diff(
    const Json& component_ir, const Json& style_ir, const Json& resolved_variants,
    const std::unordered_map<std::string, flexUI::Element*>& nodes) {
  struct AppliedDecl {
    size_t rule_index = 0;
    std::string selector;
    std::string value;
  };

  const Json variants = merge_demo_variants(component_ir, resolved_variants);
  auto evaluate_rule = [&](const Json& rule) {
    const std::string target = rule.value("target", std::string());
    const auto node_it = nodes.find(target);
    flexUI::Element* elem = node_it != nodes.end() ? node_it->second : nullptr;
    if (!rule.contains("when")) {
      return true;
    }
    for (const auto& condition : rule.at("when")) {
      const std::string type = condition.value("type", std::string());
      const std::string name = condition.value("name", std::string());
      if (type == "variant") {
        const auto it = variants.find(name);
        if (it == variants.end()) {
          return false;
        }
        const Json expected =
            condition.contains("value") ? condition.at("value") : condition.at("equals");
        if (*it != expected) {
          return false;
        }
        continue;
      }
      if (!elem) {
        return false;
      }
      if (type == "state") {
        const bool actual = elem->has_state(flexUI::Symbol(name));
        const Json expected =
            condition.contains("value") ? condition.at("value")
                                        : condition.value("equals", Json(true));
        const bool expected_bool = expected.is_boolean() ? expected.get<bool>() : true;
        if (actual != expected_bool) {
          return false;
        }
        continue;
      }
      if (type == "attr") {
        const std::string* value = elem->attribute(name);
        if (condition.contains("equals") || condition.contains("value")) {
          const Json expected_json =
              condition.contains("equals") ? condition.at("equals") : condition.at("value");
          const std::string expected = expected_json.is_string()
                                           ? expected_json.get<std::string>()
                                           : expected_json.dump();
          if (!value || *value != expected) {
            return false;
          }
        } else if (!value) {
          return false;
        }
        continue;
      }
      if (type == "media" || type == "container") {
        return false;
      }
    }
    return true;
  };

  std::map<std::string, std::map<std::string, AppliedDecl>> winners;
  if (style_ir.contains("rules") && style_ir.at("rules").is_array()) {
    size_t rule_index = 0;
    for (const auto& rule : style_ir.at("rules")) {
      ++rule_index;
      if (!evaluate_rule(rule)) {
        continue;
      }
      const std::string target = rule.value("target", std::string("<missing-target>"));
      const std::string selector = rule.value("selector", std::string("&"));
      for (auto it = rule.at("decls").begin(); it != rule.at("decls").end(); ++it) {
        winners[target][it.key()] = AppliedDecl{rule_index, selector,
                                                it.value().get<std::string>()};
      }
    }
  }

  std::ostringstream out;
  if (winners.empty()) {
    out << "no comparable active declarations\n";
    return trim_for_panel(out.str(), 520);
  }

  std::map<std::string, int> status_counts;
  std::map<std::string, int> semantic_group_counts;
  std::map<std::string, std::vector<std::string>> semantic_group_samples;

  for (const auto& [target, properties] : winners) {
    auto node_it = nodes.find(target);
    flexUI::Element* elem = node_it != nodes.end() ? node_it->second : nullptr;
    for (const auto& [property, winner] : properties) {
      const auto runtime = runtime_style_value_for_property(elem, property);
      const std::string normalized_provenance =
          normalize_property_value_for_compare(elem, property, winner.value);
      std::string status;
      std::string normalized_runtime;
      if (!runtime.has_value()) {
        status = "unmapped";
      } else {
        normalized_runtime = normalize_property_value_for_compare(elem, property, *runtime);
        if (*runtime == winner.value) {
          status = "match";
        } else if (normalized_runtime == normalized_provenance) {
          status = "normalized-match";
        } else if (contains_dynamic_css_function_local(winner.value) ||
                   contains_dynamic_css_function_local(normalized_provenance)) {
          status = "unresolved-token-diff";
        } else {
          status = "semantic-diff";
        }
      }
      status_counts[status]++;
      const std::string group = property_group_for_diff(property);
      if (status == "semantic-diff") {
        semantic_group_counts[group]++;
        auto& samples = semantic_group_samples[group];
        if (samples.size() < 4) {
          samples.push_back(target + "." + property);
        }
      }
    }
  }

  out << "summary\n";
  for (const auto& [status, count] : status_counts) {
    out << "  " << status << ": " << count << "\n";
  }
  if (!semantic_group_counts.empty()) {
    out << "semantic-diff groups\n";
    for (const auto& [group, count] : semantic_group_counts) {
      out << "  " << group << ": " << count;
      const auto sample_it = semantic_group_samples.find(group);
      if (sample_it != semantic_group_samples.end() && !sample_it->second.empty()) {
        out << "  [";
        for (size_t i = 0; i < sample_it->second.size(); ++i) {
          if (i) out << ", ";
          out << sample_it->second[i];
        }
        out << "]";
      }
      out << "\n";
    }
  }
  out << "\n";

  for (const auto& [target, properties] : winners) {
    out << target << "\n";
    auto node_it = nodes.find(target);
    flexUI::Element* elem = node_it != nodes.end() ? node_it->second : nullptr;
    for (const auto& [property, winner] : properties) {
      const auto runtime = runtime_style_value_for_property(elem, property);
      const std::string normalized_provenance =
          normalize_property_value_for_compare(elem, property, winner.value);
      out << "  " << property << "  [" << property_group_for_diff(property) << "]\n";
      out << "    provenance: " << winner.value << "  (#" << winner.rule_index
          << " \"" << winner.selector << "\")\n";
      out << "    normalized provenance: " << normalized_provenance << "\n";
      if (!runtime.has_value()) {
        out << "    runtime: <unmapped>\n";
        out << "    status: unmapped\n";
      } else {
        const std::string normalized_runtime =
            normalize_property_value_for_compare(elem, property, *runtime);
        out << "    runtime: " << *runtime << "\n";
        out << "    normalized runtime: " << normalized_runtime << "\n";
        if (*runtime == winner.value) {
          out << "    status: match\n";
        } else if (normalized_runtime == normalized_provenance) {
          out << "    status: normalized-match\n";
        } else if (contains_dynamic_css_function_local(winner.value) ||
                   contains_dynamic_css_function_local(normalized_provenance)) {
          out << "    status: unresolved-token-diff\n";
        } else {
          out << "    status: semantic-diff\n";
        }
      }
    }
    out << "\n";
  }

  return trim_for_panel(out.str(), 520);
}

std::string summarize_resolved_layout_snapshot(
    const Json& component_ir,
    const std::unordered_map<std::string, flexUI::Element*>& nodes) {
  std::unordered_map<std::string, Json> node_specs;
  std::unordered_map<std::string, std::vector<std::string>> children;
  if (component_ir.contains("nodes")) {
    for (const auto& node : component_ir.at("nodes")) {
      const std::string node_id = node.value("id", std::string());
      if (!node_id.empty()) {
        node_specs[node_id] = node;
      }
    }
  }
  if (component_ir.contains("edges")) {
    for (const auto& edge : component_ir.at("edges")) {
      const std::string parent = edge.value("parent", std::string());
      const std::string child = edge.value("child", std::string());
      if (!parent.empty() && !child.empty()) {
        children[parent].push_back(child);
      }
    }
  }

  auto format_box = [](const char* label, float a, float b, float c, float d) {
    std::ostringstream out;
    out << label << "(" << a << ", " << b << ", " << c << ", " << d << ")";
    return out.str();
  };

  std::ostringstream out;
  out << "root: " << component_ir.value("root", std::string("<missing>")) << "\n";

  std::function<void(const std::string&, int)> dump_node =
      [&](const std::string& node_id, int depth) {
        const auto node_it = node_specs.find(node_id);
        const auto elem_it = nodes.find(node_id);
        const std::string indent(static_cast<size_t>(depth) * 2, ' ');

        out << indent << "- " << node_id;
        if (node_it != node_specs.end()) {
          const auto& spec = node_it->second;
          const std::string widget = spec.value("widget", std::string());
          if (!widget.empty()) {
            out << " widget=" << widget;
          }
          const std::string slot = spec.value("slot", std::string());
          if (!slot.empty()) {
            out << " slot=" << slot;
          }
        }
        out << "\n";

        if (elem_it == nodes.end() || !elem_it->second) {
          out << indent << "  unresolved element\n";
          return;
        }

        auto* elem = elem_it->second;
        const auto* style = elem->computed_style;
        const float width = elem->layout_width();
        const float height = elem->layout_height();
        const float abs_x = elem->absolute_x();
        const float abs_y = elem->absolute_y();
        float padding[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float margin[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float border[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        if (style) {
          for (int i = 0; i < 4; ++i) {
            padding[i] = style->padding[i];
            margin[i] = style->margin[i];
            border[i] = style->border_width[i];
          }
        }
        const float content_w = std::max(0.0f, width - padding[1] - padding[3] - border[1] -
                                                    border[3]);
        const float content_h = std::max(0.0f, height - padding[0] - padding[2] - border[0] -
                                                     border[2]);

        out << indent << "  "
            << format_box("local", elem->x(), elem->y(), width, height) << "\n";
        out << indent << "  "
            << format_box("absolute", abs_x, abs_y, width, height) << "\n";
        out << indent << "  content(" << content_w << ", " << content_h << ")\n";
        out << indent << "  " << format_box("padding", padding[0], padding[1], padding[2],
                                            padding[3])
            << "\n";
        out << indent << "  "
            << format_box("border", border[0], border[1], border[2], border[3]) << "\n";
        out << indent << "  "
            << format_box("margin", margin[0], margin[1], margin[2], margin[3]) << "\n";

        if (elem->is_scroll_container() || elem->scroll_x() > 0.0f || elem->scroll_y() > 0.0f) {
          out << indent << "  scroll(offset=" << elem->scroll_x() << ", " << elem->scroll_y()
              << " content=" << elem->scroll_content_width() << "x"
              << elem->scroll_content_height() << " max=" << elem->max_scroll_x() << ", "
              << elem->max_scroll_y() << ")\n";
        }
        if (elem->clip()) {
          out << indent << "  clip(" << elem->clip_width() << ", " << elem->clip_height()
              << ")\n";
        }

        const auto child_it = children.find(node_id);
        if (child_it == children.end()) {
          return;
        }
        for (const auto& child_id : child_it->second) {
          dump_node(child_id, depth + 1);
        }
      };

  const std::string root_id = component_ir.value("root", std::string());
  if (!root_id.empty()) {
    dump_node(root_id, 0);
  }
  return trim_for_panel(out.str(), 520);
}

const char* DEMO_CSS = R"(
  * {
    box-sizing: border-box;
  }

  :root {
    font-family: NotoSansSC;
    --background: #0f172a;
    --foreground: #e2e8f0;
    --primary: #2563eb;
    --primary-foreground: #eff6ff;
    --destructive: #dc2626;
    --destructive-foreground: #fef2f2;
    --secondary: #1e293b;
    --secondary-foreground: #e2e8f0;
    --muted: #111827;
    --muted-foreground: #94a3b8;
    --accent: #1d4ed8;
    --accent-foreground: #eff6ff;
    --popover: #111827;
    --popover-foreground: #e5e7eb;
    --border: #334155;
    --input: #475569;
    --ring: #60a5fa;
    --warning: #f59e0b;
    --sidebar: #111827;
    --sidebar-foreground: #e5e7eb;
  }

  #demo-root {
    display: flex;
    flex-direction: row;
    width: 100%;
    height: 100%;
    gap: 24px;
    padding: 24px;
    background-color: #020617;
    color: var(--foreground);
    pointer-events: none;
  }

  #demo-column {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    gap: 16px;
    width: 520px;
  }

  .demo-panel {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    gap: 12px;
    padding: 16px;
    border-width: 1px;
    border-style: solid;
    border-color: rgba(148, 163, 184, 0.2);
    border-radius: 14px;
    background-color: rgba(15, 23, 42, 0.88);
  }

  .demo-title {
    font-size: 20px;
    font-weight: 700;
    color: #f8fafc;
  }

  .demo-copy {
    width: 100%;
    font-size: 13px;
    color: #94a3b8;
  }

  .demo-stack {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    gap: 12px;
  }

  .showroom-panel {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    gap: 16px;
    flex: 1 1 0;
    min-height: 0;
  }

  .showroom-grid {
    display: flex;
    flex-direction: row;
    flex-wrap: wrap;
    align-items: flex-start;
    gap: 16px;
    min-width: 0;
  }

  .showroom-column {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    gap: 16px;
    min-width: 0;
  }

  .showroom-column.flow-column {
    flex: 1 1 360px;
    width: auto;
  }

  .showroom-column.overlay-column {
    flex: 1 1 420px;
    min-width: 380px;
  }

  .showcase-card {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    gap: 12px;
    padding: 16px;
    border-width: 1px;
    border-style: solid;
    border-color: rgba(148, 163, 184, 0.2);
    border-radius: 14px;
    background-color: rgba(15, 23, 42, 0.88);
  }

  .showcase-host {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    justify-content: flex-start;
    gap: 8px;
    width: 100%;
    min-height: 52px;
    min-width: 0;
    padding: 12px;
    border-width: 1px;
    border-style: solid;
    border-color: rgba(96, 165, 250, 0.16);
    border-radius: 12px;
    background-color: rgba(2, 6, 23, 0.42);
  }

  .overlay-stage-row {
    display: flex;
    flex-direction: row;
    flex-wrap: wrap;
    align-items: stretch;
    gap: 12px;
  }

  .overlay-stage-host {
    flex: 1 1 220px;
    min-height: 132px;
    position: relative;
    border-style: dashed;
    border-color: rgba(96, 165, 250, 0.28);
    background-color: rgba(15, 23, 42, 0.58);
  }

  .demo-inline {
    display: flex;
    flex-direction: row;
    align-items: center;
    gap: 16px;
  }

  .demo-section {
    font-size: 12px;
    font-weight: 700;
    letter-spacing: 0.08em;
    text-transform: uppercase;
    color: #60a5fa;
  }

  .sample-note {
    font-size: 12px;
    color: #94a3b8;
  }

  .sample-chip {
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 0.06em;
    text-transform: uppercase;
    color: #bfdbfe;
  }

  .sample-meta {
    font-size: 11px;
    color: #64748b;
  }

  #button-host {
    width: 220px;
  }

  #input-host,
  #select-host,
  #dropdown-host,
  #searchbox-host,
  #tabs-host {
    width: 100%;
  }

  #sidebar-host {
    min-height: 180px;
  }

  #popover-host,
  #tooltip-host,
  #toast-host,
  #menu-host,
  #notification-host {
    min-height: 132px;
  }

  #notification-host {
    width: 100%;
    min-height: 96px;
    justify-content: flex-start;
    border-style: dashed;
  }

  #dialog-host {
    width: 100%;
    min-height: 380px;
    position: relative;
    border-width: 1px;
    border-style: dashed;
    border-color: rgba(96, 165, 250, 0.35);
    border-radius: 16px;
    background-color: rgba(15, 23, 42, 0.65);
  }

  #dialog-note {
    font-size: 12px;
    color: #93c5fd;
  }

  #workspace-column {
    flex: 1;
    display: flex;
    flex-direction: column;
    align-items: stretch;
    gap: 16px;
    min-width: 320px;
    min-height: 0;
  }

  #inspector-panel {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    gap: 12px;
    padding: 16px;
    border-width: 1px;
    border-style: solid;
    border-color: rgba(148, 163, 184, 0.2);
    border-radius: 16px;
    background-color: rgba(15, 23, 42, 0.88);
    min-height: 0;
    flex: 1 1 0;
  }

  #showroom-scroll {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    gap: 16px;
    flex: 1 1 0;
    min-height: 0;
    overflow-y: auto;
    padding-right: 4px;
  }

  #inspector-body {
    display: flex;
    flex-direction: column;
    align-items: stretch;
    gap: 12px;
    flex: 1;
    min-height: 0;
    overflow-y: auto;
  }

  .inspector-meta {
    width: 100%;
    font-size: 12px;
    color: #93c5fd;
  }

  .inspector-code {
    width: 100%;
    font-family: NotoSansSC;
    font-size: 12px;
    line-height: 1.45;
    color: #cbd5e1;
    white-space: pre-wrap;
    padding: 12px;
    border-width: 1px;
    border-style: solid;
    border-color: rgba(96, 165, 250, 0.16);
    border-radius: 12px;
    background-color: rgba(2, 6, 23, 0.82);
  }
)";

std::string trim_for_panel(const std::string& text, size_t limit) {
  if (text.size() <= limit) {
    return text;
  }
  return text.substr(0, limit) + "\n...";
}

}  // namespace

class ShadcnIRDemo final : public ::flex::GlfwApp {
 public:
  ShadcnIRDemo() : ::flex::GlfwApp("flexUI shadcn IR Demo", 1280, 860) {}

 protected:
  bool on_init() override {
    if (!load_demo_fonts()) {
      return false;
    }

    box_ = std::make_unique<flexUI::Box>(renderer());
    box_->set_viewport(static_cast<float>(width()), static_cast<float>(height()));
    build_ui();
    dump_layout_if_requested();
    return true;
  }

  void on_update(float dt) override {
    box_->update_time(dt * 1000.0f);
  }

  void on_render() override {
    box_->invalidate();
    box_->update();
    canvas()->draw();
    canvas()->sync();
  }

  void on_resize(int w, int h) override {
    ::flex::GlfwApp::on_resize(w, h);
    if (box_) {
      box_->set_viewport(static_cast<float>(w), static_cast<float>(h));
    }
  }

  void on_mouse_button(int button, int action, int mods) override {
    (void)button;
    (void)action;
    (void)mods;
  }

  void on_cursor_pos(double x, double y) override {
    (void)x;
    (void)y;
  }

  void on_key(int key, int action, int mods) override {
    (void)key;
    (void)action;
    (void)mods;
  }

  void on_char(unsigned int codepoint) override {
    (void)codepoint;
  }

 private:
  flexUI::Box* ime_box() override { return nullptr; }
  flexUI::Box* host_box() override { return nullptr; }

  void dump_layout_if_requested() const {
    const char* enabled = std::getenv("FLEXUI_SHADCN_DUMP_LAYOUT");
    if (!enabled || std::string(enabled) != "1") {
      return;
    }

    static constexpr const char* kNodeIds[] = {
        "demo-root",       "demo-column",     "workspace-column",
        "showroom-panel",  "showroom-scroll", "showroom-grid",
        "flow-column",     "overlay-column",  "primitives-card",
        "flow-title",      "button-host",      "input-host",
        "inspector-panel"};
    for (const char* id : kNodeIds) {
      const auto* elem = box_->query_selector(std::string("#") + id);
      if (!elem) {
        std::cerr << id << ": missing\n";
        continue;
      }
      std::cerr << id << ": x=" << elem->x() << " y=" << elem->y()
                << " width=" << elem->layout_width()
                << " height=" << elem->layout_height()
                << " grow=" << elem->flex_grow()
                << " shrink=" << elem->flex_shrink()
                << " basis=" << elem->flex_basis() << '\n';
    }
  }

  bool load_demo_fonts() {
    bool loaded = false;
    const fs::path bundled = repo_root() / "fonts" / "NotoSansSC-Regular.ttf";
    if (fs::exists(bundled) &&
        load_font("NotoSansSC", bundled.string().c_str())) {
      loaded = true;
    }
    if (!loaded && load_font("NotoSansSC", "C:/Windows/Fonts/msyh.ttc")) {
      loaded = true;
    }
    loaded |= load_font("Arial", "C:/Windows/Fonts/arial.ttf");
    loaded |= load_font("Segoe UI", "C:/Windows/Fonts/segoeui.ttf");
    if (!loaded) {
      loaded |= load_font("NotoSansSC", "C:/Windows/Fonts/arial.ttf");
    }
    return loaded;
  }

  struct InspectorTarget {
    flexUI::Element* root = nullptr;
    std::unordered_map<std::string, flexUI::Element*> nodes;
    std::string name;
    Json component;
    Json style;
    Json bridge;
    Json active_states;
    Json resolved_variants;
    std::string summary_title;
    std::string summary_text;
    std::string css_title;
    std::string css_text;
    std::string emitted_css_title;
    std::string emitted_css_text;
    std::string style_hits_title;
    std::string style_hits_text;
    std::string style_provenance_title;
    std::string style_provenance_text;
    std::string style_runtime_title;
    std::string style_runtime_text;
    std::string layout_title;
    std::string layout_text;
    std::string bridge_decisions_title;
    std::string bridge_decisions_text;
    std::string bridge_runtime_title;
    std::string node_tree_title;
    std::string node_tree_text;
    std::string component_title;
    std::string style_title;
    std::string bridge_title;
    std::string component_text;
    std::string style_text;
    std::string bridge_text;
  };

  void set_label_text(flexUI::Element* elem, const std::string& text) {
    if (!elem || !elem->widget) {
      return;
    }
    static_cast<flexUI::LabelWidget*>(elem->widget)->set_text(text);
    elem->mark_paint_dirty();
  }

  void register_inspector_target(const flexUI::shadcn_ir::InstantiatedTree& tree,
                                 flexUI::Element* root,
                                 const SampleBundle& bundle,
                                 const Json& props = Json::object(),
                                 const Json& active_states = Json::object(),
                                 const Json& resolved_variants = Json::object()) {
    if (!root) {
      return;
    }
    inspector_targets_.push_back(InspectorTarget{
        root,
        tree.nodes,
        bundle.name,
        bundle.component,
        bundle.style,
        bundle.bridge,
        active_states,
        resolved_variants,
        "resolved props/state",
        summarize_runtime_config(props, active_states),
        "resolved css scope/ids",
        summarize_css_targets(bundle.component),
        "emitted css",
        summarize_emitted_css(bundle.component, bundle.style, resolved_variants),
        "resolved style hits",
        summarize_resolved_style_hits(bundle.component, bundle.style, resolved_variants,
                                      tree.nodes),
        "resolved style provenance",
        summarize_resolved_style_provenance(bundle.component, bundle.style,
                                           resolved_variants, tree.nodes),
        "provenance vs runtime",
        summarize_style_provenance_runtime_diff(bundle.component, bundle.style,
                                                resolved_variants, tree.nodes),
        "resolved layout snapshot",
        summarize_resolved_layout_snapshot(bundle.component, tree.nodes),
        "resolved bridge decisions",
        summarize_bridge_decisions(bundle.bridge, active_states, tree.nodes),
        "resolved bridge host",
        "resolved node tree",
        summarize_resolved_node_tree(bundle.component, tree.nodes),
        bundle.name + ".component.json",
        bundle.name + ".style.json",
        bundle.name + ".bridge.json",
        bundle.component_text,
        bundle.style_text,
        bundle.bridge_text,
    });
  }

  flexUI::Element* wrap_sample_host(flexUI::Element* parent, const std::string& id,
                                    flexUI::Element* child) {
    auto* host = box_->create("div", id);
    host->add_class("showcase-host");
    host->append(child);
    parent->append(host);
    return host;
  }

  void append_sample_caption(flexUI::Element* host, const std::string& name,
                             const std::string& meta) {
    if (!host) {
      return;
    }
    auto* title = box_->create_widget<flexUI::LabelWidget>(
        "label", name + "-sample-chip", name);
    title->add_class("sample-chip");
    host->append(title);
    auto* meta_label = box_->create_widget<flexUI::LabelWidget>(
        "label", name + "-sample-meta", meta);
    meta_label->add_class("sample-meta");
    host->append(meta_label);
  }

  bool select_inspector_target(flexUI::Element* current) {
    for (const auto& target : inspector_targets_) {
      if (target.root != current) {
        continue;
      }

      set_label_text(inspector_meta_,
                     "当前预览目标: " + target.name +
                         "\n消费链: component.json -> style.json -> bridge.json -> instantiate/emit/apply");
      set_label_text(summary_label_, target.summary_title);
      set_label_text(summary_code_, target.summary_text);
      set_label_text(css_label_, target.css_title);
      set_label_text(css_code_, target.css_text);
      set_label_text(emitted_css_label_, target.emitted_css_title);
      set_label_text(emitted_css_code_, target.emitted_css_text);
      set_label_text(style_hits_label_, target.style_hits_title);
      set_label_text(style_hits_code_,
                     summarize_resolved_style_hits(target.component, target.style,
                                                   target.resolved_variants, target.nodes));
      set_label_text(style_provenance_label_, target.style_provenance_title);
      set_label_text(style_provenance_code_,
                     summarize_resolved_style_provenance(target.component, target.style,
                                                         target.resolved_variants,
                                                         target.nodes));
      set_label_text(style_runtime_label_, target.style_runtime_title);
      set_label_text(style_runtime_code_,
                     summarize_style_provenance_runtime_diff(target.component,
                                                             target.style,
                                                             target.resolved_variants,
                                                             target.nodes));
      set_label_text(layout_label_, target.layout_title);
      set_label_text(layout_code_,
                     summarize_resolved_layout_snapshot(target.component, target.nodes));
      set_label_text(bridge_decisions_label_, target.bridge_decisions_title);
      set_label_text(bridge_decisions_code_, target.bridge_decisions_text);
      set_label_text(bridge_runtime_label_, target.bridge_runtime_title);
      set_label_text(bridge_runtime_code_,
                     summarize_bridge_host_snapshot(target.bridge, target.active_states,
                                                   target.nodes));
      set_label_text(node_tree_label_, target.node_tree_title);
      set_label_text(node_tree_code_, target.node_tree_text);
      set_label_text(component_label_, target.component_title);
      set_label_text(component_code_, target.component_text);
      set_label_text(style_label_, target.style_title);
      set_label_text(style_code_, target.style_text);
      set_label_text(bridge_label_, target.bridge_title);
      set_label_text(bridge_code_, target.bridge_text);
      box_->invalidate();
      return true;
    }
    return false;
  }

  void build_ui() {
    const fs::path sample_dir = shadcn_sample_dir();
    const SampleBundle button = load_sample_bundle(sample_dir, "button");
    const SampleBundle input = load_sample_bundle(sample_dir, "input");
    const SampleBundle dialog = load_sample_bundle(sample_dir, "dialog");
    const SampleBundle select = load_sample_bundle(sample_dir, "select");
    const SampleBundle checkbox = load_sample_bundle(sample_dir, "checkbox");
    const SampleBundle radio = load_sample_bundle(sample_dir, "radio");
    const SampleBundle toggle = load_sample_bundle(sample_dir, "switch");
    const SampleBundle dropdown = load_sample_bundle(sample_dir, "dropdown");
    const SampleBundle popover = load_sample_bundle(sample_dir, "popover");
    const SampleBundle menu = load_sample_bundle(sample_dir, "menu");
    const SampleBundle toast = load_sample_bundle(sample_dir, "toast");
    const SampleBundle tooltip = load_sample_bundle(sample_dir, "tooltip");
    const SampleBundle searchbox = load_sample_bundle(sample_dir, "searchbox");
    const SampleBundle sidebar = load_sample_bundle(sample_dir, "sidebar");
    const SampleBundle tabs = load_sample_bundle(sample_dir, "tabs");
    const SampleBundle notification =
        load_sample_bundle(sample_dir, "notification");

    auto* root = box_->create("div", "demo-root");
    auto* column = box_->create("div", "demo-column");
    root->append(column);
    auto* workspace = box_->create("div", "workspace-column");
    root->append(workspace);

    auto* intro = box_->create("div");
    intro->add_class("demo-panel");
    auto* intro_title = box_->create_widget<flexUI::LabelWidget>(
        "label", "intro-title", "shadcn IR -> flexUI");
    intro_title->add_class("demo-title");
    intro->append(intro_title);
    auto* intro_copy = box_->create_widget<flexUI::LabelWidget>(
        "label", "intro-copy",
        "当前 demo 只作为静态 showroom，所有点击/键盘/hover 交互都已关闭；右侧 Inspector 默认展示 dialog 的 component/style/bridge 样例。");
    intro_copy->add_class("demo-copy");
    intro->append(intro_copy);
    column->append(intro);

    auto* controls = box_->create("div");
    controls->add_class("demo-panel");
    auto* controls_title = box_->create_widget<flexUI::LabelWidget>(
        "label", "controls-title", "Resolved Components");
    controls_title->add_class("demo-title");
    controls->append(controls_title);
    auto* controls_copy = box_->create_widget<flexUI::LabelWidget>(
        "label", "controls-copy",
        "右侧 showroom 按 flow / overlay 分列展示，避免混合布局和自动换行把组件墙撑散。");
    controls_copy->add_class("demo-copy");
    controls->append(controls_copy);
    column->append(controls);

    auto* showroom = box_->create("div", "showroom-panel");
    showroom->add_class("showroom-panel");
    workspace->append(showroom);

    auto* showroom_scroll = box_->create("div", "showroom-scroll");
    showroom->append(showroom_scroll);

    auto* showroom_grid = box_->create("div", "showroom-grid");
    showroom_grid->add_class("showroom-grid");
    showroom_scroll->append(showroom_grid);

    auto* flow_column = box_->create("div", "flow-column");
    flow_column->add_class("showroom-column");
    flow_column->add_class("flow-column");
    showroom_grid->append(flow_column);

    auto* overlay_column = box_->create("div", "overlay-column");
    overlay_column->add_class("showroom-column");
    overlay_column->add_class("overlay-column");
    showroom_grid->append(overlay_column);

    auto* flow_title = box_->create_widget<flexUI::LabelWidget>(
        "label", "flow-title", "Flow Components");
    flow_title->add_class("demo-section");
    flow_column->append(flow_title);

    auto* primitives_card = box_->create("div", "primitives-card");
    primitives_card->add_class("showcase-card");
    flow_column->append(primitives_card);
    auto* primitives_title = box_->create_widget<flexUI::LabelWidget>(
        "label", "primitives-title", "Form Primitives");
    primitives_title->add_class("demo-section");
    primitives_card->append(primitives_title);

    const Json button_props = json{{"text", "Save changes"}};
    const Json button_states = json{{"focus-visible", true}};
    const Json button_variants = json{{"variant", "default"}, {"size", "default"}};
    auto button_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, button.component, button_props);
    auto* button_host = wrap_sample_host(primitives_card, "button-host", button_tree.root);
    append_sample_caption(button_host, "button", "host 220px, static preview");
    register_inspector_target(button_tree, button_tree.root, button, button_props, button_states,
                              button_variants);

    const Json input_props = json{{"placeholder", "Work email"},
                                  {"text", "design@flexui.dev"},
                                  {"readonly", false}};
    const Json input_states = json{{"focus-visible", true}, {"read-only", false}};
    const Json input_variants = json{{"size", "default"}};
    auto input_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, input.component, input_props);
    auto* input_host = wrap_sample_host(primitives_card, "input-host", input_tree.root);
    append_sample_caption(input_host, "input", "host 100%, readonly=false");
    register_inspector_target(input_tree, input_tree.root, input, input_props, input_states,
                              input_variants);

    auto* selection_card = box_->create("div");
    selection_card->add_class("showcase-card");
    flow_column->append(selection_card);
    auto* selection_title = box_->create_widget<flexUI::LabelWidget>(
        "label", "selection-title", "Selection / Navigation");
    selection_title->add_class("demo-section");
    selection_card->append(selection_title);

    const Json select_props = json{{"options", json::array({"Alpha", "Beta", "Gamma"})},
                                   {"selected_index", 1},
                                   {"open", false}};
    const Json select_states = json{{"open", false}, {"focus-visible", true}};
    const Json select_variants = json{{"size", "default"}};
    auto select_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, select.component, select_props);
    auto* select_host = wrap_sample_host(selection_card, "select-host", select_tree.root);
    append_sample_caption(select_host, "select", "host 100%, open=false");
    register_inspector_target(select_tree, select_tree.root, select, select_props, select_states,
                              select_variants);

    const Json tabs_props = json{
        {"tabs", json::array({json{{"label", "General"}, {"id", "general"}},
                              json{{"label", "Billing"}, {"id", "billing"}}})},
        {"active_id", "billing"}};
    const Json tabs_states = json{{"focus-visible", true}};
    auto tabs_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, tabs.component, tabs_props);
    auto* tabs_host = wrap_sample_host(selection_card, "tabs-host", tabs_tree.root);
    append_sample_caption(tabs_host, "tabs", "host 100%, active=billing");
    register_inspector_target(tabs_tree, tabs_tree.root, tabs, tabs_props, tabs_states);

    const Json dropdown_props = json{{"placeholder", "Select framework"},
                                     {"selected_value", "vue"},
                                     {"open", false}};
    const Json dropdown_states = json{{"open", false}, {"focus-visible", true}};
    auto dropdown_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, dropdown.component, dropdown_props);
    auto* dropdown_host =
        wrap_sample_host(selection_card, "dropdown-host", dropdown_tree.root);
    append_sample_caption(dropdown_host, "dropdown", "host 100%, open=false");
    register_inspector_target(dropdown_tree, dropdown_tree.root, dropdown, dropdown_props,
                              dropdown_states);

    auto* inputs_card = box_->create("div");
    inputs_card->add_class("showcase-card");
    flow_column->append(inputs_card);
    auto* inputs_title = box_->create_widget<flexUI::LabelWidget>(
        "label", "inputs-title", "Toggles / Query");
    inputs_title->add_class("demo-section");
    inputs_card->append(inputs_title);

    auto* toggles = box_->create("div");
    toggles->add_class("demo-stack");
    inputs_card->append(toggles);

    const Json checkbox_props = json{{"label", "Accept terms"}, {"checked", true}};
    const Json checkbox_states = json{{"checked", true}, {"focus-visible", true}};
    auto checkbox_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, checkbox.component, checkbox_props);
    toggles->append(checkbox_tree.root);
    register_inspector_target(checkbox_tree, checkbox_tree.root, checkbox, checkbox_props,
                              checkbox_states);

    const Json radio_props = json{{"label", "Email"},
                                  {"value", "email"},
                                  {"group", "contact"},
                                  {"checked", true}};
    const Json radio_states = json{{"checked", true}};
    auto radio_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, radio.component, radio_props);
    toggles->append(radio_tree.root);
    register_inspector_target(radio_tree, radio_tree.root, radio, radio_props, radio_states);

    const Json switch_props = json{{"label", "Airplane mode"}, {"checked", true}};
    const Json switch_states = json{{"checked", true}, {"focus-visible", true}};
    auto switch_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, toggle.component, switch_props);
    toggles->append(switch_tree.root);
    register_inspector_target(switch_tree, switch_tree.root, toggle, switch_props, switch_states);

    const Json searchbox_props = json{{"text", "api"}, {"open", false}};
    const Json searchbox_states = json{{"open", false}, {"with-query", true}};
    auto searchbox_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, searchbox.component, searchbox_props);
    auto* searchbox_host =
        wrap_sample_host(inputs_card, "searchbox-host", searchbox_tree.root);
    append_sample_caption(searchbox_host, "searchbox", "host 100%, query=api");
    register_inspector_target(searchbox_tree, searchbox_tree.root, searchbox, searchbox_props,
                              searchbox_states);

    auto* overlay_title = box_->create_widget<flexUI::LabelWidget>(
        "label", "overlay-title", "Overlay / Fixed / Absolute");
    overlay_title->add_class("demo-section");
    overlay_column->append(overlay_title);

    auto* overlay_card = box_->create("div");
    overlay_card->add_class("showcase-card");
    overlay_column->append(overlay_card);
    auto* overlay_card_title = box_->create_widget<flexUI::LabelWidget>(
        "label", "overlay-card-title", "Popover / Tooltip / Toast / Menu");
    overlay_card_title->add_class("demo-section");
    overlay_card->append(overlay_card_title);
    auto* overlay_copy = box_->create_widget<flexUI::LabelWidget>(
        "label", "overlay-copy",
        "overlay/fixed 类样例默认闭合或静默，先保证 showroom 的稳定布局，不再让浮层覆盖整页。");
    overlay_copy->add_class("demo-copy");
    overlay_card->append(overlay_copy);
    auto* overlay_stage_row = box_->create("div");
    overlay_stage_row->add_class("overlay-stage-row");
    overlay_card->append(overlay_stage_row);

    const Json popover_props = json{{"title", "Share"},
                                    {"content", "Invite your team"},
                                    {"position", "right"},
                                    {"open", false}};
    const Json popover_states = json{{"open", false}, {"position", "right"}};
    auto popover_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, popover.component, popover_props);
    auto* popover_host = wrap_sample_host(overlay_stage_row, "popover-host", popover_tree.root);
    popover_host->add_class("overlay-stage-host");
    append_sample_caption(popover_host, "popover", "stage 220px+, right, closed");
    register_inspector_target(popover_tree, popover_tree.root, popover, popover_props,
                              popover_states);

    const Json tooltip_props = json{{"text", "Generated from tooltip IR"},
                                    {"position", "right"},
                                    {"open", false}};
    const Json tooltip_states = json{{"open", false}, {"right", true}};
    auto tooltip_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, tooltip.component, tooltip_props);
    auto* tooltip_host = wrap_sample_host(overlay_stage_row, "tooltip-host", tooltip_tree.root);
    tooltip_host->add_class("overlay-stage-host");
    append_sample_caption(tooltip_host, "tooltip", "stage 220px+, right, closed");
    register_inspector_target(tooltip_tree, tooltip_tree.root, tooltip, tooltip_props,
                              tooltip_states);

    const Json toast_props = json{{"message", "Changes synced to workspace"},
                                  {"type", "success"},
                                  {"duration", 6000},
                                  {"open", false}};
    const Json toast_states = json{{"open", false}, {"type", "success"}};
    auto toast_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, toast.component, toast_props);
    auto* toast_host = wrap_sample_host(overlay_stage_row, "toast-host", toast_tree.root);
    toast_host->add_class("overlay-stage-host");
    append_sample_caption(toast_host, "toast", "stage 220px+, fixed, closed");
    register_inspector_target(toast_tree, toast_tree.root, toast, toast_props, toast_states);

    const Json menu_props = json{{"open", false}, {"x", 0}, {"y", 0}};
    const Json menu_states = json{{"open", false}};
    auto menu_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, menu.component, menu_props);
    auto* menu_host = wrap_sample_host(overlay_stage_row, "menu-host", menu_tree.root);
    menu_host->add_class("overlay-stage-host");
    append_sample_caption(menu_host, "menu", "stage 220px+, absolute, closed");
    register_inspector_target(menu_tree, menu_tree.root, menu, menu_props, menu_states);

    auto* surface_card = box_->create("div");
    surface_card->add_class("showcase-card");
    overlay_column->append(surface_card);
    auto* surface_title = box_->create_widget<flexUI::LabelWidget>(
        "label", "surface-title", "Sidebar / Notification / Dialog");
    surface_title->add_class("demo-section");
    surface_card->append(surface_title);
    auto* surface_stage_row = box_->create("div");
    surface_stage_row->add_class("overlay-stage-row");
    surface_card->append(surface_stage_row);

    const Json sidebar_props = json{{"selected_id", "search"}, {"collapsed", false}};
    const Json sidebar_states = json{{"selected", true}, {"collapsed", false}};
    auto sidebar_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, sidebar.component, sidebar_props);
    auto* sidebar_host = wrap_sample_host(surface_stage_row, "sidebar-host", sidebar_tree.root);
    sidebar_host->add_class("overlay-stage-host");
    append_sample_caption(sidebar_host, "sidebar", "stage 220px+, selected=search");
    register_inspector_target(sidebar_tree, sidebar_tree.root, sidebar, sidebar_props,
                              sidebar_states);

    const Json notification_props = json{
        {"position", "top-right"},
        {"notifications",
         json::array({json{{"title", "Queued"},
                       {"message", "IR showroom ready"},
                       {"type", "info"}}})}};
    const Json notification_states = json{{"position", "top-right"},
                                          {"visible", false}};
    auto notification_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, notification.component, notification_props);
    auto* notification_host =
        wrap_sample_host(surface_stage_row, "notification-host", notification_tree.root);
    notification_host->add_class("overlay-stage-host");
    append_sample_caption(notification_host, "notification", "stage 220px+, top-right, hidden");
    auto* notification_note = box_->create_widget<flexUI::LabelWidget>(
        "label", "notification-note",
        "notification 当前样例主要验证 host bridge 和位置语义，root 本身是极小占位尺寸。");
    notification_note->add_class("sample-note");
    notification_host->append(notification_note);
    register_inspector_target(notification_tree, notification_tree.root, notification,
                              notification_props, notification_states);

    auto* dialog_stage = box_->create("div", "dialog-host");
    dialog_stage->add_class("showcase-host");
    auto* dialog_note = box_->create_widget<flexUI::LabelWidget>(
        "label", "dialog-note",
        "右侧 Dialog 同样来自 IR；左侧 flow 组件与 overlay 组件现在分区展示。");
    dialog_stage->append(dialog_note);
    append_sample_caption(dialog_stage, "dialog", "stage 100%, modal, closed");
    surface_stage_row->append(dialog_stage);

    const Json dialog_props = json{{"title", "Delete item"},
                                   {"message", "This action cannot be undone."},
                                   {"open", false}};
    const Json dialog_states = json{{"open", false}, {"closed", true}};
    const Json dialog_variants = json{{"tone", "default"}, {"size", "default"}};
    auto dialog_tree = flexUI::shadcn_ir::instantiate_component_tree(
        *box_, dialog.component, dialog_props);
    dialog_stage->append(dialog_tree.root);
    register_inspector_target(dialog_tree, dialog_stage, dialog, dialog_props, dialog_states,
                              dialog_variants);

    auto* inspector = box_->create("div", "inspector-panel");
    workspace->append(inspector);

    auto* inspector_title = box_->create_widget<flexUI::LabelWidget>(
        "label", "inspector-title", "IR Inspector");
    inspector_title->add_class("demo-title");
    inspector->append(inspector_title);

    auto* inspector_body = box_->create("div", "inspector-body");
    inspector->append(inspector_body);

    inspector_meta_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "inspector-meta",
        "当前预览目标: dialog\n消费链: component.json -> style.json -> bridge.json -> instantiate/emit/apply");
    inspector_meta_->add_class("inspector-meta");
    inspector_body->append(inspector_meta_);

    summary_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "summary-label", "resolved props/state");
    summary_label_->add_class("demo-section");
    inspector_body->append(summary_label_);
    summary_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "summary-code", summarize_runtime_config(dialog_props, dialog_states));
    summary_code_->add_class("inspector-code");
    inspector_body->append(summary_code_);

    css_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "css-label", "resolved css scope/ids");
    css_label_->add_class("demo-section");
    inspector_body->append(css_label_);
    css_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "css-code", summarize_css_targets(dialog.component));
    css_code_->add_class("inspector-code");
    inspector_body->append(css_code_);

    emitted_css_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "emitted-css-label", "emitted css");
    emitted_css_label_->add_class("demo-section");
    inspector_body->append(emitted_css_label_);
    emitted_css_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "emitted-css-code",
        summarize_emitted_css(dialog.component, dialog.style, dialog_variants));
    emitted_css_code_->add_class("inspector-code");
    inspector_body->append(emitted_css_code_);

    style_hits_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "style-hits-label", "resolved style hits");
    style_hits_label_->add_class("demo-section");
    inspector_body->append(style_hits_label_);
    style_hits_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "style-hits-code",
        summarize_resolved_style_hits(dialog.component, dialog.style, dialog_variants,
                                      dialog_tree.nodes));
    style_hits_code_->add_class("inspector-code");
    inspector_body->append(style_hits_code_);

    style_provenance_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "style-provenance-label", "resolved style provenance");
    style_provenance_label_->add_class("demo-section");
    inspector_body->append(style_provenance_label_);
    style_provenance_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "style-provenance-code",
        summarize_resolved_style_provenance(dialog.component, dialog.style, dialog_variants,
                                            dialog_tree.nodes));
    style_provenance_code_->add_class("inspector-code");
    inspector_body->append(style_provenance_code_);

    style_runtime_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "style-runtime-label", "provenance vs runtime");
    style_runtime_label_->add_class("demo-section");
    inspector_body->append(style_runtime_label_);
    style_runtime_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "style-runtime-code",
        summarize_style_provenance_runtime_diff(dialog.component, dialog.style,
                                                dialog_variants, dialog_tree.nodes));
    style_runtime_code_->add_class("inspector-code");
    inspector_body->append(style_runtime_code_);

    layout_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "layout-label", "resolved layout snapshot");
    layout_label_->add_class("demo-section");
    inspector_body->append(layout_label_);
    layout_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "layout-code",
        summarize_resolved_layout_snapshot(dialog.component, dialog_tree.nodes));
    layout_code_->add_class("inspector-code");
    inspector_body->append(layout_code_);

    bridge_decisions_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "bridge-decisions-label", "resolved bridge decisions");
    bridge_decisions_label_->add_class("demo-section");
    inspector_body->append(bridge_decisions_label_);
    bridge_decisions_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "bridge-decisions-code",
        summarize_bridge_decisions(dialog.bridge, dialog_states, dialog_tree.nodes));
    bridge_decisions_code_->add_class("inspector-code");
    inspector_body->append(bridge_decisions_code_);

    bridge_runtime_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "bridge-runtime-label", "resolved bridge host");
    bridge_runtime_label_->add_class("demo-section");
    inspector_body->append(bridge_runtime_label_);
    bridge_runtime_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "bridge-runtime-code",
        summarize_bridge_host_snapshot(dialog.bridge, dialog_states, dialog_tree.nodes));
    bridge_runtime_code_->add_class("inspector-code");
    inspector_body->append(bridge_runtime_code_);

    node_tree_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "node-tree-label", "resolved node tree");
    node_tree_label_->add_class("demo-section");
    inspector_body->append(node_tree_label_);
    node_tree_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "node-tree-code",
        summarize_resolved_node_tree(dialog.component, dialog_tree.nodes));
    node_tree_code_->add_class("inspector-code");
    inspector_body->append(node_tree_code_);

    component_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "component-label", "dialog.component.json");
    component_label_->add_class("demo-section");
    inspector_body->append(component_label_);
    component_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "component-code", dialog.component_text);
    component_code_->add_class("inspector-code");
    inspector_body->append(component_code_);

    style_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "style-label", "dialog.style.json");
    style_label_->add_class("demo-section");
    inspector_body->append(style_label_);
    style_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "style-code", dialog.style_text);
    style_code_->add_class("inspector-code");
    inspector_body->append(style_code_);

    bridge_label_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "bridge-label", "dialog.bridge.json");
    bridge_label_->add_class("demo-section");
    inspector_body->append(bridge_label_);
    bridge_code_ = box_->create_widget<flexUI::LabelWidget>(
        "label", "bridge-code", dialog.bridge_text);
    bridge_code_->add_class("inspector-code");
    inspector_body->append(bridge_code_);

    box_->set_root(root);
    box_->load_css(DEMO_CSS);
    box_->load_css(flexUI::shadcn_ir::emit_css(
        button.component, button.style,
        json{{"variant", "default"}, {"size", "default"}}));
    box_->load_css(flexUI::shadcn_ir::emit_css(
        input.component, input.style, json{{"size", "default"}}));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(checkbox.component, checkbox.style));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(radio.component, radio.style));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(toggle.component, toggle.style));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(searchbox.component, searchbox.style));
    box_->load_css(flexUI::shadcn_ir::emit_css(
        select.component, select.style, json{{"size", "default"}}));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(tabs.component, tabs.style));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(dropdown.component, dropdown.style));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(popover.component, popover.style));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(toast.component, toast.style));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(menu.component, menu.style));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(tooltip.component, tooltip.style));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(sidebar.component, sidebar.style));
    box_->load_css(
        flexUI::shadcn_ir::emit_css(notification.component, notification.style));
    box_->load_css(flexUI::shadcn_ir::emit_css(
        dialog.component, dialog.style,
        json{{"tone", "default"}, {"size", "default"}}));

    flexUI::shadcn_ir::apply_bridge_states(button.bridge, button_states, button_tree);
    flexUI::shadcn_ir::apply_bridge_states(input.bridge, input_states, input_tree);
    flexUI::shadcn_ir::apply_bridge_states(checkbox.bridge, checkbox_states, checkbox_tree);
    flexUI::shadcn_ir::apply_bridge_states(radio.bridge, radio_states, radio_tree);
    flexUI::shadcn_ir::apply_bridge_states(toggle.bridge, switch_states, switch_tree);
    flexUI::shadcn_ir::apply_bridge_states(searchbox.bridge, searchbox_states, searchbox_tree);
    flexUI::shadcn_ir::apply_bridge_states(select.bridge, select_states, select_tree);
    flexUI::shadcn_ir::apply_bridge_states(tabs.bridge, tabs_states, tabs_tree);
    flexUI::shadcn_ir::apply_bridge_states(dropdown.bridge, dropdown_states, dropdown_tree);
    flexUI::shadcn_ir::apply_bridge_states(popover.bridge, popover_states, popover_tree);
    flexUI::shadcn_ir::apply_bridge_states(menu.bridge, menu_states, menu_tree);
    flexUI::shadcn_ir::apply_bridge_states(tooltip.bridge, tooltip_states, tooltip_tree);
    flexUI::shadcn_ir::apply_bridge_states(toast.bridge, toast_states, toast_tree);
    flexUI::shadcn_ir::apply_bridge_states(sidebar.bridge, sidebar_states, sidebar_tree);
    flexUI::shadcn_ir::apply_bridge_states(notification.bridge, notification_states,
                                           notification_tree);
    flexUI::shadcn_ir::apply_bridge_states(dialog.bridge, dialog_states, dialog_tree);

    select_inspector_target(dialog_stage);
    box_->update();
  }

  std::unique_ptr<flexUI::Box> box_;
  std::vector<InspectorTarget> inspector_targets_;
  flexUI::Element* inspector_meta_ = nullptr;
  flexUI::Element* summary_label_ = nullptr;
  flexUI::Element* summary_code_ = nullptr;
  flexUI::Element* css_label_ = nullptr;
  flexUI::Element* css_code_ = nullptr;
  flexUI::Element* emitted_css_label_ = nullptr;
  flexUI::Element* emitted_css_code_ = nullptr;
  flexUI::Element* style_hits_label_ = nullptr;
  flexUI::Element* style_hits_code_ = nullptr;
  flexUI::Element* style_provenance_label_ = nullptr;
  flexUI::Element* style_provenance_code_ = nullptr;
  flexUI::Element* style_runtime_label_ = nullptr;
  flexUI::Element* style_runtime_code_ = nullptr;
  flexUI::Element* layout_label_ = nullptr;
  flexUI::Element* layout_code_ = nullptr;
  flexUI::Element* bridge_decisions_label_ = nullptr;
  flexUI::Element* bridge_decisions_code_ = nullptr;
  flexUI::Element* bridge_runtime_label_ = nullptr;
  flexUI::Element* bridge_runtime_code_ = nullptr;
  flexUI::Element* node_tree_label_ = nullptr;
  flexUI::Element* node_tree_code_ = nullptr;
  flexUI::Element* component_label_ = nullptr;
  flexUI::Element* component_code_ = nullptr;
  flexUI::Element* style_label_ = nullptr;
  flexUI::Element* style_code_ = nullptr;
  flexUI::Element* bridge_label_ = nullptr;
  flexUI::Element* bridge_code_ = nullptr;
};

int main() {
  ShadcnIRDemo demo;
  if (!demo.init()) {
    return 1;
  }
  demo.run();
  return 0;
}
