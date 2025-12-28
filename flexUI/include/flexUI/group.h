/*
 * flexUI - Group and Drawable Base Classes
 *
 * Provides hierarchical scene graph for widget composition.
 * All coordinates are relative to parent.
 */

#ifndef FLEXUI_GROUP_H
#define FLEXUI_GROUP_H

#include "flex/runtime/types.h"
#include "flex/runtime/renderer.h"
#include <vector>
#include <memory>

namespace flexUI {

// Re-export flex types
using flex::Transform;
using flex::Bounds;
using flex::Color;
using flex::Paint;

/**
 * Drawable - Base class for anything that can be drawn
 *
 * All drawables have:
 * - Local position (relative to parent)
 * - Ability to draw themselves given parent transform and alpha
 * - Bounding box for hit testing
 */
class Drawable {
public:
    virtual ~Drawable() = default;

    /**
     * Draw this drawable
     *
     * @param r         The renderer to draw to
     * @param parent    Parent's world transform (already accumulated)
     * @param alpha     Parent's accumulated alpha (0.0-1.0)
     */
    virtual void draw(flex::Renderer& r, const Transform& parent, float alpha) = 0;

    /**
     * Get local bounds (in local coordinates)
     */
    virtual Bounds local_bounds() const = 0;

    /**
     * Get world bounds (transformed by parent)
     */
    Bounds world_bounds(const Transform& parent) const {
        return local_bounds().transformed(parent);
    }

    // Position (relative to parent)
    float x() const { return x_; }
    float y() const { return y_; }
    void set_position(float x, float y) { x_ = x; y_ = y; }

    // Visibility
    bool visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }

protected:
    float x_ = 0;
    float y_ = 0;
    bool visible_ = true;
};

/**
 * Group - Container for multiple drawables
 *
 * Features:
 * - Holds child drawables
 * - Has local transform (position, rotation, scale)
 * - Has opacity that multiplies with children
 * - Draws children in order (first added = back, last added = front)
 */
class Group : public Drawable {
public:
    Group() = default;
    ~Group() override = default;

    // No copy, allow move
    Group(const Group&) = delete;
    Group& operator=(const Group&) = delete;
    Group(Group&&) = default;
    Group& operator=(Group&&) = default;

    /**
     * Add a child drawable
     */
    template<typename T, typename... Args>
    T* add(Args&&... args) {
        auto child = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = child.get();
        children_.push_back(std::move(child));
        return ptr;
    }

    /**
     * Add an existing drawable
     */
    void add(std::unique_ptr<Drawable> child) {
        if (child) children_.push_back(std::move(child));
    }

    /**
     * Remove a child by pointer
     */
    bool remove(Drawable* child) {
        for (auto it = children_.begin(); it != children_.end(); ++it) {
            if (it->get() == child) {
                children_.erase(it);
                return true;
            }
        }
        return false;
    }

    /**
     * Clear all children
     */
    void clear() { children_.clear(); }

    /**
     * Get number of children
     */
    size_t child_count() const { return children_.size(); }

    /**
     * Get child by index
     */
    Drawable* child(size_t index) {
        return index < children_.size() ? children_[index].get() : nullptr;
    }

    // Transform
    void set_transform(const Transform& t) { transform_ = t; }
    const Transform& transform() const { return transform_; }

    void set_rotation(float degrees) {
        transform_ = flex::create_transform(x_, y_, degrees, scale_x_, scale_y_);
    }

    void set_scale(float sx, float sy) {
        scale_x_ = sx;
        scale_y_ = sy;
        transform_ = flex::create_transform(x_, y_, rotation_, sx, sy);
    }

    // Opacity
    float opacity() const { return opacity_; }
    void set_opacity(float a) { opacity_ = a; }

    // Drawable interface
    void draw(flex::Renderer& r, const Transform& parent, float alpha) override {
        if (!visible_ || opacity_ <= 0) return;

        // Build world transform: parent * local_translation * transform
        Transform local = flex::make_translation(x_, y_);
        Transform world = parent * local * transform_;
        float world_alpha = alpha * opacity_;

        // Draw all children
        for (auto& child : children_) {
            if (child) child->draw(r, world, world_alpha);
        }
    }

    Bounds local_bounds() const override {
        if (children_.empty()) return Bounds{x_, y_, 0, 0};

        // Compute union of all children bounds
        float min_x = 1e10f, min_y = 1e10f;
        float max_x = -1e10f, max_y = -1e10f;

        Transform local = flex::make_translation(x_, y_) * transform_;

        for (const auto& child : children_) {
            if (!child) continue;
            Bounds cb = child->local_bounds().transformed(local);
            min_x = std::min(min_x, cb.x);
            min_y = std::min(min_y, cb.y);
            max_x = std::max(max_x, cb.x + cb.width);
            max_y = std::max(max_y, cb.y + cb.height);
        }

        if (min_x > max_x) return Bounds{x_, y_, 0, 0};
        return Bounds{min_x, min_y, max_x - min_x, max_y - min_y};
    }

protected:
    std::vector<std::unique_ptr<Drawable>> children_;
    Transform transform_ = Transform::Identity();
    float opacity_ = 1.0f;
    float rotation_ = 0;
    float scale_x_ = 1.0f;
    float scale_y_ = 1.0f;
};

} // namespace flexUI

#endif // FLEXUI_GROUP_H
