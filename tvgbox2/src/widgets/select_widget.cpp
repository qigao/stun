/*
 * tvgbox2 - SelectWidget Implementation
 */

#include <tvgbox2/widgets/select_widget.h>
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

void SelectWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  // 1. 渲染选择框
  render_select_box(scene, elem);

  // 2. 渲染当前选中文本
  render_selected_text(scene, elem);

  // 3. 渲染下拉箭头
  render_arrow(scene, elem);

  // 4. 渲染下拉列表（如果展开）
  if (dropdown_height_ > 0.1f) {
    render_dropdown(scene, elem);
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

void SelectWidget::render_select_box(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color bg_color = style->get_variable_color("--select-bg", {255, 255, 255, 255});
  Color border_color = style->get_variable_color("--select-border", {200, 200, 200, 255});

  float width = elem.width();
  float height = style->get_variable_float("--item-height", 32.0f);

  // 背景
  auto rect = tvg::Shape::gen();
  rect->appendRect(0, 0, width, height, 4, 4);
  rect->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);
  scene->push(std::move(rect));

  // 边框
  auto border = tvg::Shape::gen();
  border->appendRect(0, 0, width, height, 4, 4);
  border->strokeFill(border_color.r, border_color.g, border_color.b, border_color.a);
  border->strokeWidth(elem.has_state("focus") || expanded_ ? 2.0f : 1.0f);
  scene->push(std::move(border));
}

void SelectWidget::render_selected_text(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  if (selected_index_ < 0 || selected_index_ >= static_cast<int>(options_.size())) {
    return;
  }

  Color text_color = style->get_variable_color("--select-text", {0, 0, 0, 255});
  float item_height = style->get_variable_float("--item-height", 32.0f);

  auto text_shape = tvg::Text::gen();
  text_shape->font(style->font_family.c_str());
  text_shape->size(style->font_size);
  text_shape->text(options_[selected_index_].c_str());
  text_shape->fill(text_color.r, text_color.g, text_color.b);
  text_shape->opacity(text_color.a);

  float text_x = 12;
  float text_y = item_height / 2 + style->font_size / 3;
  text_shape->translate(text_x, text_y);

  scene->push(std::move(text_shape));
}

void SelectWidget::render_arrow(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color arrow_color = style->get_variable_color("--select-arrow", {100, 100, 100, 255});
  float item_height = style->get_variable_float("--item-height", 32.0f);
  float width = elem.width();

  // 下拉箭头：三角形
  float arrow_size = 6.0f;
  float arrow_x = width - 20;
  float arrow_y = item_height / 2;

  auto arrow = tvg::Shape::gen();

  if (expanded_) {
    // 向上箭头
    arrow->moveTo(arrow_x - arrow_size, arrow_y + arrow_size / 2);
    arrow->lineTo(arrow_x, arrow_y - arrow_size / 2);
    arrow->lineTo(arrow_x + arrow_size, arrow_y + arrow_size / 2);
  } else {
    // 向下箭头
    arrow->moveTo(arrow_x - arrow_size, arrow_y - arrow_size / 2);
    arrow->lineTo(arrow_x, arrow_y + arrow_size / 2);
    arrow->lineTo(arrow_x + arrow_size, arrow_y - arrow_size / 2);
  }

  arrow->close();
  arrow->fill(arrow_color.r, arrow_color.g, arrow_color.b, arrow_color.a);
  scene->push(std::move(arrow));
}

void SelectWidget::render_dropdown(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return;

  Color dropdown_bg = style->get_variable_color("--dropdown-bg", {255, 255, 255, 255});
  Color border_color = style->get_variable_color("--select-border", {200, 200, 200, 255});
  Color text_color = style->get_variable_color("--select-text", {0, 0, 0, 255});
  Color hover_bg = style->get_variable_color("--dropdown-item-hover", {240, 240, 240, 255});
  Color selected_bg = style->get_variable_color("--dropdown-item-selected", {230, 240, 255, 255});

  float item_height = style->get_variable_float("--item-height", 32.0f);
  float width = elem.width();
  float y_offset = item_height;

  // 下拉列表背景
  auto dropdown_rect = tvg::Shape::gen();
  dropdown_rect->appendRect(0, y_offset, width, dropdown_height_, 4, 4);
  dropdown_rect->fill(dropdown_bg.r, dropdown_bg.g, dropdown_bg.b, dropdown_bg.a);
  scene->push(std::move(dropdown_rect));

  // 边框
  auto dropdown_border = tvg::Shape::gen();
  dropdown_border->appendRect(0, y_offset, width, dropdown_height_, 4, 4);
  dropdown_border->strokeFill(border_color.r, border_color.g, border_color.b, border_color.a);
  dropdown_border->strokeWidth(1);
  scene->push(std::move(dropdown_border));

  // 渲染每个选项
  int visible_items = static_cast<int>(dropdown_height_ / item_height);

  for (int i = 0; i < static_cast<int>(options_.size()) && i < visible_items; i++) {
    float item_y = y_offset + i * item_height;

    // 选项背景（悬停或选中）
    if (i == hovered_index_ || i == selected_index_) {
      auto item_bg = tvg::Shape::gen();
      item_bg->appendRect(0, item_y, width, item_height);

      Color bg = (i == hovered_index_) ? hover_bg : selected_bg;
      item_bg->fill(bg.r, bg.g, bg.b, bg.a);
      scene->push(std::move(item_bg));
    }

    // 选项文字
    auto item_text = tvg::Text::gen();
    item_text->font(style->font_family.c_str());
    item_text->size(style->font_size);
    item_text->text(options_[i].c_str());
    item_text->fill(text_color.r, text_color.g, text_color.b);
    item_text->opacity(text_color.a);

    float text_x = 12;
    float text_y = item_y + item_height / 2 + style->font_size / 3;
    item_text->translate(text_x, text_y);

    scene->push(std::move(item_text));
  }
}

// ============================================================================
// 事件处理
// ============================================================================

bool SelectWidget::handle_mouse_down(const Event& event, Element& elem) {
  auto* style = elem.computed_style;
  if (!style) return false;

  float item_height = style->get_variable_float("--item-height", 32.0f);

  // 点击选择框本身
  if (event.y < item_height) {
    toggle_dropdown(elem);
    return true;
  }

  // 点击下拉列表中的选项
  if (expanded_ && event.y >= item_height) {
    int clicked_index = static_cast<int>((event.y - item_height) / item_height);

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

  if (event.y >= item_height) {
    int new_hovered = static_cast<int>((event.y - item_height) / item_height);

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

} // namespace tvgbox2
