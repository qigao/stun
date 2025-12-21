/*
 * tvgbox2 - DropdownWidget Implementation
 */

#include <tvgbox2/widgets/dropdown_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/box.h>
#include <thorvg.h>
#include <algorithm>

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

void DropdownWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  render_button(scene, elem);
  render_arrow(scene, elem);

  if (open_) {
    // Render dropdown menu to overlay scene (renders on top of everything)
    if (elem.owner_box) {
      auto* overlay = elem.owner_box->get_overlay_scene();
      if (overlay) {
        render_dropdown(overlay, elem);
        return;
      }
    }
    // Fallback: render in element's scene
    render_dropdown(scene, elem);
  }
}

void DropdownWidget::render_button(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;

  Color bg_color = {255, 255, 255, 255};
  Color border_color = {200, 200, 200, 255};
  Color text_color = {0, 0, 0, 255};
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

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, elem.width(), elem.height(), radius, radius);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);
  scene->push(std::move(bg));

  auto border = tvg::Shape::gen();
  border->appendRect(0, 0, elem.width(), elem.height(), radius, radius);
  border->strokeFill(border_color.r, border_color.g, border_color.b, border_color.a);
  border->strokeWidth(1);
  scene->push(std::move(border));

  std::string display_text = placeholder_;
  if (selected_index_ >= 0 && selected_index_ < static_cast<int>(options_.size())) {
    display_text = options_[selected_index_].label;
  }

  float padding = 12.0f;
  float text_y = elem.height() / 2 + font_size / 3;

  auto text_shape = tvg::Text::gen();
  text_shape->font(font_family.c_str());
  text_shape->size(font_size);
  text_shape->text(display_text.c_str());

  if (selected_index_ < 0) {
    text_shape->fill(150, 150, 150);
  } else {
    text_shape->fill(text_color.r, text_color.g, text_color.b);
  }
  text_shape->translate(padding, text_y);

  scene->push(std::move(text_shape));
}

void DropdownWidget::render_arrow(tvg::Scene* scene, const Element& elem) {
  float arrow_size = 8.0f;
  float padding = 12.0f;
  float cx = elem.width() - padding - arrow_size / 2;
  float cy = elem.height() / 2;

  auto arrow = tvg::Shape::gen();

  if (open_) {
    arrow->moveTo(cx - arrow_size / 2, cy + 2);
    arrow->lineTo(cx, cy - 3);
    arrow->lineTo(cx + arrow_size / 2, cy + 2);
  } else {
    arrow->moveTo(cx - arrow_size / 2, cy - 2);
    arrow->lineTo(cx, cy + 3);
    arrow->lineTo(cx + arrow_size / 2, cy - 2);
  }

  arrow->strokeFill(100, 100, 100, 255);
  arrow->strokeWidth(2);
  arrow->strokeCap(tvg::StrokeCap::Round);
  arrow->strokeJoin(tvg::StrokeJoin::Round);

  scene->push(std::move(arrow));
}

void DropdownWidget::render_dropdown(tvg::Scene* scene, const Element& elem) {
  if (options_.empty()) return;

  auto* style = elem.computed_style;

  Color bg_color = {255, 255, 255, 255};
  Color hover_color = {240, 240, 240, 255};
  Color text_color = {0, 0, 0, 255};
  float font_size = 14.0f;
  float max_height = 200.0f;
  std::string font_family = "Arial";

  if (style) {
    bg_color = style->get_variable_color("--dropdown-bg", 
                 style->get_variable_color("--bg", style->background_color));
    
    // Hover: try specific -> derived from text (faint) -> standard fallback
    Color default_hover = {240, 240, 240, 255}; // light grey
    hover_color = style->get_variable_color("--dropdown-item-hover", default_hover);

    text_color = style->get_variable_color("--dropdown-text", 
                   style->get_variable_color("--text-color", style->text_color));
                   
    font_size = style->font_size > 0 ? style->font_size : font_size;
    max_height = style->get_variable_float("--dropdown-max-height", max_height);
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  // Use absolute coordinates for overlay rendering
  float abs_x = elem.absolute_x();
  float abs_y = elem.absolute_y();

  float item_height = font_size * 2.5f;
  float total_height = std::min(max_height, options_.size() * item_height);
  float dropdown_y = abs_y + elem.height() + 4;

  auto shadow = tvg::Shape::gen();
  shadow->appendRect(abs_x + 2, dropdown_y + 2, elem.width(), total_height, 6, 6);
  shadow->fill(0, 0, 0, 30);
  scene->push(std::move(shadow));

  auto bg = tvg::Shape::gen();
  bg->appendRect(abs_x, dropdown_y, elem.width(), total_height, 6, 6);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);
  scene->push(std::move(bg));

  auto border = tvg::Shape::gen();
  border->appendRect(abs_x, dropdown_y, elem.width(), total_height, 6, 6);
  border->strokeFill(200, 200, 200, 255);
  border->strokeWidth(1);
  scene->push(std::move(border));

  float padding = 12.0f;
  for (size_t i = 0; i < options_.size(); i++) {
    float item_y = dropdown_y + i * item_height - scroll_offset_;

    if (item_y + item_height < dropdown_y || item_y > dropdown_y + total_height) continue;

    if (static_cast<int>(i) == hover_index_) {
      auto highlight = tvg::Shape::gen();
      highlight->appendRect(abs_x + 2, item_y + 2, elem.width() - 4, item_height - 4, 4, 4);
      highlight->fill(hover_color.r, hover_color.g, hover_color.b, hover_color.a);
      scene->push(std::move(highlight));
    }

    if (static_cast<int>(i) == selected_index_) {
      auto check = tvg::Shape::gen();
      float cx = abs_x + elem.width() - padding - 6;
      float cy = item_y + item_height / 2;
      check->moveTo(cx - 4, cy);
      check->lineTo(cx - 1, cy + 3);
      check->lineTo(cx + 4, cy - 3);
      check->strokeFill(59, 130, 246, 255);
      check->strokeWidth(2);
      check->strokeCap(tvg::StrokeCap::Round);
      scene->push(std::move(check));
    }

    auto text_shape = tvg::Text::gen();
    text_shape->font(font_family.c_str());
    text_shape->size(font_size);
    text_shape->text(options_[i].label.c_str());

    if (options_[i].disabled) {
      text_shape->fill(180, 180, 180);
    } else {
      text_shape->fill(text_color.r, text_color.g, text_color.b);
    }

    text_shape->translate(abs_x + padding, item_y + item_height / 2 + font_size / 3);
    scene->push(std::move(text_shape));
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
