/*
 * flexUI - LayoutManager
 *
 * 布局计算：CSS 属性同步到 flex 布局系统
 */

#ifndef FLEXUI_LAYOUT_MANAGER_H
#define FLEXUI_LAYOUT_MANAGER_H

#include <flex/runtime/group.h>
#include <optional>
#include "types.h"

namespace flexUI {

class Element;

/**
 * LayoutManager - 布局管理器
 *
 * 职责：
 * 1. 同步 CSS 属性到 flex 布局属性
 * 2. 计算 auto 尺寸
 * 3. 执行递归布局
 */
class LayoutManager {
public:
  // 同步 CSS 属性到 flex 布局（递归）
  static void sync_to_flex(Element* elem, float container_w, float container_h);

  // 执行布局（递归）
  static void perform_layout(Element* elem);

private:
  static void sync_to_flex_impl(
      Element* elem, float container_w, float container_h,
      std::optional<float> used_width = std::nullopt,
      std::optional<float> used_height = std::nullopt);

  // 类型转换（内联实现）
  static flex::PositionMode to_flex_position(Position p) {
    switch (p) {
      case Position::Static:   return flex::PositionMode::Static;
      case Position::Relative: return flex::PositionMode::Relative;
      case Position::Absolute: return flex::PositionMode::Absolute;
      case Position::Fixed:    return flex::PositionMode::Fixed;
      case Position::Sticky:   return flex::PositionMode::Relative;
      default:                 return flex::PositionMode::Static;
    }
  }

  static flex::BoxSizing to_flex_box_sizing(BoxSizing b) {
    switch (b) {
      case BoxSizing::ContentBox: return flex::BoxSizing::ContentBox;
      case BoxSizing::BorderBox:  return flex::BoxSizing::BorderBox;
      default:                    return flex::BoxSizing::ContentBox;
    }
  }
};

} // namespace flexUI

#endif // FLEXUI_LAYOUT_MANAGER_H
