/*
 * Meta Editor - Shape Tool Implementation
 */

#include "meta_editor/tools/shape_tool.h"
#include "meta_editor/canvas.h"
#include <cmath>
#include <stb_sprintf.h>
#include <algorithm>

namespace meta_editor {

ShapeTool::ShapeTool(ShapeType type) : shape_type_(type) {}

const char* ShapeTool::name() const {
    switch (shape_type_) {
        case ShapeType::Rectangle: return "Rectangle";
        case ShapeType::Circle: return "Circle";
        case ShapeType::Ellipse: return "Ellipse";
        case ShapeType::Polygon: return "Polygon";
        case ShapeType::Star: return "Star";
        case ShapeType::Triangle: return "Triangle";
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
        case ShapeType::Triangle: return "triangle";
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

    // Preview styles
    flex::Paint fill = flex::Paint::solid(flex::Color(0.5f, 0.7f, 0.9f, 0.3f));
    flex::Paint stroke = flex::Paint::solid(flex::Color(0.2f, 0.6f, 0.9f, 0.8f));
    flex::Paint guide_stroke = flex::Paint::solid(flex::Color(1.0f, 0.4f, 0.4f, 0.8f));
    flex::Paint point_fill = flex::Paint::solid(flex::Color(1.0f, 0.4f, 0.4f, 1.0f));
    flex::Paint dim_stroke = flex::Paint::solid(flex::Color(0.6f, 0.6f, 0.6f, 0.8f));

    // Helper to build SVG path with proper number formatting
    auto make_line = [](float x1, float y1, float x2, float y2) {
        char buf[128];
        stbsp_snprintf(buf, sizeof(buf), "M %.1f %.1f L %.1f %.1f", x1, y1, x2, y2);
        return std::string(buf);
    };

    switch (shape_type_) {
        case ShapeType::Rectangle: {
            // Draw shape preview
            renderer.draw_rect(x, y, w, h, 0, fill, stroke, 2.0f);

            // Draw corner points (start -> end)
            renderer.draw_circle(start_pos_.x(), start_pos_.y(), 4.0f, point_fill, stroke, 1.0f);
            renderer.draw_circle(current_pos_.x(), current_pos_.y(), 4.0f, point_fill, stroke, 1.0f);

            // Draw diagonal guide line
            renderer.stroke_path(make_line(start_pos_.x(), start_pos_.y(),
                                          current_pos_.x(), current_pos_.y()), guide_stroke, 1.0f);

            // Draw dimension lines
            float dim_offset = 8.0f;
            // Width dimension (top)
            renderer.stroke_path(make_line(x, y - dim_offset, x + w, y - dim_offset), dim_stroke, 1.0f);
            // Height dimension (right)
            renderer.stroke_path(make_line(x + w + dim_offset, y, x + w + dim_offset, y + h), dim_stroke, 1.0f);

            // Draw dimension text
            char dim_text[32];
            stbsp_snprintf(dim_text, sizeof(dim_text), "%.0f x %.0f", w, h);
            renderer.draw_text(dim_text, x + w / 2 - 20, y - dim_offset - 14,
                              "Arial", 11.0f, false, flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
            break;
        }

        case ShapeType::Circle: {
            float r = std::min(w, h) / 2;
            float cx = start_pos_.x();
            float cy = start_pos_.y();

            // Draw shape preview (from center)
            renderer.draw_circle(cx, cy, r, fill, stroke, 2.0f);

            // Draw center point
            renderer.draw_circle(cx, cy, 4.0f, point_fill, stroke, 1.0f);
            // Draw crosshairs at center
            float cross_size = 8.0f;
            std::string cross_h = "M " + std::to_string(cx - cross_size) + " " + std::to_string(cy) +
                                 " L " + std::to_string(cx + cross_size) + " " + std::to_string(cy);
            std::string cross_v = "M " + std::to_string(cx) + " " + std::to_string(cy - cross_size) +
                                 " L " + std::to_string(cx) + " " + std::to_string(cy + cross_size);
            renderer.stroke_path(cross_h, guide_stroke, 1.0f);
            renderer.stroke_path(cross_v, guide_stroke, 1.0f);

            // Draw radius line to current position
            std::string radius_line = "M " + std::to_string(cx) + " " + std::to_string(cy) +
                                     " L " + std::to_string(current_pos_.x()) + " " + std::to_string(current_pos_.y());
            renderer.stroke_path(radius_line, guide_stroke, 1.5f);

            // Draw endpoint
            renderer.draw_circle(current_pos_.x(), current_pos_.y(), 3.0f, point_fill, stroke, 1.0f);

            // Draw radius dimension text
            char dim_text[32];
            stbsp_snprintf(dim_text, sizeof(dim_text), "r=%.0f", r);
            renderer.draw_text(dim_text, cx + r / 2 - 10, cy - 15,
                              "Arial", 11.0f, false, flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
            break;
        }

        case ShapeType::Ellipse: {
            float cx = start_pos_.x();
            float cy = start_pos_.y();
            float rx = std::abs(current_pos_.x() - start_pos_.x());
            float ry = std::abs(current_pos_.y() - start_pos_.y());

            // Draw shape preview (from center)
            renderer.draw_ellipse(cx, cy, rx, ry, fill, stroke, 2.0f);

            // Draw center point with crosshairs
            renderer.draw_circle(cx, cy, 4.0f, point_fill, stroke, 1.0f);
            float cross_size = 8.0f;
            std::string cross_h = "M " + std::to_string(cx - cross_size) + " " + std::to_string(cy) +
                                 " L " + std::to_string(cx + cross_size) + " " + std::to_string(cy);
            std::string cross_v = "M " + std::to_string(cx) + " " + std::to_string(cy - cross_size) +
                                 " L " + std::to_string(cx) + " " + std::to_string(cy + cross_size);
            renderer.stroke_path(cross_h, guide_stroke, 1.0f);
            renderer.stroke_path(cross_v, guide_stroke, 1.0f);

            // Draw axes lines
            std::string h_axis = "M " + std::to_string(cx) + " " + std::to_string(cy) +
                                " L " + std::to_string(cx + rx) + " " + std::to_string(cy);
            std::string v_axis = "M " + std::to_string(cx) + " " + std::to_string(cy) +
                                " L " + std::to_string(cx) + " " + std::to_string(cy + ry);
            renderer.stroke_path(h_axis, guide_stroke, 1.5f);
            renderer.stroke_path(v_axis, guide_stroke, 1.5f);

            // Draw dimension text
            char dim_text[32];
            stbsp_snprintf(dim_text, sizeof(dim_text), "%.0f x %.0f", rx * 2, ry * 2);
            renderer.draw_text(dim_text, cx - 20, cy - ry - 15,
                              "Arial", 11.0f, false, flex::Color(0.3f, 0.3f, 0.3f, 1.0f));
            break;
        }

        case ShapeType::Polygon:
        case ShapeType::Star: {
            float cx = start_pos_.x();
            float cy = start_pos_.y();
            float radius = std::sqrt(w * w + h * h) / 2;

            // Draw preview
            if (shape_type_ == ShapeType::Polygon) {
                // Simple pentagon preview
                renderer.draw_circle(cx, cy, radius, fill, stroke, 2.0f);
            } else {
                renderer.draw_circle(cx, cy, radius, fill, stroke, 2.0f);
            }

            // Draw center and radius
            renderer.draw_circle(cx, cy, 4.0f, point_fill, stroke, 1.0f);
            std::string radius_line = "M " + std::to_string(cx) + " " + std::to_string(cy) +
                                     " L " + std::to_string(current_pos_.x()) + " " + std::to_string(current_pos_.y());
            renderer.stroke_path(radius_line, guide_stroke, 1.5f);
            renderer.draw_circle(current_pos_.x(), current_pos_.y(), 3.0f, point_fill, stroke, 1.0f);
            break;
        }
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
            // Group acts as Bounding Box
            auto* group = layer->add<flex::Group>();
            group->set_id("rectangle");
            group->set_position(x, y);
            group->set_layout_size(width, height);
            
            // Shape is relative content
            auto* rect = group->add<flex::Shape>();
            rect->set_rect(width, height, 0);
            rect->set_position(0, 0); // Relative to group
            rect->set_fill(flex::Color(0.5f, 0.7f, 0.9f, 1.0f));
            rect->set_stroke(flex::Color::Black, 2.0f);
            rect->set_rough(flex::RoughOptions::sketch());
            break;
        }

        case ShapeType::Circle: {
            float cx = start_pos_.x();
            float cy = start_pos_.y();
            float r = std::sqrt(width * width + height * height) / 2;
            if (r < 5) r = 5;

            // Calculate BBox for Group
            float gx = cx - r;
            float gy = cy - r;
            float gw = r * 2;
            float gh = r * 2;

            auto* group = layer->add<flex::Group>();
            group->set_id("circle");
            group->set_position(gx, gy);
            group->set_layout_size(gw, gh);

            auto* shape = group->add<flex::Shape>();
            shape->set_circle(r);
            shape->set_position(r, r); // Center relative to group
            shape->set_fill(flex::Color(0.9f, 0.5f, 0.5f, 1.0f));
            shape->set_stroke(flex::Color::Black, 2.0f);
            shape->set_rough(flex::RoughOptions::sketch());
            break;
        }

        case ShapeType::Ellipse: {
            float cx = start_pos_.x();
            float cy = start_pos_.y();
            float rx = std::abs(current_pos_.x() - start_pos_.x());
            float ry = std::abs(current_pos_.y() - start_pos_.y());
            if (rx < 5) rx = 5;
            if (ry < 5) ry = 5;

            // BBox
            float gx = cx - rx;
            float gy = cy - ry;
            float gw = rx * 2;
            float gh = ry * 2;

            auto* group = layer->add<flex::Group>();
            group->set_id("ellipse");
            group->set_position(gx, gy);
            group->set_layout_size(gw, gh);

            auto* shape = group->add<flex::Shape>();
            shape->set_ellipse(rx, ry);
            shape->set_position(rx, ry); // Center relative to group
            shape->set_fill(flex::Color(0.9f, 0.5f, 0.5f, 1.0f));
            shape->set_stroke(flex::Color::Black, 2.0f);
            shape->set_rough(flex::RoughOptions::sketch());
            break;
        }

        case ShapeType::Polygon: {
            float cx = start_pos_.x();
            float cy = start_pos_.y();
            float radius = std::sqrt(width * width + height * height) / 2;
            if (radius < 5) radius = 5;

            float gx = cx - radius;
            float gy = cy - radius;
            float gw = radius * 2;

            auto* group = layer->add<flex::Group>();
            group->set_id("polygon");
            group->set_position(gx, gy);
            group->set_layout_size(gw, gw);

            auto* shape = group->add<flex::Shape>();
            shape->set_polygon(5, radius);
            shape->set_position(radius, radius);
            shape->set_fill(flex::Color(0.5f, 0.9f, 0.5f, 1.0f));
            shape->set_stroke(flex::Color::Black, 2.0f);
            shape->set_rough(flex::RoughOptions::sketch());
            break;
        }

        case ShapeType::Star: {
            float cx = start_pos_.x();
            float cy = start_pos_.y();
            float outer_radius = std::sqrt(width * width + height * height) / 2;
            if (outer_radius < 5) outer_radius = 5;
            float inner_radius = outer_radius * 0.4f;

            float gx = cx - outer_radius;
            float gy = cy - outer_radius;
            float gw = outer_radius * 2;

            auto* group = layer->add<flex::Group>();
            group->set_id("star");
            group->set_position(gx, gy);
            group->set_layout_size(gw, gw);

            auto* shape = group->add<flex::Shape>();
            shape->set_star(5, outer_radius, inner_radius);
            shape->set_position(outer_radius, outer_radius);
            shape->set_fill(flex::Color(0.9f, 0.9f, 0.3f, 1.0f));
            shape->set_stroke(flex::Color::Black, 2.0f);
            shape->set_rough(flex::RoughOptions::sketch());
            break;
        }

        case ShapeType::Triangle: {
            float cx = start_pos_.x() + width / 2;
            float cy = start_pos_.y() + height / 2;
            
            auto* group = layer->add<flex::Group>();
            group->set_id("triangle");
            group->set_position(start_pos_.x(), start_pos_.y());
            group->set_layout_size(width, height);

            auto* shape = group->add<flex::Shape>();
            shape->set_triangle(width, height, flex::Direction::Up);
            shape->set_position(width/2, height/2);
            shape->set_fill(flex::Color(0.5f, 0.7f, 0.9f, 1.0f));
            shape->set_stroke(flex::Color::Black, 2.0f);
            shape->set_rough(flex::RoughOptions::sketch());
            break;
        }
    }
}

} // namespace meta_editor
