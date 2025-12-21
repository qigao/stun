/*
 * Flex Engine - Node Implementation
 */

#include "flex/node.h"
#include <algorithm>

namespace flex {

bool Node::has_tag(const std::string& tag) const {
    return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
}

// ============================================================================
// Dirty Flags and Culling
// ============================================================================

void Node::propagate_dirty() {
    // Propagate layout dirty to parent
    if (parent_ && has_flag(dirty_flags_, DirtyFlags::Layout)) {
        parent_->mark_dirty(DirtyFlags::Layout | DirtyFlags::Bounds);
    }
}

CullResult Node::cull(const Bounds& viewport) const {
    if (!visible_) return CullResult::Hidden;
    if (opacity_ <= 0.0f) return CullResult::Transparent;
    if (!intersects_viewport(viewport)) return CullResult::OutOfView;
    return CullResult::Visible;
}

bool Node::intersects_viewport(const Bounds& viewport) const {
    Bounds b = bounds();
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
    if (on_pointer_down_) on_pointer_down_(event);
}

void Node::fire_pointer_up(PointerEvent& event) {
    if (on_pointer_up_) on_pointer_up_(event);
}

void Node::fire_pointer_move(PointerEvent& event) {
    if (on_pointer_move_) on_pointer_move_(event);
}

void Node::fire_hover_enter(PointerEvent& event) {
    if (on_hover_enter_) on_hover_enter_(event);
}

void Node::fire_hover_leave(PointerEvent& event) {
    if (on_hover_leave_) on_hover_leave_(event);
}

void Node::fire_click() {
    if (on_click_) on_click_();
}

bool Node::has_pointer_handlers() const {
    return on_pointer_down_ || on_pointer_up_ || on_pointer_move_ ||
           on_hover_enter_ || on_hover_leave_ || on_click_;
}

// ============================================================================
// Keyboard Events
// ============================================================================

void Node::fire_key_down(KeyEvent& event) {
    if (on_key_down_) on_key_down_(event);
}

void Node::fire_key_up(KeyEvent& event) {
    if (on_key_up_) on_key_up_(event);
}

void Node::fire_focus(bool gained) {
    if (on_focus_) on_focus_(gained);
}

bool Node::has_key_handlers() const {
    return on_key_down_ || on_key_up_;
}

void Node::set_focused(bool f) {
    if (focused_ != f) {
        focused_ = f;
        fire_focus(f);
    }
}

} // namespace flex
