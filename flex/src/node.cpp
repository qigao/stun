/*
 * Flex Engine - Node Implementation
 */

#include "flex/node.h"
#include "flex/dsl/fsm.h"
#include <algorithm>

namespace flex {

// ============================================================================
// Constructor / Destructor
// ============================================================================

Node::Node() = default;

Node::~Node() {
    // Clean up event dispatcher if allocated
    delete events_;
}

void Node::ensure_events() {
    if (!events_) {
        events_ = new EventDispatcher();
    }
}

// ============================================================================
// Dirty Flags and Culling
// ============================================================================

void Node::mark_dirty(DirtyFlags flags) {
    // If transform or local bounds change, world bounds also need recalculation
    if (has_flag(flags, DirtyFlags::Transform | DirtyFlags::Bounds)) {
        flags |= DirtyFlags::WorldBounds;
    }

    dirty_flags_ |= flags;
    
    // If transform changed, we must also invalidate world transforms of all children
    // This is handled by recursion in Group::mark_dirty
    propagate_dirty();
}

void Node::propagate_dirty() {
    // Propagate layout dirty to parent
    if (parent_ && has_flag(dirty_flags_, DirtyFlags::Layout)) {
        parent_->mark_dirty(DirtyFlags::Layout | DirtyFlags::Bounds);
    }
}

void Node::update_local_transform() {
    float tx = x_;
    float ty = y_;

    // Apply anchor offset if not TopLeft (default)
    if (anchor_ != Anchor::TopLeft) {
        Bounds b = compute_bounds();
        float w = b.width;
        float h = b.height;

        switch (anchor_) {
            case Anchor::TopLeft:
                break;
            case Anchor::Top:
                tx -= w / 2;
                break;
            case Anchor::TopRight:
                tx -= w;
                break;
            case Anchor::Left:
                ty -= h / 2;
                break;
            case Anchor::Center:
                tx -= w / 2;
                ty -= h / 2;
                break;
            case Anchor::Right:
                tx -= w;
                ty -= h / 2;
                break;
            case Anchor::BottomLeft:
                ty -= h;
                break;
            case Anchor::Bottom:
                tx -= w / 2;
                ty -= h;
                break;
            case Anchor::BottomRight:
                tx -= w;
                ty -= h;
                break;
        }
    }

    local_transform_ = create_transform(tx, ty, rotation_, scale_x_, scale_y_);
    // No need to clear flag here, it's cleared by callers (world_transform() or similar)
}

void Node::update_world_transform() {
    if (is_dirty(DirtyFlags::Transform)) {
        update_local_transform();
        if (parent_) {
            world_transform_ = parent_->world_transform() * local_transform_;
        } else {
            world_transform_ = local_transform_;
        }
        clear_dirty(DirtyFlags::Transform);
    }
}



bool Node::intersects_viewport(const Bounds& viewport) const {
    Bounds b = world_bounds();
    // AABB intersection test
    return !(b.x + b.width < viewport.x ||
             b.x > viewport.x + viewport.width ||
             b.y + b.height < viewport.y ||
             b.y > viewport.y + viewport.height);
}


// ============================================================================
// Hit Testing and Events
// ============================================================================

bool Node::hit_test(float px, float py) const {
    if (!visible_) return false;
    return bounds().contains(px, py);
}

void Node::fire_pointer_down(PointerEvent& event) {
    if (events_ && events_->on_pointer_down) events_->on_pointer_down(event);
}

void Node::fire_pointer_up(PointerEvent& event) {
    if (events_ && events_->on_pointer_up) events_->on_pointer_up(event);
}

void Node::fire_pointer_move(PointerEvent& event) {
    if (events_ && events_->on_pointer_move) events_->on_pointer_move(event);
}

void Node::fire_hover_enter(PointerEvent& event) {
    if (events_ && events_->on_hover_enter) events_->on_hover_enter(event);
}

void Node::fire_hover_leave(PointerEvent& event) {
    if (events_ && events_->on_hover_leave) events_->on_hover_leave(event);
}

void Node::fire_click() {
    if (events_ && events_->on_click) events_->on_click();
}

bool Node::has_pointer_handlers() const {
    return events_ && events_->has_pointer_handlers();
}

// ============================================================================
// Keyboard Events
// ============================================================================

void Node::fire_key_down(KeyEvent& event) {
    if (events_ && events_->on_key_down) events_->on_key_down(event);
}

void Node::fire_key_up(KeyEvent& event) {
    if (events_ && events_->on_key_up) events_->on_key_up(event);
}

void Node::fire_focus(bool gained) {
    if (events_ && events_->on_focus) events_->on_focus(gained);
}

bool Node::has_key_handlers() const {
    return events_ && events_->has_key_handlers();
}

void Node::set_focused(bool f) {
    if (focused_ != f) {
        focused_ = f;
        fire_focus(f);
    }
}

// ============================================================================
// FSM and Pseudo-Class Styles Support
// ============================================================================

void Node::add_pseudo_class_style(const std::string& name, const PseudoClassStyle& style) {
    if (!pseudo_styles_) {
        pseudo_styles_ = std::make_unique<PseudoClassStyleMap>();
    }
    (*pseudo_styles_)[name] = style;
}

Node* Node::find_by_path(const std::string& path) {
    // Parse path: "child.grandchild.property"
    size_t dot_pos = path.find('.');

    if (dot_pos == std::string::npos) {
        // No dot, just find child by ID
        return find(path);
    }

    // Split: "child" . "grandchild.property"
    std::string child_id = path.substr(0, dot_pos);
    std::string remaining = path.substr(dot_pos + 1);

    // Find child
    Node* child = find(child_id);
    if (!child) return nullptr;

    // Recurse
    return child->find_by_path(remaining);
}

} // namespace flex
