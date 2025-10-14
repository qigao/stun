#include <nanogui/fluent_web_table.h>

#include <nanogui/fluent_web_theme.h>
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

/* --------------------------- FluentWebTableView --------------------------- */

FluentWebTableView::FluentWebTableView(Widget *parent)
    : Widget(parent), m_selection_mode(SelectionMode::Single), m_active_row(-1), m_anchor_row(-1),
      m_hover_row(-1), m_sort_column(-1), m_sort_ascending(true), m_show_horizontal_dividers(false),
      m_show_vertical_dividers(false), m_use_zebra_striping(false) {
  refresh_tokens();
}

int FluentWebTableView::add_column(const Column &column) {
  m_columns.push_back(column);
  preferred_size_changed();
  return static_cast<int>(m_columns.size()) - 1;
}

void FluentWebTableView::clear_columns() {
  m_columns.clear();
  m_sort_column = -1;
  preferred_size_changed();
}

void FluentWebTableView::set_columns(const std::vector<Column> &columns) {
  m_columns = columns;
  m_sort_column = -1;
  preferred_size_changed();
}

int FluentWebTableView::add_row(const std::vector<std::string> &cells, RowState state) {
  m_rows.push_back({cells, state});
  preferred_size_changed();
  return static_cast<int>(m_rows.size()) - 1;
}

void FluentWebTableView::set_rows(const std::vector<Row> &rows) {
  m_rows = rows;
  clear_selection();
  preferred_size_changed();
}

void FluentWebTableView::clear_rows() {
  m_rows.clear();
  clear_selection();
  m_active_row = -1;
  m_hover_row = -1;
  preferred_size_changed();
}

void FluentWebTableView::set_row_state(size_t index, RowState state) {
  if (index < m_rows.size()) {
    m_rows[index].state = state;
    if (Screen *scr = screen())
      scr->redraw();
  }
}

void FluentWebTableView::set_selection_mode(SelectionMode mode) {
  if (m_selection_mode == mode)
    return;
  m_selection_mode = mode;
  if (mode == SelectionMode::None)
    clear_selection();
  else if (mode == SelectionMode::Single && m_selected_rows.size() > 1) {
    int keep = m_selected_rows.empty() ? -1 : m_selected_rows.front();
    clear_selection();
    if (keep >= 0)
      set_selected(keep, true);
  }
}

