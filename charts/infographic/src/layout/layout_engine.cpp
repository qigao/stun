#include <layout/layout_engine.h>
#include <ir/unified_infographic.h>
#include <libcola/cola.h>
#include <libvpsc/rectangle.h>
#include <cmath>
#include <algorithm>
#include <memory>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flex::modules::infographic {

namespace {

void apply_cola_relax(LayoutResult& result, double ideal_length) {
    const size_t count = result.nodes.size();
    if (count < 2) return;

    std::vector<std::unique_ptr<vpsc::Rectangle>> storage;
    storage.reserve(count);
    vpsc::Rectangles rects;
    rects.reserve(count);

    cola::DesiredPositions desired;
    desired.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        const auto& node = result.nodes[i];
        double x = node.bounds.x;
        double y = node.bounds.y;
        double w = std::max(1.0f, node.bounds.width);
        double h = std::max(1.0f, node.bounds.height);

        storage.push_back(std::make_unique<vpsc::Rectangle>(x, x + w, y, y + h));
        rects.push_back(storage.back().get());
        desired.push_back(cola::DesiredPosition{
            static_cast<unsigned>(i),
            x + w * 0.5,
            y + h * 0.5,
            1000.0
        });
    }

    std::vector<cola::Edge> edges;
    cola::ConstrainedFDLayout layout(rects, edges, ideal_length);
    layout.setAvoidNodeOverlaps(true);
    layout.setDesiredPositions(&desired);
    layout.run();

    double min_x = rects[0]->getMinX();
    double min_y = rects[0]->getMinY();
    for (size_t i = 1; i < count; ++i) {
        min_x = std::min(min_x, rects[i]->getMinX());
        min_y = std::min(min_y, rects[i]->getMinY());
    }

    const double offset_x = min_x < 0 ? -min_x : 0;
    const double offset_y = min_y < 0 ? -min_y : 0;

