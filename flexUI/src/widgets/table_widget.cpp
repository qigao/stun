/*
 * flexUI - TableWidget Implementation
 */

#include <flexUI/widgets/table_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/renderer.h>
#include <algorithm>
#include <cmath>
#include <stb_sprintf.h>

namespace flexUI {

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
  
  // Default theme (Dark-ish as fallback)
  Color bg_color = {0.06f, 0.09f, 0.16f, 1.0f}; // slate-900
  Color border_color = {0.28f, 0.33f, 0.41f, 1.0f}; // slate-600
  
  if (style) {
    bg_color = style->get_variable_color("--table-bg", bg_color);
    border_color = style->get_variable_color("--table-border", border_color);
  }

  // Main Background
  r.draw_rect(0, 0, elem.width(), elem.height(), 8, Paint::solid(bg_color), Paint::none(), 0);
  
  // Outer Border
  r.draw_rect(0, 0, elem.width(), elem.height(), 8,
              Paint::none(), Paint::solid(border_color), 1);

  render_header(r, elem);
  render_rows(r, elem);
}

void TableWidget::render_header(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style && style->font_size > 0 ? style->font_size : 13.0f;
  std::string font_family = style && !style->font_family.empty() ? style->font_family : "Arial";
  
  Color header_bg = {0.12f, 0.16f, 0.23f, 1.0f}; // slate-800
  Color header_text = {0.58f, 0.64f, 0.72f, 1.0f}; // slate-400
  Color divider_color = {0.28f, 0.33f, 0.41f, 0.4f}; // transparent slate-600
  
  if (style) {
    header_bg = style->get_variable_color("--table-header-bg", header_bg);
    header_text = style->get_variable_color("--table-header-text", header_text);
    divider_color = style->get_variable_color("--table-divider", divider_color);
  }

  r.save();
  // Clip the whole table area to its rounded bounds if possible, 
  // but at least clip to the header height for now.
  r.clip_rect(0, 0, elem.width(), header_height_);

  // Header background
  r.draw_rect(0, 0, elem.width(), header_height_, 0, Paint::solid(header_bg), Paint::none(), 0);

  float x = -scroll_x_; // Apply horizontal scroll
  for (const auto& col : columns_) {
    // Only draw visible columns optimization (optional but nice)
    if (x + col.width > 0 && x < elem.width()) {
      // Column text (Uppercase look for professional feel)
      std::string header_upper = col.header;
      for (auto & c: header_upper) c = toupper(c);

      // Center text vertically in header
      r.draw_text(header_upper, x + 12, (header_height_ - font_size) / 2.0f,
                  font_family, font_size, true, header_text); // true for bold

      // Column divider (interactive zone hint)
      char path[64];
      stbsp_snprintf(path, sizeof(path), "M %.4g 12 L %.4g %.4g", x + col.width, x + col.width, header_height_ - 12);
      r.stroke_path(path, Paint::solid(divider_color), 1);
    }
    x += col.width;
  }

  // Header bottom border
  char bottom_path[64];
  stbsp_snprintf(bottom_path, sizeof(bottom_path), "M 0 %.4g L %.4g %.4g", header_height_, elem.width(), header_height_);
  r.stroke_path(bottom_path, Paint::solid(divider_color), 1);

  r.restore();
}

