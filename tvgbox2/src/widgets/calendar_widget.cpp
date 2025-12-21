/*
 * tvgbox2 - CalendarWidget Implementation
 */

#include <tvgbox2/widgets/calendar_widget.h>
#include <tvgbox2/computed_style.h>
#include <tvgbox2/element.h>
#include <tvgbox2/event.h>
#include <thorvg.h>
#include <algorithm>
#include <cmath>

namespace tvgbox2 {

static const char* MONTH_NAMES[] = {
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December"
};

static const char* WEEKDAY_NAMES[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

CalendarWidget::CalendarWidget() {
  view_.year = 2024;
  view_.month = 1;
  view_.day = 1;
  selected_ = view_;
}

void CalendarWidget::set_selected_date(const Date& date) { selected_ = date; dirty_ = true; }
void CalendarWidget::set_view_date(const Date& date) { view_ = date; dirty_ = true; }

void CalendarWidget::prev_month() {
  if (--view_.month < 1) { view_.month = 12; view_.year--; }
  dirty_ = true;
}

void CalendarWidget::next_month() {
  if (++view_.month > 12) { view_.month = 1; view_.year++; }
  dirty_ = true;
}

void CalendarWidget::prev_year() { view_.year--; dirty_ = true; }
void CalendarWidget::next_year() { view_.year++; dirty_ = true; }

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

void CalendarWidget::render(tvg::Scene* scene, const Element& elem, Renderer& renderer) {
  auto* style = elem.computed_style;
  Color bg_color = {255, 255, 255, 255};
  if (style) bg_color = style->get_variable_color("--calendar-bg", bg_color);

  auto bg = tvg::Shape::gen();
  bg->appendRect(0, 0, elem.width(), elem.height(), 8, 8);
  bg->fill(bg_color.r, bg_color.g, bg_color.b, bg_color.a);
  scene->push(std::move(bg));

  render_header(scene, elem);
  render_weekdays(scene, elem);
  render_days(scene, elem);
}

void CalendarWidget::render_header(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style && style->font_size > 0 ? style->font_size : 16.0f;
  std::string font_family = style && !style->font_family.empty() ? style->font_family : "Arial";
  float header_h = 48.0f, padding = 12.0f;

  auto prev = tvg::Shape::gen();
  prev->moveTo(padding + 8, header_h / 2);
  prev->lineTo(padding + 14, header_h / 2 - 6);
  prev->lineTo(padding + 14, header_h / 2 + 6);
  prev->close();
  prev->fill(100, 100, 100, 255);
  scene->push(std::move(prev));

  auto next = tvg::Shape::gen();
  float nx = elem.width() - padding - 8;
  next->moveTo(nx, header_h / 2);
  next->lineTo(nx - 6, header_h / 2 - 6);
  next->lineTo(nx - 6, header_h / 2 + 6);
  next->close();
  next->fill(100, 100, 100, 255);
  scene->push(std::move(next));

  std::string month_year = std::string(MONTH_NAMES[view_.month - 1]) + " " + std::to_string(view_.year);
  auto title = tvg::Text::gen();
  title->font(font_family.c_str());
  title->size(font_size);
  title->text(month_year.c_str());
  title->fill(0, 0, 0);
  title->translate((elem.width() - month_year.size() * font_size * 0.5f) / 2, header_h / 2 + font_size / 3);
  scene->push(std::move(title));
}

void CalendarWidget::render_weekdays(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style && style->font_size > 0 ? style->font_size * 0.8f : 12.0f;
  std::string font_family = style && !style->font_family.empty() ? style->font_family : "Arial";
  float header_h = 48.0f, weekday_h = 32.0f, cell_w = elem.width() / 7;

  for (int i = 0; i < 7; i++) {
    auto text = tvg::Text::gen();
    text->font(font_family.c_str());
    text->size(font_size);
    text->text(WEEKDAY_NAMES[i]);
    text->fill(100, 100, 100);
    text->translate(i * cell_w + (cell_w - font_size * 1.2f) / 2, header_h + weekday_h / 2 + font_size / 3);
    scene->push(std::move(text));
  }
}

void CalendarWidget::render_days(tvg::Scene* scene, const Element& elem) {
  auto* style = elem.computed_style;
  float font_size = style && style->font_size > 0 ? style->font_size : 14.0f;
  std::string font_family = style && !style->font_family.empty() ? style->font_family : "Arial";
  Color selected_bg = {59, 130, 246, 255};
  if (style) selected_bg = style->get_variable_color("--calendar-selected", selected_bg);

  float header_h = 48.0f, weekday_h = 32.0f, start_y = header_h + weekday_h;
  float cell_w = elem.width() / 7, cell_h = (elem.height() - start_y) / 6;
  int first_day = day_of_week(view_.year, view_.month, 1);
  int days = days_in_month(view_.year, view_.month);

  for (int d = 1; d <= days; d++) {
    int pos = first_day + d - 1, row = pos / 7, col = pos % 7;
    float cx = col * cell_w + cell_w / 2, cy = start_y + row * cell_h + cell_h / 2;
    float radius = std::min(cell_w, cell_h) * 0.4f;
    bool is_selected = (d == selected_.day && view_.month == selected_.month && view_.year == selected_.year);
    bool is_hover = (d == hover_day_);

    if (is_selected) {
      auto bg = tvg::Shape::gen();
      bg->appendCircle(cx, cy, radius, radius);
      bg->fill(selected_bg.r, selected_bg.g, selected_bg.b, selected_bg.a);
      scene->push(std::move(bg));
    } else if (is_hover) {
      auto bg = tvg::Shape::gen();
      bg->appendCircle(cx, cy, radius, radius);
      bg->fill(240, 240, 240, 255);
      scene->push(std::move(bg));
    }

    auto text = tvg::Text::gen();
    text->font(font_family.c_str());
    text->size(font_size);
    text->text(std::to_string(d).c_str());
    if (is_selected) text->fill(255, 255, 255);
    else text->fill(0, 0, 0);
    float text_w = (d >= 10 ? 2 : 1) * font_size * 0.5f;
    text->translate(cx - text_w / 2, cy + font_size / 3);
    scene->push(std::move(text));
  }
}

bool CalendarWidget::handle_event(const Event& event, Element& elem) {
  float header_h = 48.0f, weekday_h = 32.0f, start_y = header_h + weekday_h;
  float cell_w = elem.width() / 7, cell_h = (elem.height() - start_y) / 6, padding = 12.0f;
  float local_x = event.x - elem.absolute_x(), local_y = event.y - elem.absolute_y();

  if (event.type == EventType::MouseDown) {
    if (local_y < header_h) {
      if (local_x < padding + 24) { prev_month(); elem.mark_paint_dirty(); return true; }
      if (local_x > elem.width() - padding - 24) { next_month(); elem.mark_paint_dirty(); return true; }
    }
    if (local_y >= start_y) {
      int first_day = day_of_week(view_.year, view_.month, 1);
      int days = days_in_month(view_.year, view_.month);
      int col = static_cast<int>(local_x / cell_w), row = static_cast<int>((local_y - start_y) / cell_h);
      int day = row * 7 + col - first_day + 1;
      if (day >= 1 && day <= days) {
        selected_.year = view_.year; selected_.month = view_.month; selected_.day = day;
        dirty_ = true; elem.mark_paint_dirty();
        if (on_select_) on_select_(selected_);
        return true;
      }
    }
  }
  if (event.type == EventType::MouseMove && local_y >= start_y) {
    int first_day = day_of_week(view_.year, view_.month, 1);
    int days = days_in_month(view_.year, view_.month);
    int col = static_cast<int>(local_x / cell_w), row = static_cast<int>((local_y - start_y) / cell_h);
    int day = row * 7 + col - first_day + 1;
    int new_hover = (day >= 1 && day <= days) ? day : -1;
    if (new_hover != hover_day_) { hover_day_ = new_hover; elem.mark_paint_dirty(); }
  }
  return false;
}

void CalendarWidget::update(float delta_ms, Element& elem) {}

} // namespace tvgbox2
