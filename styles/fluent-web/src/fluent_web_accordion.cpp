#include <nanogui/fluent_web_accordion.h>

#include <nanogui/fluent_icons.h>
#include <nanogui/fluent_web_theme.h>
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

NVGcolor to_nvg(const Color &c) {
  return nvgRGBAf(c.r(), c.g(), c.b(), c.w());
}

} // namespace

class AccordionSectionBody : public Widget {
public:
  AccordionSectionBody(Widget *parent, FluentWebAccordion *owner)
      : Widget(parent), m_owner(owner) {
    set_layout(new GroupLayout());
    sync_from_owner();
  }

  void sync_from_owner() {
    int margin = 12;
    int spacing = 8;
    m_background = Color(1.f, 1.f, 1.f, 1.f);
    m_border = Color(0.85f, 0.85f, 0.85f, 1.f);

    if (m_owner) {
      margin = m_owner->m_body_padding;
      spacing = m_owner->m_body_spacing;
      m_background = m_owner->m_body_background;
      m_border = m_owner->m_body_border_color;
    }

    if (auto *group = dynamic_cast<GroupLayout *>(layout())) {
      group->set_margin(margin);
      group->set_spacing(spacing);
      group->set_group_spacing(spacing);
      group->set_group_indent(margin);
    }

    preferred_size_changed();
  }

  void draw(NVGcontext *ctx) override {
    const float x = static_cast<float>(m_pos.x());
    const float y = static_cast<float>(m_pos.y());
    const float w = static_cast<float>(m_size.x());
    const float h = static_cast<float>(m_size.y());

    if (w > 0.f && h > 0.f && m_background.w() > 0.f) {
      nvgBeginPath(ctx);
      nvgRect(ctx, x, y, w, h);
      nvgFillColor(ctx, to_nvg(m_background));
      nvgFill(ctx);
    }

    if (w > 0.f && m_border.w() > 0.f) {
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, x, y + 0.5f);
      nvgLineTo(ctx, x + w, y + 0.5f);
      nvgStrokeWidth(ctx, 1.f);
      nvgStrokeColor(ctx, to_nvg(m_border));
      nvgStroke(ctx);
    }

    Widget::draw(ctx);
  }

private:
  FluentWebAccordion *m_owner;
  Color m_background;
  Color m_border;
};

FluentWebAccordion::FluentWebAccordion(Widget *parent)
    : Widget(parent), m_single_expand(true), m_header_font_size(16.f),
      m_body_font_size(14.f), m_header_color(Color(0.94f, 0.94f, 0.94f, 1.f)),
      m_header_hover_color(Color(0.90f, 0.90f, 0.90f, 1.f)),
      m_header_pressed_color(Color(0.85f, 0.85f, 0.85f, 1.f)),
      m_header_foreground(Color(0.1f, 0.1f, 0.1f, 1.f)),
      m_body_background(Color(1.f, 1.f, 1.f, 1.f)),
      m_body_foreground(Color(0.2f, 0.2f, 0.2f, 1.f)),
      m_body_border_color(Color(0.8f, 0.8f, 0.8f, 1.f)), m_header_padding(12),
      m_section_spacing(8), m_body_padding(12), m_body_spacing(8) {
  set_layout(nullptr);
}

Widget *FluentWebAccordion::add_section(const std::string &title,
                                        bool expanded) {
  auto *header =
      new FluentWebButton(this, title, FluentWebButton::Appearance::Subtle);
  header->set_flags(Button::ToggleButton);
  header->set_icon(FLUENT_ICON_CHEVRON_DOWN);
  header->set_icon_position(Button::IconPosition::Right);
  header->set_pushed(expanded);

  auto *body = new AccordionSectionBody(this, this);
  body->set_visible(expanded);

  Section section{header, body, expanded};
  m_sections.emplace_back(section);

  const size_t index = m_sections.size() - 1;
  header->set_change_callback(
      [this, index](bool pushed) { expand_section(index, pushed); });

  update_section_chrome(m_sections.back());
  preferred_size_changed();

  if (auto *scr = screen())
    scr->perform_layout();

  return body;
}

void FluentWebAccordion::clear_sections() {
  for (auto &section : m_sections) {
    if (section.header_button) {
      section.header_button->set_visible(false);
      remove_child(section.header_button);
      section.header_button = nullptr;
    }
    if (section.body_container) {
      section.body_container->set_visible(false);
      remove_child(section.body_container);
      section.body_container = nullptr;
    }
  }
  m_sections.clear();
  preferred_size_changed();
  if (auto *scr = screen())
    scr->perform_layout();
}

void FluentWebAccordion::expand_section(size_t index, bool expand) {
  if (index >= m_sections.size())
    return;

  if (m_single_expand && expand) {
    for (size_t i = 0; i < m_sections.size(); ++i) {
      if (i == index)
        continue;
      auto &other = m_sections[i];
      other.expanded = false;
      if (other.header_button)
        other.header_button->set_pushed(false);
      if (other.body_container)
        other.body_container->set_visible(false);
      update_section_chrome(other);
    }
  }

  auto &section = m_sections[index];
  section.expanded = expand;
  if (section.header_button)
    section.header_button->set_pushed(expand);
  if (section.body_container)
    section.body_container->set_visible(expand);
  update_section_chrome(section);
  preferred_size_changed();

  if (auto *scr = screen())
    scr->perform_layout();
}

void FluentWebAccordion::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_tokens();
  update_spacing();
  for (auto &section : m_sections)
    update_section_chrome(section);
  preferred_size_changed();
}