void FluentWebTableView::set_selected(int index, bool selected) {
  if (index < 0 || index >= static_cast<int>(m_rows.size()))
    return;

  auto it = std::find(m_selected_rows.begin(), m_selected_rows.end(), index);
  if (selected) {
    if (m_selection_mode == SelectionMode::None)
      return;
    if (m_selection_mode == SelectionMode::Single) {
      clear_selection();
      m_selected_rows.push_back(index);
    } else if (it == m_selected_rows.end()) {
      m_selected_rows.push_back(index);
    }
  } else if (it != m_selected_rows.end()) {
    m_selected_rows.erase(it);
  }

  std::sort(m_selected_rows.begin(), m_selected_rows.end());
  if (m_selection_callback)
    m_selection_callback(m_selected_rows);
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebTableView::clear_selection() {
  m_selected_rows.clear();
  if (m_selection_callback)
    m_selection_callback(m_selected_rows);
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebTableView::set_theme(Theme *theme) {
  Widget::set_theme(theme);
  refresh_tokens();
}

void FluentWebTableView::refresh_tokens() {
  auto *fluent = dynamic_cast<FluentWebTheme *>(theme());

  m_background = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground1,
                              Color(1.f, 1.f, 1.f, 1.f));
  m_border_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralStroke2,
                                Color(0.f, 0.f, 0.f, 0.08f));
  m_header_background = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground2,
                                     Color(0.98f, 0.98f, 0.98f, 1.f));
  m_header_text = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground1,
                               Color(0.12f, 0.12f, 0.12f, 1.f));
  m_cell_text = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground1,
                             Color(0.12f, 0.12f, 0.12f, 1.f));
  m_cell_meta_text = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralForeground3,
                                  Color(0.45f, 0.45f, 0.45f, 1.f));
  m_hover_background = themed_color(fluent, FluentWebTheme::ColorToken::colorSubtleBackgroundHover,
                                    Color(0.94f, 0.94f, 0.94f, 1.f));
  m_selected_background =
      themed_color(fluent, FluentWebTheme::ColorToken::colorSubtleBackgroundPressed,
                   Color(0.88f, 0.88f, 0.88f, 1.f));
  m_selected_border = themed_color(fluent, FluentWebTheme::ColorToken::colorBrandStroke1,
                                   Color(0.2f, 0.4f, 0.85f, 1.f));
  m_grid_color = themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralStroke1,
                              Color(0.f, 0.f, 0.f, 0.05f));
  m_focus_outer = themed_color(fluent, FluentWebTheme::ColorToken::colorStrokeFocus2,
                               Color(1.f, 1.f, 1.f, 1.f));
  m_focus_inner = themed_color(fluent, FluentWebTheme::ColorToken::colorStrokeFocus1,
                               Color(0.f, 0.f, 0.f, 1.f));
  m_state_success = themed_color(fluent, FluentWebTheme::ColorToken::colorPaletteGreenBackground2,
                                 Color(0.8f, 0.95f, 0.85f, 1.f));
  m_state_warning = themed_color(fluent, FluentWebTheme::ColorToken::colorPaletteYellowBackground2,
                                 Color(0.98f, 0.93f, 0.7f, 1.f));
  m_state_error = themed_color(fluent, FluentWebTheme::ColorToken::colorPaletteRedBackground2,
                               Color(0.98f, 0.85f, 0.85f, 1.f));
  m_state_info = themed_color(fluent, FluentWebTheme::ColorToken::colorPaletteBlueBackground2,
                              Color(0.85f, 0.92f, 0.98f, 1.f));
  m_row_even_background = m_background;
  m_row_odd_background =
      themed_color(fluent, FluentWebTheme::ColorToken::colorNeutralBackground1Hover,
                   Color(0.98f, 0.98f, 0.98f, 1.f));

  m_corner_radius = round_spacing(fluent, FluentWebTheme::SpaceToken::MNudge, 6);
  m_header_height = round_spacing(fluent, FluentWebTheme::SpaceToken::XXL, 44);
  m_row_height = round_spacing(fluent, FluentWebTheme::SpaceToken::XXL, 44);
  m_cell_padding = round_spacing(fluent, FluentWebTheme::SpaceToken::M, 12);

  preferred_size_changed();
}

Vector2i FluentWebTableView::preferred_size_impl(NVGcontext *) const {
  int width = 0;
  for (const auto &col : m_columns)
    width += col.min_width;
  int height = m_header_height + static_cast<int>(m_rows.size()) * m_row_height;
  return Vector2i(std::max(width, 200), std::max(height, m_header_height + m_row_height));
}

std::vector<float> FluentWebTableView::compute_column_widths(float available_width) const {
  std::vector<float> widths;
  if (m_columns.empty())
    return widths;

  float total_weight = 0.f;
  float fixed_width = 0.f;
  for (const auto &col : m_columns) {
    total_weight += col.weight;
    fixed_width += static_cast<float>(col.min_width);
  }

  float extra = std::max(0.f, available_width - fixed_width);
  for (const auto &col : m_columns) {
    float w = static_cast<float>(col.min_width);
    if (total_weight > 0.f)
      w += extra * (col.weight / total_weight);
    widths.push_back(w);
  }
  return widths;
}

Color FluentWebTableView::row_state_color(RowState state) const {
  switch (state) {
  case RowState::Success:
    return m_state_success;
  case RowState::Warning:
    return m_state_warning;
  case RowState::Error:
    return m_state_error;
  case RowState::Informational:
    return m_state_info;
  default:
    return Color(0.f, 0.f, 0.f, 0.f);
  }
}

Color FluentWebTableView::row_background_for(size_t index, RowState state, bool selected,
                                             bool hovered) const {
  if (selected)
    return m_selected_background;
  if (hovered)
    return m_hover_background;
  if (state != RowState::Normal)
    return row_state_color(state);
  return m_background;
}

