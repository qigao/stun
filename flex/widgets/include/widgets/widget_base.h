/*
 * Flex Widgets - Base Classes and Common Functionality
 * Provides FSM states, animation helpers, and builder utilities
 */

#pragma once

#include "flex.h"
#include "widget_theme.h"

#include <functional>
#include <memory>

namespace flex {
namespace widgets {

// ============================================================================
// Widget States (for FSM)
// ============================================================================

enum class WidgetState { Normal, Hover, Pressed, Focused, Disabled };

// ============================================================================
// Animation Helpers
// ============================================================================

// Interpolate between two colors
inline ThemeColor lerp_color(const ThemeColor &from, const ThemeColor &to, float t) {
  auto lerp = [](uint8_t a, uint8_t b, float t) -> uint8_t { return uint8_t(a + (b - a) * t); };
  return ThemeColor(lerp(from.r(), to.r(), t), lerp(from.g(), to.g(), t), lerp(from.b(), to.b(), t),
                    lerp(from.a(), to.a(), t));
}

// Easing functions
inline float ease_out_quad(float t) { return t * (2 - t); }

inline float ease_out_cubic(float t) { return 1 - (1 - t) * (1 - t) * (1 - t); }

inline float ease_in_out_quad(float t) {
  return t < 0.5f ? 2 * t * t : 1 - (-2 * t + 2) * (-2 * t + 2) / 2;
}

// ============================================================================
// Builder Helpers
// ============================================================================

// Create a rounded rectangle for backgrounds
inline Shape::Ptr create_rect_bg(float width, float height, const ThemeColor &fill,
                                 float corner_radius = 0, const ThemeColor *stroke = nullptr,
                                 float stroke_width = 1.0f) {
  auto shape = Shape::create();
  shape->set_rect(width, height, corner_radius);
  shape->set_fill(
      Color(fill.r() / 255.0f, fill.g() / 255.0f, fill.b() / 255.0f, fill.a() / 255.0f));
  if (stroke) {
    shape->set_stroke(Color(stroke->r() / 255.0f, stroke->g() / 255.0f, stroke->b() / 255.0f,
                            stroke->a() / 255.0f),
                      stroke_width);
  }
  return shape;
}

// Create text label
inline Text::Ptr create_text(const std::string &content, float font_size, const ThemeColor &color,
                             const std::string &font_family = "") {
  auto text = Text::create();
  text->set_content(content);
  text->set_font_size(font_size);
  text->set_color(
      Color(color.r() / 255.0f, color.g() / 255.0f, color.b() / 255.0f, color.a() / 255.0f));
  if (!font_family.empty()) {
    text->set_font_family(font_family);
  }
  return text;
}

// Create circle
inline Shape::Ptr create_circle(float radius, const ThemeColor &fill,
                                const ThemeColor *stroke = nullptr, float stroke_width = 1.0f) {
  auto shape = Shape::create();
  shape->set_circle(radius);
  shape->set_fill(
      Color(fill.r() / 255.0f, fill.g() / 255.0f, fill.b() / 255.0f, fill.a() / 255.0f));
  if (stroke) {
    shape->set_stroke(Color(stroke->r() / 255.0f, stroke->g() / 255.0f, stroke->b() / 255.0f,
                            stroke->a() / 255.0f),
                      stroke_width);
  }
  return shape;
}

// ============================================================================
// Property Helpers
// ============================================================================

inline float get_float(const Props &props, const std::string &key, float default_val) {
  auto it = props.find(key);
  if (it != props.end()) {
    if (auto *f = std::get_if<float>(&it->second)) {
      return *f;
    }
  }
  return default_val;
}

inline std::string get_string(const Props &props, const std::string &key,
                              const std::string &default_val = "") {
  auto it = props.find(key);
  if (it != props.end()) {
    if (auto *s = std::get_if<std::string>(&it->second)) {
      return *s;
    }
  }
  return default_val;
}

inline bool get_bool(const Props &props, const std::string &key, bool default_val = false) {
  auto it = props.find(key);
  if (it != props.end()) {
    if (auto *b = std::get_if<bool>(&it->second)) {
      return *b;
    }
  }
  return default_val;
}

inline uint32_t get_color(const Props &props, const std::string &key, uint32_t default_val) {
  auto it = props.find(key);
  if (it != props.end()) {
    if (auto *c = std::get_if<uint32_t>(&it->second)) {
      return *c;
    }
  }
  return default_val;
}

// ============================================================================
// Widget Update Context
// ============================================================================

struct WidgetUpdateContext {
  Node *root = nullptr;
  WidgetState state = WidgetState::Normal;
  float animation_progress = 1.0f; // 0-1, 1 = complete

  // Find child by ID
  template <typename T> T *find(const std::string &id) {
    if (!root)
      return nullptr;
    auto *node = root->find(id);
    return dynamic_cast<T *>(node);
  }
};

// ============================================================================
// Common Widget Callbacks
// ============================================================================

using ClickCallback = std::function<void()>;
using ChangeCallback = std::function<void(float)>;    // For sliders, progress
using BoolChangeCallback = std::function<void(bool)>; // For checkbox, toggle
using TextChangeCallback = std::function<void(const std::string &)>;
using SelectCallback = std::function<void(int)>; // For lists, combos

// ============================================================================
// Widget Registration Helper
// ============================================================================

template <typename BuilderFunc, typename UpdateFunc = std::nullptr_t>
inline Component::Ptr create_widget(const std::string &name,
                                    std::initializer_list<std::pair<std::string, PropValue>> props,
                                    BuilderFunc builder, UpdateFunc updater = nullptr) {
  auto component = Component::create(name);

  for (const auto &[key, value] : props) {
    std::visit([&](const auto &v) { component->add_prop(key, v); }, value);
  }

  component->set_builder(builder);

  return component;
}

// ============================================================================
// State Style Applicator
// ============================================================================

inline void apply_state_style(Node *root, WidgetState state,
                              const WidgetTheme &theme = current_theme()) {
  if (!root)
    return;

  auto *bg = root->find("bg");
  auto *border_node = root->find("border");

  Shape *bg_shape = dynamic_cast<Shape *>(bg);
  Shape *border_shape = dynamic_cast<Shape *>(border_node);

  ThemeColor bg_color = theme.surface;
  ThemeColor border_color = theme.border;

  switch (state) {
  case WidgetState::Normal:
    bg_color = theme.surface;
    border_color = theme.border;
    break;
  case WidgetState::Hover:
    bg_color = theme.surface_hover;
    border_color = theme.border_hover;
    break;
  case WidgetState::Pressed:
    bg_color = theme.surface_pressed;
    border_color = theme.border_hover;
    break;
  case WidgetState::Focused:
    bg_color = theme.surface;
    border_color = theme.border_focus;
    break;
  case WidgetState::Disabled:
    bg_color = theme.disabled_bg;
    border_color = theme.border;
    break;
  }

  if (bg_shape) {
    bg_shape->set_fill(Color(bg_color.r() / 255.0f, bg_color.g() / 255.0f, bg_color.b() / 255.0f,
                             bg_color.a() / 255.0f));
  }

  if (border_shape) {
    border_shape->set_stroke(Color(border_color.r() / 255.0f, border_color.g() / 255.0f,
                                   border_color.b() / 255.0f, border_color.a() / 255.0f));
  }
}

} // namespace widgets
} // namespace flex
