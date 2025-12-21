/*
 * tvgbox2 - ProgressBarWidget Implementation
 */

#include <tvgbox2/widgets/progressbar_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/renderer.h>
#include <thorvg.h>
#include <algorithm>
#include <cmath>

namespace tvgbox2 {

// ============================================================================
// 构造函数
// ============================================================================

ProgressBarWidget::ProgressBarWidget(float value, bool indeterminate)
    : value_(std::max(0.0f, std::min(100.0f, value))),
      indeterminate_(indeterminate) {}

// ============================================================================
// 值访问
// ============================================================================

void ProgressBarWidget::set_value(float value) {
  value_ = std::max(0.0f, std::min(100.0f, value));
  dirty_ = true;
}

void ProgressBarWidget::set_indeterminate(bool indeterminate) {
  if (indeterminate_ != indeterminate) {
    indeterminate_ = indeterminate;
    animation_time_ = 0.0f;
    animation_position_ = 0.0f;
    dirty_ = true;
  }
}

// ============================================================================
// Widget 接口实现
// ============================================================================

void ProgressBarWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  // 1. 渲染背景
  render_background(scene, elem);

  // 2. 渲染填充
  if (indeterminate_ || elem.has_state("indeterminate")) {
    render_fill_indeterminate(scene, elem);
  } else {
    render_fill_determinate(scene, elem);
  }
}

bool ProgressBarWidget::handle_event(const Event& event, Element& elem) {
  // ProgressBar 无交互
  return false;
}

void ProgressBarWidget::update(float delta_ms, Element& elem) {
  if (indeterminate_ || elem.has_state("indeterminate")) {
    update_indeterminate_animation(delta_ms, elem);
  }
}

// ============================================================================
// 渲染辅助
// ============================================================================

void ProgressBarWidget::render_background(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float height = style->get_variable_float("--progress-height", 8.0f);
  float radius = style->get_variable_float("--progress-border-radius", 4.0f);
  Color bg_color = style->get_variable_color("--progress-bg", {229, 231, 235, 255});

  float y = (elem.height() - height) / 2;

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, y, elem.width(), height, radius, radius);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);

  scene->push(std::move(bg));
}

void ProgressBarWidget::render_fill_determinate(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  if (value_ <= 0.0f) return;  // 无进度

  float height = style->get_variable_float("--progress-height", 8.0f);
  float radius = style->get_variable_float("--progress-border-radius", 4.0f);
  Color fill_color = style->get_variable_color("--progress-fill", {59, 130, 246, 255});

  float y = (elem.height() - height) / 2;
  float fill_width = elem.width() * (value_ / 100.0f);

  if (fill_width > 0) {
    auto fill = tvg::Shape::gen();
    fill->appendRect(0, y, fill_width, height, radius, radius);
    fill->fill(fill_color.r, fill_color.g, fill_color.b, fill_color.a);

    scene->push(std::move(fill));
  }
}

void ProgressBarWidget::render_fill_indeterminate(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float height = style->get_variable_float("--progress-height", 8.0f);
  float radius = style->get_variable_float("--progress-border-radius", 4.0f);
  Color fill_color = style->get_variable_color("--progress-fill", {59, 130, 246, 255});

  float y = (elem.height() - height) / 2;

  // 不确定模式：绘制移动的进度条片段
  float segment_width = elem.width() * 0.3f;  // 30% 宽度的片段
  float x = (elem.width() - segment_width) * animation_position_;

  auto fill = tvg::Shape::gen();
  fill->appendRect(x, y, segment_width, height, radius, radius);
  fill->fill(fill_color.r, fill_color.g, fill_color.b, fill_color.a);

  scene->push(std::move(fill));
}

// ============================================================================
// 动画更新
// ============================================================================

void ProgressBarWidget::update_indeterminate_animation(float delta_ms, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float duration = style->get_variable_float("--indeterminate-animation-duration", 1500.0f);

  animation_time_ += delta_ms;

  // 循环动画
  if (animation_time_ >= duration) {
    animation_time_ -= duration;
  }

  // 计算位置（0 → 1 → 0，形成来回移动）
  float progress = animation_time_ / duration;

  // 使用 sine 波形产生平滑来回运动
  animation_position_ = (std::sin(progress * 2 * 3.14159f) + 1.0f) / 2.0f;

  elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
}

} // namespace tvgbox2
