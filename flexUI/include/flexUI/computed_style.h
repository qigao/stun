/*
 * flexUI - Computed Style
 *
 * 计算后的样式（已解析的 CSS 属性值）
 */

#ifndef FLEXUI_COMPUTED_STYLE_H
#define FLEXUI_COMPUTED_STYLE_H

#include "types.h"
#include <string>
#include <unordered_map>
#include <cmath>

namespace flexUI {

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
  float width = 0;         // auto = 0
  float height = 0;
  bool width_is_percent = false;   // true if width was specified as percentage
  bool height_is_percent = false;  // true if height was specified as percentage
  float padding[4] = {0};  // top, right, bottom, left
  float margin[4] = {0};
  float border_width[4] = {0};
  float border_radius[4] = {0};

  // ========== 颜色 ==========
  Color background_color{1.0f, 1.0f, 1.0f, 0.0f};  // transparent white
  Color text_color{0.0f, 0.0f, 0.0f, 1.0f};        // black
  Color border_color{0.0f, 0.0f, 0.0f, 1.0f};      // black

  // 渐变（如果 background 是渐变）
  bool has_gradient = false;
  LinearGradient gradient;

  // 阴影
  bool has_shadow = false;
  BoxShadow shadow;

  // ========== 字体 ==========
  std::string font_family = "Arial";
  float font_size = 16.0f;
  FontWeight font_weight = FontWeight::Normal;
  FontStyle font_style = FontStyle::Normal;
  TextAlign text_align = TextAlign::Left;

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
  float transform_rotate = 0;  // degrees

  // Overflow
  Overflow overflow_x = Overflow::Visible;
  Overflow overflow_y = Overflow::Visible;

  // Z-index
  int z_index = 0;

  // ========== Transitions ==========
  std::string transition;  // CSS transition shorthand

  // ========== CSS Variables ==========
  std::unordered_map<Symbol, std::string, SymbolHash> variables;

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
};

} // namespace flexUI

#endif // FLEXUI_COMPUTED_STYLE_H
