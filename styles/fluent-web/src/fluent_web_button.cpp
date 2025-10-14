#include <nanogui/fluent_web_button.h>

#include <nanogui/icons.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>
#include <nanovg.h>

#include <algorithm>
#include <cmath>

NAMESPACE_BEGIN(nanogui)

namespace {

struct VisualState {
  Color background;
  Color border;
  Color text;
};

struct VariantPalette {
  VisualState normal;
  VisualState hover;
  VisualState pressed;
  VisualState disabled;
};

Color with_alpha(const Color &c, float alpha) { return Color(c.r(), c.g(), c.b(), alpha); }

VariantPalette make_variant_palette(const FluentWebTheme &theme,
                                    FluentWebButton::Appearance appearance) {
  auto color = [&](FluentWebTheme::ColorToken token) -> Color { return theme.color(token); };

  const Color disabled_bg = color(FluentWebTheme::ColorToken::colorNeutralBackgroundDisabled);
  const Color disabled_border = color(FluentWebTheme::ColorToken::colorNeutralStrokeDisabled);
  const Color disabled_text = color(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled);

  VariantPalette palette{};

  switch (appearance) {
  case FluentWebButton::Appearance::Primary: {
    palette.normal = {color(FluentWebTheme::ColorToken::colorBrandBackground),
                      color(FluentWebTheme::ColorToken::colorTransparentStroke),
                      color(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand)};
    palette.hover = {color(FluentWebTheme::ColorToken::colorBrandBackgroundHover),
                     color(FluentWebTheme::ColorToken::colorTransparentStroke),
                     color(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand)};
    palette.pressed = {color(FluentWebTheme::ColorToken::colorBrandBackgroundPressed),
                       color(FluentWebTheme::ColorToken::colorTransparentStroke),
                       color(FluentWebTheme::ColorToken::colorNeutralForegroundOnBrand)};
    break;
  }
  case FluentWebButton::Appearance::Secondary: {
    palette.normal = {color(FluentWebTheme::ColorToken::colorNeutralBackground1),
                      color(FluentWebTheme::ColorToken::colorNeutralStroke1),
                      color(FluentWebTheme::ColorToken::colorNeutralForeground1)};
    palette.hover = {color(FluentWebTheme::ColorToken::colorNeutralBackground1Hover),
                     color(FluentWebTheme::ColorToken::colorNeutralStroke1Hover),
                     color(FluentWebTheme::ColorToken::colorNeutralForeground1)};
    palette.pressed = {color(FluentWebTheme::ColorToken::colorNeutralBackground1Pressed),
                       color(FluentWebTheme::ColorToken::colorNeutralStroke1Pressed),
                       color(FluentWebTheme::ColorToken::colorNeutralForeground1)};
    break;
  }
  case FluentWebButton::Appearance::Outline: {
    palette.normal = {color(FluentWebTheme::ColorToken::colorTransparentBackground),
                      color(FluentWebTheme::ColorToken::colorNeutralStrokeAccessible),
                      color(FluentWebTheme::ColorToken::colorNeutralForeground1)};
    palette.hover = {color(FluentWebTheme::ColorToken::colorSubtleBackgroundHover),
                     color(FluentWebTheme::ColorToken::colorNeutralStrokeAccessibleHover),
                     color(FluentWebTheme::ColorToken::colorNeutralForeground1)};
    palette.pressed = {color(FluentWebTheme::ColorToken::colorSubtleBackgroundPressed),
                       color(FluentWebTheme::ColorToken::colorNeutralStrokeAccessiblePressed),
                       color(FluentWebTheme::ColorToken::colorNeutralForeground1)};
    break;
  }
  case FluentWebButton::Appearance::Subtle: {
    palette.normal = {color(FluentWebTheme::ColorToken::colorSubtleBackground),
                      with_alpha(color(FluentWebTheme::ColorToken::colorSubtleBackground), 0.f),
                      color(FluentWebTheme::ColorToken::colorNeutralForeground2)};
    palette.hover = {color(FluentWebTheme::ColorToken::colorSubtleBackgroundHover),
                     with_alpha(color(FluentWebTheme::ColorToken::colorSubtleBackgroundHover), 0.f),
                     color(FluentWebTheme::ColorToken::colorNeutralForeground2)};
    palette.pressed = {
        color(FluentWebTheme::ColorToken::colorSubtleBackgroundPressed),
        with_alpha(color(FluentWebTheme::ColorToken::colorSubtleBackgroundPressed), 0.f),
        color(FluentWebTheme::ColorToken::colorNeutralForeground2)};
    break;
  }
  case FluentWebButton::Appearance::Transparent: {
    palette.normal = {
        color(FluentWebTheme::ColorToken::colorTransparentBackground),
        with_alpha(color(FluentWebTheme::ColorToken::colorTransparentBackground), 0.f),
        color(FluentWebTheme::ColorToken::colorNeutralForeground1)};
    palette.hover = {
        color(FluentWebTheme::ColorToken::colorTransparentBackgroundHover),
        with_alpha(color(FluentWebTheme::ColorToken::colorTransparentBackgroundHover), 0.f),
        color(FluentWebTheme::ColorToken::colorNeutralForeground1)};
    palette.pressed = {
        color(FluentWebTheme::ColorToken::colorTransparentBackgroundPressed),
        with_alpha(color(FluentWebTheme::ColorToken::colorTransparentBackgroundPressed), 0.f),
        color(FluentWebTheme::ColorToken::colorNeutralForeground1)};
    break;
  }
  case FluentWebButton::Appearance::Danger: {
    palette.normal = {color(FluentWebTheme::ColorToken::colorStatusDangerBackground3),
                      color(FluentWebTheme::ColorToken::colorStatusDangerBorder2),
                      color(FluentWebTheme::ColorToken::colorStatusDangerForeground3)};
    palette.hover = {color(FluentWebTheme::ColorToken::colorStatusDangerBackground3Hover),
                     color(FluentWebTheme::ColorToken::colorStatusDangerBorder2),
                     color(FluentWebTheme::ColorToken::colorStatusDangerForeground3)};
    palette.pressed = {color(FluentWebTheme::ColorToken::colorStatusDangerBackground3Pressed),
                       color(FluentWebTheme::ColorToken::colorStatusDangerBorder2),
                       color(FluentWebTheme::ColorToken::colorStatusDangerForeground3)};
    break;
  }
  }

  palette.disabled = {disabled_bg, disabled_border, disabled_text};
  return palette;
}

NVGcolor to_nvg(const Color &c) { return nvgRGBAf(c.r(), c.g(), c.b(), c.w()); }

Vector2f text_position(const Vector2f &center, float text_width) {
  return {center.x() - text_width * 0.5f, center.y() - 1.f};
}

} // namespace

