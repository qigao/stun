/*
 * tvgbox2 - BadgeWidget
 *
 * Small label/count indicator for notifications, status, etc.
 */

#ifndef TVGBOX2_BADGE_WIDGET_H
#define TVGBOX2_BADGE_WIDGET_H

#include "../widget.h"
#include <string>

namespace tvgbox2 {

/**
 * BadgeWidget - Notification badge / status indicator
 *
 * CSS variables:
 *   --badge-bg: "r,g,b,a"        // Background color
 *   --badge-text: "r,g,b,a"      // Text color
 *   --badge-size: "20"           // Min size (for dot badge)
 *   --badge-dot: "true" | "false" // Show as dot (no text)
 *
 * Example:
 *   auto* badge = box->create_widget<BadgeWidget>("badge", "notif", "3");
 */
class BadgeWidget : public Widget {
public:
  explicit BadgeWidget(const std::string& text = "");

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "BadgeWidget"; }

  // Badge content
  const std::string& text() const { return text_; }
  void set_text(const std::string& text) { text_ = text; dirty_ = true; }

  // Numeric value (convenience)
  void set_count(int count);
  int count() const { return count_; }

  // Dot mode (no text, just colored dot)
  bool is_dot() const { return dot_; }
  void set_dot(bool dot) { dot_ = dot; dirty_ = true; }

  // Visibility
  void show() { visible_ = true; dirty_ = true; }
  void hide() { visible_ = false; dirty_ = true; }
  bool is_visible() const { return visible_; }

private:
  void render_background(tvg::Scene* scene, const Element& elem);
  void render_text(tvg::Scene* scene, const Element& elem);

  std::string text_;
  int count_ = 0;
  bool dot_ = false;
  bool visible_ = true;
};

} // namespace tvgbox2

#endif // TVGBOX2_BADGE_WIDGET_H
