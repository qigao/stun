/*
 * flexUI - Box Implementation
 */

#include <flexUI/box.h>
#include <flexUI/renderer.h>
#include <flexUI/layout_manager.h>
#include <flexUI/render_manager.h>
#include <flexUI/view_pipeline.h>
#include <flexUI/tailwindcss.h>
#include <flexUI/detail/css_typed_value.h>
#include "default_style_assets.h"
#include <flex/bridge/renderer.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cctype>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace flexUI {

namespace {

CssLoadOptions tailwind_jit_load_options() {
  CssLoadOptions options;
  options.source = "<tailwind-jit>";
  options.strict = true;
  return options;
}

CssLoadOptions default_theme_load_options() {
  CssLoadOptions options;
  options.source = "<flexui-default-theme>";
  options.strict = true;
  return options;
}

Color resolved_element_background(const Element& elem,
                                  const ComputedStyle& style) {
  return elem.is_widget_owned()
             ? style.background_color
             : style.get_variable_color("--bg", style.background_color);
}

const Symbol& sticky_base_x_var() {
  static const Symbol sym("__flex_sticky_base_x");
  return sym;
}

const Symbol& sticky_base_y_var() {
  static const Symbol sym("__flex_sticky_base_y");
  return sym;
}

bool is_scroll_overflow(Overflow value) {
  return value == Overflow::Auto || value == Overflow::Scroll;
}

float parse_cached_position(const std::string& value, float fallback) {
  if (value.empty()) {
    return fallback;
  }
  char* end = nullptr;
  const float parsed = std::strtof(value.c_str(), &end);
  return end == value.c_str() ? fallback : parsed;
}

std::string format_cached_position(float value) {
  std::ostringstream out;
  out << value;
  return out.str();
}

float element_content_width(const Element* elem) {
  if (!elem) {
    return 0.0f;
  }

  float width = elem->layout_width();
  if (auto* style = elem->computed_style) {
    width -= style->padding[1] + style->padding[3] + style->border_width[1] +
             style->border_width[3];
  }
  return std::max(width, 0.0f);
}

bool is_inline_size_container(const Element* elem) {
  return elem && elem->computed_style &&
         elem->computed_style->get_variable(Symbol("container-type"), "") ==
             "inline-size";
}

bool is_visible_in_tree(const Element* elem) {
  for (const Element* current = elem; current;
       current = current->parent_elem()) {
    if (!current->is_visible()) {
      return false;
    }
  }
  return true;
}

void mark_visible_active_effects_dirty(Element* elem,
                                       TransitionManager& transitions,
                                       AnimationManager& animations,
                                       float time_ms) {
  if (!elem || !elem->is_visible()) {
    return;
  }

  const auto element_id = reinterpret_cast<std::uintptr_t>(elem);
  if (transitions.has_active(element_id, time_ms) ||
      animations.has_active(element_id, time_ms)) {
    elem->mark_paint_dirty();
  }

  for (auto* child : elem->children()) {
    mark_visible_active_effects_dirty(static_cast<Element*>(child), transitions,
                                      animations, time_ms);
  }
}

void snapshot_container_widths(
    const Element* elem, std::unordered_map<const Element*, float>& widths) {
  if (!elem) {
    return;
  }

  if (is_inline_size_container(elem)) {
    widths[elem] = element_content_width(elem);
  }

  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      snapshot_container_widths(child, widths);
    }
  }
}

bool reconcile_container_queries(
    Element* root, std::unordered_map<const Element*, float>& previous) {
  if (!root) {
    return false;
  }

  std::unordered_map<const Element*, float> current;
  snapshot_container_widths(root, current);

  bool changed = false;
  for (const auto& [elem, width] : current) {
    const auto it = previous.find(elem);
    if (it == previous.end() || std::fabs(it->second - width) > 0.001f) {
      const_cast<Element*>(elem)->mark_style_dirty();
      changed = true;
    }
  }

  for (const auto& [elem, width] : previous) {
    if (current.find(elem) == current.end()) {
      const_cast<Element*>(elem)->mark_style_dirty();
      changed = true;
    }
  }

  previous = std::move(current);
  return changed;
}

void snapshot_sticky_layout(Element* elem) {
  if (!elem) {
    return;
  }

  auto* style = elem->computed_style;
  if (style && style->position == Position::Sticky) {
    style->variables[sticky_base_x_var()] = format_cached_position(elem->x());
    style->variables[sticky_base_y_var()] = format_cached_position(elem->y());
  }

  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      snapshot_sticky_layout(child);
    }
  }
}

void sync_layout_dependent_widget_semantics(Element* elem) {
  if (!elem) {
    return;
  }

  if (elem->widget) {
    elem->widget->sync_host_semantics_for_layout(*elem);
  }

  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      sync_layout_dependent_widget_semantics(child);
    }
  }
}

Element* nearest_scroll_ancestor(Element* elem, bool horizontal) {
  for (auto* parent = elem ? elem->parent_elem() : nullptr; parent;
       parent = parent->parent_elem()) {
    auto* style = parent->computed_style;
    if (!style) {
      continue;
    }
    const Overflow axis_overflow =
        horizontal ? style->overflow_x : style->overflow_y;
    if (is_scroll_overflow(axis_overflow)) {
      return parent;
    }
  }
  return nullptr;
}

void adjust_fixed_layout(Element* elem, float viewport_width, float viewport_height) {
  if (!elem) return;

  auto* style = elem->computed_style;
  if (style && style->position == Position::Fixed) {
    float width = elem->layout_width();
    float height = elem->layout_height();
    const bool auto_width = style->width_size.kind == CssSizeKind::Auto;
    const bool auto_height = style->height_size.kind == CssSizeKind::Auto;

    if (style->width_is_percent && style->width > 0) {
      width = viewport_width * style->width / 100.0f;
      elem->set_layout_width(width);
    }
    if (style->height_is_percent && style->height > 0) {
      height = viewport_height * style->height / 100.0f;
      elem->set_layout_height(height);
    }

    if (!std::isnan(style->left) && !std::isnan(style->right) && auto_width) {
      width = viewport_width - style->left - style->right;
      elem->set_layout_width(width);
    }
    if (!std::isnan(style->top) && !std::isnan(style->bottom) && auto_height) {
      height = viewport_height - style->top - style->bottom;
      elem->set_layout_height(height);
    }

    float x = elem->x();
    float y = elem->y();
    if (!std::isnan(style->left)) {
      x = style->left;
    } else if (!std::isnan(style->right)) {
      x = viewport_width - style->right - width;
    }

    if (!std::isnan(style->top)) {
      y = style->top;
    } else if (!std::isnan(style->bottom)) {
      y = viewport_height - style->bottom - height;
    }

    elem->set_x(x);
    elem->set_y(y);
  }

  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      adjust_fixed_layout(child, viewport_width, viewport_height);
    }
  }
}

void adjust_sticky_layout(Element* elem) {
  if (!elem) {
    return;
  }

  auto* style = elem->computed_style;
  if (style && style->position == Position::Sticky) {
    const float base_x = parse_cached_position(
        style->get_variable(sticky_base_x_var(), ""), elem->x());
    const float base_y = parse_cached_position(
        style->get_variable(sticky_base_y_var(), ""), elem->y());

    elem->set_x(base_x);
    elem->set_y(base_y);
    const float base_abs_x = elem->absolute_x();
    const float base_abs_y = elem->absolute_y();

    float sticky_x = base_x;
    float sticky_y = base_y;

    if (!std::isnan(style->top)) {
      if (Element* scroll_parent = nearest_scroll_ancestor(elem, false)) {
        const float desired_abs_y =
            std::max(base_abs_y, scroll_parent->absolute_y() + style->top);
        sticky_y = base_y + (desired_abs_y - base_abs_y);
      }
    }

    if (!std::isnan(style->left)) {
      if (Element* scroll_parent = nearest_scroll_ancestor(elem, true)) {
        const float desired_abs_x =
            std::max(base_abs_x, scroll_parent->absolute_x() + style->left);
        sticky_x = base_x + (desired_abs_x - base_abs_x);
      }
    }

    elem->set_x(sticky_x);
    elem->set_y(sticky_y);
  }

  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      adjust_sticky_layout(child);
    }
  }
}

