/*
    src/apple_alert.cpp -- Apple HIG alert implementation
*/

#include <nanogui/apple_alert.h>
#include <nanogui/layout.h>
#include <nanogui/label.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

NAMESPACE_BEGIN(nanogui)

// Helper to get screen dimensions
static inline Vector2i get_screen_size(Widget* widget) {
    Widget* w = widget;
    while (w->parent())
        w = w->parent();
#if defined(NANOGUI_BUILD_GLFW)
    Screen* screen = static_cast<Screen*>(w);
    return Vector2i(screen->width(), screen->height());
#elif defined(NANOGUI_USE_SDL3)
    Screen* screen = static_cast<Screen*>(w);
    return Vector2i(screen->width(), screen->height());
#elif defined(NANOGUI_USE_SDL2)
    Screen* screen = static_cast<Screen*>(w);
    return Vector2i(screen->width(), screen->height());
#else
    (void)w;
    return Vector2i(800, 600); // Default fallback
#endif
}

AppleAlert::AppleAlert(Widget *parent, const std::string &title,
                       const std::string &message, Style style)
    : Window(parent, title), m_style(style), m_message(message) {
  
  set_modal(true);
  set_fixed_width(270); // Standard iOS alert width

  // Content container
  m_content_container = new Widget(this);
  m_content_container->set_layout(new GroupLayout(16));

  // Message label
  if (!message.empty()) {
    auto msg_label = new Label(m_content_container, message, "sans", 13);
    msg_label->set_fixed_width(238); // 270 - 32 padding
  }

  // Button container
  m_button_container = new Widget(this);
  m_button_container->set_layout(
      new BoxLayout(Orientation::Vertical, Alignment::Fill, 0, 8));
}

AppleTheme *AppleAlert::apple_theme() const {
  return dynamic_cast<AppleTheme *>(const_cast<Theme *>(m_theme.get()));
}

void AppleAlert::set_message(const std::string &message) {
  m_message = message;
}

void AppleAlert::add_button(const std::string &label,
                            const std::function<void()> &callback,
                            AppleButton::Style style) {
  auto button = new AppleButton(m_button_container, label, 0, style);
  button->set_fixed_height(44);
  button->set_callback([this, callback]() {
    if (callback)
      callback();
    dismiss();
  });
}

void AppleAlert::show() {
  set_visible(true);
  request_focus();
  center();
}

void AppleAlert::dismiss() {
  set_visible(false);
  if (parent())
    parent()->request_focus();
}

void AppleAlert::draw(NVGcontext *ctx) {
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

  // Draw alert background
  float corner_radius = theme->corner_radius(AppleTheme::CornerStyle::Large);
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), corner_radius);
  nvgFillColor(ctx, theme->secondary_system_background());
  nvgFill(ctx);

  // Draw shadow
  NVGpaint shadow = nvgBoxGradient(ctx, m_pos.x(), m_pos.y() + 4, m_size.x(),
                                   m_size.y(), corner_radius, 16,
                                   nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0));
  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x() - 16, m_pos.y() - 16, m_size.x() + 32, m_size.y() + 32);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), corner_radius);
  nvgPathWinding(ctx, NVG_HOLE);
  nvgFillPaint(ctx, shadow);
  nvgFill(ctx);

  // Draw title
  if (!m_title.empty()) {
    nvgFontSize(ctx, theme->font_size(AppleTheme::TextStyle::Headline));
    nvgFontFace(ctx, "sans-bold");
    nvgFillColor(ctx, theme->label());
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgText(ctx, m_pos.x() + m_size.x() * 0.5f, m_pos.y() + 20,
            m_title.c_str(), nullptr);
  }

  Widget::draw(ctx);
}

Vector2i AppleAlert::preferred_size_impl(NVGcontext *ctx) const {
  Vector2i size = Window::preferred_size_impl(ctx);
  return Vector2i(270, std::max(size.y(), 150));
}

NAMESPACE_END(nanogui)
