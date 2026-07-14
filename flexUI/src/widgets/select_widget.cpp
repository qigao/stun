/*
 * flexUI - SelectWidget Implementation
 */

#include <flexUI/widgets/select_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/text_layout.h>
#include <stb_sprintf.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

namespace {

struct OverlayRect {
  float x = 0.0f;
  float y = 0.0f;
  float w = 0.0f;
  float h = 0.0f;
};

bool hit_overlay(float px, float py, const OverlayRect& r) {
  return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
}

OverlayRect select_overlay_rect(const Element& elem, float dropdown_height) {
  const auto anchor_bounds = detail::css_render_world_bounds(&elem);
  return {anchor_bounds.x, anchor_bounds.y + anchor_bounds.height,
          anchor_bounds.width, dropdown_height};
}

}  // namespace

// ============================================================================
// 构造函数
// ============================================================================

SelectWidget::SelectWidget(const std::vector<std::string>& options, int selected_index)
    : options_(options), selected_index_(selected_index) {
  if (selected_index_ < -1 || selected_index_ >= static_cast<int>(options_.size())) {
    selected_index_ = -1;
  }
}

bool SelectWidget::measure_intrinsic_size(const Element& elem, float available_width,
                                          float available_height, float& out_width,
                                          float& out_height) const {
  (void)available_width;
  (void)available_height;
  const auto* style = elem.computed_style;
  ComputedStyle measure_style;
  if (style) {
    measure_style = *style;
  }
  const float font_size =
      style && style->font_size > 0.0f ? style->font_size : 14.0f;
  measure_style.font_size = font_size;

  float widest = 0.0f;
  for (const auto& option : options_) {
    widest = std::max(widest,
                      approximate_segmented_text_width(&measure_style, option));
  }
  if (widest <= 0.0f) {
    widest = approximate_segmented_text_width(&measure_style, "Select");
  }

  const float item_height =
      style ? style->get_variable_float("--item-height", 32.0f) : 32.0f;
  out_width = std::max(120.0f, widest + 50.0f);
  out_height = item_height;
  return true;
}

void SelectWidget::sync_host_semantics() {
  set_host_attribute("role", "combobox");
  set_host_data_state("open", "closed", expanded_);
  set_host_boolean_attribute("aria-expanded", expanded_);
  set_host_boolean_attribute("aria-disabled", disabled_);
  set_host_presence_attribute("disabled", disabled_);
  if (selected_index_ >= 0 && selected_index_ < static_cast<int>(options_.size())) {
    set_host_attribute("data-value", options_[selected_index_]);
  } else {
    clear_host_attribute("data-value");
  }
}

// ============================================================================
// 状态访问
// ============================================================================

void SelectWidget::set_options(const std::vector<std::string>& options) {
  options_ = options;

  // 验证 selected_index_
  if (selected_index_ >= static_cast<int>(options_.size())) {
    selected_index_ = -1;
  }

  dirty_ = true;
  if (auto* host = host_element()) {
    host->mark_paint_dirty();
  }
  sync_host_semantics();
}

void SelectWidget::set_selected_index(int index) {
  if (index < -1 || index >= static_cast<int>(options_.size())) {
    return;
  }

  if (selected_index_ != index) {
    selected_index_ = index;
    dirty_ = true;
    if (auto* host = host_element()) {
      host->mark_paint_dirty();
    }
    sync_host_semantics();

    if (change_callback_) {
      change_callback_(index, selected_value());
    }
  }
}

void SelectWidget::set_expanded(bool expanded) {
  if (expanded_ != expanded) {
    expanded_ = expanded;

    if (expanded_) {
      target_dropdown_height_ = static_cast<float>(options_.size()) * 32.0f;  // 默认项高度
    } else {
      target_dropdown_height_ = 0.0f;
      hovered_index_ = -1;
    }

    dirty_ = true;
    if (auto* host = host_element()) {
      host->mark_paint_dirty();
    }
    sync_host_semantics();
  }
}

void SelectWidget::set_disabled(bool disabled) {
  if (disabled_ == disabled) {
    return;
  }
  disabled_ = disabled;
  dirty_ = true;
  if (auto* host = host_element()) {
    host->mark_paint_dirty();
  }
  sync_host_semantics();
}

// ============================================================================
// Widget 接口实现
// ============================================================================

void SelectWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  render_select_box(commands, elem);
  render_selected_text(commands, elem);
  render_arrow(commands, elem);
}

void SelectWidget::emit_overlay_commands(const Element& elem, RenderCommandList& commands) {
  if (dropdown_height_ > 0.1f) {
    render_dropdown(commands, elem);
  }
}

