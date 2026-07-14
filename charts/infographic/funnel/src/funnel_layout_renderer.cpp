#include <layout_renderer.h>
#include <render_helpers.h>

namespace flex::modules::infographic {

void FunnelLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    const float CENTER_X = 300, START_Y = 80, STEP_H = 60;
    float max_w = 400;
    
    size_t idx = 0;
    for (const auto& item : info.items) {
        float w = 0.0f;
        float h = STEP_H - 5;
        float x = 0.0f;
        float y = 0.0f;
        float center_x = CENTER_X;
        if (ctx.layout && ctx.layout->nodes.size() == info.items.size()) {
            const auto& node = ctx.layout->nodes[idx];
            w = node.bounds.width;
            h = node.bounds.height;
            x = node.bounds.x;
            y = node.bounds.y;
            center_x = x + w / 2.0f;
        } else {
            float ratio = 1.0f - (float)idx / (info.items.size() + 1);
            w = max_w * ratio;
            x = CENTER_X - w / 2;
            y = START_Y + idx * STEP_H;
        }
        
        auto seg = ctx.arena.create<flex::Shape>();
        seg->set_rect(w, h, 4.0f);
        seg->set_fill(get_palette_color(info.theme, idx));
        seg->set_position(x, y);
        ctx.root->add_child(seg);
        
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(item->label);
        label->set_font_size(14.0f);
        label->set_color(flex::Color(1, 1, 1));
        label->set_position(center_x, y + h / 2);
        label->set_anchor(flex::Anchor::Center);
        ctx.root->add_child(label);
        
        idx++;
    }
}

} // namespace flex::modules::infographic