void TableWidget::render_rows(flex::Renderer& r, const Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style && style->font_size > 0 ? style->font_size : 14.0f;
  std::string font_family = style && !style->font_family.empty() ? style->font_family : "Arial";
  
  Color text_color = {0.97f, 0.98f, 0.99f, 1.0f}; // slate-50
  Color selected_bg = {0.23f, 0.51f, 0.96f, 0.3f}; // blue-500 @ 0.3
  Color hover_bg = {1.0f, 1.0f, 1.0f, 0.05f}; // white @ 0.05
  Color stripe_bg = {1.0f, 1.0f, 1.0f, 0.02f}; // white @ 0.02
  Color divider_color = {0.28f, 0.33f, 0.41f, 0.3f}; // transparent slate-600
  
  if (style) {
    text_color = style->get_variable_color("--table-text", text_color);
    selected_bg = style->get_variable_color("--table-selected", selected_bg);
    hover_bg = style->get_variable_color("--table-hover", hover_bg);
    stripe_bg = style->get_variable_color("--table-stripe", stripe_bg);
    divider_color = style->get_variable_color("--table-divider", divider_color);
  }

  float visible_h = elem.height() - header_height_;
  int max_visible = static_cast<int>(visible_h / row_height_) + 1;
  int start_row = static_cast<int>(scroll_y_ / row_height_);

  r.save();
  // Clip to the rows area (below header, inside table container)
  r.clip_rect(0, header_height_, elem.width(), visible_h);

  for (int i = start_row; i < (std::min)(start_row + max_visible, static_cast<int>(rows_.size())); i++) {
    float y = header_height_ + (i - start_row) * row_height_ - std::fmod(scroll_y_, row_height_);

    // Row highlights (Stripe -> Hover -> Selected)
    if (i == selected_row_) {
      r.draw_rect(0, y, elem.width(), row_height_, 0, Paint::solid(selected_bg), Paint::none(), 0);
    } else if (i == hover_row_) {
      r.draw_rect(0, y, elem.width(), row_height_, 0, Paint::solid(hover_bg), Paint::none(), 0);
    } else if (i % 2 == 1) {
      r.draw_rect(0, y, elem.width(), row_height_, 0, Paint::solid(stripe_bg), Paint::none(), 0);
    }

    float x = -scroll_x_;
    for (size_t c = 0; c < columns_.size() && c < rows_[i].size(); c++) {
      if (x + columns_[c].width > 0 && x < elem.width()) {
         r.draw_text(rows_[i][c].text, x + 12, y + (row_height_ - font_size) / 2.0f,
                     font_family, font_size, false, text_color);
      }
      x += columns_[c].width;
    }

    // Row divider
    if (i < static_cast<int>(rows_.size()) - 1) {
        char path[64];
        stbsp_snprintf(path, sizeof(path), "M 0 %.4g L %.4g %.4g", y + row_height_, elem.width(), y + row_height_);
        r.stroke_path(path, Paint::solid(divider_color), 1);
    }
  }

  r.restore();
}

bool TableWidget::handle_event(const Event& event, Element& elem) {
  float local_y = event.y - elem.absolute_y();

  if (event.type == EventType::MouseDown) {
    // Check if dragging column separator
    if (local_y <= header_height_) {
        float x = -scroll_x_;
        for (int i = 0; i < (int)columns_.size(); ++i) {
            x += columns_[i].width;
            if (std::abs((elem.absolute_x() + x) - event.x) < 5) { // 5px grab zone
                resizing_col_ = i;
                resize_start_x_ = event.x;
                resize_start_width_ = columns_[i].width;
                return true;
            }
        }
    }
    
    // Check click on row
    if (local_y > header_height_) {
        int row = static_cast<int>((local_y - header_height_ + scroll_y_) / row_height_);
        if (row >= 0 && row < static_cast<int>(rows_.size())) {
            selected_row_ = row;
            dirty_ = true;
            elem.mark_paint_dirty();
            if (on_select_) on_select_(row);
            return true;
        }
    }
  }

  if (event.type == EventType::MouseMove) {
      if (resizing_col_ >= 0) {
          float delta = event.x - resize_start_x_;
          columns_[resizing_col_].width = (std::max)(10.0f, resize_start_width_ + delta);
          dirty_ = true;
          elem.mark_paint_dirty();
          return true;
      }

      if (local_y > header_height_) {
        int row = static_cast<int>((local_y - header_height_ + scroll_y_) / row_height_);
        if (row != hover_row_) {
            hover_row_ = (row >= 0 && row < static_cast<int>(rows_.size())) ? row : -1;
            elem.mark_paint_dirty();
        }
      }
  }
  
  if (event.type == EventType::MouseUp) {
      if (resizing_col_ >= 0) {
          resizing_col_ = -1;
          return true;
      }
  }

  if (event.type == EventType::MouseWheel) {
    if (event.mods & (int)KeyMod::Shift) { // Horizontal scroll
        float total_width = 0;
        for(const auto& c : columns_) total_width += c.width;
        float max_scroll_x = (std::max)(0.0f, total_width - elem.width());
        scroll_x_ = std::clamp(scroll_x_ - event.delta_y * 30, 0.0f, max_scroll_x);
    } else { // Vertical scroll
        float max_scroll = (std::max)(0.0f, rows_.size() * row_height_ - (elem.height() - header_height_));
        scroll_y_ = std::clamp(scroll_y_ - event.delta_y * 30, 0.0f, max_scroll);
    }
    elem.mark_paint_dirty();
    return true;
  }

  return false;
}

void TableWidget::update(float delta_ms, Element& elem) {}

} // namespace flexUI
