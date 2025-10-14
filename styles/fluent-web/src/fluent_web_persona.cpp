#include <nanogui/fluent_web_persona.h>

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

FluentWebPersona::FluentWebPersona(Widget *parent, const std::string &name,
                                   const std::string &secondary)
    : Widget(parent), m_name(name), m_secondary_text(secondary), m_persona_size(Size::Medium),
      m_presence(Presence::None), m_clickable(false), m_hovered(false) {
  refresh_tokens();
}

void FluentWebPersona::set_name(const std::string &name) {
  if (m_name == name)
    return;
  m_name = name;
  preferred_size_changed();
}

void FluentWebPersona::set_secondary_text(const std::string &text) {
  if (m_secondary_text == text)
    return;
  m_secondary_text = text;
  preferred_size_changed();
}

void FluentWebPersona::set_initials(const std::string &initials) {
  if (m_initials == initials)
    return;
  m_initials = initials;
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebPersona::set_size(Size size) {
  if (m_persona_size == size)
    return;
  m_persona_size = size;
  preferred_size_changed();
}

void FluentWebPersona::set_presence(Presence presence) {
  if (m_presence == presence)
    return;
  m_presence = presence;
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebPersona::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_tokens();
}

void FluentWebPersona::refresh_tokens() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());

  m_avatar_background = themed_color(fluent, FluentWebTheme::ColorToken::colorBrandBackground,
                                     Color(0.2f, 0.4f, 0.85f, 1.f));
  m_avatar_text = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand,
                               Color(1.f, 1.f, 1.f, 1.f));
  m_name_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground1,
                              Color(0.12f, 0.12f, 0.12f, 1.f));
  m_secondary_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground2,
                                   Color(0.38f, 0.38f, 0.38f, 1.f));
  m_hover_background = themed_color(fluent, FluentWebTheme::ColorToken::colorSubtleBackgroundHover,
                                    Color(0.94f, 0.94f, 0.94f, 1.f));

  m_presence_available = themed_color(fluent, FluentWebTheme::ColorToken::colorPaletteGreenForeground1,
                                      Color(0.1f, 0.7f, 0.3f, 1.f));
  m_presence_busy = themed_color(fluent, FluentWebTheme::ColorToken::colorPaletteRedForeground1,
                                 Color(0.85f, 0.2f, 0.2f, 1.f));
  m_presence_away = themed_color(fluent, FluentWebTheme::ColorToken::colorPaletteYellowForeground1,
                                 Color(0.95f, 0.7f, 0.1f, 1.f));
  m_presence_offline = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground3,
                                    Color(0.6f, 0.6f, 0.6f, 1.f));
  m_presence_dnd = m_presence_busy;

  m_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);
  m_spacing = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);
  m_name_font_size = 14.f;
  m_secondary_font_size = 12.f;

  preferred_size_changed();
}

Color FluentWebPersona::presence_color() const {
  switch (m_presence) {
    case Presence::Available: return m_presence_available;
    case Presence::Busy: return m_presence_busy;
    case Presence::Away: return m_presence_away;
    case Presence::Offline: return m_presence_offline;
    case Presence::DoNotDisturb: return m_presence_dnd;
    default: return Color(0.f, 0.f, 0.f, 0.f);
  }
}

int FluentWebPersona::avatar_size() const {
  switch (m_persona_size) {
    case Size::Small: return 24;
    case Size::Large: return 56;
    default: return 40;
  }
}

Vector2i FluentWebPersona::preferred_size_impl(NVGcontext *ctx) const {
  int av_size = avatar_size();
  int height = av_size + m_padding * 2;

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, m_name_font_size);
  float bounds[4];
  nvgTextBounds(ctx, 0, 0, m_name.c_str(), nullptr, bounds);
  int text_width = static_cast<int>(bounds[2] - bounds[0]);

  int width = m_padding * 2 + av_size + m_spacing + text_width + m_padding;
  return Vector2i(width, height);
}

