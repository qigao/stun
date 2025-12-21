/*
 * tvgbox2 - SliderWidget Implementation
 */

#include <tvgbox2/widgets/slider_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <thorvg.h>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <iomanip>

namespace tvgbox2 {

// ============================================================================
// 构造函数
// ============================================================================

SliderWidget::SliderWidget(float min, float max, float value, float step)
    : min_(min), max_(max), value_(value), step_(step) {
  update_value_position();
}

// ============================================================================
// 值访问
// ============================================================================

void SliderWidget::set_value(float value) {
  float clamped = std::max(min_, std::min(max_, value));

  if (step_ > 0) {
    clamped = snap_to_step(clamped);
  }

  if (value_ != clamped) {
    value_ = clamped;
    update_value_position();
    dirty_ = true;

    if (change_callback_) {
      change_callback_(value_);
    }
  }
}

// ============================================================================
// Widget 接口实现
// ============================================================================

void SliderWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  // 1. 渲染轨道
  render_track(scene, elem);

  // 2. 渲染填充部分
  render_fill(scene, elem);

  // 3. 渲染滑块
  render_thumb(scene, elem);

  // 4. 渲染数值标签（如果启用）
  auto* style = elem.computed_style;
  if (style && style->get_variable("--show-value", "false") == "true") {
    render_value_label(scene, elem);
  }
}

bool SliderWidget::handle_event(const Event& event, Element& elem) {
  if (disabled_ || elem.has_state("disabled")) {
    return false;
  }

  switch (event.type) {
    case EventType::MouseDown:
      return handle_mouse_down(event, elem);

    case EventType::MouseMove:
      return handle_mouse_move(event, elem);

    case EventType::MouseUp:
      return handle_mouse_up(event, elem);

    case EventType::KeyDown:
      return handle_key_down(event, elem);

    default:
      return false;
  }
}

void SliderWidget::update(float delta_ms, Element& elem) {
  update_transitions(delta_ms, elem);
}

// ============================================================================
// 渲染辅助
// ============================================================================

void SliderWidget::render_track(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float track_height = style->get_variable_float("--track-height", 4.0f);
  Color track_bg = style->get_variable_color("--track-bg", {229, 231, 235, 255});

  float track_y = (elem.height() - track_height) / 2;

  auto track = tvg::Shape::gen();
  float radius = track_height / 2;
  track->appendRect(0, track_y, elem.width(), track_height, radius, radius);
  track->fill(track_bg.r, track_bg.g, track_bg.b, track_bg.a);

  scene->push(std::move(track));
}

void SliderWidget::render_fill(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float track_height = style->get_variable_float("--track-height", 4.0f);
  Color track_fill = style->get_variable_color("--track-fill", {59, 130, 246, 255});

  float track_y = (elem.height() - track_height) / 2;
  float fill_width = elem.width() * value_position_;

  if (fill_width > 0) {
    auto fill = tvg::Shape::gen();
    float radius = track_height / 2;
    fill->appendRect(0, track_y, fill_width, track_height, radius, radius);
    fill->fill(track_fill.r, track_fill.g, track_fill.b, track_fill.a);

    scene->push(std::move(fill));
  }
}

void SliderWidget::render_thumb(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float thumb_size = style->get_variable_float("--thumb-size", 20.0f);
  Color thumb_bg = style->get_variable_color("--thumb-bg", {255, 255, 255, 255});
  Color thumb_border = style->get_variable_color("--thumb-border", {200, 200, 200, 255});

  // 计算 thumb 位置
  float thumb_x = elem.width() * value_position_ - thumb_size / 2;
  float thumb_y = (elem.height() - thumb_size) / 2;

  // 应用缩放动画
  float actual_size = thumb_size * current_thumb_scale_;
  float size_diff = thumb_size - actual_size;
  thumb_x += size_diff / 2;
  thumb_y += size_diff / 2;

  // 绘制 thumb 背景
  auto thumb = tvg::Shape::gen();
  thumb->appendCircle(thumb_x + actual_size / 2, thumb_y + actual_size / 2,
                     actual_size / 2, actual_size / 2);
  thumb->fill(thumb_bg.r, thumb_bg.g, thumb_bg.b, thumb_bg.a);

  scene->push(std::move(thumb));

  // 绘制 thumb 边框
  auto border = tvg::Shape::gen();
  border->appendCircle(thumb_x + actual_size / 2, thumb_y + actual_size / 2,
                      actual_size / 2, actual_size / 2);
  border->strokeFill(thumb_border.r, thumb_border.g, thumb_border.b, thumb_border.a);
  border->strokeWidth(2);

  scene->push(std::move(border));
}

