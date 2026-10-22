#include <layout/layout_engine.h>
#include <ir/unified_infographic.h>
#include <stun/graphlayout/projection.h>
#include <stun/graphlayout/tidy_tree.h>
#include <cmath>
#include <algorithm>
#include <memory>
#include <limits>
#include <stdexcept>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flex::modules::infographic {

namespace {

// Graph-layout geometry is owned by Stun, not by the rendering module.
void apply_graph_projection(LayoutResult& result) {
    const std::size_t count = result.nodes.size();
    if (count < 2) return;
    stun::graphlayout::Graph graph;
    stun::graphlayout::Layout desired;
    graph.nodes.reserve(count);
    desired.nodes.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto& b = result.nodes[i].bounds;
        if (!std::isfinite(b.x) || !std::isfinite(b.y) ||
            !std::isfinite(b.width) || !std::isfinite(b.height) ||
            b.width <= 0 || b.height <= 0)
            throw std::invalid_argument("invalid infographic rectangle geometry");
        graph.nodes.push_back({"infographic:" + std::to_string(i), b.width, b.height});
        desired.nodes.push_back({b.x, b.y, b.width, b.height, 0, 0});
    }
    stun::graphlayout::ProjectionOptions options;
    options.clearance = 1.0; // Keep a visible gap after float conversion.
    stun::graphlayout::Layout projected;
    const auto status = stun::graphlayout::project_graph(
        graph, desired, {}, projected, options);
    if (!status)
        throw std::invalid_argument("infographic graph projection: " + status.message);
    double min_x = projected.nodes.front().x;
    double min_y = projected.nodes.front().y;
    for (const auto& node : projected.nodes) {
        min_x = std::min(min_x, node.x);
        min_y = std::min(min_y, node.y);
    }
    const double offset_x = min_x < 0 ? -min_x : 0;
    const double offset_y = min_y < 0 ? -min_y : 0;
    for (std::size_t i = 0; i < count; ++i) {
        auto& b = result.nodes[i].bounds;
        const double x = projected.nodes[i].x + offset_x;
        const double y = projected.nodes[i].y + offset_y;
        if (!std::isfinite(x) || !std::isfinite(y) ||
            x > std::numeric_limits<float>::max() ||
            y > std::numeric_limits<float>::max())
            throw std::overflow_error("infographic projected coordinates exceed float range");
        b.x = static_cast<float>(x);
        b.y = static_cast<float>(y);
    }
}

void ensure_canvas_contains_nodes(LayoutResult& result) {
    for (const auto& node : result.nodes) {
        const auto& b = node.bounds;
        const double right = static_cast<double>(b.x) + b.width + 50.0;
        const double bottom = static_cast<double>(b.y) + b.height + 50.0;
        if (!std::isfinite(right) || !std::isfinite(bottom) ||
            right >= std::numeric_limits<int>::max() ||
            bottom >= std::numeric_limits<int>::max())
            throw std::overflow_error("infographic layout exceeds canvas capacity");
        result.canvas_width = std::max(result.canvas_width, static_cast<int>(std::ceil(right)));
        result.canvas_height = std::max(result.canvas_height, static_cast<int>(std::ceil(bottom)));
    }
}

} // namespace

// ============================================================================
// Grid Layout
// ============================================================================

LayoutResult GridLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                       const StyleConfig& style) {
    LayoutResult result;
    result.canvas_width = width;
    result.canvas_height = height;
    result.content_start_y = 100;
    
    int item_count = static_cast<int>(ast.items.size());
    if (item_count == 0) return result;
    
    int cols = std::min(style.max_columns, item_count);
    int rows = (item_count + cols - 1) / cols;
    
    int total_width = cols * style.card_width + (cols - 1) * style.item_spacing;
    int start_x = (width - total_width) / 2;
    
    for (int i = 0; i < item_count; ++i) {
        int col = i % cols;
        int row = i / cols;
        
        LayoutNode node;
        node.item = ast.items[i].get();
        node.index = i;
        node.bounds.x = static_cast<float>(start_x + col * (style.card_width + style.item_spacing));
        node.bounds.y = static_cast<float>(result.content_start_y + row * (style.card_height + style.item_spacing));
        node.bounds.width = static_cast<float>(style.card_width);
        node.bounds.height = static_cast<float>(style.card_height);
        
        result.nodes.push_back(node);
    }
    
    apply_graph_projection(result);
    result.canvas_height = result.content_start_y + rows * (style.card_height + style.item_spacing) + 50;
    ensure_canvas_contains_nodes(result);
    return result;
}

