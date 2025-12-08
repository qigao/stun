#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

namespace flexchart {

void renderHistogramSeries(NVGcontext* vg, const SeriesData& series,
                           float x, float y, float w, float h,
                           int hoveredIdx, Color color) {
    // If bins provided, use them; otherwise compute from data
    std::vector<HistogramBin> bins = series.histogramBins;
    
    if (bins.empty() && !series.data.empty()) {
        // Compute bins from raw data
        double minVal = *std::min_element(series.data.begin(), series.data.end());
        double maxVal = *std::max_element(series.data.begin(), series.data.end());
        double range = maxVal - minVal;
        if (range <= 0) range = 1;
        
        int numBins = series.histogramBinCount;
        double binWidth = range / numBins;
        
        bins.resize(numBins);
        for (int i = 0; i < numBins; i++) {
            bins[i].min = minVal + i * binWidth;
            bins[i].max = minVal + (i + 1) * binWidth;
            bins[i].count = 0;
        }
        
        for (double v : series.data) {
            int idx = std::min((int)((v - minVal) / binWidth), numBins - 1);
            bins[idx].count++;
        }
    }
    
    if (bins.empty()) return;
    
    // Find max count for scaling
    int maxCount = 0;
    for (const auto& b : bins) maxCount = std::max(maxCount, b.count);
    if (maxCount <= 0) maxCount = 1;
    
    float barW = w / bins.size();
    float gap = barW * 0.1f;
    
    for (size_t i = 0; i < bins.size(); i++) {
        float barH = h * bins[i].count / maxCount;
        float bx = x + i * barW + gap / 2;
        float by = y + h - barH;
        
        bool hovered = ((int)i == hoveredIdx);
        
        nvgBeginPath(vg);
        nvgRect(vg, bx, by, barW - gap, barH);
        nvgFillColor(vg, hovered ?
            nvgRGBA(std::min(255, color.r + 30), std::min(255, color.g + 30), std::min(255, color.b + 30), 255) :
            color.toNVG());
        nvgFill(vg);
        
        // Border
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 150));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
    }
    
    // Draw x-axis labels
    nvgFontSize(vg, 9.0f);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(100, 100, 100));
    
    for (size_t i = 0; i <= bins.size(); i += std::max((size_t)1, bins.size() / 5)) {
        float lx = x + i * barW;
        double val = (i < bins.size()) ? bins[i].min : bins.back().max;
        char label[16];
        snprintf(label, sizeof(label), "%.1f", val);
        nvgText(vg, lx, y + h + 5, label, nullptr);
    }
}

} // namespace flexchart
