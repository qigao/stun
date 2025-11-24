#include <nanogui/fluent_web_tooltip.h>

#include <nanogui/fluent_web_theme.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>
#include <cmath>

NAMESPACE_BEGIN(nanogui)

namespace {

int round_spacing(const FluentWebTheme *theme,
                  FluentWebTheme::SpaceToken token, int fallback) {
  if (!theme)
    return fallback;
  return static_cast<int>(std::round(theme->spacing(token)));
}

Color themed_color(const FluentWebTheme *theme, FluentWebTheme::ColorToken token,
                   const Color &fallback) {
  return theme ? theme->color(token) : fallback;
}

} // namespace

FluentWebPopover::FluentWebPopover(Widget *parent, Window *parent_window)
    : Popup(parent, parent_window), m_background(Color(1.f, 1.f, 1.f, 1.f)),
      m_border(Color(0.f, 0.f, 0.f, 0.1f)) {
  set_layout(new GroupLayout());
}

void FluentWebPopover::set_theme(Theme *theme) {
  Popup::set_theme(theme);
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme);
  refresh_tokens(fluent);
  update_spacing(fluent);
}

void FluentWebPopover::refresh_tokens(const FluentWebTheme *fluent) {
  if (!fluent) {
    m_background = Color(1.f, 1.f, 1.f, 1.f);
    m_border = Color(0.f, 0.f, 0.f, 0.1f);
    return;
  }
  m_background = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground1,
                              Color(1.f, 1.f, 1.f, 1.f));
  m_border = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralStroke2,
                          Color(0.f, 0.f, 0.f, 0.25f));
}

void FluentWebPopover::update_spacing(const FluentWebTheme *fluent) {
  int margin = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);
  int spacing = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);

  if (auto *group = dynamic_cast<GroupLayout *>(layout())) {
    group->set_margin(margin);
    group->set_spacing(spacing);
    group->set_group_indent(0);
  }
}

void FluentWebPopover::draw(NVGcontext *ctx) {
  refresh_relative_placement();

  if (!m_visible)
    return;

  int ds = m_theme->m_window_drop_shadow_size;
  int cr = m_theme->m_window_corner_radius;

  nvgSave(ctx);
  nvgResetScissor(ctx);

  NVGcolor shadow = m_theme->m_drop_shadow;
  NVGcolor transparent = m_theme->m_transparent;

  NVGpaint shadow_paint = nvgBoxGradient(
      ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), cr * 2, ds * 2, shadow,
      transparent);

  nvgBeginPath(ctx);
  nvgRect(ctx, m_pos.x() - ds, m_pos.y() - ds, m_size.x() + 2 * ds,
          m_size.y() + 2 * ds);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), cr);
  nvgPathWinding(ctx, NVG_HOLE);
  nvgFillPaint(ctx, shadow_paint);
  nvgFill(ctx);

  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, m_pos.x(), m_pos.y(), m_size.x(), m_size.y(), cr);
  NVGcolor bg = nvgRGBAf(m_background.r(), m_background.g(), m_background.b(), m_background.w());
  nvgFillColor(ctx, bg);
  nvgFill(ctx);

  if (m_border.w() > 0.f) {
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, nvgRGBAf(m_border.r(), m_border.g(), m_border.b(), m_border.w()));
    nvgStroke(ctx);
  }

  nvgRestore(ctx);
  Widget::draw(ctx);
}

FluentWebTooltip::FluentWebTooltip(Widget *parent, Window *parent_window,
                                   const std::string &text)
    : FluentWebPopover(parent, parent_window), m_label(nullptr), m_text(text) {
  set_visible(false);
  set_anchor_size(0);
  set_anchor_offset(0);
  m_label = new Label(this, text, "sans", 14);
}

void FluentWebTooltip::set_text(const std::string &text) {
  m_text = text;
  if (m_label)
    m_label->set_caption(text);
}

FluentWebTooltip *FluentWebTooltip::show(Widget *anchor, const std::string &text) {
  if (!anchor)
    return nullptr;
  Screen *screen = anchor->screen();
  if (!screen)
    return nullptr;
  Window *anchor_window = anchor->window();
  auto *tooltip = new FluentWebTooltip(screen, anchor_window, text);
  tooltip->set_theme(anchor->theme());
  tooltip->configure_anchor_for(anchor);
  tooltip->set_visible(true);
  screen->move_window_to_front(tooltip);
  return tooltip;
}

void FluentWebTooltip::dismiss() {
  if (visible())
    set_visible(false);
  dispose();
}

void FluentWebTooltip::configure_anchor_for(Widget *anchor) {
  Window *anchor_window = anchor->window();
  Vector2i anchor_pos;
  if (anchor_window) {
    anchor_pos = anchor->absolute_position() - anchor_window->position();
  } else {
    anchor_pos = anchor->absolute_position();
  }
  anchor_pos.x() += anchor->width() / 2;
  anchor_pos.y() += anchor->height();
  set_anchor_pos(anchor_pos);
  set_side(Popup::Right);
  set_anchor_size(0);
  set_position(anchor->absolute_position() + Vector2i(anchor->width() / 2, anchor->height() + 4));
  if (Screen *scr = anchor->screen())
    scr->perform_layout();
  refresh_relative_placement();
}

NAMESPACE_END(nanogui)
