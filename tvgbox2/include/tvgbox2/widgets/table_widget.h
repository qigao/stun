#ifndef TVGBOX2_TABLE_WIDGET_H
#define TVGBOX2_TABLE_WIDGET_H

#include "../widget.h"
#include <string>
#include <vector>
#include <functional>

namespace tvgbox2 {

class TableWidget : public Widget {
public:
  struct Column { std::string header; float width = 100.0f; };
  struct Cell { std::string text; };

  TableWidget();

  void render(tvg::Scene* scene, const Element& elem, Renderer& renderer) override;
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
  void render_header(tvg::Scene* scene, const Element& elem);
  void render_rows(tvg::Scene* scene, const Element& elem);

  std::vector<Column> columns_;
  std::vector<std::vector<Cell>> rows_;
  int selected_row_ = -1;
  int hover_row_ = -1;
  float scroll_y_ = 0;
  float row_height_ = 40.0f;
  float header_height_ = 44.0f;
  SelectCallback on_select_;
};

} // namespace tvgbox2
#endif
