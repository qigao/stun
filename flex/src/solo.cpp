/*
 * Flex Engine - Solo Node Implementation
 */

#include "flex/solo.h"
#include "flex/renderer.h"

namespace flex {

// ============================================================================
// Active Child Control
// ============================================================================

void Solo::set_active_index(int index) {
    active_index_ = index;
    use_id_ = false;
    active_id_.clear();
}

void Solo::set_active_id(const std::string& id) {
    active_id_ = id;
    use_id_ = true;
}

Node* Solo::active_child() const {
    if (children().empty()) return nullptr;

    if (use_id_) {
        // Find by ID
        for (const auto& child : children()) {
            if (child->id() == active_id_) {
                return child.get();
            }
        }
        return nullptr;
    } else {
        // Find by index
        int index = active_index_;
        if (index < 0) index = 0;
        if (index >= static_cast<int>(children().size())) {
            index = static_cast<int>(children().size()) - 1;
        }
        return children()[index].get();
    }
}

void Solo::next() {
    if (children().empty()) return;
    active_index_ = (active_index_ + 1) % static_cast<int>(children().size());
    use_id_ = false;
    active_id_.clear();
}

void Solo::previous() {
    if (children().empty()) return;
    active_index_ = active_index_ - 1;
    if (active_index_ < 0) {
        active_index_ = static_cast<int>(children().size()) - 1;
    }
    use_id_ = false;
    active_id_.clear();
}

// ============================================================================
// Rendering
// ============================================================================

void Solo::render(Renderer& renderer) {
    if (!visible() || opacity() <= 0) return;

    // Only render the active child
    Node* active = active_child();
    if (active && active->visible()) {
        renderer.save();
        renderer.translate(x(), y());
        renderer.scale(scale_x(), scale_y());
        renderer.rotate(rotation());
        renderer.set_global_alpha(opacity());

        active->render(renderer);

        renderer.restore();
    }
}
} // namespace flex