int FluentWebTableView::row_at_position(const Vector2i &p) const {
  int local_y = p.y() - m_pos.y() - m_header_height;
  if (local_y < 0)
    return -1;
  int row = local_y / m_row_height;
  return (row >= 0 && row < static_cast<int>(m_rows.size())) ? row : -1;
}

int FluentWebTableView::column_at_position(const Vector2i &p) const {
  auto widths = compute_column_widths(static_cast<float>(m_size.x()));
  float x = static_cast<float>(m_pos.x());
  for (size_t i = 0; i < widths.size(); ++i) {
    x += widths[i];
    if (p.x() < static_cast<int>(x))
      return static_cast<int>(i);
  }
  return -1;
}

void FluentWebTableView::draw(NVGcontext *ctx) {
  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y());
  float w = static_cast<float>(m_size.x());
  float h = static_cast<float>(m_size.y());

  nvgSave(ctx);

  // Background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, static_cast<float>(m_corner_radius));
  nvgFillColor(ctx,
               nvgRGBAf(m_background.r(), m_background.g(), m_background.b(), m_background.w()));
  nvgFill(ctx);

  // Border
  if (m_border_color.w() > 0.f) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x + 0.5f, y + 0.5f, w - 1.f, h - 1.f,
                   std::max(0.f, static_cast<float>(m_corner_radius) - 0.5f));
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(ctx, nvgRGBAf(m_border_color.r(), m_border_color.g(), m_border_color.b(),
                                 m_border_color.w()));
    nvgStroke(ctx);
  }

  auto col_widths = compute_column_widths(w);
  nvgScissor(ctx, x, y, w, h);
  draw_header(ctx, col_widths);
  draw_rows(ctx, col_widths);
  nvgResetScissor(ctx);

  nvgRestore(ctx);
  Widget::draw(ctx);
}

void FluentWebTableView::draw_header(NVGcontext *ctx, const std::vector<float> &col_widths) const {
  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y());
  float w = static_cast<float>(m_size.x());
  float hh = static_cast<float>(m_header_height);

  // Header background
  nvgBeginPath(ctx);
  nvgRect(ctx, x, y, w, hh);
  nvgFillColor(ctx, nvgRGBAf(m_header_background.r(), m_header_background.g(),
                             m_header_background.b(), m_header_background.w()));
  nvgFill(ctx);

  // Header text
  nvgFontFace(ctx, "sans-bold");
  nvgFontSize(ctx, 14.f);
  nvgFillColor(
      ctx, nvgRGBAf(m_header_text.r(), m_header_text.g(), m_header_text.b(), m_header_text.w()));

  float cursor_x = x;
  for (size_t i = 0; i < m_columns.size() && i < col_widths.size(); ++i) {
    const auto &col = m_columns[i];
    float col_w = col_widths[i];

    int align = NVG_ALIGN_MIDDLE;
    float text_x = cursor_x + static_cast<float>(m_cell_padding);
    if (col.align == TextAlign::Center) {
      align |= NVG_ALIGN_CENTER;
      text_x = cursor_x + col_w * 0.5f;
    } else if (col.align == TextAlign::End) {
      align |= NVG_ALIGN_RIGHT;
      text_x = cursor_x + col_w - static_cast<float>(m_cell_padding);
    } else {
      align |= NVG_ALIGN_LEFT;
    }

    nvgTextAlign(ctx, align);
    nvgText(ctx, text_x, y + hh * 0.5f, col.title.c_str(), nullptr);

    // Sort indicator
    if (col.sortable && static_cast<int>(i) == m_sort_column) {
      const char *arrow = m_sort_ascending ? "▲" : "▼";
      nvgFontSize(ctx, 10.f);
      nvgText(ctx, text_x + 60.f, y + hh * 0.5f, arrow, nullptr);
      nvgFontSize(ctx, 14.f);
    }

    cursor_x += col_w;
  }

  // Header bottom border
  if (m_grid_color.w() > 0.f) {
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x, y + hh);
    nvgLineTo(ctx, x + w, y + hh);
    nvgStrokeWidth(ctx, 1.f);
    nvgStrokeColor(
        ctx, nvgRGBAf(m_grid_color.r(), m_grid_color.g(), m_grid_color.b(), m_grid_color.w()));
    nvgStroke(ctx);
  }
}

