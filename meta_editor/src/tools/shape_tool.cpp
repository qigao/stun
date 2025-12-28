/*
 * Meta Editor - Shape Tool Implementation
 */

#include "meta_editor/tools/shape_tool.h"
#include "meta_editor/canvas.h"
#include <cmath>

namespace meta_editor {

ShapeTool::ShapeTool(ShapeType type) : shape_type_(type) {}

const char* ShapeTool::name() const {
    switch (shape_type_) {
        case ShapeType::Rectangle: return "Rectangle";
        case ShapeType::Circle: return "Circle";
        case ShapeType::Ellipse: return "Ellipse";
        case ShapeType::Polygon: return "Polygon";
        case ShapeType::Star: return "Star";
        default: return "Shape";
    }
}

const char* ShapeTool::icon() const {
    switch (shape_type_) {
        case ShapeType::Rectangle: return "square";
        case ShapeType::Circle: return "circle";
        case ShapeType::Ellipse: return "ellipse";
        case ShapeType::Polygon: return "polygon";
        case ShapeType::Star: return "star";
        default: return "shape";
    }
}

bool ShapeTool::on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    is_drawing_ = true;

    // Snap start position
    if (canvas_->is_snap_to_grid()) {
        start_pos_ = canvas_->snap_to_grid(world_pos);
    } else {
        start_pos_ = world_pos;
    }
    current_pos_ = start_pos_;

    // TODO: Check for modifier keys
    constrain_proportions_ = false;  // Shift key
    draw_from_center_ = false;       // Alt key

    return true;
}

bool ShapeTool::on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (is_drawing_) {
        // Snap current position
        if (canvas_->is_snap_to_grid()) {
            current_pos_ = canvas_->snap_to_grid(world_pos);
        } else {
            current_pos_ = world_pos;
        }
        return true;
    }
    return false;
}

bool ShapeTool::on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) {
    if (is_drawing_) {
        // Snap final position
        if (canvas_->is_snap_to_grid()) {
            current_pos_ = canvas_->snap_to_grid(world_pos);
        } else {
            current_pos_ = world_pos;
        }
        create_shape();
        is_drawing_ = false;
        return true;
    }
    return false;
}

bool ShapeTool::on_key_down(int key, int mods) {
    // TODO: Handle Shift/Alt for constrain/center
    return false;
}

void ShapeTool::render_overlay(flex::Renderer& renderer) {
    if (!is_drawing_) return;

    // Calculate bounds
    float x = std::min(start_pos_.x(), current_pos_.x());
    float y = std::min(start_pos_.y(), current_pos_.y());
    float w = std::abs(current_pos_.x() - start_pos_.x());
    float h = std::abs(current_pos_.y() - start_pos_.y());

    if (w < 1 || h < 1) return;

    // Preview style
    flex::Paint fill = flex::Paint::solid(flex::Color(0.5f, 0.7f, 0.9f, 0.3f));
    flex::Paint stroke = flex::Paint::solid(flex::Color(0.2f, 0.6f, 0.9f, 0.8f));

    switch (shape_type_) {
        case ShapeType::Rectangle:
            renderer.draw_rect(x, y, w, h, 0, fill, stroke, 2.0f);
            break;

        case ShapeType::Circle: {
            float r = std::min(w, h) / 2;
            float cx = x + w / 2;
            float cy = y + h / 2;
            renderer.draw_circle(cx, cy, r, fill, stroke, 2.0f);
            break;
        }

        case ShapeType::Ellipse: {
            float cx = x + w / 2;
            float cy = y + h / 2;
            renderer.draw_ellipse(cx, cy, w / 2, h / 2, fill, stroke, 2.0f);
            break;
        }

        case ShapeType::Polygon:
        case ShapeType::Star:
            // Draw as rectangle preview for now
            renderer.draw_rect(x, y, w, h, 0, fill, stroke, 2.0f);
            break;
    }
}

void ShapeTool::create_shape() {
    auto* layer = canvas_->content_root();
    auto* allocator = canvas_->instance()->object_allocator();

    // Calculate bounds (already snapped during drawing)
    float x = std::min(start_pos_.x(), current_pos_.x());
    float y = std::min(start_pos_.y(), current_pos_.y());
    float width = std::abs(current_pos_.x() - start_pos_.x());
    float height = std::abs(current_pos_.y() - start_pos_.y());

    if (width < 1 || height < 1) return;  // Too small

    // Constrain proportions if shift held
    if (constrain_proportions_) {
        float size = std::max(width, height);
        width = height = size;
    }
    
    // Create shape based on type
    switch (shape_type_) {
        case ShapeType::Rectangle: {
            auto* rect = flex::Shape::create(*allocator);
            rect->set_rect(width, height, 0);
            rect->set_position(x, y);
            rect->set_fill(flex::Color(0.5f, 0.7f, 0.9f, 1.0f));
            rect->set_stroke(flex::Color::Black, 2.0f);
            rect->set_rough(flex::RoughOptions::sketch());  // Hand-drawn style
            layer->add_child(rect);
            break;
        }

        case ShapeType::Circle:
        case ShapeType::Ellipse: {
            float cx = x + width / 2;
            float cy = y + height / 2;
            auto* shape = flex::Shape::create(*allocator);
            if (shape_type_ == ShapeType::Circle) {
                shape->set_circle(std::min(width, height) / 2);
            } else {
                shape->set_ellipse(width / 2, height / 2);
            }
            shape->set_position(cx, cy);
            shape->set_fill(flex::Color(0.9f, 0.5f, 0.5f, 1.0f));
            shape->set_stroke(flex::Color::Black, 2.0f);
            shape->set_rough(flex::RoughOptions::sketch());  // Hand-drawn style
            layer->add_child(shape);
            break;
        }

        case ShapeType::Polygon: {
            float cx = x + width / 2;
            float cy = y + height / 2;
            float radius = std::min(width, height) / 2;
            auto* shape = flex::Shape::create(*allocator);
            shape->set_polygon(6, radius);  // Hexagon
            shape->set_position(cx, cy);
            shape->set_fill(flex::Color(0.5f, 0.9f, 0.5f, 1.0f));
            shape->set_stroke(flex::Color::Black, 2.0f);
            shape->set_rough(flex::RoughOptions::sketch());  // Hand-drawn style
            layer->add_child(shape);
            break;
        }

        case ShapeType::Star: {
            float cx = x + width / 2;
            float cy = y + height / 2;
            float outer_radius = std::min(width, height) / 2;
            float inner_radius = outer_radius * 0.4f;
            auto* shape = flex::Shape::create(*allocator);
            shape->set_star(5, outer_radius, inner_radius);
            shape->set_position(cx, cy);
            shape->set_fill(flex::Color(0.9f, 0.9f, 0.3f, 1.0f));
            shape->set_stroke(flex::Color::Black, 2.0f);
            shape->set_rough(flex::RoughOptions::sketch());  // Hand-drawn style
            layer->add_child(shape);
            break;
        }
    }
}

} // namespace meta_editor
