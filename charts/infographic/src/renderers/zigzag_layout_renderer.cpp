#include <layout_renderer.h>
#include <render_helpers.h>
#include <sstream>

namespace flex::modules::infographic {

void ZigzagLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    const float CARD_W = 200, CARD_H = 80, V_GAP = 30, START_Y = 80;
    float left_x = 80, right_x = 280;
    
    size_t idx = 0;
    for (const auto& item : info.items) {
        bool is_left = (idx % 2 == 0);
        float x = is_left ? left_x : right_x;
        float y = START_Y + idx * (CARD_H + V_GAP);
        
        auto card = ctx.arena.create<flex::Shape>();
        card->set_rect(CARD_W, CARD_H, 8.0f);
        card->set_fill(get_palette_color(info.theme, idx));
        card->set_position(x, y);
        ctx.root->add_child(card);
        
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(item->label);
        label->set_font_size(14.0f);
        label->set_font_weight(flex::FontWeight::Bold);
        label->set_color(flex::Color(1, 1, 1));
        label->set_position(x + CARD_W / 2, y + 20);
        label->set_anchor(flex::Anchor::Top);
        ctx.root->add_child(label);
        
        if (item->desc) {
            auto desc = ctx.arena.create<flex::Text>();
            desc->set_content(*item->desc);
            desc->set_font_size(11.0f);
            desc->set_color(flex::Color(1, 1, 1, 0.85f));
            desc->set_position(x + CARD_W / 2, y + 50);
            desc->set_anchor(flex::Anchor::Top);
            ctx.root->add_child(desc);
        }
        
        if (idx < info.items.size() - 1) {
            float next_x = !is_left ? left_x : right_x;
            float next_y = y + CARD_H + V_GAP;
            std::ostringstream path;
            path << "M " << (x + CARD_W / 2) << " " << (y + CARD_H)
                 << " Q " << ((x + next_x + CARD_W) / 2) << " " << (y + CARD_H + V_GAP / 2)
                 << " " << (next_x + CARD_W / 2) << " " << next_y;
            auto line = ctx.arena.create<flex::Shape>();
            line->set_path(path.str());
            line->set_stroke(flex::Color(0.6f, 0.6f, 0.6f), 2.0f);
            ctx.root->add_child(line);
        }
        idx++;
    }
}

} // namespace flex::modules::infographic
