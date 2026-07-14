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
    float root_y = START_Y;
    float root_w = NODE_W;
    float root_h = NODE_H;
    size_t expected_nodes = info.items.size();
    if (!info.items.empty() && info.items.size() == 1) {
        expected_nodes = 1 + info.items[0]->children.size();
    }
    if (ctx.layout && ctx.layout->nodes.size() == expected_nodes && ctx.layout->parent_index.size() == ctx.layout->nodes.size()) {
        for (size_t i = 0; i < ctx.layout->nodes.size(); ++i) {
            int parent = ctx.layout->parent_index[i];
            if (parent < 0 || static_cast<size_t>(parent) >= ctx.layout->nodes.size()) continue;
            const auto& pnode = ctx.layout->nodes[parent];
            const auto& cnode = ctx.layout->nodes[i];
            float px = pnode.bounds.x + pnode.bounds.width / 2.0f;
            float py = pnode.bounds.y + pnode.bounds.height;
            float cx = cnode.bounds.x + cnode.bounds.width / 2.0f;
            float cy = cnode.bounds.y;
            std::ostringstream path;
            path << "M " << px << " " << py << " L " << cx << " " << cy;
            auto line = ctx.arena.create<flex::Shape>();
            line->set_path(path.str());
            line->set_stroke(flex::Color(0.7f, 0.7f, 0.7f), 2.0f);
            ctx.root->add_child(line);
        }

        for (size_t i = 0; i < ctx.layout->nodes.size(); ++i) {
            const auto& node = ctx.layout->nodes[i];
            const auto* item = node.item;
            auto bg = ctx.arena.create<flex::Shape>();
            bg->set_rect(node.bounds.width, node.bounds.height, 8.0f);
            bg->set_fill(get_palette_color(info.theme, i));
            bg->set_position(node.bounds.x, node.bounds.y);
            ctx.root->add_child(bg);

            auto label = ctx.arena.create<flex::Text>();
            label->set_content(item ? item->label : "");
            label->set_font_size(12.0f);
            label->set_color(flex::Color(1, 1, 1));
            label->set_position(node.bounds.x + node.bounds.width / 2.0f,
                                node.bounds.y + node.bounds.height / 2.0f);
            label->set_anchor(flex::Anchor::Center);
            ctx.root->add_child(label);
        }
        return;
    }
    if (ctx.layout && ctx.layout->nodes.size() == expected_nodes) {
        const auto& node = ctx.layout->nodes[0];
        root_x = node.bounds.x + node.bounds.width / 2.0f;
        root_y = node.bounds.y;
        root_w = node.bounds.width;
        root_h = node.bounds.height;
    }
    
    auto rootBg = ctx.arena.create<flex::Shape>();
    rootBg->set_rect(root_w, root_h, 8.0f);
    rootBg->set_fill(get_palette_color(info.theme, 0));
    rootBg->set_position(root_x - root_w / 2, root_y);
    ctx.root->add_child(rootBg);
    
    auto rootLabel = ctx.arena.create<flex::Text>();
    rootLabel->set_content(rootItem->label);
    rootLabel->set_font_size(14.0f);
    rootLabel->set_color(flex::Color(1, 1, 1));
    rootLabel->set_position(root_x, root_y + root_h / 2);
    rootLabel->set_anchor(flex::Anchor::Center);
    ctx.root->add_child(rootLabel);
    
    size_t n = rootItem->children.size();
    if (n == 0) return;
    
    float total_w = n * NODE_W + (n - 1) * H_GAP;
    float start_x = root_x - total_w / 2;
    float child_y = root_y + root_h + V_GAP;
    
    for (size_t i = 0; i < n; ++i) {
        float cx = start_x + i * (NODE_W + H_GAP) + NODE_W / 2;
        float cy = child_y;
        float cw = NODE_W;
        float ch = NODE_H;
        if (ctx.layout && ctx.layout->nodes.size() == expected_nodes) {
            size_t node_index = i + 1;
            if (node_index < ctx.layout->nodes.size()) {
                const auto& node = ctx.layout->nodes[node_index];
                cx = node.bounds.x + node.bounds.width / 2.0f;
                cy = node.bounds.y;
                cw = node.bounds.width;
                ch = node.bounds.height;
            }
        }
        
        std::ostringstream path;
        path << "M " << root_x << " " << (root_y + root_h) << " L " << cx << " " << cy;
        auto line = ctx.arena.create<flex::Shape>();
        line->set_path(path.str());
        line->set_stroke(flex::Color(0.7f, 0.7f, 0.7f), 2.0f);
        ctx.root->add_child(line);
        
        auto bg = ctx.arena.create<flex::Shape>();
        bg->set_rect(cw, ch, 6.0f);
        bg->set_fill(get_palette_color(info.theme, i + 1));
        bg->set_position(cx - cw / 2, cy);
        ctx.root->add_child(bg);
        
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(rootItem->children[i]->label);
        label->set_font_size(12.0f);
        label->set_color(flex::Color(1, 1, 1));
        label->set_position(cx, cy + ch / 2);
        label->set_anchor(flex::Anchor::Center);
        ctx.root->add_child(label);
    }
}

} // namespace flex::modules::infographic
