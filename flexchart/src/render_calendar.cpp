#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>
#include <unordered_map>

namespace flexchart {

static int daysInMonth(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
        return 29;
    return days[month - 1];
}

static int dayOfWeek(int year, int month, int day) {
    // Zeller's formula (0 = Sunday)
    if (month < 3) { month += 12; year--; }
    int k = year % 100;
    int j = year / 100;
    int h = (day + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
    return ((h + 6) % 7); // Convert to 0=Monday
}

void renderCalendarSeries(NVGcontext* vg, const SeriesData& series,
                          float x, float y, float w, float h,
                          int hoveredIdx) {
    if (series.calendarData.empty()) return;
    
    int year = series.calendarYear;
    
    // Build value map
    std::unordered_map<std::string, double> valueMap;
    double minVal = 1e9, maxVal = -1e9;
    for (const auto& d : series.calendarData) {
        valueMap[d.date] = d.value;
        minVal = std::min(minVal, d.value);
        maxVal = std::max(maxVal, d.value);
    }
    double range = maxVal - minVal;
    if (range <= 0) range = 1;
    
    // Calculate cell size
    float cellSize = std::min((w - 40) / 53.0f, (h - 30) / 7.0f);
    cellSize = std::max(8.0f, cellSize);
    
    // Draw month labels
    nvgFontSize(vg, 9.0f);
    nvgFontFace(vg, "sans-serif");
    nvgFillColor(vg, nvgRGB(100, 100, 100));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    
    static const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    
    // Draw day labels
    static const char* days[] = {"M","T","W","T","F","S","S"};
    for (int d = 0; d < 7; d++) {
        nvgText(vg, x, y + 15 + d * cellSize + cellSize / 2, days[d], nullptr);
    }
    
    // Draw cells
    float startX = x + 15;
    float startY = y + 15;
    
    int week = 0;
    for (int month = 1; month <= 12; month++) {
        int days = daysInMonth(year, month);
        
        for (int day = 1; day <= days; day++) {
            int dow = dayOfWeek(year, month, day);
            
            char dateStr[16];
            snprintf(dateStr, sizeof(dateStr), "%04d-%02d-%02d", year, month, day);
            
            float cx = startX + week * cellSize;
            float cy = startY + dow * cellSize;
            
            // Get value and color
            auto it = valueMap.find(dateStr);
            double val = (it != valueMap.end()) ? it->second : 0;
            double ratio = (val - minVal) / range;
            ratio = std::max(0.0, std::min(1.0, ratio));
            
            uint8_t r = 230 - 139 * ratio;
            uint8_t g = 230 - 87 * ratio;
            uint8_t b = 230 + 6 * ratio;
            
            nvgBeginPath(vg);
            nvgRect(vg, cx + 1, cy + 1, cellSize - 2, cellSize - 2);
            nvgFillColor(vg, nvgRGBA(r, g, b, 255));
            nvgFill(vg);
            
            // Next week on Sunday
            if (dow == 6) week++;
        }
        
        // Draw month label at first day
        float monthX = startX + (week - daysInMonth(year, month) / 7) * cellSize;
        nvgText(vg, monthX, y + 2, months[month - 1], nullptr);
    }
}

} // namespace flexchart
