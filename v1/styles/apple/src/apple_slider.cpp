/*
    src/apple_slider.cpp -- Apple HIG slider implementation
*/

#include <nanogui/apple_slider.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleSlider::AppleSlider(Widget *parent)
    : Widget(parent), m_value(0.5f), m_dragging(false) {
  set_fixed_height(24);
}

AppleTheme *AppleSlider::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

bool AppleSlider::mouse_button_event(const Vector2i &p, int button, bool down,
                                      int modifiers) {
  Widget::mouse_button_event(p, button, down, modifiers);
  if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
    return false;

  if (down) {
    m_dragging = true;
    float track_width = m_size.x() - 16.0f;
    float new_value = (p.x() - m_pos.x() - 8.0f) / track_width;
    m_value = std::max(0.0f, std::min(1.0f, new_value));
    if (m_callback)
      m_callback(m_value);
  } else {
    if (m_dragging && m_final_callback)
      m_final_callback(m_value);
    m_dragging = false;
  }
  return true;
}

bool AppleSlider::mouse_drag_event(const Vector2i &p, const Vector2i &rel,
                                    int button, int modifiers) {
  if (!m_enabled || !m_dragging)
    return false;

  float track_width = m_size.x() - 16.0f;
  float new_value = (p.x() - m_pos.x() - 8.0f) / track_width;
  m_value = std::max(0.0f, std::min(1.0f, new_value));
  
  if (m_callback)
    m_callback(m_value);
  
  return true;
}

void AppleSlider::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  AppleTheme *theme = apple_theme();
  if (!theme)
    return;

  float track_height = 4.0f;
  float thumb_radius = 8.0f;
  float track_width = m_size.x() - thumb_radius * 2;
  
  float track_x = m_pos.x() + thumb_radius;
  float track_y = m_pos.y() + (m_size.y() - track_height) * 0.5f;
  float thumb_x = track_x + m_value * track_width;
  float thumb_y = m_pos.y() + m_size.y() * 0.5f;

  // Draw track background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, track_x, track_y, track_width, track_height, track_height * 0.5f);
  nvgFillColor(ctx, theme->tertiary_system_fill());
  nvgFill(ctx);

  // Draw filled track
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, track_x, track_y, m_value * track_width, track_height,
                 track_height * 0.5f);
  nvgFillColor(ctx, theme->accent());
  nvgFill(ctx);

  // Draw thumb
  nvgBeginPath(ctx);
  nvgCircle(ctx, thumb_x, thumb_y, thumb_radius);
  
  // Thumb shadow
  NVGpaint shadow = nvgRadialGradient(ctx, thumb_x, thumb_y + 1, thumb_radius - 2,
                                      thumb_radius + 2,
                                      nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 0));
  nvgFillPaint(ctx, shadow);
  nvgFill(ctx);

  // Thumb fill
  nvgBeginPath(ctx);
  nvgCircle(ctx, thumb_x, thumb_y, thumb_radius);
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
  nvgFill(ctx);

  // Thumb border
  nvgStrokeWidth(ctx, 0.5f);
  nvgStrokeColor(ctx, nvgRGBA(0, 0, 0, 20));
  nvgStroke(ctx);
}

Vector2i AppleSlider::preferred_size_impl(NVGcontext *ctx) const {
  return Vector2i(200, 24);
}

NAMESPACE_END(nanogui)
