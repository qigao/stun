/*
 * Flex Engine - Artboard
 *
 * Root container with fixed dimensions.
 * Entry point for the scene graph.
 */

#pragma once

#include "group.h"
#include <memory>

namespace flex {

// ============================================================================
// Artboard - Root scene container
// ============================================================================

class Artboard {
public:
    using Ptr = std::shared_ptr<Artboard>;

    Artboard(float width, float height);
    ~Artboard() = default;

    // Factory
    static Ptr create(float width, float height) {
        return std::make_shared<Artboard>(width, height);
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

    Group* root() const { return root_.get(); }

    // Convenience: add child to root
    void add_child(Node::Ptr child) { root_->add_child(child); }

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
    Group::Ptr root_;
};

} // namespace flex
