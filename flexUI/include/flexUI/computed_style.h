/*
 * flexUI - Computed Style
 *
 * 计算后的样式（已解析的 CSS 属性值）
 */

#ifndef FLEXUI_COMPUTED_STYLE_H
#define FLEXUI_COMPUTED_STYLE_H

#include "types.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <cmath>
#include <vector>

namespace flexUI {

enum class BackgroundClip {
  BorderBox,
  PaddingBox,
  ContentBox,
};

enum class TextTransform {
  None,
  Uppercase,
  Lowercase,
  Capitalize,
};

struct AnimationStyleEntry {
  std::string name;
  float duration_ms = 0.0f;
  float delay_ms = 0.0f;
  EasingType timing = EasingType::Ease;
  float iteration_count = 1.0f;
  bool infinite = false;
  AnimationFillMode fill_mode = AnimationFillMode::None;
  AnimationDirection direction = AnimationDirection::Normal;
  AnimationPlayState play_state = AnimationPlayState::Running;

  bool equals_ignoring_play_state(const AnimationStyleEntry& other) const {
    return name == other.name &&
           std::fabs(duration_ms - other.duration_ms) <= 0.001f &&
           std::fabs(delay_ms - other.delay_ms) <= 0.001f &&
           timing == other.timing &&
           std::fabs(iteration_count - other.iteration_count) <= 0.001f &&
           infinite == other.infinite &&
           fill_mode == other.fill_mode &&
           direction == other.direction;
  }
};

struct BackgroundImageLayer {
  bool has_gradient = false;
  bool has_image_url = false;
  LinearGradient gradient;
  std::string gradient_type = "linear";
  std::string image_url;
  std::string radial_position = "center";
  std::string radial_size = "farthest-corner";
  std::string position;
  std::string size;
  std::string repeat;
  std::string origin;
  std::string clip;
};

struct CachedColorValue {
  std::string resolved;
  Color color{};
};

struct CachedFloatValue {
  std::string resolved;
  float value = 0.0f;
};

struct TextShadow {
  float offset_x = 0.0f;
  float offset_y = 0.0f;
  float blur_radius = 0.0f;
  Color color{0.0f, 0.0f, 0.0f, 1.0f};
};

enum class CssSizeKind {
  Auto,
  Length,
  Percentage,
  Expression,
};

struct CssSize {
  CssSizeKind kind = CssSizeKind::Auto;
  float value = 0.0f;
  std::string expression;
};

/**
 * ComputedStyle - 计算后的样式
 *
 * 设计原则：
 * 1. 只包含已解析的值（不是字符串）
 * 2. 所有元素共享的通用属性
 * 3. Widget 特定属性通过 CSS variables
 */
struct ComputedStyle {
  // ========== 盒模型 ==========
  BoxSizing box_sizing = BoxSizing::ContentBox;
  CssSize width_size;
  CssSize height_size;
  // Compatibility projections. New code should inspect width_size/height_size.
  float width = 0;
  float height = 0;
  bool width_is_percent = false;   // true if width was specified as percentage
  bool height_is_percent = false;  // true if height was specified as percentage
  float padding[4] = {0};  // top, right, bottom, left
  float margin[4] = {0};
  float border_width[4] = {0};
  BorderStyle border_style[4] = {BorderStyle::Solid, BorderStyle::Solid,
                                 BorderStyle::Solid, BorderStyle::Solid};
  float border_radius[4] = {0};

  // ========== 颜色 ==========
  Color background_color{1.0f, 1.0f, 1.0f, 0.0f};  // transparent white
  Color text_color{0.0f, 0.0f, 0.0f, 1.0f};        // black
  Color border_color{0.0f, 0.0f, 0.0f, 1.0f};      // black
  Color border_colors[4] = {
      Color{0.0f, 0.0f, 0.0f, 1.0f},
      Color{0.0f, 0.0f, 0.0f, 1.0f},
      Color{0.0f, 0.0f, 0.0f, 1.0f},
      Color{0.0f, 0.0f, 0.0f, 1.0f}};
  bool has_border_side_colors = false;

  // 渐变（如果 background 是渐变）
  bool has_gradient = false;
  LinearGradient gradient;
  BackgroundClip background_clip = BackgroundClip::BorderBox;
  BackgroundClip background_origin = BackgroundClip::PaddingBox;
  std::vector<BackgroundImageLayer> background_layers;

