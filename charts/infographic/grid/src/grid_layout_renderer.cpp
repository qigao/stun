#include <layout_renderer.h>
#include <render_helpers.h>

namespace flex::modules::infographic {

void GridLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    const float CARD_W = 180, CARD_H = 120, GAP = 20, PADDING = 40;
    const float ICON_SIZE = 32, ILLUS_SIZE = 50;
    constexpr size_t cols = 3;
    
    size_t idx = 0;
    for (const auto& item : info.items) {
        float x = 0;
        float y = 0;
        float w = CARD_W;
        float h = CARD_H;
        if (ctx.layout && ctx.layout->nodes.size() == info.items.size()) {
            const auto& node = ctx.layout->nodes[idx];
            x = node.bounds.x;
            y = node.bounds.y;
            w = node.bounds.width;
            h = node.bounds.height;
        } else {
            const size_t row = idx / cols;
            const size_t col = idx % cols;
            x = PADDING + static_cast<float>(col) * (CARD_W + GAP);
            y = PADDING + 60 + static_cast<float>(row) * (CARD_H + GAP);
        }
        
        auto card = ctx.arena.create<flex::Shape>();
        card->set_rect(w, h, 8.0f);
        card->set_fill(get_palette_color(info.theme, idx));
        card->set_position(x, y);
        ctx.root->add_child(card);
        
        float label_y = y + h / 2;
        
        if (item->illus) {
            auto illus = create_illus(*item->illus, ILLUS_SIZE, ILLUS_SIZE, ctx.arena);
            illus->set_position(x + (w - ILLUS_SIZE) / 2, y + 15);
            ctx.root->add_child(illus);
            label_y = y + 15 + ILLUS_SIZE + 10;
        } else if (item->icon && has_icon(*item->icon)) {
            auto icon = create_icon(*item->icon, ICON_SIZE, "#ffffff", ctx.arena);
            if (icon) {
                icon->set_position(x + w / 2 - ICON_SIZE / 2, y + 20);
                ctx.root->add_child(icon);
                label_y = y + 20 + ICON_SIZE + 15;
            }
        }
        
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(item->label);
        label->set_font_size(16.0f);
        label->set_color(flex::Color(1, 1, 1));
        label->set_position(x + w / 2, label_y);
        label->set_anchor(flex::Anchor::Center);
        ctx.root->add_child(label);
        
        idx++;
    }
}

} // namespace flex::modules::infographic
