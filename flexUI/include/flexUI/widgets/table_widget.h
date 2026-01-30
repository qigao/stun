/*
 * flexUI - TableWidget
 *
 * Data table - 使用 flex::Renderer 渲染
 */

#ifndef FLEXUI_TABLE_WIDGET_H
#define FLEXUI_TABLE_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <vector>
#include <functional>

namespace flexUI {

class TableWidget : public Widget {
public:
  struct Column { std::string header; float width = 100.0f; };
  struct Cell { std::string text; };

  TableWidget();

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "TableWidget"; }

  void add_column(const std::string& header, float width = 100.0f);
  void add_row(const std::vector<std::string>& cells);
  void clear();
  void set_cell(int row, int col, const std::string& value);

  int selected_row() const { return selected_row_; }
  void set_selected_row(int row);

  using SelectCallback = std::function<void(int row)>;
  void set_select_callback(SelectCallback cb) { on_select_ = std::move(cb); }

private:
  void render_header(flex::Renderer& r, const Element& elem);
  void render_rows(flex::Renderer& r, const Element& elem);

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
};

} // namespace flexUI

#endif // FLEXUI_TABLE_WIDGET_H
