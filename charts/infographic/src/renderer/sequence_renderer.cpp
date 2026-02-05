#include <renderer/sequence_renderer.h>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flex::modules::infographic {

// ========== SequenceRendererBase ==========

std::vector<SequenceRendererBase::NodePosition> SequenceRendererBase::calculate_positions(
    const UnifiedInfographic& infographic, int center_x, int center_y, int radius) const {
    
    std::vector<NodePosition> positions;
    size_t count = infographic.items.size();
    
    for (size_t i = 0; i < count; ++i) {
        double angle = 2 * M_PI * i / count - M_PI / 2;  // 从顶部开始
        NodePosition pos;
        pos.x = center_x + static_cast<int>(radius * cos(angle));
        pos.y = center_y + static_cast<int>(radius * sin(angle));
        pos.angle = angle;
        positions.push_back(pos);
    }
    
    return positions;
}

// ========== TimelineRenderer ==========

void TimelineRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int timeline_y = ctx.content_start_y + 20;
    int step_width = 150;
    int item_count = static_cast<int>(infographic.items.size());
    int start_x = (ctx.width - item_count * step_width) / 2;
    
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        std::string color = get_color(infographic.theme, i);
        int x = start_x + static_cast<int>(i) * step_width + step_width / 2;
        
        // 节点圆圈
        svg_circle(ctx.svg, x, timeline_y, 20, color);
        svg_text(ctx.svg, x, timeline_y + 5, std::to_string(i + 1), 14, "white", "middle", true);
        
        // 连接线
        if (i < infographic.items.size() - 1) {
            svg_line(ctx.svg, x + 20, timeline_y, x + step_width - 20, timeline_y, color, 3);
        }
        
        // 标签
        svg_text(ctx.svg, x, timeline_y + 50, item->label, 14, "#333", "middle", true);
        
        // 描述
        if (item->desc.has_value()) {
            svg_text(ctx.svg, x, timeline_y + 70, item->desc.value(), 12, "#666", "middle");
        }
    }
}

// ========== FunnelRenderer ==========

void FunnelRenderer::calculate_canvas_size(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int item_count = static_cast<int>(infographic.items.size());
    ctx.height = 100 + item_count * 80 + 100;  // header + items + footer
}

void FunnelRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int funnel_top = ctx.content_start_y;
    int funnel_height = static_cast<int>(infographic.items.size()) * 80;
    int funnel_width_top = 300;
    int funnel_width_bottom = 100;
    int center_x = ctx.width / 2;
    int step_height = funnel_height / static_cast<int>(infographic.items.size());
    
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        std::string color = get_color(infographic.theme, i);
        int y = funnel_top + static_cast<int>(i) * step_height;
        
        // 计算梯形宽度
        float progress = static_cast<float>(i) / std::max(1.0f, static_cast<float>(infographic.items.size() - 1));
        int current_width = funnel_width_top - static_cast<int>((funnel_width_top - funnel_width_bottom) * progress);
        
        float next_progress = (i == infographic.items.size() - 1) ? progress :
                             static_cast<float>(i + 1) / (infographic.items.size() - 1);
        int next_width = funnel_width_top - static_cast<int>((funnel_width_top - funnel_width_bottom) * next_progress);
        
        // 绘制梯形
        ctx.svg << "<polygon points=\""
                << (center_x - current_width / 2) << "," << y << " "
                << (center_x + current_width / 2) << "," << y << " "
                << (center_x + next_width / 2) << "," << (y + step_height) << " "
                << (center_x - next_width / 2) << "," << (y + step_height)
                << "\" fill=\"" << color << "\" stroke=\"white\" stroke-width=\"2\"/>\n";
        
        // 标签
        svg_text(ctx.svg, center_x, y + step_height / 2 - 5, item->label, 14, "white", "middle", true);
        
        // 数值或描述
        if (item->value.has_value()) {
            svg_text(ctx.svg, center_x, y + step_height / 2 + 15,
                    std::to_string(static_cast<int>(item->value.value())), 12, "white", "middle");
        } else if (item->desc.has_value()) {
            svg_text(ctx.svg, center_x, y + step_height / 2 + 15, item->desc.value(), 12, "white", "middle");
        }
    }
}

// ========== CircularRenderer ==========

void CircularRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int center_x = ctx.width / 2;
    int center_y = ctx.content_start_y + 200;
    int radius = 150;
    
    auto positions = calculate_positions(infographic, center_x, center_y, radius);
    
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        std::string color = get_color(infographic.theme, i);
        const auto& pos = positions[i];
        
        // 连接到中心的线
        svg_line(ctx.svg, center_x, center_y, pos.x, pos.y, color, 2);
        
        // 节点圆圈
        svg_circle(ctx.svg, pos.x, pos.y, 25, color, 0.8f, "white", 3);
        svg_text(ctx.svg, pos.x, pos.y + 5, std::to_string(i + 1), 14, "white", "middle", true);
        
        // 外部标签
        int label_x = center_x + static_cast<int>((radius + 50) * cos(pos.angle));
        int label_y = center_y + static_cast<int>((radius + 50) * sin(pos.angle));
        svg_text(ctx.svg, label_x, label_y, item->label, 12, "#333", "middle", true);
    }
    
    // 中心节点
    svg_circle(ctx.svg, center_x, center_y, 25, "#333");
    svg_text(ctx.svg, center_x, center_y + 5, "Core", 12, "white", "middle", true);
}

// ========== VerticalRoadmapRenderer ==========

void VerticalRoadmapRenderer::calculate_canvas_size(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int item_count = static_cast<int>(infographic.items.size());
    ctx.height = 100 + item_count * 80 + 50;
}

void VerticalRoadmapRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int roadmap_x = 100;
    int step_height = 80;
    int start_y = ctx.content_start_y;
    int item_count = static_cast<int>(infographic.items.size());
    
    // 主线
    svg_line(ctx.svg, roadmap_x, start_y, roadmap_x, start_y + (item_count - 1) * step_height, "#e5e7eb", 3);
    
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        std::string color = get_color(infographic.theme, i);
        int y = start_y + static_cast<int>(i) * step_height;
        
        // 里程碑圆圈
        svg_circle(ctx.svg, roadmap_x, y, 12, color, 1.0f, "white", 3);
        
        // 卡片
        svg_rect(ctx.svg, roadmap_x + 30, y - 25, 250, 50, "white", 8, 1.0f, "#e5e7eb", 1);
        
        // 标签
        svg_text(ctx.svg, roadmap_x + 45, y - 5, item->label, 14, "#1f2937", "start", true);
        
        // 描述
        if (item->desc.has_value()) {
            svg_text(ctx.svg, roadmap_x + 45, y + 15, item->desc.value(), 12, "#6b7280");
        }
    }
}

} // namespace flex::modules::infographic
