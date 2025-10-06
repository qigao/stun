/*
    src/apple_popover.cpp -- Apple HIG popover implementation
*/

#include <nanogui/apple_popover.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

ApplePopover::ApplePopover(Widget *parent, Widget *anchor)
    : Popup(parent, nullptr), m_arrow_position(ArrowPosition::Top) {
    // Store anchor for positioning
    set_anchor_pos(anchor->absolute_position());
}

AppleTheme *ApplePopover::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void ApplePopover::draw_arrow(NVGcontext *ctx, float x, float y) {
  AppleTheme *theme = apple_theme();
  if (!theme)
    return;

  float arrow_size = 12.0f;

  nvgBeginPath(ctx);
  
  switch (m_arrow_position) {
    case ArrowPosition::Top:
      nvgMoveTo(ctx, x, y);
      nvgLineTo(ctx, x - arrow_size, y + arrow_size);
      nvgLineTo(ctx, x + arrow_size, y + arrow_size);
      break;
    case ArrowPosition::Bottom:
      nvgMoveTo(ctx, x, y);
      nvgLineTo(ctx, x - arrow_size, y - arrow_size);
      nvgLineTo(ctx, x + arrow_size, y - arrow_size);
      break;
    case ArrowPosition::Left:
      nvgMoveTo(ctx, x, y);
      nvgLineTo(ctx, x + arrow_size, y - arrow_size);
      nvgLineTo(ctx, x + arrow_size, y + arrow_size);
      break;
    case ArrowPosition::Right:
      nvgMoveTo(ctx, x, y);
      nvgLineTo(ctx, x - arrow_size, y - arrow_size);
      nvgLineTo(ctx, x - arrow_size, y + arrow_size);
      break;
    case ArrowPosition::None:
      return;
  }
  
  nvgClosePath(ctx);
  nvgFillColor(ctx, theme->secondary_system_background());
  nvgFill(ctx);
}

void ApplePopover::draw(NVGcontext *ctx) {
  AppleTheme *theme = apple_theme();
  if (!theme) {
    Popup::draw(ctx);
    return;
  }

  float corner_radius = theme->corner_radius(AppleTheme::CornerStyle::Large);

  // Draw shadow
  NVGpaint shadow = nvgBoxGradient(ctx, m_pos.x(), m_pos.y() + 4, m_size.x(),
                                   m_size.y(), corner_radius, 16,
                                   nvgRGBA(0, 0, 0, 48), nvgRGBA(0, 0, 0, 0));
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x() - 16, m_pos.y() - 16, m_size.x() + 32, m_size.y() + 32);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), corner_radius);
  nvgPathWinding(ctx, NVG_HOLE);
  nvgFillPaint(ctx, shadow);
  nvgFill(ctx);

  // Draw popover background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), corner_radius);
  nvgFillColor(ctx, theme->secondary_system_background());
  nvgFill(ctx);

  // Draw border
  nvgStrokeWidth(ctx, 1.0f);
  nvgStrokeColor(ctx, theme->separator());
  nvgStroke(ctx);

  // Draw arrow
  float arrow_x = m_pos.x() + m_size.x() * 0.5f;
  float arrow_y = m_pos.y();
  
  switch (m_arrow_position) {
    case ArrowPosition::Top:
      arrow_y = m_pos.y();
      break;
    case ArrowPosition::Bottom:
      arrow_y = m_pos.y() + m_size.y();
      break;
    case ArrowPosition::Left:
      arrow_x = m_pos.x();
      arrow_y = m_pos.y() + m_size.y() * 0.5f;
      break;
    case ArrowPosition::Right:
      arrow_x = m_pos.x() + m_size.x();
      arrow_y = m_pos.y() + m_size.y() * 0.5f;
      break;
    case ArrowPosition::None:
      break;
  }
  
  if (m_arrow_position != ArrowPosition::None) {
    draw_arrow(ctx, arrow_x, arrow_y);
  }

  Widget::draw(ctx);
}

Vector2i ApplePopover::preferred_size_impl(NVGcontext *ctx) const {
  Vector2i size = Popup::preferred_size_impl(ctx);
  return Vector2i(std::max(size.x(), 200), std::max(size.y(), 100));
}

NAMESPACE_END(nanogui)
