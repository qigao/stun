/*
 * flexUI - TableWidget
 *
 * Data table - 使用 RenderCommandList 渲染
 */

#ifndef FLEXUI_TABLE_WIDGET_H
#define FLEXUI_TABLE_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../render_command.h"
#include "../shapes.h"
#include <string>
#include <vector>
#include <functional>

namespace flexUI {

class RenderCommandList;

class TableWidget : public Widget {
public:
  struct Column { std::string header; float width = 100.0f; };
  struct Cell { std::string text; };

  TableWidget();

  void emit_render_commands(const Element& elem, RenderCommandList& commands) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  bool needs_frame_update(const Element& elem) const override {
    (void)elem;
    return false;
  }
  bool state_affects_paint(Symbol state) const override {
    (void)state;
    return false;
  }
  const char* type_name() const override { return "TableWidget"; }
  bool paints_host_box() const override { return true; }

  void add_column(const std::string& header, float width = 100.0f);
  void add_row(const std::vector<std::string>& cells);
  void clear();
  void set_cell(int row, int col, const std::string& value);

  int selected_row() const { return selected_row_; }
  void set_selected_row(int row);

  using SelectCallback = std::function<void(int row)>;
  void set_select_callback(SelectCallback cb) { on_select_ = std::move(cb); }

private:
  void render_header(RenderCommandList& commands, const Element& elem);
  void render_rows(RenderCommandList& commands, const Element& elem);
  bool render_cache_matches(const Element& elem) const;
  void update_render_cache_key(const Element& elem);
  void invalidate_render_cache();

  std::vector<Column> columns_;
  std::vector<std::vector<Cell>> rows_;
  int selected_row_ = -1;
  int hover_row_ = -1;
  float scroll_y_ = 0;
  float scroll_x_ = 0;
  float row_height_ = 40.0f;
  float header_height_ = 44.0f;
  
  // Resizing state
  int resizing_col_ = -1;
  float resize_start_x_ = 0;
  float resize_start_width_ = 0;
  SelectCallback on_select_;

  bool render_cache_valid_ = false;
  uint64_t render_revision_ = 0;
  uint64_t cached_revision_ = 0;
  float cached_width_ = 0.0f;
  float cached_height_ = 0.0f;
  float cached_scroll_x_ = 0.0f;
  float cached_scroll_y_ = 0.0f;
  int cached_selected_row_ = -1;
  int cached_hover_row_ = -1;
  float cached_font_size_ = 0.0f;
  int cached_font_weight_ = 0;
  int cached_font_style_ = 0;
  int cached_text_align_ = 0;
  int cached_text_transform_ = 0;
  int cached_direction_ = 0;
  float cached_letter_spacing_ = 0.0f;
  float cached_word_spacing_ = 0.0f;
  float cached_text_indent_ = 0.0f;
  float cached_tab_size_ = 8.0f;
  std::string cached_font_family_;
  std::string cached_table_bg_;
  std::string cached_table_border_;
  std::string cached_header_bg_;
  std::string cached_header_text_;
  std::string cached_table_text_;
  std::string cached_selected_bg_;
  std::string cached_hover_bg_;
  std::string cached_stripe_bg_;
  std::string cached_divider_;
  std::string cached_white_space_;
  std::string cached_text_overflow_;
  std::vector<RenderCommand> render_cache_;
};

} // namespace flexUI

#endif // FLEXUI_TABLE_WIDGET_H
