#include <layout_renderer.h>
#include <render_helpers.h>
#include <algorithm>
#include <sstream>

namespace flex::modules::infographic {

void RoadmapLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    const float START_Y = 80, STEP_H = 90, LINE_X = 130;
    const float ICON_SIZE = 24;

    float line_x = LINE_X;
    float line_start_y = START_Y;
    float line_end_y = START_Y + info.items.size() * STEP_H - 30;
    if (ctx.layout && ctx.layout->nodes.size() == info.items.size()) {
        float sum_x = 0.0f;
        float min_y = ctx.layout->nodes[0].bounds.y;
        float max_y = min_y;
        for (const auto& node : ctx.layout->nodes) {
            float cx = node.bounds.x + node.bounds.width / 2.0f;
            float cy = node.bounds.y + node.bounds.height / 2.0f;
            sum_x += cx;
            min_y = std::min(min_y, cy);
            max_y = std::max(max_y, cy);
        }
        line_x = sum_x / static_cast<float>(ctx.layout->nodes.size());
        line_start_y = min_y;
        line_end_y = max_y;
    }

    std::ostringstream path;
    path << "M " << line_x << " " << line_start_y << " L " << line_x << " " << line_end_y;
    auto line = ctx.arena.create<flex::Shape>();
    line->set_path(path.str());
    line->set_stroke(flex::Color(0.8f, 0.8f, 0.8f), 3.0f);
    ctx.root->add_child(line);
    
    size_t idx = 0;
    for (const auto& item : info.items) {
        float y = START_Y + idx * STEP_H;
        float x = line_x;
        if (ctx.layout && ctx.layout->nodes.size() == info.items.size()) {
            const auto& node = ctx.layout->nodes[idx];
            x = node.bounds.x + node.bounds.width / 2.0f;
            y = node.bounds.y + node.bounds.height / 2.0f;
        }
        
        if (item->icon && has_icon(*item->icon)) {
            auto icon = create_icon(*item->icon, ICON_SIZE, "#333333", ctx.arena);
            if (icon) {
                icon->set_position(x - ICON_SIZE / 2, y - ICON_SIZE / 2);
                ctx.root->add_child(icon);
            }
        } else {
            auto dot = ctx.arena.create<flex::Shape>();
            dot->set_circle(12.0f);
            dot->set_fill(get_palette_color(info.theme, idx));
            dot->set_position(x, y);
            ctx.root->add_child(dot);
        }
        
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(item->label);
        label->set_font_size(15.0f);
        label->set_font_weight(flex::FontWeight::Bold);
        label->set_color(flex::Color(0.2f, 0.2f, 0.2f));
        label->set_position(x + 30, y - 5);
        ctx.root->add_child(label);
        
        if (item->desc) {
            auto desc = ctx.arena.create<flex::Text>();
            desc->set_content(*item->desc);
            desc->set_font_size(12.0f);
            desc->set_color(flex::Color(0.5f, 0.5f, 0.5f));
            desc->set_position(x + 30, y + 18);
            ctx.root->add_child(desc);
        }
        idx++;
    }
}

} // namespace flex::modules::infographic
