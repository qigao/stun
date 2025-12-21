/*
 * Snap Helper
 *
 * Provides smart snapping functionality for aligning objects during
 * move and resize operations.
 */

#pragma once

#include "../core/types.h"
#include "../model/node.h"
#include <vector>

namespace editor {

// Snap guide lines to display during drag
struct SnapGuides {
    std::vector<float> vertical;    // X positions of vertical lines
    std::vector<float> horizontal;  // Y positions of horizontal lines

    void clear() {
        vertical.clear();
        horizontal.clear();
    }

    bool empty() const {
        return vertical.empty() && horizontal.empty();
    }
};

// Snap calculation helper
class SnapHelper {
public:
    SnapHelper() = default;

    // Configuration
    void setThreshold(float worldSpaceThreshold) { threshold_ = worldSpaceThreshold; }
    void setEnabled(bool enabled) { enabled_ = enabled; }
    bool enabled() const { return enabled_; }

    // Collect snap targets from all non-selected nodes
    // Call this when starting a drag operation
    void collectTargets(const std::vector<EditorNode::Ptr>& allNodes,
                       const std::vector<EditorNode::Ptr>& selected);

    // Add canvas bounds as snap targets
    void addCanvasBounds(float width, float height);

    // Calculate snap for a proposed move
    // Returns snap guides and modifies outDx/outDy with snapped deltas
    SnapGuides calculateSnap(const Rect& selectionBounds,
                            float proposedDx, float proposedDy,
                            float* outDx, float* outDy);

    // Clear all targets
    void clearTargets();

private:
    // Find the closest snap and return the offset needed
    // Returns FLT_MAX if no snap within threshold
    struct SnapResult {
        float offset;       // Offset to apply to achieve snap
        float snapPosition; // The position we're snapping to
    };

    SnapResult findClosestSnap(const std::vector<float>& sources,
                               const std::vector<float>& targets);

    std::vector<float> targetX_;  // X positions to snap to (vertical lines)
    std::vector<float> targetY_;  // Y positions to snap to (horizontal lines)
    float threshold_ = 8.0f;      // Snap threshold in world space
    bool enabled_ = true;
};

} // namespace editor
