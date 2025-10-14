#include <nanogui/fluent_web_list_view.h>

#include <nanogui/fluent_web_theme.h>
#include <nanogui/layout.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>

#include <algorithm>
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

const FluentWebTheme::TypographySpec &
typography_or_default(const FluentWebTheme *theme, FluentWebTheme::TypographyToken token,
                      const FluentWebTheme::TypographySpec &fallback) {
  if (!theme)
    return fallback;
  return theme->typography(token);
}

#if defined(GLFW_MOD_CONTROL)
constexpr int kControlModifierMask = GLFW_MOD_CONTROL;
#else
constexpr int kControlModifierMask = 0;
#endif
#if defined(GLFW_MOD_SHIFT)
constexpr int kShiftModifierMask = GLFW_MOD_SHIFT;
#else
constexpr int kShiftModifierMask = 0;
#endif

} // namespace

/* --------------------------- FluentWebListItem --------------------------- */

FluentWebListItem::FluentWebListItem(FluentWebListView *owner, const std::string &primary,
                                     const std::string &secondary, Layout layout)
    : Widget(owner), m_owner(owner), m_primary_text(primary), m_secondary_text(secondary),
      m_leading_icon(0), m_trailing_icon(0), m_layout(layout), m_selected(false),
      m_hovered(false) {
  set_cursor(Cursor::Hand);
  apply_metrics();
}
void FluentWebListItem::set_primary_text(const std::string &text) {
  if (m_primary_text == text)
    return;
  m_primary_text = text;
  preferred_size_changed();
}

void FluentWebListItem::set_secondary_text(const std::string &text) {
  if (m_secondary_text == text)
    return;
  m_secondary_text = text;
  preferred_size_changed();
}

void FluentWebListItem::set_meta_text(const std::string &text) {
  if (m_meta_text == text)
    return;
  m_meta_text = text;
  preferred_size_changed();
}

void FluentWebListItem::set_layout(Layout layout) {
  if (m_layout == layout)
    return;
  m_layout = layout;
  preferred_size_changed();
}

void FluentWebListItem::set_selected(bool selected) {
  if (m_selected == selected)
    return;
  m_selected = selected;
  if (Screen *scr = screen())
    scr->redraw();
}

Vector2i FluentWebListItem::preferred_size_impl(NVGcontext *) const {
  if (!m_owner)
    return Vector2i(0);

  int height = m_owner->item_vertical_padding() * 2;
  height += static_cast<int>(std::round(m_owner->primary_line_height()));
  if (m_layout != Layout::OneLine && !m_secondary_text.empty())
    height += static_cast<int>(std::round(m_owner->secondary_line_height()));

  return Vector2i(0, height);
}