FluentWebButton::FluentWebButton(Widget *parent, const std::string &caption, Appearance appearance)
    : Button(parent, caption), m_appearance(appearance), m_compact(false) {
  update_metrics();
}

void FluentWebButton::set_appearance(Appearance appearance) {
  if (appearance == m_appearance)
    return;
  m_appearance = appearance;
  if (auto *scr = screen())
    scr->redraw();
}

void FluentWebButton::set_compact(bool compact) {
  if (m_compact == compact)
    return;
  m_compact = compact;
  update_metrics();
  preferred_size_changed();
  if (auto *scr = screen())
    scr->redraw();
}

void FluentWebButton::set_theme(Theme *theme) {
  Button::set_theme(theme);
  update_metrics();
}

void FluentWebButton::update_metrics() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());
  if (!fluent)
    return;

  const float horizontal_spacing =
      fluent->spacing(m_compact ? FluentWebTheme::SpaceToken::S : FluentWebTheme::SpaceToken::L);
  const float vertical_spacing =
      fluent->spacing(m_compact ? FluentWebTheme::SpaceToken::XS : FluentWebTheme::SpaceToken::S);

  set_padding(Vector2i(static_cast<int>(std::round(horizontal_spacing)),
                       static_cast<int>(std::round(vertical_spacing))));

  if (m_font_size == -1) {
    const auto &body = fluent->typography(FluentWebTheme::TypographyToken::Body1);
    m_font_size = static_cast<int>(std::round(body.font_size));
  }
}

Vector2i FluentWebButton::preferred_size_impl(NVGcontext *ctx) const {
  return Button::preferred_size_impl(ctx);
}

