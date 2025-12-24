/*
 * Flex Engine - Group Node
 *
 * Container node that can have children.
 * Applies its transform to all children.
 */

#pragma once

#include "flex/node.h"
#include <vector>
#include <memory>

namespace flex {

// ============================================================================
// Group - Container node with children
// ============================================================================

class Group : public Node {
public:
    using Ptr = std::shared_ptr<Group>;

    Group() = default;
    ~Group() override = default;

    // Factory
    static Ptr create() { return std::make_shared<Group>(); }

    // Type identification
    NodeType type() const override { return NodeType::Group; }
    const char* type_name() const override { return "Group"; }
    bool is_group() const override { return true; }

    // -------------------------------------------
    // Children Management
    // -------------------------------------------

    const std::vector<Node::Ptr>& children() const { return children_; }
    size_t child_count() const { return children_.size(); }

    void add_child(Node::Ptr child);
    void remove_child(Node* child);
    void remove_child_at(size_t index);
    void insert_child(Node::Ptr child, size_t index);
    void clear_children();

    Node* child_at(size_t index) const;
    Node* find_child(const std::string& id) const;
    Node* find_child_recursive(const std::string& id) const;

    // Override Node::find() to enable hierarchical search
    Node* find(const std::string& id) override { return find_child_recursive(id); }

    // Query by tag
    std::vector<Node*> find_by_tag(const std::string& tag) const;

    // -------------------------------------------
    // Clipping
    // -------------------------------------------

    bool clip() const { return clip_; }
    void set_clip(bool c) { clip_ = c; mark_dirty(DirtyFlags::Visual); }

    float clip_width() const { return clip_width_; }
    float clip_height() const { return clip_height_; }
    void set_clip_size(float w, float h) { clip_width_ = w; clip_height_ = h; mark_dirty(DirtyFlags::Bounds); }

    // -------------------------------------------
    // Layout (Flexbox)
    // -------------------------------------------

    LayoutMode layout() const { return layout_; }
    void set_layout(LayoutMode mode) { layout_ = mode; mark_dirty(DirtyFlags::Layout); }

    FlexDirection flex_direction() const { return flex_direction_; }
    void set_flex_direction(FlexDirection d) { flex_direction_ = d; mark_dirty(DirtyFlags::Layout); }

    JustifyContent justify_content() const { return justify_content_; }
    void set_justify_content(JustifyContent j) { justify_content_ = j; mark_dirty(DirtyFlags::Layout); }

    AlignItems align_items() const { return align_items_; }
    void set_align_items(AlignItems a) { align_items_ = a; mark_dirty(DirtyFlags::Layout); }

    FlexWrap flex_wrap() const { return flex_wrap_; }
    void set_flex_wrap(FlexWrap w) { flex_wrap_ = w; mark_dirty(DirtyFlags::Layout); }

    float gap() const { return gap_; }
    void set_gap(float g) { gap_ = g; mark_dirty(DirtyFlags::Layout); }

    float padding_top() const { return padding_[0]; }
    float padding_right() const { return padding_[1]; }
    float padding_bottom() const { return padding_[2]; }
    float padding_left() const { return padding_[3]; }
    void set_padding(float all) {
        padding_[0] = padding_[1] = padding_[2] = padding_[3] = all;
        mark_dirty(DirtyFlags::Layout);
    }
    void set_padding(float top, float right, float bottom, float left) {
        padding_[0] = top; padding_[1] = right; padding_[2] = bottom; padding_[3] = left;
        mark_dirty(DirtyFlags::Layout);
    }

    // Perform layout calculation (updates child positions)
    void perform_layout();

    // -------------------------------------------
    // Bounds
    // -------------------------------------------

    Bounds bounds() const override;

    // -------------------------------------------
    // Rendering
    // -------------------------------------------

    void render(Renderer& renderer) override;

private:
    std::vector<Node::Ptr> children_;

    // Clipping
    bool clip_ = false;
    float clip_width_ = 0;
    float clip_height_ = 0;

    // Layout
    LayoutMode layout_ = LayoutMode::None;
    FlexDirection flex_direction_ = FlexDirection::Row;
    JustifyContent justify_content_ = JustifyContent::Start;
    AlignItems align_items_ = AlignItems::Start;
    FlexWrap flex_wrap_ = FlexWrap::NoWrap;
    float gap_ = 0;
    float padding_[4] = {0, 0, 0, 0};  // top, right, bottom, left
};

} // namespace flex
