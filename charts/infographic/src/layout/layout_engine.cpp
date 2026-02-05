#include <layout/layout_engine.h>
#include <ir/unified_infographic.h>
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace flex::modules::infographic {

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
    
    // Simple tree: root at top, children below
    int item_count = static_cast<int>(ast.items.size());
    if (item_count == 0) return result;
    
    int level_height = 100;
    int node_width = 120;
    
    // First item is root
    LayoutNode root;
    root.item = ast.items[0].get();
    root.index = 0;
    root.bounds.x = static_cast<float>(width / 2 - node_width / 2);
    root.bounds.y = static_cast<float>(result.content_start_y);
    root.bounds.width = static_cast<float>(node_width);
    root.bounds.height = 60;
    result.nodes.push_back(root);
    
    // Remaining items as children
    int children_count = item_count - 1;
    if (children_count > 0) {
        int total_width = children_count * node_width + (children_count - 1) * 20;
        int start_x = (width - total_width) / 2;
        
        for (int i = 1; i < item_count; ++i) {
            LayoutNode node;
            node.item = ast.items[i].get();
            node.index = i;
            node.bounds.x = static_cast<float>(start_x + (i - 1) * (node_width + 20));
            node.bounds.y = static_cast<float>(result.content_start_y + level_height);
            node.bounds.width = static_cast<float>(node_width);
            node.bounds.height = 60;
            result.nodes.push_back(node);
        }
    }
    
    result.canvas_height = result.content_start_y + 2 * level_height + 50;
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
