/*
 * Snap Helper Implementation
 */

#include <editor/tool/snap_helper.h>
#include <algorithm>
#include <cmath>
#include <cfloat>

namespace editor {

void SnapHelper::collectTargets(const std::vector<EditorNode::Ptr>& allNodes,
                                const std::vector<EditorNode::Ptr>& selected) {
    clearTargets();

    // Build set of selected node pointers for fast lookup
    std::vector<EditorNode*> selectedPtrs;
    selectedPtrs.reserve(selected.size());
    for (const auto& node : selected) {
        selectedPtrs.push_back(node.get());
    }

    auto isSelected = [&selectedPtrs](EditorNode* node) {
        return std::find(selectedPtrs.begin(), selectedPtrs.end(), node) != selectedPtrs.end();
    };

    // Collect snap points from non-selected nodes
    for (const auto& node : allNodes) {
        if (isSelected(node.get())) continue;
        if (!node->visible()) continue;

        Rect bounds = node->bounds();

        // Add edges
        float left = bounds.x;
        float right = bounds.x + bounds.width;
        float top = bounds.y;
        float bottom = bounds.y + bounds.height;

        // Add center
        float centerX = bounds.x + bounds.width / 2;
        float centerY = bounds.y + bounds.height / 2;

        targetX_.push_back(left);
        targetX_.push_back(right);
        targetX_.push_back(centerX);

        targetY_.push_back(top);
        targetY_.push_back(bottom);
        targetY_.push_back(centerY);
    }

    // Sort and remove duplicates
    std::sort(targetX_.begin(), targetX_.end());
    targetX_.erase(std::unique(targetX_.begin(), targetX_.end()), targetX_.end());

    std::sort(targetY_.begin(), targetY_.end());
    targetY_.erase(std::unique(targetY_.begin(), targetY_.end()), targetY_.end());
}

void SnapHelper::addCanvasBounds(float width, float height) {
    // Canvas edges
    targetX_.push_back(0);
    targetX_.push_back(width);
    targetX_.push_back(width / 2);  // Center

    targetY_.push_back(0);
    targetY_.push_back(height);
    targetY_.push_back(height / 2);  // Center

    // Re-sort after adding
    std::sort(targetX_.begin(), targetX_.end());
    targetX_.erase(std::unique(targetX_.begin(), targetX_.end()), targetX_.end());

    std::sort(targetY_.begin(), targetY_.end());
    targetY_.erase(std::unique(targetY_.begin(), targetY_.end()), targetY_.end());
}

void SnapHelper::clearTargets() {
    targetX_.clear();
    targetY_.clear();
}

SnapHelper::SnapResult SnapHelper::findClosestSnap(const std::vector<float>& sources,
                                                    const std::vector<float>& targets) {
    SnapResult result;
    result.offset = FLT_MAX;
    result.snapPosition = 0;

    for (float src : sources) {
        for (float tgt : targets) {
            float dist = tgt - src;
            if (std::abs(dist) < std::abs(result.offset)) {
                result.offset = dist;
                result.snapPosition = tgt;
            }
        }
    }

    return result;
}

SnapGuides SnapHelper::calculateSnap(const Rect& selectionBounds,
                                     float proposedDx, float proposedDy,
                                     float* outDx, float* outDy) {
    SnapGuides guides;
    *outDx = proposedDx;
    *outDy = proposedDy;

    if (!enabled_ || (targetX_.empty() && targetY_.empty())) {
        return guides;
    }

    // Calculate proposed new bounds
    Rect newBounds = {
        selectionBounds.x + proposedDx,
        selectionBounds.y + proposedDy,
        selectionBounds.width,
        selectionBounds.height
    };

    // Source points for X (vertical snap lines)
    std::vector<float> srcX = {
        newBounds.x,                            // Left edge
        newBounds.x + newBounds.width / 2,      // Center
        newBounds.x + newBounds.width           // Right edge
    };

    // Source points for Y (horizontal snap lines)
    std::vector<float> srcY = {
        newBounds.y,                            // Top edge
        newBounds.y + newBounds.height / 2,     // Center
        newBounds.y + newBounds.height          // Bottom edge
    };

    // Find best X snap
    SnapResult snapX = findClosestSnap(srcX, targetX_);
    if (std::abs(snapX.offset) <= threshold_) {
        *outDx = proposedDx + snapX.offset;
        guides.vertical.push_back(snapX.snapPosition);
    }

    // Find best Y snap
    SnapResult snapY = findClosestSnap(srcY, targetY_);
    if (std::abs(snapY.offset) <= threshold_) {
        *outDy = proposedDy + snapY.offset;
        guides.horizontal.push_back(snapY.snapPosition);
    }

    return guides;
}

} // namespace editor
