#include <layout_renderer.h>
#include <render_helpers.h>

namespace flex::modules::infographic {

void FunnelLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    const float CENTER_X = 300, START_Y = 80, STEP_H = 60;
    float max_w = 400;
    
    size_t idx = 0;
    for (const auto& item : info.items) {
        float ratio = 1.0f - (float)idx / (info.items.size() + 1);
        float w = max_w * ratio;
        float y = START_Y + idx * STEP_H;
        
        auto seg = ctx.arena.create<flex::Shape>();
        seg->set_rect(w, STEP_H - 5, 4.0f);
        seg->set_fill(get_palette_color(info.theme, idx));
        seg->set_position(CENTER_X - w / 2, y);
        ctx.root->add_child(seg);
        
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(item->label);
        label->set_font_size(14.0f);
        label->set_color(flex::Color(1, 1, 1));
        label->set_position(CENTER_X, y + (STEP_H - 5) / 2);
        label->set_anchor(flex::Anchor::Center);
        ctx.root->add_child(label);
        
        idx++;
    }
}

} // namespace flex::modules::infographic
