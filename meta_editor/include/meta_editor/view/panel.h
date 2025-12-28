/*
 * Meta Editor - Panel Base Class
 *
 * Common functionality for floating panels:
 * - Position/size management
 * - Background/border rendering
 * - Hit testing
 * - Drag to move
 * - Consistent styling
 */

#pragma once

#include <flex.h>

namespace meta_editor {

// Panel style configuration
struct PanelStyle {
    flex::Color background = {0.17f, 0.17f, 0.19f, 0.95f};
    flex::Color border = {0.3f, 0.3f, 0.32f, 1.0f};
    float border_width = 1.0f;
    float corner_radius = 8.0f;
};

class Panel {
public:
    virtual ~Panel() = default;

    // Render the panel
    virtual void render(flex::Renderer& renderer) = 0;

    // Position
    void set_position(float x, float y) { x_ = x; y_ = y; }
    float x() const { return x_; }
    float y() const { return y_; }

    // Size
    void set_size(float w, float h) { width_ = w; height_ = h; }
    float width() const { return width_; }
    float height() const { return height_; }

    // Visibility
    bool is_visible() const { return visible_; }
    void set_visible(bool v) { visible_ = v; }

    // Style
    void set_style(const PanelStyle& style) { style_ = style; }
    const PanelStyle& style() const { return style_; }

    // Draggable
    void set_draggable(bool d) { draggable_ = d; }
    bool is_draggable() const { return draggable_; }

    // Hit testing - check if point is inside panel bounds
    bool contains(float px, float py) const {
        return visible_ &&
               px >= x_ && px <= x_ + width_ &&
               py >= y_ && py <= y_ + content_height();
    }

    // Drag handling - returns true if event was consumed
    bool handle_drag_start(float px, float py) {
        if (!draggable_ || !contains(px, py)) return false;
        dragging_ = true;
        drag_offset_x_ = px - x_;
        drag_offset_y_ = py - y_;
        return true;
    }

    bool handle_drag_move(float px, float py) {
        if (!dragging_) return false;
        x_ = px - drag_offset_x_;
        y_ = py - drag_offset_y_;
        return true;
    }

    void handle_drag_end() {
        dragging_ = false;
    }

    bool is_dragging() const { return dragging_; }

protected:
    // Subclasses can override to return dynamic height
    virtual float content_height() const { return height_; }

    // Render panel background with current style
    void render_background(flex::Renderer& renderer) const {
        render_background(renderer, content_height());
    }

    void render_background(flex::Renderer& renderer, float height) const {
        flex::Paint bg = flex::Paint::solid(style_.background);
        flex::Paint border = flex::Paint::solid(style_.border);
        renderer.draw_rect(x_, y_, width_, height, style_.corner_radius,
                          bg, border, style_.border_width);
    }

    float x_ = 0;
    float y_ = 0;
    float width_ = 100;
    float height_ = 100;
    bool visible_ = true;
    bool draggable_ = true;  // Draggable by default
    PanelStyle style_;

private:
    bool dragging_ = false;
    float drag_offset_x_ = 0;
    float drag_offset_y_ = 0;
};

} // namespace meta_editor
