#include <layout_renderer.h>
#include <render_helpers.h>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace flex::modules::infographic {

void TreeLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    if (info.items.empty()) return;
    if (!ctx.layout || !ctx.root ||
        ctx.layout->nodes.size() != ctx.layout->parent_index.size())
        throw std::invalid_argument("infographic tree renderer requires a complete tree layout");

    // The layout is the sole source of geometry and parent identities. Never
    // switch to a root/first-level-only path when deeper descendants exist.
    std::vector<const DataItem*> expected_items;
    std::vector<const DataItem*> todo;
    for (auto it = info.items.rbegin(); it != info.items.rend(); ++it)
        todo.push_back(it->get());
    while (!todo.empty()) {
        const DataItem* item = todo.back();
        todo.pop_back();
        if (!item || expected_items.size() >= ctx.layout->nodes.size())
            throw std::invalid_argument("infographic tree layout/data mismatch");
        expected_items.push_back(item);
        for (auto it = item->children.rbegin(); it != item->children.rend(); ++it)
            todo.push_back(it->get());
    }
    if (expected_items.size() != ctx.layout->nodes.size())
        throw std::invalid_argument("infographic tree layout is missing descendants");
    for (std::size_t i = 0; i < expected_items.size(); ++i)
        if (ctx.layout->nodes[i].item != expected_items[i])
            throw std::invalid_argument("infographic tree layout identity mismatch");

    // Edges are added first so cards cover the line endpoints.
    for (std::size_t i = 0; i < ctx.layout->nodes.size(); ++i) {
        const int parent = ctx.layout->parent_index[i];
        if (parent == -1) continue;
        if (parent < 0 || static_cast<std::size_t>(parent) >= ctx.layout->nodes.size() ||
            static_cast<std::size_t>(parent) >= i)
            throw std::invalid_argument("invalid infographic parent index");
        const auto& pnode = ctx.layout->nodes[static_cast<std::size_t>(parent)];
        const auto& cnode = ctx.layout->nodes[i];
        const float px = pnode.bounds.x + pnode.bounds.width / 2.0f;
        const float py = pnode.bounds.y + pnode.bounds.height;
        const float cx = cnode.bounds.x + cnode.bounds.width / 2.0f;
        const float cy = cnode.bounds.y;
        std::ostringstream path;
        path << "M " << px << " " << py << " L " << cx << " " << cy;
        auto line = ctx.arena.create<flex::Shape>();
        line->set_path(path.str());
        line->set_stroke(flex::Color(0.7f, 0.7f, 0.7f), 2.0f);
        ctx.root->add_child(line);
    }

    for (std::size_t i = 0; i < ctx.layout->nodes.size(); ++i) {
        const auto& node = ctx.layout->nodes[i];
        if (!node.item)
            throw std::invalid_argument("infographic tree layout has a null item");
        auto bg = ctx.arena.create<flex::Shape>();
        bg->set_rect(node.bounds.width, node.bounds.height, 8.0f);
        bg->set_fill(get_palette_color(info.theme, i));
        bg->set_position(node.bounds.x, node.bounds.y);
        ctx.root->add_child(bg);

        auto label = ctx.arena.create<flex::Text>();
        label->set_content(node.item->label);
        label->set_font_size(12.0f);
        label->set_color(flex::Color(1, 1, 1));
        label->set_position(node.bounds.x + node.bounds.width / 2.0f,
                            node.bounds.y + node.bounds.height / 2.0f);
        label->set_anchor(flex::Anchor::Center);
        ctx.root->add_child(label);
    }
}

} // namespace flex::modules::infographic
