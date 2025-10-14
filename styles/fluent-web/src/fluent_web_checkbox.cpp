#include <nanogui/fluent_web_checkbox.h>

#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>

NAMESPACE_BEGIN(nanogui)

namespace {

struct BoxState {
  Color background;
  Color border;
  Color glyph;
};

struct CheckboxPalette {
  BoxState unchecked_normal;
  BoxState unchecked_hover;
  BoxState unchecked_pressed;
  BoxState checked_normal;
  BoxState checked_hover;
  BoxState checked_pressed;
  BoxState disabled_unchecked;
  BoxState disabled_checked;
  Color label_enabled;
  Color label_disabled;
  Color focus_inner;
  Color focus_outer;
};

CheckboxPalette make_palette(const FluentWebTheme &theme) {
  CheckboxPalette palette{};

  auto c = [&](FluentWebTheme::ColorToken token) { return theme.color(token); };

  palette.unchecked_normal = {c(FluentWebTheme::ColorToken::colorSubtleBackground),
                              c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessible),
                              c(FluentWebTheme::ColorToken::colorNeutralForeground1)};
  palette.unchecked_hover = {c(FluentWebTheme::ColorToken::colorSubtleBackgroundHover),
                             c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessibleHover),
                             c(FluentWebTheme::ColorToken::colorNeutralForeground1)};
  palette.unchecked_pressed = {c(FluentWebTheme::ColorToken::colorSubtleBackgroundPressed),
                               c(FluentWebTheme::ColorToken::colorNeutralStrokeAccessiblePressed),
                               c(FluentWebTheme::ColorToken::colorNeutralForeground1)};

  const Color compound_bg =
      c(FluentWebTheme::ColorToken::colorCompoundBrandBackground);
  const Color compound_bg_hover =
      c(FluentWebTheme::ColorToken::colorCompoundBrandBackgroundHover);
  const Color compound_bg_pressed =
      c(FluentWebTheme::ColorToken::colorCompoundBrandBackgroundPressed);
  const Color compound_glyph =
      c(FluentWebTheme::ColorToken::colorNeutralForegroundInverted);

  palette.checked_normal = {compound_bg,
                            compound_bg,
                            compound_glyph};
  palette.checked_hover = {compound_bg_hover,
                           compound_bg_hover,
                           compound_glyph};
  palette.checked_pressed = {compound_bg_pressed,
                             compound_bg_pressed,
                             compound_glyph};

  palette.disabled_unchecked = {c(FluentWebTheme::ColorToken::colorNeutralBackgroundDisabled),
                                c(FluentWebTheme::ColorToken::colorNeutralStrokeDisabled),
                                c(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled)};
  palette.disabled_checked = palette.disabled_unchecked;

  palette.label_enabled = c(FluentWebTheme::ColorToken::colorNeutralForeground1);
  palette.label_disabled = c(FluentWebTheme::ColorToken::colorNeutralForegroundDisabled);
  palette.focus_inner = c(FluentWebTheme::ColorToken::colorStrokeFocus2);
  palette.focus_outer = c(FluentWebTheme::ColorToken::colorStrokeFocus1);

  return palette;
}

NVGcolor to_nvg(const Color &c) { return nvgRGBAf(c.r(), c.g(), c.b(), c.w()); }

} // namespace

FluentWebCheckbox::FluentWebCheckbox(Widget *parent, const std::string &caption,
                                     const std::function<void(bool)> &callback)
    : CheckBox(parent, caption, callback) {
  update_metrics();
}

void FluentWebCheckbox::set_theme(Theme *theme) {
  CheckBox::set_theme(theme);
  update_metrics();
  preferred_size_changed();
}

void FluentWebCheckbox::update_metrics() {
  const FluentWebTheme *fluent = dynamic_cast<const FluentWebTheme *>(this->theme());
  if (!fluent)
    return;

  const auto &body = fluent->typography(FluentWebTheme::TypographyToken::Body1);
  m_typography_size = static_cast<int>(std::round(body.font_size));
  m_font_size = m_typography_size;
  m_box_size = fluent->spacing(FluentWebTheme::SpaceToken::L);
  m_label_spacing = fluent->spacing(FluentWebTheme::SpaceToken::S);
}

