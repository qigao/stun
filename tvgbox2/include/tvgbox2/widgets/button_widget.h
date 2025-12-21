/*
 * tvgbox2 - ButtonWidget
 *
 * 现代化按钮控件 - 所有行为由 CSS 定义
 */

#ifndef TVGBOX2_BUTTON_WIDGET_H
#define TVGBOX2_BUTTON_WIDGET_H

#include "../widget.h"
#include <string>
#include <vector>

namespace tvgbox2 {

/**
 * ButtonWidget - 现代化按钮
 *
 * 设计理念：Tailwind/Shadcn 风格 - 所有行为通过 CSS 配置
 *
 * CSS 变量支持：
 *   --ripple: "true" | "false"           // Material Design 涟漪效果
 *   --ripple-color: "r,g,b,a"            // 涟漪颜色
 *   --transition-duration: "300"         // 过渡动画时长（ms）
 *   --hover-scale: "1.05"                // hover 缩放比例
 *   --active-scale: "0.95"               // active 缩放比例
 *   --loading-spinner-color: "r,g,b,a"   // 加载动画颜色
 *   --icon-position: "left" | "right"    // 图标位置
 *   --icon-spacing: "8"                  // 图标间距（px）
 *
 * 伪状态支持：
 *   :hover    - 鼠标悬停
 *   :active   - 鼠标按下
 *   :focus    - 键盘焦点
 *   :disabled - 禁用状态
 *   :loading  - 加载状态（显示 spinner，禁用点击）
 *
 * 示例用法（CSS）：
 *   button.primary {
 *     background-color: #3b82f6;
 *     --ripple: true;
 *     --transition-duration: 300;
 *     --hover-scale: 1.05;
 *   }
 *   button.primary:hover {
 *     background-color: #2563eb;
 *   }
 *   button.primary:active {
 *     --active-scale: 0.95;
 *   }
 */
class ButtonWidget : public Widget {
public:
  /**
   * 构造函数
   *
   * @param text 按钮文本（可选，也可以通过 Element.text_content 设置）
   */
  explicit ButtonWidget(const std::string& text = "");

  // ========================================================================
  // Widget 接口实现
  // ========================================================================

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "ButtonWidget"; }

  // ========================================================================
  // 文本访问（也可以直接用 Element.text_content）
  // ========================================================================

  const std::string& text() const { return text_; }
  void set_text(const std::string& text) { text_ = text; dirty_ = true; }

  // ========================================================================
  // 状态控制
  // ========================================================================

  bool is_disabled() const { return disabled_; }
  void set_disabled(bool disabled) { disabled_ = disabled; dirty_ = true; }

  bool is_loading() const { return loading_; }
  void set_loading(bool loading) { loading_ = loading; dirty_ = true; }

private:
  // ========== 渲染辅助 ==========

  void render_background(tvg::Scene* scene, const Element& elem);
  void render_text(tvg::Scene* scene, const Element& elem);
  void render_ripples(tvg::Scene* scene, const Element& elem);
  void render_loading_spinner(tvg::Scene* scene, const Element& elem);

  // ========== 事件处理 ==========

  bool handle_mouse_down(const Event& event, Element& elem);
  bool handle_mouse_up(const Event& event, Element& elem);

  // ========== 动画更新 ==========

  void update_transitions(float delta_ms, Element& elem);
  void update_ripples(float delta_ms, Element& elem);
  void update_spinner(float delta_ms, Element& elem);

  // ========== 涟漪效果 ==========

  struct Ripple {
    float x, y;           // 涟漪中心（相对于按钮）
    float radius;         // 当前半径
    float max_radius;     // 最大半径
    float alpha;          // 透明度（0-1）
    bool finished = false;
  };

  void add_ripple(float x, float y, const Element& elem);

  // ========== 状态 ==========

  std::string text_;
  bool disabled_ = false;
  bool loading_ = false;

  // 涟漪效果
  std::vector<Ripple> ripples_;

  // 过渡动画状态
  float current_scale_ = 1.0f;
  float target_scale_ = 1.0f;

  // 加载动画
  float spinner_rotation_ = 0;  // 度数
};

} // namespace tvgbox2

#endif // TVGBOX2_BUTTON_WIDGET_H