bool SelectWidget::handle_event(const Event& event, Element& elem) {
  if (disabled_ || elem.has_state("disabled")) {
    return false;
  }

  switch (event.type) {
    case EventType::MouseDown:
      return handle_mouse_down(event, elem);

    case EventType::MouseMove:
      if (expanded_) {
        return handle_mouse_move(event, elem);
      }
      return false;

    case EventType::KeyDown:
      return handle_key_down(event, elem);

    default:
      return false;
  }
}

void SelectWidget::update(float delta_ms, Element& elem) {
  update_dropdown_animation(delta_ms);
  if (dirty_) {
    elem.mark_paint_dirty();
  }
}

// ============================================================================
// 渲染辅助
// ============================================================================

void SelectWidget::render_select_box(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color bg_color = style->get_variable_color("--select-bg", {1.0f, 1.0f, 1.0f, 1.0f});
  Color border_color = style->get_variable_color("--select-border", {0.78f, 0.78f, 0.78f, 1.0f});

  float width = elem.width();
  float height = style->get_variable_float("--item-height", 32.0f);

  // 背景
  commands.draw_rect(0, 0, width, height, 4, Paint::solid(bg_color), Paint::none(), 0);

  // 边框
  float stroke_width = (elem.has_state("focus") || expanded_) ? 2.0f : 1.0f;
  commands.draw_rect(0, 0, width, height, 4, Paint::none(),
                     Paint::solid(border_color), stroke_width);
}

void SelectWidget::render_selected_text(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  if (selected_index_ < 0 || selected_index_ >= static_cast<int>(options_.size())) {
    return;
  }

  Color text_color = style->get_variable_color("--select-text", {0.0f, 0.0f, 0.0f, 1.0f});
  float item_height = style->get_variable_float("--item-height", 32.0f);
  const float padding = 12.0f;
  const float arrow_reserve = 26.0f;
  const float text_width = std::max(0.0f, elem.width() - padding - arrow_reserve - padding);
  const auto text_block = layout_text_block(
      style, options_[selected_index_], padding, 0.0f, text_width, item_height,
      text_color, TextVerticalAlign::Middle);
  emit_text_block(commands, text_block);
}

void SelectWidget::render_arrow(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color arrow_color = style->get_variable_color("--select-arrow", {0.39f, 0.39f, 0.39f, 1.0f});
  float item_height = style->get_variable_float("--item-height", 32.0f);
  float width = elem.width();

  // 下拉箭头：三角形
  float arrow_size = 6.0f;
  float arrow_x = width - 20;
  float arrow_y = item_height / 2;

  char path[128];
  if (expanded_) {
    // 向上箭头
    stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g Z",
             arrow_x - arrow_size, arrow_y + arrow_size / 2,
             arrow_x, arrow_y - arrow_size / 2,
             arrow_x + arrow_size, arrow_y + arrow_size / 2);
  } else {
    // 向下箭头
    stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g Z",
             arrow_x - arrow_size, arrow_y - arrow_size / 2,
             arrow_x, arrow_y + arrow_size / 2,
             arrow_x + arrow_size, arrow_y - arrow_size / 2);
  }

  commands.fill_path(path, Paint::solid(arrow_color));
}

void SelectWidget::render_dropdown(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color dropdown_bg = style->get_variable_color("--dropdown-bg", {1.0f, 1.0f, 1.0f, 1.0f});
  Color border_color = style->get_variable_color("--select-border", {0.78f, 0.78f, 0.78f, 1.0f});
  Color text_color = style->get_variable_color("--select-text", {0.0f, 0.0f, 0.0f, 1.0f});
  Color hover_bg = style->get_variable_color("--dropdown-item-hover", {0.94f, 0.94f, 0.94f, 1.0f});
  Color selected_bg = style->get_variable_color("--dropdown-item-selected", {0.90f, 0.94f, 1.0f, 1.0f});

  float item_height = style->get_variable_float("--item-height", 32.0f);
  const float interactive_height =
      dropdown_height_ > 0.1f ? dropdown_height_ : target_dropdown_height_;
  const OverlayRect dropdown_bounds = select_overlay_rect(elem, interactive_height);
  float width = dropdown_bounds.w;
  float dropdown_x = dropdown_bounds.x;
  float dropdown_y = dropdown_bounds.y;

  // 下拉列表背景
  commands.draw_rect(dropdown_x, dropdown_y, width, dropdown_height_, 4,
                     Paint::solid(dropdown_bg), Paint::none(), 0);

  // 边框
  commands.draw_rect(dropdown_x, dropdown_y, width, dropdown_height_, 4, Paint::none(),
                     Paint::solid(border_color), 1);

  // 渲染每个选项
  int visible_items = static_cast<int>(dropdown_height_ / item_height);

  for (int i = 0; i < static_cast<int>(options_.size()) && i < visible_items; i++) {
    float item_y = dropdown_y + i * item_height;

    // 选项背景（悬停或选中）
    if (i == hovered_index_ || i == selected_index_) {
      Color bg = (i == hovered_index_) ? hover_bg : selected_bg;
      commands.draw_rect(dropdown_x, item_y, width, item_height, 0,
                         Paint::solid(bg), Paint::none(), 0);
    }

    // 选项文字
    const float padding = 12.0f;
    const float text_width = std::max(0.0f, width - padding * 2.0f - 12.0f);
    const auto text_block = layout_text_block(
        style, options_[i], dropdown_x + padding, item_y, text_width,
        item_height, text_color, TextVerticalAlign::Middle);
    emit_text_block(commands, text_block);
  }
}

