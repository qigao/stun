/*
 * flexUI - TooltipWidget
 *
 * Hover information popup - 使用 flex::Renderer 渲染
 */

#ifndef FLEXUI_TOOLTIP_WIDGET_H
#define FLEXUI_TOOLTIP_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>

namespace flexUI {

/**
 * TooltipWidget - Hover information popup
 *
 * Attaches to a parent element and shows on hover.
 *
 * CSS variables:
 *   --tooltip-bg: "r,g,b,a"           // Background color
 *   --tooltip-text: "r,g,b,a"         // Text color
 *   --tooltip-delay: "500"            // Show delay (ms)
 *   --tooltip-position: "top" | "bottom" | "left" | "right"
 *   --tooltip-offset: "8"             // Distance from target
 *   --tooltip-arrow: "true" | "false" // Show arrow pointer
 *
 * Example:
 *   auto* tooltip = box->create_widget<TooltipWidget>("tooltip", "tip1", "Help text");
 */
class TooltipWidget : public Widget {
public:
  enum class Position { Top, Bottom, Left, Right };

  explicit TooltipWidget(const std::string& text = "");

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "TooltipWidget"; }

  // Content
  const std::string& text() const { return text_; }
  void set_text(const std::string& text) { text_ = text; dirty_ = true; }

  // Visibility control
  void show() { target_visible_ = true; }
  void hide() { target_visible_ = false; }
  bool is_visible() const { return visible_; }

  // Position
  Position position() const { return position_; }
  void set_position(Position pos) { position_ = pos; dirty_ = true; }

private:
  void render_background(flex::Renderer& r, const Element& elem);
  void render_text(flex::Renderer& r, const Element& elem);
  void render_arrow(flex::Renderer& r, const Element& elem);

  std::string text_;
  Position position_ = Position::Top;
  bool visible_ = false;
  bool target_visible_ = false;
  float delay_timer_ = 0;
  float opacity_ = 0;  // For fade animation
};

} // namespace flexUI

#endif // FLEXUI_TOOLTIP_WIDGET_H
