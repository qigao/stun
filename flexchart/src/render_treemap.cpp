#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace flexchart {

static void renderTreemapRect(NVGcontext* vg, const std::vector<TreemapData>& nodes,
                              float x, float y, float w, float h,
                              const std::vector<Color>& colors, int depth,
                              int& colorIdx) {
    if (nodes.empty() || w < 2 || h < 2) return;
    
    double total = 0;
    for (const auto& n : nodes) total += n.value;
    if (total <= 0) return;
    
    bool horizontal = w >= h;
    float pos = 0;
    
    for (size_t i = 0; i < nodes.size(); i++) {
        const auto& node = nodes[i];
        float ratio = node.value / total;
        float nodeW, nodeH, nodeX, nodeY;
        
        if (horizontal) {
            nodeW = w * ratio;
            nodeH = h;
            nodeX = x + pos;
            nodeY = y;
            pos += nodeW;
        } else {
            nodeW = w;
            nodeH = h * ratio;
            nodeX = x;
            nodeY = y + pos;
            pos += nodeH;
        }
        
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[colorIdx % colors.size()];
        
        // Lighten color for deeper levels
        if (depth > 0) {
            color.r = std::min(255, color.r + depth * 20);
            color.g = std::min(255, color.g + depth * 20);
            color.b = std::min(255, color.b + depth * 20);
        }
        
        nvgBeginPath(vg);
        nvgRect(vg, nodeX + 1, nodeY + 1, nodeW - 2, nodeH - 2);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        // Border
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 200));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);
        
        // Label
        if (nodeW > 40 && nodeH > 20) {
            nvgFontSize(vg, std::min(12.0f, std::min(nodeW * 0.15f, nodeH * 0.4f)));
            nvgFontFace(vg, "sans-serif");
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgFillColor(vg, nvgRGB(255, 255, 255));
            
            char label[64];
            snprintf(label, sizeof(label), "%s\n%.0f", node.name.c_str(), node.value);
            nvgText(vg, nodeX + nodeW/2, nodeY + nodeH/2, node.name.c_str(), nullptr);
        }
        
        // Recursively render children
        if (!node.children.empty()) {
            renderTreemapRect(vg, node.children, 
                            nodeX + 2, nodeY + 2, nodeW - 4, nodeH - 4,
                            colors, depth + 1, colorIdx);
        }
        
        colorIdx++;
    }
}

void renderTreemapSeries(NVGcontext* vg, const SeriesData& series,
                         float x, float y, float w, float h,
                         int hoveredIdx,
                         const std::vector<Color>& colors) {
    if (series.treemapData.empty()) return;
    
    int colorIdx = 0;
    renderTreemapRect(vg, series.treemapData, x, y, w, h, colors, 0, colorIdx);
}

} // namespace flexchart