// ============================================================================
// 事件处理
// ============================================================================

bool SelectWidget::handle_mouse_down(const Event& event, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return false;

  float item_height = style->get_variable_float("--item-height", 32.0f);

  const flex::Vec2 local_pos =
      detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
  float local_x = local_pos.x;
  float local_y = local_pos.y;

  // 点击选择框本身
  if (local_x >= 0.0f && local_x <= elem.width() &&
      local_y >= 0.0f && local_y < item_height) {
    toggle_dropdown(elem);
    return true;
  }

  // 点击下拉列表中的选项
  const float interactive_height =
      dropdown_height_ > 0.1f ? dropdown_height_ : target_dropdown_height_;
  const OverlayRect dropdown_bounds = select_overlay_rect(elem, interactive_height);
  if (expanded_ && hit_overlay(event.x, event.y, dropdown_bounds)) {
    int clicked_index =
        static_cast<int>((event.y - dropdown_bounds.y) / item_height);

    if (clicked_index >= 0 && clicked_index < static_cast<int>(options_.size())) {
      select_option(clicked_index, elem);
      set_expanded(false);
      return true;
    }
  }

  return false;
}

bool SelectWidget::handle_mouse_move(const Event& event, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return false;

  float item_height = style->get_variable_float("--item-height", 32.0f);
  const float interactive_height =
      dropdown_height_ > 0.1f ? dropdown_height_ : target_dropdown_height_;
  const OverlayRect dropdown_bounds = select_overlay_rect(elem, interactive_height);

  if (hit_overlay(event.x, event.y, dropdown_bounds)) {
    int new_hovered =
        static_cast<int>((event.y - dropdown_bounds.y) / item_height);

    if (new_hovered >= 0 && new_hovered < static_cast<int>(options_.size())) {
      if (hovered_index_ != new_hovered) {
        hovered_index_ = new_hovered;
        dirty_ = true;
      }
      return true;
    }
  }

  if (hovered_index_ != -1) {
    hovered_index_ = -1;
    dirty_ = true;
  }

  return false;
}

bool SelectWidget::handle_key_down(const Event& event, Element& elem) {
  if (!elem.has_state("focus")) {
    return false;
  }

  switch (event.key) {
    case KeyCode::Enter:
    case KeyCode::Num0:  // Space 的替代
      toggle_dropdown(elem);
      return true;

    case KeyCode::Up:
      if (expanded_) {
        if (hovered_index_ > 0) {
          hovered_index_--;
          dirty_ = true;
        }
      } else {
        if (selected_index_ > 0) {
          select_option(selected_index_ - 1, elem);
        }
      }
      return true;

    case KeyCode::Down:
      if (expanded_) {
        if (hovered_index_ < static_cast<int>(options_.size()) - 1) {
          hovered_index_++;
          dirty_ = true;
        }
      } else {
        if (selected_index_ < static_cast<int>(options_.size()) - 1) {
          select_option(selected_index_ + 1, elem);
        }
      }
      return true;

    case KeyCode::Escape:
      if (expanded_) {
        set_expanded(false);
        return true;
      }
      break;

    default:
      break;
  }

  return false;
}

void SelectWidget::select_option(int index, Element& elem) {
  set_selected_index(index);
}

void SelectWidget::toggle_dropdown(Element& elem) {
  (void)elem;
  set_expanded(!expanded_);

  if (expanded_) {
    hovered_index_ = selected_index_;
  }
}

// ============================================================================
// 动画更新
// ============================================================================

void SelectWidget::update_dropdown_animation(float delta_ms) {
  if (std::abs(dropdown_height_ - target_dropdown_height_) > 0.1f) {
    float speed = 1000.0f;  // 像素/秒
    float delta = speed * (delta_ms / 1000.0f);

    if (dropdown_height_ < target_dropdown_height_) {
      dropdown_height_ = std::min(dropdown_height_ + delta, target_dropdown_height_);
    } else {
      dropdown_height_ = std::max(dropdown_height_ - delta, target_dropdown_height_);
    }

    dirty_ = true;
  }
}

} // namespace flexUI