void FluentWebAccordion::perform_layout(NVGcontext *ctx) {
  int resolved_width = m_size.x();
  if (resolved_width <= 0)
    resolved_width = preferred_size_impl(ctx).x();

  int y = 0;
  for (size_t i = 0; i < m_sections.size(); ++i) {
    auto &section = m_sections[i];
    auto *header = section.header_button;
    if (!header)
      continue;

    Vector2i header_pref = header->preferred_size(ctx);
    header->set_position(Vector2i(0, y));
    header->set_size(Vector2i(resolved_width, header_pref.y()));
    header->perform_layout(ctx);
    y += header_pref.y();

    if (section.expanded && section.body_container) {
      Vector2i body_pref = section.body_container->preferred_size(ctx);
      section.body_container->set_position(Vector2i(0, y));
      section.body_container->set_size(Vector2i(resolved_width, body_pref.y()));
      section.body_container->perform_layout(ctx);
      y += body_pref.y();
    } else if (section.body_container) {
      section.body_container->set_visible(false);
    }

    if (i + 1 < m_sections.size())
      y += m_section_spacing;
  }

  m_size.y() = y;
}

Vector2i FluentWebAccordion::preferred_size_impl(NVGcontext *ctx) const {
  int width = 0;
  int height = 0;

  for (size_t i = 0; i < m_sections.size(); ++i) {
    const Section &section = m_sections[i];
    if (!section.header_button)
      continue;

    Vector2i header_pref = section.header_button->preferred_size(ctx);
    width = std::max(width, header_pref.x());
    height += header_pref.y();

    if (section.expanded && section.body_container) {
      Vector2i body_pref = section.body_container->preferred_size(ctx);
      width = std::max(width, body_pref.x());
      height += body_pref.y();
    }

    if (i + 1 < m_sections.size())
      height += m_section_spacing;
  }

  return Vector2i(width, height);
}

bool FluentWebAccordion::mouse_button_event(const Vector2i &p, int button,
                                            bool down, int modifiers) {
  return Widget::mouse_button_event(p, button, down, modifiers);
}

void FluentWebAccordion::refresh_tokens() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());
  if (!fluent) {
    m_header_font_size = 16.f;
    m_body_font_size = 14.f;
    m_header_color = Color(0.94f, 0.94f, 0.94f, 1.f);
    m_header_hover_color = Color(0.90f, 0.90f, 0.90f, 1.f);
    m_header_pressed_color = Color(0.85f, 0.85f, 0.85f, 1.f);
    m_header_foreground = Color(0.1f, 0.1f, 0.1f, 1.f);
    m_body_background = Color(1.f, 1.f, 1.f, 1.f);
    m_body_foreground = Color(0.2f, 0.2f, 0.2f, 1.f);
    m_body_border_color = Color(0.8f, 0.8f, 0.8f, 1.f);
    return;
  }

  m_header_font_size =
      fluent->typography(FluentWebTheme::TypographyToken::Body2).font_size;
  m_body_font_size =
      fluent->typography(FluentWebTheme::TypographyToken::Body1).font_size;
  m_header_color = themed_color(
      fluent, FluentWebTheme::ColorToken::colorNeutralBackground3,
      Color(0.94f, 0.94f, 0.94f, 1.f));
  m_header_hover_color =
      themed_color(fluent,
                   FluentWebTheme::ColorToken::colorNeutralBackground3Hover,
                   Color(0.90f, 0.90f, 0.90f, 1.f));
  m_header_pressed_color =
      themed_color(fluent,
                   FluentWebTheme::ColorToken::colorNeutralBackground3Pressed,
                   Color(0.85f, 0.85f, 0.85f, 1.f));
  m_header_foreground = themed_color(
      fluent, FluentWebTheme::ColorToken::colorNeutralForeground1,
      Color(0.1f, 0.1f, 0.1f, 1.f));
  m_body_background = themed_color(
      fluent, FluentWebTheme::ColorToken::colorNeutralBackground1,
      Color(1.f, 1.f, 1.f, 1.f));
  m_body_foreground = themed_color(
      fluent, FluentWebTheme::ColorToken::colorNeutralForeground2,
      Color(0.2f, 0.2f, 0.2f, 1.f));
  m_body_border_color = themed_color(
      fluent, FluentWebTheme::ColorToken::colorNeutralStroke2,
      Color(0.8f, 0.8f, 0.8f, 1.f));
}

void FluentWebAccordion::update_spacing() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());
  m_header_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::M, 12);
  m_section_spacing = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);
  m_body_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::M, 12);
  m_body_spacing = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);
}

void FluentWebAccordion::update_section_chrome(Section &section) {
  auto *header = section.header_button;
  if (!header)
    return;

  const bool fluent_theme = dynamic_cast<FluentWebTheme *>(theme()) != nullptr;
  header->set_font_size(static_cast<int>(std::round(m_header_font_size)));
  header->set_text_color(m_header_foreground);
  header->set_icon(section.expanded ? FLUENT_ICON_CHEVRON_UP
                                    : FLUENT_ICON_CHEVRON_DOWN);
  header->set_icon_position(Button::IconPosition::Right);
  header->set_padding(Vector2i(m_header_padding, m_header_padding));

  if (fluent_theme) {
    header->set_background_color(Color(0.f, 0.f, 0.f, 0.f));
  } else {
    header->set_background_color(section.expanded ? m_header_pressed_color
                                                  : m_header_color);
  }

  if (auto *body = dynamic_cast<AccordionSectionBody *>(section.body_container)) {
    body->sync_from_owner();
    body->set_visible(section.expanded);
  } else if (section.body_container) {
    section.body_container->set_visible(section.expanded);
  }
}

NAMESPACE_END(nanogui)
