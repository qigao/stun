/*
 * tvgbox2 - SliderWidget
 *
 * 滑块控件 - 所有行为由 CSS 定义
 */

#ifndef TVGBOX2_SLIDER_WIDGET_H
#define TVGBOX2_SLIDER_WIDGET_H

#include "../widget.h"
#include <functional>

namespace tvgbox2 {

/**
 * SliderWidget - 数值滑块
 *
 * 设计理念：连续值输入，支持拖拽和动画
 *
 * CSS 变量支持：
 *   --track-height: "4"                    // 轨道高度（px）
 *   --track-bg: "r,g,b,a"                  // 轨道背景色
 *   --track-fill: "r,g,b,a"                // 轨道填充色（已滑过的部分）
 *   --thumb-size: "20"                     // 滑块大小（px）
 *   --thumb-bg: "r,g,b,a"                  // 滑块背景色
 *   --thumb-border: "r,g,b,a"              // 滑块边框色
 *   --transition-duration: "100"           // 过渡动画时长（ms）
 *   --show-value: "true" | "false"         // 是否显示数值
 *
 * 伪状态支持：
 *   :hover     - 鼠标悬停
 *   :active    - 正在拖拽
 *   :disabled  - 禁用状态
 *   :focus     - 键盘焦点
 *
 * 示例用法（CSS）：
 *   slider {
 *     --track-height: 4;
 *     --track-bg: 229,231,235,255;
 *     --track-fill: 59,130,246,255;
 *     --thumb-size: 20;
 *     --thumb-bg: 255,255,255,255;
 *   }
 */
class SliderWidget : public Widget {
public:
  /**
   * 构造函数
   *
   * @param min 最小值
   * @param max 最大值
   * @param value 初始值
   * @param step 步长（0 = 连续）
   */
  explicit SliderWidget(float min = 0.0f, float max = 100.0f,
                       float value = 50.0f, float step = 0.0f);

  // ========================================================================
  // Widget 接口实现
  // ========================================================================

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "SliderWidget"; }

  // ========================================================================
  // 值访问
  // ========================================================================

  float value() const { return value_; }
  void set_value(float value);

  float min() const { return min_; }
  void set_min(float min) { min_ = min; update_value_position(); dirty_ = true; }

  float max() const { return max_; }
  void set_max(float max) { max_ = max; update_value_position(); dirty_ = true; }

  float step() const { return step_; }
  void set_step(float step) { step_ = step; }

  bool is_disabled() const { return disabled_; }
  void set_disabled(bool disabled) { disabled_ = disabled; dirty_ = true; }

  // ========================================================================
  // 回调
  // ========================================================================

  using ChangeCallback = std::function<void(float value)>;
  void set_change_callback(ChangeCallback callback) { change_callback_ = callback; }

private:
  // ========== 渲染辅助 ==========

  void render_track(tvg::Scene* scene, const Element& elem);
  void render_fill(tvg::Scene* scene, const Element& elem);
  void render_thumb(tvg::Scene* scene, const Element& elem);
  void render_value_label(tvg::Scene* scene, const Element& elem);

  // ========== 事件处理 ==========

  bool handle_mouse_down(const Event& event, Element& elem);
  bool handle_mouse_move(const Event& event, Element& elem);
  bool handle_mouse_up(const Event& event, Element& elem);
  bool handle_key_down(const Event& event, Element& elem);

  // ========== 值计算 ==========

  void update_value_from_x(float x, const Element& elem);
  void update_value_position();
  float snap_to_step(float value);

  // ========== 动画更新 ==========

  void update_transitions(float delta_ms, Element& elem);

  // ========== 状态 ==========

  float min_ = 0.0f;
  float max_ = 100.0f;
  float value_ = 50.0f;
  float step_ = 0.0f;

  bool disabled_ = false;
  bool is_dragging_ = false;

  // 动画状态
  float value_position_ = 0.5f;  // 归一化位置（0-1）
  float current_thumb_scale_ = 1.0f;
  float target_thumb_scale_ = 1.0f;

  // 回调
  ChangeCallback change_callback_;
};

} // namespace tvgbox2

#endif // TVGBOX2_SLIDER_WIDGET_H
