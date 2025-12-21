/*
 * tvgbox2 - AccordionWidget Implementation
 */

#include <tvgbox2/widgets/accordion_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <algorithm>
#include <cmath>

namespace tvgbox2 {

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

void AccordionWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  auto* style = elem.computed_style;
  float header_height = 44.0f;
  float y = 0;

  for (size_t i = 0; i < sections_.size(); i++) {
    render_section(scene, elem, sections_[i], y);
    y += header_height;
    if (animation_progress_[i] > 0) {
      y += sections_[i].content_height * animation_progress_[i];
    }
  }
}

void AccordionWidget::render_section(tvg::Scene* scene, const Element& elem, Section& section, float y) {
  auto* style = elem.computed_style;

  Color bg_color = {255, 255, 255, 255};
  Color border_color = {229, 231, 235, 255};
  Color text_color = {0, 0, 0, 255};
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

  size_t idx = &section - &sections_[0];
  float progress = animation_progress_[idx];

  auto header_bg = tvg::Shape::gen();
  header_bg->appendRect(0, y, elem.width(), header_height);
  header_bg->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);
  scene->push(std::move(header_bg));

  auto border = tvg::Shape::gen();
  border->moveTo(0, y + header_height);
  border->lineTo(elem.width(), y + header_height);
  border->strokeFill(border_color.r, border_color.g, border_color.b, border_color.a);
  border->strokeWidth(1);
  scene->push(std::move(border));

  render_arrow(scene, 16, y + header_height / 2, section.expanded);

  auto title = tvg::Text::gen();
  title->font(font_family.c_str());
  title->size(font_size);
  title->text(section.title.c_str());
  title->fill(text_color.r, text_color.g, text_color.b);
  title->translate(40, y + header_height / 2 + font_size / 3);
  scene->push(std::move(title));

  if (progress > 0) {
    float content_y = y + header_height;
    float visible_height = section.content_height * progress;
    
    auto content_bg = tvg::Shape::gen();
    content_bg->appendRect(0, content_y, elem.width(), visible_height);
    content_bg->fill(250, 250, 250, 255);
    scene->push(std::move(content_bg));
  }
}

void AccordionWidget::render_arrow(tvg::Scene* scene, float x, float y, bool expanded) {
  auto arrow = tvg::Shape::gen();
  
  if (expanded) {
    arrow->moveTo(x - 4, y - 2);
    arrow->lineTo(x, y + 3);
    arrow->lineTo(x + 4, y - 2);
  } else {
    arrow->moveTo(x - 2, y - 4);
    arrow->lineTo(x + 3, y);
    arrow->lineTo(x - 2, y + 4);
  }

  arrow->strokeFill(100, 100, 100, 255);
  arrow->strokeWidth(2);
  arrow->strokeCap(tvg::StrokeCap::Round);
  arrow->strokeJoin(tvg::StrokeJoin::Round);

  scene->push(std::move(arrow));
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

} // namespace tvgbox2
