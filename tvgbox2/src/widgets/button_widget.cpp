/*
 * tvgbox2 - ButtonWidget Implementation
 */

#include <tvgbox2/widgets/button_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <tvgbox2/text_util.h>
#include <thorvg.h>
#include <algorithm>
#include <cmath>
#include <iostream>
namespace tvgbox2 {

// ============================================================================
// 构造函数
// ============================================================================

ButtonWidget::ButtonWidget(const std::string& text)
    : text_(text) {}

// ============================================================================
// Widget 接口实现
// ============================================================================

void ButtonWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  // 如果禁用或加载中，应用视觉效果
  bool is_disabled = disabled_ || elem.has_state("disabled");
  bool is_loading = loading_ || elem.has_state("loading");

  // 1. 渲染背景
  render_background(scene, elem);

  // 2. 渲染涟漪效果（如果启用）
  auto* style = elem.computed_style;
  if (style && style->get_variable("--ripple", "false") == "true") {
    render_ripples(scene, elem);
  }

  // 3. 渲染内容
  if (is_loading) {
    // 加载状态：显示 spinner
    render_loading_spinner(scene, elem);
  } else {
    // 正常状态：显示文本
    render_text(scene, elem);
  }
}

bool ButtonWidget::handle_event(const Event& event, Element& elem) {
  // 禁用或加载中不响应事件
  if (disabled_ || loading_ || elem.has_state("disabled") || elem.has_state("loading")) {
    return false;
  }

  switch (event.type) {
    case EventType::MouseDown:
      return handle_mouse_down(event, elem);

    case EventType::MouseUp:
      return handle_mouse_up(event, elem);

    default:
      return false;
  }
}

void ButtonWidget::update(float delta_ms, Element& elem) {
  // 1. 更新过渡动画
  update_transitions(delta_ms, elem);

  // 2. 更新涟漪效果
  update_ripples(delta_ms, elem);

  // 3. 更新加载动画
  if (loading_ || elem.has_state("loading")) {
    update_spinner(delta_ms, elem);
  }
}

// ============================================================================
// 渲染辅助
// ============================================================================

void ButtonWidget::render_background(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  // 应用缩放变换（用于 hover/active 动画）
  auto bg = tvg::Shape::gen();

  // 考虑缩放：从中心缩放
  float center_x = elem.width() / 2;
  float center_y = elem.height() / 2;

  if (current_scale_ != 1.0f) {
    bg->translate(-center_x, -center_y);
    bg->scale(current_scale_);
    bg->translate(center_x, center_y);
  }

  // 绘制圆角矩形
  float radius = style->border_radius[0];  // 简化：只用第一个值
  if (radius > 0) {
    bg->appendRect(0, 0, elem.width(), elem.height(), radius, radius);
  } else {
    bg->appendRect(0, 0, elem.width(), elem.height());
  }

  // 尝试从 CSS 变量读取背景色（--bg），fallback 到 background_color
  Color bg_color = style->get_variable_color("--bg", style->background_color);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);

  scene->push(std::move(bg));
}

