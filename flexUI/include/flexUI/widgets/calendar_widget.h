/*
 * flexUI - CalendarWidget
 *
 * Date picker - 使用 flex::Renderer 渲染
 */

#ifndef FLEXUI_CALENDAR_WIDGET_H
#define FLEXUI_CALENDAR_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../shapes.h"
#include <string>
#include <functional>

namespace flexUI {

struct Date {
  int year = 2024;
  int month = 1;  // 1-12
  int day = 1;    // 1-31

  bool operator==(const Date& other) const {
    return year == other.year && month == other.month && day == other.day;
  }
};

class CalendarWidget : public Widget {
public:
  CalendarWidget();

  void render(const Element& elem, Renderer& renderer) override;
  bool handle_event(const Event& event, Element& elem) override;
  void update(float delta_ms, Element& elem) override;
  const char* type_name() const override { return "CalendarWidget"; }

  Date selected_date() const { return selected_; }
  void set_selected_date(const Date& date);

  Date view_date() const { return view_; }
  void set_view_date(const Date& date);

  void prev_month();
  void next_month();
  void prev_year();
  void next_year();

  using SelectCallback = std::function<void(const Date& date)>;
  void set_select_callback(SelectCallback cb) { on_select_ = std::move(cb); }

private:
  void render_header(flex::Renderer& r, const Element& elem);
  void render_weekdays(flex::Renderer& r, const Element& elem);
  void render_days(flex::Renderer& r, const Element& elem);

  int days_in_month(int year, int month) const;
  int day_of_week(int year, int month, int day) const;  // 0=Sun

  Date selected_;
  Date view_;
  int hover_day_ = -1;
  SelectCallback on_select_;
};

} // namespace flexUI

#endif // FLEXUI_CALENDAR_WIDGET_H