void FluentWebListItem::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  if (!m_owner)
    return;

  const Color &hover_bg = m_owner->hover_background_color();
  const Color &selected_bg = m_owner->selected_background_color();
  const Color &selected_border = m_owner->selected_border_color();
  const Color &primary_color = m_owner->primary_text_color();
  const Color &secondary_color = m_owner->secondary_text_color();
  const Color &meta_color = m_owner->meta_text_color();
  int horizontal_padding = m_owner->item_horizontal_padding();
  int vertical_padding = m_owner->item_vertical_padding();
  int icon_size = m_owner->leading_icon_size();

  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y());
  float w = static_cast<float>(m_size.x());
  float h = static_cast<float>(m_size.y());

  nvgSave(ctx);

  if (m_selected && selected_bg.w() > 0.f) {
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, nvgRGBAf(selected_bg.r(), selected_bg.g(), selected_bg.b(), selected_bg.w()));
    nvgFill(ctx);

    if (selected_border.w() > 0.f) {
      nvgBeginPath(ctx);
      nvgRect(ctx, x, y, 3.f, h);
      nvgFillColor(ctx, nvgRGBAf(selected_border.r(), selected_border.g(), selected_border.b(),
                                 selected_border.w()));
      nvgFill(ctx);
    }
  } else if (m_hovered && hover_bg.w() > 0.f) {
    nvgBeginPath(ctx);
    nvgRect(ctx, x, y, w, h);
    nvgFillColor(ctx, nvgRGBAf(hover_bg.r(), hover_bg.g(), hover_bg.b(), hover_bg.w()));
    nvgFill(ctx);
  }

  float cursor_x = x + static_cast<float>(horizontal_padding);
  float center_y = y + h * 0.5f;

  if (m_leading_icon) {
    float icon_draw = static_cast<float>(icon_size);
    nvgFontFace(ctx, "icons");
    nvgFontSize(ctx, icon_draw);
    nvgFillColor(ctx, nvgRGBAf(primary_color.r(), primary_color.g(), primary_color.b(),
                               primary_color.w()));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, cursor_x, center_y, utf8(m_leading_icon).data(), nullptr);
    cursor_x += icon_draw + static_cast<float>(horizontal_padding / 2);
  }

  float text_block_width = w - (cursor_x - x) - static_cast<float>(horizontal_padding);
  if (m_trailing_icon)
    text_block_width -= icon_size + static_cast<float>(horizontal_padding / 2);

  nvgScissor(ctx, cursor_x, y, text_block_width, h);

  nvgFontFace(ctx, "sans");
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);

  float text_y = y + static_cast<float>(vertical_padding);
  nvgFontSize(ctx, m_owner->primary_font_size());
  nvgFillColor(ctx, nvgRGBAf(primary_color.r(), primary_color.g(), primary_color.b(),
                             primary_color.w()));
  nvgText(ctx, cursor_x, text_y, m_primary_text.c_str(), nullptr);

  if (m_layout != Layout::OneLine && !m_secondary_text.empty()) {
    text_y += m_owner->secondary_line_height();
    nvgFontSize(ctx, m_owner->secondary_font_size());
    nvgFillColor(ctx, nvgRGBAf(secondary_color.r(), secondary_color.g(), secondary_color.b(),
                               secondary_color.w()));
    nvgText(ctx, cursor_x, text_y, m_secondary_text.c_str(), nullptr);
  }

  if (!m_meta_text.empty()) {
    float meta_x = x + w - static_cast<float>(horizontal_padding);
    float meta_y = y + static_cast<float>(vertical_padding);
    nvgFontSize(ctx, m_owner->secondary_font_size());
    nvgFillColor(ctx, nvgRGBAf(meta_color.r(), meta_color.g(), meta_color.b(), meta_color.w()));
    nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
    nvgText(ctx, meta_x, meta_y, m_meta_text.c_str(), nullptr);
  }

  nvgResetScissor(ctx);

  if (m_trailing_icon) {
    float icon_draw = static_cast<float>(icon_size);
    nvgFontFace(ctx, "icons");
    nvgFontSize(ctx, icon_draw);
    nvgFillColor(ctx, nvgRGBAf(primary_color.r(), primary_color.g(), primary_color.b(),
                               primary_color.w()));
    nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, x + w - static_cast<float>(horizontal_padding), center_y,
            utf8(m_trailing_icon).data(), nullptr);
  }

  nvgRestore(ctx);
}

bool FluentWebListItem::mouse_button_event(const Vector2i &p, int button, bool down,
                                           int modifiers) {
#if defined(NANOGUI_USE_OPENGL)
  if (!Widget::mouse_button_event(p, button, down, modifiers) && button == GLFW_MOUSE_BUTTON_1 &&
      !down && m_owner) {
    m_owner->notify_item_pressed(this, modifiers);
    return true;
  }
#else
  (void)p;
  (void)button;
  (void)down;
  (void)modifiers;
#endif
  return false;
}

bool FluentWebListItem::mouse_enter_event(const Vector2i &p, bool enter) {
  m_hovered = enter;
  if (Screen *scr = screen())
    scr->redraw();
  return Widget::mouse_enter_event(p, enter);
}

void FluentWebListItem::apply_metrics() {
  preferred_size_changed();
  if (Screen *scr = screen())
    scr->redraw();
}
/* --------------------------- FluentWebListView --------------------------- */

FluentWebListView::FluentWebListView(Widget *parent)
    : Widget(parent), m_selection_mode(SelectionMode::Single), m_active_index(-1),
      m_focus_index(-1), m_corner_radius(6), m_container_padding(8), m_item_spacing(4),
      m_item_horizontal_padding(12), m_item_vertical_padding(10), m_icon_size(20),
      m_primary_font_size(14.f), m_secondary_font_size(12.f), m_primary_line_height(20.f),
      m_secondary_line_height(16.f) {
  set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, m_container_padding,
                           m_item_spacing));
  refresh_metrics();
}

