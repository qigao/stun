#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <set>

namespace flexchart {

void renderThemeRiverSeries(NVGcontext* vg, const SeriesData& series,
                            float x, float y, float w, float h,
                            int hoveredIdx,
                            const std::vector<Color>& colors) {
    if (series.themeRiverData.empty()) return;
    
    // Get unique dates and names
    std::set<std::string> dateSet, nameSet;
    for (const auto& d : series.themeRiverData) {
        dateSet.insert(d.date);
        nameSet.insert(d.name);
    }
    
    std::vector<std::string> dates(dateSet.begin(), dateSet.end());
    std::vector<std::string> names(nameSet.begin(), nameSet.end());
    std::sort(dates.begin(), dates.end());
    
    if (dates.empty() || names.empty()) return;
    
    // Build value matrix [date][name]
    std::unordered_map<std::string, std::unordered_map<std::string, double>> values;
    for (const auto& d : series.themeRiverData) {
        values[d.date][d.name] = d.value;
    }
    
    // Calculate max total for scaling
    double maxTotal = 0;
    for (const auto& date : dates) {
        double total = 0;
        for (const auto& name : names) {
            total += values[date][name];
        }
        maxTotal = std::max(maxTotal, total);
    }
    if (maxTotal <= 0) maxTotal = 1;
    
    float cx = x + w / 2;
    float chartH = h * 0.8f;
    float dateW = w / dates.size();
    
    // Draw each stream
    for (size_t ni = 0; ni < names.size(); ni++) {
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[ni % colors.size()];
        
        std::vector<float> topY(dates.size()), bottomY(dates.size());
        
        for (size_t di = 0; di < dates.size(); di++) {
            double total = 0;
            for (const auto& name : names) total += values[dates[di]][name];
            
            double before = 0;
            for (size_t i = 0; i < ni; i++) {
                before += values[dates[di]][names[i]];
            }
            double val = values[dates[di]][names[ni]];
            
            float centerY = y + h / 2;
            float halfHeight = (chartH / 2) * (total / maxTotal);
            
            float ratio1 = before / total;
            float ratio2 = (before + val) / total;
            
            bottomY[di] = centerY + halfHeight * (2 * ratio1 - 1);
            topY[di] = centerY + halfHeight * (2 * ratio2 - 1);
        }
        
        // Draw stream area
        nvgBeginPath(vg);
        nvgMoveTo(vg, x, bottomY[0]);
        
        for (size_t di = 0; di < dates.size(); di++) {
            float px = x + (di + 0.5f) * dateW;
            nvgLineTo(vg, px, bottomY[di]);
        }
        
        for (int di = dates.size() - 1; di >= 0; di--) {
            float px = x + (di + 0.5f) * dateW;
            nvgLineTo(vg, px, topY[di]);
        }
        
        nvgClosePath(vg);
        nvgFillColor(vg, nvgRGBA(color.r, color.g, color.b, 200));
        nvgFill(vg);
    }
    
    // Draw labels
    nvgFontSize(vg, 9.0f);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
    nvgFillColor(vg, nvgRGB(100, 100, 100));
    
    for (size_t di = 0; di < dates.size(); di += std::max((size_t)1, dates.size() / 6)) {
        float px = x + (di + 0.5f) * dateW;
        nvgText(vg, px, y + h - 15, dates[di].c_str(), nullptr);
    }
}

} // namespace flexchart
