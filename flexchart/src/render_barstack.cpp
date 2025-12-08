#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderBarStackSeries(NVGcontext* vg, const std::vector<SeriesData>& allSeries,
                          float x, float y, float w, float h,
                          const std::vector<std::string>& labels,
                          int hoveredSeriesIdx, int hoveredDataIdx,
                          const std::vector<Color>& colors) {
    // Filter BarStack series
    std::vector<const SeriesData*> stackSeries;
    for (const auto& s : allSeries) {
        if (s.type == SeriesType::BarStack) {
            stackSeries.push_back(&s);
        }
    }
    
    if (stackSeries.empty()) return;
    
    size_t numBars = 0;
    for (const auto* s : stackSeries) {
        numBars = std::max(numBars, s->data.size());
    }
    if (numBars == 0) return;
    
    // Calculate total for each bar position
    std::vector<double> totals(numBars, 0);
    for (const auto* s : stackSeries) {
        for (size_t i = 0; i < s->data.size(); i++) {
            totals[i] += std::abs(s->data[i]);
        }
    }
    
    double maxTotal = *std::max_element(totals.begin(), totals.end());
    if (maxTotal <= 0) maxTotal = 100;
    
    float barW = w / numBars;
    float gap = barW * 0.2f;
    
    // Draw stacked bars
    for (size_t i = 0; i < numBars; i++) {
        float bx = x + i * barW + gap / 2;
        float currentY = y + h;
        
        for (size_t si = 0; si < stackSeries.size(); si++) {
            const auto* s = stackSeries[si];
            if (i >= s->data.size()) continue;
            
            double val = std::abs(s->data[i]);
            float barH = h * (val / maxTotal);
            
            bool hovered = ((int)si == hoveredSeriesIdx && (int)i == hoveredDataIdx);
            Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[si % colors.size()];
            
            nvgBeginPath(vg);
            nvgRect(vg, bx, currentY - barH, barW - gap, barH);
            nvgFillColor(vg, hovered ?
                nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
                color.toNVG());
            nvgFill(vg);
            
            // Border
            nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 150));
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);
            
            currentY -= barH;
        }
    }
    
    // Draw labels
    nvgFontSize(vg, 10.0f);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(100, 100, 100));
    
    for (size_t i = 0; i < std::min(numBars, labels.size()); i++) {
        float cx = x + (i + 0.5f) * barW;
        nvgText(vg, cx, y + h + 5, labels[i].c_str(), nullptr);
    }
}

} // namespace flexchart