void ButtonWidget::render_text(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  std::string display_text = !elem.text_content.empty() ? elem.text_content : text_;
  if (display_text.empty()) return;

  Color text_color = style->get_variable_color("--text", style->text_color);

  // Split text into segments
  auto segments = segment_text(display_text);

  // 1. Measure total width and common height
  float total_width = 0;
  float max_height = 0;
  float max_ascender = 0; // approximate via negative y in bounds
  float max_descender = 0;

  struct MeasuredSegment {
    tvg::Text* shape;
    float width;
    float x_offset; // relative to origin
    float y_offset;
    float w, h;
  };
  std::vector<MeasuredSegment> measured_segs;

  for (const auto& seg : segments) {
    // defaults to raw pointer, assume scene takes ownership upon push
    auto text_shape = tvg::Text::gen();

    if (seg.type == TextSegmentType::Emoji) {
      text_shape->font(get_emoji_font_name());
    } else {
      text_shape->font(style->font_family.c_str());
    }

    text_shape->size(style->font_size);
    text_shape->text(seg.text.c_str());

    float x, y, w, h;
    text_shape->bounds(&x, &y, &w, &h);

    MeasuredSegment item;
    item.shape = text_shape;
    item.width = w;
    item.x_offset = x;
    item.y_offset = y;
    item.w = w;
    item.h = h;
    measured_segs.push_back(item);

    total_width += w;
    
    if (-y > max_ascender) max_ascender = -y;
    if (y + h > max_descender) max_descender = y + h;
  }
  
  max_height = max_ascender + max_descender;
  if (max_height < style->font_size) max_height = style->font_size; // fallback

  // 2. Calculate start position to center text
  float start_x = (elem.width() - total_width) / 2.0f;
  
  float total_text_h = max_ascender + max_descender;
  // If bounds are zero (e.g. space), fallback to font metrics
  if (total_text_h < 1.0f) total_text_h = style->font_size;
  if (max_ascender < 1.0f) max_ascender = style->font_size * 0.8f;

  float baseline_y = (elem.height() - total_text_h) / 2.0f + max_ascender;

  // 3. Render
  float current_x = start_x;
  for (auto& mseg : measured_segs) {
    mseg.shape->fill(text_color.r, text_color.g, text_color.b);
    mseg.shape->opacity(text_color.a);
    
    mseg.shape->translate(current_x, baseline_y);
    scene->push(mseg.shape);
    
    current_x += mseg.width;
  }
}

void ButtonWidget::render_ripples(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  // 获取涟漪颜色
  Color ripple_color = style->get_variable_color("--ripple-color", {255, 255, 255, 128});

  for (const auto& ripple : ripples_) {
    if (ripple.finished) continue;

    auto circle = tvg::Shape::gen();
    circle->appendCircle(ripple.x, ripple.y, ripple.radius, ripple.radius);

    uint8_t alpha = static_cast<uint8_t>(ripple.alpha * ripple_color.a);
    circle->fill(ripple_color.r, ripple_color.g, ripple_color.b, alpha);

    scene->push(std::move(circle));
  }
}

void ButtonWidget::render_loading_spinner(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  // 获取 spinner 颜色
  Color spinner_color = style->get_variable_color("--loading-spinner-color", style->text_color);

  // 绘制旋转的圆弧
  float center_x = elem.width() / 2;
  float center_y = elem.height() / 2;
  float radius = style->font_size * 0.6f;

  auto spinner = tvg::Shape::gen();

  // 绘制圆弧（270度）
  float start_angle = spinner_rotation_;
  float end_angle = spinner_rotation_ + 270;

  // 简化：绘制一个圆环（ThorVG 可能需要不同的 API）
  // 这里用圆形 + 透明度渐变模拟
  spinner->appendCircle(center_x, center_y, radius, radius);
  spinner->strokeFill(spinner_color.r, spinner_color.g, spinner_color.b, spinner_color.a);
  spinner->strokeWidth(2);

  // 应用旋转
  spinner->translate(-center_x, -center_y);
  spinner->rotate(spinner_rotation_);
  spinner->translate(center_x, center_y);

  scene->push(std::move(spinner));
}

// ============================================================================
// 事件处理
// ============================================================================

bool ButtonWidget::handle_mouse_down(const Event& event, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return false;

  // 添加涟漪效果（如果启用）
  if (style->get_variable("--ripple", "false") == "true") {
    // 转换到按钮局部坐标（使用绝对坐标）
    float local_x = event.x - elem.absolute_x();
    float local_y = event.y - elem.absolute_y();
    add_ripple(local_x, local_y, elem);
  }

  // 读取 active-scale
  target_scale_ = style->get_variable_float("--active-scale", 0.95f);
  elem.mark_paint_dirty();  // 通知 Box 需要重新渲染

  return false;  // 不阻止事件传播，让 Box 处理 active 状态
}

