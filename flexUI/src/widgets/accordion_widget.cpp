/*
 * flexUI - AccordionWidget Implementation
 */

#include <flexUI/widgets/accordion_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <cmath>
#include <stb_sprintf.h>

namespace flexUI {

AccordionWidget::AccordionWidget() {}

void AccordionWidget::add_section(const std::string& title, const std::string& id, float content_height) {
  sections_.push_back({title, id, false, content_height});
  animation_progress_.push_back(0.0f);
  dirty_ = true;
}

void AccordionWidget::remove_section(const std::string& id) {
  for (size_t i = 0; i < sections_.size(); i++) {
    if (sections_[i].id == id) {
      sections_.erase(sections_.begin() + i);
      animation_progress_.erase(animation_progress_.begin() + i);
      dirty_ = true;
      return;
    }
  }
}

void AccordionWidget::clear_sections() {
  sections_.clear();
  animation_progress_.clear();
  dirty_ = true;
}

void AccordionWidget::expand(const std::string& id) {
  for (size_t i = 0; i < sections_.size(); i++) {
    if (sections_[i].id == id) {
      if (!allow_multiple_) {
        for (auto& s : sections_) s.expanded = false;
      }
      sections_[i].expanded = true;
      dirty_ = true;
      if (on_change_) on_change_(id, true);
      return;
    }
  }
}

void AccordionWidget::collapse(const std::string& id) {
  for (auto& s : sections_) {
    if (s.id == id) {
      s.expanded = false;
      dirty_ = true;
      if (on_change_) on_change_(id, false);
      return;
    }
  }
}

void AccordionWidget::toggle(const std::string& id) {
  for (auto& s : sections_) {
    if (s.id == id) {
      if (s.expanded) collapse(id);
      else expand(id);
      return;
    }
  }
}

bool AccordionWidget::is_expanded(const std::string& id) const {
  for (const auto& s : sections_) {
    if (s.id == id) return s.expanded;
  }
  return false;
}

void AccordionWidget::render(const Element& elem, Renderer& renderer) {
  auto& r = renderer.flex();
  float header_height = 44.0f;
  float y = 0;

  for (size_t i = 0; i < sections_.size(); i++) {
    render_section(r, elem, sections_[i], y, i);
    y += header_height;
    if (animation_progress_[i] > 0) {
      y += sections_[i].content_height * animation_progress_[i];
    }
  }
}

void AccordionWidget::render_section(flex::Renderer& r, const Element& elem, Section& section, float y, size_t idx) {
  auto* style = elem.computed_style;

  Color bg_color = {1.0f, 1.0f, 1.0f, 1.0f};
  Color border_color = {0.90f, 0.91f, 0.92f, 1.0f};
  Color text_color = {0.0f, 0.0f, 0.0f, 1.0f};
  float font_size = 14.0f;
  float header_height = 44.0f;
  std::string font_family = "Arial";

  if (style) {
    bg_color = style->get_variable_color("--accordion-bg", bg_color);
    border_color = style->get_variable_color("--accordion-border", border_color);
    text_color = style->get_variable_color("--accordion-text", text_color);
    font_size = style->font_size > 0 ? style->font_size : font_size;
    if (!style->font_family.empty()) font_family = style->font_family;
  }

  float progress = animation_progress_[idx];

  r.draw_rect(0, y, elem.width(), header_height, 0, Paint::solid(bg_color), Paint::none(), 0);

  // Border line
  char path[128];
  stbsp_snprintf(path, sizeof(path), "M 0 %.4g L %.4g %.4g", y + header_height, elem.width(), y + header_height);
  r.stroke_path(path, Paint::solid(border_color), 1);

  render_arrow(r, 16, y + header_height / 2, section.expanded);

  r.draw_text(section.title, 40, y + header_height / 2 - font_size / 2,
              font_family, font_size, false, text_color);

  if (progress > 0) {
    float content_y = y + header_height;
    float visible_height = section.content_height * progress;

    r.draw_rect(0, content_y, elem.width(), visible_height, 0,
                Paint::solid(Color{0.98f, 0.98f, 0.98f, 1.0f}), Paint::none(), 0);
  }
}

void AccordionWidget::render_arrow(flex::Renderer& r, float x, float y, bool expanded) {
  char path[128];

  if (expanded) {
    stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
             x - 4, y - 2, x, y + 3, x + 4, y - 2);
  } else {
    stbsp_snprintf(path, sizeof(path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g",
             x - 2, y - 4, x + 3, y, x - 2, y + 4);
  }

  r.stroke_path(path, Paint::solid(Color{0.39f, 0.39f, 0.39f, 1.0f}), 2);
}

bool AccordionWidget::handle_event(const Event& event, Element& elem) {
  if (event.type != EventType::MouseDown) return false;

  float header_height = 44.0f;
  float local_y = event.y - elem.absolute_y();
  float y = 0;

  for (size_t i = 0; i < sections_.size(); i++) {
    if (local_y >= y && local_y < y + header_height) {
      toggle(sections_[i].id);
      elem.mark_paint_dirty();
      return true;
    }
    y += header_height;
    if (animation_progress_[i] > 0) {
      y += sections_[i].content_height * animation_progress_[i];
    }
  }

  return false;
}

void AccordionWidget::update(float delta_ms, Element& elem) {
  bool changed = false;
  float speed = 6.0f * delta_ms / 1000.0f;

  for (size_t i = 0; i < sections_.size(); i++) {
    float target = sections_[i].expanded ? 1.0f : 0.0f;
    if (std::abs(animation_progress_[i] - target) > 0.01f) {
      animation_progress_[i] += (target - animation_progress_[i]) * speed;
      changed = true;
    } else {
      animation_progress_[i] = target;
    }
  }

  if (changed) {
    elem.mark_paint_dirty();
  }
}

} // namespace flexUI
