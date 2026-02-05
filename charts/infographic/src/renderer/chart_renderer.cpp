#include <renderer/chart_renderer.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flex::modules::infographic {

// ========== ChartRendererBase ==========

double ChartRendererBase::calculate_total(const UnifiedInfographic& infographic) const {
    double total = 0;
    for (const auto& item : infographic.items) {
        if (item->value.has_value()) {
            total += item->value.value();
        }
    }
    return total;
}

void ChartRendererBase::render_legend(SvgContext& ctx, const UnifiedInfographic& infographic,
                                      int x, int y) {
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        std::string color = get_color(infographic.theme, i);
        int item_y = y + static_cast<int>(i) * 30;
        
        svg_rect(ctx.svg, x, item_y - 10, 15, 15, color);
        
        std::string label = item->label;
        if (item->value.has_value()) {
            label += " (" + std::to_string(static_cast<int>(item->value.value())) + ")";
        }
        svg_text(ctx.svg, x + 25, item_y + 5, label, 14, "#333");
    }
}

// ========== PieChartRenderer ==========

void PieChartRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int center_x = 300;
    int center_y = ctx.content_start_y + 200;
    int radius = 120;
    
    double total = calculate_total(infographic);
    if (total <= 0) return;
    
    double current_angle = -90;  // 从顶部开始
    
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        if (!item->value.has_value()) continue;
        
        double value = item->value.value();
        double angle = (value / total) * 360;
        std::string color = get_color(infographic.theme, i);
        
        // 计算弧形路径
        double start_rad = current_angle * M_PI / 180;
        double end_rad = (current_angle + angle) * M_PI / 180;
        
        double x1 = center_x + radius * cos(start_rad);
        double y1 = center_y + radius * sin(start_rad);
        double x2 = center_x + radius * cos(end_rad);
        double y2 = center_y + radius * sin(end_rad);
        
        int large_arc = (angle > 180) ? 1 : 0;
        
        // 绘制扇形
        ctx.svg << "<path d=\"M " << center_x << " " << center_y
                << " L " << x1 << " " << y1
                << " A " << radius << " " << radius << " 0 " << large_arc << " 1 " << x2 << " " << y2
                << " Z\" fill=\"" << color << "\" opacity=\"0.8\"/>\n";
        
        // 标签位置
        double mid_angle = current_angle + angle / 2;
        double mid_rad = mid_angle * M_PI / 180;
        int label_x = center_x + static_cast<int>(radius * 0.7 * cos(mid_rad));
        int label_y = center_y + static_cast<int>(radius * 0.7 * sin(mid_rad));
        
        svg_text(ctx.svg, label_x, label_y, item->label, 12, "white", "middle", true);
        
        current_angle += angle;
    }
    
    // 图例
    render_legend(ctx, infographic, 500, ctx.content_start_y + 100);
}

// ========== DonutChartRenderer ==========

void DonutChartRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int center_x = 300;
    int center_y = ctx.content_start_y + 200;
    int outer_radius = 120;
    int inner_radius = 60;
    
    double total = calculate_total(infographic);
    if (total <= 0) return;
    
    double current_angle = -90;
    
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        if (!item->value.has_value()) continue;
        
        double value = item->value.value();
        double angle = (value / total) * 360;
        std::string color = get_color(infographic.theme, i);
        
        double start_rad = current_angle * M_PI / 180;
        double end_rad = (current_angle + angle) * M_PI / 180;
        
        // 外弧
        double ox1 = center_x + outer_radius * cos(start_rad);
        double oy1 = center_y + outer_radius * sin(start_rad);
        double ox2 = center_x + outer_radius * cos(end_rad);
        double oy2 = center_y + outer_radius * sin(end_rad);
        
        // 内弧
        double ix1 = center_x + inner_radius * cos(start_rad);
        double iy1 = center_y + inner_radius * sin(start_rad);
        double ix2 = center_x + inner_radius * cos(end_rad);
        double iy2 = center_y + inner_radius * sin(end_rad);
        
        int large_arc = (angle > 180) ? 1 : 0;
        
        // 绘制环形扇区
        ctx.svg << "<path d=\"M " << ox1 << " " << oy1
                << " A " << outer_radius << " " << outer_radius << " 0 " << large_arc << " 1 " << ox2 << " " << oy2
                << " L " << ix2 << " " << iy2
                << " A " << inner_radius << " " << inner_radius << " 0 " << large_arc << " 0 " << ix1 << " " << iy1
                << " Z\" fill=\"" << color << "\" opacity=\"0.8\"/>\n";
        
        current_angle += angle;
    }
    
    // 中心文字
    svg_text(ctx.svg, center_x, center_y, std::to_string(static_cast<int>(total)), 24, "#333", "middle", true);
    svg_text(ctx.svg, center_x, center_y + 20, "Total", 12, "#666", "middle");
    
    // 图例
    render_legend(ctx, infographic, 500, ctx.content_start_y + 100);
}

// ========== BarChartRenderer ==========

void BarChartRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int chart_x = 150;
    int chart_y = ctx.content_start_y;
    int chart_width = 500;
    int bar_height = 40;
    int bar_spacing = 15;
    
    double max_value = 0;
    for (const auto& item : infographic.items) {
        if (item->value.has_value() && item->value.value() > max_value) {
            max_value = item->value.value();
        }
    }
    if (max_value <= 0) max_value = 100;
    
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        std::string color = get_color(infographic.theme, i);
        int y = chart_y + static_cast<int>(i) * (bar_height + bar_spacing);
        
        // 标签
        svg_text(ctx.svg, chart_x - 10, y + bar_height / 2 + 5, item->label, 12, "#333", "end");
        
        // 条形
        double value = item->value.has_value() ? item->value.value() : 0;
        int bar_width = static_cast<int>((value / max_value) * chart_width);
        
        svg_rect(ctx.svg, chart_x, y, bar_width, bar_height, color, 4, 0.8f);
        
        // 数值
        if (item->value.has_value()) {
            svg_text(ctx.svg, chart_x + bar_width + 10, y + bar_height / 2 + 5,
                    std::to_string(static_cast<int>(value)), 12, "#333");
        }
    }
}

} // namespace flex::modules::infographic