FluentWebListItem *FluentWebListView::add_item(const std::string &primary,
                                               const std::string &secondary,
                                               FluentWebListItem::Layout layout) {
  auto *item = new FluentWebListItem(this, primary, secondary, layout);
  m_items.push_back(item);
  item->apply_metrics();
  refresh_items_from_provider();
  preferred_size_changed();
  return item;
}

void FluentWebListView::clear_items() {
  for (auto *item : m_items) {
    if (item)
      remove_child(item);
  }
  m_items.clear();
  m_selected_indices.clear();
  m_active_index = -1;
  m_focus_index = -1;
  preferred_size_changed();
}

void FluentWebListView::set_item_count(size_t count) {
  if (count == m_items.size()) {
    refresh_items_from_provider();
    return;
  }
  if (count < m_items.size()) {
    for (size_t i = count; i < m_items.size(); ++i)
      remove_child(m_items[i]);
    m_items.resize(count);
  } else {
    for (size_t i = m_items.size(); i < count; ++i) {
      auto *item = new FluentWebListItem(this);
      item->apply_metrics();
      m_items.push_back(item);
    }
  }
  m_selected_indices.clear();
  m_active_index = m_items.empty() ? -1 : 0;
  refresh_items_from_provider();
  preferred_size_changed();
}

void FluentWebListView::set_item_provider(
    const std::function<void(int, FluentWebListItem *)> &provider) {
  m_item_provider = provider;
  refresh_items_from_provider();
}

void FluentWebListView::set_selection_mode(SelectionMode mode) {
  if (m_selection_mode == mode)
    return;
  m_selection_mode = mode;
  if (mode == SelectionMode::None) {
    clear_selection();
  } else if (mode == SelectionMode::Single && m_selected_indices.size() > 1) {
    int keep = m_selected_indices.empty() ? -1 : m_selected_indices.front();
    clear_selection();
    if (keep >= 0)
      set_selected(keep, true);
  }
}

void FluentWebListView::set_selection_callback(
    const std::function<void(const std::vector<int> &)> &cb) {
  m_selection_callback = cb;
}

void FluentWebListView::set_activation_callback(const std::function<void(int)> &cb) {
  m_activation_callback = cb;
}

void FluentWebListView::set_selected(int index, bool selected) {
  if (index < 0 || index >= static_cast<int>(m_items.size()))
    return;

  auto it = std::find(m_selected_indices.begin(), m_selected_indices.end(), index);
  if (selected) {
    if (m_selection_mode == SelectionMode::None)
      return;
    if (m_selection_mode == SelectionMode::Single) {
      clear_selection();
      m_selected_indices.push_back(index);
    } else if (it == m_selected_indices.end()) {
      m_selected_indices.push_back(index);
    }
    m_items[index]->set_selected(true);
  } else if (it != m_selected_indices.end()) {
    m_selected_indices.erase(it);
    m_items[index]->set_selected(false);
  }

  std::sort(m_selected_indices.begin(), m_selected_indices.end());
  if (m_selection_callback)
    m_selection_callback(m_selected_indices);
}

void FluentWebListView::clear_selection() {
  for (int idx : m_selected_indices) {
    if (idx >= 0 && idx < static_cast<int>(m_items.size()))
      m_items[idx]->set_selected(false);
  }
  m_selected_indices.clear();
  if (m_selection_callback)
    m_selection_callback(m_selected_indices);
}
bool FluentWebListView::keyboard_event(int key, int scancode, int action, int modifiers) {
#if !defined(NANOGUI_USE_OPENGL)
  (void)key;
  (void)scancode;
  (void)action;
  (void)modifiers;
  return Widget::keyboard_event(key, scancode, action, modifiers);
#else
  if (action != GLFW_PRESS && action != GLFW_REPEAT)
    return Widget::keyboard_event(key, scancode, action, modifiers);
  if (m_items.empty())
    return false;

  int target = m_active_index;
  if (key == GLFW_KEY_DOWN) {
    target = std::min(static_cast<int>(m_items.size()) - 1, std::max(0, m_active_index) + 1);
  } else if (key == GLFW_KEY_UP) {
    target = std::max(0, std::min(m_active_index, static_cast<int>(m_items.size()) - 1) - 1);
  } else if (key == GLFW_KEY_HOME) {
    target = 0;
  } else if (key == GLFW_KEY_END) {
    target = static_cast<int>(m_items.size()) - 1;
  } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE) {
    if (m_active_index >= 0 && m_active_index < static_cast<int>(m_items.size())) {
      if (m_selection_mode == SelectionMode::Multiple && (modifiers & kControlModifierMask))
        toggle_selection(m_active_index);
      else
        select_single(m_active_index);
      if (m_activation_callback)
        m_activation_callback(m_active_index);
    }
    return true;
  } else {
    return Widget::keyboard_event(key, scancode, action, modifiers);
  }

  if (target != m_active_index) {
    m_active_index = target;
    if (m_selection_mode == SelectionMode::Single && !(modifiers & kControlModifierMask) &&
        !(modifiers & kShiftModifierMask))
      select_single(target);
    update_focus_highlight();
    if (Screen *scr = screen())
      scr->redraw();
  }
  return true;
