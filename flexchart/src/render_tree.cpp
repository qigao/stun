#include <flexchart/flexchart.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flexchart {

struct TreeLayout {
    float x, y;
    const TreeNode* node;
};

static int countLeaves(const std::vector<TreeNode>& nodes) {
    int count = 0;
    for (const auto& n : nodes) {
        if (n.children.empty()) count++;
        else count += countLeaves(n.children);
    }
    return count;
}

static void layoutTree(const std::vector<TreeNode>& nodes, 
                      std::vector<TreeLayout>& layouts,
                      float x, float y, float w, float h, int depth,
                      float& leafY, float leafSpacing) {
    for (const auto& node : nodes) {
        TreeLayout tl;
        tl.node = &node;
        tl.x = x + depth * (w / 5);
        
        if (node.children.empty()) {
            tl.y = leafY;
            leafY += leafSpacing;
        } else {
            float startY = leafY;
            layoutTree(node.children, layouts, x, y, w, h, depth + 1, leafY, leafSpacing);
            float endY = leafY - leafSpacing;
            tl.y = (startY + endY) / 2;
        }
        
        layouts.push_back(tl);
    }
}

static void drawTreeLinks(NVGcontext* vg, const std::vector<TreeLayout>& layouts,
                         const std::vector<TreeNode>& nodes, size_t& idx) {
    for (const auto& node : nodes) {
        const TreeLayout& parent = layouts[idx++];
        
        if (!node.children.empty()) {
            size_t childStart = idx;
            
            for (size_t i = 0; i < node.children.size(); i++) {
                const TreeLayout& child = layouts[childStart + i];
                
                // Draw link
                nvgBeginPath(vg);
                nvgMoveTo(vg, parent.x + 5, parent.y);
                float midX = (parent.x + child.x) / 2;
                nvgBezierTo(vg, midX, parent.y, midX, child.y, child.x - 5, child.y);
                nvgStrokeColor(vg, nvgRGBA(180, 180, 180, 200));
                nvgStrokeWidth(vg, 1.5f);
                nvgStroke(vg);
            }
            
            // Recurse
            drawTreeLinks(vg, layouts, node.children, idx);
        }
    }
}

void renderTreeSeries(NVGcontext* vg, const SeriesData& series,
                      float x, float y, float w, float h,
                      int hoveredIdx,
                      const std::vector<Color>& colors) {
    if (series.treeData.empty()) return;
    
    int leaves = countLeaves(series.treeData);
    if (leaves == 0) leaves = 1;
    
    float leafSpacing = (h - 20) / leaves;
    float leafY = y + 10;
    
    std::vector<TreeLayout> layouts;
    layoutTree(series.treeData, layouts, x + 10, y, w - 20, h, 0, leafY, leafSpacing);
    
    // Draw links
    size_t idx = 0;
    drawTreeLinks(vg, layouts, series.treeData, idx);
    
    // Draw nodes
    for (size_t i = 0; i < layouts.size(); i++) {
        const auto& tl = layouts[i];
        bool hovered = ((int)i == hoveredIdx);
        
        Color color = colors.empty() ? Color{91, 143, 249, 255} : colors[0];
        
        float r = hovered ? 8.0f : 5.0f;
        
        nvgBeginPath(vg);
        nvgCircle(vg, tl.x, tl.y, r);
        nvgFillColor(vg, color.toNVG());
        nvgFill(vg);
        
        // Label
        nvgFontSize(vg, 10.0f);
        nvgFontFace(vg, "sans-serif");
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(60, 60, 60));
        nvgText(vg, tl.x + r + 5, tl.y, tl.node->name.c_str(), nullptr);
    }
}

} // namespace flexchart
