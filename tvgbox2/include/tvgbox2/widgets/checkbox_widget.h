/*
 * tvgbox2 - CheckboxWidget
 *
 * 复选框控件 - 所有行为由 CSS 定义
 */

#ifndef TVGBOX2_CHECKBOX_WIDGET_H
#define TVGBOX2_CHECKBOX_WIDGET_H

#include "../widget.h"
#include <string>
#include <functional>

namespace tvgbox2 {

/**
 * CheckboxWidget - 复选框
 *
 * 设计理念：布尔值输入，支持动画和自定义样式
 *
 * CSS 变量支持：
 *   --checkbox-size: "20"                    // 复选框大小（px）
 *   --checkbox-bg: "r,g,b,a"                 // 未选中背景色
 *   --checkbox-bg-checked: "r,g,b,a"         // 选中背景色
 *   --checkbox-border: "r,g,b,a"             // 边框颜色
 *   --checkbox-checkmark: "r,g,b,a"          // 勾选标记颜色
 *   --transition-duration: "200"             // 过渡动画时长（ms）
 *   --label-spacing: "8"                     // 文字与复选框间距
 *
 * 伪状态支持：
 *   :checked   - 选中状态
 *   :hover     - 鼠标悬停
 *   :disabled  - 禁用状态
 *   :focus     - 键盘焦点
 *
 * 示例用法（CSS）：
 *   checkbox {
 *     --checkbox-size: 20;
 *     --checkbox-bg: 255,255,255,255;
 *     --checkbox-bg-checked: 59,130,246,255;
 *     --transition-duration: 200;
 *   }
 *   checkbox:checked {
 *     --checkbox-checkmark: 255,255,255,255;
 *   }
 */
class CheckboxWidget : public Widget {
public:
  /**
   * 构造函数
   *
   * @param label 文字标签（可选）
   * @param checked 初始选中状态
   */
  explicit CheckboxWidget(const std::string& label = "", bool checked = false);

  // ========================================================================
  // Widget 接口实现
  // ========================================================================

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "CheckboxWidget"; }

  // ========================================================================
  // 状态访问
  // ========================================================================

  bool is_checked() const { return checked_; }
  void set_checked(bool checked);

  const std::string& label() const { return label_; }
  void set_label(const std::string& label) { label_ = label; dirty_ = true; }

  bool is_disabled() const { return disabled_; }
  void set_disabled(bool disabled) { disabled_ = disabled; dirty_ = true; }

  // ========================================================================
  // 回调
  // ========================================================================

  using ChangeCallback = std::function<void(bool checked)>;
  void set_change_callback(ChangeCallback callback) { change_callback_ = callback; }

private:
  // ========== 渲染辅助 ==========

  void render_checkbox_box(tvg::Scene* scene, const Element& elem);
  void render_checkmark(tvg::Scene* scene, const Element& elem);
  void render_label(tvg::Scene* scene, const Element& elem);

  // ========== 事件处理 ==========

  bool handle_mouse_down(const Event& event, Element& elem);
  bool handle_key_down(const Event& event, Element& elem);

  // ========== 动画更新 ==========

  void update_transitions(float delta_ms, Element& elem);

  // ========== 状态 ==========

  bool checked_ = false;
  std::string label_;
  bool disabled_ = false;

  // 动画状态
  float checkmark_scale_ = 0.0f;    // 勾选标记缩放（0-1）
  float target_checkmark_scale_ = 0.0f;

  // 回调
  ChangeCallback change_callback_;
};

} // namespace tvgbox2

#endif // TVGBOX2_CHECKBOX_WIDGET_H
