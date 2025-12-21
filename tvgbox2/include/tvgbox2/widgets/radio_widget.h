/*
 * tvgbox2 - RadioWidget
 *
 * 单选按钮控件 - CSS 控制样式
 */

#ifndef TVGBOX2_RADIO_WIDGET_H
#define TVGBOX2_RADIO_WIDGET_H

#include "../widget.h"
#include <string>
#include <functional>

namespace tvgbox2 {

/**
 * RadioWidget - 单选按钮
 *
 * 设计理念：组内互斥选择（通过回调实现分组）
 *
 * CSS 变量支持：
 *   --radio-size: "20"                     // 单选按钮大小（px）
 *   --radio-bg: "r,g,b,a"                  // 未选中背景色
 *   --radio-bg-checked: "r,g,b,a"          // 选中背景色
 *   --radio-border: "r,g,b,a"              // 边框颜色
 *   --radio-dot: "r,g,b,a"                 // 内圆点颜色
 *   --transition-duration: "200"           // 过渡动画时长（ms）
 *   --label-spacing: "8"                   // 文字与按钮间距
 *
 * 伪状态支持：
 *   :checked   - 选中状态
 *   :hover     - 鼠标悬停
 *   :disabled  - 禁用状态
 *   :focus     - 键盘焦点
 *
 * 示例用法（CSS）：
 *   radio {
 *     --radio-size: 20;
 *     --radio-bg: 255,255,255,255;
 *     --radio-bg-checked: 59,130,246,255;
 *     --transition-duration: 200;
 *   }
 *   radio:checked {
 *     --radio-dot: 255,255,255,255;
 *   }
 *
 * 分组使用（通过回调）：
 *   // 创建组
 *   std::string current_selection;
 *   auto on_change = [&](RadioWidget* radio, const std::string& value) {
 *     current_selection = value;
 *     // 通知其他 radio 取消选中
 *   };
 */
class RadioWidget : public Widget {
public:
  /**
   * 构造函数
   *
   * @param label 文字标签
   * @param value 该选项的值
   * @param group 分组名（用于识别同组 radio）
   * @param checked 初始选中状态
   */
  explicit RadioWidget(const std::string& label = "",
                      const std::string& value = "",
                      const std::string& group = "",
                      bool checked = false);

  // ========================================================================
  // Widget 接口实现
  // ========================================================================

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "RadioWidget"; }

  // ========================================================================
  // 状态访问
  // ========================================================================

  bool is_checked() const { return checked_; }
  void set_checked(bool checked);

  const std::string& label() const { return label_; }
  void set_label(const std::string& label) { label_ = label; dirty_ = true; }

  const std::string& value() const { return value_; }
  void set_value(const std::string& value) { value_ = value; }

  const std::string& group() const { return group_; }
  void set_group(const std::string& group) { group_ = group; }

  bool is_disabled() const { return disabled_; }
  void set_disabled(bool disabled) { disabled_ = disabled; dirty_ = true; }

  // ========================================================================
  // 回调
  // ========================================================================

  using ChangeCallback = std::function<void(const std::string& value, const std::string& group)>;
  void set_change_callback(ChangeCallback callback) { change_callback_ = callback; }

private:
  // ========== 渲染辅助 ==========

  void render_radio_circle(tvg::Scene* scene, const Element& elem);
  void render_dot(tvg::Scene* scene, const Element& elem);
  void render_label(tvg::Scene* scene, const Element& elem);

  // ========== 事件处理 ==========

  bool handle_mouse_down(const Event& event, Element& elem);
  bool handle_key_down(const Event& event, Element& elem);

  // ========== 动画更新 ==========

  void update_transitions(float delta_ms, const Element& elem);

  // ========== 状态 ==========

  bool checked_ = false;
  std::string label_;
  std::string value_;
  std::string group_;
  bool disabled_ = false;

  // 动画状态
  float dot_scale_ = 0.0f;          // 内圆点缩放（0-1）
  float target_dot_scale_ = 0.0f;

  // 回调
  ChangeCallback change_callback_;
};

} // namespace tvgbox2

#endif // TVGBOX2_RADIO_WIDGET_H