#endif
}

bool FluentWebListView::focus_event(bool focused) {
  bool handled = Widget::focus_event(focused);
  if (focused) {
    if (m_active_index < 0 && !m_items.empty())
      m_active_index = 0;
  } else {
    m_focus_index = -1;
  }
  update_focus_highlight();
  return handled;
}

void FluentWebListView::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_metrics();
  for (auto *item : m_items)
    item->apply_metrics();
}

void FluentWebListView::draw(NVGcontext *ctx) {
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

void FluentWebListView::notify_item_pressed(FluentWebListItem *item, int modifiers) {
  int index = index_for_item(item);
  if (index < 0)
    return;
  m_active_index = index;
  request_focus();
  if (m_selection_mode == SelectionMode::Multiple && (modifiers & kControlModifierMask))
    toggle_selection(index);
  else
    select_single(index);
  if (m_activation_callback)
    m_activation_callback(index);
}

void FluentWebListView::refresh_metrics() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());

  m_background = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground1,
                              Color(1.f, 1.f, 1.f, 1.f));
  m_border_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralStroke2,
                                Color(0.f, 0.f, 0.f, 0.08f));
  m_primary_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground1,
                                 Color(0.12f, 0.12f, 0.12f, 1.f));
  m_secondary_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground2,
                                   Color(0.38f, 0.38f, 0.38f, 1.f));
  m_meta_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground3,
                              Color(0.45f, 0.45f, 0.45f, 1.f));
  m_hover_background = themed_color(
      fluent, FluentWebTheme::ColorToken::colorSubtleBackgroundHover,
      Color(0.94f, 0.94f, 0.94f, 1.f));
  m_selected_background = themed_color(
      fluent, FluentWebTheme::ColorToken::colorSubtleBackgroundPressed,
      Color(0.88f, 0.88f, 0.88f, 1.f));
  m_selected_border = themed_color(fluent, FluentWebTheme::ColorToken::colorBrandStroke1,
                                   Color(0.2f, 0.4f, 0.85f, 1.f));

  const auto &body = typography_or_default(
      fluent, FluentWebTheme::TypographyToken::Body1,
      FluentWebTheme::TypographySpec{14.f, 20.f, 400, 0.f});
  const auto &caption = typography_or_default(
      fluent, FluentWebTheme::TypographyToken::Caption1,
      FluentWebTheme::TypographySpec{12.f, 16.f, 400, 0.f});

  m_primary_font_size = body.font_size;
  m_primary_line_height = body.line_height;
  m_secondary_font_size = caption.font_size;
  m_secondary_line_height = caption.line_height;

  m_corner_radius = round_spacing(fluent, FluentWebTheme::SpaceToken::MNudge, 6);
  m_container_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);
  m_item_spacing = round_spacing(fluent, FluentWebTheme::SpaceToken::SNudge, 4);
  m_item_horizontal_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::M, 12);
  m_item_vertical_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::S, 8);
  m_icon_size = round_spacing(fluent, FluentWebTheme::SpaceToken::L, 20);

  rebuild_layout();
}

void FluentWebListView::rebuild_layout() {
  if (auto *box = dynamic_cast<BoxLayout *>(layout())) {
    box->set_margin(m_container_padding);
    box->set_spacing(m_item_spacing);
  }
  for (auto *item : m_items)
    item->apply_metrics();
  preferred_size_changed();
}

void FluentWebListView::refresh_items_from_provider() {
  if (!m_item_provider)
    return;
  for (size_t i = 0; i < m_items.size(); ++i)
    m_item_provider(static_cast<int>(i), m_items[i]);
}

