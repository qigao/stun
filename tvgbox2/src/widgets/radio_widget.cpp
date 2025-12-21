/*
 * tvgbox2 - RadioWidget Implementation
 */

#include <tvgbox2/widgets/radio_widget.h>
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

RadioWidget::RadioWidget(const std::string& label, const std::string& value,
                         const std::string& group, bool checked)
    : label_(label), value_(value), group_(group), checked_(checked) {
  target_dot_scale_ = checked ? 1.0f : 0.0f;
  dot_scale_ = target_dot_scale_;
}

// ============================================================================
// 状态访问
// ============================================================================

void RadioWidget::set_checked(bool checked) {
  if (checked_ != checked) {
    checked_ = checked;
    target_dot_scale_ = checked ? 1.0f : 0.0f;
    dirty_ = true;

    if (checked && change_callback_) {
      change_callback_(value_, group_);
    }
  }
}

// ============================================================================
// Widget 接口实现
// ============================================================================

void RadioWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  // 1. 渲染单选按钮圆圈
  render_radio_circle(scene, elem);

  // 2. 渲染内圆点（如果选中）
  if (dot_scale_ > 0.01f) {
    render_dot(scene, elem);
  }

  // 3. 渲染文字标签
  if (!label_.empty()) {
    render_label(scene, elem);
  }
}

bool RadioWidget::handle_event(const Event& event, Element& elem) {
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

void RadioWidget::update(float delta_ms, Element& elem) {
  update_transitions(delta_ms, elem);
}

// ============================================================================
// 渲染辅助
// ============================================================================

void RadioWidget::render_radio_circle(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  // 读取单选按钮大小
  float radio_size = style->get_variable_float("--radio-size", 20.0f);

  // 读取背景色
  Color bg_color;
  if (checked_ || elem.has_state("checked")) {
    bg_color = style->get_variable_color("--radio-bg-checked", {59, 130, 246, 255});
  } else {
    bg_color = style->get_variable_color("--radio-bg", {255, 255, 255, 255});
  }

  // 读取边框色
  Color border_color = style->get_variable_color("--radio-border", {200, 200, 200, 255});

  float radius = radio_size / 2;
  float center_x = radius;
  float center_y = radius;

  // 绘制背景圆
  auto circle = tvg::Shape::gen();
  circle->appendCircle(center_x, center_y, radius, radius);
  circle->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);

  scene->push(std::move(circle));

  // 绘制边框
  auto border = tvg::Shape::gen();
  border->appendCircle(center_x, center_y, radius, radius);
  border->strokeFill(border_color.r, border_color.g, border_color.b, border_color.a);
  border->strokeWidth(2);

  scene->push(std::move(border));
}

void RadioWidget::render_dot(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float radio_size = style->get_variable_float("--radio-size", 20.0f);
  Color dot_color = style->get_variable_color("--radio-dot", {255, 255, 255, 255});

  float outer_radius = radio_size / 2;
  float center_x = outer_radius;
  float center_y = outer_radius;

  // 内圆点半径：外圆的 50%，应用动画缩放
  float dot_radius = outer_radius * 0.5f * dot_scale_;

  if (dot_radius > 0.1f) {
    auto dot = tvg::Shape::gen();
    dot->appendCircle(center_x, center_y, dot_radius, dot_radius);
    dot->fill(dot_color.r, dot_color.g, dot_color.b, dot_color.a);

    scene->push(std::move(dot));
  }
}

void RadioWidget::render_label(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float radio_size = style->get_variable_float("--radio-size", 20.0f);
  float label_spacing = style->get_variable_float("--label-spacing", 8.0f);

  auto text_shape = tvg::Text::gen();
  text_shape->font(style->font_family.c_str());
  text_shape->size(style->font_size);
  text_shape->text(label_.c_str());
  text_shape->fill(style->text_color.r, style->text_color.g, style->text_color.b);
  text_shape->opacity(style->text_color.a);

  // 文字位置：单选按钮右侧
  float text_x = radio_size + label_spacing;
  float text_y = radio_size / 2 + style->font_size / 3;  // 垂直居中对齐
  text_shape->translate(text_x, text_y);

  scene->push(std::move(text_shape));
}

// ============================================================================
// 事件处理
// ============================================================================

bool RadioWidget::handle_mouse_down(const Event& event, Element& elem) {
  // Radio 通常不能取消选中，只能选中
  if (!checked_) {
    set_checked(true);

    // 更新伪状态
    elem.add_state("checked");
  }

  return true;  // 消费事件
}

bool RadioWidget::handle_key_down(const Event& event, Element& elem) {
  // 空格键或回车键选中
  if (event.key == KeyCode::Enter || event.key == KeyCode::Num0) {  // 简化：用数字0代替Space
    if (!checked_) {
      set_checked(true);
      elem.add_state("checked");
    }
    return true;
  }

  return false;
}

// ============================================================================
// 动画更新
// ============================================================================

void RadioWidget::update_transitions(float delta_ms, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  // 读取过渡时长
  float duration = style->get_variable_float("--transition-duration", 200.0f);
  if (duration <= 0) {
    dot_scale_ = target_dot_scale_;
    return;
  }

  // 平滑过渡
  float speed = 1000.0f / duration;
  float delta = speed * (delta_ms / 1000.0f);

  if (std::abs(dot_scale_ - target_dot_scale_) > 0.01f) {
    if (dot_scale_ < target_dot_scale_) {
      dot_scale_ = std::min(dot_scale_ + delta, target_dot_scale_);
    } else {
      dot_scale_ = std::max(dot_scale_ - delta, target_dot_scale_);
    }
    dirty_ = true;
  }
}

} // namespace tvgbox2
