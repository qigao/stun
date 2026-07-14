/*
 * flexUI - TableWidget Implementation
 */

#include <flexUI/widgets/table_widget.h>
#include <flexUI/computed_style.h>
#include <flexUI/detail/css_render_transform.h>
#include <flexUI/element.h>
#include <flexUI/event.h>
#include <flexUI/render_command.h>
#include <flexUI/text_layout.h>
#include <algorithm>
#include <cmath>
#include <stb_sprintf.h>

namespace flexUI {

namespace {

void sync_table_host(Element* host, size_t column_count, size_t row_count,
                     int selected_row) {
  if (!host) {
    return;
  }

  const bool has_selection =
      selected_row >= 0 && static_cast<size_t>(selected_row) < row_count;

  host->set_attribute("role", "table");
  host->set_attribute("data-state", row_count == 0 || column_count == 0
                                        ? "empty"
                                        : has_selection ? "selected" : "idle");
  host->set_attribute("data-column-count", std::to_string(column_count));
  host->set_attribute("data-row-count", std::to_string(row_count));
  host->set_attribute("data-selected-count", has_selection ? "1" : "0");
  host->set_attribute("aria-colcount", std::to_string(column_count));
  host->set_attribute("aria-rowcount", std::to_string(row_count));

  if (has_selection) {
    host->set_attribute("data-selected-row", std::to_string(selected_row));
  } else {
    host->remove_attribute("data-selected-row");
  }

  host->set_state("selected", has_selection);
}

} // namespace

TableWidget::TableWidget() {}

void TableWidget::invalidate_render_cache() {
  ++render_revision_;
  render_cache_valid_ = false;
  dirty_ = true;
}

void TableWidget::add_column(const std::string& header, float width) {
  columns_.push_back({header, width});
  invalidate_render_cache();
  sync_table_host(host_element(), columns_.size(), rows_.size(), selected_row_);
}

void TableWidget::add_row(const std::vector<std::string>& cells) {
  std::vector<Cell> row;
  for (const auto& text : cells) row.push_back({text});
  rows_.push_back(row);
  invalidate_render_cache();
  sync_table_host(host_element(), columns_.size(), rows_.size(), selected_row_);
}

void TableWidget::clear() {
  rows_.clear();
  selected_row_ = -1;
  hover_row_ = -1;
  invalidate_render_cache();
  sync_table_host(host_element(), columns_.size(), rows_.size(), selected_row_);
}

void TableWidget::set_cell(int row, int col, const std::string& value) {
  if (row >= 0 && row < static_cast<int>(rows_.size()) &&
      col >= 0 && col < static_cast<int>(rows_[row].size())) {
    rows_[row][col].text = value;
    invalidate_render_cache();
  }
}

void TableWidget::set_selected_row(int row) {
  if (row >= -1 && row < static_cast<int>(rows_.size())) {
    selected_row_ = row;
    invalidate_render_cache();
    sync_table_host(host_element(), columns_.size(), rows_.size(), selected_row_);
  }
}

bool TableWidget::render_cache_matches(const Element& elem) const {
  if (!render_cache_valid_ || cached_revision_ != render_revision_ ||
      cached_width_ != elem.width() || cached_height_ != elem.height() ||
      cached_scroll_x_ != scroll_x_ || cached_scroll_y_ != scroll_y_ ||
      cached_selected_row_ != selected_row_ || cached_hover_row_ != hover_row_) {
    return false;
  }

  auto* style = elem.computed_style;
  const float font_size = style ? style->font_size : 0.0f;
  const std::string font_family = style ? style->font_family : std::string();
  const int font_weight =
      style ? static_cast<int>(style->font_weight) : 0;
  const int font_style =
      style ? static_cast<int>(style->font_style) : 0;
  const int text_align =
      style ? static_cast<int>(style->text_align) : 0;
  const int text_transform =
      style ? static_cast<int>(style->text_transform) : 0;
  const int direction =
      style ? static_cast<int>(style->direction) : 0;
  const float letter_spacing = style ? style->letter_spacing : 0.0f;
  const float word_spacing = style ? style->word_spacing : 0.0f;
  const float text_indent = style ? style->text_indent : 0.0f;
  const float tab_size = style ? style->tab_size : 8.0f;
  if (cached_font_size_ != font_size || cached_font_family_ != font_family ||
      cached_font_weight_ != font_weight ||
      cached_font_style_ != font_style ||
      cached_text_align_ != text_align ||
      cached_text_transform_ != text_transform ||
      cached_direction_ != direction ||
      cached_letter_spacing_ != letter_spacing ||
      cached_word_spacing_ != word_spacing ||
      cached_text_indent_ != text_indent ||
      cached_tab_size_ != tab_size) {
    return false;
  }
  if (!style) {
    return cached_table_bg_.empty() && cached_table_border_.empty() &&
           cached_header_bg_.empty() && cached_header_text_.empty() &&
           cached_table_text_.empty() && cached_selected_bg_.empty() &&
           cached_hover_bg_.empty() && cached_stripe_bg_.empty() &&
           cached_divider_.empty() && cached_white_space_.empty() &&
           cached_text_overflow_.empty();
  }
  return cached_table_bg_ == style->get_variable(Symbol("--table-bg"), "") &&
         cached_table_border_ == style->get_variable(Symbol("--table-border"), "") &&
         cached_header_bg_ == style->get_variable(Symbol("--table-header-bg"), "") &&
         cached_header_text_ == style->get_variable(Symbol("--table-header-text"), "") &&
         cached_table_text_ == style->get_variable(Symbol("--table-text"), "") &&
         cached_selected_bg_ == style->get_variable(Symbol("--table-selected"), "") &&
         cached_hover_bg_ == style->get_variable(Symbol("--table-hover"), "") &&
         cached_stripe_bg_ == style->get_variable(Symbol("--table-stripe"), "") &&
         cached_divider_ == style->get_variable(Symbol("--table-divider"), "") &&
         cached_white_space_ == style->get_variable(Symbol("--white-space"), "") &&
         cached_text_overflow_ == style->get_variable(Symbol("--text-overflow"), "");
}

void TableWidget::update_render_cache_key(const Element& elem) {
  render_cache_valid_ = true;
  dirty_ = false;
  cached_revision_ = render_revision_;
  cached_width_ = elem.width();
  cached_height_ = elem.height();
  cached_scroll_x_ = scroll_x_;
  cached_scroll_y_ = scroll_y_;
  cached_selected_row_ = selected_row_;
  cached_hover_row_ = hover_row_;

  auto* style = elem.computed_style;
  cached_font_size_ = style ? style->font_size : 0.0f;
  cached_font_weight_ = style ? static_cast<int>(style->font_weight) : 0;
  cached_font_style_ = style ? static_cast<int>(style->font_style) : 0;
  cached_text_align_ = style ? static_cast<int>(style->text_align) : 0;
  cached_text_transform_ =
      style ? static_cast<int>(style->text_transform) : 0;
  cached_direction_ = style ? static_cast<int>(style->direction) : 0;
  cached_letter_spacing_ = style ? style->letter_spacing : 0.0f;
  cached_word_spacing_ = style ? style->word_spacing : 0.0f;
  cached_text_indent_ = style ? style->text_indent : 0.0f;
  cached_tab_size_ = style ? style->tab_size : 8.0f;
  cached_font_family_ = style ? style->font_family : std::string();
  cached_table_bg_ = style ? style->get_variable(Symbol("--table-bg"), "") : "";
  cached_table_border_ =
      style ? style->get_variable(Symbol("--table-border"), "") : "";
  cached_header_bg_ =
      style ? style->get_variable(Symbol("--table-header-bg"), "") : "";
  cached_header_text_ =
      style ? style->get_variable(Symbol("--table-header-text"), "") : "";
  cached_table_text_ = style ? style->get_variable(Symbol("--table-text"), "") : "";
  cached_selected_bg_ =
      style ? style->get_variable(Symbol("--table-selected"), "") : "";
  cached_hover_bg_ = style ? style->get_variable(Symbol("--table-hover"), "") : "";
  cached_stripe_bg_ =
      style ? style->get_variable(Symbol("--table-stripe"), "") : "";
  cached_divider_ = style ? style->get_variable(Symbol("--table-divider"), "") : "";
  cached_white_space_ =
      style ? style->get_variable(Symbol("--white-space"), "") : "";
  cached_text_overflow_ =
      style ? style->get_variable(Symbol("--text-overflow"), "") : "";
}

void TableWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  if (render_cache_matches(elem)) {
    commands.append(render_cache_);
    return;
  }