void FluentWebListView::update_focus_highlight() {
  if (m_active_index >= 0 && m_active_index < static_cast<int>(m_items.size()))
    m_focus_index = m_active_index;
}

int FluentWebListView::index_for_item(const FluentWebListItem *item) const {
  auto it = std::find(m_items.begin(), m_items.end(), item);
  if (it == m_items.end())
    return -1;
  return static_cast<int>(std::distance(m_items.begin(), it));
}

void FluentWebListView::select_single(int index) {
  if (m_selection_mode == SelectionMode::None)
    return;
  clear_selection();
  set_selected(index, true);
}

void FluentWebListView::toggle_selection(int index) {
  if (m_selection_mode == SelectionMode::None)
    return;
  bool already = std::find(m_selected_indices.begin(), m_selected_indices.end(), index) !=
                 m_selected_indices.end();
  set_selected(index, !already);
}
/* --------------------------- FluentWebGridItem --------------------------- */

FluentWebGridItem::FluentWebGridItem(FluentWebGridView *owner, const std::string &title,
                                     const std::string &subtitle, int icon)
    : Widget(owner), m_owner(owner), m_title(title), m_subtitle(subtitle), m_icon(icon),
      m_selected(false), m_hovered(false) {
  set_cursor(Cursor::Hand);
  apply_metrics();
}

void FluentWebGridItem::set_title(const std::string &title) {
  if (m_title == title)
    return;
  m_title = title;
  preferred_size_changed();
}

void FluentWebGridItem::set_subtitle(const std::string &subtitle) {
  if (m_subtitle == subtitle)
    return;
  m_subtitle = subtitle;
  preferred_size_changed();
}

void FluentWebGridItem::set_selected(bool selected) {
  if (m_selected == selected)
    return;
  m_selected = selected;
  if (Screen *scr = screen())
    scr->redraw();
}

Vector2i FluentWebGridItem::preferred_size_impl(NVGcontext *) const {
  if (!m_owner)
    return Vector2i(0);
  return m_owner->tile_size();
}

void FluentWebGridItem::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  if (!m_owner)
    return;

  const Color &base_bg = m_owner->tile_background();
  const Color &hover_bg = m_owner->tile_hover_background();
  const Color &selected_bg = m_owner->tile_selected_background();
  const Color &selected_border = m_owner->tile_selected_border();
  const Color &title_color = m_owner->title_color();
  const Color &subtitle_color = m_owner->subtitle_color();
  int padding = m_owner->tile_padding();

  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y());
  float w = static_cast<float>(m_size.x());
  float h = static_cast<float>(m_size.y());

  nvgSave(ctx);

  Color bg = base_bg;
  if (m_selected)
    bg = selected_bg;
  else if (m_hovered)
    bg = hover_bg;

  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, 8.f);
  nvgFillColor(ctx, nvgRGBAf(bg.r(), bg.g(), bg.b(), bg.w()));
  nvgFill(ctx);

  if (m_selected && selected_border.w() > 0.f) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f, 8.f);
    nvgStrokeWidth(ctx, 2.f);
    nvgStrokeColor(ctx, nvgRGBAf(selected_border.r(), selected_border.g(),
                                 selected_border.b(), selected_border.w()));
    nvgStroke(ctx);
  }

  float cursor_y = y + static_cast<float>(padding);
  float cursor_x = x + static_cast<float>(padding);
  float text_width = w - static_cast<float>(padding * 2);

  if (m_icon) {
    float icon_size = 32.f;
    nvgFontFace(ctx, "icons");
    nvgFontSize(ctx, icon_size);
    nvgFillColor(ctx, nvgRGBAf(title_color.r(), title_color.g(), title_color.b(),
                               title_color.w()));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgText(ctx, cursor_x, cursor_y, utf8(m_icon).data(), nullptr);
    cursor_y += icon_size + static_cast<float>(padding);
  }

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, m_owner->title_font_size());
  nvgFillColor(ctx, nvgRGBAf(title_color.r(), title_color.g(), title_color.b(),
                             title_color.w()));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  nvgText(ctx, cursor_x, cursor_y, m_title.c_str(), nullptr);
  cursor_y += m_owner->title_font_size() + static_cast<float>(padding / 2);

  if (!m_subtitle.empty()) {
    nvgFontSize(ctx, m_owner->subtitle_font_size());
    nvgFillColor(ctx, nvgRGBAf(subtitle_color.r(), subtitle_color.g(), subtitle_color.b(),
                               subtitle_color.w()));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextBox(ctx, cursor_x, cursor_y, text_width, m_subtitle.c_str(), nullptr);
  }

  nvgRestore(ctx);
}

