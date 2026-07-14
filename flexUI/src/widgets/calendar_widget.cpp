/*
 * flexUI - CalendarWidget Implementation
 */

#include <flexUI/widgets/calendar_widget.h>
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

static const char* MONTH_NAMES[] = {
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December"
};

static const char* WEEKDAY_NAMES[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

namespace {

struct CalendarGridMetrics {
  float header_h = 48.0f;
  float weekday_h = 32.0f;
  float start_y = 80.0f;
  float cell_w = 0.0f;
  float cell_h = 0.0f;
  float padding = 12.0f;
};

CalendarGridMetrics calendar_grid_metrics(const Element& elem) {
  CalendarGridMetrics metrics;
  metrics.start_y = metrics.header_h + metrics.weekday_h;
  metrics.cell_w = elem.width() / 7.0f;
  metrics.cell_h = (elem.height() - metrics.start_y) / 6.0f;
  return metrics;
}

} // namespace

CalendarWidget::CalendarWidget() {
  view_.year = 2024;
  view_.month = 1;
  view_.day = 1;
  selected_ = view_;
}

std::string CalendarWidget::format_date(const Date& date) const {
  char buffer[32];
  stbsp_snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d", date.year, date.month,
                 date.day);
  return buffer;
}

std::string CalendarWidget::format_view_label() const {
  return std::string(MONTH_NAMES[view_.month - 1]) + " " + std::to_string(view_.year);
}

void CalendarWidget::sync_host_semantics() {
  set_host_attribute("role", "grid");
  set_host_attribute("aria-label", format_view_label());
  set_host_attribute("data-value", format_date(selected_));
  set_host_attribute("data-view-month", std::to_string(view_.month));
  set_host_attribute("data-view-year", std::to_string(view_.year));
}

void CalendarWidget::set_selected_date(const Date& date) {
  selected_ = date;
  sync_host_semantics();
  dirty_ = true;
}
void CalendarWidget::set_view_date(const Date& date) {
  view_ = date;
  static_cache_valid_ = false;
  sync_host_semantics();
  dirty_ = true;
}

void CalendarWidget::prev_month() {
  if (--view_.month < 1) { view_.month = 12; view_.year--; }
  static_cache_valid_ = false;
  sync_host_semantics();
  dirty_ = true;
}

void CalendarWidget::next_month() {
  if (++view_.month > 12) { view_.month = 1; view_.year++; }
  static_cache_valid_ = false;
  sync_host_semantics();
  dirty_ = true;
}

void CalendarWidget::prev_year() {
  view_.year--;
  static_cache_valid_ = false;
  sync_host_semantics();
  dirty_ = true;
}

void CalendarWidget::next_year() {
  view_.year++;
  static_cache_valid_ = false;
  sync_host_semantics();
  dirty_ = true;
}

int CalendarWidget::days_in_month(int year, int month) const {
  static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  int d = days[month - 1];
  if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) d = 29;
  return d;
}

int CalendarWidget::day_of_week(int year, int month, int day) const {
  if (month < 3) { month += 12; year--; }
  int k = year % 100, j = year / 100;
  int h = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
  return ((h + 6) % 7);
}

void CalendarWidget::emit_render_commands(const Element& elem, RenderCommandList& commands) {
  sync_host_semantics();
  if (!static_cache_matches(elem)) {
    RenderCommandList static_commands(commands.capabilities());
    render_static_layer(static_commands, elem);
    static_commands_ = static_commands.commands();
    update_static_cache_key(elem);
  }
  commands.append(static_commands_);
  render_selected_day(commands, elem);
}

bool CalendarWidget::static_cache_matches(const Element& elem) const {
  if (!static_cache_valid_ || cached_year_ != view_.year ||
      cached_month_ != view_.month || cached_width_ != elem.width() ||
      cached_height_ != elem.height()) {
    return false;
  }
  auto* style = elem.computed_style;
  const float font_size = style ? style->font_size : 0.0f;
  const std::string font_family = style ? style->font_family : std::string();
  return cached_font_size_ == font_size && cached_font_family_ == font_family;
}

void CalendarWidget::update_static_cache_key(const Element& elem) {
  static_cache_valid_ = true;
  cached_year_ = view_.year;
  cached_month_ = view_.month;
  cached_width_ = elem.width();
  cached_height_ = elem.height();
  auto* style = elem.computed_style;
  cached_font_size_ = style ? style->font_size : 0.0f;
  cached_font_family_ = style ? style->font_family : std::string();
}

void CalendarWidget::render_static_layer(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  Color bg_color = {1.0f, 1.0f, 1.0f, 1.0f};
  if (style) bg_color = style->get_variable_color("--calendar-bg", bg_color);
  commands.draw_rect(0, 0, elem.width(), elem.height(), 8,
                     Paint::solid(bg_color), Paint::none(), 0);

  render_header(commands, elem);
  render_weekdays(commands, elem);
  render_day_labels(commands, elem);
}

