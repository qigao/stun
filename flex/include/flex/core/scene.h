/*
 * Flex Engine - Scene
 *
 * Root container with fixed dimensions.
 * Entry point for the scene graph.
 */

#pragma once

#include "group.h"
#include "allocator.h"

namespace flex {

// ============================================================================
// Scene - Root scene container
// ============================================================================

class Scene {
public:
    using RawPtr = Scene*;
    using Ptr = RawPtr;

    Scene(float width, float height, ArenaAllocator& arena);
    ~Scene() = default;

    // Factory (Arena mode)
    static RawPtr create(float width, float height, ArenaAllocator& arena) {
        return arena.create<Scene>(width, height, arena);
    }

    // -------------------------------------------
    // Dimensions
    // -------------------------------------------

    float width() const { return width_; }
    float height() const { return height_; }
    void set_size(float width, float height) {
        width_ = width;
        height_ = height;
        if (root_) {
            root_->set_layout_size(width, height);
        }
    }

    // -------------------------------------------
    // Background
    // -------------------------------------------

    const Color& background() const { return background_; }
    void set_background(const Color& color) { background_ = color; }

    // -------------------------------------------
    // Scene Graph Root
    // -------------------------------------------

    Group::RawPtr root() const { return root_; }

    // Convenience: add child to root
    void add_child(Node::RawPtr child) { root_->add_child(child); }
    template<typename T>
    void add_child(const std::shared_ptr<T>& child) { root_->add_child(child); }

    // Find node by ID (recursive)
    Node::RawPtr find(const std::string& id) const {
        return root_->find_child_recursive(id);
    }

    // -------------------------------------------
    // Rendering
    // -------------------------------------------

    void render(Renderer& renderer);

private:
    float width_;
    float height_;
    Color background_ = {1, 1, 1, 1};  // Default white
    Group::RawPtr root_;
};

} // namespace flex
