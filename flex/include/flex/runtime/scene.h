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
    using Ptr = Scene*;

    Scene(float width, float height, ArenaAllocator& arena);
    ~Scene() = default;

    // Factory (Arena mode)
    static Ptr create(float width, float height, ArenaAllocator& arena) {
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
    }

    // -------------------------------------------
    // Background
    // -------------------------------------------

    const Color& background() const { return background_; }
    void set_background(const Color& color) { background_ = color; }

    // -------------------------------------------
    // Scene Graph Root
    // -------------------------------------------

    Group* root() const { return root_; }

    // Convenience: add child to root
    void add_child(Node* child) { root_->add_child(child); }

    // Find node by ID (recursive)
    Node* find(const std::string& id) const {
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
    Group* root_;
};

} // namespace flex
