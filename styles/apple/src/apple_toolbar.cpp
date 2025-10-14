/*
    src/apple_toolbar.cpp -- Apple HIG toolbar implementation
*/

#include <nanogui/apple_toolbar.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleToolbar::AppleToolbar(Widget *parent, Position position)
    : Widget(parent), m_position(position) {
  set_fixed_height(52);
  set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 8, 8));
}

AppleTheme *AppleToolbar::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleToolbar::add_button(const std::string &label, int icon,
                              const std::function<void()> &callback) {
  auto button = new AppleButton(this, label, icon, AppleButton::Style::Tertiary);
  button->set_callback(callback);
}

void AppleToolbar::add_spacer() {
  auto spacer = new Widget(this);
  spacer->set_fixed_width(16);
}

void AppleToolbar::add_flexible_space() {
  auto spacer = new Widget(this);
  // This will expand to fill available space
}

void AppleToolbar::draw(NVGcontext *ctx) {
  AppleTheme *theme = apple_theme();
  if (!theme) {
    Widget::draw(ctx);
    return;
  }

  // Draw background
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
  nvgFillColor(ctx, theme->secondary_system_background());
  nvgFill(ctx);

  // Draw border
  nvgBeginPath(ctx);
  if (m_position == Position::Top) {
    nvgMoveTo(ctx, m_pos.x(), m_pos.y() + m_size.y());
    nvgLineTo(ctx, m_pos.x() + m_size.x(), m_pos.y() + m_size.y());
  } else {
    nvgMoveTo(ctx, m_pos.x(), m_pos.y());
    nvgLineTo(ctx, m_pos.x() + m_size.x(), m_pos.y());
  }
  nvgStrokeWidth(ctx, 0.5f);
  nvgStrokeColor(ctx, theme->separator());
  nvgStroke(ctx);

  Widget::draw(ctx);
}

Vector2i AppleToolbar::preferred_size_impl(NVGcontext *ctx) const {
  return Vector2i(m_size.x(), 52);
}

NAMESPACE_END(nanogui)
