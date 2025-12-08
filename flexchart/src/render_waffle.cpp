#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderWaffleSeries(NVGcontext* vg, const SeriesData& series,
                        float x, float y, float w, float h,
                        Color color) {
    int cols = series.waffleCols;
    int rows = series.waffleRows;
    int total = cols * rows;
    
    double ratio = series.waffleMax > 0 ? series.waffleValue / series.waffleMax : 0;
    ratio = std::max(0.0, std::min(1.0, ratio));
    int filled = (int)(total * ratio + 0.5);
    
    float cellW = (w - 20) / cols;
    float cellH = (h - 40) / rows;
    float cellSize = std::min(cellW, cellH) - 2;
    
    float startX = x + (w - cols * (cellSize + 2)) / 2;
    float startY = y + 20;
    
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int idx = r * cols + c;
            float cx = startX + c * (cellSize + 2);
            float cy = startY + r * (cellSize + 2);
            
            bool isFilled = idx < filled;
            
            nvgBeginPath(vg);
            nvgRoundedRect(vg, cx, cy, cellSize, cellSize, 2);
            nvgFillColor(vg, isFilled ? color.toNVG() : nvgRGBA(220, 220, 220, 255));
            nvgFill(vg);
        }
    }
    
    // Draw percentage
    char text[32];
    snprintf(text, sizeof(text), "%.0f%%", ratio * 100);
    
    nvgFontSize(vg, 14.0f);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(60, 60, 60));
    nvgText(vg, x + w / 2, y + h - 18, text, nullptr);
}

} // namespace flexchart
