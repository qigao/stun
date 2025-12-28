/*
 * flexUI - SelectWidget Implementation
 */

#include <flexUI/widgets/select_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <stb_sprintf.h>
#include <algorithm>
#include <cmath>

namespace flexUI {

// ============================================================================
// 构造函数
// ============================================================================

SelectWidget::SelectWidget(const std::vector<std::string>& options, int selected_index)
    : options_(options), selected_index_(selected_index) {
  if (selected_index_ < -1 || selected_index_ >= static_cast<int>(options_.size())) {
    selected_index_ = -1;
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
}

void SelectWidget::set_selected_index(int index) {
  if (index < -1 || index >= static_cast<int>(options_.size())) {
    return;
  }

  if (selected_index_ != index) {
    selected_index_ = index;
    dirty_ = true;

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
  }
}

// ============================================================================
// Widget 接口实现
// ============================================================================

void SelectWidget::render(const Element& elem, Renderer& renderer) {
  auto& r = renderer.flex();

  // 1. 渲染选择框
  render_select_box(r, elem);

  // 2. 渲染当前选中文本
  render_selected_text(r, elem);

  // 3. 渲染下拉箭头
  render_arrow(r, elem);

  // 4. 渲染下拉列表（如果展开）
  if (dropdown_height_ > 0.1f) {
    render_dropdown(r, elem);
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
}

// ============================================================================
// 渲染辅助
// ============================================================================

void SelectWidget::render_select_box(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color bg_color = style->get_variable_color("--select-bg", {1.0f, 1.0f, 1.0f, 1.0f});
  Color border_color = style->get_variable_color("--select-border", {0.78f, 0.78f, 0.78f, 1.0f});

  float width = elem.width();
  float height = style->get_variable_float("--item-height", 32.0f);

  // 背景
  r.draw_rect(0, 0, width, height, 4, Paint::solid(bg_color), Paint::none(), 0);

  // 边框
  float stroke_width = (elem.has_state("focus") || expanded_) ? 2.0f : 1.0f;
  r.draw_rect(0, 0, width, height, 4, Paint::none(), Paint::solid(border_color), stroke_width);
}

void SelectWidget::render_selected_text(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  if (selected_index_ < 0 || selected_index_ >= static_cast<int>(options_.size())) {
    return;
  }

  Color text_color = style->get_variable_color("--select-text", {0.0f, 0.0f, 0.0f, 1.0f});
  float item_height = style->get_variable_float("--item-height", 32.0f);

  float text_x = 12;
  float text_y = (item_height - style->font_size) / 2;

  r.draw_text(options_[selected_index_], text_x, text_y, style->font_family, style->font_size, false, text_color);
}

void SelectWidget::render_arrow(flex::Renderer& r, const Element& elem) {
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

  r.fill_path(path, Paint::solid(arrow_color));
}

void SelectWidget::render_dropdown(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color dropdown_bg = style->get_variable_color("--dropdown-bg", {1.0f, 1.0f, 1.0f, 1.0f});
  Color border_color = style->get_variable_color("--select-border", {0.78f, 0.78f, 0.78f, 1.0f});
  Color text_color = style->get_variable_color("--select-text", {0.0f, 0.0f, 0.0f, 1.0f});
  Color hover_bg = style->get_variable_color("--dropdown-item-hover", {0.94f, 0.94f, 0.94f, 1.0f});
  Color selected_bg = style->get_variable_color("--dropdown-item-selected", {0.90f, 0.94f, 1.0f, 1.0f});

  float item_height = style->get_variable_float("--item-height", 32.0f);
  float width = elem.width();
  float y_offset = item_height;

  // 下拉列表背景
  r.draw_rect(0, y_offset, width, dropdown_height_, 4, Paint::solid(dropdown_bg), Paint::none(), 0);

  // 边框
  r.draw_rect(0, y_offset, width, dropdown_height_, 4, Paint::none(), Paint::solid(border_color), 1);

  // 渲染每个选项
  int visible_items = static_cast<int>(dropdown_height_ / item_height);

  for (int i = 0; i < static_cast<int>(options_.size()) && i < visible_items; i++) {
    float item_y = y_offset + i * item_height;

    // 选项背景（悬停或选中）
    if (i == hovered_index_ || i == selected_index_) {
      Color bg = (i == hovered_index_) ? hover_bg : selected_bg;
      r.draw_rect(0, item_y, width, item_height, 0, Paint::solid(bg), Paint::none(), 0);
    }

    // 选项文字
    float text_x = 12;
    float text_y = item_y + (item_height - style->font_size) / 2;
    r.draw_text(options_[i], text_x, text_y, style->font_family, style->font_size, false, text_color);
  }
}

// ============================================================================
// 事件处理
// ============================================================================

bool SelectWidget::handle_mouse_down(const Event& event, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return false;

  float item_height = style->get_variable_float("--item-height", 32.0f);

  // Convert to local coordinates
  float local_x = event.x - elem.absolute_x();
  float local_y = event.y - elem.absolute_y();

  // 点击选择框本身
  if (local_y < item_height) {
    toggle_dropdown(elem);
    return true;
  }

  // 点击下拉列表中的选项
  if (expanded_ && local_y >= item_height) {
    int clicked_index = static_cast<int>((local_y - item_height) / item_height);

    if (clicked_index >= 0 && clicked_index < static_cast<int>(options_.size())) {
      select_option(clicked_index, elem);
      set_expanded(false);
      elem.remove_state("expanded");
      return true;
    }
  }

  return false;
}

bool SelectWidget::handle_mouse_move(const Event& event, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return false;

  float item_height = style->get_variable_float("--item-height", 32.0f);

  // Convert to local coordinates
  float local_y = event.y - elem.absolute_y();

  if (local_y >= item_height) {
    int new_hovered = static_cast<int>((local_y - item_height) / item_height);

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
        elem.remove_state("expanded");
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
  set_expanded(!expanded_);

  if (expanded_) {
    elem.add_state("expanded");
    hovered_index_ = selected_index_;
  } else {
    elem.remove_state("expanded");
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
