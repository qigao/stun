/*
 * flexUI - BadgeWidget
 *
 * Small label/count indicator - 使用 RenderCommandList 渲染
 */

#ifndef FLEXUI_BADGE_WIDGET_H
#define FLEXUI_BADGE_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../render_command.h"
#include "../shapes.h"
#include <string>

namespace flexUI {

/**
 * BadgeWidget - Notification badge / status indicator
 *
 * CSS variables:
 *   --badge-bg: "r,g,b,a"        // Background color
 *   --badge-text: "r,g,b,a"      // Text color
 *   --badge-size: "20"           // Min size (for dot badge)
 *   --badge-dot: "true" | "false" // Show as dot (no text)
 */
class BadgeWidget : public Widget {
public:
  explicit BadgeWidget(const std::string& text = "");

  void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  bool needs_frame_update(const Element& elem) const override {
    (void)elem;
    return false;
  }
  bool state_affects_paint(Symbol state) const override {
    (void)state;
    return false;
  }
  const char* type_name() const override { return "BadgeWidget"; }
  bool paints_host_box() const override { return true; }

  // Badge content
  const std::string& text() const { return text_; }
  void set_text(const std::string& text) { text_ = text; sync_host_semantics(); dirty_ = true; }

  // Numeric value (convenience)
  void set_count(int count);
  int count() const { return count_; }

  // Dot mode (no text, just colored dot)
  bool is_dot() const { return dot_; }
  void set_dot(bool dot) { dot_ = dot; sync_host_semantics(); dirty_ = true; }

  // Visibility
  void show() { visible_ = true; sync_host_semantics(); dirty_ = true; }
  void hide() { visible_ = false; sync_host_semantics(); dirty_ = true; }
  bool is_visible() const { return visible_; }

private:
  void sync_host_semantics() override;
  void render_background(RenderCommandList& commands, const Element& elem);
  void render_text(RenderCommandList& commands, const Element& elem);

  std::string text_;
  int count_ = 0;
  bool dot_ = false;
  bool visible_ = true;
};

} // namespace flexUI

#endif // FLEXUI_BADGE_WIDGET_H
