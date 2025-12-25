/*
 * tvgbox2 - DropdownWidget Implementation
 */

#include <tvgbox2/widgets/dropdown_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <tvgbox2/box.h>
#include <algorithm>
#include <cstdio>

namespace tvgbox2 {

DropdownWidget::DropdownWidget(const std::string& placeholder)
    : placeholder_(placeholder) {}

void DropdownWidget::add_option(const std::string& label, const std::string& value, bool disabled) {
  options_.push_back({label, value, disabled});
  dirty_ = true;
}

void DropdownWidget::clear_options() {
  options_.clear();
  selected_index_ = -1;
  selected_value_.clear();
  dirty_ = true;
}

void DropdownWidget::set_selected_value(const std::string& value) {
  for (size_t i = 0; i < options_.size(); i++) {
    if (options_[i].value == value) {
      selected_index_ = static_cast<int>(i);
      selected_value_ = value;
      dirty_ = true;
      return;
    }
  }
  selected_index_ = -1;
  selected_value_.clear();
}

void DropdownWidget::set_selected_index(int index) {
  if (index >= 0 && index < static_cast<int>(options_.size())) {
    selected_index_ = index;
    selected_value_ = options_[index].value;
    dirty_ = true;
  }
}

void DropdownWidget::render(const Element& elem, Renderer& renderer) {
  auto& r = renderer.flex();

  render_button(r, elem);
  render_arrow(r, elem);

  if (open_) {
    render_dropdown(r, elem);
  }
}

void DropdownWidget::render_button(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = {1.0f, 1.0f, 1.0f, 1.0f};
  Color border_color = {0.78f, 0.78f, 0.78f, 1.0f};
  Color text_color = {0.0f, 0.0f, 0.0f, 1.0f};
  float radius = 6.0f;
  float font_size = 14.0f;
  std::string font_family = "Arial";

  if (style) {
    bg_color = style->get_variable_color("--dropdown-bg",
                 style->get_variable_color("--bg", style->background_color));

    border_color = style->get_variable_color("--dropdown-border",
                     style->get_variable_color("--border-color",
                       style->get_variable_color("--border", border_color)));

    text_color = style->get_variable_color("--dropdown-text",
                   style->get_variable_color("--text-color", style->text_color));

    radius = style->border_radius[0];
    font_size = style->font_size > 0 ? style->font_size : font_size;
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  r.draw_rect(0, 0, elem.width(), elem.height(), radius, Paint::solid(bg_color), Paint::none(), 0);
  r.draw_rect(0, 0, elem.width(), elem.height(), radius, Paint::none(), Paint::solid(border_color), 1);

  std::string display_text = placeholder_;
  if (selected_index_ >= 0 && selected_index_ < static_cast<int>(options_.size())) {
    display_text = options_[selected_index_].label;
  }

  float padding = 12.0f;
  float text_y = elem.height() / 2 + font_size / 3;

  Color display_color = (selected_index_ < 0) ? Color{0.59f, 0.59f, 0.59f, 1.0f} : text_color;
  r.draw_text(display_text, padding, text_y, font_family, font_size, false, display_color);
}

void DropdownWidget::render_arrow(flex::Renderer& r, const Element& elem) {
  float arrow_size = 8.0f;
  float padding = 12.0f;
  float cx = elem.width() - padding - arrow_size / 2;
  float cy = elem.height() / 2;

  char path[128];
  if (open_) {
    snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
             cx - arrow_size / 2, cy + 2,
             cx, cy - 3,
             cx + arrow_size / 2, cy + 2);
  } else {
    snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
             cx - arrow_size / 2, cy - 2,
             cx, cy + 3,
             cx + arrow_size / 2, cy - 2);
  }

  r.stroke_path(path, Paint::solid(Color{0.39f, 0.39f, 0.39f, 1.0f}), 2);
}

