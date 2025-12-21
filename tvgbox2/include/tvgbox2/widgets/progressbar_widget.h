/*
 * tvgbox2 - ProgressBarWidget
 *
 * 进度条控件 - CSS 控制样式和动画
 */

#ifndef TVGBOX2_PROGRESSBAR_WIDGET_H
#define TVGBOX2_PROGRESSBAR_WIDGET_H

#include "../widget.h"

namespace tvgbox2 {

/**
 * ProgressBarWidget - 进度条
 *
 * 设计理念：纯视觉反馈，支持确定和不确定进度
 *
 * CSS 变量支持：
 *   --progress-height: "8"                 // 进度条高度
 *   --progress-bg: "r,g,b,a"               // 背景色
 *   --progress-fill: "r,g,b,a"             // 填充色
 *   --progress-border-radius: "4"          // 圆角
 *   --indeterminate-animation-duration: "1500"  // 不确定模式动画时长
 *
 * 伪状态支持：
 *   :indeterminate  - 不确定模式（无限循环动画）
 *
 * 示例用法（CSS）：
 *   progressbar {
 *     --progress-height: 8;
 *     --progress-bg: 229,231,235,255;
 *     --progress-fill: 59,130,246,255;
 *   }
 *   progressbar:indeterminate {
 *     --indeterminate-animation-duration: 1500;
 *   }
 */
class ProgressBarWidget : public Widget {
public:
  /**
   * 构造函数
   *
   * @param value 初始值 (0-100)
   * @param indeterminate 是否为不确定模式
   */
  explicit ProgressBarWidget(float value = 0.0f, bool indeterminate = false);

  // ========================================================================
  // Widget 接口实现
  // ========================================================================

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "ProgressBarWidget"; }

  // ========================================================================
  // 值访问
  // ========================================================================

  float value() const { return value_; }
  void set_value(float value);

  bool is_indeterminate() const { return indeterminate_; }
  void set_indeterminate(bool indeterminate);

private:
  // ========== 渲染辅助 ==========

  void render_background(tvg::Scene* scene, const Element& elem);
  void render_fill_determinate(tvg::Scene* scene, const Element& elem);
  void render_fill_indeterminate(tvg::Scene* scene, const Element& elem);

  // ========== 动画更新 ==========

  void update_indeterminate_animation(float delta_ms, Element& elem);

  // ========== 状态 ==========

  float value_ = 0.0f;  // 0-100
  bool indeterminate_ = false;

  // 不确定模式动画
  float animation_time_ = 0.0f;  // ms
  float animation_position_ = 0.0f;  // 0-1
};

} // namespace tvgbox2

#endif // TVGBOX2_PROGRESSBAR_WIDGET_H
