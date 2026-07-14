/*
 * flexUI - BadgeWidget Implementation
 */
#include <flexUI/widgets/badge_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/text_layout.h>
#include <algorithm>

namespace flexUI {

BadgeWidget::BadgeWidget(const std::string& text) : text_(text) {}

void BadgeWidget::sync_host_semantics() {
  set_host_attribute("role", "status");
  set_host_boolean_attribute("aria-hidden", !visible_);
  set_host_data_state("visible", "hidden", visible_);
  set_host_boolean_attribute("data-dot", dot_);

  if (count_ > 0) {
    set_host_attribute("data-count", std::to_string(count_));
  } else {
    clear_host_attribute("data-count");
  }

  if (!text_.empty()) {
    set_host_attribute("data-value", text_);
    set_host_attribute("aria-label", text_);
  } else if (dot_) {
    clear_host_attribute("data-value");
    set_host_attribute("aria-label", "Badge");
  } else {
    clear_host_attribute("data-value");
    clear_host_attribute("aria-label");
  }
}

void BadgeWidget::set_count(int count) {
  count_ = count;
  if (count > 99) {
    text_ = "99+";
  } else if (count > 0) {
    text_ = std::to_string(count);
  } else {
    text_ = "";
  }
  sync_host_semantics();
  dirty_ = true;
}

void BadgeWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  sync_host_semantics();
  if (!visible_) return;

  render_background(commands, elem);
  if (!dot_) {
    render_text(commands, elem);
  }
}

void BadgeWidget::render_background(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;

  // Badge background color (normalized 0-1)
  Color bg_color = {0.94f, 0.27f, 0.27f, 1.0f};  // Default: red (239/255, 68/255, 68/255)
  if (style) {
    bg_color = style->get_variable_color("--badge-bg", bg_color);
  }

  float w = elem.width();
  float h = elem.height();

  // Badge is always pill-shaped (full radius)
  float radius = std::min(w, h) / 2;

  commands.draw_rect(0, 0, w, h, radius, Paint::solid(bg_color),
                     Paint::none(), 0);
}

void BadgeWidget::render_text(RenderCommandList& commands, const Element& elem) {
  if (text_.empty()) return;

  auto* style = elem.computed_style;

  Color text_color = {1.0f, 1.0f, 1.0f, 1.0f};  // Default: white
  float font_size = 12.0f;
  if (style) {
    text_color = style->get_variable_color("--badge-text", text_color);
    font_size = style->font_size > 0 ? style->font_size : font_size;
  }

  if (style) {
    const auto text_block = layout_text_block(
        style, text_, 0.0f, 0.0f, elem.width(), elem.height(), text_color,
        TextVerticalAlign::Middle);
    emit_text_block(commands, text_block);
  }
}

bool BadgeWidget::handle_event(const Event& event, Element& elem) {
  return false;  // Badges don't handle events
}

void BadgeWidget::update(float delta_ms, Element& elem) {
  // No animation
}

} // namespace flexUI
