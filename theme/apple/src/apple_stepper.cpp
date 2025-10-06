/*
    src/apple_stepper.cpp -- Apple HIG stepper implementation
*/

#include <nanogui/apple_stepper.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleStepper::AppleStepper(Widget *parent)
    : Widget(parent), m_value(0), m_min_value(0), m_max_value(100), m_step(1),
      m_minus_pressed(false), m_plus_pressed(false) {
  set_fixed_size(Vector2i(94, 29)); // Standard stepper size
}

AppleTheme *AppleStepper::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleStepper::set_value(int value) {
  m_value = std::max(m_min_value, std::min(m_max_value, value));
  if (m_callback)
    m_callback(m_value);
}

void AppleStepper::increment() {
  set_value(m_value + m_step);
}

void AppleStepper::decrement() {
  set_value(m_value - m_step);
}

bool AppleStepper::mouse_button_event(const Vector2i &p, int button, bool down,
                                       int modifiers) {
  Widget::mouse_button_event(p, button, down, modifiers);
  if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
    return false;

  float half_width = m_size.x() * 0.5f;
  bool is_minus = (p.x() - m_pos.x()) < half_width;

  if (down) {
    if (is_minus) {
      m_minus_pressed = true;
      decrement();
    } else {
      m_plus_pressed = true;
      increment();
    }
  } else {
    m_minus_pressed = false;
    m_plus_pressed = false;
  }

  return true;
}

void AppleStepper::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  AppleTheme *theme = apple_theme();
  if (!theme)
    return;

  float corner_radius = theme->corner_radius(AppleTheme::CornerStyle::Medium);
  float half_width = m_size.x() * 0.5f;

  // Draw container
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(),
                 corner_radius);
  nvgFillColor(ctx, theme->system_fill());
  nvgFill(ctx);

  nvgStrokeWidth(ctx, 1.0f);
  nvgStrokeColor(ctx, theme->separator());
  nvgStroke(ctx);

  // Draw minus button background if pressed
  if (m_minus_pressed) {
    nvgBeginPath(ctx);
    nvgRoundedRectVarying(ctx, m_pos.x() + 1, m_pos.y() + 1, half_width - 2,
                          m_size.y() - 2, corner_radius - 1, 0, 0,
                          corner_radius - 1);
    nvgFillColor(ctx, theme->tertiary_system_fill());
    nvgFill(ctx);
  }

  // Draw plus button background if pressed
  if (m_plus_pressed) {
    nvgBeginPath(ctx);
    nvgRoundedRectVarying(ctx, m_pos.x() + half_width + 1, m_pos.y() + 1,
                          half_width - 2, m_size.y() - 2, 0, corner_radius - 1,
                          corner_radius - 1, 0);
    nvgFillColor(ctx, theme->tertiary_system_fill());
    nvgFill(ctx);
  }

  // Draw separator
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, m_pos.x() + half_width, m_pos.y() + 4);
  nvgLineTo(ctx, m_pos.x() + half_width, m_pos.y() + m_size.y() - 4);
  nvgStrokeWidth(ctx, 1.0f);
  nvgStrokeColor(ctx, theme->separator());
  nvgStroke(ctx);

  // Draw minus icon
  nvgBeginPath(ctx);
  float minus_y = m_pos.y() + m_size.y() * 0.5f;
  nvgMoveTo(ctx, m_pos.x() + 10, minus_y);
  nvgLineTo(ctx, m_pos.x() + half_width - 10, minus_y);
  nvgStrokeWidth(ctx, 2.0f);
  nvgStrokeColor(ctx, m_value > m_min_value ? theme->accent()
                                            : theme->quaternary_label());
  nvgStroke(ctx);

  // Draw plus icon
  float plus_x = m_pos.x() + half_width + half_width * 0.5f;
  float plus_y = m_pos.y() + m_size.y() * 0.5f;

  // Horizontal line
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, plus_x - 8, plus_y);
  nvgLineTo(ctx, plus_x + 8, plus_y);
  nvgStrokeWidth(ctx, 2.0f);
  nvgStrokeColor(ctx, m_value < m_max_value ? theme->accent()
                                            : theme->quaternary_label());
  nvgStroke(ctx);

  // Vertical line
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, plus_x, plus_y - 8);
  nvgLineTo(ctx, plus_x, plus_y + 8);
  nvgStroke(ctx);
}

Vector2i AppleStepper::preferred_size_impl(NVGcontext *ctx) const {
  return Vector2i(94, 29);
}

NAMESPACE_END(nanogui)