    for (size_t i = 0; i < count; ++i) {
        auto& node = result.nodes[i];
        node.bounds.x = static_cast<float>(rects[i]->getMinX() + offset_x);
        node.bounds.y = static_cast<float>(rects[i]->getMinY() + offset_y);
        node.bounds.width = static_cast<float>(rects[i]->width());
        node.bounds.height = static_cast<float>(rects[i]->height());
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
    
    apply_cola_relax(result, style.card_width);
    result.canvas_height = result.content_start_y + rows * (style.card_height + style.item_spacing) + 50;
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
    
    apply_cola_relax(result, row_height);
    result.canvas_height = result.content_start_y + item_count * (row_height + style.item_spacing) + 50;
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
    
    apply_cola_relax(result, item_height);
    result.canvas_height = result.content_start_y + item_count * (item_height + 5) + 50;
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
    
    apply_cola_relax(result, card_width);
    result.canvas_height = result.content_start_y + item_count * step_height + 50;
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
    
    apply_cola_relax(result, step_width);
    result.canvas_height = result.content_start_y + 150;
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
    
    apply_cola_relax(result, funnel_width_top);
    result.canvas_height = result.content_start_y + item_count * step_height + 50;
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
    
    apply_cola_relax(result, radius);
    result.canvas_height = center_y + radius + 100;
    return result;
}

// ============================================================================
// Tree Layout
// ============================================================================

LayoutResult TreeLayoutEngine::compute(const UnifiedInfographic& ast, int width, int height,
                                       const StyleConfig& style) {
    LayoutResult result;
    result.canvas_width = width;
    result.content_start_y = 100;
    
    struct TreeNodeInfo {
        const DataItem* item;
        int parent;
        int depth;
    };

    std::vector<TreeNodeInfo> nodes;
    nodes.reserve(64);

    auto add_node = [&](const DataItem* item, int parent, int depth, auto&& self) -> void {
        int index = static_cast<int>(nodes.size());
        nodes.push_back({item, parent, depth});
        for (const auto& child : item->children) {
            self(child.get(), index, depth + 1, self);
        }
    };

    if (!ast.items.empty()) {
        if (ast.items.size() == 1) {
            add_node(ast.items[0].get(), -1, 0, add_node);
        } else {
            for (const auto& root_item : ast.items) {
                add_node(root_item.get(), -1, 0, add_node);
            }
        }
    }

    if (nodes.empty()) return result;

    const int node_width = style.card_width;
    const int node_height = style.card_height > 0 ? style.card_height : 60;
    const int h_gap = std::max(10, style.item_spacing);
    const int level_height = std::max(node_height + style.item_spacing, 80);

    int max_depth = 0;
    for (const auto& node : nodes) {
        max_depth = std::max(max_depth, node.depth);
    }

    std::vector<std::vector<int>> levels(max_depth + 1);
    for (size_t i = 0; i < nodes.size(); ++i) {
        levels[nodes[i].depth].push_back(static_cast<int>(i));
    }

    result.nodes.reserve(nodes.size());
    result.parent_index.reserve(nodes.size());

    for (int depth = 0; depth <= max_depth; ++depth) {
        const auto& level_nodes = levels[depth];
        if (level_nodes.empty()) continue;
        int total_width = static_cast<int>(level_nodes.size()) * node_width +
                          static_cast<int>(level_nodes.size() - 1) * h_gap;
        int start_x = (width - total_width) / 2;
        int y = result.content_start_y + depth * level_height;

        for (size_t i = 0; i < level_nodes.size(); ++i) {
            int idx = level_nodes[i];
            LayoutNode node;
            node.item = nodes[idx].item;
            node.index = idx;
            node.bounds.x = static_cast<float>(start_x + static_cast<int>(i) * (node_width + h_gap));
            node.bounds.y = static_cast<float>(y);
            node.bounds.width = static_cast<float>(node_width);
            node.bounds.height = static_cast<float>(node_height);
            result.nodes.push_back(node);
            result.parent_index.push_back(nodes[idx].parent);
        }
    }

    apply_cola_relax(result, node_width);
    for (size_t i = 0; i < nodes.size() && i < result.nodes.size(); ++i) {
        int depth = nodes[i].depth;
        result.nodes[i].bounds.y = static_cast<float>(result.content_start_y + depth * level_height);
        result.nodes[i].bounds.height = static_cast<float>(node_height);
    }

    // Auto-fit to canvas with margins (center + scale)
    float min_x = result.nodes[0].bounds.x;
    float min_y = result.nodes[0].bounds.y;
    float max_x = min_x + result.nodes[0].bounds.width;
    float max_y = min_y + result.nodes[0].bounds.height;
    for (const auto& node : result.nodes) {
        min_x = std::min(min_x, node.bounds.x);
        min_y = std::min(min_y, node.bounds.y);
        max_x = std::max(max_x, node.bounds.x + node.bounds.width);
        max_y = std::max(max_y, node.bounds.y + node.bounds.height);
    }

    const float margin = 40.0f;
    const float content_w = std::max(1.0f, max_x - min_x);
    const float content_h = std::max(1.0f, max_y - min_y);
    const float avail_w = std::max(1.0f, width - 2.0f * margin);
    const float avail_h = std::max(1.0f, height - 2.0f * margin);
    const float scale = std::min(1.0f, std::min(avail_w / content_w, avail_h / content_h));
    const float offset_x = margin + (avail_w - content_w * scale) / 2.0f;
    const float offset_y = margin + (avail_h - content_h * scale) / 2.0f;

    for (auto& node : result.nodes) {
        node.bounds.x = (node.bounds.x - min_x) * scale + offset_x;
        node.bounds.y = (node.bounds.y - min_y) * scale + offset_y;
        node.bounds.width *= scale;
        node.bounds.height *= scale;
    }

    result.canvas_height = height;
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
    
    apply_cola_relax(result, quadrant_width);
    result.canvas_height = result.content_start_y + 2 * quadrant_height + 70;
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
    
    apply_cola_relax(result, bar_height);
    result.canvas_height = result.content_start_y + item_count * (bar_height + bar_spacing) + 50;
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
