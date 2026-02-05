#include <layout_renderer.h>
#include <render_helpers.h>
#include <sstream>

namespace flex::modules::infographic {

void RoadmapLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    const float START_Y = 80, STEP_H = 90, LINE_X = 130;
    const float ICON_SIZE = 24;
    
    std::ostringstream path;
    path << "M " << LINE_X << " " << START_Y << " L " << LINE_X << " " << (START_Y + info.items.size() * STEP_H - 30);
    auto line = ctx.arena.create<flex::Shape>();
    line->set_path(path.str());
    line->set_stroke(flex::Color(0.8f, 0.8f, 0.8f), 3.0f);
    ctx.root->add_child(line);
    
    size_t idx = 0;
    for (const auto& item : info.items) {
        float y = START_Y + idx * STEP_H;
        
        if (item->icon && has_icon(*item->icon)) {
            auto icon = create_icon(*item->icon, ICON_SIZE, "#333333", ctx.arena);
            if (icon) {
                icon->set_position(LINE_X - ICON_SIZE / 2, y - ICON_SIZE / 2);
                ctx.root->add_child(icon);
            }
        } else {
            auto dot = ctx.arena.create<flex::Shape>();
            dot->set_circle(12.0f);
            dot->set_fill(get_palette_color(info.theme, idx));
            dot->set_position(LINE_X, y);
            ctx.root->add_child(dot);
        }
        
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(item->label);
        label->set_font_size(15.0f);
        label->set_font_weight(flex::FontWeight::Bold);
        label->set_color(flex::Color(0.2f, 0.2f, 0.2f));
        label->set_position(LINE_X + 30, y - 5);
        ctx.root->add_child(label);
        
        if (item->desc) {
            auto desc = ctx.arena.create<flex::Text>();
            desc->set_content(*item->desc);
            desc->set_font_size(12.0f);
            desc->set_color(flex::Color(0.5f, 0.5f, 0.5f));
            desc->set_position(LINE_X + 30, y + 18);
            ctx.root->add_child(desc);
        }
        idx++;
    }
}

} // namespace flex::modules::infographic
