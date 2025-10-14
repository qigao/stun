#include <nanogui/fluent_web_dialog.h>

#include <nanogui/fluent_web_theme.h>
#include <nanogui/label.h>
#include <nanogui/layout.h>
#include <nanogui/screen.h>
#include <cmath>

NAMESPACE_BEGIN(nanogui)

namespace {

int round_spacing(const FluentWebTheme *theme,
                  FluentWebTheme::SpaceToken token, int fallback) {
  if (!theme)
    return fallback;
  return static_cast<int>(std::round(theme->spacing(token)));
}

Color get_color(const FluentWebTheme *theme,
                FluentWebTheme::ColorToken token,
                Color fallback) {
  return theme ? theme->color(token) : fallback;
}

} // namespace

FluentWebDialog::FluentWebDialog(Widget *parent, const std::string &title)
    : Window(parent, title),
      m_title_label(nullptr),
      m_subtitle_label(nullptr),
      m_body_label(nullptr),
      m_header(nullptr),
      m_content_container(nullptr),
      m_action_row(nullptr),
      m_dismissible(true) {
  set_modal(true);
  set_layout(new GroupLayout());

  m_header = new Widget(this);
  m_header->set_layout(new GroupLayout());

  m_title_label = new Label(m_header, title, "sans-bold", 20);
  m_subtitle_label = new Label(m_header, "", "sans", 14);
  m_subtitle_label->set_visible(false);

  m_content_container = new Widget(this);
  m_content_container->set_layout(new GroupLayout());

  m_action_row = new Widget(this);
  auto *actions_layout =
      new BoxLayout(Orientation::Horizontal, Alignment::Middle, 0, 12);
  actions_layout->set_margin(0);
  m_action_row->set_layout(actions_layout);

  refresh_chrome();
  update_metrics();
}

void FluentWebDialog::set_subtitle(const std::string &text) {
  m_subtitle = text;
  if (!m_subtitle.empty()) {
    m_subtitle_label->set_visible(true);
    m_subtitle_label->set_caption(m_subtitle);
  } else {
    m_subtitle_label->set_visible(false);
  }
  refresh_chrome();
}

void FluentWebDialog::set_dismissible(bool value) {
  m_dismissible = value;
}

void FluentWebDialog::clear_actions() {
  for (Widget *button : m_action_buttons) {
    if (button)
      m_action_row->remove_child(button);
  }
  m_action_buttons.clear();
  if (auto *scr = screen())
    scr->perform_layout();
}

FluentWebButton *FluentWebDialog::add_action(const std::string &caption,
                                             FluentWebButton::Appearance appearance,
                                             std::function<void()> callback) {
  auto *button = new FluentWebButton(m_action_row, caption, appearance);
  button->set_callback([this, callback = std::move(callback)] {
    if (callback)
      callback();
  });
  m_action_buttons.push_back(button);
  if (auto *scr = screen())
    scr->perform_layout();
  return button;
}

void FluentWebDialog::set_body_text(const std::string &text) {
  if (!m_body_label) {
    m_body_label = new Label(m_content_container, text, "sans", 16);
  } else {
    m_body_label->set_caption(text);
  }
  m_body_label->set_visible(!text.empty());
  refresh_chrome();
}

void FluentWebDialog::show() {
  if (!visible()) {
    set_visible(true);
    center();
    if (screen())
      screen()->move_window_to_front(this);
  }
  request_focus();
}

void FluentWebDialog::dismiss() {
  if (visible())
    set_visible(false);
  if (m_on_dismiss)
    m_on_dismiss();
  dispose();
}

void FluentWebDialog::set_theme(Theme *theme) {
  Window::set_theme(theme);
  refresh_chrome();
  update_metrics();
}

void FluentWebDialog::refresh_chrome() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());
  const Color title_color =
      get_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground1,
                Color(0.f, 0.f, 0.f, 1.f));
  const Color secondary_text =
      get_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground2,
                Color(0.2f, 0.2f, 0.2f, 1.f));

  if (m_title_label)
    m_title_label->set_color(title_color);
  if (m_subtitle_label)
    m_subtitle_label->set_color(secondary_text);
  if (m_body_label)
    m_body_label->set_color(secondary_text);
}

void FluentWebDialog::update_metrics() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());

  int margin = round_spacing(fluent, FluentWebTheme::SpaceToken::XL, 20);
  int spacing = round_spacing(fluent, FluentWebTheme::SpaceToken::M, 12);
  int indent = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);

  if (auto *layout = dynamic_cast<GroupLayout *>(this->layout())) {
    layout->set_margin(margin);
    layout->set_spacing(spacing);
    layout->set_group_indent(indent);
  }

  if (auto *header_layout = dynamic_cast<GroupLayout *>(m_header->layout())) {
    header_layout->set_margin(0);
    header_layout->set_spacing(spacing / 2);
    header_layout->set_group_indent(0);
  }

  if (auto *content_layout =
          dynamic_cast<GroupLayout *>(m_content_container->layout())) {
    content_layout->set_margin(0);
    content_layout->set_spacing(spacing);
    content_layout->set_group_indent(0);
  }

  if (auto *actions_layout = dynamic_cast<BoxLayout *>(m_action_row->layout())) {
    actions_layout->set_margin(0);
    actions_layout->set_spacing(spacing);
    actions_layout->set_alignment(Alignment::Maximum);
  }
}

NAMESPACE_END(nanogui)