bool ButtonWidget::handle_mouse_up(const Event& event, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return false;

  // 恢复到 hover-scale 或 1.0
  if (elem.has_state("hover")) {
    target_scale_ = style->get_variable_float("--hover-scale", 1.0f);
  } else {
    target_scale_ = 1.0f;
  }
  elem.mark_paint_dirty();  // 通知 Box 需要重新渲染

  return false;
}

// ============================================================================
// 动画更新
// ============================================================================

void ButtonWidget::update_transitions(float delta_ms, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  // 读取过渡时长
  float duration = style->get_variable_float("--transition-duration", 0);
  if (duration <= 0) {
    // 无过渡，直接设置
    current_scale_ = target_scale_;
    return;
  }

  // 平滑过渡
  float speed = 1000.0f / duration;  // 每秒变化量
  float delta = speed * (delta_ms / 1000.0f);

  if (std::abs(current_scale_ - target_scale_) > 0.001f) {
    if (current_scale_ < target_scale_) {
      current_scale_ = std::min(current_scale_ + delta, target_scale_);
    } else {
      current_scale_ = std::max(current_scale_ - delta, target_scale_);
    }
    elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
  }

  // 更新 hover 状态的目标缩放
  if (elem.has_state("hover") && !elem.has_state("active")) {
    float hover_scale = style->get_variable_float("--hover-scale", 1.0f);
    if (target_scale_ != hover_scale) {
      target_scale_ = hover_scale;
    }
  } else if (!elem.has_state("hover") && !elem.has_state("active")) {
    if (target_scale_ != 1.0f) {
      target_scale_ = 1.0f;
    }
  }
}

void ButtonWidget::update_ripples(float delta_ms, Element& elem) {
  bool any_active = false;

  for (auto& ripple : ripples_) {
    if (ripple.finished) continue;

    // 涟漪扩散速度：300ms 到达最大半径
    float speed = ripple.max_radius / 300.0f;  // px/ms
    ripple.radius += speed * delta_ms;

    // 透明度衰减
    ripple.alpha = 1.0f - (ripple.radius / ripple.max_radius);

    if (ripple.radius >= ripple.max_radius) {
      ripple.finished = true;
    } else {
      any_active = true;
    }
  }

  // 清理已完成的涟漪
  ripples_.erase(
    std::remove_if(ripples_.begin(), ripples_.end(),
                   [](const Ripple& r) { return r.finished; }),
    ripples_.end()
  );

  if (any_active) {
    elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
  }
}

void ButtonWidget::update_spinner(float delta_ms, Element& elem) {
  // 旋转速度：360度/秒
  spinner_rotation_ += (360.0f / 1000.0f) * delta_ms;
  if (spinner_rotation_ >= 360.0f) {
    spinner_rotation_ -= 360.0f;
  }
  elem.mark_paint_dirty();  // 通知 Box 需要重新渲染
}

// ============================================================================
// 涟漪效果
// ============================================================================

void ButtonWidget::add_ripple(float x, float y, const Element& elem) {
  Ripple ripple;
  ripple.x = x;
  ripple.y = y;
  ripple.radius = 0;
  ripple.alpha = 1.0f;

  // 计算最大半径：到最远角的距离
  float dx1 = x, dy1 = y;
  float dx2 = elem.width() - x, dy2 = elem.height() - y;
  float dist1 = std::sqrt(dx1 * dx1 + dy1 * dy1);
  float dist2 = std::sqrt(dx2 * dx2 + dy2 * dy2);
  float dist3 = std::sqrt(dx1 * dx1 + dy2 * dy2);
  float dist4 = std::sqrt(dx2 * dx2 + dy1 * dy1);

  ripple.max_radius = std::max({dist1, dist2, dist3, dist4});

  ripples_.push_back(ripple);
  dirty_ = true;
}

} // namespace tvgbox2
