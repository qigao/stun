/*
 * Flex Engine - Group Implementation
 */

#include "flex/runtime/group.h"
#include "flex/runtime/renderer.h"
#include <algorithm>
#include <cmath>
#include <iostream>
namespace flex {

// ============================================================================
// Layout Calculation (Flexbox)
// ============================================================================

namespace {

// Resolve a dimension that might be a percentage
float resolve_dimension(float value, bool is_percent, float container_size) {
    if (is_percent && value > 0) {
        return container_size * (value / 100.0f);
    }
    return value;
}

// Get the size of a child along the main axis (including margin)
float get_child_main_size(Node* child, FlexDirection direction, float container_main, float container_cross) {
    bool is_row = (direction == FlexDirection::Row || direction == FlexDirection::RowReverse);

    // Resolve percentage dimensions
    float w = resolve_dimension(child->layout_width(), child->width_is_percent(), is_row ? container_main : container_cross);
    float h = resolve_dimension(child->layout_height(), child->height_is_percent(), is_row ? container_cross : container_main);

    // If still 0, use bounds
    if (w <= 0) w = child->bounds().width;
    if (h <= 0) h = child->bounds().height;

    // Add margin
    if (is_row) {
        return w + child->margin_left() + child->margin_right();
    } else {
        return h + child->margin_top() + child->margin_bottom();
    }
}

// Get the size of a child along the cross axis (including margin)
float get_child_cross_size(Node* child, FlexDirection direction, float container_main, float container_cross) {
    bool is_row = (direction == FlexDirection::Row || direction == FlexDirection::RowReverse);

    // Resolve percentage dimensions
    float w = resolve_dimension(child->layout_width(), child->width_is_percent(), is_row ? container_main : container_cross);
    float h = resolve_dimension(child->layout_height(), child->height_is_percent(), is_row ? container_cross : container_main);

    // If still 0, use bounds
    if (w <= 0) w = child->bounds().width;
    if (h <= 0) h = child->bounds().height;

    // Add margin
    if (is_row) {
        return h + child->margin_top() + child->margin_bottom();
    } else {
        return w + child->margin_left() + child->margin_right();
    }
}

// Get the content size (excluding margin) for a child
float get_child_content_main(Node* child, FlexDirection direction, float container_main, float container_cross) {
    bool is_row = (direction == FlexDirection::Row || direction == FlexDirection::RowReverse);
    float w = resolve_dimension(child->layout_width(), child->width_is_percent(), is_row ? container_main : container_cross);
    float h = resolve_dimension(child->layout_height(), child->height_is_percent(), is_row ? container_cross : container_main);
    if (w <= 0) w = child->bounds().width;
    if (h <= 0) h = child->bounds().height;
    return is_row ? w : h;
}

float get_child_content_cross(Node* child, FlexDirection direction, float container_main, float container_cross) {
    bool is_row = (direction == FlexDirection::Row || direction == FlexDirection::RowReverse);
    float w = resolve_dimension(child->layout_width(), child->width_is_percent(), is_row ? container_main : container_cross);
    float h = resolve_dimension(child->layout_height(), child->height_is_percent(), is_row ? container_cross : container_main);
    if (w <= 0) w = child->bounds().width;
    if (h <= 0) h = child->bounds().height;
    return is_row ? h : w;
}

} // anonymous namespace

void Group::perform_layout() {
    if (children_.empty()) {
        clear_dirty(DirtyFlags::Layout);
        return;
    }

    // Determine main and cross axis based on direction
    bool is_row = (flex_direction_ == FlexDirection::Row || flex_direction_ == FlexDirection::RowReverse);
    bool is_reverse = (flex_direction_ == FlexDirection::RowReverse || flex_direction_ == FlexDirection::ColumnReverse);

    // Get container size
    float container_w = layout_width();
    float container_h = layout_height();
    float container_main = is_row ? container_w : container_h;
    float container_cross = is_row ? container_h : container_w;

    // Subtract padding for content area
    float padding_main_start = is_row ? padding_[3] : padding_[0];  // left : top
    float padding_main_end = is_row ? padding_[1] : padding_[2];    // right : bottom
    float padding_cross_start = is_row ? padding_[0] : padding_[3]; // top : left
    float padding_cross_end = is_row ? padding_[2] : padding_[1];   // bottom : right

    float available_main = container_main - padding_main_start - padding_main_end;
    float available_cross = container_cross - padding_cross_start - padding_cross_end;

    // ========================================================================
    // Phase 1: Layout absolute/fixed positioned children
    // ========================================================================
    for (auto* child : children_) {
        if (!child->visible()) continue;

        PositionMode pm = child->position_mode();
        if (pm != PositionMode::Absolute && pm != PositionMode::Fixed) continue;

        // Resolve child size (percentage based on container)
        float child_w = resolve_dimension(child->layout_width(), child->width_is_percent(), container_w);
        float child_h = resolve_dimension(child->layout_height(), child->height_is_percent(), container_h);

        // Containing block (fixed uses root/viewport, but we don't have that info here - treat as container)
        float cb_w = container_w;
        float cb_h = container_h;

        float top = child->position_top();
        float right = child->position_right();
        float bottom = child->position_bottom();
        float left = child->position_left();

        // If both left and right specified but no width, stretch
        if (!std::isnan(left) && !std::isnan(right) && child_w <= 0) {
            child_w = cb_w - left - right;
        }
        // If both top and bottom specified but no height, stretch
        if (!std::isnan(top) && !std::isnan(bottom) && child_h <= 0) {
            child_h = cb_h - top - bottom;
        }

        // Calculate position
        float x = 0, y = 0;
        if (!std::isnan(left)) {
            x = left;
        } else if (!std::isnan(right)) {
            x = cb_w - right - child_w;
        }

        if (!std::isnan(top)) {
            y = top;
        } else if (!std::isnan(bottom)) {
            y = cb_h - bottom - child_h;
        }

        // Set position and size
        child->set_position(x, y);
        if (child_w > 0) child->set_layout_width(child_w);
        if (child_h > 0) child->set_layout_height(child_h);
    }

    // ========================================================================
    // Phase 2: Flex layout for normal flow children
    // ========================================================================
    if (layout_ == LayoutMode::None) {
        clear_dirty(DirtyFlags::Layout);
        return;
    }

    // Collect flex items (exclude absolute/fixed)
    struct ChildLayout {
        Node* node;
        float base_main;    // Base main axis size (content only)
        float cross;        // Cross axis size (content only)
        float margin_main_start;
        float margin_main_end;
        float margin_cross_start;
        float margin_cross_end;
        float final_main;   // After flex grow/shrink
        float main_pos;     // Final position
        float cross_pos;    // Final position
    };

    std::vector<ChildLayout> items;
    float total_base_main = 0;
    float total_flex_grow = 0;
    float total_flex_shrink = 0;

    for (auto* child : children_) {
        if (!child->visible()) continue;
        if (child->position_absolute()) continue;  // Skip positioned elements

        ChildLayout item;
        item.node = child;

        // Get margins
        if (is_row) {
            item.margin_main_start = child->margin_left();
            item.margin_main_end = child->margin_right();
            item.margin_cross_start = child->margin_top();
            item.margin_cross_end = child->margin_bottom();
        } else {
            item.margin_main_start = child->margin_top();
            item.margin_main_end = child->margin_bottom();
            item.margin_cross_start = child->margin_left();
            item.margin_cross_end = child->margin_right();
        }

        // Get base size (flex_basis or content size)
        float basis = child->flex_basis();
        if (basis > 0) {
            item.base_main = basis;
        } else {
            item.base_main = get_child_content_main(child, flex_direction_, available_main, available_cross);
        }

        item.cross = get_child_content_cross(child, flex_direction_, available_main, available_cross);
        item.final_main = item.base_main;

        total_base_main += item.base_main + item.margin_main_start + item.margin_main_end;
        total_flex_grow += child->flex_grow();
        total_flex_shrink += child->flex_shrink();

        items.push_back(item);
    }

    // Add gaps
    float total_gaps = gap_ * std::max(0, static_cast<int>(items.size()) - 1);
    total_base_main += total_gaps;

    // Calculate flex grow/shrink
    float remaining = available_main - total_base_main;

    if (remaining > 0 && total_flex_grow > 0) {
        // Distribute extra space
        for (auto& item : items) {
            float grow = item.node->flex_grow();
            if (grow > 0) {
                item.final_main += (remaining * grow / total_flex_grow);
            }
        }
    } else if (remaining < 0 && total_flex_shrink > 0) {
        // Shrink items
        float shrink_amount = -remaining;
        for (auto& item : items) {
            float shrink = item.node->flex_shrink();
            if (shrink > 0) {
                item.final_main -= (shrink_amount * shrink / total_flex_shrink);
                item.final_main = std::max(0.0f, item.final_main);
            }
        }
    }

    // Calculate main axis positions based on justify_content
    float used_main = 0;
    for (const auto& item : items) {
        used_main += item.final_main + item.margin_main_start + item.margin_main_end;
    }
    used_main += total_gaps;

    float free_space = available_main - used_main;
    float main_start = padding_main_start;
    float item_gap = gap_;

    switch (justify_content_) {
        case JustifyContent::Start:
            break;
        case JustifyContent::End:
            main_start += free_space;
            break;
        case JustifyContent::Center:
            main_start += free_space / 2;
            break;
        case JustifyContent::SpaceBetween:
            if (items.size() > 1) {
                item_gap = (available_main - used_main + total_gaps) / (items.size() - 1);
            }
            break;
        case JustifyContent::SpaceAround:
            if (!items.empty()) {
                float space = free_space / items.size();
                main_start += space / 2;
                item_gap = gap_ + space;
            }
            break;
        case JustifyContent::SpaceEvenly:
            if (!items.empty()) {
                float space = free_space / (items.size() + 1);
                main_start += space;
                item_gap = gap_ + space;
            }
            break;
    }

    // Position items
    float current_main = main_start;
    for (auto& item : items) {
        // Add margin before
        current_main += item.margin_main_start;
        item.main_pos = current_main;

        // Calculate cross axis position based on align_items / align_self
        AlignItems align = align_items_;
        if (item.node->align_self() != AlignSelf::Auto) {
            align = static_cast<AlignItems>(static_cast<int>(item.node->align_self()) - 1);
        }

        float item_cross = item.cross;
        switch (align) {
            case AlignItems::Start:
                item.cross_pos = padding_cross_start + item.margin_cross_start;
                break;
            case AlignItems::End:
                item.cross_pos = padding_cross_start + available_cross - item_cross - item.margin_cross_end;
                break;
            case AlignItems::Center:
                item.cross_pos = padding_cross_start + (available_cross - item_cross) / 2;
                break;
            case AlignItems::Stretch:
                item.cross_pos = padding_cross_start + item.margin_cross_start;
                item_cross = available_cross - item.margin_cross_start - item.margin_cross_end;
                break;
        }

        current_main += item.final_main + item.margin_main_end + item_gap;

        // Store computed cross size for stretch
        item.cross = item_cross;
    }

    // Handle reverse
    if (is_reverse) {
        for (auto& item : items) {
            item.main_pos = available_main - item.main_pos - item.final_main + padding_main_start;
        }
    }

    // Apply relative offset and set final positions
    for (const auto& item : items) {
        float x, y;
        if (is_row) {
            x = item.main_pos;
            y = item.cross_pos;
        } else {
            x = item.cross_pos;
            y = item.main_pos;
        }

        // Apply relative positioning offset
        if (item.node->position_mode() == PositionMode::Relative) {
            if (!std::isnan(item.node->position_left())) {
                x += item.node->position_left();
            } else if (!std::isnan(item.node->position_right())) {
                x -= item.node->position_right();
            }
            if (!std::isnan(item.node->position_top())) {
                y += item.node->position_top();
            } else if (!std::isnan(item.node->position_bottom())) {
                y -= item.node->position_bottom();
            }
        }

        // Set position and layout size
        item.node->set_position(x, y);

        // Update layout size
        if (is_row) {
            item.node->set_layout_width(item.final_main);
            if (align_items_ == AlignItems::Stretch && item.node->align_self() == AlignSelf::Auto) {
                item.node->set_layout_height(item.cross);
            }
        } else {
            item.node->set_layout_height(item.final_main);
            if (align_items_ == AlignItems::Stretch && item.node->align_self() == AlignSelf::Auto) {
                item.node->set_layout_width(item.cross);
            }
        }
    }

    // Clear layout dirty flag
    clear_dirty(DirtyFlags::Layout);
}

// ============================================================================
// Children Management
// ============================================================================

void Group::add_child(Node* child) {
    if (!child) return;

    // Remove from previous parent
    if (child->parent_) {
        auto* prev_parent = dynamic_cast<Group*>(child->parent_);
        if (prev_parent) {
            prev_parent->remove_child(child);
        }
    }

    child->parent_ = this;
    children_.push_back(child);

    mark_dirty(DirtyFlags::Children | DirtyFlags::Layout | DirtyFlags::Bounds);
}

void Group::remove_child(Node* child) {
    auto it = std::find(children_.begin(), children_.end(), child);

    if (it != children_.end()) {
        (*it)->parent_ = nullptr;
        children_.erase(it);
        mark_dirty(DirtyFlags::Children | DirtyFlags::Layout | DirtyFlags::Bounds);
    }
}

void Group::remove_child_at(size_t index) {
    if (index < children_.size()) {
        children_[index]->parent_ = nullptr;
        children_.erase(children_.begin() + index);
        mark_dirty(DirtyFlags::Children | DirtyFlags::Layout | DirtyFlags::Bounds);
    }
}

void Group::insert_child(Node* child, size_t index) {
    if (!child) return;

    // Remove from previous parent
    if (child->parent_) {
        auto* prev_parent = dynamic_cast<Group*>(child->parent_);
        if (prev_parent) {
            prev_parent->remove_child(child);
        }
    }

    child->parent_ = this;
    if (index >= children_.size()) {
        children_.push_back(child);
    } else {
        children_.insert(children_.begin() + index, child);
    }
    mark_dirty(DirtyFlags::Children | DirtyFlags::Layout | DirtyFlags::Bounds);
}

void Group::clear_children() {
    if (children_.empty()) return;

    for (auto& child : children_) {
        child->parent_ = nullptr;
    }
    children_.clear();
    mark_dirty(DirtyFlags::Children | DirtyFlags::Layout | DirtyFlags::Bounds);
}

Node* Group::child_at(size_t index) const {
    if (index < children_.size()) {
        return children_[index];
    }
    return nullptr;
}

Node* Group::find_child(const std::string& id) const {
    for (const auto& child : children_) {
        if (child->id() == id) {
            return child;
        }
    }
    return nullptr;
}

Node* Group::find_child_recursive(const std::string& id) const {
    // Support nested path syntax: "parent/child/grandchild"
    auto slash_pos = id.find('/');
    if (slash_pos != std::string::npos) {
        // Split path into first component and remaining path
        std::string first = id.substr(0, slash_pos);
        std::string rest = id.substr(slash_pos + 1);

        // Find first component in direct children
        auto* parent = find_child(first);
        if (!parent) return nullptr;

        // Recursively find remaining path
        return parent->find(rest);
    }

    // No '/' -> original recursive search
    for (const auto& child : children_) {
        if (child->id() == id) {
            return child;
        }
        if (child->is_group()) {
            auto* group = static_cast<Group*>(child);
            auto* found = group->find_child_recursive(id);
            if (found) return found;
        }
    }
    return nullptr;
}

std::vector<Node*> Group::find_by_tag(const std::string& tag) const {
    std::vector<Node*> result;
    for (const auto& child : children_) {
        if (child->has_tag(tag)) {
            result.push_back(child);
        }
        if (child->is_group()) {
            auto* group = static_cast<Group*>(child);
            auto sub_result = group->find_by_tag(tag);
            result.insert(result.end(), sub_result.begin(), sub_result.end());
        }
    }
    return result;
}

void Group::mark_dirty(DirtyFlags flags) {
    // Basic marking for this node
    Node::mark_dirty(flags);

    // If transform changed, all children's world transforms are now invalid
    if (has_flag(flags, DirtyFlags::Transform)) {
        for (auto& child : children_) {
            child->mark_dirty(DirtyFlags::Transform);
        }
    }
}

// ============================================================================
// Bounds
// ============================================================================

Bounds Group::compute_bounds() const {
    Bounds b;
    b.x = 0;
    b.y = 0;

    // If clip is enabled, use clip dimensions
    if (clip_) {
        b.width = layout_width();
        b.height = layout_height();
        return b;
    }

    // Otherwise, union of all children's bounds (which are in their parent's space, i.e., our local space)
    if (children_.empty()) {
        b.width = layout_width();
        b.height = layout_height();
        return b;
    }

    float min_x = 0, min_y = 0;
    float max_x = 0, max_y = 0;
    bool first = true;

    for (const auto& child : children_) {
        if (!child->visible()) continue;
        
        // child->bounds() is now in child's local space.
        // We need the child's bounds in OUR local space.
        // For a simple group without rotation/scale, this is:
        // child_local_bounds + child_position.
        // But with Eigen, we could do full matrix transform of the bounds.
        // For now, let's use the child's local_transform to transform its local bounds 4 corners.
        
        Bounds cb = child->bounds();
        Transform ct = child->local_transform();
        
        // Transform the 4 corners of child's local bounds to our space
        Vec2 p1 = ct * Vec2(cb.x, cb.y);
        Vec2 p2 = ct * Vec2(cb.x + cb.width, cb.y);
        Vec2 p3 = ct * Vec2(cb.x, cb.y + cb.height);
        Vec2 p4 = ct * Vec2(cb.x + cb.width, cb.y + cb.height);
        
        float c_min_x = std::min({p1.x(), p2.x(), p3.x(), p4.x()});
        float c_min_y = std::min({p1.y(), p2.y(), p3.y(), p4.y()});
        float c_max_x = std::max({p1.x(), p2.x(), p3.x(), p4.x()});
        float c_max_y = std::max({p1.y(), p2.y(), p3.y(), p4.y()});


        if (first) {
            min_x = c_min_x; min_y = c_min_y;
            max_x = c_max_x; max_y = c_max_y;
            first = false;
        } else {
            min_x = std::min(min_x, c_min_x);
            min_y = std::min(min_y, c_min_y);
            max_x = std::max(max_x, c_max_x);
            max_y = std::max(max_y, c_max_y);
        }
    }

    b.x = min_x;
    b.y = min_y;
    b.width = max_x - min_x;
    b.height = max_y - min_y;

    // Ensure at least layout size
    b.width = std::max(b.width, layout_width());
    b.height = std::max(b.height, layout_height());

    return b;
}

// ============================================================================
// Rendering
// ============================================================================

void Group::render(Renderer& renderer) {
    if (!visible_) return;

    if (is_dirty(DirtyFlags::Layout)) {
        perform_layout();
    }

    renderer.save();
    
    renderer.translate(x_, y_);
    if (rotation_ != 0.0f) renderer.rotate(rotation_);
    if (scale_x_ != 1.0f || scale_y_ != 1.0f) renderer.scale(scale_x_, scale_y_);
    
    if (opacity_ < 1.0f) renderer.set_global_alpha(opacity_);
    if (clip_ && layout_width() > 0 && layout_height() > 0) {
        renderer.clip_rect(0, 0, layout_width(), layout_height());
    }

    // Render children with full frustum culling
    Bounds vp = renderer.viewport();
    for (const auto& child : children_) {
        if (child->cull(vp) == CullResult::Visible) {
            child->render(renderer);
        }
    }

    renderer.restore();
}
} // namespace flex
