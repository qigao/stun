#include <renderer/hierarchy_renderer.h>
#include <layout/layout_engine.h>
#include <algorithm>

namespace flex::modules::infographic {

void HierarchyTreeRenderer::render_content(SvgContext& ctx, const UnifiedInfographic& infographic) {
    auto layout_engine = create_layout_engine(infographic.template_type);
    if (!layout_engine) return;

    const std::string template_name = template_type_to_string(infographic.template_type);
    const StyleConfig style = parse_style_from_template(template_name);
    LayoutResult layout = layout_engine->compute(infographic, ctx.width, ctx.height, style);
    if (layout.nodes.empty()) return;

    if (layout.parent_index.size() == layout.nodes.size()) {
        for (size_t i = 0; i < layout.nodes.size(); ++i) {
            int parent = layout.parent_index[i];
            if (parent < 0 || static_cast<size_t>(parent) >= layout.nodes.size()) continue;
            const auto& pnode = layout.nodes[parent];
            const auto& cnode = layout.nodes[i];
            const double px = pnode.bounds.x + pnode.bounds.width / 2.0;
            const double py = pnode.bounds.y + pnode.bounds.height;
            const double cx = cnode.bounds.x + cnode.bounds.width / 2.0;
            const double cy = cnode.bounds.y;
            svg_line(ctx.svg,
                     static_cast<int>(px),
                     static_cast<int>(py),
                     static_cast<int>(cx),
                     static_cast<int>(cy),
                     "#cbd5e1",
                     2);
        }
    }

    for (size_t i = 0; i < layout.nodes.size(); ++i) {
        const auto& node = layout.nodes[i];
        const auto* item = node.item ? node.item : nullptr;
        const std::string color = get_color(infographic.theme, i);

        svg_rect(ctx.svg,
                 static_cast<int>(node.bounds.x),
                 static_cast<int>(node.bounds.y),
                 static_cast<int>(node.bounds.width),
                 static_cast<int>(node.bounds.height),
                 color,
                 8,
                 1.0f,
                 "#ffffff",
                 0);

        svg_text(ctx.svg,
                 static_cast<int>(node.bounds.x + node.bounds.width / 2.0),
                 static_cast<int>(node.bounds.y + node.bounds.height / 2.0 + 5),
                 item ? item->label : "",
                 14,
                 "#ffffff",
                 "middle",
                 true);
    }
}

} // namespace flex::modules::infographic