void DropdownWidget::render_dropdown(flex::Renderer& r, const Element& elem) {
  if (options_.empty()) return;

  auto* style = elem.computed_style;

  Color bg_color = {1.0f, 1.0f, 1.0f, 1.0f};
  Color hover_color = {0.94f, 0.94f, 0.94f, 1.0f};
  Color text_color = {0.0f, 0.0f, 0.0f, 1.0f};
  float font_size = 14.0f;
  float max_height = 200.0f;
  std::string font_family = "Arial";

  if (style) {
    bg_color = style->get_variable_color("--dropdown-bg",
                 style->get_variable_color("--bg", style->background_color));

    hover_color = style->get_variable_color("--dropdown-item-hover", hover_color);

    text_color = style->get_variable_color("--dropdown-text",
                   style->get_variable_color("--text-color", style->text_color));

    font_size = style->font_size > 0 ? style->font_size : font_size;
    max_height = style->get_variable_float("--dropdown-max-height", max_height);
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  float item_height = font_size * 2.5f;
  float total_height = std::min(max_height, options_.size() * item_height);
  float dropdown_y = elem.height() + 4;

  // Shadow
  r.draw_rect(2, dropdown_y + 2, elem.width(), total_height, 6,
              Paint::solid(Color{0.0f, 0.0f, 0.0f, 0.12f}), Paint::none(), 0);

  // Background
  r.draw_rect(0, dropdown_y, elem.width(), total_height, 6,
              Paint::solid(bg_color), Paint::none(), 0);

  // Border
  r.draw_rect(0, dropdown_y, elem.width(), total_height, 6,
              Paint::none(), Paint::solid(Color{0.78f, 0.78f, 0.78f, 1.0f}), 1);

  float padding = 12.0f;
  for (size_t i = 0; i < options_.size(); i++) {
    float item_y = dropdown_y + i * item_height - scroll_offset_;

    if (item_y + item_height < dropdown_y || item_y > dropdown_y + total_height) continue;

    if (static_cast<int>(i) == hover_index_) {
      r.draw_rect(2, item_y + 2, elem.width() - 4, item_height - 4, 4,
                  Paint::solid(hover_color), Paint::none(), 0);
    }

    if (static_cast<int>(i) == selected_index_) {
      // Checkmark
      float cx = elem.width() - padding - 6;
      float cy = item_y + item_height / 2;
      char check_path[128];
      snprintf(check_path, sizeof(check_path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
               cx - 4, cy,
               cx - 1, cy + 3,
               cx + 4, cy - 3);
      r.stroke_path(check_path, Paint::solid(Color{0.23f, 0.51f, 0.96f, 1.0f}), 2);
    }

    Color item_text_color = options_[i].disabled ? Color{0.71f, 0.71f, 0.71f, 1.0f} : text_color;
    r.draw_text(options_[i].label, padding, item_y + item_height / 2 + font_size / 3,
                font_family, font_size, false, item_text_color);
  }
}

bool DropdownWidget::handle_event(const Event& event, Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style ? (style->font_size > 0 ? style->font_size : 14.0f) : 14.0f;
  float item_height = font_size * 2.5f;
  float dropdown_y = elem.height() + 4;

  switch (event.type) {
    case EventType::MouseDown: {
      float local_y = event.y - elem.absolute_y();

      if (local_y >= 0 && local_y <= elem.height()) {
        toggle();
        elem.mark_paint_dirty();
        return true;
      }

      if (open_ && local_y > dropdown_y) {
        int index = static_cast<int>((local_y - dropdown_y + scroll_offset_) / item_height);
        if (index >= 0 && index < static_cast<int>(options_.size())) {
          if (!options_[index].disabled) {
            set_selected_index(index);
            close();
            elem.mark_paint_dirty();
            if (on_change_) on_change_(selected_value_);
          }
        }
        return true;
      }

      if (open_) {
        close();
        elem.mark_paint_dirty();
      }
      break;
    }

    case EventType::MouseMove: {
      if (!open_) return false;

      float local_x = event.x - elem.absolute_x();
      float local_y = event.y - elem.absolute_y();

      // Check if within dropdown bounds
      if (local_y > dropdown_y && local_x >= 0 && local_x <= elem.width()) {
        float offset_in_dropdown = local_y - dropdown_y + scroll_offset_;
        int index = static_cast<int>(offset_in_dropdown / item_height);

        // Clamp to valid range
        if (index < 0) index = 0;
        if (index >= static_cast<int>(options_.size())) index = static_cast<int>(options_.size()) - 1;

        if (index != hover_index_) {
          hover_index_ = index;
          elem.mark_paint_dirty();
        }
      } else {
        // Outside dropdown area - clear hover
        if (hover_index_ != -1) {
          hover_index_ = -1;
          elem.mark_paint_dirty();
        }
      }
      break;
    }

    case EventType::MouseWheel: {
      if (!open_) return false;

      float max_height = style ? style->get_variable_float("--dropdown-max-height", 200.0f) : 200.0f;
      float total_height = options_.size() * item_height;
      float max_scroll = std::max(0.0f, total_height - max_height);

      scroll_offset_ = std::clamp(scroll_offset_ - event.delta_y * 20, 0.0f, max_scroll);
      elem.mark_paint_dirty();
      return true;
    }

    default:
      break;
  }

  return false;
}

void DropdownWidget::update(float delta_ms, Element& elem) {
  // Keep dirty while open so overlay renders each frame
  if (open_) {
    elem.mark_paint_dirty();
  }
}

} // namespace tvgbox2