void FluentWebTableView::draw_rows(NVGcontext *ctx, const std::vector<float> &col_widths) const {
  float x = static_cast<float>(m_pos.x());
  float y = static_cast<float>(m_pos.y()) + static_cast<float>(m_header_height);
  float w = static_cast<float>(m_size.x());
  float rh = static_cast<float>(m_row_height);

  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 14.f);

  for (size_t row_idx = 0; row_idx < m_rows.size(); ++row_idx) {
    const auto &row = m_rows[row_idx];
    bool selected = std::find(m_selected_rows.begin(), m_selected_rows.end(),
                              static_cast<int>(row_idx)) != m_selected_rows.end();
    bool hovered = static_cast<int>(row_idx) == m_hover_row;

    Color bg = row_background_for(row_idx, row.state, selected, hovered);
    if (bg.w() > 0.f) {
      nvgBeginPath(ctx);
      nvgRect(ctx, x, y, w, rh);
      nvgFillColor(ctx, nvgRGBAf(bg.r(), bg.g(), bg.b(), bg.w()));
      nvgFill(ctx);
    }

    // Selection border
    if (selected && m_selected_border.w() > 0.f) {
      nvgBeginPath(ctx);
      nvgRect(ctx, x, y, 3.f, rh);
      nvgFillColor(ctx, nvgRGBAf(m_selected_border.r(), m_selected_border.g(),
                                 m_selected_border.b(), m_selected_border.w()));
      nvgFill(ctx);
    }

    // Cell text
    nvgFillColor(ctx, nvgRGBAf(m_cell_text.r(), m_cell_text.g(), m_cell_text.b(), m_cell_text.w()));
    float cursor_x = x;
    for (size_t col_idx = 0; col_idx < m_columns.size() && col_idx < col_widths.size(); ++col_idx) {
      const auto &col = m_columns[col_idx];
      float col_w = col_widths[col_idx];

      if (col_idx < row.cells.size()) {
        int align = NVG_ALIGN_MIDDLE;
        float text_x = cursor_x + static_cast<float>(m_cell_padding);
        if (col.align == TextAlign::Center) {
          align |= NVG_ALIGN_CENTER;
          text_x = cursor_x + col_w * 0.5f;
        } else if (col.align == TextAlign::End) {
          align |= NVG_ALIGN_RIGHT;
          text_x = cursor_x + col_w - static_cast<float>(m_cell_padding);
        } else {
          align |= NVG_ALIGN_LEFT;
        }

        nvgTextAlign(ctx, align);
        nvgText(ctx, text_x, y + rh * 0.5f, row.cells[col_idx].c_str(), nullptr);
      }

      cursor_x += col_w;
    }

    // Row divider
    if (m_show_horizontal_dividers && m_grid_color.w() > 0.f) {
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, x, y + rh);
      nvgLineTo(ctx, x + w, y + rh);
      nvgStrokeWidth(ctx, 1.f);
      nvgStrokeColor(
          ctx, nvgRGBAf(m_grid_color.r(), m_grid_color.g(), m_grid_color.b(), m_grid_color.w()));
      nvgStroke(ctx);
    }

    y += rh;
  }
}

