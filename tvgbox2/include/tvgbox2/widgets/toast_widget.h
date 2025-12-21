/*
 * tvgbox2 - ToastWidget
 *
 * Auto-dismissing notification messages
 */

#ifndef TVGBOX2_TOAST_WIDGET_H
#define TVGBOX2_TOAST_WIDGET_H

#include "../widget.h"
#include "../types.h"
#include <string>
#include <functional>

namespace tvgbox2 {

/**
 * ToastWidget - Notification toast
 *
 * CSS variables:
 *   --toast-bg: "r,g,b,a"           // Background color
 *   --toast-text: "r,g,b,a"         // Text color
 *   --toast-duration: "3000"        // Auto-dismiss time (ms), 0 = manual
 *   --toast-position: "top-right" | "top-left" | "bottom-right" | "bottom-left"
 *
 * Variants (via classes):
 *   .toast-success  - Green success toast
 *   .toast-error    - Red error toast
 *   .toast-warning  - Yellow warning toast
 *   .toast-info     - Blue info toast
 *
 * Example:
 *   auto* toast = box->create_widget<ToastWidget>("toast", "t1", "Saved!", ToastWidget::Type::Success);
 *   toast->show();
 */
class ToastWidget : public Widget {
public:
  enum class Type { Default, Success, Error, Warning, Info };

  explicit ToastWidget(const std::string& message = "", Type type = Type::Default);

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "ToastWidget"; }

  // Content
  const std::string& message() const { return message_; }
  void set_message(const std::string& msg) { message_ = msg; dirty_ = true; }

  // Toast type
  Type toast_type() const { return type_; }
  void set_type(Type t) { type_ = t; dirty_ = true; }

  // Visibility
  void show();
  void hide();
  bool is_visible() const { return visible_; }

  // Duration (0 = no auto-dismiss)
  void set_duration(float ms) { duration_ = ms; }
  float duration() const { return duration_; }

  // Callbacks
  using DismissCallback = std::function<void()>;
  void set_dismiss_callback(DismissCallback cb) { on_dismiss_ = std::move(cb); }

private:
  void render_background(tvg::Scene* scene, const Element& elem);
  void render_icon(tvg::Scene* scene, const Element& elem);
  void render_text(tvg::Scene* scene, const Element& elem);
  void render_close_button(tvg::Scene* scene, const Element& elem);
  Color get_type_color() const;

  std::string message_;
  Type type_ = Type::Default;
  bool visible_ = false;
  float duration_ = 3000.0f;
  float timer_ = 0;
  float opacity_ = 0;
  DismissCallback on_dismiss_;
};

} // namespace tvgbox2

#endif // TVGBOX2_TOAST_WIDGET_H
