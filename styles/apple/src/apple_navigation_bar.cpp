/*
    src/apple_navigation_bar.cpp -- Apple HIG navigation bar implementation
*/

#include <nanogui/apple_navigation_bar.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleNavigationBar::AppleNavigationBar(Widget *parent, const std::string &title,
                                       Style style)
    : Widget(parent), m_title(title), m_style(style),
      m_left_button(nullptr), m_right_button(nullptr) {
  set_fixed_height(style == Style::Large ? 96 : 44);
}

AppleTheme *AppleNavigationBar::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleNavigationBar::set_left_button(const std::string &label, int icon,
                                         const std::function<void()> &callback) {
  if (m_left_button) {
    remove_child(m_left_button);
  }
  m_left_button = new AppleButton(this, label, icon, AppleButton::Style::Tertiary);
  m_left_button->set_callback(callback);
  m_left_button->set_position(Vector2i(8, 8));
}

void AppleNavigationBar::set_right_button(const std::string &label, int icon,
                                          const std::function<void()> &callback) {
  if (m_right_button) {
    remove_child(m_right_button);
  }
  m_right_button = new AppleButton(this, label, icon, AppleButton::Style::Tertiary);
  m_right_button->set_callback(callback);
}

void AppleNavigationBar::draw(NVGcontext *ctx) {
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

  // Draw bottom border
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, m_pos.x(), m_pos.y() + m_size.y());
  nvgLineTo(ctx, m_pos.x() + m_size.x(), m_pos.y() + m_size.y());
  nvgStrokeWidth(ctx, 0.5f);
  nvgStrokeColor(ctx, theme->separator());
  nvgStroke(ctx);

  // Position right button
  if (m_right_button) {
    Vector2i btn_size = m_right_button->preferred_size(ctx);
    m_right_button->set_position(
        Vector2i(m_size.x() - btn_size.x() - 8, 8));
  }

  // Draw title
  if (!m_title.empty()) {
    float title_y = m_pos.y() + (m_style == Style::Large ? 60 : 22);
    float font_size = theme->font_size(m_style == Style::Large
                                           ? AppleTheme::TextStyle::LargeTitle
                                           : AppleTheme::TextStyle::Headline);

    nvgFontSize(ctx, font_size);
    nvgFontFace(ctx, m_style == Style::Large ? "sans-bold" : "sans-bold");
    nvgFillColor(ctx, theme->label());
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, m_pos.x() + m_size.x() * 0.5f, title_y, m_title.c_str(),
            nullptr);
  }

  Widget::draw(ctx);
}

Vector2i AppleNavigationBar::preferred_size_impl(NVGcontext *ctx) const {
  return Vector2i(m_size.x(), m_style == Style::Large ? 96 : 44);
}

NAMESPACE_END(nanogui)