  RenderCommandList rebuilt(commands.capabilities());
  auto* style = elem.computed_style;

  // Default theme (Dark-ish as fallback)
  Color bg_color = {0.06f, 0.09f, 0.16f, 1.0f}; // slate-900
  Color border_color = {0.28f, 0.33f, 0.41f, 1.0f}; // slate-600
  
  if (style) {
    bg_color = style->get_variable_color("--table-bg", bg_color);
    border_color = style->get_variable_color("--table-border", border_color);
  }

  rebuilt.draw_rect(0, 0, elem.width(), elem.height(), 8,
                    Paint::solid(bg_color), Paint::none(), 0);
  rebuilt.draw_rect(0, 0, elem.width(), elem.height(), 8,
                    Paint::none(), Paint::solid(border_color), 1);

  render_header(rebuilt, elem);
  render_rows(rebuilt, elem);

  render_cache_ = rebuilt.commands();
  update_render_cache_key(elem);
  commands.append(render_cache_);
}

void TableWidget::render_header(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  
  Color header_bg = {0.12f, 0.16f, 0.23f, 1.0f}; // slate-800
  Color header_text = {0.58f, 0.64f, 0.72f, 1.0f}; // slate-400
  Color divider_color = {0.28f, 0.33f, 0.41f, 0.4f}; // transparent slate-600
  
  if (style) {
    header_bg = style->get_variable_color("--table-header-bg", header_bg);
    header_text = style->get_variable_color("--table-header-text", header_text);
    divider_color = style->get_variable_color("--table-divider", divider_color);
  }

  commands.save();
  commands.clip_rect(0, 0, elem.width(), header_height_);
  commands.draw_rect(0, 0, elem.width(), header_height_, 0,
                     Paint::solid(header_bg), Paint::none(), 0);

  float x = -scroll_x_; // Apply horizontal scroll
  for (const auto& col : columns_) {
    // Only draw visible columns optimization (optional but nice)
    if (x + col.width > 0 && x < elem.width()) {
      // Column text (Uppercase look for professional feel)
      std::string header_upper = col.header;
      for (auto& c : header_upper) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      }

      // Center text vertically in header
      if (style) {
        const auto text_block = layout_text_block(
            style, header_upper, x + 12.0f, 0.0f,
            std::max(0.0f, col.width - 24.0f), header_height_, header_text,
            TextVerticalAlign::Middle);
        emit_text_block(commands, text_block);
      }

      // Column divider (interactive zone hint)
      char path[64];
      stbsp_snprintf(path, sizeof(path), "M %.4g 12 L %.4g %.4g", x + col.width, x + col.width, header_height_ - 12);
      commands.stroke_path(path, Paint::solid(divider_color), 1);
    }
    x += col.width;
  }

  // Header bottom border
  char bottom_path[64];
  stbsp_snprintf(bottom_path, sizeof(bottom_path), "M 0 %.4g L %.4g %.4g", header_height_, elem.width(), header_height_);
  commands.stroke_path(bottom_path, Paint::solid(divider_color), 1);
  commands.restore();
}

