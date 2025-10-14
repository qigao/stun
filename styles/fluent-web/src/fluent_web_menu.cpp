#include <nanogui/fluent_web_menu.h>

#include <nanogui/fluent_web_button.h>
#include <nanogui/fluent_web_theme.h>
#include <nanogui/icons.h>
#include <nanogui/layout.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#if defined(NANOGUI_USE_OPENGL)
#include <GLFW/glfw3.h>
#endif

#include <algorithm>
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

class FluentWebMenu::SeparatorWidget : public Widget {
public:
  SeparatorWidget(Widget *parent, Color color, float thickness, int inset)
      : Widget(parent), m_color(color), m_thickness(thickness), m_inset(inset) {
    set_fixed_height(static_cast<int>(std::ceil(thickness)) + inset * 2);
  }

  void set_style(Color color, float thickness, int inset) {
    m_color = color;
    m_thickness = thickness;
    m_inset = inset;
    set_fixed_height(static_cast<int>(std::ceil(thickness)) + inset * 2);
  }

  void draw(NVGcontext *ctx) override {
    if (m_color.w() <= 0.f)
      return;
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y() + m_inset);
    float w = static_cast<float>(m_size.x());
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, m_thickness);
    nvgFillColor(ctx, nvgRGBAf(m_color.r(), m_color.g(), m_color.b(), m_color.w()));
    nvgFill(ctx);
  }

private:
  Color m_color;
  float m_thickness;
  int m_inset;
};

class FluentWebMenu::MenuItemWidget : public Widget {
public:
  MenuItemWidget(FluentWebMenu *menu, size_t index)
      : Widget(menu), m_menu(menu), m_index(index), m_pressed(false) {
    set_cursor(Cursor::Hand);
  }

  void set_index(size_t index) { m_index = index; }

  bool mouse_enter_event(const Vector2i &p, bool enter) override {
    if (enter) {
      m_menu->set_highlight(static_cast<int>(m_index));
    }
    return true;
  }

  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override {
    if (button != GLFW_MOUSE_BUTTON_1)
      return false;
    if (down) {
      m_pressed = true;
      m_menu->set_highlight(static_cast<int>(m_index));
    } else if (m_pressed) {
      m_pressed = false;
      m_menu->activate(static_cast<int>(m_index));
    }
    return true;
  }

  Vector2i preferred_size_impl(NVGcontext *ctx) const override {
    const Item &item = m_menu->m_items[m_index];
    if (item.separator)
      return Vector2i(0);

    int width = m_menu->m_item_padding * 2;
    int height = m_menu->m_item_height;

    nvgFontFace(ctx, "sans");
    const auto &typo =
        m_menu->theme() ? static_cast<FluentWebTheme *>(m_menu->theme())
                              ->typography(FluentWebTheme::TypographyToken::Body1)
                        : FluentWebTheme::TypographySpec{14.f, 20.f, 400, 0.f};
    nvgFontSize(ctx, typo.font_size);
    float label_width = nvgTextBounds(ctx, 0.f, 0.f, item.label.c_str(), nullptr, nullptr);
    width += static_cast<int>(std::ceil(label_width));

    if (!item.shortcut.empty()) {
      nvgFontSize(ctx, typo.font_size - 1.f);
      float shortcut_width =
          nvgTextBounds(ctx, 0.f, 0.f, item.shortcut.c_str(), nullptr, nullptr);
      width += static_cast<int>(std::ceil(shortcut_width)) + m_menu->m_item_padding;
    }

    if (item.icon) {
      width += m_menu->m_item_height; // reserve square for icon + gap
    }

    return Vector2i(width, height);
  }

