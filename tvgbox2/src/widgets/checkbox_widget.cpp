/*
 * tvgbox2 - CheckboxWidget Implementation
 */

#include <tvgbox2/widgets/checkbox_widget.h>
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

CheckboxWidget::CheckboxWidget(const std::string& label, bool checked)
    : label_(label), checked_(checked) {
  target_checkmark_scale_ = checked ? 1.0f : 0.0f;
  checkmark_scale_ = target_checkmark_scale_;
}

// ============================================================================
// 状态访问
// ============================================================================

void CheckboxWidget::set_checked(bool checked) {
  if (checked_ != checked) {
    checked_ = checked;
    target_checkmark_scale_ = checked ? 1.0f : 0.0f;
    dirty_ = true;

    if (change_callback_) {
      change_callback_(checked_);
    }
  }
}

// ============================================================================
// Widget 接口实现
// ============================================================================

void CheckboxWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  // 1. 渲染复选框主体
  render_checkbox_box(scene, elem);

  // 2. 渲染勾选标记（如果选中）
  if (checkmark_scale_ > 0.01f) {
    render_checkmark(scene, elem);
  }

  // 3. 渲染文字标签
  if (!label_.empty()) {
    render_label(scene, elem);
  }
}

bool CheckboxWidget::handle_event(const Event& event, Element& elem) {
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

void CheckboxWidget::update(float delta_ms, Element& elem) {
  update_transitions(delta_ms, elem);
}

// ============================================================================
// 渲染辅助
// ============================================================================

void CheckboxWidget::render_checkbox_box(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  // 读取复选框大小
  float checkbox_size = style->get_variable_float("--checkbox-size", 20.0f);

  // 读取背景色
  Color bg_color;
  if (checked_ || elem.has_state("checked")) {
    bg_color = style->get_variable_color("--checkbox-bg-checked", {59, 130, 246, 255});
  } else {
    bg_color = style->get_variable_color("--checkbox-bg", {255, 255, 255, 255});
  }

  // 读取边框色
  Color border_color = style->get_variable_color("--checkbox-border", {200, 200, 200, 255});

  // 绘制背景
  auto box = tvg::Shape::gen();
  float radius = checkbox_size * 0.15f;  // 圆角：15% of size
  box->appendRect(0, 0, checkbox_size, checkbox_size, radius, radius);
  box->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);

  scene->push(std::move(box));

  // 绘制边框
  auto border = tvg::Shape::gen();
  border->appendRect(0, 0, checkbox_size, checkbox_size, radius, radius);
  border->strokeFill(border_color.r, border_color.g, border_color.b, border_color.a);
  border->strokeWidth(2);

  scene->push(std::move(border));
}

void CheckboxWidget::render_checkmark(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float checkbox_size = style->get_variable_float("--checkbox-size", 20.0f);
  Color checkmark_color = style->get_variable_color("--checkbox-checkmark", {255, 255, 255, 255});

  // 绘制勾选标记（简化版：用 L 形线条）
  auto checkmark = tvg::Shape::gen();

  float center_x = checkbox_size / 2;
  float center_y = checkbox_size / 2;
  float size = checkbox_size * 0.4f * checkmark_scale_;  // 应用动画缩放

  // L 形路径（勾选标记）
  checkmark->moveTo(center_x - size * 0.5f, center_y);
  checkmark->lineTo(center_x - size * 0.2f, center_y + size * 0.4f);
  checkmark->lineTo(center_x + size * 0.5f, center_y - size * 0.4f);

  checkmark->strokeFill(checkmark_color.r, checkmark_color.g, checkmark_color.b, checkmark_color.a);
  checkmark->strokeWidth(2);

  scene->push(std::move(checkmark));
}

void CheckboxWidget::render_label(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  float checkbox_size = style->get_variable_float("--checkbox-size", 20.0f);
  float label_spacing = style->get_variable_float("--label-spacing", 8.0f);

  auto text_shape = tvg::Text::gen();
  text_shape->font(style->font_family.c_str());
  text_shape->size(style->font_size);
  text_shape->text(label_.c_str());
  text_shape->fill(style->text_color.r, style->text_color.g, style->text_color.b);
  text_shape->opacity(style->text_color.a);

  // 文字位置：复选框右侧
  float text_x = checkbox_size + label_spacing;
  float text_y = checkbox_size / 2 + style->font_size / 3;  // 垂直居中对齐
  text_shape->translate(text_x, text_y);

  scene->push(std::move(text_shape));
}

// ============================================================================
// 事件处理
// ============================================================================

bool CheckboxWidget::handle_mouse_down(const Event& event, Element& elem) {
  // 切换选中状态
  set_checked(!checked_);

  // 更新伪状态
  if (checked_) {
    elem.add_state("checked");
  } else {
    elem.remove_state("checked");
  }

  elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
  return true;  // 消费事件
}

bool CheckboxWidget::handle_key_down(const Event& event, Element& elem) {
  // 空格键切换
  if (event.key == KeyCode::Enter || event.key == KeyCode::Num0) {  // 简化：用数字0代替Space
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

void CheckboxWidget::update_transitions(float delta_ms, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  // 读取过渡时长
  float duration = style->get_variable_float("--transition-duration", 200.0f);
  if (duration <= 0) {
    checkmark_scale_ = target_checkmark_scale_;
    return;
  }

  // 平滑过渡
  float speed = 1000.0f / duration;
  float delta = speed * (delta_ms / 1000.0f);

  if (std::abs(checkmark_scale_ - target_checkmark_scale_) > 0.01f) {
    if (checkmark_scale_ < target_checkmark_scale_) {
      checkmark_scale_ = std::min(checkmark_scale_ + delta, target_checkmark_scale_);
    } else {
      checkmark_scale_ = std::max(checkmark_scale_ - delta, target_checkmark_scale_);
    }
    elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
  }
}

} // namespace tvgbox2