void TableWidget::render_rows(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  
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

  commands.save();
  commands.clip_rect(0, header_height_, elem.width(), visible_h);

  for (int i = start_row; i < (std::min)(start_row + max_visible, static_cast<int>(rows_.size())); i++) {
    float y = header_height_ + (i - start_row) * row_height_ - std::fmod(scroll_y_, row_height_);

    // Row highlights (Stripe -> Hover -> Selected)
    if (i == selected_row_) {
      commands.draw_rect(0, y, elem.width(), row_height_, 0,
                         Paint::solid(selected_bg), Paint::none(), 0);
    } else if (i == hover_row_) {
      commands.draw_rect(0, y, elem.width(), row_height_, 0,
                         Paint::solid(hover_bg), Paint::none(), 0);
    } else if (i % 2 == 1) {
      commands.draw_rect(0, y, elem.width(), row_height_, 0,
                         Paint::solid(stripe_bg), Paint::none(), 0);
    }

    float x = -scroll_x_;
    for (size_t c = 0; c < columns_.size() && c < rows_[i].size(); c++) {
      if (x + columns_[c].width > 0 && x < elem.width()) {
         if (style) {
           const auto text_block = layout_text_block(
               style, rows_[i][c].text, x + 12.0f, y,
               std::max(0.0f, columns_[c].width - 24.0f), row_height_, text_color,
               TextVerticalAlign::Middle);
           emit_text_block(commands, text_block);
         }
      }
      x += columns_[c].width;
    }

    // Row divider
    if (i < static_cast<int>(rows_.size()) - 1) {
        char path[64];
        stbsp_snprintf(path, sizeof(path), "M 0 %.4g L %.4g %.4g", y + row_height_, elem.width(), y + row_height_);
        commands.stroke_path(path, Paint::solid(divider_color), 1);
    }
  }

  commands.restore();
}

bool TableWidget::handle_event(const Event& event, Element& elem) {
  const flex::Vec2 local_pos =
      detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
  float local_x = local_pos.x;
  float local_y = local_pos.y;

  if (event.type == EventType::MouseDown) {
    // Check if dragging column separator
    if (local_y <= header_height_) {
        float x = -scroll_x_;
        for (int i = 0; i < (int)columns_.size(); ++i) {
            x += columns_[i].width;
            if (std::abs(x - local_x) < 5) { // 5px grab zone
            resizing_col_ = i;
            resize_start_x_ = local_x;
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
            invalidate_render_cache();
            sync_table_host(host_element(), columns_.size(), rows_.size(),
                            selected_row_);
            elem.mark_paint_dirty();
            if (on_select_) on_select_(row);
            return true;
        }
    }
  }

  if (event.type == EventType::MouseMove) {
      if (resizing_col_ >= 0) {
          float delta = local_x - resize_start_x_;
          columns_[resizing_col_].width = (std::max)(10.0f, resize_start_width_ + delta);
          invalidate_render_cache();
          elem.mark_paint_dirty();
          return true;
      }

      if (local_y > header_height_) {
        int row = static_cast<int>((local_y - header_height_ + scroll_y_) / row_height_);
        if (row != hover_row_) {
            hover_row_ = (row >= 0 && row < static_cast<int>(rows_.size())) ? row : -1;
            invalidate_render_cache();
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
    invalidate_render_cache();
    elem.mark_paint_dirty();
    return true;
  }

  return false;
}

void TableWidget::update(float delta_ms, Element& elem) {}

} // namespace flexUI
