#include <layout_renderer.h>
#include <render_helpers.h>
#include <sstream>

namespace flex::modules::infographic {

void TimelineLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    const float START_X = 100, START_Y = 100, SPACING = 150;
    const float ICON_SIZE = 24;
    
    auto line = ctx.arena.create<flex::Shape>();
    float line_len = info.items.size() * SPACING;
    std::ostringstream path;
    path << "M 0 0 L " << line_len << " 0";
    line->set_path(path.str());
    line->set_stroke(flex::Color(0.7f, 0.7f, 0.7f), 3.0f);
    line->set_position(START_X, START_Y);
    ctx.root->add_child(line);
    
    size_t idx = 0;
    for (const auto& item : info.items) {
        float x = START_X + idx * SPACING;
        
        if (item->icon && has_icon(*item->icon)) {
            auto icon = create_icon(*item->icon, ICON_SIZE, "#333333", ctx.arena);
            if (icon) {
                icon->set_position(x - ICON_SIZE / 2, START_Y - ICON_SIZE / 2);
                ctx.root->add_child(icon);
            }
        } else {
            auto dot = ctx.arena.create<flex::Shape>();
            dot->set_circle(12.0f);
            dot->set_fill(get_palette_color(info.theme, idx));
            dot->set_position(x, START_Y);
            ctx.root->add_child(dot);
        }
        
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(item->label);
        label->set_font_size(14.0f);
        label->set_color(flex::Color(0.2f, 0.2f, 0.2f));
        label->set_position(x, START_Y + 30);
        label->set_anchor(flex::Anchor::Top);
        ctx.root->add_child(label);
        
        if (item->time) {
            auto time_text = ctx.arena.create<flex::Text>();
            time_text->set_content(*item->time);
            time_text->set_font_size(11.0f);
            time_text->set_color(flex::Color(0.5f, 0.5f, 0.5f));
            time_text->set_position(x, START_Y - 25);
            time_text->set_anchor(flex::Anchor::Bottom);
            ctx.root->add_child(time_text);
        }
        idx++;
    }
}

} // namespace flex::modules::infographic
