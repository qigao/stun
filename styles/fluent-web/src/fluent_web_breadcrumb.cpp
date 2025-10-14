#include <nanogui/fluent_web_breadcrumb.h>

#include <nanogui/fluent_web_theme.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <cmath>

#if defined(NANOGUI_USE_OPENGL)
  #include <GLFW/glfw3.h>
#endif

NAMESPACE_BEGIN(nanogui)

namespace {

int round_spacing(const FluentWebTheme *theme, FluentWebTheme::SpaceToken token, int fallback) {
  if (!theme)
    return fallback;
  return static_cast<int>(std::round(theme->spacing(token)));
}

Color themed_color(const FluentWebTheme *theme, FluentWebTheme::ColorToken token,
                   const Color &fallback) {
  return theme ? theme->color(token) : fallback;
}

} // namespace

FluentWebBreadcrumb::FluentWebBreadcrumb(Widget *parent)
    : Widget(parent), m_hover_index(-1) {
  set_cursor(Cursor::Hand);
  refresh_tokens();
}

void FluentWebBreadcrumb::add_item(const std::string &label, int icon, bool clickable) {
  m_items.push_back({label, icon, clickable});
  preferred_size_changed();
}

void FluentWebBreadcrumb::set_items(const std::vector<Item> &items) {
  m_items = items;
  preferred_size_changed();
}

void FluentWebBreadcrumb::clear_items() {
  m_items.clear();
  preferred_size_changed();
}

void FluentWebBreadcrumb::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_tokens();
}

void FluentWebBreadcrumb::refresh_tokens() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());

  m_text_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground2,
                              Color(0.38f, 0.38f, 0.38f, 1.f));
  m_hover_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground1,
                               Color(0.12f, 0.12f, 0.12f, 1.f));
  m_disabled_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForegroundDisabled,
                                  Color(0.6f, 0.6f, 0.6f, 1.f));
  m_separator_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground3,
                                   Color(0.45f, 0.45f, 0.45f, 1.f));

  m_item_spacing = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);
  m_icon_size = round_spacing(fluent, FluentWebTheme::SpaceToken::L, 16);
  m_font_size = 14.f;

  preferred_size_changed();
}

Vector2i FluentWebBreadcrumb::preferred_size_impl(NVGcontext *ctx) const {
  if (m_items.empty())
    return Vector2i(0, static_cast<int>(m_font_size) + 8);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, m_font_size);

  float total_width = 0.f;
  for (size_t i = 0; i < m_items.size(); ++i) {
    if (m_items[i].icon)
      total_width += static_cast<float>(m_icon_size + m_item_spacing / 2);

    float bounds[4];
    nvgTextBounds(ctx, 0, 0, m_items[i].label.c_str(), nullptr, bounds);
    total_width += bounds[2] - bounds[0];

    if (i < m_items.size() - 1)
      total_width += static_cast<float>(m_item_spacing * 2 + 10);
  }

  return Vector2i(static_cast<int>(total_width), static_cast<int>(m_font_size) + 8);
}

void FluentWebBreadcrumb::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y()) + static_cast<float>(m_size.y()) * 0.5f;

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, m_font_size);
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  for (size_t i = 0; i < m_items.size(); ++i) {
    const auto &item = m_items[i];
    bool is_last = (i == m_items.size() - 1);
    bool hovered = static_cast<int>(i) == m_hover_index && item.clickable;

    Color color = is_last ? m_hover_color : (item.clickable ? m_text_color : m_disabled_color);
    if (hovered)
      color = m_hover_color;

    if (item.icon) {
      nvgFontFace(ctx, "icons");
      nvgFontSize(ctx, static_cast<float>(m_icon_size));
      nvgFillColor(ctx, nvgRGBAf(color.r(), color.g(), color.b(), color.w()));
      nvgText(ctx, x, y, utf8(item.icon).data(), nullptr);
      x += static_cast<float>(m_icon_size + m_item_spacing / 2);
      nvgFontFace(ctx, "sans");
      nvgFontSize(ctx, m_font_size);
    }

    nvgFillColor(ctx, nvgRGBAf(color.r(), color.g(), color.b(), color.w()));
    float bounds[4];
    nvgTextBounds(ctx, x, y, item.label.c_str(), nullptr, bounds);
    nvgText(ctx, x, y, item.label.c_str(), nullptr);
    x = bounds[2] + static_cast<float>(m_item_spacing);

    if (!is_last) {
      nvgFillColor(ctx, nvgRGBAf(m_separator_color.r(), m_separator_color.g(),
                                 m_separator_color.b(), m_separator_color.w()));
      nvgText(ctx, x, y, "/", nullptr);
      x += 10.f + static_cast<float>(m_item_spacing);
    }
  }
}

int FluentWebBreadcrumb::item_at_position(const Vector2i &p) const {
  // Simplified hit testing
  (void)p;
  return -1;
}

bool FluentWebBreadcrumb::mouse_button_event(const Vector2i &p, int button, bool down,
                                             int modifiers) {
#if !defined(NANOGUI_USE_OPENGL)
  (void)p; (void)button; (void)down; (void)modifiers;
  return Widget::mouse_button_event(p, button, down, modifiers);
#else
  if (Widget::mouse_button_event(p, button, down, modifiers))
    return true;

  if (button == GLFW_MOUSE_BUTTON_1 && !down && m_hover_index >= 0 &&
      m_hover_index < static_cast<int>(m_items.size()) && m_items[m_hover_index].clickable) {
    if (m_item_callback)
      m_item_callback(m_hover_index);
    return true;
  }
  return false;
#endif
}

bool FluentWebBreadcrumb::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button,
                                             int modifiers) {
  if (Widget::mouse_motion_event(p, rel, button, modifiers))
    return true;

  int old_hover = m_hover_index;
  m_hover_index = item_at_position(p);
  if (old_hover != m_hover_index) {
    if (Screen *scr = screen())
      scr->redraw();
  }
  return false;
}

NAMESPACE_END(nanogui)
