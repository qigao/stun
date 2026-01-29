/*
 * Meta Editor - Snap Helper
 *
 * Smart guides - snap to object edges, centers, and grid.
 */

#pragma once

#include <flex/runtime/types.h>
#include <flex/runtime.h>
#include <vector>

namespace meta_editor {

class Canvas;
class SelectionManager;

struct SnapResult {
    flex::Vec2 snapped_pos;
    bool snapped_x = false;
    bool snapped_y = false;
    float guide_x = 0;  // X position of vertical guide line
    float guide_y = 0;  // Y position of horizontal guide line
};

struct SnapGuide {
    enum class Type { Vertical, Horizontal };
    Type type;
    float position;      // World coordinate
    float start, end;    // Extent of guide line
};

class SnapHelper {
public:
    SnapHelper(Canvas* canvas, SelectionManager* selection);

    // Set snap threshold in screen pixels
    void set_threshold(float pixels) { threshold_ = pixels; }

    // Enable/disable different snap types
    void set_snap_to_objects(bool enable) { snap_to_objects_ = enable; }
    void set_snap_to_grid(bool enable) { snap_to_grid_ = enable; }

    // Snap a point during drag
    // excluded_nodes: nodes being dragged (don't snap to self)
    SnapResult snap_point(const flex::Vec2& world_pos,
                         const std::vector<flex::Node*>& excluded_nodes);

    // Snap bounds (for dragging selection)
    SnapResult snap_bounds(const flex::Bounds& bounds,
                          const std::vector<flex::Node*>& excluded_nodes);

    // Get current snap guides for rendering
    const std::vector<SnapGuide>& guides() const { return guides_; }

    // Clear guides after drag ends
    void clear_guides() { guides_.clear(); }

private:
    void collect_snap_targets(const std::vector<flex::Node*>& excluded);

    struct SnapTarget {
        float left, center_x, right;
        float top, center_y, bottom;
    };

    Canvas* canvas_;
    SelectionManager* selection_;

    float threshold_ = 8.0f;  // Snap threshold in screen pixels
    bool snap_to_objects_ = true;
    bool snap_to_grid_ = true;

    std::vector<SnapTarget> targets_;
    std::vector<SnapGuide> guides_;
};

} // namespace meta_editor