// ============================================================================
// Row Layout
// ============================================================================

LayoutResult RowLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                      const StyleConfig& style) {
    LayoutResult result;
    result.canvas_width = width;
    result.content_start_y = 100;
    
    int item_count = static_cast<int>(ast.items.size());
    int row_height = 80;
    
    for (int i = 0; i < item_count; ++i) {
        LayoutNode node;
        node.item = ast.items[i].get();
        node.index = i;
        node.bounds.x = 50;
        node.bounds.y = static_cast<float>(result.content_start_y + i * (row_height + style.item_spacing));
        node.bounds.width = static_cast<float>(width - 100);
        node.bounds.height = static_cast<float>(row_height);
        
        result.nodes.push_back(node);
    }
    
    apply_graph_projection(result);
    result.canvas_height = result.content_start_y + item_count * (row_height + style.item_spacing) + 50;
    ensure_canvas_contains_nodes(result);
    return result;
}

// ============================================================================
// Column Layout
// ============================================================================

LayoutResult ColumnLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                         const StyleConfig& style) {
    LayoutResult result;
    result.canvas_width = width;
    result.content_start_y = 100;
    
    int item_count = static_cast<int>(ast.items.size());
    int item_height = 50;
    
    for (int i = 0; i < item_count; ++i) {
        LayoutNode node;
        node.item = ast.items[i].get();
        node.index = i;
        node.bounds.x = 100;
        node.bounds.y = static_cast<float>(result.content_start_y + i * (item_height + 5));
        node.bounds.width = static_cast<float>(width - 200);
        node.bounds.height = static_cast<float>(item_height);
        
        result.nodes.push_back(node);
    }
    
    apply_graph_projection(result);
    result.canvas_height = result.content_start_y + item_count * (item_height + 5) + 50;
    ensure_canvas_contains_nodes(result);
    return result;
}

// ============================================================================
// Zigzag Layout
// ============================================================================

LayoutResult ZigzagLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                         const StyleConfig& style) {
    LayoutResult result;
    result.canvas_width = width;
    result.content_start_y = 100;
    
    int item_count = static_cast<int>(ast.items.size());
    int step_height = 100;
    int card_width = 250;
    
    for (int i = 0; i < item_count; ++i) {
        LayoutNode node;
        node.item = ast.items[i].get();
        node.index = i;
        
        // Alternate left and right
        bool is_left = (i % 2 == 0);
        node.bounds.x = is_left ? 100.0f : static_cast<float>(width - card_width - 100);
        node.bounds.y = static_cast<float>(result.content_start_y + i * step_height);
        node.bounds.width = static_cast<float>(card_width);
        node.bounds.height = 80;
        
        result.nodes.push_back(node);
    }
    
    apply_graph_projection(result);
    result.canvas_height = result.content_start_y + item_count * step_height + 50;
    ensure_canvas_contains_nodes(result);
    return result;
}

// ============================================================================
// Timeline Layout
// ============================================================================

LayoutResult TimelineLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                           const StyleConfig& style) {
    LayoutResult result;
    result.canvas_width = width;
    result.content_start_y = 120;
    
    int item_count = static_cast<int>(ast.items.size());
    if (item_count == 0) return result;
    
    int step_width = 150;
    int start_x = (width - item_count * step_width) / 2;
    
    for (int i = 0; i < item_count; ++i) {
        LayoutNode node;
        node.item = ast.items[i].get();
        node.index = i;
        node.bounds.x = static_cast<float>(start_x + i * step_width + step_width / 2 - 40);
        node.bounds.y = static_cast<float>(result.content_start_y);
        node.bounds.width = 80;
        node.bounds.height = 100;
        
        result.nodes.push_back(node);
    }
    
    apply_graph_projection(result);
    result.canvas_height = result.content_start_y + 150;
    ensure_canvas_contains_nodes(result);
    return result;
}