  void draw(NVGcontext *ctx) override {
    if (m_index >= m_menu->m_items.size())
      return;

    const Item &item = m_menu->m_items[m_index];
    if (item.separator)
      return;

    bool highlighted = (m_menu->m_highlight_index == static_cast<int>(m_index));
    float x = static_cast<float>(m_pos.x());
    float y = static_cast<float>(m_pos.y());
    float w = static_cast<float>(m_size.x());
    float h = static_cast<float>(m_size.y());

    if (highlighted && item.enabled) {
      const Color &bg = m_pressed ? m_menu->m_item_bg_active : m_menu->m_item_bg_hover;
      if (bg.w() > 0.f) {
        nvgBeginPath(ctx);
        nvgRect(ctx, x, y, w, h);
        nvgFillColor(ctx, nvgRGBAf(bg.r(), bg.g(), bg.b(), bg.w()));
        nvgFill(ctx);
      }
    }

    float label_x = x + static_cast<float>(m_menu->m_item_padding);
    float label_y = y + h * 0.5f;

    // Icon slot
    if (item.icon) {
      float icon_size = h * 0.6f;
      float icon_x = label_x;
      float icon_y = label_y;
      NVGcolor icon_color =
          nvgRGBAf(m_menu->m_item_text.r(), m_menu->m_item_text.g(), m_menu->m_item_text.b(),
                   item.enabled ? m_menu->m_item_text.w() : m_menu->m_item_disabled_text.w());

      if (nvg_is_font_icon(item.icon)) {
        nvgFontFace(ctx, "icons");
        nvgFontSize(ctx, icon_size);
        nvgFillColor(ctx, icon_color);
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(ctx, icon_x, icon_y, utf8(item.icon).data(), nullptr);
      } else {
        int iw, ih;
        nvgImageSize(ctx, item.icon, &iw, &ih);
        float aspect = static_cast<float>(iw) / static_cast<float>(ih);
        float draw_w = icon_size * aspect;
        NVGpaint paint =
            nvgImagePattern(ctx, icon_x, icon_y - icon_size * 0.5f, draw_w, icon_size, 0.f,
                            item.icon, item.enabled ? 1.f : 0.4f);
        nvgBeginPath(ctx);
        nvgRect(ctx, icon_x, icon_y - icon_size * 0.5f, draw_w, icon_size);
        nvgFillPaint(ctx, paint);
        nvgFill(ctx);
      }
      label_x += icon_size + static_cast<float>(m_menu->m_item_gap);
    }

    const Color &text_color = item.enabled ? m_menu->m_item_text : m_menu->m_item_disabled_text;

    const auto &typo =
        m_menu->theme() ? static_cast<FluentWebTheme *>(m_menu->theme())
                              ->typography(FluentWebTheme::TypographyToken::Body1)
                        : FluentWebTheme::TypographySpec{14.f, 20.f, 400, 0.f};
    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, typo.font_size);
    nvgFillColor(ctx, nvgRGBAf(text_color.r(), text_color.g(), text_color.b(), text_color.w()));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, label_x, label_y, item.label.c_str(), nullptr);

    if (!item.shortcut.empty()) {
      nvgFontFace(ctx, "sans");
      nvgFontSize(ctx, typo.font_size - 1.f);
      const Color &shortcut_color =
          item.enabled ? m_menu->m_item_shortcut_text : m_menu->m_item_disabled_text;
      nvgFillColor(ctx,
                   nvgRGBAf(shortcut_color.r(), shortcut_color.g(), shortcut_color.b(),
                            shortcut_color.w()));
      nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
      float shortcut_x = x + w - static_cast<float>(m_menu->m_item_padding);
      nvgText(ctx, shortcut_x, label_y, item.shortcut.c_str(), nullptr);
    }
  }

private:
  FluentWebMenu *m_menu;
  size_t m_index;
  bool m_pressed;
};

FluentWebMenu::FluentWebMenu(Widget *parent, Window *parent_window)
    : FluentWebPopover(parent, parent_window), m_anchor(nullptr), m_highlight_index(-1),
      m_close_on_selection(true), m_item_text(Color(0.1f, 0.1f, 0.1f, 1.f)),
      m_item_disabled_text(Color(0.5f, 0.5f, 0.5f, 1.f)),
      m_item_shortcut_text(Color(0.4f, 0.4f, 0.4f, 1.f)),
      m_item_bg_hover(Color(0.9f, 0.9f, 0.9f, 1.f)),
      m_item_bg_active(Color(0.85f, 0.85f, 0.85f, 1.f)),
      m_separator_color(Color(0.8f, 0.8f, 0.8f, 1.f)), m_separator_thickness(1.f),
      m_item_height(40), m_item_padding(12), m_item_gap(8), m_menu_margin(8) {
  set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, m_menu_margin,
                           round_spacing(nullptr, FluentWebTheme::SpaceToken::SNudge, 0)));
  set_visible(false);
  set_anchor_size(0);
  set_anchor_offset(0);
}

int FluentWebMenu::add_item(const std::string &label, const std::function<void()> &callback,
                            int icon, const std::string &shortcut, bool enabled) {
  Item item;
  item.label = label;
  item.shortcut = shortcut;
  item.icon = icon;
  item.enabled = enabled;
  item.separator = false;
  item.callback = callback;
  m_items.emplace_back(std::move(item));
  rebuild_item_widgets();
  return static_cast<int>(m_items.size()) - 1;
}

