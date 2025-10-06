/*
    src/apple_checkbox.cpp -- Apple HIG checkbox implementation
*/

#include <nanogui/apple_checkbox.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleCheckbox::AppleCheckbox(Widget *parent, const std::string &caption,
                             const std::function<void(bool)> &callback)
    : Widget(parent), m_checked(false), m_caption(caption), m_callback(callback) {
  set_fixed_height(20);
}

AppleTheme *AppleCheckbox::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

bool AppleCheckbox::mouse_button_event(const Vector2i &p, int button, bool down,
                                        int modifiers) {
  Widget::mouse_button_event(p, button, down, modifiers);
  if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
    return false;
  
  if (down) {
    m_checked = !m_checked;
    if (m_callback)
      m_callback(m_checked);
  }
  return true;
}

void AppleCheckbox::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  AppleTheme *theme = apple_theme();
  if (!theme)
    return;

  float box_size = 16.0f;
  float x = m_pos.x();
  float y = m_pos.y() + (m_size.y() - box_size) * 0.5f;

  // Draw checkbox box
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, box_size, box_size, 3.0f);

  if (m_checked) {
    nvgFillColor(ctx, theme->accent());
  } else {
    nvgFillColor(ctx, theme->system_fill());
  }
  nvgFill(ctx);

  // Draw border
  nvgStrokeWidth(ctx, 1.0f);
  nvgStrokeColor(ctx, theme->separator());
  nvgStroke(ctx);

  // Draw checkmark if checked
  if (m_checked) {
    nvgStrokeWidth(ctx, 2.0f);
    nvgStrokeColor(ctx, nvgRGBA(255, 255, 255, 255));
    nvgLineCap(ctx, NVG_ROUND);
    nvgLineJoin(ctx, NVG_ROUND);

    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x + 4, y + 8);
    nvgLineTo(ctx, x + 7, y + 11);
    nvgLineTo(ctx, x + 12, y + 5);
    nvgStroke(ctx);
  }

  // Draw caption
  if (!m_caption.empty()) {
    nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Body));
    nvgFontFace(ctx, "sans");
    nvgFillColor(ctx, m_enabled ? theme->label() : theme->tertiary_label());
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, x + box_size + 8, m_pos.y() + m_size.y() * 0.5f,
            m_caption.c_str(), nullptr);
  }
}

Vector2i AppleCheckbox::preferred_size_impl(NVGcontext *ctx) const {
  AppleTheme *theme = apple_theme();
  float font_size = theme ? theme->font_size(AppleTheme::TextStyle::Body) : 17.0f;

  float w = 16.0f;
  if (!m_caption.empty()) {
    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, "sans");
    float tw = nvgTextBounds(ctx, 0, 0, m_caption.c_str(), nullptr, nullptr);
    w += 8.0f + tw;
  }

  return Vector2i(static_cast<int>(w), 20);
}

NAMESPACE_END(nanogui)