void FluentWebButton::draw(NVGcontext *ctx) {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());
  if (!fluent) {
    Button::draw(ctx);
    return;
  }

  Widget::draw(ctx);

  const VariantPalette palette = make_variant_palette(*fluent, m_appearance);

  const bool disabled = !m_enabled;
  const bool hovered = m_mouse_focus && m_enabled;
  const bool pressed = (m_pushed && m_enabled);

  const VisualState &state =
      disabled ? palette.disabled
               : (pressed ? palette.pressed : (hovered ? palette.hover : palette.normal));

  Color background = state.background;
  Color border = state.border;
  Color text = state.text;

  if (m_background_color.w() > 0.f)
    background = m_background_color;
  if (m_text_color.w() > 0.f)
    text = m_text_color;

  const float radius = fluent->corner_radius(FluentWebTheme::RadiusToken::Large);

  const float x = static_cast<float>(m_pos.x());
  const float y = static_cast<float>(m_pos.y());
  const float w = static_cast<float>(m_size.x());
  const float h = static_cast<float>(m_size.y());

  // Draw button background first
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, radius);
  if (background.w() > 0.f) {
    nvgFillColor(ctx, to_nvg(background));
    nvgFill(ctx);
  }

  if (border.w() > 0.f) {
    nvgStrokeWidth(ctx, 1.0f);
    nvgStrokeColor(ctx, to_nvg(border));
    nvgStroke(ctx);
  }

  // Draw focus ring on top
  if (m_focused && m_enabled) {
    Color focus_outer = fluent->color(FluentWebTheme::ColorToken::colorStrokeFocus1);
    Color focus_inner = fluent->color(FluentWebTheme::ColorToken::colorStrokeFocus2);

    nvgSave(ctx);
    nvgResetScissor(ctx); // Ensure focus ring isn't clipped

    // Outer focus ring
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x - 2.5f, y - 2.5f, w + 5.f, h + 5.f, radius + 2.5f);
    nvgStrokeWidth(ctx, 2.5f);
    nvgStrokeColor(ctx, to_nvg(focus_outer));
    nvgStroke(ctx);

    // Inner focus ring
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x - 0.75f, y - 0.75f, w + 1.5f, h + 1.5f, radius + 0.75f);
    nvgStrokeWidth(ctx, 1.5f);
    nvgStrokeColor(ctx, to_nvg(focus_inner));
    nvgStroke(ctx);

    nvgRestore(ctx);
  }

  const int font_size = m_font_size == -1 ? m_theme->m_button_font_size : m_font_size;
  nvgFontSize(ctx, static_cast<float>(font_size));
  nvgFontFace(ctx, "sans-bold");
  float tw = nvgTextBounds(ctx, 0.f, 0.f, m_caption.c_str(), nullptr, nullptr);

  float icon_width = 0.f;
  float icon_height = static_cast<float>(font_size);
  bool has_icon = m_icon != 0;

  if (has_icon) {
    if (nvg_is_font_icon(m_icon)) {
      icon_height *= icon_scale();
      nvgFontFace(ctx, "icons");
      nvgFontSize(ctx, icon_height);
      icon_width = nvgTextBounds(ctx, 0.f, 0.f, utf8(m_icon).data(), nullptr, nullptr);
    } else {
      int iw, ih;
      nvgImageSize(ctx, m_icon, &iw, &ih);
      icon_width = iw * icon_height / ih;
    }
    if (!m_caption.empty())
      icon_width += font_size * 0.25f;
  }

  Vector2f center = Vector2f(m_pos) + Vector2f(m_size) * 0.5f;
  Vector2f label_pos = text_position(center, tw + icon_width);
  NVGcolor text_color = to_nvg(text);

  if (has_icon) {
    Vector2f icon_pos = center;
    icon_pos.y() -= 1.f;

    if (m_icon_position == IconPosition::LeftCentered) {
      icon_pos.x() -= (tw + icon_width) * 0.5f;
      label_pos.x() += icon_width * 0.5f;
    } else if (m_icon_position == IconPosition::RightCentered) {
      label_pos.x() -= icon_width * 0.5f;
      icon_pos.x() += tw * 0.5f;
    } else if (m_icon_position == IconPosition::Left) {
      icon_pos.x() = x + m_padding.x() - font_size * 0.25f;
      label_pos.x() = icon_pos.x() + icon_width;
    } else if (m_icon_position == IconPosition::Right) {
      icon_pos.x() = x + w - icon_width - m_padding.x() + font_size * 0.25f;
    }

    nvgFillColor(ctx, text_color);
    if (nvg_is_font_icon(m_icon)) {
      nvgFontFace(ctx, "icons");
      nvgFontSize(ctx, icon_height);
      nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgText(ctx, icon_pos.x(), icon_pos.y() + 1.f, utf8(m_icon).data(), nullptr);
    } else {
      NVGpaint img_paint =
          nvgImagePattern(ctx, icon_pos.x(), icon_pos.y() - icon_height / 2.f, icon_width,
                          icon_height, 0.f, m_icon, disabled ? 0.35f : 0.8f);
      nvgFillPaint(ctx, img_paint);
      nvgFill(ctx);
    }
  }

  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, static_cast<float>(font_size));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgFillColor(ctx, text_color);
  nvgText(ctx, label_pos.x(), label_pos.y() + 1.f, m_caption.c_str(), nullptr);
}

NAMESPACE_END(nanogui)