void FluentWebMenu::add_separator() {
  Item item;
  item.separator = true;
  item.enabled = false;
  m_items.emplace_back(std::move(item));
  rebuild_item_widgets();
}

void FluentWebMenu::clear_items() {
  m_items.clear();
  m_highlight_index = -1;
  while (!m_children.empty()) {
    int index = static_cast<int>(m_children.size()) - 1;
    remove_child_at(index);
  }
  m_item_widgets.clear();
}

void FluentWebMenu::show_for_anchor(Widget *anchor) {
  m_anchor = anchor;
  if (anchor) {
    Window *anchor_window = anchor->window();
    Vector2i anchor_pos;
    if (anchor_window)
      anchor_pos = anchor->absolute_position() - anchor_window->position();
    else
      anchor_pos = anchor->absolute_position();
    anchor_pos.y() += anchor->height();
    set_anchor_pos(anchor_pos);
    set_position(anchor->absolute_position() + Vector2i(0, anchor->height()));
  }

  if (Screen *scr = screen()) {
    set_size(preferred_size(scr->nvg_context()));
    scr->perform_layout();
    scr->move_window_to_front(this);
  }

  ensure_highlight_valid();
  set_visible(true);
  request_focus();
  refresh_relative_placement();
}

void FluentWebMenu::dismiss() {
  if (visible())
    set_visible(false);
  m_anchor = nullptr;
}

void FluentWebMenu::set_theme(Theme *theme) {
  FluentWebPopover::set_theme(theme);
  refresh_tokens();
  update_layout_metrics();
  update_item_states();
}

Vector2i FluentWebMenu::preferred_size_impl(NVGcontext *ctx) const {
  bool has_entries = false;
  for (const auto &item : m_items) {
    if (!item.separator) {
      has_entries = true;
      break;
    }
  }
  if (!has_entries)
    return Vector2i(m_menu_margin * 2, m_menu_margin * 2);

  int width = 0;
  int height = m_menu_margin * 2;

  for (size_t i = 0; i < m_items.size(); ++i) {
    const Item &item = m_items[i];
    if (item.separator) {
      height += static_cast<int>(std::ceil(m_separator_thickness)) + m_item_gap;
      continue;
    }
    const MenuItemWidget *widget = m_item_widgets[i];
    if (!widget)
      continue;
    Vector2i pref = widget->preferred_size(ctx);
    width = std::max(width, pref.x());
    height += pref.y() + m_item_gap;
  }

  if (!m_items.empty())
    height -= m_item_gap; // remove trailing gap

  width += m_menu_margin * 2;
  return Vector2i(width, height);
}

bool FluentWebMenu::keyboard_event(int key, int, int action, int) {
  if (!visible())
    return false;

  if (action != GLFW_PRESS && action != GLFW_REPEAT)
    return false;

  if (key == GLFW_KEY_DOWN) {
    int next = next_enabled_index(m_highlight_index, +1);
    if (next >= 0)
      set_highlight(next);
    return true;
  }
  if (key == GLFW_KEY_UP) {
    int next = next_enabled_index(m_highlight_index, -1);
    if (next >= 0)
      set_highlight(next);
    return true;
  }
  if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
    if (m_highlight_index >= 0)
      activate(m_highlight_index);
    return true;
  }
  if (key == GLFW_KEY_ESCAPE) {
    dismiss();
    return true;
  }
  return false;
}

bool FluentWebMenu::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
  if (!contains(p)) {
    if (!down)
      dismiss();
    return false;
  }
  return FluentWebPopover::mouse_button_event(p, button, down, modifiers);
}

