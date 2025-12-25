/*
 * tvgbox2 - Core Types
 *
 * 基础类型定义（颜色、枚举等）
 * Uses flex::Color for unified type system across flex and tvgbox2.
 */

#ifndef TVGBOX2_TYPES_H
#define TVGBOX2_TYPES_H

#include <cstdint>
#include "flex/runtime/types.h"

namespace tvgbox2 {

// ============================================================================
// 颜色 - Use flex::Color directly
// ============================================================================
// flex::Color uses float (0.0-1.0) for r,g,b,a
// This replaces the old uint8_t based Color

using Color = flex::Color;

// Helper to create Color from uint8_t values (0-255)
inline Color color_from_u8(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

// Helper to convert Color to uint8_t values
inline void color_to_u8(const Color& c, uint8_t& r, uint8_t& g, uint8_t& b, uint8_t& a) {
    r = static_cast<uint8_t>(c.r * 255.0f);
    g = static_cast<uint8_t>(c.g * 255.0f);
    b = static_cast<uint8_t>(c.b * 255.0f);
    a = static_cast<uint8_t>(c.a * 255.0f);
}

// Lerp helper (flex::Color doesn't have this)
inline Color color_lerp(const Color& a, const Color& b, float t) {
    return Color(
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t
    );
}

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
  Color color{0.0f, 0.0f, 0.0f, 0.5f};  // black with 50% opacity
  bool inset = false;
};

} // namespace tvgbox2

#endif // TVGBOX2_TYPES_H
