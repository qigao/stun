#include <whiteboard/ddf/layout_algorithms.h>
#include <whiteboard/ddf/data_layer.h>
#include <algorithm>
#include <cmath>

namespace whiteboard {
namespace ddf {

GridLayout::GridLayout(const GridLayoutParams& params)
    : params_(params) {
}

LayoutResult GridLayout::compute(const DataLayer& data_layer) {
    LayoutResult result;
    
    auto all_nodes = data_layer.get_all_nodes();
    if (all_nodes.empty()) {
        return result;
    }
    
    int node_count = static_cast<int>(all_nodes.size());
    int columns = params_.columns > 0 ? params_.columns : calculate_columns(node_count);
    int rows = (node_count + columns - 1) / columns;  // Ceiling division
    
    // Position nodes in grid
    for (int i = 0; i < node_count; ++i) {
        int row = i / columns;
        int col = i % columns;
        
        float x = col * (params_.cell_width + params_.horizontal_spacing);
        float y = row * (params_.cell_height + params_.vertical_spacing);
        
        result.positions[all_nodes[i]->id] = Vec2(x, y);
    }
    
    // Calculate bounds
    if (!result.positions.empty()) {
        result.bounds_min = Vec2(0.0f, 0.0f);
        result.bounds_max = Vec2(
            (columns - 1) * (params_.cell_width + params_.horizontal_spacing) + params_.cell_width,
            (rows - 1) * (params_.cell_height + params_.vertical_spacing) + params_.cell_height
        );
    }
    
    return result;
}

int GridLayout::calculate_columns(int node_count) {
    // Calculate optimal number of columns (roughly square grid)
    return static_cast<int>(std::ceil(std::sqrt(static_cast<float>(node_count))));
}

} // namespace ddf
} // namespace whiteboard
