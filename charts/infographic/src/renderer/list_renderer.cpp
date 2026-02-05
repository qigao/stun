#include <renderer/list_renderer.h>
#include <algorithm>

namespace flex::modules::infographic {

// ========== ListRendererBase ==========

ListRendererBase::GridLayout ListRendererBase::calculate_grid(
    const UnifiedInfographic& infographic, int max_cols) const {
    
    GridLayout layout;
    int item_count = static_cast<int>(infographic.items.size());
    
    layout.cols = std::min(max_cols, item_count);
    layout.rows = (item_count + layout.cols - 1) / layout.cols;
    layout.cell_width = 200;
    layout.cell_height = 120;
    
    int total_width = layout.cols * layout.cell_width + (layout.cols - 1) * 20;
    layout.start_x = (800 - total_width) / 2;
    layout.start_y = 100;
    
    return layout;
}

// ========== ListGridRenderer ==========

void ListGridRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    auto layout = calculate_grid(infographic, 3);
    int margin = 20;
    
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        std::string color = get_color(infographic.theme, i);
        
        int col = static_cast<int>(i) % layout.cols;
        int row = static_cast<int>(i) / layout.cols;
        int x = layout.start_x + col * (layout.cell_width + margin);
        int y = ctx.content_start_y + row * (layout.cell_height + margin);
        
        // 卡片背景
        svg_rect(ctx.svg, x, y, layout.cell_width, layout.cell_height, color, 8, 0.1f);
        svg_rect(ctx.svg, x, y, layout.cell_width, layout.cell_height, "none", 8, 1.0f, color, 2);
        
        // 标签
        svg_text(ctx.svg, x + layout.cell_width / 2, y + 40, item->label, 16, "#333", "middle", true);
        
        // 描述
        if (item->desc.has_value()) {
            svg_text(ctx.svg, x + layout.cell_width / 2, y + 65, item->desc.value(), 12, "#666", "middle");
        }
        
        // 数值
        if (item->value.has_value()) {
            svg_text(ctx.svg, x + layout.cell_width / 2, y + 90, 
                    std::to_string(static_cast<int>(item->value.value())), 20, color, "middle", true);
        }
    }
}

// ========== ListRowsRenderer ==========

void ListRowsRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int row_height = 80;
    int margin = 10;
    
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        std::string color = get_color(infographic.theme, i);
        int y = ctx.content_start_y + static_cast<int>(i) * (row_height + margin);
        
        // 行背景
        svg_rect(ctx.svg, 50, y, 700, row_height, color, 6, 0.1f);
        
        // 序号圆圈
        svg_circle(ctx.svg, 100, y + row_height / 2, 20, color);
        svg_text(ctx.svg, 100, y + row_height / 2 + 5, std::to_string(i + 1), 14, "white", "middle", true);
        
        // 内容
        svg_text(ctx.svg, 140, y + row_height / 2 - 5, item->label, 16, "#333", "start", true);
        
        if (item->desc.has_value()) {
            svg_text(ctx.svg, 140, y + row_height / 2 + 15, item->desc.value(), 12, "#666");
        }
        
        // 右箭头
        ctx.svg << "<path d=\"M 720 " << (y + row_height / 2 - 8) << " L 730 " << (y + row_height / 2)
                << " L 720 " << (y + row_height / 2 + 8) << "\" stroke=\"" << color
                << "\" stroke-width=\"2\" fill=\"none\"/>\n";
    }
}

// ========== ListColumnRenderer ==========

void ListColumnRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    int item_height = 50;
    int margin = 5;
    
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        std::string color = get_color(infographic.theme, i);
        int y = ctx.content_start_y + static_cast<int>(i) * (item_height + margin);
        
        // 复选框
        svg_rect(ctx.svg, 100, y + 15, 20, 20, "none", 3, 1.0f, color, 2);
        
        // 勾选标记
        if (item->done.has_value() && item->done.value()) {
            ctx.svg << "<path d=\"M 105 " << (y + 25) << " L 110 " << (y + 30)
                    << " L 115 " << (y + 20) << "\" stroke=\"" << color
                    << "\" stroke-width=\"2\" fill=\"none\"/>\n";
        }
        
        // 文本
        svg_text(ctx.svg, 135, y + 30, item->label, 14, "#333");
        
        if (item->desc.has_value()) {
            svg_text(ctx.svg, 135, y + 45, item->desc.value(), 10, "#999");
        }
    }
}

// ========== ListCandyGridRenderer ==========

LayoutParams ListCandyGridRenderer::get_layout_params() const {
    LayoutParams params;
    params.card_width = 160;
    params.card_height = 100;
    params.margin = 15;
    return params;
}

void ListCandyGridRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    auto params = get_layout_params();
    auto layout = calculate_grid(infographic, 4);
    layout.cell_width = params.card_width;
    layout.cell_height = params.card_height;
    
    int total_width = layout.cols * layout.cell_width + (layout.cols - 1) * params.margin;
    layout.start_x = (ctx.width - total_width) / 2;
    
    for (size_t i = 0; i < infographic.items.size(); ++i) {
        const auto& item = infographic.items[i];
        std::string color = get_color(infographic.theme, i);
        
        int col = static_cast<int>(i) % layout.cols;
        int row = static_cast<int>(i) / layout.cols;
        int x = layout.start_x + col * (layout.cell_width + params.margin);
        int y = ctx.content_start_y + row * (layout.cell_height + params.margin);
        
        // 糖果色圆角卡片
        svg_rect(ctx.svg, x, y, layout.cell_width, layout.cell_height, color, 20, 0.8f);
        
        // 白色文本
        svg_text(ctx.svg, x + layout.cell_width / 2, y + layout.cell_height / 2 - 5,
                item->label, 14, "white", "middle", true);
        
        if (item->desc.has_value()) {
            svg_text(ctx.svg, x + layout.cell_width / 2, y + layout.cell_height / 2 + 15,
                    item->desc.value(), 10, "white", "middle");
        }
    }
}

} // namespace flex::modules::infographic
