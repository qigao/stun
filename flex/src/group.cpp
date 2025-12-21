/*
 * Flex Engine - Group Implementation
 */

#include "flex/group.h"
#include "flex/renderer.h"
#include <algorithm>
#include <cmath>

namespace flex {

// ============================================================================
// Layout Calculation (Flexbox)
// ============================================================================

namespace {

// Get the size of a child along the main axis
float get_child_main_size(Node* child, FlexDirection direction) {
    Bounds b = child->bounds();
    float explicit_w = child->layout_width();
    float explicit_h = child->layout_height();

    bool is_row = (direction == FlexDirection::Row || direction == FlexDirection::RowReverse);

    if (is_row) {
        return (explicit_w > 0) ? explicit_w : b.width;
    } else {
        return (explicit_h > 0) ? explicit_h : b.height;
    }
}

// Get the size of a child along the cross axis
float get_child_cross_size(Node* child, FlexDirection direction) {
    Bounds b = child->bounds();
    float explicit_w = child->layout_width();
    float explicit_h = child->layout_height();

    bool is_row = (direction == FlexDirection::Row || direction == FlexDirection::RowReverse);

    if (is_row) {
        return (explicit_h > 0) ? explicit_h : b.height;
    } else {
        return (explicit_w > 0) ? explicit_w : b.width;
    }
}

} // anonymous namespace

void Group::perform_layout() {
    if (layout_ == LayoutMode::None || children_.empty()) {
        return;
    }

    // Determine main and cross axis based on direction
    bool is_row = (flex_direction_ == FlexDirection::Row || flex_direction_ == FlexDirection::RowReverse);
    bool is_reverse = (flex_direction_ == FlexDirection::RowReverse || flex_direction_ == FlexDirection::ColumnReverse);

    // Get container size (use clip size if clipping, otherwise calculate from content)
    float container_main = 0;
    float container_cross = 0;

    if (clip_ && clip_width_ > 0 && clip_height_ > 0) {
        container_main = is_row ? clip_width_ : clip_height_;
        container_cross = is_row ? clip_height_ : clip_width_;
    } else {
        // Calculate based on layout_width/layout_height or bounds
        float w = (layout_width_ > 0) ? layout_width_ : 0;
        float h = (layout_height_ > 0) ? layout_height_ : 0;

        // If no explicit size, we need to measure children first (content-based sizing)
        if (w == 0 || h == 0) {
            float total_main = 0;
            float max_cross = 0;
            for (const auto& child : children_) {
                if (!child->visible()) continue;
                float main_size = get_child_main_size(child.get(), flex_direction_);
                float cross_size = get_child_cross_size(child.get(), flex_direction_);
                total_main += main_size;
                max_cross = std::max(max_cross, cross_size);
            }
            total_main += gap_ * std::max(0, static_cast<int>(children_.size()) - 1);

            if (w == 0) w = is_row ? total_main : max_cross;
            if (h == 0) h = is_row ? max_cross : total_main;
        }

        container_main = is_row ? w : h;
        container_cross = is_row ? h : w;
    }

    // Subtract padding
    float padding_main_start = is_row ? padding_[3] : padding_[0];  // left : top
    float padding_main_end = is_row ? padding_[1] : padding_[2];    // right : bottom
    float padding_cross_start = is_row ? padding_[0] : padding_[3]; // top : left
    float padding_cross_end = is_row ? padding_[2] : padding_[1];   // bottom : right

    float available_main = container_main - padding_main_start - padding_main_end;
    float available_cross = container_cross - padding_cross_start - padding_cross_end;

    // Collect visible children and their sizes
    struct ChildLayout {
        Node* node;
        float base_main;    // Base main axis size
        float cross;        // Cross axis size
        float final_main;   // After flex grow/shrink
        float main_pos;     // Final position
        float cross_pos;    // Final position
    };

    std::vector<ChildLayout> items;
    float total_base_main = 0;
    float total_flex_grow = 0;
    float total_flex_shrink = 0;

    for (const auto& child : children_) {
        if (!child->visible()) continue;

        ChildLayout item;
        item.node = child.get();

        // Get base size (flex_basis or content size)
        float basis = child->flex_basis();
        if (basis > 0) {
            item.base_main = basis;
        } else {
            item.base_main = get_child_main_size(child.get(), flex_direction_);
        }

        item.cross = get_child_cross_size(child.get(), flex_direction_);
        item.final_main = item.base_main;

        total_base_main += item.base_main;
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
        used_main += item.final_main;
    }
    used_main += total_gaps;

    float free_space = available_main - used_main;
    float main_start = padding_main_start;
    float item_gap = gap_;

    switch (justify_content_) {
        case JustifyContent::Start:
            // Default: start at padding
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
        item.main_pos = current_main;

        // Calculate cross axis position based on align_items / align_self
        AlignItems align = align_items_;
        if (item.node->align_self() != AlignSelf::Auto) {
            align = static_cast<AlignItems>(static_cast<int>(item.node->align_self()) - 1);
        }

        switch (align) {
            case AlignItems::Start:
                item.cross_pos = padding_cross_start;
                break;
            case AlignItems::End:
                item.cross_pos = padding_cross_start + available_cross - item.cross;
                break;
            case AlignItems::Center:
                item.cross_pos = padding_cross_start + (available_cross - item.cross) / 2;
                break;
            case AlignItems::Stretch:
                item.cross_pos = padding_cross_start;
                item.cross = available_cross;  // Stretch to fill
                break;
        }

        current_main += item.final_main + item_gap;
    }

    // Apply positions (handle reverse)
    if (is_reverse) {
        for (auto& item : items) {
            item.main_pos = available_main - item.main_pos - item.final_main + padding_main_start;
        }
    }

    // Set final positions on nodes
    for (const auto& item : items) {
        if (is_row) {
            item.node->set_x(item.main_pos);
            item.node->set_y(item.cross_pos);
            // Update layout size if stretched
            if (item.node->layout_width() == 0) {
                item.node->set_layout_width(item.final_main);
            }
            if (align_items_ == AlignItems::Stretch && item.node->align_self() == AlignSelf::Auto) {
                item.node->set_layout_height(item.cross);
            }
        } else {
            item.node->set_x(item.cross_pos);
            item.node->set_y(item.main_pos);
            // Update layout size if stretched
            if (item.node->layout_height() == 0) {
                item.node->set_layout_height(item.final_main);
            }
            if (align_items_ == AlignItems::Stretch && item.node->align_self() == AlignSelf::Auto) {
                item.node->set_layout_width(item.cross);
            }
        }
    }
}

// ============================================================================
// Children Management
// ============================================================================

void Group::add_child(Node::Ptr child) {
    if (!child) return;

    // Remove from previous parent
    if (child->parent_) {
        auto* prev_parent = dynamic_cast<Group*>(child->parent_);
        if (prev_parent) {
            prev_parent->remove_child(child.get());
        }
    }

    child->parent_ = this;
    children_.push_back(child);
    mark_dirty(DirtyFlags::Children | DirtyFlags::Layout | DirtyFlags::Bounds);
}

void Group::remove_child(Node* child) {
    auto it = std::find_if(children_.begin(), children_.end(),
        [child](const Node::Ptr& ptr) { return ptr.get() == child; });

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

void Group::insert_child(Node::Ptr child, size_t index) {
    if (!child) return;

    // Remove from previous parent
    if (child->parent_) {
        auto* prev_parent = dynamic_cast<Group*>(child->parent_);
        if (prev_parent) {
            prev_parent->remove_child(child.get());
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
        return children_[index].get();
    }
    return nullptr;
}

Node* Group::find_child(const std::string& id) const {
    for (const auto& child : children_) {
        if (child->id() == id) {
            return child.get();
        }
    }
    return nullptr;
}

Node* Group::find_child_recursive(const std::string& id) const {
    for (const auto& child : children_) {
        if (child->id() == id) {
            return child.get();
        }
        if (child->is_group()) {
            auto* group = static_cast<Group*>(child.get());
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
            result.push_back(child.get());
        }
        if (child->is_group()) {
            auto* group = static_cast<Group*>(child.get());
            auto sub_result = group->find_by_tag(tag);
            result.insert(result.end(), sub_result.begin(), sub_result.end());
        }
    }
    return result;
}

// ============================================================================
// Bounds
// ============================================================================

Bounds Group::bounds() const {
    Bounds b;
    b.x = x_;
    b.y = y_;
    
    // If clip is enabled, use clip dimensions
    if (clip_ && clip_width_ > 0 && clip_height_ > 0) {
        b.width = clip_width_ * scale_x_;
        b.height = clip_height_ * scale_y_;
        return b;
    }
    
    // Otherwise compute from children bounds
    if (children_.empty()) {
        return b;
    }
    
    float min_x = 0, min_y = 0, max_x = 0, max_y = 0;
    bool first = true;
    
    for (const auto& child : children_) {
        Bounds cb = child->bounds();
        if (!cb.valid()) continue;
        
        if (first) {
            min_x = cb.x;
            min_y = cb.y;
            max_x = cb.x + cb.width;
            max_y = cb.y + cb.height;
            first = false;
        } else {
            min_x = std::min(min_x, cb.x);
            min_y = std::min(min_y, cb.y);
            max_x = std::max(max_x, cb.x + cb.width);
            max_y = std::max(max_y, cb.y + cb.height);
        }
    }
    
    b.width = (max_x - min_x) * scale_x_;
    b.height = (max_y - min_y) * scale_y_;
    return b;
}

// ============================================================================
// Rendering
// ============================================================================

void Group::render(Renderer& renderer) {
    if (!visible_) return;

    // Perform layout before rendering
    perform_layout();

    renderer.save();

    // Apply transform
    renderer.translate(x_, y_);
    if (rotation_ != 0) {
        renderer.rotate(rotation_);
    }
    if (scale_x_ != 1 || scale_y_ != 1) {
        renderer.scale(scale_x_, scale_y_);
    }

    // Apply opacity
    if (opacity_ < 1.0f) {
        renderer.set_global_alpha(opacity_);
    }

    // Apply clipping (if enabled with dimensions)
    if (clip_ && clip_width_ > 0 && clip_height_ > 0) {
        renderer.clip_rect(0, 0, clip_width_, clip_height_);
    }

    // Render children
    for (const auto& child : children_) {
        child->render(renderer);
    }

    renderer.restore();
}
} // namespace flex
