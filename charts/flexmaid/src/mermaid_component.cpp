#include <flex/modules/flexmaid/mermaid_component.h>
#include <flex/modules/flexmaid/flexmaid.h>
#include <flex/runtime/group.h>
#include <flex/runtime/shape.h>
#include <flex/runtime/text.h>
#include <flex/runtime/component.h>
#include <flex.h>
#include <sstream>
#include <cmath>

namespace flex::modules::flexmaid {

namespace {

using DiagramNode = flex::modules::flexmaid::Node;  // Mermaid node
using FlexNode = flex::Node;  // Flex runtime node

flex::Color to_flex_color(const std::string& hex) {
    if (hex.empty() || hex[0] != '#') return flex::Color(0.2f, 0.2f, 0.2f);
    uint32_t val = std::stoul(hex.substr(1), nullptr, 16);
    if (hex.length() == 7) {
        return flex::Color(((val >> 16) & 0xFF) / 255.0f,
                     ((val >> 8) & 0xFF) / 255.0f,
                     (val & 0xFF) / 255.0f);
    }
    return flex::Color(0.2f, 0.2f, 0.2f);
}

flex::Shape* create_node_shape(const DiagramNode& node, const LayoutData::Bounds& b, 
                         flex::ArenaAllocator& arena, const Theme& theme) {
    auto shape = arena.create<flex::Shape>();
    shape->set_position(b.left(), b.top());
    shape->set_fill(to_flex_color(theme.primary_color));
    shape->set_stroke(to_flex_color(theme.line_color), theme.line_width);

    switch (node.shape) {
        case NodeShape::Rectangle:
        case NodeShape::Class:
        case NodeShape::Entity:
            shape->set_rect(b.width, b.height);
            break;
        case NodeShape::RoundRect:
        case NodeShape::State:
            shape->set_rect(b.width, b.height, 10.0f);
            break;
        case NodeShape::Circle:
        case NodeShape::CircleFilled:
            shape->set_circle(std::min(b.width, b.height) / 2);
            shape->set_position(b.x, b.y);
            break;
        case NodeShape::Diamond: {
            std::ostringstream path;
            path << "M " << b.width/2 << " 0 "
                 << "L " << b.width << " " << b.height/2 << " "
                 << "L " << b.width/2 << " " << b.height << " "
                 << "L 0 " << b.height/2 << " Z";
            shape->set_path(path.str());
            break;
        }
        case NodeShape::Stadium:
            shape->set_rect(b.width, b.height, b.height / 2);
            break;
        default:
            shape->set_rect(b.width, b.height);
            break;
    }
    return shape;
}

flex::Text* create_node_label(const DiagramNode& node, const LayoutData::Bounds& b,
                        flex::ArenaAllocator& arena, const Theme& theme) {
    auto text = arena.create<flex::Text>();
    text->set_content(node.label);
    text->set_font_size((float)theme.font_size);
    text->set_color(to_flex_color(theme.text_color));
    text->set_position(b.x, b.y);
    text->set_anchor(flex::Anchor::Center);
    return text;
}

flex::Shape* create_edge_path(const Edge& edge, const LayoutData::Path& path,
                        flex::ArenaAllocator& arena, const Theme& theme) {
    if (path.points.size() < 2) return nullptr;
    
    std::ostringstream d;
    d << "M " << path.points[0].first << " " << path.points[0].second;
    for (size_t i = 1; i < path.points.size(); ++i) {
        d << " L " << path.points[i].first << " " << path.points[i].second;
    }
    
    auto shape = arena.create<flex::Shape>();
    shape->set_path(d.str());
    shape->set_stroke(to_flex_color(theme.line_color), theme.line_width);
    shape->set_fill(flex::Color(0, 0, 0, 0));
    
    return shape;
}

flex::Shape* create_arrow(float x, float y, float angle, flex::ArenaAllocator& arena, const Theme& theme) {
    auto arrow = arena.create<flex::Shape>();
    arrow->set_path("M -8 -4 L 0 0 L -8 4 Z");
    arrow->set_fill(to_flex_color(theme.line_color));
    arrow->set_position(x, y);
    arrow->set_rotation(angle);
    return arrow;
}

} // namespace

void MermaidComponent::register_component() {
    flex::ComponentRegistry::instance().register_component("Mermaid", [](const flex::Props& props) {
        std::string source = flex::get_prop_string(props, "source", "");
        if (source.empty()) return std::shared_ptr<FlexNode>(nullptr);
        return std::shared_ptr<FlexNode>(nullptr);
    });
}

flex::Group* MermaidComponent::build(const UnifiedDiagram& diagram, flex::Instance& instance) {
    FlexMaid maid;
    auto layout_result = maid.layout(diagram);
    if (!layout_result.success) return nullptr;
    return build(diagram, layout_result.data, instance, maid.get_theme());
}

flex::Group* MermaidComponent::build(const UnifiedDiagram& diagram, const LayoutData& layout,
                                flex::Instance& instance, const Theme& theme) {
    flex::ArenaAllocator& arena = *instance.object_allocator();
    
    auto root = flex::Group::create(arena);
    root->set_id("mermaid-root");
    root->set_layout_size(layout.width, layout.height);
    
    auto bg = arena.create<flex::Shape>();
    bg->set_rect(layout.width, layout.height);
    bg->set_fill(to_flex_color(theme.background_color));
    root->add_child(bg);

    auto edges_layer = flex::Group::create(arena);
    edges_layer->set_id("edges");
    root->add_child(edges_layer);
    
    auto nodes_layer = flex::Group::create(arena);
    nodes_layer->set_id("nodes");
    root->add_child(nodes_layer);

    for (size_t i = 0; i < diagram.edges.size() && i < layout.edge_paths.size(); ++i) {
        const auto& edge = diagram.edges[i];
        const auto& path = layout.edge_paths[i];
        
        if (auto line = create_edge_path(edge, path, arena, theme)) {
            edges_layer->add_child(line);
        }
        
        if (edge.end_decoration == EdgeDecoration::Arrow && path.points.size() >= 2) {
            auto& p1 = path.points[path.points.size() - 2];
            auto& p2 = path.points.back();
            float angle = std::atan2(p2.second - p1.second, p2.first - p1.first);
            edges_layer->add_child(create_arrow(p2.first, p2.second, angle, arena, theme));
        }
    }

    for (const auto& node : diagram.nodes) {
        auto it = layout.node_bounds.find(node.id);
        if (it == layout.node_bounds.end()) continue;
        const auto& bounds = it->second;
        
        nodes_layer->add_child(create_node_shape(node, bounds, arena, theme));
        nodes_layer->add_child(create_node_label(node, bounds, arena, theme));
    }

    return root;
}

} // namespace flex::modules::flexmaid
