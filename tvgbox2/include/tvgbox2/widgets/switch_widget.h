/*
 * tvgbox2 - SwitchWidget
 *
 * 滑动开关控件 - CSS 控制样式
 */

#ifndef TVGBOX2_SWITCH_WIDGET_H
#define TVGBOX2_SWITCH_WIDGET_H

#include "../widget.h"
#include <string>
#include <functional>

namespace tvgbox2 {

/**
 * SwitchWidget - 滑动开关（iOS 风格）
 *
 * 设计理念：视觉化的布尔开关，滑动动画
 *
 * CSS 变量支持：
 *   --switch-width: "50"                   // 开关宽度
 *   --switch-height: "28"                  // 开关高度
 *   --switch-bg-off: "r,g,b,a"             // 关闭状态背景
 *   --switch-bg-on: "r,g,b,a"              // 开启状态背景
 *   --switch-thumb: "r,g,b,a"              // 滑块颜色
 *   --transition-duration: "200"           // 滑动动画时长（ms）
 *   --label-spacing: "8"                   // 文字与开关间距
 *
 * 伪状态支持：
 *   :checked   - 开启状态
 *   :hover     - 鼠标悬停
 *   :disabled  - 禁用状态
 *   :focus     - 键盘焦点
 *
 * 示例用法（CSS）：
 *   switch {
 *     --switch-width: 50;
 *     --switch-height: 28;
 *     --switch-bg-off: 200,200,200,255;
 *     --switch-bg-on: 34,197,94,255;
 *     --transition-duration: 200;
 *   }
 *   switch:checked {
 *     --switch-thumb: 255,255,255,255;
 *   }
 *
 * 对比 CheckboxWidget：
 *   - Checkbox: 方形，勾选标记，传统表单风格
 *   - Switch: 圆角矩形，滑动拨片，现代移动风格
 */
class SwitchWidget : public Widget {
public:
  /**
   * 构造函数
   *
   * @param label 文字标签
   * @param checked 初始开启状态
   */
  explicit SwitchWidget(const std::string& label = "", bool checked = false);

  // ========================================================================
  // Widget 接口实现
  // ========================================================================

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "SwitchWidget"; }

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

  void render_track(tvg::Scene* scene, const Element& elem);
  void render_thumb(tvg::Scene* scene, const Element& elem);
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
  float thumb_position_ = 0.0f;       // 滑块位置（0-1）
  float target_thumb_position_ = 0.0f;

  // 回调
  ChangeCallback change_callback_;
};

} // namespace tvgbox2

#endif // TVGBOX2_SWITCH_WIDGET_H