void adjust_positioned_layout(Element* elem, float viewport_width,
                              float viewport_height) {
  adjust_fixed_layout(elem, viewport_width, viewport_height);
  adjust_sticky_layout(elem);
}

void start_transition_if_changed(TransitionManager& transitions,
                                 std::uintptr_t element_id,
                                 const std::string& property,
                                 float previous_value,
                                 float target_value,
                                 const TransitionDef& def,
                                 float current_time_ms,
                                 bool& started_any) {
  const float current_value =
      transitions.get(element_id, property, previous_value, current_time_ms);
  if (std::fabs(current_value - target_value) > 0.0001f) {
    transitions.start(element_id, property, current_value, target_value, def,
                      current_time_ms);
    started_any = true;
  }
}

void start_color_transition_if_changed(TransitionManager& transitions,
                                       std::uintptr_t element_id,
                                       const std::string& property_name,
                                       const Color& previous_value,
                                       const Color& target_value,
                                       const TransitionDef& def,
                                       float current_time_ms,
                                       bool& started_any) {
  const auto* property = detail::style_property_find(property_name);
  if (!property) {
    return;
  }
  if (transitions.start_color(element_id, *property, previous_value,
                              target_value, def, current_time_ms)) {
    started_any = true;
  }
}

bool animation_spec_changed(const ComputedStyle& previous_style,
                            const ComputedStyle& current_style) {
  const auto entries_equal_ignoring_play_state =
      [](const std::vector<AnimationStyleEntry>& lhs,
         const std::vector<AnimationStyleEntry>& rhs) {
        if (lhs.size() != rhs.size()) {
          return false;
        }
        for (size_t i = 0; i < lhs.size(); ++i) {
          if (!lhs[i].equals_ignoring_play_state(rhs[i])) {
            return false;
          }
        }
        return true;
      };

  return previous_style.animation_name != current_style.animation_name ||
         std::fabs(previous_style.animation_duration_ms -
                   current_style.animation_duration_ms) > 0.001f ||
         std::fabs(previous_style.animation_delay_ms -
                   current_style.animation_delay_ms) > 0.001f ||
         previous_style.animation_timing != current_style.animation_timing ||
         std::fabs(previous_style.animation_iteration_count -
                   current_style.animation_iteration_count) > 0.001f ||
         previous_style.animation_infinite != current_style.animation_infinite ||
         previous_style.animation_fill_mode != current_style.animation_fill_mode ||
         previous_style.animation_direction != current_style.animation_direction ||
         !entries_equal_ignoring_play_state(previous_style.animations,
                                            current_style.animations);
}

bool animation_play_state_changed(const ComputedStyle& previous_style,
                                  const ComputedStyle& current_style) {
  if (previous_style.animation_play_state != current_style.animation_play_state) {
    return true;
  }
  if (previous_style.animations.size() != current_style.animations.size()) {
    return false;
  }
  for (size_t i = 0; i < previous_style.animations.size(); ++i) {
    if (previous_style.animations[i].play_state !=
        current_style.animations[i].play_state) {
      return true;
    }
  }
  return false;
}

