/*
 * tvgbox2 - TableWidget Implementation
 */

#include <tvgbox2/widgets/table_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <tvgbox2/renderer.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace tvgbox2 {

TableWidget::TableWidget() {}

void TableWidget::add_column(const std::string& header, float width) {
  columns_.push_back({header, width});
  dirty_ = true;
}

void TableWidget::add_row(const std::vector<std::string>& cells) {
  std::vector<Cell> row;
  for (const auto& text : cells) row.push_back({text});
  rows_.push_back(row);
  dirty_ = true;
}

void TableWidget::clear() {
  rows_.clear();
  selected_row_ = -1;
  dirty_ = true;
}

void TableWidget::set_cell(int row, int col, const std::string& value) {
  if (row >= 0 && row < static_cast<int>(rows_.size()) &&
      col >= 0 && col < static_cast<int>(rows_[row].size())) {
    rows_[row][col].text = value;
    dirty_ = true;
  }
}

void TableWidget::set_selected_row(int row) {
  if (row >= -1 && row < static_cast<int>(rows_.size())) {
    selected_row_ = row;
    dirty_ = true;
  }
}

void TableWidget::render(const Element& elem, Renderer& renderer) {
  auto& r = renderer.flex();
  auto* style = elem.computed_style;
  Color bg_color = {1.0f, 1.0f, 1.0f, 1.0f};
  if (style) bg_color = style->get_variable_color("--table-bg", bg_color);

  r.draw_rect(0, 0, elem.width(), elem.height(), 4, Paint::solid(bg_color), Paint::none(), 0);
  r.draw_rect(0, 0, elem.width(), elem.height(), 4,
              Paint::none(), Paint::solid(Color{0.90f, 0.91f, 0.92f, 1.0f}), 1);

  render_header(r, elem);
  render_rows(r, elem);
}

void TableWidget::render_header(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style && style->font_size > 0 ? style->font_size : 14.0f;
  std::string font_family = style && !style->font_family.empty() ? style->font_family : "Arial";
  Color header_bg = {0.98f, 0.98f, 0.98f, 1.0f};
  Color header_text = {0.22f, 0.25f, 0.32f, 1.0f};
  if (style) {
    header_bg = style->get_variable_color("--table-header-bg", header_bg);
    header_text = style->get_variable_color("--table-header-text", header_text);
  }

  r.draw_rect(0, 0, elem.width(), header_height_, 4, Paint::solid(header_bg), Paint::none(), 0);

  float x = 0;
  for (const auto& col : columns_) {
    r.draw_text(col.header, x + 12, header_height_ / 2 + font_size / 3,
                font_family, font_size, false, header_text);

    // Column divider
    char path[64];
    snprintf(path, sizeof(path), "M %.4g 8 L %.4g %.4g", x + col.width, x + col.width, header_height_ - 8);
    r.stroke_path(path, Paint::solid(Color{0.90f, 0.91f, 0.92f, 1.0f}), 1);

    x += col.width;
  }

  // Header bottom line
  char bottom_path[64];
  snprintf(bottom_path, sizeof(bottom_path), "M 0 %.4g L %.4g %.4g", header_height_, elem.width(), header_height_);
  r.stroke_path(bottom_path, Paint::solid(Color{0.90f, 0.91f, 0.92f, 1.0f}), 1);
}

void TableWidget::render_rows(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style && style->font_size > 0 ? style->font_size : 14.0f;
  std::string font_family = style && !style->font_family.empty() ? style->font_family : "Arial";
  Color text_color = {0.0f, 0.0f, 0.0f, 1.0f};
  Color selected_bg = {0.94f, 0.96f, 1.0f, 1.0f};
  Color hover_bg = {0.98f, 0.98f, 0.98f, 1.0f};
  if (style) {
    text_color = style->get_variable_color("--table-text", text_color);
    selected_bg = style->get_variable_color("--table-selected", selected_bg);
    hover_bg = style->get_variable_color("--table-hover", hover_bg);
  }

  float visible_h = elem.height() - header_height_;
  int max_visible = static_cast<int>(visible_h / row_height_) + 1;
  int start_row = static_cast<int>(scroll_y_ / row_height_);

  for (int i = start_row; i < std::min(start_row + max_visible, static_cast<int>(rows_.size())); i++) {
    float y = header_height_ + (i - start_row) * row_height_ - std::fmod(scroll_y_, row_height_);

    if (i == selected_row_) {
      r.draw_rect(0, y, elem.width(), row_height_, 0, Paint::solid(selected_bg), Paint::none(), 0);
    } else if (i == hover_row_) {
      r.draw_rect(0, y, elem.width(), row_height_, 0, Paint::solid(hover_bg), Paint::none(), 0);
    }

    float x = 0;
    for (size_t c = 0; c < columns_.size() && c < rows_[i].size(); c++) {
      r.draw_text(rows_[i][c].text, x + 12, y + row_height_ / 2 + font_size / 3,
                  font_family, font_size, false, text_color);
      x += columns_[c].width;
    }

    // Row divider
    char path[64];
    snprintf(path, sizeof(path), "M 0 %.4g L %.4g %.4g", y + row_height_, elem.width(), y + row_height_);
    r.stroke_path(path, Paint::solid(Color{0.95f, 0.96f, 0.96f, 1.0f}), 1);
  }
}

bool TableWidget::handle_event(const Event& event, Element& elem) {
  float local_y = event.y - elem.absolute_y();

  if (event.type == EventType::MouseDown && local_y > header_height_) {
    int row = static_cast<int>((local_y - header_height_ + scroll_y_) / row_height_);
    if (row >= 0 && row < static_cast<int>(rows_.size())) {
      selected_row_ = row;
      dirty_ = true;
      elem.mark_paint_dirty();
      if (on_select_) on_select_(row);
      return true;
    }
  }

  if (event.type == EventType::MouseMove && local_y > header_height_) {
    int row = static_cast<int>((local_y - header_height_ + scroll_y_) / row_height_);
    if (row != hover_row_) {
      hover_row_ = (row >= 0 && row < static_cast<int>(rows_.size())) ? row : -1;
      elem.mark_paint_dirty();
    }
  }

  if (event.type == EventType::MouseWheel) {
    float max_scroll = std::max(0.0f, rows_.size() * row_height_ - (elem.height() - header_height_));
    scroll_y_ = std::clamp(scroll_y_ - event.delta_y * 30, 0.0f, max_scroll);
    elem.mark_paint_dirty();
    return true;
  }

  return false;
}

void TableWidget::update(float delta_ms, Element& elem) {}

} // namespace tvgbox2
