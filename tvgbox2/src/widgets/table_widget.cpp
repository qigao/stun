/*
 * tvgbox2 - TableWidget Implementation
 */

#include <tvgbox2/widgets/table_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <algorithm>

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

void TableWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  auto* style = elem.computed_style;
  Color bg_color = {255, 255, 255, 255};
  if (style) bg_color = style->get_variable_color("--table-bg", bg_color);

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, elem.width(), elem.height(), 4, 4);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);
  scene->push(std::move(bg));

  auto border = tvg::Shape::gen();
  border->appendRect(0, 0, elem.width(), elem.height(), 4, 4);
  border->strokeFill(229, 231, 235, 255);
  border->strokeWidth(1);
  scene->push(std::move(border));

  render_header(scene, elem);
  render_rows(scene, elem);
}

void TableWidget::render_header(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style && style->font_size > 0 ? style->font_size : 14.0f;
  std::string font_family = style && !style->font_family.empty() ? style->font_family : "Arial";
  Color header_bg = {249, 250, 251, 255};
  Color header_text = {55, 65, 81, 255};
  if (style) {
    header_bg = style->get_variable_color("--table-header-bg", header_bg);
    header_text = style->get_variable_color("--table-header-text", header_text);
  }

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, elem.width(), header_height_, 4, 4);
  bg->fill(header_bg.r, header_bg.g, header_bg.b, header_bg.a);
  scene->push(std::move(bg));

  float x = 0;
  for (const auto& col : columns_) {
    auto text = tvg::Text::gen();
    text->font(font_family.c_str());
    text->size(font_size);
    text->text(col.header.c_str());
    text->fill(header_text.r, header_text.g, header_text.b);
    text->translate(x + 12, header_height_ / 2 + font_size / 3);
    scene->push(std::move(text));

    auto divider = tvg::Shape::gen();
    divider->moveTo(x + col.width, 8);
    divider->lineTo(x + col.width, header_height_ - 8);
    divider->strokeFill(229, 231, 235, 255);
    divider->strokeWidth(1);
    scene->push(std::move(divider));

    x += col.width;
  }

  auto bottom = tvg::Shape::gen();
  bottom->moveTo(0, header_height_);
  bottom->lineTo(elem.width(), header_height_);
  bottom->strokeFill(229, 231, 235, 255);
  bottom->strokeWidth(1);
  scene->push(std::move(bottom));
}

void TableWidget::render_rows(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style && style->font_size > 0 ? style->font_size : 14.0f;
  std::string font_family = style && !style->font_family.empty() ? style->font_family : "Arial";
  Color text_color = {0, 0, 0, 255};
  Color selected_bg = {239, 246, 255, 255};
  Color hover_bg = {249, 250, 251, 255};
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
      auto bg = tvg::Shape::gen();
      bg->appendRect(0, y, elem.width(), row_height_);
      bg->fill(selected_bg.r, selected_bg.g, selected_bg.b, selected_bg.a);
      scene->push(std::move(bg));
    } else if (i == hover_row_) {
      auto bg = tvg::Shape::gen();
      bg->appendRect(0, y, elem.width(), row_height_);
      bg->fill(hover_bg.r, hover_bg.g, hover_bg.b, hover_bg.a);
      scene->push(std::move(bg));
    }

    float x = 0;
    for (size_t c = 0; c < columns_.size() && c < rows_[i].size(); c++) {
      auto text = tvg::Text::gen();
      text->font(font_family.c_str());
      text->size(font_size);
      text->text(rows_[i][c].text.c_str());
      text->fill(text_color.r, text_color.g, text_color.b);
      text->translate(x + 12, y + row_height_ / 2 + font_size / 3);
      scene->push(std::move(text));
      x += columns_[c].width;
    }

    auto divider = tvg::Shape::gen();
    divider->moveTo(0, y + row_height_);
    divider->lineTo(elem.width(), y + row_height_);
    divider->strokeFill(243, 244, 246, 255);
    divider->strokeWidth(1);
    scene->push(std::move(divider));
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