void SliderWidget::render_value_label(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  // 格式化数值
  std::ostringstream oss;
  oss << std::fixed << std::setprecision(1) << value_;
  std::string value_text = oss.str();

  auto text_shape = tvg::Text::gen();
  text_shape->font(style->font_family.c_str());
  text_shape->size(style->font_size * 0.8f);  // 小一点
  text_shape->text(value_text.c_str());
  text_shape->fill(style->text_color.r, style->text_color.g, style->text_color.b);
  text_shape->opacity(style->text_color.a);

  // 位置：滑块上方
  float thumb_size = style->get_variable_float("--thumb-size", 20.0f);
  float thumb_x = elem.width() * value_position_;
  float text_x = thumb_x - value_text.size() * style->font_size * 0.3f;
  float text_y = (elem.height() - thumb_size) / 2 - 5;

  text_shape->translate(text_x, text_y);

  scene->push(std::move(text_shape));
}

// ============================================================================
// 事件处理
// ============================================================================

bool SliderWidget::handle_mouse_down(const Event& event, Element& elem) {
  is_dragging_ = true;
  target_thumb_scale_ = 1.2f;  // 放大 thumb

  // 立即更新值
  update_value_from_x(event.x, elem);

  elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
  return true;  // 消费事件
}

bool SliderWidget::handle_mouse_move(const Event& event, Element& elem) {
  if (is_dragging_) {
    update_value_from_x(event.x, elem);
    elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
    return true;
  }
  return false;
}

bool SliderWidget::handle_mouse_up(const Event& event, Element& elem) {
  if (is_dragging_) {
    is_dragging_ = false;
    target_thumb_scale_ = 1.0f;  // 恢复 thumb
    elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
    return true;
  }
  return false;
}

bool SliderWidget::handle_key_down(const Event& event, Element& elem) {
  float delta = (max_ - min_) * 0.01f;  // 1% per keypress
  if (step_ > 0) {
    delta = step_;
  }

  bool changed = false;

  if (event.key == KeyCode::Left) {
    set_value(value_ - delta);
    changed = true;
  } else if (event.key == KeyCode::Right) {
    set_value(value_ + delta);
    changed = true;
  }

  if (changed) {
    elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
  }

  return changed;
}

// ============================================================================
// 值计算
// ============================================================================

void SliderWidget::update_value_from_x(float x, const Element& elem) {
  // 转换到相对位置
  float relative_x = x - elem.x();
  float position = relative_x / elem.width();
  position = std::max(0.0f, std::min(1.0f, position));

  // 计算新值
  float new_value = min_ + position * (max_ - min_);
  set_value(new_value);
}

void SliderWidget::update_value_position() {
  if (max_ > min_) {
    value_position_ = (value_ - min_) / (max_ - min_);
  } else {
    value_position_ = 0.0f;
  }
}

float SliderWidget::snap_to_step(float value) {
  if (step_ <= 0) return value;

  float steps = std::round((value - min_) / step_);
  return min_ + steps * step_;
}

// ============================================================================
// 动画更新
// ============================================================================

void SliderWidget::update_transitions(float delta_ms, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float duration = style->get_variable_float("--transition-duration", 100.0f);
  if (duration <= 0) {
    current_thumb_scale_ = target_thumb_scale_;
    return;
  }

  // 平滑过渡
  float speed = 1000.0f / duration;
  float delta = speed * (delta_ms / 1000.0f);

  if (std::abs(current_thumb_scale_ - target_thumb_scale_) > 0.01f) {
    if (current_thumb_scale_ < target_thumb_scale_) {
      current_thumb_scale_ = std::min(current_thumb_scale_ + delta, target_thumb_scale_);
    } else {
      current_thumb_scale_ = std::max(current_thumb_scale_ - delta, target_thumb_scale_);
    }
    elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
  }
}

} // namespace tvgbox2
