/*
    src/apple_text_field.cpp -- Apple HIG text field implementation
*/

#include <nanogui/apple_text_field.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleTextField::AppleTextField(Widget *parent, const std::string &value)
    : TextBox(parent, value) {
  set_fixed_height(32);
  set_editable(true);
  set_alignment(TextBox::Alignment::Left);
}

AppleTheme *AppleTextField::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleTextField::draw(NVGcontext *ctx) {
  AppleTheme *theme = apple_theme();
  if (!theme) {
    TextBox::draw(ctx);
    return;
  }

  float corner_radius = theme->corner_radius(AppleTheme::CornerStyle::Medium);

  // Draw background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), corner_radius);
  nvgFillColor(ctx, theme->system_fill());
  nvgFill(ctx);

  // Draw border
  if (m_focused) {
    nvgStrokeWidth(ctx, 2.0f);
    nvgStrokeColor(ctx, theme->accent());
  } else {
    nvgStrokeWidth(ctx, 1.0f);
    nvgStrokeColor(ctx, theme->separator());
  }
  nvgStroke(ctx);

  // Draw text content
  nvgSave(ctx);
  nvgIntersectScissor(ctx, m_pos.x() + 8, m_pos.y(), m_size.x() - 16, m_size.y());

  nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Body));
  nvgFontFace(ctx, "sans");
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  float text_x = m_pos.x() + 8;
  float text_y = m_pos.y() + m_size.y() * 0.5f;

  if (m_value.empty() && !m_focused) {
    // Draw placeholder
    nvgFillColor(ctx, theme->placeholder_text());
    nvgText(ctx, text_x, text_y, m_placeholder.c_str(), nullptr);
  } else {
    // Draw value
    nvgFillColor(ctx, m_enabled ? theme->label() : theme->tertiary_label());
    nvgText(ctx, text_x, text_y, m_value.c_str(), nullptr);

    // Draw cursor if focused
    if (m_focused) {
      float cursor_x = text_x;
      if (!m_value.empty()) {
        cursor_x += nvgTextBounds(ctx, 0, 0, m_value.c_str(), nullptr, nullptr);
      }
      
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, cursor_x, text_y - 8);
      nvgLineTo(ctx, cursor_x, text_y + 8);
      nvgStrokeWidth(ctx, 1.5f);
      nvgStrokeColor(ctx, theme->accent());
      nvgStroke(ctx);
    }
  }

  nvgRestore(ctx);
}

Vector2i AppleTextField::preferred_size_impl(NVGcontext *ctx) const {
  AppleTheme *theme = apple_theme();
  float font_size = theme ? theme->font_size(AppleTheme::TextStyle::Body) : 17.0f;

  nvgFontSize(ctx, font_size);
  nvgFontFace(ctx, "sans");

  float tw = 200.0f; // Default width
  if (!m_value.empty()) {
    tw = nvgTextBounds(ctx, 0, 0, m_value.c_str(), nullptr, nullptr) + 16;
  }

  return Vector2i(static_cast<int>(tw), 32);
}

NAMESPACE_END(nanogui)