// ============================================================================
// Funnel Layout
// ============================================================================

LayoutResult FunnelLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                         const StyleConfig& style) {
    LayoutResult result;
    result.canvas_width = width;
    result.content_start_y = 100;
    
    int item_count = static_cast<int>(ast.items.size());
    if (item_count == 0) return result;
    
    int step_height = 80;
    int funnel_width_top = 300;
    int funnel_width_bottom = 100;
    int center_x = width / 2;
    
    for (int i = 0; i < item_count; ++i) {
        float progress = static_cast<float>(i) / std::max(1, item_count - 1);
        int current_width = funnel_width_top - static_cast<int>((funnel_width_top - funnel_width_bottom) * progress);
        
        LayoutNode node;
        node.item = ast.items[i].get();
        node.index = i;
        node.bounds.x = static_cast<float>(center_x - current_width / 2);
        node.bounds.y = static_cast<float>(result.content_start_y + i * step_height);
        node.bounds.width = static_cast<float>(current_width);
        node.bounds.height = static_cast<float>(step_height);
        
        result.nodes.push_back(node);
    }
    
    apply_graph_projection(result);
    result.canvas_height = result.content_start_y + item_count * step_height + 50;
    ensure_canvas_contains_nodes(result);
    return result;
}

// ============================================================================
// Circular Layout
// ============================================================================

LayoutResult CircularLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                           const StyleConfig& style) {
    LayoutResult result;
    result.canvas_width = width;
    result.content_start_y = 100;
    
    int item_count = static_cast<int>(ast.items.size());
    if (item_count == 0) return result;
    
    int center_x = width / 2;
    int center_y = result.content_start_y + 200;
    int radius = 150;
    
    for (int i = 0; i < item_count; ++i) {
        double angle = 2 * M_PI * i / item_count - M_PI / 2;
        
        LayoutNode node;
        node.item = ast.items[i].get();
        node.index = i;
        node.bounds.x = static_cast<float>(center_x + radius * cos(angle) - 25);
        node.bounds.y = static_cast<float>(center_y + radius * sin(angle) - 25);
        node.bounds.width = 50;
        node.bounds.height = 50;
        
        result.nodes.push_back(node);
    }
    
    apply_graph_projection(result);
    result.canvas_height = center_y + radius + 100;
    ensure_canvas_contains_nodes(result);
    return result;
}

// ============================================================================
// Tree Layout
// ============================================================================

