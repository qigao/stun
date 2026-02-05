#include <renderer/compare_renderer.h>
#include <algorithm>

namespace flex::modules::infographic {

// ========== VsLayoutRenderer ==========

void VsLayoutRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int left_x = 50;
    int right_x = 400;
    int start_y = ctx.content_start_y;
    
    // VS 标识
    svg_text(ctx.svg, 400, 300, "VS", 36, "#999", "middle", true);
    svg_line(ctx.svg, 375, 120, 375, 480, "#ddd", 2);
    svg_line(ctx.svg, 425, 120, 425, 480, "#ddd", 2);
    
    // 渲染左右两侧
    for (size_t i = 0; i < std::min(size_t(2), infographic.items.size()); ++i) {
        const auto& item = infographic.items[i];
        std::string color = get_color(infographic.theme, i);
        int x = (i == 0) ? left_x : right_x;
        
        // 区域背景
        svg_rect(ctx.svg, x, start_y, SECTION_WIDTH, SECTION_HEIGHT, color, 12, 0.05f);
        svg_rect(ctx.svg, x, start_y, SECTION_WIDTH, SECTION_HEIGHT, "none", 12, 1.0f, color, 3);
        
        // 主标题
        svg_text(ctx.svg, x + SECTION_WIDTH / 2, start_y + 40, item->label, 20, color, "middle", true);
        
        // 子项目列表
        if (!item->children.empty()) {
            int item_y = start_y + 80;
            for (const auto& child : item->children) {
                svg_circle(ctx.svg, x + 30, item_y - 5, 4, color);
                svg_text(ctx.svg, x + 50, item_y, child->label, 14, "#333");
                item_y += 30;
            }
        }
        
        // 描述
        if (item->desc.has_value()) {
            svg_text(ctx.svg, x + SECTION_WIDTH / 2, start_y + SECTION_HEIGHT - 30,
                    item->desc.value(), 12, "#666", "middle");
        }
    }
}

// ========== SwotRenderer ==========

void SwotRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int quad_size = 180;
    int center_x = ctx.width / 2;
    int center_y = ctx.content_start_y + 200;
    
    // 象限位置：左上、右上、左下、右下
    int positions[4][2] = {
        {center_x - quad_size / 2 - 20, center_y - quad_size / 2 - 20},
        {center_x + 20, center_y - quad_size / 2 - 20},
        {center_x - quad_size / 2 - 20, center_y + 20},
        {center_x + 20, center_y + 20}
    };
    
    // SWOT 专用颜色
    static const std::vector<std::string> swot_colors = {"#22c55e", "#ef4444", "#3b82f6", "#f97316"};
    
    // 坐标轴
    svg_line(ctx.svg, center_x - 200, center_y, center_x + 200, center_y, "#999", 2);
    svg_line(ctx.svg, center_x, center_y - 150, center_x, center_y + 150, "#999", 2);
    
    // 坐标轴标签
    svg_text(ctx.svg, center_x - 100, center_y - 10, "Internal", 12, "#666", "middle");
    svg_text(ctx.svg, center_x + 100, center_y - 10, "External", 12, "#666", "middle");
    svg_text(ctx.svg, center_x + 10, center_y - 100, "Positive", 12, "#666");
    svg_text(ctx.svg, center_x + 10, center_y + 120, "Negative", 12, "#666");
    
    // 渲染四个象限
    for (size_t i = 0; i < std::min(size_t(4), infographic.items.size()); ++i) {
        const auto& item = infographic.items[i];
        std::string color = swot_colors[i];
        int x = positions[i][0];
        int y = positions[i][1];
        
        // 象限背景
        svg_rect(ctx.svg, x, y, quad_size, quad_size, color, 8, 0.1f);
        svg_rect(ctx.svg, x, y, quad_size, quad_size, "none", 8, 1.0f, color, 3);
        
        // 标题
        svg_text(ctx.svg, x + quad_size / 2, y + 25, item->label, 16, color, "middle", true);
        
        // 子项目
        if (!item->children.empty()) {
            int item_y = y + 50;
            for (const auto& child : item->children) {
                if (item_y > y + quad_size - 20) break;
                svg_text(ctx.svg, x + 15, item_y, "• " + child->label, 11, "#333");
                item_y += 18;
            }
        }
    }
}

} // namespace flex::modules::infographic