  // 阴影
  bool has_shadow = false;
  BoxShadow shadow;
  std::vector<BoxShadow> shadows;
  float outline_width = 0.0f;
  float outline_offset = 0.0f;
  Color outline_color{0.0f, 0.0f, 0.0f, 0.0f};
  BorderStyle outline_style = BorderStyle::Solid;
  float ring_width = 0.0f;
  float ring_offset = 0.0f;
  Color ring_color{0.0f, 0.0f, 0.0f, 0.0f};
  Color ring_offset_color{0.0f, 0.0f, 0.0f, 0.0f};

  // ========== 字体 ==========
  std::string font_family = "Arial";
  float font_size = 16.0f;
  FontWeight font_weight = FontWeight::Normal;
  FontStyle font_style = FontStyle::Normal;
  Direction direction = Direction::Ltr;
  UnicodeBidi unicode_bidi = UnicodeBidi::Normal;
  TextAlign text_align = TextAlign::Left;
  TextTransform text_transform = TextTransform::None;
  float letter_spacing = 0.0f;
  float word_spacing = 0.0f;
  float text_indent = 0.0f;
  float tab_size = 8.0f;
  bool has_text_shadow = false;
  TextShadow text_shadow;
  std::vector<TextShadow> text_shadows;

  // ========== 布局 ==========
  Display display = Display::Block;
  Position position = Position::Static;

  // Flexbox
  FlexDirection flex_direction = FlexDirection::Row;
  JustifyContent justify_content = JustifyContent::Start;
  AlignItems align_items = AlignItems::Stretch;
  float gap = 0;

  // Position offsets (NAN = auto)
  float top = NAN;
  float right = NAN;
  float bottom = NAN;
  float left = NAN;

  // ========== 视觉效果 ==========
  float opacity = 1.0f;
  Visibility visibility = Visibility::Visible;

  // Transform
  float transform_x = 0;
  float transform_y = 0;
  float transform_scale = 1.0f;
  float transform_scale_x = 1.0f;
  float transform_scale_y = 1.0f;
  float transform_rotate = 0;  // degrees
  bool has_transform_matrix = false;
  flex::Transform transform_matrix{};
  float transform_origin_x = 0.5f;
  float transform_origin_y = 0.5f;
  bool transform_origin_x_percent = true;
  bool transform_origin_y_percent = true;

  // Overflow
  Overflow overflow_x = Overflow::Visible;
  Overflow overflow_y = Overflow::Visible;

  // Z-index
  int z_index = 0;

  // ========== Transitions ==========
  std::string transition;  // CSS transition shorthand

  // ========== Animations ==========
  std::string animation_name;
  float animation_duration_ms = 0.0f;
  float animation_delay_ms = 0.0f;
  EasingType animation_timing = EasingType::Ease;
  float animation_iteration_count = 1.0f;
  bool animation_infinite = false;
  AnimationFillMode animation_fill_mode = AnimationFillMode::None;
  AnimationDirection animation_direction = AnimationDirection::Normal;
  AnimationPlayState animation_play_state = AnimationPlayState::Running;
  std::vector<AnimationStyleEntry> animations;

  // ========== CSS Variables ==========
  std::unordered_map<Symbol, std::string, SymbolHash> variables;
  mutable std::unordered_map<Symbol, CachedColorValue, SymbolHash> color_cache;
  mutable std::unordered_map<Symbol, CachedFloatValue, SymbolHash> float_cache;

  /**
   * Resolve var(--name) references recursively
   */
  std::string resolve_variable_value(const std::string& value, int depth = 0) const;

  /**
   * 获取 CSS variable
   */
  std::string get_variable(Symbol name,
                          const std::string& default_value = "") const;

  /**
   * 获取 CSS variable 并解析为颜色
   */
  Color get_variable_color(Symbol name,
                          const Color& default_color) const;

  /**
   * 获取 CSS variable 并解析为浮点数
   */
  float get_variable_float(Symbol name, float default_value) const;

  /**
   * Hash raw CSS variable entries for render-cache keys.
   */
  uint64_t variables_signature() const;
};

} // namespace flexUI

#endif // FLEXUI_COMPUTED_STYLE_H
