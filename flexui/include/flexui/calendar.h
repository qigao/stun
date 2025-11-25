#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>
#include <functional>

namespace flexui {

struct CalendarStyle {
    float cellSize = 40;
    NVGcolor bgColor = nvgRGB(255, 255, 255);
    NVGcolor headerBg = nvgRGB(245, 245, 245);
    NVGcolor todayBg = nvgRGB(227, 242, 253);
    NVGcolor selectedBg = nvgRGB(25, 118, 210);
    NVGcolor textColor = nvgRGB(51, 51, 51);
    NVGcolor selectedTextColor = nvgRGB(255, 255, 255);
    NVGcolor disabledTextColor = nvgRGB(189, 189, 189);
    float fontSize = 14;
};

class Calendar : public Widget {
public:
    using SelectCallback = std::function<void(int, int, int)>;

    Calendar(NVGCSSRenderer* renderer, const std::string& id,
             int year, int month, const CalendarStyle& style = CalendarStyle());

    void draw(NVGcontext* vg) override;
    bool handleMouseDown(float mx, float my) override;

    void setDate(int year, int month);
    void setSelectedDay(int day) { selected_day_ = day; }
    int getSelectedDay() const { return selected_day_; }
    void setSelectCallback(SelectCallback callback) { select_callback_ = callback; }

private:
    int year_;
    int month_;
    int selected_day_ = -1;
    CalendarStyle style_;
    SelectCallback select_callback_;

    int getDaysInMonth() const;
    int getFirstDayOfWeek() const;
};

} // namespace flexui
