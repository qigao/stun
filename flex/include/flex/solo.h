/*
 * Flex Engine - Solo Node
 *
 * Container that shows only one child at a time.
 * Useful for tabs, carousels, conditional content.
 */

#pragma once

#include "flex/group.h"
#include <string>

namespace flex {

// ============================================================================
// Solo - Single-child switcher
// ============================================================================

class Solo : public Group {
public:
    using Ptr = std::shared_ptr<Solo>;

    Solo() = default;
    ~Solo() override = default;

    // Factory
    static Ptr create() { return std::make_shared<Solo>(); }

    // Type identification
    NodeType type() const override { return NodeType::Group; }  // Subtype of Group
    const char* type_name() const override { return "Solo"; }

    // -------------------------------------------
    // Active Child Control
    // -------------------------------------------

    // Get/set active child by index
    int active_index() const { return active_index_; }
    void set_active_index(int index);

    // Get/set active child by ID
    const std::string& active_id() const { return active_id_; }
    void set_active_id(const std::string& id);

    // Get currently active child
    Node* active_child() const;

    // Convenience: cycle through children
    void next();
    void previous();

    // -------------------------------------------
    // Rendering
    // -------------------------------------------

    void render(Renderer& renderer) override;

private:
    int active_index_ = 0;
    std::string active_id_;
    bool use_id_ = false;  // If true, use active_id_ instead of active_index_
};

} // namespace flex