bool FluentWebTableView::mouse_button_event(const Vector2i &p, int button, bool down,
                                            int modifiers) {
#if !defined(NANOGUI_USE_OPENGL)
  (void)p;
  (void)button;
  (void)down;
  (void)modifiers;
  return Widget::mouse_button_event(p, button, down, modifiers);
#else
  if (Widget::mouse_button_event(p, button, down, modifiers))
    return true;

  if (button != GLFW_MOUSE_BUTTON_1 || down)
    return false;

  // Check header click for sorting
  int local_y = p.y() - m_pos.y();
  if (local_y >= 0 && local_y < m_header_height) {
    int col = column_at_position(p);
    if (col >= 0 && col < static_cast<int>(m_columns.size()) && m_columns[col].sortable) {
      bool ascending = (col == m_sort_column) ? !m_sort_ascending : true;
      ensure_sort_column(col, ascending);
      if (m_sort_callback)
        m_sort_callback(col, ascending);
      return true;
    }
  }

  // Check row click for selection
  int row = row_at_position(p);
  if (row >= 0 && m_selection_mode != SelectionMode::None) {
    if (m_selection_mode == SelectionMode::Multiple && (modifiers & kControlModifierMask)) {
      set_selected(row, std::find(m_selected_rows.begin(), m_selected_rows.end(), row) ==
                            m_selected_rows.end());
    } else if (m_selection_mode == SelectionMode::Multiple && (modifiers & kShiftModifierMask) &&
               m_anchor_row >= 0) {
      clear_selection();
      int start = std::min(m_anchor_row, row);
      int end = std::max(m_anchor_row, row);
      for (int i = start; i <= end; ++i)
        set_selected(i, true);
    } else {
      clear_selection();
      set_selected(row, true);
      m_anchor_row = row;
    }
    m_active_row = row;
    request_focus();
    return true;
  }

  return false;
#endif
}

bool FluentWebTableView::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button,
                                            int modifiers) {
  if (Widget::mouse_motion_event(p, rel, button, modifiers))
    return true;

  int row = row_at_position(p);
  if (row != m_hover_row) {
    m_hover_row = row;
    if (Screen *scr = screen())
      scr->redraw();
  }
  return false;
}

bool FluentWebTableView::keyboard_event(int key, int scancode, int action, int modifiers) {
#if !defined(NANOGUI_USE_OPENGL)
  (void)key;
  (void)scancode;
  (void)action;
  (void)modifiers;
  return Widget::keyboard_event(key, scancode, action, modifiers);
#else
  if (action != GLFW_PRESS && action != GLFW_REPEAT)
    return Widget::keyboard_event(key, scancode, action, modifiers);
  if (m_rows.empty() || m_selection_mode == SelectionMode::None)
    return false;

  int target = m_active_row;
  if (key == GLFW_KEY_DOWN) {
    target = std::min(static_cast<int>(m_rows.size()) - 1, std::max(0, m_active_row) + 1);
  } else if (key == GLFW_KEY_UP) {
    target = std::max(0, std::min(m_active_row, static_cast<int>(m_rows.size()) - 1) - 1);
  } else if (key == GLFW_KEY_HOME) {
    target = 0;
  } else if (key == GLFW_KEY_END) {
    target = static_cast<int>(m_rows.size()) - 1;
  } else {
    return Widget::keyboard_event(key, scancode, action, modifiers);
  }

  if (target != m_active_row) {
    m_active_row = target;
    if (!(modifiers & kShiftModifierMask)) {
      clear_selection();
      set_selected(target, true);
      m_anchor_row = target;
    }
    if (Screen *scr = screen())
      scr->redraw();
  }
  return true;
#endif
}

bool FluentWebTableView::focus_event(bool focused) {
  if (Screen *scr = screen())
    scr->redraw();
  return Widget::focus_event(focused);
}

void FluentWebTableView::ensure_sort_column(int column, bool ascending) {
  m_sort_column = column;
  m_sort_ascending = ascending;
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebTableView::apply_sort() {
  // User implements sorting via callback
}

void FluentWebTableView::update_focus_row() {
  if (Screen *scr = screen())
    scr->redraw();
}

void FluentWebTableView::draw_focus_ring(NVGcontext *) const {
  // Optional focus ring implementation
}

/* --------------------------- FluentWebDataGrid --------------------------- */

FluentWebDataGrid::FluentWebDataGrid(Widget *parent) : FluentWebTableView(parent) {
  m_show_horizontal_dividers = true;
  m_show_vertical_dividers = true;
  m_use_zebra_striping = true;
}

Color FluentWebDataGrid::row_background_for(size_t index, RowState state, bool selected,
                                            bool hovered) const {
  if (selected)
    return m_selected_background;
  if (hovered)
    return m_hover_background;
  if (state != RowState::Normal)
    return row_state_color(state);
  if (m_use_zebra_striping)
    return (index % 2 == 0) ? m_row_even_background : m_row_odd_background;
  return m_background;
}

NAMESPACE_END(nanogui)