void FluentWebMenu::refresh_tokens() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());
  if (!fluent) {
    m_item_text = Color(0.1f, 0.1f, 0.1f, 1.f);
    m_item_disabled_text = Color(0.6f, 0.6f, 0.6f, 1.f);
    m_item_shortcut_text = Color(0.4f, 0.4f, 0.4f, 1.f);
    m_item_bg_hover = Color(0.95f, 0.95f, 0.95f, 1.f);
    m_item_bg_active = Color(0.9f, 0.9f, 0.9f, 1.f);
    m_separator_color = Color(0.8f, 0.8f, 0.8f, 1.f);
    m_separator_thickness = 1.f;
    return;
  }

  m_item_text = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground1,
                             Color(0.1f, 0.1f, 0.1f, 1.f));
  m_item_disabled_text = themed_color(
      fluent, FluentWebTheme::ColorToken::colorNeutralForegroundDisabled,
      Color(0.6f, 0.6f, 0.6f, 1.f));
  m_item_shortcut_text = themed_color(
      fluent, FluentWebTheme::ColorToken::colorNeutralForeground2,
      Color(0.4f, 0.4f, 0.4f, 1.f));
  m_item_bg_hover = themed_color(
      fluent, FluentWebTheme::ColorToken::colorSubtleBackgroundHover,
      Color(0.93f, 0.93f, 0.93f, 1.f));
  m_item_bg_active = themed_color(
      fluent, FluentWebTheme::ColorToken::colorSubtleBackgroundPressed,
      Color(0.87f, 0.87f, 0.87f, 1.f));
  m_separator_color = themed_color(
      fluent, FluentWebTheme::ColorToken::colorNeutralStroke2,
      Color(0.82f, 0.82f, 0.82f, 1.f));
  m_separator_thickness = std::max(1.f, fluent->spacing(FluentWebTheme::SpaceToken::SNudge) * 0.2f);
}

void FluentWebMenu::update_layout_metrics() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());
  m_item_height = round_spacing(fluent, FluentWebTheme::SpaceToken::XL, 40);
  m_item_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::M, 12);
  m_item_gap = round_spacing(fluent, FluentWebTheme::SpaceToken::SNudge, 6);
  m_menu_margin = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);

  if (auto *box = dynamic_cast<BoxLayout *>(layout())) {
    box->set_margin(m_menu_margin);
    box->set_spacing(m_item_gap);
  }

  for (Widget *child : m_children) {
    if (auto *sep = dynamic_cast<SeparatorWidget *>(child))
      sep->set_style(m_separator_color, m_separator_thickness, m_item_gap / 2);
  }
}

void FluentWebMenu::rebuild_item_widgets() {
  while (!m_children.empty()) {
    int index = static_cast<int>(m_children.size()) - 1;
    remove_child_at(index);
  }
  m_item_widgets.assign(m_items.size(), nullptr);

  for (size_t i = 0; i < m_items.size(); ++i) {
    if (m_items[i].separator) {
      auto *sep = new SeparatorWidget(this, m_separator_color, m_separator_thickness,
                                      m_item_gap / 2);
      sep->set_fixed_height(static_cast<int>(std::ceil(m_separator_thickness)) + m_item_gap);
    } else {
      auto *item_widget = new MenuItemWidget(this, i);
      item_widget->set_fixed_height(m_item_height);
      m_item_widgets[i] = item_widget;
    }
  }

  update_item_states();
  ensure_highlight_valid();
  preferred_size_changed();
}

void FluentWebMenu::ensure_highlight_valid() {
  if (m_items.empty()) {
    m_highlight_index = -1;
    return;
  }
  if (m_highlight_index >= 0 && m_highlight_index < (int)m_items.size() &&
      !m_items[m_highlight_index].separator && m_items[m_highlight_index].enabled)
    return;

  m_highlight_index = next_enabled_index(-1, +1);
}