bool FluentWebGridItem::mouse_button_event(const Vector2i &p, int button, bool down,
                                           int modifiers) {
#if defined(NANOGUI_USE_OPENGL)
  if (!Widget::mouse_button_event(p, button, down, modifiers) && button == GLFW_MOUSE_BUTTON_1 &&
      !down && m_owner) {
    m_owner->notify_item_pressed(this, modifiers);
    return true;
  }
#else
  (void)p;
  (void)button;
  (void)down;
  (void)modifiers;
#endif
  return false;
}

bool FluentWebGridItem::mouse_enter_event(const Vector2i &p, bool enter) {
  m_hovered = enter;
  if (Screen *scr = screen())
    scr->redraw();
  return Widget::mouse_enter_event(p, enter);
}

void FluentWebGridItem::apply_metrics() {
  preferred_size_changed();
  if (Screen *scr = screen())
    scr->redraw();
}
/* --------------------------- FluentWebGridView --------------------------- */

FluentWebGridView::FluentWebGridView(Widget *parent)
    : Widget(parent), m_selection_mode(FluentWebListView::SelectionMode::Single), m_columns(3),
      m_tile_size(Vector2i(160, 140)), m_active_index(-1), m_focus_index(-1), m_corner_radius(8),
      m_container_padding(12), m_tile_spacing(12), m_tile_padding(12), m_title_font_size(14.f),
      m_subtitle_font_size(12.f) {
  refresh_metrics();
}

FluentWebGridItem *FluentWebGridView::add_item(const std::string &title,
                                               const std::string &subtitle, int icon) {
  auto *item = new FluentWebGridItem(this, title, subtitle, icon);
  m_items.push_back(item);
  item->apply_metrics();
  refresh_items_from_provider();
  preferred_size_changed();
  return item;
}

void FluentWebGridView::clear_items() {
  for (auto *item : m_items) {
    if (item)
      remove_child(item);
  }
  m_items.clear();
  m_selected_indices.clear();
  m_active_index = -1;
  m_focus_index = -1;
  preferred_size_changed();
}

void FluentWebGridView::set_item_count(size_t count) {
  if (count == m_items.size()) {
    refresh_items_from_provider();
    return;
  }
  if (count < m_items.size()) {
    for (size_t i = count; i < m_items.size(); ++i)
      remove_child(m_items[i]);
    m_items.resize(count);
  } else {
    for (size_t i = m_items.size(); i < count; ++i) {
      auto *item = new FluentWebGridItem(this);
      item->apply_metrics();
      m_items.push_back(item);
    }
  }
  m_selected_indices.clear();
  m_active_index = m_items.empty() ? -1 : 0;
  refresh_items_from_provider();
  preferred_size_changed();
}

void FluentWebGridView::set_item_provider(
    const std::function<void(int, FluentWebGridItem *)> &provider) {
  m_item_provider = provider;
  refresh_items_from_provider();
}

void FluentWebGridView::set_columns(int columns) {
  if (columns <= 0)
    columns = 1;
  if (m_columns == columns)
    return;
  m_columns = columns;
  preferred_size_changed();
  if (Screen *scr = screen())
    scr->perform_layout();
}

void FluentWebGridView::set_tile_size(const Vector2i &size) {
  if (m_tile_size == size)
    return;
  m_tile_size = size;
  for (auto *item : m_items)
    item->preferred_size_changed();
  preferred_size_changed();
}

void FluentWebGridView::set_selection_mode(FluentWebListView::SelectionMode mode) {
  if (m_selection_mode == mode)
    return;
  m_selection_mode = mode;
  if (mode == FluentWebListView::SelectionMode::None) {
    clear_selection();
  } else if (mode == FluentWebListView::SelectionMode::Single && m_selected_indices.size() > 1) {
    int keep = m_selected_indices.empty() ? -1 : m_selected_indices.front();
    clear_selection();
    if (keep >= 0)
      set_selected(keep, true);
  }
}

void FluentWebGridView::set_selection_callback(
    const std::function<void(const std::vector<int> &)> &cb) {
  m_selection_callback = cb;
}

void FluentWebGridView::set_activation_callback(const std::function<void(int)> &cb) {
  m_activation_callback = cb;
}

