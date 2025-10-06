/*
    src/apple_toggle.cpp -- Apple HIG toggle implementation
*/

#include <nanogui/apple_toggle.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleToggle::AppleToggle(Widget *parent, const std::string &caption)
    : Widget(parent), m_pushed(false), m_caption(caption) {}

bool AppleToggle::mouse_button_event(const Vector2i &p, int button, bool down,
                                      int modifiers) {
  Widget::mouse_button_event(p, button, down, modifiers);
  if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
    return false;
  if (down) {
    m_pushed = !m_pushed;
    if (m_callback)
      m_callback(m_pushed);
  }
  return true;
}

void AppleToggle::draw(NVGcontext *ctx) {
  Widget::draw(ctx);
  // Minimal implementation
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), 51, 31, 15.5f);
  nvgFillColor(ctx, m_pushed ? nvgRGBA(52, 199, 89, 255)
                             : nvgRGBA(120, 120, 128, 91));
  nvgFill(ctx);
}

Vector2i AppleToggle::preferred_size_impl(NVGcontext *ctx) const {
  return Vector2i(51, 31);
}

NAMESPACE_END(nanogui)