std::string trim_copy(const std::string& value) {
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

Color parse_animation_color(const std::string& value) {
  const std::string trimmed = trim_copy(value);
  if (trimmed.empty()) {
    return {0.0f, 0.0f, 0.0f, 0.0f};
  }
  if (trimmed[0] == '#') {
    std::string hex = trimmed.substr(1);
    if (hex.size() == 3) {
      hex = std::string(2, hex[0]) + std::string(2, hex[1]) +
            std::string(2, hex[2]);
    }
    if (hex.size() == 6) {
      unsigned int r, g, b;
      std::sscanf(hex.c_str(), "%2x%2x%2x", &r, &g, &b);
      return {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f};
    }
    if (hex.size() == 8) {
      unsigned int r, g, b, a;
      std::sscanf(hex.c_str(), "%2x%2x%2x%2x", &r, &g, &b, &a);
      return {r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f};
    }
  }
  if (trimmed.find("rgba") == 0 || trimmed.find("rgb") == 0) {
    const size_t start = trimmed.find('(');
    const size_t end = trimmed.find(')');
    if (start != std::string::npos && end != std::string::npos && end > start) {
      std::string inner = trimmed.substr(start + 1, end - start - 1);
      for (char& ch : inner) {
        if (ch == ',') {
          ch = ' ';
        }
      }
      std::istringstream stream(inner);
      int r = 0;
      int g = 0;
      int b = 0;
      float a = 1.0f;
      stream >> r >> g >> b;
      if (trimmed.find("rgba") == 0) {
        stream >> a;
      }
      return {r / 255.0f, g / 255.0f, b / 255.0f, a};
    }
  }
  return {0.0f, 0.0f, 0.0f, 1.0f};
}

float parse_animation_length(const std::string& value) {
  const std::string trimmed = trim_copy(value);
  if (trimmed.empty()) {
    return 0.0f;
  }
  if (trimmed.size() >= 2 &&
      trimmed.compare(trimmed.size() - 2, 2, "px") == 0) {
    return std::stof(trimmed.substr(0, trimmed.size() - 2));
  }
  return std::stof(trimmed);
}

float parse_animation_angle_degrees(const std::string& value) {
  std::string token = trim_copy(value);
  std::transform(token.begin(), token.end(), token.begin(),
                 [](unsigned char ch) {
                   return static_cast<char>(std::tolower(ch));
                 });
  if (token.empty()) {
    return 0.0f;
  }
  if (token.size() >= 3 &&
      token.compare(token.size() - 3, 3, "deg") == 0) {
    return std::stof(token.substr(0, token.size() - 3));
  }
  if (token.size() >= 3 &&
      token.compare(token.size() - 3, 3, "rad") == 0) {
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

struct TransformValues {
  float x = 0.0f;
  float y = 0.0f;
  float scale = 1.0f;
  float scale_x = 1.0f;
  float scale_y = 1.0f;
  float rotate = 0.0f;
};

TransformValues parse_animation_transform(const std::string& value) {
  TransformValues result;
  size_t i = 0;
  while (i < value.size()) {
    while (i < value.size() &&
           std::isspace(static_cast<unsigned char>(value[i]))) {
      ++i;
    }
    size_t fn_start = i;
    while (i < value.size() &&
           std::isalnum(static_cast<unsigned char>(value[i]))) {
      ++i;
    }
    const std::string function = value.substr(fn_start, i - fn_start);
    if (function.empty() || i >= value.size() || value[i] != '(') {
      ++i;
      continue;
    }
    size_t arg_start = ++i;
    int depth = 1;
    while (i < value.size() && depth > 0) {
      if (value[i] == '(') {
        ++depth;
      } else if (value[i] == ')') {
        --depth;
        if (depth == 0) {
          break;
        }
      }
      ++i;
    }
    const std::string args_text = value.substr(arg_start, i - arg_start);
    std::vector<std::string> args;
    std::string current;
    for (char ch : args_text) {
      if (ch == ',') {
        const std::string token = trim_copy(current);
        if (!token.empty()) {
          args.push_back(token);
        }
        current.clear();
      } else {
        current.push_back(ch);
      }
    }
    const std::string tail = trim_copy(current);
    if (!tail.empty()) {
      args.push_back(tail);
    }
    std::string function_name = function;
    std::transform(function_name.begin(), function_name.end(),
                   function_name.begin(), [](unsigned char ch) {
                     return static_cast<char>(std::tolower(ch));
                   });
    if (function_name == "translate" && !args.empty()) {
      result.x = parse_animation_length(args[0]);
      result.y = args.size() > 1 ? parse_animation_length(args[1]) : 0.0f;
    } else if (function_name == "translate3d" && !args.empty()) {
      result.x = parse_animation_length(args[0]);
      result.y = args.size() > 1 ? parse_animation_length(args[1]) : 0.0f;
    } else if (function_name == "translatex" && !args.empty()) {
      result.x = parse_animation_length(args[0]);
    } else if (function_name == "translatey" && !args.empty()) {
      result.y = parse_animation_length(args[0]);
    } else if (function_name == "scale" && !args.empty()) {
      const float sx = std::stof(args[0]);
      const float sy = args.size() > 1 ? std::stof(args[1]) : sx;
      result.scale = sx == sy ? sx : 1.0f;
      result.scale_x = sx;
      result.scale_y = sy;
    } else if (function_name == "scale3d" && !args.empty()) {
      const float sx = std::stof(args[0]);
      const float sy = args.size() > 1 ? std::stof(args[1]) : sx;
      result.scale = sx == sy ? sx : 1.0f;
      result.scale_x = sx;
      result.scale_y = sy;
    } else if (function_name == "scalex" && !args.empty()) {
      result.scale_x = std::stof(args[0]);
    } else if (function_name == "scaley" && !args.empty()) {
      result.scale_y = std::stof(args[0]);
    } else if ((function_name == "rotate" || function_name == "rotatez") &&
               !args.empty()) {
      result.rotate = parse_animation_angle_degrees(args[0]);
    }
    if (i < value.size()) {
      ++i;
    }
  }
  return result;
}

bool is_animation_color_token(const std::string& value) {
  const std::string trimmed = trim_copy(value);
  if (trimmed.empty()) {
    return false;
  }
  if (trimmed[0] == '#') {
    return true;
  }
  return trimmed.rfind("rgb(", 0) == 0 || trimmed.rfind("rgba(", 0) == 0;
}

BoxShadow parse_animation_box_shadow(const std::string& value, bool& ok) {
  const std::string trimmed = trim_copy(value);
  ok = false;
  if (trimmed.empty() || trimmed == "none") {
    ok = true;
    return BoxShadow{};
  }

  std::vector<std::string> parts;
  std::string current;
  int paren_depth = 0;
  for (char ch : trimmed) {
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
    if (std::isspace(static_cast<unsigned char>(ch)) && paren_depth == 0) {
      const std::string token = trim_copy(current);
      if (!token.empty()) {
        parts.push_back(token);
      }
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  const std::string tail = trim_copy(current);
  if (!tail.empty()) {
    parts.push_back(tail);
  }

  BoxShadow shadow;
  std::vector<float> lengths;
  lengths.reserve(4);
  for (const auto& part : parts) {
    std::string lowered = part;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (lowered == "inset") {
      shadow.inset = true;
      continue;
    }
    if (is_animation_color_token(lowered)) {
      shadow.color = parse_animation_color(lowered);
      continue;
    }
    try {
      lengths.push_back(parse_animation_length(part));
    } catch (...) {
      return shadow;
    }
  }

  if (lengths.size() < 2) {
    return shadow;
  }
  shadow.offset_x = lengths[0];
  shadow.offset_y = lengths[1];
  if (lengths.size() >= 3) {
    shadow.blur_radius = lengths[2];
  }
  if (lengths.size() >= 4) {
    shadow.spread_radius = lengths[3];
  }
  ok = true;
  return shadow;
}

void finalize_animation_points(std::map<float, float>& values, float base_value,
                               std::vector<AnimationValuePoint>& out) {
  if (values.empty()) {
    return;
  }
  if (values.begin()->first > 0.0f) {
    values.emplace(0.0f, base_value);
  }
  if (values.rbegin()->first < 1.0f) {
    values.emplace(1.0f, base_value);
  }
  out.clear();
  out.reserve(values.size());
  for (const auto& [offset, value] : values) {
    out.push_back(AnimationValuePoint{offset, value});
  }
}

void register_float_animation_track(
    AnimationManager& animations, std::uintptr_t element_id,
    const std::string& property, float base_value,
    const std::vector<AnimationKeyframeStep>& keyframes, const AnimationDef& def,
    float current_time_ms,
    const std::function<bool(const std::map<std::string, std::string>&,
                             float&)>& extractor) {
  std::map<float, float> values;
  for (const auto& frame : keyframes) {
    float sampled = 0.0f;
    if (extractor(frame.properties, sampled)) {
      values[frame.offset] = sampled;
    }
  }
  std::vector<AnimationValuePoint> points;
  finalize_animation_points(values, base_value, points);
  if (!points.empty()) {
    animations.start(element_id, property, points, def, current_time_ms);
  }
}

void register_typed_float_animation_track(
    AnimationManager& animations, std::uintptr_t element_id,
    const detail::StylePropertyDesc& property, float base_value,
    const std::vector<AnimationKeyframeStep>& keyframes,
    const AnimationDef& def, float current_time_ms,
    const std::function<bool(const std::map<std::string, std::string>&,
                             float&)>& extractor) {
  std::map<float, float> values;
  for (const auto& frame : keyframes) {
    float sampled = 0.0f;
    if (extractor(frame.properties, sampled)) {
      values[frame.offset] = sampled;
    }
  }
  std::vector<AnimationValuePoint> points;
  finalize_animation_points(values, base_value, points);
  if (!points.empty()) {
    animations.start_float(element_id, property, points, def, current_time_ms);
  }
}

void register_typed_float_animation_track(
    AnimationManager& animations, std::uintptr_t element_id,
    detail::StylePropertyId property_id, float base_value,
    const std::vector<AnimationKeyframeStep>& keyframes,
    const AnimationDef& def, float current_time_ms,
    const std::function<bool(const std::map<std::string, std::string>&,
                             float&)>& extractor) {
  const auto* property = detail::style_property_descriptor(property_id);
  if (!property) {
    return;
  }
  register_typed_float_animation_track(
      animations, element_id, *property, base_value, keyframes, def,
      current_time_ms, extractor);
}

void register_color_animation_track(AnimationManager& animations,
                                    std::uintptr_t element_id,
                                    const std::string& property_prefix,
                                    const Color& base_color,
                                    const std::vector<AnimationKeyframeStep>& keyframes,
                                    const AnimationDef& def,
                                    float current_time_ms,
                                    const std::function<bool(
                                        const std::map<std::string, std::string>&,
                                        Color&)>& extractor) {
  std::map<float, float> r_values;
  std::map<float, float> g_values;
  std::map<float, float> b_values;
  std::map<float, float> a_values;
  for (const auto& frame : keyframes) {
    Color color;
    if (extractor(frame.properties, color)) {
      r_values[frame.offset] = color.r;
      g_values[frame.offset] = color.g;
      b_values[frame.offset] = color.b;
      a_values[frame.offset] = color.a;
    }
  }

  std::vector<AnimationValuePoint> points;
  finalize_animation_points(r_values, base_color.r, points);
  if (!points.empty()) {
    animations.start(element_id, property_prefix + "-r", points, def,
                     current_time_ms);
  }
  finalize_animation_points(g_values, base_color.g, points);
  if (!points.empty()) {
    animations.start(element_id, property_prefix + "-g", points, def,
                     current_time_ms);
  }
  finalize_animation_points(b_values, base_color.b, points);
  if (!points.empty()) {
    animations.start(element_id, property_prefix + "-b", points, def,
                     current_time_ms);
  }
  finalize_animation_points(a_values, base_color.a, points);
  if (!points.empty()) {
    animations.start(element_id, property_prefix + "-a", points, def,
                     current_time_ms);
  }
}

void register_element_animations(Box& box, Element* elem, StyleEngine& style_engine,
                                 float current_time_ms) {
  if (!elem || !elem->computed_style) {
    return;
  }

  const auto& style = *elem->computed_style;
  const auto element_id = reinterpret_cast<std::uintptr_t>(elem);
  box.animations().clear_element(element_id);

  const bool has_list_animations = !style.animations.empty();
  if (!has_list_animations &&
      (style.animation_name.empty() || style.animation_duration_ms <= 0.0f)) {
    return;
  }

  std::vector<AnimationStyleEntry> entries = style.animations;
  if (entries.empty()) {
    AnimationStyleEntry entry;
    entry.name = style.animation_name;
    entry.duration_ms = style.animation_duration_ms;
    entry.delay_ms = style.animation_delay_ms;
    entry.timing = style.animation_timing;
    entry.iteration_count = style.animation_iteration_count;
    entry.infinite = style.animation_infinite;
    entry.fill_mode = style.animation_fill_mode;
    entry.direction = style.animation_direction;
    entry.play_state = style.animation_play_state;
    entries.push_back(entry);
  }

  for (const auto& entry : entries) {
    if (entry.name.empty() || entry.duration_ms <= 0.0f) {
      continue;
    }

    const auto* keyframes = style_engine.keyframes(entry.name);
    if (!keyframes || keyframes->empty()) {
      continue;
    }

    AnimationDef def;
    def.duration_ms = entry.duration_ms;
    def.delay_ms = entry.delay_ms;
    def.easing = entry.timing;
    def.iteration_count = entry.iteration_count;
    def.infinite = entry.infinite;
    def.fill_mode = entry.fill_mode;
    def.direction = entry.direction;
    def.play_state = entry.play_state;

    if (const auto* opacity_property =
            detail::style_property_descriptor(detail::StylePropertyId::Opacity)) {
      register_typed_float_animation_track(
          box.animations(), element_id, *opacity_property, style.opacity,
          *keyframes, def, current_time_ms,
          [opacity_property](const auto& props, float& out) {
            const auto it = props.find("opacity");
            if (it == props.end()) {
              return false;
            }
            const auto value =
                detail::compile_css_literal(opacity_property, it->second);
            if (!value.type ||
                !cmeta_type_equal(value.type, &cmeta_type_float)) {
              return false;
            }
            if (const auto* scalar = std::get_if<float>(&value.value)) {
              out = *scalar;
              return true;
            }
            return false;
          });
    }

    register_color_animation_track(
        box.animations(), element_id, "background-color",
        resolved_element_background(*elem, style), *keyframes, def,
        current_time_ms,
        [](const auto& props, Color& out) {
          auto it = props.find("background-color");
          if (it == props.end()) {
            it = props.find("background");
            if (it == props.end()) {
              return false;
            }
          }
          out = parse_animation_color(it->second);
          return true;
        });

    register_typed_float_animation_track(
        box.animations(), element_id, detail::StylePropertyId::TransformX,
        style.transform_x, *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("transform");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_transform(it->second).x;
          return true;
        });
    register_typed_float_animation_track(
        box.animations(), element_id, detail::StylePropertyId::TransformY,
        style.transform_y, *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("transform");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_transform(it->second).y;
          return true;
        });
    register_typed_float_animation_track(
        box.animations(), element_id, detail::StylePropertyId::TransformScale,
        style.transform_scale, *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("transform");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_transform(it->second).scale;
          return true;
        });
    register_typed_float_animation_track(
        box.animations(), element_id, detail::StylePropertyId::TransformScaleX,
        style.transform_scale_x, *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("transform");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_transform(it->second).scale_x;
          return true;
        });
    register_typed_float_animation_track(
        box.animations(), element_id, detail::StylePropertyId::TransformScaleY,
        style.transform_scale_y, *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("transform");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_transform(it->second).scale_y;
          return true;
        });
    register_typed_float_animation_track(
        box.animations(), element_id, detail::StylePropertyId::TransformRotate,
        style.transform_rotate, *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("transform");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_transform(it->second).rotate;
          return true;
        });

    register_color_animation_track(
        box.animations(), element_id, "border-color", style.border_color,
        *keyframes, def, current_time_ms,
        [](const auto& props, Color& out) {
          auto it = props.find("border-color");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_color(it->second);
          return true;
        });

    register_float_animation_track(
        box.animations(), element_id, "outline-width", style.outline_width,
        *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("outline-width");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_length(it->second);
          return true;
        });
    register_float_animation_track(
        box.animations(), element_id, "outline-offset", style.outline_offset,
        *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("outline-offset");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_length(it->second);
          return true;
        });
    register_color_animation_track(
        box.animations(), element_id, "outline-color", style.outline_color,
        *keyframes, def, current_time_ms,
        [](const auto& props, Color& out) {
          auto it = props.find("outline-color");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_color(it->second);
          return true;
        });

    register_float_animation_track(
        box.animations(), element_id, "ring-width", style.ring_width, *keyframes,
        def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("ring-width");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_length(it->second);
          return true;
        });
    register_float_animation_track(
        box.animations(), element_id, "ring-offset", style.ring_offset, *keyframes,
        def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("ring-offset");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_length(it->second);
          return true;
        });
    register_color_animation_track(
        box.animations(), element_id, "ring-color", style.ring_color, *keyframes,
        def, current_time_ms,
        [](const auto& props, Color& out) {
          auto it = props.find("ring-color");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_color(it->second);
          return true;
        });
    register_color_animation_track(
        box.animations(), element_id, "ring-offset-color",
        style.ring_offset_color, *keyframes, def, current_time_ms,
        [](const auto& props, Color& out) {
          auto it = props.find("ring-offset-color");
          if (it == props.end()) {
            return false;
          }
          out = parse_animation_color(it->second);
          return true;
        });

    register_float_animation_track(
        box.animations(), element_id, "box-shadow-offset-x", style.shadow.offset_x,
        *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("box-shadow");
          if (it == props.end()) {
            return false;
          }
          bool ok = false;
          const BoxShadow shadow = parse_animation_box_shadow(it->second, ok);
          if (!ok) {
            return false;
          }
          out = shadow.offset_x;
          return true;
        });
    register_float_animation_track(
        box.animations(), element_id, "box-shadow-offset-y", style.shadow.offset_y,
        *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("box-shadow");
          if (it == props.end()) {
            return false;
          }
          bool ok = false;
          const BoxShadow shadow = parse_animation_box_shadow(it->second, ok);
          if (!ok) {
            return false;
          }
          out = shadow.offset_y;
          return true;
        });
    register_float_animation_track(
        box.animations(), element_id, "box-shadow-blur", style.shadow.blur_radius,
        *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("box-shadow");
          if (it == props.end()) {
            return false;
          }
          bool ok = false;
          const BoxShadow shadow = parse_animation_box_shadow(it->second, ok);
          if (!ok) {
            return false;
          }
          out = shadow.blur_radius;
          return true;
        });
    register_float_animation_track(
        box.animations(), element_id, "box-shadow-spread",
        style.shadow.spread_radius, *keyframes, def, current_time_ms,
        [](const auto& props, float& out) {
          auto it = props.find("box-shadow");
          if (it == props.end()) {
            return false;
          }
          bool ok = false;
          const BoxShadow shadow = parse_animation_box_shadow(it->second, ok);
          if (!ok) {
            return false;
          }
          out = shadow.spread_radius;
          return true;
        });
    register_color_animation_track(
        box.animations(), element_id, "box-shadow-color", style.shadow.color,
        *keyframes, def, current_time_ms,
        [](const auto& props, Color& out) {
          auto it = props.find("box-shadow");
          if (it == props.end()) {
            return false;
          }
          bool ok = false;
          const BoxShadow shadow = parse_animation_box_shadow(it->second, ok);
          if (!ok) {
            return false;
          }
          out = shadow.color;
          return true;
        });
  }
}

} // namespace