void FluentWebGridView::set_selected(int index, bool selected) {
  if (index < 0 || index >= static_cast<int>(m_items.size()))
    return;

  auto it = std::find(m_selected_indices.begin(), m_selected_indices.end(), index);
  if (selected) {
    if (m_selection_mode == FluentWebListView::SelectionMode::None)
      return;
    if (m_selection_mode == FluentWebListView::SelectionMode::Single) {
      clear_selection();
      m_selected_indices.push_back(index);
    } else if (it == m_selected_indices.end()) {
      m_selected_indices.push_back(index);
    }
    m_items[index]->set_selected(true);
  } else if (it != m_selected_indices.end()) {
    m_selected_indices.erase(it);
    m_items[index]->set_selected(false);
  }

  std::sort(m_selected_indices.begin(), m_selected_indices.end());
  if (m_selection_callback)
    m_selection_callback(m_selected_indices);
}

void FluentWebGridView::clear_selection() {
  for (int idx : m_selected_indices) {
    if (idx >= 0 && idx < static_cast<int>(m_items.size()))
      m_items[idx]->set_selected(false);
  }
  m_selected_indices.clear();
  if (m_selection_callback)
    m_selection_callback(m_selected_indices);
}

void FluentWebGridView::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_metrics();
  for (auto *item : m_items)
    item->apply_metrics();
}

void FluentWebGridView::perform_layout(NVGcontext *ctx) {
  if (m_items.empty())
    return;

  int columns = std::max(1, m_columns);
  Vector2i tile = m_tile_size;
  int spacing = m_tile_spacing;
  int padding = m_container_padding;

  int column = 0;
  int x_cursor = padding;
  int y_cursor = padding;

  for (auto *item : m_items) {
    item->set_position(Vector2i(x_cursor, y_cursor));
    item->set_size(tile);
    item->perform_layout(ctx);

    ++column;
    if (column >= columns) {
      column = 0;
      x_cursor = padding;
      y_cursor += tile.y() + spacing;
    } else {
      x_cursor += tile.x() + spacing;
    }
  }
}

Vector2i FluentWebGridView::preferred_size_impl(NVGcontext *) const {
  if (m_items.empty())
    return Vector2i(m_container_padding * 2, m_container_padding * 2);

  int columns = std::max(1, m_columns);
  int rows = static_cast<int>((m_items.size() + columns - 1) / columns);

  int width = m_container_padding * 2 + m_tile_size.x();
  if (columns > 1)
    width += (columns - 1) * (m_tile_size.x() + m_tile_spacing);

  int height = m_container_padding * 2 + rows * m_tile_size.y();
  if (rows > 1)
    height += (rows - 1) * m_tile_spacing;

  return Vector2i(width, height);
}

bool FluentWebGridView::keyboard_event(int key, int scancode, int action, int modifiers) {
#if !defined(NANOGUI_USE_OPENGL)
  (void)key;
  (void)scancode;
  (void)action;
  (void)modifiers;
  return Widget::keyboard_event(key, scancode, action, modifiers);
#else
  if (action != GLFW_PRESS && action != GLFW_REPEAT)
    return Widget::keyboard_event(key, scancode, action, modifiers);
  if (m_items.empty())
    return false;

  int columns = std::max(1, m_columns);
  int target = m_active_index;

  if (key == GLFW_KEY_RIGHT) {
    target = std::min(static_cast<int>(m_items.size()) - 1, std::max(0, m_active_index) + 1);
  } else if (key == GLFW_KEY_LEFT) {
    target = std::max(0, std::min(m_active_index, static_cast<int>(m_items.size()) - 1) - 1);
  } else if (key == GLFW_KEY_DOWN) {
    target = std::min(static_cast<int>(m_items.size()) - 1, std::max(0, m_active_index) + columns);
  } else if (key == GLFW_KEY_UP) {
    target = std::max(0, std::min(m_active_index, static_cast<int>(m_items.size()) - 1) - columns);
  } else if (key == GLFW_KEY_HOME) {
    target = 0;
  } else if (key == GLFW_KEY_END) {
    target = static_cast<int>(m_items.size()) - 1;
  } else if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE) {
    if (m_active_index >= 0 && m_active_index < static_cast<int>(m_items.size())) {
      if (m_selection_mode == FluentWebListView::SelectionMode::Multiple &&
          (modifiers & kControlModifierMask))
        toggle_selection(m_active_index);
      else
        select_single(m_active_index);
      if (m_activation_callback)
        m_activation_callback(m_active_index);
    }
    return true;
  } else {
    return Widget::keyboard_event(key, scancode, action, modifiers);
  }

  if (target != m_active_index) {
    m_active_index = target;
    if (m_selection_mode == FluentWebListView::SelectionMode::Single &&
        !(modifiers & kControlModifierMask))
      select_single(target);
    if (Screen *scr = screen())
      scr->redraw();
  }
  return true;
