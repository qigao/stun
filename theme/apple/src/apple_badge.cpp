/*
    src/apple_badge.cpp -- Apple HIG badge implementation
*/

#include <nanogui/apple_badge.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleBadge::AppleBadge(Widget *parent, const std::string &text)
    : Widget(parent), m_text(text), m_count(0) {}

AppleTheme *AppleBadge::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleBadge::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  AppleTheme *theme = apple_theme();
  if (!theme)
    return;

  std::string display_text = m_text;
  if (m_count > 0) {
    if (m_count > 99) {
      display_text = "99+";
    } else {
      display_text = std::to_string(m_count);
    }
  }

  if (display_text.empty())
    return;

  float font_size = theme->font_size(AppleTheme::TextStyle::Caption1);
  nvgFontSize(ctx, font_size);
  nvgFontFace(ctx, "sans-bold");

  float tw = nvgTextBounds(ctx, 0, 0, display_text.c_str(), nullptr, nullptr);
  float padding = 6.0f;
  float min_width = 18.0f;
  float width = std::max(min_width, tw + padding * 2);
  float height = 18.0f;
  float corner_radius = height * 0.5f;

  float x = m_pos.x() + (m_size.x() - width) * 0.5f;
  float y = m_pos.y() + (m_size.y() - height) * 0.5f;

  // Draw badge background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, width, height, corner_radius);
  nvgFillColor(ctx, theme->system_red());
  nvgFill(ctx);

  // Draw text
  nvgFillColor(ctx, nvgRGBA(255, 255, 255, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgText(ctx, x + width * 0.5f, y + height * 0.5f, display_text.c_str(), nullptr);
}

Vector2i AppleBadge::preferred_size_impl(NVGcontext *ctx) const {
  AppleTheme *theme = apple_theme();
  float font_size = theme ? theme->font_size(AppleTheme::TextStyle::Caption1) : 12.0f;

  std::string display_text = m_text;
  if (m_count > 0) {
    display_text = m_count > 99 ? "99+" : std::to_string(m_count);
  }

  if (display_text.empty())
    return Vector2i(0, 0);

  nvgFontSize(ctx, font_size);
  nvgFontFace(ctx, "sans-bold");

  float tw = nvgTextBounds(ctx, 0, 0, display_text.c_str(), nullptr, nullptr);
  float padding = 6.0f;
  float min_width = 18.0f;
  float width = std::max(min_width, tw + padding * 2);

  return Vector2i(static_cast<int>(width), 18);
}

NAMESPACE_END(nanogui)