LayoutResult TreeLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                       const StyleConfig& style) {
    if (width <= 0 || height <= 0 || style.card_width <= 0 || style.card_height <= 0)
        throw std::invalid_argument("infographic tree requires positive canvas and card dimensions");

    LayoutResult result;
    result.canvas_width = width;
    result.canvas_height = height;
    result.content_start_y = 100;
    if (ast.items.empty()) return result;

    using namespace stun::graphlayout;
    Tree forest;
    std::vector<const DataItem*> items;
    std::vector<std::pair<const DataItem*, std::size_t>> todo;
    for (auto it = ast.items.rbegin(); it != ast.items.rend(); ++it)
        todo.push_back({it->get(), TreeNoParent});

    // Explicit stack bounds traversal depth and preserves the source's ordered
    // children without recursive C++ calls on very deep trees.
    while (!todo.empty()) {
        const auto current = todo.back();
        todo.pop_back();
        if (!current.first)
            throw std::invalid_argument("infographic tree has a null data item");
        if (forest.nodes.size() >= 2048 ||
            forest.nodes.size() >= static_cast<std::size_t>(std::numeric_limits<int>::max()))
            throw std::overflow_error("infographic tree node count exceeds layout capacity");
        const auto index = forest.nodes.size();
        forest.nodes.push_back({"infographic:" + std::to_string(index),
                                static_cast<double>(style.card_width),
                                static_cast<double>(style.card_height), current.second});
        items.push_back(current.first);
        for (auto it = current.first->children.rbegin();
             it != current.first->children.rend(); ++it)
            todo.push_back({it->get(), index});
    }

    TidyTreeOptions options;
    options.sibling_gap = static_cast<double>(std::max(10, style.item_spacing));
    options.layer_gap = static_cast<double>(std::max(20, style.item_spacing));
    options.forest_gap = options.sibling_gap * 3.0;

    TidyTreeLayout placement;
    const auto status = layout_tidy_tree(forest, placement, options);
    if (!status)
        throw std::invalid_argument("infographic tidy tree: " + status.message);

    // Preserve actual card sizes and the minimum connector spacing. An overly
    // large tree expands the canvas, rather than scaling cards until unreadable.
    const double margin = 40.0;
    const double offset_x = std::max(margin,
        (static_cast<double>(width) - placement.width) * 0.5);
    result.nodes.reserve(items.size());
    result.parent_index.reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto& p = placement.nodes[i];
        const double x = p.x + offset_x;
        const double y = p.y + static_cast<double>(result.content_start_y);
        if (!std::isfinite(x) || !std::isfinite(y) ||
            x > std::numeric_limits<float>::max() ||
            y > std::numeric_limits<float>::max())
            throw std::overflow_error("infographic tidy tree exceeds float coordinate range");
        LayoutNode node;
        node.item = items[i];
        node.index = static_cast<int>(i);
        node.bounds.x = static_cast<float>(x);
        node.bounds.y = static_cast<float>(y);
        node.bounds.width = static_cast<float>(p.width);
        node.bounds.height = static_cast<float>(p.height);
        result.nodes.push_back(std::move(node));
        result.parent_index.push_back(p.parent == TreeNoParent ? -1 : static_cast<int>(p.parent));
    }
    ensure_canvas_contains_nodes(result);
    return result;
}

// ============================================================================
// Quadrant Layout (SWOT style)
// ============================================================================

LayoutResult QuadrantLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                           const StyleConfig& style) {
    LayoutResult result;
    result.canvas_width = width;
    result.content_start_y = 100;
    
    int item_count = static_cast<int>(ast.items.size());
    int quadrant_width = (width - 60) / 2;
    int quadrant_height = 200;
    
    // 4 quadrants: top-left, top-right, bottom-left, bottom-right
    int positions[4][2] = {{20, 0}, {quadrant_width + 40, 0}, 
                           {20, quadrant_height + 20}, {quadrant_width + 40, quadrant_height + 20}};
    
    for (int i = 0; i < std::min(4, item_count); ++i) {
        LayoutNode node;
        node.item = ast.items[i].get();
        node.index = i;
        node.bounds.x = static_cast<float>(positions[i][0]);
        node.bounds.y = static_cast<float>(result.content_start_y + positions[i][1]);
        node.bounds.width = static_cast<float>(quadrant_width);
        node.bounds.height = static_cast<float>(quadrant_height);
        result.nodes.push_back(node);
    }
    
    apply_graph_projection(result);
    result.canvas_height = result.content_start_y + 2 * quadrant_height + 70;
    ensure_canvas_contains_nodes(result);
    return result;
}

// ============================================================================
// Pie Layout
// ============================================================================

LayoutResult PieLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                      const StyleConfig& style) {
    LayoutResult result;
    result.canvas_width = width;
    result.content_start_y = 100;
    
    int item_count = static_cast<int>(ast.items.size());
    if (item_count == 0) return result;
    
    // Calculate total value
    double total = 0;
    for (const auto& item : ast.items) {
        if (item->value) total += *item->value;
    }
    if (total == 0) total = item_count;  // Equal distribution if no values
    
    int center_x = width / 2;
    int center_y = result.content_start_y + 150;
    int radius = 120;
    
    double start_angle = -M_PI / 2;
    for (int i = 0; i < item_count; ++i) {
        double value = ast.items[i]->value.value_or(1.0);
        double sweep = 2 * M_PI * value / total;
        double mid_angle = start_angle + sweep / 2;
        
        LayoutNode node;
        node.item = ast.items[i].get();
        node.index = i;
        // Store arc info in bounds (x=start_angle, y=sweep, width=radius)
        node.bounds.x = static_cast<float>(start_angle);
        node.bounds.y = static_cast<float>(sweep);
        node.bounds.width = static_cast<float>(radius);
        node.bounds.height = static_cast<float>(mid_angle);  // Store mid angle for label
        
        result.nodes.push_back(node);
        start_angle += sweep;
    }
    
    result.canvas_height = center_y + radius + 100;
    ensure_canvas_contains_nodes(result);
    return result;
}