void CalendarWidget::render_header(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  float header_h = 48.0f, padding = 12.0f;

  // Prev arrow (filled triangle)
  char prev_path[128];
  stbsp_snprintf(prev_path, sizeof(prev_path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g Z",
           padding + 14, header_h / 2 - 6,
           padding + 14, header_h / 2 + 6,
           padding + 8, header_h / 2);

  // Next arrow (filled triangle)
  float nx = elem.width() - padding - 8;
  char next_path[128];
  stbsp_snprintf(next_path, sizeof(next_path), "M %.4g %.4g L %.4g %.4g L %.4g %.4g Z",
           nx - 6, header_h / 2 - 6,
           nx - 6, header_h / 2 + 6,
           nx, header_h / 2);

  commands.fill_path(prev_path, Paint::solid(Color{0.39f, 0.39f, 0.39f, 1.0f}));
  commands.fill_path(next_path, Paint::solid(Color{0.39f, 0.39f, 0.39f, 1.0f}));

  std::string month_year = format_view_label();
  if (style) {
    const auto text_block = layout_text_block(
        style, month_year, 40.0f, 0.0f, std::max(0.0f, elem.width() - 80.0f),
        header_h, Color{0.0f, 0.0f, 0.0f, 1.0f}, TextVerticalAlign::Middle);
    emit_text_block(commands, text_block);
  }
}

void CalendarWidget::render_weekdays(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  float header_h = 48.0f, weekday_h = 32.0f, cell_w = elem.width() / 7;
  ComputedStyle text_style;
  const ComputedStyle* text_style_ptr = style;
  if (style) {
    text_style = *style;
    text_style.text_align = TextAlign::Center;
    text_style_ptr = &text_style;
  }

  for (int i = 0; i < 7; i++) {
    if (text_style_ptr) {
      const auto text_block = layout_text_block(
          text_style_ptr, WEEKDAY_NAMES[i], i * cell_w, header_h, cell_w, weekday_h,
          Color{0.39f, 0.39f, 0.39f, 1.0f}, TextVerticalAlign::Middle);
      emit_text_block(commands, text_block);
    }
  }
}

void CalendarWidget::render_day_labels(RenderCommandList& commands, const Element& elem) {
  auto* style = elem.computed_style;
  const auto metrics = calendar_grid_metrics(elem);
  int first_day = day_of_week(view_.year, view_.month, 1);
  int days = days_in_month(view_.year, view_.month);
  ComputedStyle text_style;
  const ComputedStyle* text_style_ptr = style;
  if (style) {
    text_style = *style;
    text_style.text_align = TextAlign::Center;
    text_style_ptr = &text_style;
  }

  for (int d = 1; d <= days; d++) {
    int pos = first_day + d - 1, row = pos / 7, col = pos % 7;

    if (text_style_ptr) {
      const auto text_block = layout_text_block(
          text_style_ptr, std::to_string(d), col * metrics.cell_w,
          metrics.start_y + row * metrics.cell_h, metrics.cell_w,
          metrics.cell_h, Color{0.0f, 0.0f, 0.0f, 1.0f},
          TextVerticalAlign::Middle);
      emit_text_block(commands, text_block);
    }
  }
}

void CalendarWidget::render_selected_day(RenderCommandList& commands, const Element& elem) {
  if (selected_.year != view_.year || selected_.month != view_.month) {
    return;
  }

  auto* style = elem.computed_style;
  const auto metrics = calendar_grid_metrics(elem);
  const int days = days_in_month(view_.year, view_.month);
  if (selected_.day < 1 || selected_.day > days) {
    return;
  }

  Color selected_bg = {0.23f, 0.51f, 0.96f, 1.0f};
  if (style) selected_bg = style->get_variable_color("--calendar-selected", selected_bg);

  const int first_day = day_of_week(view_.year, view_.month, 1);
  const int pos = first_day + selected_.day - 1;
  const int row = pos / 7;
  const int col = pos % 7;
  const float cx = col * metrics.cell_w + metrics.cell_w / 2;
  const float cy = metrics.start_y + row * metrics.cell_h + metrics.cell_h / 2;
  const float radius = std::min(metrics.cell_w, metrics.cell_h) * 0.4f;
  commands.draw_circle(cx, cy, radius, Paint::solid(selected_bg), Paint::none(), 0);

  ComputedStyle text_style;
  const ComputedStyle* text_style_ptr = style;
  if (style) {
    text_style = *style;
    text_style.text_align = TextAlign::Center;
    text_style_ptr = &text_style;
  }
  if (text_style_ptr) {
    const auto text_block = layout_text_block(
        text_style_ptr, std::to_string(selected_.day), col * metrics.cell_w,
        metrics.start_y + row * metrics.cell_h, metrics.cell_w, metrics.cell_h,
        Color{1.0f, 1.0f, 1.0f, 1.0f}, TextVerticalAlign::Middle);
    emit_text_block(commands, text_block);
  }
}

bool CalendarWidget::handle_event(const Event& event, Element& elem) {
  const auto metrics = calendar_grid_metrics(elem);
  const flex::Vec2 local_pos =
      detail::css_render_to_local(&elem, flex::Vec2(event.x, event.y));
  float local_x = local_pos.x, local_y = local_pos.y;

  if (event.type == EventType::MouseDown) {
    if (local_y < metrics.header_h) {
      if (local_x < metrics.padding + 24) { prev_month(); elem.mark_paint_dirty(); return true; }
      if (local_x > elem.width() - metrics.padding - 24) { next_month(); elem.mark_paint_dirty(); return true; }
    }
    if (local_x >= 0.0f && local_x < elem.width() &&
        local_y >= metrics.start_y && local_y < elem.height()) {
      int first_day = day_of_week(view_.year, view_.month, 1);
      int days = days_in_month(view_.year, view_.month);
      int col = static_cast<int>(local_x / metrics.cell_w);
      int row = static_cast<int>((local_y - metrics.start_y) / metrics.cell_h);
      int day = row * 7 + col - first_day + 1;
      if (day >= 1 && day <= days) {
        selected_.year = view_.year; selected_.month = view_.month; selected_.day = day;
        sync_host_semantics();
        dirty_ = true; elem.mark_paint_dirty();
        if (on_select_) on_select_(selected_);
        return true;
      }
    }
  }
  return false;
}

void CalendarWidget::update(float delta_ms, Element& elem) {}

} // namespace flexUI
