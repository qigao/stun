/*
 * flexUI - DropdownWidget Implementation
 */

#include <flexUI/widgets/dropdown_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/box.h>
#include <flexUI/text_layout.h>
#include <algorithm>
#include <stb_sprintf.h>

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

OverlayRect dropdown_overlay_rect(const Element& elem, float total_height) {
  const auto anchor_bounds = detail::css_render_world_bounds(&elem);
  return {anchor_bounds.x, anchor_bounds.y + anchor_bounds.height + 4.0f,
          anchor_bounds.width, total_height};
}

}  // namespace

DropdownWidget::DropdownWidget(const std::string& placeholder)
    : placeholder_(placeholder) {}

void DropdownWidget::sync_host_semantics() {
  set_host_attribute("role", "combobox");
  set_host_data_state("open", "closed", open_);
  set_host_boolean_attribute("aria-expanded", open_);
  if (selected_value_.empty()) {
    clear_host_attribute("data-value");
  } else {
    set_host_attribute("data-value", selected_value_);
  }
}

void DropdownWidget::add_option(const std::string& label, const std::string& value, bool disabled) {
  options_.push_back({label, value, disabled});
  dirty_ = true;
}

void DropdownWidget::clear_options() {
  options_.clear();
  selected_index_ = -1;
  selected_value_.clear();
  dirty_ = true;
  sync_host_semantics();
}

void DropdownWidget::set_selected_value(const std::string& value) {
  for (size_t i = 0; i < options_.size(); i++) {
    if (options_[i].value == value) {
      selected_index_ = static_cast<int>(i);
      selected_value_ = value;
      dirty_ = true;
      sync_host_semantics();
      return;
    }
  }
  selected_index_ = -1;
  selected_value_.clear();
  sync_host_semantics();
}

void DropdownWidget::set_selected_index(int index) {
  if (index >= 0 && index < static_cast<int>(options_.size())) {
    selected_index_ = index;
    selected_value_ = options_[index].value;
    dirty_ = true;
    sync_host_semantics();
  }
}

void DropdownWidget::open() {
  if (open_) {
    return;
  }
  open_ = true;
  dirty_ = true;
  sync_host_semantics();
}

void DropdownWidget::close() {
  if (!open_) {
    return;
  }
  open_ = false;
  dirty_ = true;
  sync_host_semantics();
}

void DropdownWidget::toggle() {
  open_ = !open_;
  dirty_ = true;
  sync_host_semantics();
}

void DropdownWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  render_button(commands, elem);
  render_arrow(commands, elem);

  // Dropdown options are now rendered in emit_overlay_commands().
}

void DropdownWidget::emit_overlay_commands(const Element& elem, RenderCommandList& commands) {
  if (!open_) return;
  render_dropdown(commands, elem);
}

void DropdownWidget::render_button(RenderCommandList& commands, const Element& elem) {
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

  commands.draw_rect(0, 0, elem.width(), elem.height(), radius,
                     Paint::solid(bg_color), Paint::none(), 0);
  commands.draw_rect(0, 0, elem.width(), elem.height(), radius, Paint::none(),
                     Paint::solid(border_color), 1);

  std::string display_text = placeholder_;
  if (selected_index_ >= 0 && selected_index_ < static_cast<int>(options_.size())) {
    display_text = options_[selected_index_].label;
  }

  float padding = 12.0f;
  const float arrow_reserve = 28.0f;

  Color display_color = (selected_index_ < 0) ? Color{0.59f, 0.59f, 0.59f, 1.0f} : text_color;
  const float text_width = std::max(0.0f, elem.width() - padding - arrow_reserve - padding);
  const auto text_block = layout_text_block(
      style, display_text, padding, 0.0f, text_width, elem.height(), display_color,
      TextVerticalAlign::Middle);
  emit_text_block(commands, text_block);
}

void DropdownWidget::render_arrow(RenderCommandList& commands, const Element& elem) {
  float arrow_size = 8.0f;
  float padding = 12.0f;
  float cx = elem.width() - padding - arrow_size / 2;
  float cy = elem.height() / 2;

  char path[128];
  if (open_) {
    stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
             cx - arrow_size / 2, cy + 2,
             cx, cy - 3,
             cx + arrow_size / 2, cy + 2);
  } else {
    stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
             cx - arrow_size / 2, cy - 2,
             cx, cy + 3,
             cx + arrow_size / 2, cy - 2);
  }

  commands.stroke_path(path, Paint::solid(Color{0.39f, 0.39f, 0.39f, 1.0f}), 2);
}

