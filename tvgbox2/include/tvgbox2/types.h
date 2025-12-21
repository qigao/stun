/*
 * tvgbox2 - Core Types
 *
 * 基础类型定义（颜色、枚举等）
 */

#ifndef TVGBOX2_TYPES_H
#define TVGBOX2_TYPES_H

#include <cstdint>

namespace tvgbox2 {

// ============================================================================
// 颜色
// ============================================================================

struct Color {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 255;

  Color() = default;
  Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
    : r(r), g(g), b(b), a(a) {}

  bool operator==(const Color& other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }

  bool operator!=(const Color& other) const {
    return !(*this == other);
  }

  static Color lerp(const Color& a, const Color& b, float t) {
    return Color(
      static_cast<uint8_t>(a.r + (b.r - a.r) * t),
      static_cast<uint8_t>(a.g + (b.g - a.g) * t),
      static_cast<uint8_t>(a.b + (b.b - a.b) * t),
      static_cast<uint8_t>(a.a + (b.a - a.a) * t)
    );
  }
};

// ============================================================================
// CSS 枚举类型
// ============================================================================

enum class Display {
  Block,
  Inline,
  Flex,
  Grid,
  None,
};

enum class Position {
  Static,
  Relative,
  Absolute,
  Fixed,
};

enum class Visibility {
  Visible,
  Hidden,
  Collapse,
};

enum class Overflow {
  Visible,
  Hidden,
  Scroll,
  Auto,
};

enum class FlexDirection {
  Row,
  RowReverse,
  Column,
  ColumnReverse,
};

enum class JustifyContent {
  FlexStart,
  FlexEnd,
  Center,
  SpaceBetween,
  SpaceAround,
  SpaceEvenly,
};

enum class AlignItems {
  FlexStart,
  FlexEnd,
  Center,
  Baseline,
  Stretch,
};

enum class BoxSizing {
  ContentBox,
  BorderBox,
};

enum class TextAlign {
  Left,
  Center,
  Right,
  Justify,
};

enum class FontWeight {
  Normal = 400,
  Bold = 700,
  Light = 300,
  Medium = 500,
  SemiBold = 600,
  ExtraBold = 800,
  Black = 900,
};

enum class FontStyle {
  Normal,
  Italic,
  Oblique,
};

// ============================================================================
// 过渡/动画
// ============================================================================

enum class EasingType {
  Linear,
  Ease,
  EaseIn,
  EaseOut,
  EaseInOut,
  CubicBezier,
};

// ============================================================================
// 渐变
// ============================================================================

struct GradientStop {
  float offset = 0.0f;  // 0.0 to 1.0
  Color color;
};

struct LinearGradient {
  float angle = 0.0f;  // degrees
  GradientStop stops[8];  // 最多8个色标
  int stop_count = 0;
};

struct RadialGradient {
  float cx = 0.5f;  // center x (0.0 to 1.0)
  float cy = 0.5f;  // center y (0.0 to 1.0)
  float radius = 0.5f;
  GradientStop stops[8];
  int stop_count = 0;
};

// ============================================================================
// 阴影
// ============================================================================

struct BoxShadow {
  float offset_x = 0;
  float offset_y = 0;
  float blur_radius = 0;
  float spread_radius = 0;
  Color color = {0, 0, 0, 128};
  bool inset = false;
};

} // namespace tvgbox2

#endif // TVGBOX2_TYPES_H
