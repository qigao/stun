#include <flexui/calendar.h>
#include <nanovg_css_internal.h>
#include <ctime>

namespace flexui {

Calendar::Calendar(NVGCSSRenderer* renderer, const std::string& id,
                   int year, int month, const CalendarStyle& style)
    : Widget(renderer, id, "calendar"), year_(year), month_(month), style_(style) {
}

int Calendar::getDaysInMonth() const {
    int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month_ == 2 && ((year_ % 4 == 0 && year_ % 100 != 0) || year_ % 400 == 0)) {
        return 29;
    }
    return days[month_ - 1];
}

int Calendar::getFirstDayOfWeek() const {
    std::tm time = {};
    time.tm_year = year_ - 1900;
    time.tm_mon = month_ - 1;
    time.tm_mday = 1;
    std::mktime(&time);
    return time.tm_wday;
}

void Calendar::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    if (w == 0 || h == 0) return;

    const char* weekDays[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

    nvgFontSize(vg, style_.fontSize);
    nvgFontFace(vg, "sans-serif");

    // Draw header with weekday names
    for (int i = 0; i < 7; ++i) {
        nvgBeginPath(vg);
        nvgRect(vg, x + i * style_.cellSize, y, style_.cellSize, style_.cellSize);
        nvgFillColor(vg, style_.headerBg);
        nvgFill(vg);

        nvgFillColor(vg, style_.textColor);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, x + i * style_.cellSize + style_.cellSize / 2,
                y + style_.cellSize / 2, weekDays[i], nullptr);
    }

    // Draw days
    int daysInMonth = getDaysInMonth();
    int firstDay = getFirstDayOfWeek();
    int day = 1;

    for (int row = 0; row < 6 && day <= daysInMonth; ++row) {
        for (int col = 0; col < 7 && day <= daysInMonth; ++col) {
            if (row == 0 && col < firstDay) {
                continue;
            }

            float cellX = x + col * style_.cellSize;
            float cellY = y + (row + 1) * style_.cellSize;

            NVGcolor bgColor = style_.bgColor;
            NVGcolor textColor = style_.textColor;

            if (day == selected_day_) {
                bgColor = style_.selectedBg;
                textColor = style_.selectedTextColor;
            }

            nvgBeginPath(vg);
            nvgRect(vg, cellX, cellY, style_.cellSize, style_.cellSize);
            nvgFillColor(vg, bgColor);
            nvgFill(vg);

            char dayStr[3];
            snprintf(dayStr, sizeof(dayStr), "%d", day);
            nvgFillColor(vg, textColor);
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(vg, cellX + style_.cellSize / 2, cellY + style_.cellSize / 2, dayStr, nullptr);

            day++;
        }
    }
}

bool Calendar::handleMouseDown(float mx, float my) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;

    int daysInMonth = getDaysInMonth();
    int firstDay = getFirstDayOfWeek();
    int day = 1;

    for (int row = 0; row < 6 && day <= daysInMonth; ++row) {
        for (int col = 0; col < 7 && day <= daysInMonth; ++col) {
            if (row == 0 && col < firstDay) {
                continue;
            }

            float cellX = x + col * style_.cellSize;
            float cellY = y + (row + 1) * style_.cellSize;

            if (mx >= cellX && mx <= cellX + style_.cellSize &&
                my >= cellY && my <= cellY + style_.cellSize) {
                selected_day_ = day;
                if (select_callback_) {
                    select_callback_(year_, month_, day);
                }
                return true;
            }
            day++;
        }
    }
    return false;
}

void Calendar::setDate(int year, int month) {
    year_ = year;
    month_ = month;
    selected_day_ = -1;
}

} // namespace flexui