void FluentWebMenu::set_highlight(int index) {
  if (index < 0 || index >= (int)m_items.size()) {
    m_highlight_index = -1;
    return;
  }
  if (m_items[index].separator || !m_items[index].enabled)
    return;
  if (m_highlight_index == index)
    return;
  m_highlight_index = index;
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebMenu::activate(int index) {
  if (index < 0 || index >= (int)m_items.size())
    return;
  Item &item = m_items[index];
  if (item.separator || !item.enabled)
    return;
  if (item.callback)
    item.callback();
  if (m_close_on_selection)
    dismiss();
}

int FluentWebMenu::next_enabled_index(int start, int delta) const {
  if (m_items.empty())
    return -1;

  int count = static_cast<int>(m_items.size());
  int idx = start;
  for (int attempts = 0; attempts < count; ++attempts) {
    idx += delta;
    if (idx < 0)
      idx = count - 1;
    else if (idx >= count)
      idx = 0;

    const Item &item = m_items[idx];
    if (!item.separator && item.enabled)
      return idx;
  }
  return -1;
}

void FluentWebMenu::update_item_states() {
  for (size_t i = 0; i < m_item_widgets.size(); ++i) {
    if (auto *widget = m_item_widgets[i])
      widget->set_fixed_height(m_item_height);
  }
  preferred_size_changed();
}

FluentWebMenuBar::FluentWebMenuBar(Widget *parent)
    : Widget(parent), m_open_menu(nullptr), m_background(Color(0.98f, 0.98f, 0.98f, 1.f)),
      m_border(Color(0.f, 0.f, 0.f, 0.05f)), m_padding(8), m_spacing(8) {
  set_layout(new BoxLayout(Orientation::Horizontal, Alignment::Middle, m_padding, m_spacing));
}

FluentWebMenu *FluentWebMenuBar::add_menu(const std::string &label) {
  auto *button =
      new FluentWebButton(this, label, FluentWebButton::Appearance::Transparent);
  button->set_compact(true);
  button->set_background_color(Color(0.f, 0.f));

  Widget *parent_for_menu = screen() ? static_cast<Widget *>(screen()) : static_cast<Widget *>(this);
  Window *parent_window = window();
  auto *menu = new FluentWebMenu(parent_for_menu, parent_window);
  menu->set_theme(theme());

  m_menus.push_back(MenuEntry{button, menu});
  button->set_callback([this, menu]() { handle_button_activation(menu); });

  perform_layout(screen() ? screen()->nvg_context() : nullptr);
  return menu;
}

void FluentWebMenuBar::clear_menus() {
  close_open_menu();
  for (auto &entry : m_menus) {
    if (entry.button) {
      remove_child(entry.button);
      entry.button = nullptr;
    }
    if (entry.menu) {
      entry.menu->dismiss();
      if (Widget *menu_parent = entry.menu->parent())
        menu_parent->remove_child(entry.menu);
      else
        entry.menu->dec_ref();
      entry.menu = nullptr;
    }
  }
  m_menus.clear();
}

void FluentWebMenuBar::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_tokens();
  for (auto &entry : m_menus) {
    if (entry.menu)
      entry.menu->set_theme(theme);
  }
}

Vector2i FluentWebMenuBar::preferred_size_impl(NVGcontext *) const {
  return Vector2i(0, m_padding * 2 + 32);
}

void FluentWebMenuBar::perform_layout(NVGcontext *ctx) {
  if (auto *box = dynamic_cast<BoxLayout *>(layout())) {
    box->set_margin(m_padding);
    box->set_spacing(m_spacing);
  }
  Widget::perform_layout(ctx);
  for (auto &entry : m_menus) {
    if (entry.button) {
      entry.button->set_background_color(Color(0.f, 0.f));
      entry.button->set_compact(true);
    }
  }
}

void FluentWebMenuBar::refresh_tokens() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());
  if (!fluent) {
    m_background = Color(0.98f, 0.98f, 0.98f, 1.f);
    m_border = Color(0.f, 0.f, 0.f, 0.05f);
    m_padding = 8;
    m_spacing = 8;
    return;
  }

  m_background = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground2,
                              Color(0.96f, 0.96f, 0.96f, 1.f));
  m_border = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralStroke2,
                          Color(0.f, 0.f, 0.f, 0.1f));
  m_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);
  m_spacing = round_spacing(fluent, FluentWebTheme::SpaceToken::M, 12);
}

void FluentWebMenuBar::close_open_menu() {
  if (m_open_menu) {
    m_open_menu->dismiss();
    m_open_menu = nullptr;
  }
}

void FluentWebMenuBar::handle_button_activation(FluentWebMenu *menu) {
  if (!menu)
    return;

  MenuEntry *entry_ptr = nullptr;
  for (auto &entry : m_menus) {
    if (entry.menu == menu) {
      entry_ptr = &entry;
      break;
    }
  }
  if (!entry_ptr)
    return;

  if (m_open_menu == menu && menu->visible()) {
    close_open_menu();
    return;
  }

  if (m_open_menu && m_open_menu != menu)
    close_open_menu();

  menu->show_for_anchor(entry_ptr->button);
  m_open_menu = menu;
}

void FluentWebMenuBar::draw(NVGcontext *ctx) {
  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y());
  float w = static_cast<float>(m_size.x());
  float h = static_cast<float>(m_size.y());

  if (m_background.w() > 0.f) {
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, nvgRGBAf(m_background.r(), m_background.g(), m_background.b(),
                               m_background.w()));
    nvgFill(ctx);
  }

  if (m_border.w() > 0.f) {
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y + h - 1.f, w, 1.f);
    nvgFillColor(ctx, nvgRGBAf(m_border.r(), m_border.g(), m_border.b(), m_border.w()));
    nvgFill(ctx);
  }

  Widget::draw(ctx);
}

NAMESPACE_END(nanogui)
