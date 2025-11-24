#pragma once

#include <nanogui/widget.h>
#include <nanogui/vector.h>

#include <functional>
#include <string>
#include <vector>

NAMESPACE_BEGIN(nanogui)

class FluentWebTheme;

/**
 * Fluent 2 table shell with sortable columns and selectable rows.
 */
class NANOGUI_EXPORT FluentWebTableView : public Widget {
public:
  enum class SelectionMode { None, Single, Multiple };

  enum class TextAlign { Start, Center, End };

  enum class RowState { Normal, Success, Warning, Error, Informational };

  struct Column {
    std::string title;
    float weight;
    int min_width;
    TextAlign align;
    bool sortable;

    Column(const std::string &t, float w = 1.f, int min = 96,
           TextAlign alignment = TextAlign::Start, bool is_sortable = true)
        : title(t), weight(std::max(w, 0.f)), min_width(min), align(alignment),
          sortable(is_sortable) {}
  };

  struct Row {
    std::vector<std::string> cells;
    RowState state = RowState::Normal;
  };

  explicit FluentWebTableView(Widget *parent);

  int add_column(const Column &column);
  void clear_columns();
  void set_columns(const std::vector<Column> &columns);
  const std::vector<Column> &columns() const { return m_columns; }

  int add_row(const std::vector<std::string> &cells, RowState state = RowState::Normal);
  void set_rows(const std::vector<Row> &rows);
  const std::vector<Row> &rows() const { return m_rows; }
  void clear_rows();

  void set_row_state(size_t index, RowState state);

  void set_selection_mode(SelectionMode mode);
  SelectionMode selection_mode() const { return m_selection_mode; }
  const std::vector<int> &selected_rows() const { return m_selected_rows; }
  void set_selected(int index, bool selected);
  void clear_selection();

  void set_selection_callback(const std::function<void(const std::vector<int> &)> &cb) {
    m_selection_callback = cb;
  }
  void set_sort_callback(const std::function<void(int, bool)> &cb) { m_sort_callback = cb; }

  void set_theme(Theme *theme) override;
  Vector2i preferred_size_impl(NVGcontext *ctx) const override;
  void draw(NVGcontext *ctx) override;
  bool mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) override;
  bool mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button,
                          int modifiers) override;
  bool keyboard_event(int key, int scancode, int action, int modifiers) override;
  bool focus_event(bool focused) override;

protected:
  void refresh_tokens();
  void ensure_sort_column(int column, bool ascending);
  void apply_sort();
  void update_focus_row();

  virtual Color row_background_for(size_t index, RowState state, bool selected, bool hovered) const;
  Color row_state_color(RowState state) const;

  void draw_header(NVGcontext *ctx, const std::vector<float> &col_widths) const;
  void draw_rows(NVGcontext *ctx, const std::vector<float> &col_widths) const;
  void draw_focus_ring(NVGcontext *ctx) const;

  std::vector<float> compute_column_widths(float available_width) const;
  int row_at_position(const Vector2i &p) const;
  int column_at_position(const Vector2i &p) const;

  std::vector<Column> m_columns;
  std::vector<Row> m_rows;
  std::vector<int> m_selected_rows;
  SelectionMode m_selection_mode;
  int m_active_row;
  int m_anchor_row;
  int m_hover_row;
  int m_sort_column;
  bool m_sort_ascending;

  bool m_show_horizontal_dividers;
  bool m_show_vertical_dividers;
  bool m_use_zebra_striping;

  std::function<void(const std::vector<int> &)> m_selection_callback;
  std::function<void(int, bool)> m_sort_callback;

  // Cached colors and metrics from Fluent tokens
  Color m_background;
  Color m_border_color;
  Color m_header_background;
  Color m_header_text;
  Color m_cell_text;
  Color m_cell_meta_text;
  Color m_hover_background;
  Color m_selected_background;
  Color m_selected_border;
  Color m_grid_color;
  Color m_focus_outer;
  Color m_focus_inner;
  Color m_state_success;
  Color m_state_warning;
  Color m_state_error;
  Color m_state_info;
  Color m_row_even_background;
  Color m_row_odd_background;

  int m_corner_radius;
  int m_header_height;
  int m_row_height;
  int m_cell_padding;
};

/**
 * Fluent 2 data grid shell with zebra striping and grid lines.
 */
class NANOGUI_EXPORT FluentWebDataGrid : public FluentWebTableView {
public:
  explicit FluentWebDataGrid(Widget *parent);

protected:
  Color row_background_for(size_t index, RowState state, bool selected,
                           bool hovered) const override;
};

NAMESPACE_END(nanogui)
