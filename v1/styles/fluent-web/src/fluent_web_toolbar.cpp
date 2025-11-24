#include <nanogui/fluent_web_toolbar.h>

#include <nanogui/fluent_web_theme.h>
#include <nanogui/layout.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <cmath>

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

FluentWebToolbar::FluentWebToolbar(Widget *parent)
    : Widget(parent), m_overflow_threshold(10) {
  set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, 8, 8));
  refresh_tokens();
}

void FluentWebToolbar::add_action(const std::string &label, int icon,
                                  const std::function<void()> &callback, bool enabled,
                                  bool primary) {
  m_actions.push_back({label, icon, callback, enabled, primary});
  rebuild_buttons();
}

void FluentWebToolbar::add_separator() {
  // Add a visual separator (can be implemented as a thin widget)
  rebuild_buttons();
}

void FluentWebToolbar::clear_actions() {
  m_actions.clear();
  for (auto *btn : m_buttons)
    remove_child(btn);
  m_buttons.clear();
  preferred_size_changed();
}

void FluentWebToolbar::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_tokens();
  rebuild_buttons();
}

void FluentWebToolbar::refresh_tokens() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());

  m_background = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground2,
                              Color(0.98f, 0.98f, 0.98f, 1.f));
  m_border_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralStroke2,
                                Color(0.f, 0.f, 0.f, 0.08f));

  m_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);
  m_spacing = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);
  m_corner_radius = round_spacing(fluent, FluentWebTheme::SpaceToken::MNudge, 6);

  if (auto *box = dynamic_cast<BoxLayout *>(layout())) {
    box->set_margin(m_padding);
    box->set_spacing(m_spacing);
  }

  preferred_size_changed();
}

void FluentWebToolbar::rebuild_buttons() {
  for (auto *btn : m_buttons)
    remove_child(btn);
  m_buttons.clear();

  for (const auto &action : m_actions) {
    auto appearance = action.primary ? FluentWebButton::Appearance::Primary 
                                     : FluentWebButton::Appearance::Secondary;
    auto *btn = new FluentWebButton(this, action.label, appearance);
    if (action.icon) {
      btn->Button::set_icon(action.icon);
      btn->Button::set_icon_position(Button::IconPosition::LeftCentered);
    }
    btn->set_enabled(action.enabled);
    btn->set_fixed_size({0, 0}); // Let button auto-size
    if (action.callback)
      btn->set_callback(action.callback);
    m_buttons.push_back(btn);
  }

  preferred_size_changed();
}

void FluentWebToolbar::perform_layout(NVGcontext *ctx) {
  Widget::perform_layout(ctx);
}

Vector2i FluentWebToolbar::preferred_size_impl(NVGcontext *ctx) const {
  return Widget::preferred_size_impl(ctx);
}

void FluentWebToolbar::draw(NVGcontext *ctx) {
  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y());
  float w = static_cast<float>(m_size.x());
  float h = static_cast<float>(m_size.y());

  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, static_cast<float>(m_corner_radius));
  nvgFillColor(ctx, nvgRGBAf(m_background.r(), m_background.g(), m_background.b(),
                             m_background.w()));
  nvgFill(ctx);

  if (m_border_color.w() > 0.f) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f,
                   std::max(0.f, static_cast<float>(m_corner_radius) - 0.5f));
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, nvgRGBAf(m_border_color.r(), m_border_color.g(), m_border_color.b(),
                                 m_border_color.w()));
    nvgStroke(ctx);
  }

  Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
