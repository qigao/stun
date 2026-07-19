/*
 * flexUI - CalendarWidget
 *
 * Date picker - 使用 RenderCommandList 渲染
 */

#ifndef FLEXUI_CALENDAR_WIDGET_H
#define FLEXUI_CALENDAR_WIDGET_H

#include "../widget.h"
#include "../group.h"
#include "../render_command.h"
#include "../shapes.h"
#include <string>
#include <functional>
#include <vector>

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
  const char* type_name() const override { return "CalendarWidget"; }
  bool paints_host_box() const override { return true; }

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
  void sync_host_semantics() override;
  void render_static_layer(RenderCommandList& commands, const Element& elem);
  void render_header(RenderCommandList& commands, const Element& elem);
  void render_weekdays(RenderCommandList& commands, const Element& elem);
  void render_day_labels(RenderCommandList& commands, const Element& elem);
  void render_selected_day(RenderCommandList& commands, const Element& elem);
  bool static_cache_matches(const Element& elem) const;
  void update_static_cache_key(const Element& elem);

  int days_in_month(int year, int month) const;
  int day_of_week(int year, int month, int day) const;  // 0=Sun
  int day_at_local_position(float local_x, float local_y,
                            const Element& elem) const;
  std::string format_date(const Date& date) const;
  std::string format_view_label() const;

  Date selected_;
  Date view_;
  int hovered_day_ = 0;
  SelectCallback on_select_;

  bool static_cache_valid_ = false;
  int cached_year_ = 0;
  int cached_month_ = 0;
  float cached_width_ = 0.0f;
  float cached_height_ = 0.0f;
  float cached_font_size_ = 0.0f;
  std::string cached_font_family_;
  std::vector<RenderCommand> static_commands_;
};

} // namespace flexUI

#endif // FLEXUI_CALENDAR_WIDGET_H
