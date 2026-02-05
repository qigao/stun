#include <layout_renderer.h>
#include <render_helpers.h>
#include <sstream>

namespace flex::modules::infographic {

void TreeLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    if (info.items.empty()) return;
    
    const float NODE_W = 120, NODE_H = 40, H_GAP = 30, V_GAP = 60;
    const float START_Y = 80;
    
    auto& rootItem = info.items[0];
    float root_x = 250;
    
    auto rootBg = ctx.arena.create<flex::Shape>();
    rootBg->set_rect(NODE_W, NODE_H, 8.0f);
    rootBg->set_fill(get_palette_color(info.theme, 0));
    rootBg->set_position(root_x - NODE_W / 2, START_Y);
    ctx.root->add_child(rootBg);
    
    auto rootLabel = ctx.arena.create<flex::Text>();
    rootLabel->set_content(rootItem->label);
    rootLabel->set_font_size(14.0f);
    rootLabel->set_color(flex::Color(1, 1, 1));
    rootLabel->set_position(root_x, START_Y + NODE_H / 2);
    rootLabel->set_anchor(flex::Anchor::Center);
    ctx.root->add_child(rootLabel);
    
    size_t n = rootItem->children.size();
    if (n == 0) return;
    
    float total_w = n * NODE_W + (n - 1) * H_GAP;
    float start_x = root_x - total_w / 2;
    float child_y = START_Y + NODE_H + V_GAP;
    
    for (size_t i = 0; i < n; ++i) {
        float cx = start_x + i * (NODE_W + H_GAP) + NODE_W / 2;
        
        std::ostringstream path;
        path << "M " << root_x << " " << (START_Y + NODE_H) << " L " << cx << " " << child_y;
        auto line = ctx.arena.create<flex::Shape>();
        line->set_path(path.str());
        line->set_stroke(flex::Color(0.7f, 0.7f, 0.7f), 2.0f);
        ctx.root->add_child(line);
        
        auto bg = ctx.arena.create<flex::Shape>();
        bg->set_rect(NODE_W, NODE_H, 6.0f);
        bg->set_fill(get_palette_color(info.theme, i + 1));
        bg->set_position(cx - NODE_W / 2, child_y);
        ctx.root->add_child(bg);
        
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(rootItem->children[i]->label);
        label->set_font_size(12.0f);
        label->set_color(flex::Color(1, 1, 1));
        label->set_position(cx, child_y + NODE_H / 2);
        label->set_anchor(flex::Anchor::Center);
        ctx.root->add_child(label);
    }
}

} // namespace flex::modules::infographic