void DropdownWidget::render_dropdown(RenderCommandList& commands, const Element& elem) {
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
  const OverlayRect dropdown_bounds = dropdown_overlay_rect(elem, total_height);
  float dropdown_x = dropdown_bounds.x;
  float dropdown_y = dropdown_bounds.y;
  float dropdown_w = dropdown_bounds.w;

  // Shadow
  commands.draw_rect(dropdown_x + 2, dropdown_y + 2, dropdown_w, total_height, 6,
                     Paint::solid(Color{0.0f, 0.0f, 0.0f, 0.12f}), Paint::none(), 0);

  // Background
  commands.draw_rect(dropdown_x, dropdown_y, dropdown_w, total_height, 6,
                     Paint::solid(bg_color), Paint::none(), 0);

  // Border
  commands.draw_rect(dropdown_x, dropdown_y, dropdown_w, total_height, 6,
                     Paint::none(), Paint::solid(Color{0.78f, 0.78f, 0.78f, 1.0f}), 1);

  float padding = 12.0f;
  for (size_t i = 0; i < options_.size(); i++) {
    float item_y = dropdown_y + i * item_height - scroll_offset_;

    if (item_y + item_height < dropdown_y || item_y > dropdown_y + total_height) continue;

    if (static_cast<int>(i) == hover_index_) {
      commands.draw_rect(dropdown_x + 2, item_y + 2, dropdown_w - 4,
                         item_height - 4, 4, Paint::solid(hover_color),
                         Paint::none(), 0);
    }

    if (static_cast<int>(i) == selected_index_) {
      // Checkmark
      float cx = dropdown_x + dropdown_w - padding - 6;
      float cy = item_y + item_height / 2;
      char check_path[128];
      stbsp_snprintf(check_path, sizeof(check_path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
               cx - 4, cy,
               cx - 1, cy + 3,
               cx + 4, cy - 3);
      commands.stroke_path(check_path, Paint::solid(Color{0.23f, 0.51f, 0.96f, 1.0f}), 2);
    }

    Color item_text_color = options_[i].disabled ? Color{0.71f, 0.71f, 0.71f, 1.0f} : text_color;
    const float text_width = std::max(0.0f, dropdown_w - padding * 2.0f - 18.0f);
    const auto text_block = layout_text_block(
        style, options_[i].label, dropdown_x + padding, item_y, text_width,
        item_height, item_text_color, TextVerticalAlign::Middle);
    emit_text_block(commands, text_block);
  }
}

bool DropdownWidget::handle_event(const Event& event, Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style ? (style->font_size > 0 ? style->font_size : 14.0f) : 14.0f;
  float item_height = font_size * 2.5f;
  float max_height = style ? style->get_variable_float("--dropdown-max-height", 200.0f) : 200.0f;
  float visible_height = std::min(max_height, options_.size() * item_height);
  const OverlayRect dropdown_bounds = dropdown_overlay_rect(elem, visible_height);

  switch (event.type) {
    case EventType::MouseDown: {
      const flex::Vec2 local_pos =
          detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
      float local_x = local_pos.x;
      float local_y = local_pos.y;

      if (local_x >= 0 && local_x <= elem.width() &&
          local_y >= 0 && local_y <= elem.height()) {
        toggle();
        elem.mark_paint_dirty();
        return true;
      }

      if (open_ && hit_overlay(event.x, event.y, dropdown_bounds)) {
        int index = static_cast<int>(
            (event.y - dropdown_bounds.y + scroll_offset_) / item_height);
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

      const flex::Vec2 local_pos =
          detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
      (void)local_pos;

      // Check if within dropdown bounds
      if (hit_overlay(event.x, event.y, dropdown_bounds)) {
        float offset_in_dropdown = event.y - dropdown_bounds.y + scroll_offset_;
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

      float total_content_height = options_.size() * item_height;
      float max_scroll = std::max(0.0f, total_content_height - max_height);

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
  // Dropdown 只在状态变化时标记 dirty（在 handle_event 中处理）
  // 不需要每帧重绘
  (void)delta_ms;
  (void)elem;
}

} // namespace flexUI