Vector2i FluentWebCheckbox::preferred_size_impl(NVGcontext *ctx) const {
  const auto *fluent = dynamic_cast<const FluentWebTheme *>(this->theme());
  if (!fluent || m_typography_size <= 0)
    return CheckBox::preferred_size_impl(ctx);

  nvgFontSize(ctx, static_cast<float>(m_typography_size));
  nvgFontFace(ctx, "sans");
  float text_width = nvgTextBounds(ctx, 0.f, 0.f, m_caption.c_str(), nullptr, nullptr);

  int width = static_cast<int>(std::round(m_box_size + m_label_spacing + text_width));

  float vertical_padding = fluent->spacing(FluentWebTheme::SpaceToken::XS);
  int height = static_cast<int>(std::round(
      std::max(m_box_size, static_cast<float>(m_typography_size)) + 2.f * vertical_padding));

  return Vector2i(width, height);
}

void FluentWebCheckbox::draw(NVGcontext *ctx) {
  auto *fluent = dynamic_cast<FluentWebTheme *>(this->theme());
  if (!fluent) {
    CheckBox::draw(ctx);
    return;
  }

  Widget::draw(ctx);

  const CheckboxPalette palette = make_palette(*fluent);

  const bool disabled = !m_enabled;
  const bool hovered = m_mouse_focus && m_enabled;
  const bool pressed = m_pushed && m_enabled;

  const BoxState *box_state = nullptr;
  if (disabled) {
    box_state = m_checked ? &palette.disabled_checked : &palette.disabled_unchecked;
  } else if (m_checked) {
    box_state = pressed ? &palette.checked_pressed
                        : (hovered ? &palette.checked_hover : &palette.checked_normal);
  } else {
    box_state = pressed ? &palette.unchecked_pressed
                        : (hovered ? &palette.unchecked_hover : &palette.unchecked_normal);
  }

  const float box_size = m_box_size;
  const float box_x = static_cast<float>(m_pos.x());
  const float box_y = static_cast<float>(m_pos.y()) + (m_size.y() - box_size) * 0.5f;
  const float radius = fluent->corner_radius(FluentWebTheme::RadiusToken::Small);

  if (m_focused && !disabled) {
    nvgSave(ctx);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, box_x - 3.f, box_y - 3.f, box_size + 6.f, box_size + 6.f, radius + 3.f);
    nvgStrokeWidth(ctx, 2.0f);
    nvgStrokeColor(ctx, to_nvg(palette.focus_outer));
    nvgStroke(ctx);

    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, box_x - 1.5f, box_y - 1.5f, box_size + 3.f, box_size + 3.f, radius + 2.f);
    nvgStrokeWidth(ctx, 1.3f);
    nvgStrokeColor(ctx, to_nvg(palette.focus_inner));
    nvgStroke(ctx);
    nvgRestore(ctx);
  }

  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, box_x, box_y, box_size, box_size, radius);
  if (box_state->background.w() > 0.f) {
    nvgFillColor(ctx, to_nvg(box_state->background));
    nvgFill(ctx);
  }

  if (box_state->border.w() > 0.f) {
    nvgStrokeWidth(ctx, 1.0f);
    nvgStrokeColor(ctx, to_nvg(box_state->border));
    nvgStroke(ctx);
  }

  if (m_checked) {
    nvgBeginPath(ctx);
    nvgFontFace(ctx, "icons");
    float icon_size = box_size * 0.80f;
    nvgFontSize(ctx, icon_size);
    NVGcolor glyph_color = to_nvg(box_state->glyph);
    nvgFillColor(ctx, glyph_color);
    nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(ctx, box_x + box_size * 0.5f, box_y + box_size * 0.5f + 1.f,
            utf8(m_theme->m_check_box_icon).data(), nullptr);
  }

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, static_cast<float>(m_typography_size > 0 ? m_typography_size : m_font_size));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  Color label_color = disabled ? palette.label_disabled : palette.label_enabled;
  nvgFillColor(ctx, to_nvg(label_color));

  float text_x = box_x + box_size + m_label_spacing;
  float text_y = static_cast<float>(m_pos.y()) + m_size.y() * 0.5f;
  nvgText(ctx, text_x, text_y, m_caption.c_str(), nullptr);
}

NAMESPACE_END(nanogui)
