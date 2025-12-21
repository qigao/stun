/*
 * tvgbox2 - SwitchWidget Implementation
 */

#include <tvgbox2/widgets/switch_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <thorvg.h>
#include <algorithm>
#include <cmath>

namespace tvgbox2 {

// ============================================================================
// 构造函数
// ============================================================================

SwitchWidget::SwitchWidget(const std::string& label, bool checked)
    : label_(label), checked_(checked) {
  target_thumb_position_ = checked ? 1.0f : 0.0f;
  thumb_position_ = target_thumb_position_;
}

// ============================================================================
// 状态访问
// ============================================================================

void SwitchWidget::set_checked(bool checked) {
  if (checked_ != checked) {
    checked_ = checked;
    target_thumb_position_ = checked ? 1.0f : 0.0f;
    dirty_ = true;

    if (change_callback_) {
      change_callback_(checked_);
    }
  }
}

// ============================================================================
// Widget 接口实现
// ============================================================================

void SwitchWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  // 1. 渲染轨道（背景）
  render_track(scene, elem);

  // 2. 渲染滑块
  render_thumb(scene, elem);

  // 3. 渲染文字标签
  if (!label_.empty()) {
    render_label(scene, elem);
  }
}

bool SwitchWidget::handle_event(const Event& event, Element& elem) {
  if (disabled_ || elem.has_state("disabled")) {
    return false;
  }

  switch (event.type) {
    case EventType::MouseDown:
      return handle_mouse_down(event, elem);

    case EventType::KeyDown:
      return handle_key_down(event, elem);

    default:
      return false;
  }
}

void SwitchWidget::update(float delta_ms, Element& elem) {
  update_transitions(delta_ms, elem);
}

// ============================================================================
// 渲染辅助
// ============================================================================

void SwitchWidget::render_track(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float width = style->get_variable_float("--switch-width", 50.0f);
  float height = style->get_variable_float("--switch-height", 28.0f);

  // 根据状态选择背景色
  Color bg_color;
  if (checked_ || elem.has_state("checked")) {
    bg_color = style->get_variable_color("--switch-bg-on", {34, 197, 94, 255});  // Green
  } else {
    bg_color = style->get_variable_color("--switch-bg-off", {200, 200, 200, 255});  // Gray
  }

  // 绘制圆角矩形轨道
  auto track = tvg::Shape::gen();
  float radius = height / 2;
  track->appendRect(0, 0, width, height, radius, radius);
  track->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);

  scene->push(std::move(track));
}

void SwitchWidget::render_thumb(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float width = style->get_variable_float("--switch-width", 50.0f);
  float height = style->get_variable_float("--switch-height", 28.0f);
  Color thumb_color = style->get_variable_color("--switch-thumb", {255, 255, 255, 255});

  // 滑块尺寸（略小于轨道高度）
  float thumb_size = height - 4;
  float thumb_radius = thumb_size / 2;

  // 滑块位置：从左到右移动
  float margin = 2;
  float travel_distance = width - thumb_size - 2 * margin;
  float thumb_x = margin + travel_distance * thumb_position_;
  float thumb_y = margin;

  // 绘制圆形滑块
  auto thumb = tvg::Shape::gen();
  thumb->appendCircle(thumb_x + thumb_radius, thumb_y + thumb_radius, thumb_radius, thumb_radius);
  thumb->fill(thumb_color.r, thumb_color.g, thumb_color.b, thumb_color.a);

  scene->push(std::move(thumb));
}

void SwitchWidget::render_label(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float switch_width = style->get_variable_float("--switch-width", 50.0f);
  float switch_height = style->get_variable_float("--switch-height", 28.0f);
  float label_spacing = style->get_variable_float("--label-spacing", 8.0f);

  auto text_shape = tvg::Text::gen();
  text_shape->font(style->font_family.c_str());
  text_shape->size(style->font_size);
  text_shape->text(label_.c_str());
  text_shape->fill(style->text_color.r, style->text_color.g, style->text_color.b);
  text_shape->opacity(style->text_color.a);

  // 文字位置：开关右侧
  float text_x = switch_width + label_spacing;
  float text_y = switch_height / 2 + style->font_size / 3;
  text_shape->translate(text_x, text_y);

  scene->push(std::move(text_shape));
}

// ============================================================================
// 事件处理
// ============================================================================

bool SwitchWidget::handle_mouse_down(const Event& event, Element& elem) {
  // 切换状态
  set_checked(!checked_);

  // 更新伪状态
  if (checked_) {
    elem.add_state("checked");
  } else {
    elem.remove_state("checked");
  }

  elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
  return true;
}

bool SwitchWidget::handle_key_down(const Event& event, Element& elem) {
  // 空格键或回车键切换
  if (event.key == KeyCode::Enter || event.key == KeyCode::Num0) {
    set_checked(!checked_);

    if (checked_) {
      elem.add_state("checked");
    } else {
      elem.remove_state("checked");
    }

    elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
    return true;
  }

  return false;
}

// ============================================================================
// 动画更新
// ============================================================================

void SwitchWidget::update_transitions(float delta_ms, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  // 读取过渡时长
  float duration = style->get_variable_float("--transition-duration", 200.0f);
  if (duration <= 0) {
    thumb_position_ = target_thumb_position_;
    return;
  }

  // 平滑过渡
  float speed = 1000.0f / duration;
  float delta = speed * (delta_ms / 1000.0f);

  if (std::abs(thumb_position_ - target_thumb_position_) > 0.01f) {
    if (thumb_position_ < target_thumb_position_) {
      thumb_position_ = std::min(thumb_position_ + delta, target_thumb_position_);
    } else {
      thumb_position_ = std::max(thumb_position_ - delta, target_thumb_position_);
    }
    elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
  }
}

} // namespace tvgbox2
