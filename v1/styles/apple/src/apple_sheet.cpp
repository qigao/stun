/*
    src/apple_sheet.cpp -- Apple HIG sheet implementation
*/

#include <nanogui/apple_sheet.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

NAMESPACE_BEGIN(nanogui)

// Helper to get screen dimensions
static inline Vector2i get_screen_size(Widget *widget) {
  Widget *w = widget;
  while (w->parent())
    w = w->parent();
#if defined(NANOGUI_BUILD_GLFW)
  Screen *screen = static_cast<Screen *>(w);
  return Vector2i(screen->width(), screen->height());
#elif defined(NANOGUI_USE_SDL3)
  Screen *screen = static_cast<Screen *>(w);
  return Vector2i(screen->width(), screen->height());
#elif defined(NANOGUI_USE_SDL2)
  Screen *screen = static_cast<Screen *>(w);
  return Vector2i(screen->width(), screen->height());
#else
  (void)w;
  return Vector2i(800, 600); // Default fallback
#endif
}

AppleSheet::AppleSheet(Widget *parent, const std::string &title, Style style)
    : Window(parent, title), m_style(style), m_animation_progress(0.0f) {
  set_modal(true);

  switch (style) {
  case Style::FormSheet:
    set_fixed_size(Vector2i(540, 620));
    break;
  case Style::FullScreen:
    // Will be sized to screen
    break;
  case Style::Standard:
  default:
    set_fixed_width(600);
    break;
  }
}

AppleTheme *AppleSheet::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleSheet::show() {
  set_visible(true);
  request_focus();
  center();
  m_animation_progress = 1.0f;
}

void AppleSheet::dismiss() {
  set_visible(false);
  if (parent())
    parent()->request_focus();
}

void AppleSheet::draw(NVGcontext *ctx) {
  AppleTheme *theme = apple_theme();
  if (!theme) {
    Window::draw(ctx);
    return;
  }

  // Draw backdrop
  if (m_modal) {
    Vector2i screen_size = get_screen_size(this);
    nvgBeginPath(ctx);
    nvgRect(ctx, 0, 0, screen_size.x(), screen_size.y());
    nvgFillColor(ctx, nvgRGBA(0, 0, 0, 128));
    nvgFill(ctx);
  }

  float corner_radius = theme->corner_radius(AppleTheme::CornerStyle::Large);

  // Draw shadow
  NVGpaint shadow = nvgBoxGradient(ctx, m_pos.x(), m_pos.y() + 8, m_size.x(), m_size.y(),
                                   corner_radius, 24, nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0));
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x() - 24, m_pos.y() - 24, m_size.x() + 48, m_size.y() + 48);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), corner_radius);
  nvgPathWinding(ctx, NVG_HOLE);
  nvgFillPaint(ctx, shadow);
  nvgFill(ctx);

  // Draw sheet background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), corner_radius);
  nvgFillColor(ctx, theme->system_background());
  nvgFill(ctx);

  // Draw title bar
  if (!m_title.empty()) {
    nvgBeginPath(ctx);
    nvgRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), 52);
    nvgFillColor(ctx, theme->secondary_system_background());
    nvgFill(ctx);

    // Draw title
    nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Headline));
    nvgFontFace(ctx, "sans-bold");
    nvgFillColor(ctx, theme->label());
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, m_pos.x() + m_size.x() * 0.5f, m_pos.y() + 26, m_title.c_str(), nullptr);

    // Draw separator
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, m_pos.x(), m_pos.y() + 52);
    nvgLineTo(ctx, m_pos.x() + m_size.x(), m_pos.y() + 52);
    nvgStrokeWidth(ctx, 0.5f);
    nvgStrokeColor(ctx, theme->separator());
    nvgStroke(ctx);
  }

  Widget::draw(ctx);
}

Vector2i AppleSheet::preferred_size_impl(NVGcontext *ctx) const {
  if (m_style == Style::FullScreen) {
    return get_screen_size(const_cast<AppleSheet *>(this));
  }

  Vector2i size = Window::preferred_size_impl(ctx);

  if (m_style == Style::FormSheet) {
    return Vector2i(540, 620);
  }

  return Vector2i(600, std::max(size.y(), 400));
}

NAMESPACE_END(nanogui)
