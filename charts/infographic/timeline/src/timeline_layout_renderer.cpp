#include <layout_renderer.h>
#include <render_helpers.h>
#include <algorithm>
#include <sstream>

namespace flex::modules::infographic {

void TimelineLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    const float START_X = 100, START_Y = 100, SPACING = 150;
    const float ICON_SIZE = 24;

    float line_y = START_Y;
    float line_start_x = START_X;
    float line_end_x = START_X + info.items.size() * SPACING;
    if (ctx.layout && ctx.layout->nodes.size() == info.items.size()) {
        float min_x = ctx.layout->nodes[0].bounds.x + ctx.layout->nodes[0].bounds.width / 2.0f;
        float max_x = min_x;
        float sum_y = 0.0f;
        for (const auto& node : ctx.layout->nodes) {
            float cx = node.bounds.x + node.bounds.width / 2.0f;
            float cy = node.bounds.y + node.bounds.height / 2.0f;
            min_x = std::min(min_x, cx);
            max_x = std::max(max_x, cx);
            sum_y += cy;
        }
        line_start_x = min_x;
        line_end_x = max_x;
        line_y = sum_y / static_cast<float>(ctx.layout->nodes.size());
    }

    auto line = ctx.arena.create<flex::Shape>();
    std::ostringstream path;
    path << "M 0 0 L " << (line_end_x - line_start_x) << " 0";
    line->set_path(path.str());
    line->set_stroke(flex::Color(0.7f, 0.7f, 0.7f), 3.0f);
    line->set_position(line_start_x, line_y);
    ctx.root->add_child(line);
    
    size_t idx = 0;
    for (const auto& item : info.items) {
        float x = START_X + idx * SPACING;
        float y = START_Y;
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
        label->set_font_size(14.0f);
        label->set_color(flex::Color(0.2f, 0.2f, 0.2f));
        label->set_position(x, y + 30);
        label->set_anchor(flex::Anchor::Top);
        ctx.root->add_child(label);
        
        if (item->time) {
            auto time_text = ctx.arena.create<flex::Text>();
            time_text->set_content(*item->time);
            time_text->set_font_size(11.0f);
            time_text->set_color(flex::Color(0.5f, 0.5f, 0.5f));
            time_text->set_position(x, y - 25);
            time_text->set_anchor(flex::Anchor::Bottom);
            ctx.root->add_child(time_text);
        }
        idx++;
    }
}

} // namespace flex::modules::infographic
