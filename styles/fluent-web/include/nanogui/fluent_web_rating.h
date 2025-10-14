#pragma once

#include <nanogui/widget.h>

#include <functional>

NAMESPACE_BEGIN(nanogui)

class FluentWebTheme;

/**
 * Fluent 2 rating component (star rating).
 */
class NANOGUI_EXPORT FluentWebRating : public Widget {
public:
  explicit FluentWebRating(Widget *parent, int max_rating = 5);

  void set_rating(int rating);
  int rating() const { return m_rating; }

  void set_max_rating(int max) { m_max_rating = max; preferred_size_changed(); }
  int max_rating() const { return m_max_rating; }

  void set_editable(bool editable) { m_editable = editable; }
  bool editable() const { return m_editable; }

  void set_callback(const std::function<void(int)> &cb) { m_callback = cb; }

  void set_theme(Theme *theme) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void draw(NVGcontext *ctx) override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button,
                          int modifiers) override;

protected:
  void refresh_tokens();
  int star_at_position(const Vector2i &p) const;

  int m_rating;
  int m_max_rating;
  int m_hover_rating;
  bool m_editable;
  std::function<void(int)> m_callback;

  Color m_filled_color;
  Color m_empty_color;
  Color m_hover_color;
  int m_star_size;
  int m_star_spacing;
};

NAMESPACE_END(nanogui)