#endif
}

bool FluentWebGridView::focus_event(bool focused) {
  bool handled = Widget::focus_event(focused);
  if (focused) {
    if (m_active_index < 0 && !m_items.empty())
      m_active_index = 0;
  } else {
    m_focus_index = -1;
  }
  return handled;
}

void FluentWebGridView::draw(NVGcontext *ctx) {
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

void FluentWebGridView::notify_item_pressed(FluentWebGridItem *item, int modifiers) {
  int index = index_for_item(item);
  if (index < 0)
    return;
  m_active_index = index;
  request_focus();
  if (m_selection_mode == FluentWebListView::SelectionMode::Multiple &&
      (modifiers & kControlModifierMask))
    toggle_selection(index);
  else
    select_single(index);
  if (m_activation_callback)
    m_activation_callback(index);
}

void FluentWebGridView::refresh_metrics() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());

  m_background = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground1,
                              Color(1.f, 1.f, 1.f, 1.f));
  m_border_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralStroke2,
                                Color(0.f, 0.f, 0.f, 0.08f));
  m_tile_background = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground1,
                                   Color(0.98f, 0.98f, 0.98f, 1.f));
  m_tile_hover_background = themed_color(
      fluent, FluentWebTheme::ColorToken::colorSubtleBackgroundHover,
      Color(0.93f, 0.93f, 0.93f, 1.f));
  m_tile_selected_background = themed_color(
      fluent, FluentWebTheme::ColorToken::colorNeutralBackground3,
      Color(0.88f, 0.88f, 0.88f, 1.f));
  m_tile_selected_border = themed_color(
      fluent, FluentWebTheme::ColorToken::colorBrandStroke1, Color(0.2f, 0.4f, 0.85f, 1.f));
  m_title_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground1,
                               Color(0.12f, 0.12f, 0.12f, 1.f));
  m_subtitle_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground2,
                                  Color(0.4f, 0.4f, 0.4f, 1.f));

  const auto &title_typo = typography_or_default(
      fluent, FluentWebTheme::TypographyToken::Body1,
      FluentWebTheme::TypographySpec{14.f, 20.f, 500, 0.f});
  const auto &subtitle_typo = typography_or_default(
      fluent, FluentWebTheme::TypographyToken::Caption1,
      FluentWebTheme::TypographySpec{12.f, 16.f, 400, 0.f});

  m_title_font_size = title_typo.font_size;
  m_subtitle_font_size = subtitle_typo.font_size;

  m_corner_radius = round_spacing(fluent, FluentWebTheme::SpaceToken::MNudge, 8);
  m_container_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::M, 12);
  m_tile_spacing = round_spacing(fluent, FluentWebTheme::SpaceToken::M, 12);
  m_tile_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::M, 12);

  preferred_size_changed();
}

void FluentWebGridView::refresh_items_from_provider() {
  if (!m_item_provider)
    return;
  for (size_t i = 0; i < m_items.size(); ++i)
    m_item_provider(static_cast<int>(i), m_items[i]);
}

int FluentWebGridView::index_for_item(const FluentWebGridItem *item) const {
  auto it = std::find(m_items.begin(), m_items.end(), item);
  if (it == m_items.end())
    return -1;
  return static_cast<int>(std::distance(m_items.begin(), it));
}

void FluentWebGridView::select_single(int index) {
  if (m_selection_mode == FluentWebListView::SelectionMode::None)
    return;
  clear_selection();
  set_selected(index, true);
}

void FluentWebGridView::toggle_selection(int index) {
  if (m_selection_mode == FluentWebListView::SelectionMode::None)
    return;
  bool already = std::find(m_selected_indices.begin(), m_selected_indices.end(), index) !=
                 m_selected_indices.end();
  set_selected(index, !already);
}

NAMESPACE_END(nanogui)