BoxOptions BoxOptions::legacy_without_jit() {
  BoxOptions options;
  options.utility_jit = UtilityJitMode::Disabled;
  return options;
}

Box::Box(flex::Renderer* renderer, BoxOptions options)
    : theme_mode_(options.theme),
      bindings_([this]() {
        dirty_style_ = true;
        dirty_layout_ = true;
        dirty_paint_ = true;
      }) {
  if (renderer) {
    renderer_ = std::make_unique<Renderer>(renderer);
    render_mgr_ = std::make_unique<RenderManager>(renderer_.get());
  }
  pipeline_ = std::make_unique<ViewPipeline>();
  style_engine_.append_css(R"(
    [role=checkbox] > box,
    [role=checkbox] > indicator,
    [role=checkbox] > label,
    [role=switch] > track,
    [role=switch] > thumb,
    [role=switch] > label {
      position: absolute;
      pointer-events: none;
    }
    [role=checkbox] > box {
      width: var(--checkbox-size, 20px);
      height: var(--checkbox-size, 20px);
      background-color: var(--checkbox-bg, var(--host-control-background, #ffffff));
      border-width: var(--host-control-border-width, 2px);
      border-style: solid;
      border-color: var(--checkbox-border, var(--host-control-border-color, #c8c8c8));
      border-radius: var(--checkbox-radius, var(--host-control-border-radius, 3px));
    }
    [role=checkbox]:checked > box {
      background-color: var(--checkbox-bg-checked, var(--host-control-background, var(--accent-color, #3b82f6)));
    }
    [role=checkbox] > indicator {
      color: var(--checkbox-checkmark, #ffffff);
      font-weight: 700;
      text-align: center;
    }
    [role=checkbox] > label,
    [role=switch] > label {
      background-color: transparent;
      color: var(--text-color, currentColor);
      text-align: left;
    }
    [role=switch] > track {
      width: var(--switch-width, 50px);
      height: var(--switch-height, 28px);
      background-color: var(--switch-bg-off, var(--host-control-background, #c8c8c8));
      border-width: var(--host-control-border-width, 0px);
      border-style: solid;
      border-color: var(--host-control-border-color, transparent);
      border-radius: var(--host-control-border-radius, 999px);
    }
    [role=switch]:checked > track {
      background-color: var(--switch-bg-on, var(--host-control-background, var(--accent-color, #22c55e)));
    }
    [role=switch] > thumb {
      width: var(--switch-thumb-size, auto);
      height: var(--switch-thumb-size, auto);
      background-color: var(--switch-thumb, #ffffff);
      border-radius: 999px;
    }
    [role=button] > ripple-layer,
    [role=button] > label,
    [role=button] > spinner {
      position: absolute;
      inset: 0;
      pointer-events: none;
      background-color: transparent;
    }
    [role=button] > label {
      color: var(--button-text, var(--text, var(--text-color, #f8fafc)));
      text-align: center;
    }
    [role=button][data-variant=secondary] > label,
    [role=button][data-variant=outline] > label,
    [role=button][data-variant=ghost] > label {
      color: var(--button-text, var(--text, var(--text-color, #0f172a)));
    }
    [role=button][aria-disabled=true] > label { opacity: 0.65; }
    [role=button] > spinner {
      color: var(--loading-spinner-color, var(--button-text, var(--text-color, currentColor)));
    }
    input > viewport,
    [role=textbox][aria-multiline=true] > viewport,
    input > selection-layer,
    [role=textbox][aria-multiline=true] > selection-layer,
    input > text,
    [role=textbox][aria-multiline=true] > text,
    input > placeholder,
    [role=textbox][aria-multiline=true] > placeholder,
    input > caret,
    [role=textbox][aria-multiline=true] > caret {
      position: absolute;
      inset: 0;
      pointer-events: none;
      background-color: transparent;
    }
    input > viewport,
    [role=textbox][aria-multiline=true] > viewport {
      overflow: hidden;
    }
    input > selection-layer {
      background-color: var(--selection-bg, var(--input-selection-bg, rgba(100,149,237,0.5)));
    }
    [role=textbox][aria-multiline=true] > selection-layer {
      background-color: var(--selection-bg, var(--textarea-selection-bg, rgba(100,149,237,0.5)));
    }
    input > text { color: var(--input-text, currentColor); }
    [role=textbox][aria-multiline=true] > text { color: var(--textarea-text, currentColor); }
    input > placeholder { color: var(--input-placeholder, #a0a0a0); }
    [role=textbox][aria-multiline=true] > placeholder { color: var(--textarea-placeholder, #a0a0a0); }
    input > caret {
      background-color: var(--input-cursor, var(--caret-color, currentColor));
    }
    [role=textbox][aria-multiline=true] > caret {
      background-color: var(--textarea-cursor, var(--caret-color, currentColor));
    }
    [role=slider] > track {
      position: absolute;
      pointer-events: none;
      height: var(--track-height, 4px);
      background-color: var(--track-bg, #e5e7eb);
      border-radius: 999px;
    }
    [role=slider] > fill {
      position: absolute;
      pointer-events: none;
      height: var(--track-height, 4px);
      background-color: var(--track-fill, #3b82f6);
      border-radius: 999px;
    }
    [role=slider] > thumb {
      position: absolute;
      pointer-events: none;
      width: var(--thumb-size, 20px);
      height: var(--thumb-size, 20px);
      background-color: var(--thumb-bg, #ffffff);
      border: 2px solid var(--thumb-border, #c8c8c8);
      border-radius: 999px;
    }
    [role=slider] > value {
      position: absolute;
      pointer-events: none;
      background-color: transparent;
      text-align: center;
    }
    [role=progressbar] > track {
      position: absolute;
      pointer-events: none;
      height: var(--progress-height, 8px);
      background-color: var(--progress-bg, #e5e7eb);
      border-radius: var(--progress-border-radius, 4px);
    }
    [role=progressbar] > fill {
      position: absolute;
      pointer-events: none;
      height: var(--progress-height, 8px);
      background-color: var(--progress-fill, #3b82f6);
      border-radius: var(--progress-border-radius, 4px);
    }
  )");

  const auto theme_result = style_engine_.load_stylesheet(
      std::string(detail::default_theme_css()), default_theme_load_options());
  if (!theme_result.applied) {
    throw std::runtime_error("failed to load flexUI default theme");
  }
  if (options.utility_jit == UtilityJitMode::BuiltIn) {
    enable_utility_jit(detail::builtin_utility_catalog(),
                       options.utility_limits);
  }
}

Box::~Box() {
  for (auto& elem_ptr : elements_) {
    elem_ptr->widget = nullptr;
  }
}

bool Box::has_view_root() const {
  return root_ != nullptr;
}

bool Box::needs_style_stage() const {
  return dirty_style_;
}

bool Box::needs_layout_stage() const {
  return dirty_layout_;
}

bool Box::needs_render_stage() const {
  return render_mgr_ && dirty_paint_;
}

void Box::run_style_stage() {
  sync_utility_stylesheet();
  compute_styles(root_);
  dirty_style_ = false;
}

void Box::run_layout_stage() {
  root_->set_x(0);
  root_->set_y(0);
  LayoutManager::sync_to_flex(root_, viewport_width_, viewport_height_);
  LayoutManager::perform_layout(root_);
  snapshot_sticky_layout(root_);
  dirty_layout_ = false;
}

void Box::run_layout_semantics_stage() {
  sync_layout_dependent_widget_semantics(root_);
}

bool Box::run_container_query_stage() {
  return reconcile_container_queries(root_, container_widths_);
}

void Box::run_positioning_stage() {
  adjust_positioned_layout(root_, viewport_width_, viewport_height_);
}

RenderFrame Box::make_render_frame() const {
  RenderFrame frame;
  frame.root = root_;
  frame.viewport = {viewport_width_, viewport_height_, 1.0f};
  return frame;
}

void Box::run_render_stage() {
  render_mgr_->render_frame(make_render_frame());
  dirty_paint_ = false;
}

void Box::load_css(const std::string& css) {
  style_engine_.append_css(css);
  if (root_) root_->mark_style_dirty();
}

CssLoadResult Box::load_stylesheet(const std::string& css,
                                   const CssLoadOptions& options) {
  auto result = style_engine_.load_stylesheet(css, options);
  if (result.applied && root_) {
    root_->mark_style_dirty();
  }
  return result;
}

CssLoadResult Box::replace_stylesheet(StylesheetId stylesheet_id,
                                      const std::string& css,
                                      const CssLoadOptions& options) {
  auto result = style_engine_.replace_stylesheet(stylesheet_id, css, options);
  if (result.applied && root_) {
    root_->mark_style_dirty();
  }
  return result;
}

bool Box::remove_stylesheet(StylesheetId stylesheet_id) {
  const bool removed = style_engine_.remove_stylesheet(stylesheet_id);
  if (removed && root_) {
    root_->mark_style_dirty();
  }
  return removed;
}

void Box::enable_utility_jit(nlohmann::json utility_whitelist,
                             tailwind::UtilityJitOptions options) {
  enable_utility_jit(std::make_shared<const tailwind::UtilityCatalog>(
                         std::move(utility_whitelist)),
                     options);
}

void Box::enable_utility_jit(
    std::shared_ptr<const tailwind::UtilityCatalog> utility_catalog,
    tailwind::UtilityJitOptions options) {
  auto next_jit = std::make_unique<tailwind::UtilityJit>(
      std::move(utility_catalog), options);

  std::vector<Element*> pending;
  if (root_) {
    pending.push_back(root_);
  }
  while (!pending.empty()) {
    Element* element = pending.back();
    pending.pop_back();
    for (const auto& token : element->utility_names()) {
      if (!next_jit->contains(token)) {
        throw std::invalid_argument(
            "active explicit utility is absent from the new catalog: " +
            token);
      }
    }
    for (auto* child : element->children()) {
      if (child) {
        pending.push_back(static_cast<Element*>(child));
      }
    }
  }

  const auto load_options = tailwind_jit_load_options();
  const CssLoadResult slot = utility_stylesheet_id_ == 0
      ? style_engine_.load_stylesheet("", load_options)
      : style_engine_.replace_stylesheet(utility_stylesheet_id_, "",
                                         load_options);
  if (!slot.applied) {
    throw std::runtime_error("failed to reserve Tailwind JIT stylesheet");
  }

  utility_jit_ = std::move(next_jit);
  utility_stylesheet_id_ = slot.stylesheet_id;
  utility_applied_revision_ = 0;
  utility_tree_dirty_ = true;
  missing_utility_tokens_.clear();
  if (root_) {
    root_->mark_style_dirty();
  }
}

void Box::disable_utility_jit() {
  if (utility_stylesheet_id_ != 0) {
    style_engine_.remove_stylesheet(utility_stylesheet_id_);
  }
  utility_jit_.reset();
  utility_stylesheet_id_ = 0;
  utility_applied_revision_ = 0;
  utility_tree_dirty_ = false;
  missing_utility_tokens_.clear();
  if (root_) {
    root_->mark_style_dirty();
  }
}

void Box::notify_utility_tree_changed() {
  if (utility_jit_) {
    utility_tree_dirty_ = true;
  }
}

void Box::validate_utility_token(std::string_view token) const {
  if (!utility_jit_) {
    throw std::logic_error("explicit utilities require an enabled utility JIT");
  }
  if (!utility_jit_->contains(token)) {
    throw std::invalid_argument("unknown explicit utility token: " +
                                std::string(token));
  }
}

bool Box::owns_element(const Element* element) const {
  return element && element->owner_box_ == this;
}

void Box::sync_utility_stylesheet() {
  if (!utility_jit_ || !utility_tree_dirty_) {
    return;
  }

  std::unordered_set<std::string> active_tokens;
  std::vector<std::string> missing_required;
  std::vector<Element*> pending;
  if (root_) {
    pending.push_back(root_);
  }
  while (!pending.empty()) {
    Element* elem = pending.back();
    pending.pop_back();
    for (const auto& token : elem->class_names()) {
      if (utility_jit_->contains(token)) {
        active_tokens.insert(token);
      }
    }
    for (const auto& token : elem->utility_names()) {
      if (!utility_jit_->contains(token)) {
        missing_required.push_back(token);
      }
    }
    for (auto* child : elem->children()) {
      if (child) {
        pending.push_back(static_cast<Element*>(child));
      }
    }
  }

  std::sort(missing_required.begin(), missing_required.end());
  missing_required.erase(
      std::unique(missing_required.begin(), missing_required.end()),
      missing_required.end());
  missing_utility_tokens_ = missing_required;
  if (!missing_required.empty()) {
    throw std::runtime_error("explicit utility is absent from the active catalog: " +
                             missing_required.front());
  }

  std::vector<std::string> tokens(active_tokens.begin(), active_tokens.end());
  std::sort(tokens.begin(), tokens.end());
  const auto compiled = utility_jit_->replace_tokens(tokens);
  missing_utility_tokens_ = compiled.missing_tokens;
  utility_tree_dirty_ = false;
  if (compiled.revision == utility_applied_revision_) {
    return;
  }

  if (utility_stylesheet_id_ == 0) {
    utility_tree_dirty_ = true;
    throw std::logic_error("Tailwind JIT stylesheet slot is missing");
  }

  const auto load_options = tailwind_jit_load_options();
  const CssLoadResult load_result = style_engine_.replace_stylesheet(
      utility_stylesheet_id_, compiled.stylesheet, load_options);
  if (!load_result.applied) {
    utility_tree_dirty_ = true;
    throw std::runtime_error("failed to apply Tailwind JIT stylesheet");
  }
  utility_applied_revision_ = compiled.revision;
}

void Box::deactivate_subtree(Element* root) {
  if (!root) {
    return;
  }
  events_.detach_subtree(root);
  std::vector<Element*> pending{root};
  while (!pending.empty()) {
    Element* element = pending.back();
    pending.pop_back();
    element->set_hover(false);
    element->set_active(false);
    element->set_focus(false);
    element->set_focus_visible(false);
    element->set_state("focus-within", false);
    const auto element_id = reinterpret_cast<std::uintptr_t>(element);
    transitions_.clear_element(element_id);
    animations_.clear_element(element_id);
    for (auto* child : element->children()) {
      pending.push_back(static_cast<Element*>(child));
    }
  }
}

bool Box::register_font(const std::string& family, const std::string& path) {
  return renderer_ && renderer_->register_font(family, path);
}

void Box::unregister_font(const std::string& family) {
  if (renderer_) {
    renderer_->unregister_font(family);
  }
}

void Box::set_variable(const std::string& name, const std::string& value) {
  if (root_) {
    root_->set_custom_property(name, value);
  }
}

Element* Box::create(const std::string& tag, const std::string& id) {
  auto elem = std::make_unique<Element>();
  elem->set_tag(tag);
  elem->owner_box_ = this;
  Element* ptr = elem.get();
  elements_.push_back(std::move(elem));
  ptr->set_element_id(id);
  return ptr;
}

Element* Box::create_with_widget(const std::string& tag,
                                 std::unique_ptr<Widget> widget,
                                 const std::string& id) {
  Element* elem = create(tag, id);
  Widget* widget_ptr = widget.get();
  elem->widget = widget_ptr;
  elem->focusable = true;
  if (widget_ptr) {
    widget_ptr->bind_host_element(elem);
    widgets_.push_back(std::move(widget));
  }
  active_widgets_.push_back(elem);
  return elem;
}

Element* Box::create_widget_part(Element& host, const std::string& tag,
                                 const std::string& part_name) {
  Element* elem = create(tag);
  elem->ownership_ = ElementOwnership::Widget;
  elem->set_attribute("part", part_name);
  elem->set_attribute("aria-hidden", "true");
  return host.append_widget_part(elem);
}

Element* Box::create_with_widget(const std::string& tag, Widget* widget, const std::string& id) {
  return create_with_widget(tag, std::unique_ptr<Widget>(widget), id);
}

Element* Box::get_by_id(const std::string& id) {
  auto it = elements_by_id_.find(id);
  return it != elements_by_id_.end() ? it->second.element : nullptr;
}

UiHandle Box::handle_for(const Element& element) const {
  if (element.id().empty()) {
    return {};
  }
  const auto indexed = elements_by_id_.find(element.id());
  if (indexed == elements_by_id_.end() ||
      indexed->second.element != &element) {
    return {};
  }
  return {indexed->first, indexed->second.generation};
}

Element* Box::resolve_handle(const UiHandle& handle) noexcept {
  return const_cast<Element*>(
      static_cast<const Box&>(*this).resolve_handle(handle));
}

const Element* Box::resolve_handle(const UiHandle& handle) const noexcept {
  if (!handle) {
    return nullptr;
  }
  const auto indexed = elements_by_id_.find(handle.id);
  if (indexed == elements_by_id_.end() ||
      indexed->second.generation != handle.generation) {
    return nullptr;
  }
  return indexed->second.element;
}

Element* Box::query_selector(const std::string& selector) {
  if (!root_) {
    return nullptr;
  }
  std::vector<Element*> pending{root_};
  while (!pending.empty()) {
    Element* elem = pending.back();
    pending.pop_back();
    if (style_engine_.matches(elem, selector)) {
      return elem;
    }
    const auto& children = elem->children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      pending.push_back(static_cast<Element*>(*it));
    }
  }
  return nullptr;
}

std::vector<Element*> Box::query_selector_all(const std::string& selector) {
  std::vector<Element*> matches;
  if (!root_) {
    return matches;
  }
  std::vector<Element*> pending{root_};
  while (!pending.empty()) {
    Element* elem = pending.back();
    pending.pop_back();
    if (style_engine_.matches(elem, selector)) {
      matches.push_back(elem);
    }
    const auto& children = elem->children();
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
      pending.push_back(static_cast<Element*>(*it));
    }
  }
  return matches;
}

void Box::reindex_element_id(Element* elem, const std::string& old_id,
                             const std::string& new_id) {
  auto old_entry = elements_by_id_.end();
  Element* restored = nullptr;
  if (!old_id.empty()) {
    old_entry = elements_by_id_.find(old_id);
    if (old_entry != elements_by_id_.end() &&
        old_entry->second.element == elem) {
      for (const auto& candidate : elements_) {
        if (candidate.get() != elem && candidate->id() == old_id) {
          restored = candidate.get();
          break;
        }
      }
    }
  }

  const std::uint64_t required_generations =
      (new_id.empty() ? 0u : 1u) + (restored ? 1u : 0u);
  constexpr auto max_generation =
      std::numeric_limits<std::uint64_t>::max();
  if (required_generations > 0 &&
      (next_element_generation_ == 0 ||
       required_generations - 1 >
           max_generation - next_element_generation_)) {
    throw std::overflow_error("FlexUI element handle generation exhausted");
  }

  auto new_entry = elements_by_id_.end();
  if (!new_id.empty()) {
    new_entry = elements_by_id_.try_emplace(new_id, IndexedElement{}).first;
  }

  const auto next_generation = [this]() noexcept {
    return next_element_generation_++;
  };
  if (old_entry != elements_by_id_.end() &&
      old_entry->second.element == elem) {
    if (restored) {
      old_entry->second = {restored, next_generation()};
    } else {
      elements_by_id_.erase(old_entry);
    }
  }
  if (new_entry != elements_by_id_.end()) {
    new_entry->second = {elem, next_generation()};
  }
}

void Box::set_root(Element* elem) {
  if (root_ && root_ != elem) {
    root_->remove_attribute("data-flexui-theme-root");
    root_->remove_attribute("data-theme");
  }
  root_ = elem;
  if (root_) {
    root_->set_attribute("data-flexui-theme-root");
  }
  apply_theme_to_root();
  notify_utility_tree_changed();
  if (root_) root_->mark_style_dirty();
}

void Box::set_theme_mode(ThemeMode mode) {
  if (theme_mode_ == mode) {
    return;
  }
  theme_mode_ = mode;
  apply_theme_to_root();
}

void Box::apply_theme_to_root() {
  if (!root_) {
    return;
  }
  switch (theme_mode_) {
    case ThemeMode::System:
      root_->remove_attribute("data-theme");
      break;
    case ThemeMode::Light:
      root_->set_attribute("data-theme", "light");
      break;
    case ThemeMode::Dark:
      root_->set_attribute("data-theme", "dark");
      break;
  }
}

void Box::set_viewport(float width, float height) {
  if (viewport_width_ == width && viewport_height_ == height) return;
  viewport_width_ = width;
  viewport_height_ = height;
  if (root_) root_->mark_style_dirty();
}

void Box::set_media_environment(const MediaEnvironment& env) {
  if (media_environment_.prefers_reduced_motion == env.prefers_reduced_motion &&
      media_environment_.prefers_dark_scheme == env.prefers_dark_scheme &&
      media_environment_.hover_available == env.hover_available &&
      media_environment_.any_hover_available == env.any_hover_available &&
      media_environment_.forced_colors_active == env.forced_colors_active &&
      media_environment_.pointer_precision == env.pointer_precision &&
      media_environment_.any_pointer_precision == env.any_pointer_precision &&
      media_environment_.contrast_preference == env.contrast_preference) {
    return;
  }

  media_environment_ = env;
  if (root_) root_->mark_style_dirty();
}

flex::RendererCapabilities Box::renderer_capabilities() const {
  if (!renderer_) return {};
  return renderer_->capabilities();
}

bool Box::style_state_affects_selectors(Symbol state) const {
  return style_engine_.uses_pseudo_class(state);
}

void Box::update() {
  bindings_.update();
  pipeline_->update(*this);
}

ViewLifecycleState Box::lifecycle_state() const {
  return pipeline_->state();
}

void Box::invalidate() {
  if (root_) root_->mark_paint_dirty();
}

void Box::dispatch_event(Event& event) {
  events_.dispatch(event, root_);
}

void Box::update_time(float delta_ms) {
  time_ms_ += delta_ms;
  transitions_.update(time_ms_);
  animations_.update(time_ms_);
  for (size_t i = 0; i < active_widgets_.size(); ++i) {
    auto* elem = active_widgets_[i];
    if (elem && !is_visible_in_tree(elem)) {
      continue;
    }
    if (elem && elem->widget && elem->widget->needs_frame_update(*elem)) {
      elem->widget->update(delta_ms, *elem);
    }
  }
  if (transitions_.has_any_active() || animations_.has_any_active(time_ms_)) {
    mark_visible_active_effects_dirty(root_, transitions_, animations_, time_ms_);
  }
}

void Box::register_active_widget(Element* elem) {
  if (std::find(active_widgets_.begin(), active_widgets_.end(), elem) == active_widgets_.end()) {
    active_widgets_.push_back(elem);
  }
}

void Box::unregister_active_widget(Element* elem) {
  auto it = std::find(active_widgets_.begin(), active_widgets_.end(), elem);
  if (it != active_widgets_.end()) active_widgets_.erase(it);
}

void Box::compute_styles(Element* elem, bool parent_recomputed) {
  const bool recompute = elem->dirty_style() || parent_recomputed;
  if (!recompute) {
    for (auto* node : elem->children()) {
      if (auto* child = static_cast<Element*>(node)) compute_styles(child, false);
    }
    return;
  }
  if (!elem->computed_style) elem->computed_style = &elem->style_;
  const auto previous_style = *elem->computed_style;
  const auto element_id = reinterpret_cast<std::uintptr_t>(elem);
  const bool had_style_baseline =
      styled_elements_.find(element_id) != styled_elements_.end();
  style_engine_.apply_styles(elem);
  styled_elements_.insert(element_id);
  if (had_style_baseline &&
      !elem->computed_style->transition.empty()) {
    const auto defs = parse_transition_list(elem->computed_style->transition);
    bool started_any = false;
    const Color previous_bg =
        resolved_element_background(*elem, previous_style);
    const Color target_bg =
        resolved_element_background(*elem, *elem->computed_style);
    const Color previous_border = previous_style.border_color;
    const Color target_border = elem->computed_style->border_color;
    const Color previous_outline = previous_style.outline_color;
    const Color target_outline = elem->computed_style->outline_color;
    const Color previous_ring = previous_style.ring_color;
    const Color target_ring = elem->computed_style->ring_color;
    const Color previous_ring_offset = previous_style.ring_offset_color;
    const Color target_ring_offset = elem->computed_style->ring_offset_color;
    const BoxShadow& previous_shadow = previous_style.shadow;
    const BoxShadow& target_shadow = elem->computed_style->shadow;

    for (const auto& def : defs) {
      if (def.property == "all" || def.property == "opacity") {
        start_transition_if_changed(transitions_, element_id, "opacity",
                                    previous_style.opacity,
                                    elem->computed_style->opacity, def, time_ms_,
                                    started_any);
      }

      if (def.property == "all" || def.property == "transform") {
        start_transition_if_changed(transitions_, element_id, "transform-x",
                                    previous_style.transform_x,
                                    elem->computed_style->transform_x, def,
                                    time_ms_, started_any);
        start_transition_if_changed(transitions_, element_id, "transform-y",
                                    previous_style.transform_y,
                                    elem->computed_style->transform_y, def,
                                    time_ms_, started_any);
        start_transition_if_changed(transitions_, element_id, "transform-scale",
                                    previous_style.transform_scale,
                                    elem->computed_style->transform_scale, def,
                                    time_ms_, started_any);
        start_transition_if_changed(transitions_, element_id,
                                    "transform-scale-x",
                                    previous_style.transform_scale_x,
                                    elem->computed_style->transform_scale_x, def,
                                    time_ms_, started_any);
        start_transition_if_changed(transitions_, element_id,
                                    "transform-scale-y",
                                    previous_style.transform_scale_y,
                                    elem->computed_style->transform_scale_y, def,
                                    time_ms_, started_any);
        start_transition_if_changed(transitions_, element_id, "transform-rotate",
                                    previous_style.transform_rotate,
                                    elem->computed_style->transform_rotate, def,
                                    time_ms_, started_any);
      }

      if (def.property == "all" || def.property == "background-color") {
        start_color_transition_if_changed(transitions_, element_id,
                                          "background-color", previous_bg,
                                          target_bg, def, time_ms_,
                                          started_any);
      }

      if (def.property == "all" || def.property == "border-color") {
        start_color_transition_if_changed(transitions_, element_id,
                                          "border-color", previous_border,
                                          target_border, def, time_ms_,
                                          started_any);
      }

      if (def.property == "all" || def.property == "outline-width") {
        start_transition_if_changed(transitions_, element_id, "outline-width",
                                    previous_style.outline_width,
                                    elem->computed_style->outline_width, def,
                                    time_ms_, started_any);
      }

      if (def.property == "all" || def.property == "outline-offset") {
        start_transition_if_changed(transitions_, element_id, "outline-offset",
                                    previous_style.outline_offset,
                                    elem->computed_style->outline_offset, def,
                                    time_ms_, started_any);
      }

      if (def.property == "all" || def.property == "outline-color") {
        start_color_transition_if_changed(transitions_, element_id,
                                          "outline-color", previous_outline,
                                          target_outline, def, time_ms_,
                                          started_any);
      }

      if (def.property == "all" || def.property == "ring-width") {
        start_transition_if_changed(transitions_, element_id, "ring-width",
                                    previous_style.ring_width,
                                    elem->computed_style->ring_width, def,
                                    time_ms_, started_any);
      }

      if (def.property == "all" || def.property == "ring-offset") {
        start_transition_if_changed(transitions_, element_id, "ring-offset",
                                    previous_style.ring_offset,
                                    elem->computed_style->ring_offset, def,
                                    time_ms_, started_any);
      }

      if (def.property == "all" || def.property == "ring-color") {
        start_color_transition_if_changed(transitions_, element_id,
                                          "ring-color", previous_ring,
                                          target_ring, def, time_ms_,
                                          started_any);
      }

      if (def.property == "all" || def.property == "ring-offset-color") {
        start_color_transition_if_changed(transitions_, element_id,
                                          "ring-offset-color",
                                          previous_ring_offset,
                                          target_ring_offset, def, time_ms_,
                                          started_any);
      }

      if (def.property == "all" || def.property == "box-shadow") {
        start_transition_if_changed(transitions_, element_id,
                                    "box-shadow-offset-x",
                                    previous_shadow.offset_x,
                                    target_shadow.offset_x, def, time_ms_,
                                    started_any);
        start_transition_if_changed(transitions_, element_id,
                                    "box-shadow-offset-y",
                                    previous_shadow.offset_y,
                                    target_shadow.offset_y, def, time_ms_,
                                    started_any);
        start_transition_if_changed(transitions_, element_id,
                                    "box-shadow-blur",
                                    previous_shadow.blur_radius,
                                    target_shadow.blur_radius, def, time_ms_,
                                    started_any);
        start_transition_if_changed(transitions_, element_id,
                                    "box-shadow-spread",
                                    previous_shadow.spread_radius,
                                    target_shadow.spread_radius, def, time_ms_,
                                    started_any);
        start_color_transition_if_changed(transitions_, element_id,
                                          "box-shadow-color", previous_shadow.color,
                                          target_shadow.color, def, time_ms_,
                                          started_any);
      }
    }

    if (started_any) {
      notify_dirty_paint();
    }
  }
  if (!had_style_baseline ||
      animation_spec_changed(previous_style, *elem->computed_style)) {
    register_element_animations(*this, elem, style_engine_, time_ms_);
    notify_dirty_paint();
  } else if (had_style_baseline &&
             animation_play_state_changed(previous_style, *elem->computed_style)) {
    animations_.set_play_state(element_id, elem->computed_style->animation_play_state,
                               time_ms_);
    notify_dirty_paint();
  }
  elem->clear_dirty(flex::DirtyFlags::Content);
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) compute_styles(child, true);
  }
}

} // namespace flexUI
