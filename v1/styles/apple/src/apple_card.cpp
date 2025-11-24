/*
    src/apple_card.cpp -- Apple HIG card implementation
*/

#include <nanogui/apple_card.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleCard::AppleCard(Widget *parent, const std::string &title)
    : Widget(parent), m_title(title) {
  set_layout(new GroupLayout(8));
}

AppleTheme *AppleCard::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleCard::draw(NVGcontext *ctx) {
  AppleTheme *theme = apple_theme();
  if (!theme) {
    Widget::draw(ctx);
    return;
  }

  float corner_radius = theme->corner_radius(AppleTheme::CornerStyle::Large);

  // Draw shadow
  NVGpaint shadow = nvgBoxGradient(ctx, m_pos.x(), m_pos.y() + 2, m_size.x(),
                                   m_size.y(), corner_radius, 8,
                                   nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 0));
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x() - 8, m_pos.y() - 8, m_size.x() + 16, m_size.y() + 16);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), corner_radius);
  nvgPathWinding(ctx, NVG_HOLE);
  nvgFillPaint(ctx, shadow);
  nvgFill(ctx);

  // Draw card background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), corner_radius);
  nvgFillColor(ctx, theme->secondary_system_background());
  nvgFill(ctx);

  // Draw border
  nvgStrokeWidth(ctx, 1.0f);
  nvgStrokeColor(ctx, theme->separator());
  nvgStroke(ctx);

  // Draw title if present
  if (!m_title.empty()) {
    nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Headline));
    nvgFontFace(ctx, "sans-bold");
    nvgFillColor(ctx, theme->label());
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgText(ctx, m_pos.x() + 16, m_pos.y() + 16, m_title.c_str(), nullptr);
  }

  Widget::draw(ctx);
}

Vector2i AppleCard::preferred_size_impl(NVGcontext *ctx) const {
  Vector2i size = Widget::preferred_size_impl(ctx);
  return Vector2i(std::max(size.x(), 200), std::max(size.y(), 100));
}

NAMESPACE_END(nanogui)
