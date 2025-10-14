#include <nanogui/fluent_web_rating.h>

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

FluentWebRating::FluentWebRating(Widget *parent, int max_rating)
    : Widget(parent), m_rating(0), m_max_rating(max_rating), m_hover_rating(-1),
      m_editable(true) {
  set_cursor(Cursor::Hand);
  refresh_tokens();
}

void FluentWebRating::set_rating(int rating) {
  rating = std::max(0, std::min(rating, m_max_rating));
  if (m_rating == rating)
    return;
  m_rating = rating;
  if (m_callback)
    m_callback(m_rating);
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebRating::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_tokens();
}

void FluentWebRating::refresh_tokens() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());

  m_filled_color = themed_color(fluent, FluentWebTheme::ColorToken::colorPaletteYellowForeground1,
                                Color(0.95f, 0.7f, 0.1f, 1.f));
  m_empty_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground3,
                               Color(0.6f, 0.6f, 0.6f, 1.f));
  m_hover_color = themed_color(fluent, FluentWebTheme::ColorToken::colorPaletteYellowForeground2,
                               Color(0.98f, 0.8f, 0.3f, 1.f));

  m_star_size = round_spacing(fluent, FluentWebTheme::SpaceToken::XL, 24);
  m_star_spacing = round_spacing(fluent, FluentWebTheme::SpaceToken::SNudge, 4);

  preferred_size_changed();
}

Vector2i FluentWebRating::preferred_size_impl(NVGcontext *) const {
  int width = m_max_rating * m_star_size + (m_max_rating - 1) * m_star_spacing;
  return Vector2i(width, m_star_size);
}

void FluentWebRating::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y());

  nvgFontFace(ctx, "icons");
  nvgFontSize(ctx, static_cast<float>(m_star_size));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

  int display_rating = m_editable && m_hover_rating >= 0 ? m_hover_rating : m_rating;

  for (int i = 0; i < m_max_rating; ++i) {
    Color color = (i < display_rating) ? m_filled_color : m_empty_color;
    if (m_editable && m_hover_rating >= 0 && i < m_hover_rating)
      color = m_hover_color;

    nvgFillColor(ctx, nvgRGBAf(color.r(), color.g(), color.b(), color.w()));
    const char *star = (i < display_rating) ? "★" : "☆";
    nvgText(ctx, x, y, star, nullptr);
    x += static_cast<float>(m_star_size + m_star_spacing);
  }
}

int FluentWebRating::star_at_position(const Vector2i &p) const {
  int local_x = p.x() - m_pos.x();
  if (local_x < 0)
    return -1;
  int star = local_x / (m_star_size + m_star_spacing);
  return (star >= 0 && star < m_max_rating) ? star + 1 : -1;
}

bool FluentWebRating::mouse_button_event(const Vector2i &p, int button, bool down,
                                         int modifiers) {
#if !defined(NANOGUI_USE_OPENGL)
  (void)p; (void)button; (void)down; (void)modifiers;
  return Widget::mouse_button_event(p, button, down, modifiers);
#else
  if (Widget::mouse_button_event(p, button, down, modifiers))
    return true;

  if (!m_editable || button != GLFW_MOUSE_BUTTON_1 || down)
    return false;

  int star = star_at_position(p);
  if (star >= 0) {
    set_rating(star);
    return true;
  }
  return false;
#endif
}

bool FluentWebRating::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button,
                                         int modifiers) {
  if (Widget::mouse_motion_event(p, rel, button, modifiers))
    return true;

  if (!m_editable)
    return false;

  int old_hover = m_hover_rating;
  m_hover_rating = star_at_position(p);
  if (old_hover != m_hover_rating) {
    if (Screen *scr = screen())
      scr->redraw();
  }
  return false;
}

NAMESPACE_END(nanogui)
