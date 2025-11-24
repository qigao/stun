/*
    src/apple_tab_bar.cpp -- Apple HIG tab bar implementation
*/

#include <nanogui/apple_tab_bar.h>
#include <nanogui/opengl.h>

NAMESPACE_BEGIN(nanogui)

AppleTabBar::AppleTabBar(Widget *parent)
    : Widget(parent), m_selected_index(0), m_hover_index(-1) {
  set_fixed_height(49); // Standard iOS tab bar height
}

AppleTheme *AppleTabBar::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleTabBar::add_tab(const std::string &label, int icon,
                          const std::function<void()> &callback) {
  Tab tab(label, icon, callback);
  m_tabs.push_back(tab);
}

void AppleTabBar::set_selected_index(int index) {
  if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
    m_selected_index = index;
    if (m_tabs[index].callback) {
      m_tabs[index].callback();
    }
  }
}

int AppleTabBar::tab_at_position(const Vector2i &p) const {
  if (m_tabs.empty())
    return -1;

  float tab_width = static_cast<float>(m_size.x()) / m_tabs.size();
  int index = static_cast<int>((p.x() - m_pos.x()) / tab_width);

  if (index >= 0 && index < static_cast<int>(m_tabs.size())) {
    return index;
  }
  return -1;
}

bool AppleTabBar::mouse_button_event(const Vector2i &p, int button, bool down,
                                      int modifiers) {
  Widget::mouse_button_event(p, button, down, modifiers);
  if (!m_enabled || button != GLFW_MOUSE_BUTTON_1)
    return false;

  if (down) {
    int index = tab_at_position(p);
    if (index >= 0) {
      set_selected_index(index);
    }
  }
  return true;
}

void AppleTabBar::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  AppleTheme *theme = apple_theme();
  if (!theme || m_tabs.empty())
    return;

  // Draw background
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y());
  nvgFillColor(ctx, theme->secondary_system_background());
  nvgFill(ctx);

  // Draw top border
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, m_pos.x(), m_pos.y());
  nvgLineTo(ctx, m_pos.x() + m_size.x(), m_pos.y());
  nvgStrokeWidth(ctx, 0.5f);
  nvgStrokeColor(ctx, theme->separator());
  nvgStroke(ctx);

  float tab_width = static_cast<float>(m_size.x()) / m_tabs.size();

  // Draw tabs
  for (size_t i = 0; i < m_tabs.size(); ++i) {
    float x = m_pos.x() + i * tab_width;
    bool is_selected = (static_cast<int>(i) == m_selected_index);
    bool is_hover = (static_cast<int>(i) == m_hover_index);

    Color icon_color = is_selected ? theme->accent() : theme->tertiary_label();
    Color text_color = is_selected ? theme->accent() : theme->tertiary_label();

    // Draw icon
    if (m_tabs[i].icon) {
      nvgFontSize(ctx, 24);
      nvgFontFace(ctx, "icons");
      nvgFillColor(ctx, icon_color);
      nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgText(ctx, x + tab_width * 0.5f, m_pos.y() + 16,
              utf8(m_tabs[i].icon).data(), nullptr);
    }

    // Draw label
    nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Caption2));
    nvgFontFace(ctx, "sans");
    nvgFillColor(ctx, text_color);
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, x + tab_width * 0.5f, m_pos.y() + 38,
            m_tabs[i].label.c_str(), nullptr);
  }
}

Vector2i AppleTabBar::preferred_size_impl(NVGcontext *ctx) const {
  return Vector2i(m_size.x(), 49);
}

NAMESPACE_END(nanogui)
