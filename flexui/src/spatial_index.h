#pragma once

#include <vector>
#include <memory>
#include <algorithm>

namespace flexui {

// Forward declaration
class Widget;

/**
 * @brief Simple spatial index for efficient widget lookup
 * 
 * Uses a uniform grid to partition space for O(1) average-case lookup
 * instead of O(n) linear search for mouse events.
 */
class SpatialIndex {
public:
    SpatialIndex(float width, float height, int grid_size = 10)
        : width_(width), height_(height), grid_size_(grid_size) {
        cell_width_ = width / grid_size;
        cell_height_ = height / grid_size;
        cells_.resize(grid_size * grid_size);
    }

    void clear() {
        for (auto& cell : cells_) {
            cell.clear();
        }
    }

    void insert(Widget* widget, float x, float y, float w, float h) {
        if (!widget) return;

        // Calculate grid cell bounds for this widget
        int min_x = std::max(0, (int)(x / cell_width_));
        int max_x = std::min(grid_size_ - 1, (int)((x + w) / cell_width_));
        int min_y = std::max(0, (int)(y / cell_height_));
        int max_y = std::min(grid_size_ - 1, (int)((y + h) / cell_height_));

        // Insert widget into all cells it overlaps
        for (int cy = min_y; cy <= max_y; ++cy) {
            for (int cx = min_x; cx <= max_x; ++cx) {
                int cell_idx = cy * grid_size_ + cx;
                cells_[cell_idx].push_back(widget);
            }
        }
    }

    std::vector<Widget*> query(float mx, float my) const {
        int cx = std::clamp((int)(mx / cell_width_), 0, grid_size_ - 1);
        int cy = std::clamp((int)(my / cell_height_), 0, grid_size_ - 1);
        int cell_idx = cy * grid_size_ + cx;
        
        return cells_[cell_idx];
    }

    void resize(float width, float height) {
        width_ = width;
        height_ = height;
        cell_width_ = width / grid_size_;
        cell_height_ = height / grid_size_;
        clear();
    }

private:
    float width_;
    float height_;
    int grid_size_;
    float cell_width_;
    float cell_height_;
    std::vector<std::vector<Widget*>> cells_;
};

} // namespace flexui