// ============================================================================
// Bar Layout
// ============================================================================

LayoutResult BarLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                      const StyleConfig& style) {
    LayoutResult result;
    result.canvas_width = width;
    result.content_start_y = 100;
    
    int item_count = static_cast<int>(ast.items.size());
    if (item_count == 0) return result;
    
    // Find max value
    double max_value = 0;
    for (const auto& item : ast.items) {
        if (item->value) max_value = std::max(max_value, *item->value);
    }
    if (max_value == 0) max_value = 100;
    
    int bar_height = 30;
    int bar_spacing = 15;
    int max_bar_width = width - 200;
    int label_width = 100;
    
    for (int i = 0; i < item_count; ++i) {
        double value = ast.items[i]->value.value_or(0);
        int bar_width = static_cast<int>(max_bar_width * value / max_value);
        
        LayoutNode node;
        node.item = ast.items[i].get();
        node.index = i;
        node.bounds.x = static_cast<float>(label_width + 20);
        node.bounds.y = static_cast<float>(result.content_start_y + i * (bar_height + bar_spacing));
        node.bounds.width = static_cast<float>(bar_width);
        node.bounds.height = static_cast<float>(bar_height);
        
        result.nodes.push_back(node);
    }
    
    apply_graph_projection(result);
    result.canvas_height = result.content_start_y + item_count * (bar_height + bar_spacing) + 50;
    ensure_canvas_contains_nodes(result);
    return result;
}

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<LayoutEngine> create_layout_engine(TemplateType type) {
    LayoutType layout = get_layout_type(type);
    switch (layout) {
        case LayoutType::Grid:      return std::make_unique<GridLayoutEngine>();
        case LayoutType::Row:       return std::make_unique<RowLayoutEngine>();
        case LayoutType::Column:    return std::make_unique<ColumnLayoutEngine>();
        case LayoutType::Zigzag:    return std::make_unique<ZigzagLayoutEngine>();
        case LayoutType::Timeline:  return std::make_unique<TimelineLayoutEngine>();
        case LayoutType::Funnel:    return std::make_unique<FunnelLayoutEngine>();
        case LayoutType::Circular:  return std::make_unique<CircularLayoutEngine>();
        case LayoutType::Tree:      return std::make_unique<TreeLayoutEngine>();
        case LayoutType::Quadrant:  return std::make_unique<QuadrantLayoutEngine>();
        case LayoutType::Pie:       return std::make_unique<PieLayoutEngine>();
        case LayoutType::Bar:       return std::make_unique<BarLayoutEngine>();
        default:                    return std::make_unique<GridLayoutEngine>();
    }
}

StyleConfig parse_style_from_template(const std::string& template_name) {
    StyleConfig config;
    
    // Parse style hints from template name
    if (template_name.find("badge") != std::string::npos) {
        config.show_badge = true;
    }
    if (template_name.find("icon") != std::string::npos) {
        config.show_icon = true;
    }
    if (template_name.find("illus") != std::string::npos) {
        config.show_illus = true;
    }
    if (template_name.find("compact") != std::string::npos) {
        config.is_compact = true;
        config.card_height = 80;
    }
    if (template_name.find("3d") != std::string::npos) {
        config.is_3d = true;
    }
    if (template_name.find("candy") != std::string::npos) {
        config.card_radius = 20;
        config.card_opacity = 0.8f;
    }
    if (template_name.find("pill") != std::string::npos) {
        config.card_radius = 25;
    }
    if (template_name.find("arrow") != std::string::npos) {
        config.connector_style = "arrow";
    }
    if (template_name.find("curve") != std::string::npos) {
        config.connector_style = "curve";
    }
    
    return config;
}

} // namespace flex::modules::infographic