void FluentWebPersona::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y());
  float w = static_cast<float>(Widget::m_size.x());
  float h = static_cast<float>(Widget::m_size.y());

  // Hover background
  if (m_clickable && m_hovered && m_hover_background.w() > 0.f) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, 8.f);
    nvgFillColor(ctx, nvgRGBAf(m_hover_background.r(), m_hover_background.g(),
                               m_hover_background.b(), m_hover_background.w()));
    nvgFill(ctx);
  }

  float av_size = static_cast<float>(avatar_size());
  float av_x = x + static_cast<float>(m_padding);
  float av_y = y + h * 0.5f;

  // Avatar circle
  nvgBeginPath(ctx);
  nvgCircle(ctx, av_x + av_size * 0.5f, av_y, av_size * 0.5f);
  nvgFillColor(ctx, nvgRGBAf(m_avatar_background.r(), m_avatar_background.g(),
                             m_avatar_background.b(), m_avatar_background.w()));
  nvgFill(ctx);

  // Avatar initials
  if (!m_initials.empty()) {
    nvgFontFace(ctx, "sans-bold");
    nvgFontSize(ctx, av_size * 0.4f);
    nvgFillColor(ctx, nvgRGBAf(m_avatar_text.r(), m_avatar_text.g(), m_avatar_text.b(),
                               m_avatar_text.w()));
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, av_x + av_size * 0.5f, av_y, m_initials.c_str(), nullptr);
  }

  // Presence indicator
  if (m_presence != Presence::None) {
    float presence_size = av_size * 0.3f;
    float presence_x = av_x + av_size - presence_size * 0.5f;
    float presence_y = av_y + av_size * 0.35f;

    Color pres_color = presence_color();
    nvgBeginPath(ctx);
    nvgCircle(ctx, presence_x, presence_y, presence_size * 0.5f);
    nvgFillColor(ctx, nvgRGBAf(pres_color.r(), pres_color.g(), pres_color.b(), pres_color.w()));
    nvgFill(ctx);

    // White border around presence
    nvgBeginPath(ctx);
    nvgCircle(ctx, presence_x, presence_y, presence_size * 0.5f);
    nvgStrokeWidth(ctx, 2.f);
    nvgStrokeColor(ctx, nvgRGBA(255, 255, 255, 255));
    nvgStroke(ctx);
  }

  // Name and secondary text
  float text_x = av_x + av_size + static_cast<float>(m_spacing);
  float text_y = av_y;

  if (!m_secondary_text.empty())
    text_y -= m_name_font_size * 0.3f;

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, m_name_font_size);
  nvgFillColor(ctx, nvgRGBAf(m_name_color.r(), m_name_color.g(), m_name_color.b(),
                             m_name_color.w()));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(ctx, text_x, text_y, m_name.c_str(), nullptr);

  if (!m_secondary_text.empty()) {
    nvgFontSize(ctx, m_secondary_font_size);
    nvgFillColor(ctx, nvgRGBAf(m_secondary_color.r(), m_secondary_color.g(),
                               m_secondary_color.b(), m_secondary_color.w()));
    nvgText(ctx, text_x, text_y + m_name_font_size * 0.8f, m_secondary_text.c_str(), nullptr);
  }
}

bool FluentWebPersona::mouse_button_event(const Vector2i &p, int button, bool down,
                                          int modifiers) {
#if !defined(NANOGUI_USE_OPENGL)
  (void)p; (void)button; (void)down; (void)modifiers;
  return Widget::mouse_button_event(p, button, down, modifiers);
#else
  if (Widget::mouse_button_event(p, button, down, modifiers))
    return true;

  if (m_clickable && button == GLFW_MOUSE_BUTTON_1 && !down) {
    if (m_callback)
      m_callback();
    return true;
  }
  return false;
#endif
}

bool FluentWebPersona::mouse_enter_event(const Vector2i &p, bool enter) {
  m_hovered = enter;
  if (Screen *scr = screen())
    scr->redraw();
  return Widget::mouse_enter_event(p, enter);
}

NAMESPACE_END(nanogui)
