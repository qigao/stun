/*
 * Meta Editor - Shape Collection Panel Implementation
 */

#include "meta_editor/view/shape_collection_panel.h"
#include "meta_editor/canvas.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace meta_editor {

ShapeCollectionPanel::ShapeCollectionPanel(Canvas* canvas) : canvas_(canvas) {
    // Set default position and size
    x_ = 16;
    y_ = 80;
    width_ = 180;

    build_shape_library();
}

float ShapeCollectionPanel::content_height() const {
    float total = header_height_ * 4;
    for (const auto& [cat, exp] : expanded_) {
        if (exp) total += calc_section_height(cat);
    }
    return total;
}

void ShapeCollectionPanel::build_shape_library() {
    shapes_.push_back({"Rectangle", "Basic", ShapeTemplate::Type::Rectangle, {0.5f, 0.7f, 0.9f, 1.0f}});
    shapes_.push_back({"Rounded", "Basic", ShapeTemplate::Type::RoundedRect, {0.6f, 0.8f, 0.6f, 1.0f}});
    shapes_.push_back({"Circle", "Basic", ShapeTemplate::Type::Circle, {0.9f, 0.5f, 0.5f, 1.0f}});
    shapes_.push_back({"Ellipse", "Basic", ShapeTemplate::Type::Ellipse, {0.9f, 0.7f, 0.5f, 1.0f}});

    shapes_.push_back({"Triangle", "Polygons", ShapeTemplate::Type::Triangle, {0.7f, 0.5f, 0.9f, 1.0f}});
    shapes_.push_back({"Diamond", "Polygons", ShapeTemplate::Type::Diamond, {0.5f, 0.9f, 0.9f, 1.0f}});
    shapes_.push_back({"Pentagon", "Polygons", ShapeTemplate::Type::Pentagon, {0.9f, 0.9f, 0.5f, 1.0f}});
    shapes_.push_back({"Hexagon", "Polygons", ShapeTemplate::Type::Hexagon, {0.5f, 0.9f, 0.5f, 1.0f}});

    shapes_.push_back({"Star 5", "Stars", ShapeTemplate::Type::Star5, {0.9f, 0.8f, 0.3f, 1.0f}});
    shapes_.push_back({"Star 6", "Stars", ShapeTemplate::Type::Star6, {0.8f, 0.6f, 0.9f, 1.0f}});

    shapes_.push_back({"Arrow", "Special", ShapeTemplate::Type::Arrow, {0.6f, 0.6f, 0.6f, 1.0f}});
    shapes_.push_back({"Ring", "Special", ShapeTemplate::Type::Ring, {0.4f, 0.7f, 0.9f, 1.0f}});

    for (size_t i = 0; i < shapes_.size(); ++i) {
        categories_[shapes_[i].category].push_back(i);
    }

    expanded_["Basic"] = true;
    expanded_["Polygons"] = false;
    expanded_["Stars"] = false;
    expanded_["Special"] = false;
}

float ShapeCollectionPanel::calc_section_height(const std::string& category) const {
    auto it = categories_.find(category);
    if (it == categories_.end()) return 0;
    int count = static_cast<int>(it->second.size());
    int rows = (count + columns_ - 1) / columns_;
    return rows * cell_size_ + padding_ * 2;
}

void ShapeCollectionPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    // Panel background using base class
    render_background(renderer);

    float current_y = y_;
    const char* cats[] = {"Basic", "Polygons", "Stars", "Special"};
    const char* titles[] = {"Basic Shapes", "Polygons", "Stars", "Special"};

    for (int i = 0; i < 4; ++i) {
        std::string cat = cats[i];
        render_section_header(renderer, titles[i], cat, current_y);
        current_y += header_height_;

        section_y_positions_[cat] = current_y;

        if (expanded_[cat]) {
            render_shapes(renderer, cat, current_y);
            current_y += calc_section_height(cat);
        }
    }
}

void ShapeCollectionPanel::render_section_header(flex::Renderer& renderer, const char* title,
                                                  const std::string& cat, float y) {
    flex::Color header_bg = expanded_[cat] ? flex::Color{0.22f, 0.22f, 0.24f, 1.0f}
                                           : flex::Color{0.19f, 0.19f, 0.21f, 1.0f};
    renderer.draw_rect(x_, y, width_, header_height_, 0, flex::Paint::solid(header_bg), flex::Paint::none(), 0);

    float arrow_x = x_ + 14;
    float arrow_y = y + header_height_ / 2;
    flex::Color arrow_color{0.65f, 0.65f, 0.65f, 1.0f};

    std::string arrow_path;
    if (expanded_[cat]) {
        arrow_path = "M " + std::to_string(arrow_x - 4) + " " + std::to_string(arrow_y - 2) +
                     " L " + std::to_string(arrow_x) + " " + std::to_string(arrow_y + 3) +
                     " L " + std::to_string(arrow_x + 4) + " " + std::to_string(arrow_y - 2);
    } else {
        arrow_path = "M " + std::to_string(arrow_x - 2) + " " + std::to_string(arrow_y - 4) +
                     " L " + std::to_string(arrow_x + 3) + " " + std::to_string(arrow_y) +
                     " L " + std::to_string(arrow_x - 2) + " " + std::to_string(arrow_y + 4);
    }
    renderer.stroke_path(arrow_path, flex::Paint::solid(arrow_color), 2);

    renderer.draw_text(title, x_ + 28, y + header_height_ / 2 - 6, "Arial", 12, false,
                       flex::Color{0.8f, 0.8f, 0.8f, 1.0f});
}

void ShapeCollectionPanel::render_shapes(flex::Renderer& renderer, const std::string& category, float y) {
    auto it = categories_.find(category);
    if (it == categories_.end()) return;

    float content_h = calc_section_height(category);
    renderer.draw_rect(x_, y, width_, content_h, 0,
                       flex::Paint::solid(flex::Color{0.14f, 0.14f, 0.16f, 1.0f}), flex::Paint::none(), 0);

    const auto& indices = it->second;
    float start_x = x_ + padding_;
    float start_y = y + padding_;

    for (size_t i = 0; i < indices.size(); ++i) {
        int col = i % columns_;
        int row = i / columns_;

        float cx = start_x + col * cell_size_ + cell_size_ / 2;
        float cy = start_y + row * cell_size_ + cell_size_ / 2;

        const auto& tmpl = shapes_[indices[i]];
        float size = cell_size_ * 0.6f;
        flex::Paint fill = flex::Paint::solid(tmpl.fill);
        flex::Paint stroke = flex::Paint::solid(tmpl.stroke);

        flex::Paint cell_bg = flex::Paint::solid(flex::Color(0.22f, 0.22f, 0.24f, 1.0f));
        renderer.draw_rect(start_x + col * cell_size_, start_y + row * cell_size_,
                          cell_size_ - 2, cell_size_ - 2, 4, cell_bg, flex::Paint::none(), 0);

        switch (tmpl.type) {
            case ShapeTemplate::Type::Rectangle:
                renderer.draw_rect(cx - size/2, cy - size/3, size, size*0.66f, 0, fill, stroke, 1.5f);
                break;
            case ShapeTemplate::Type::RoundedRect:
                renderer.draw_rect(cx - size/2, cy - size/3, size, size*0.66f, 4, fill, stroke, 1.5f);
                break;
            case ShapeTemplate::Type::Circle:
                renderer.draw_circle(cx, cy, size/2.5f, fill, stroke, 1.5f);
                break;
            case ShapeTemplate::Type::Ellipse:
                renderer.draw_ellipse(cx, cy, size/2, size/3, fill, stroke, 1.5f);
                break;
            case ShapeTemplate::Type::Triangle:
            case ShapeTemplate::Type::Diamond:
            case ShapeTemplate::Type::Pentagon:
            case ShapeTemplate::Type::Hexagon: {
                int sides = 3;
                float rotation = -M_PI / 2;
                if (tmpl.type == ShapeTemplate::Type::Diamond) { sides = 4; rotation = 0; }
                else if (tmpl.type == ShapeTemplate::Type::Pentagon) sides = 5;
                else if (tmpl.type == ShapeTemplate::Type::Hexagon) sides = 6;

                std::string path = "M ";
                float r = size / 2.5f;
                for (int j = 0; j <= sides; ++j) {
                    float angle = rotation + j * 2 * M_PI / sides;
                    float px = cx + r * std::cos(angle);
                    float py = cy + r * std::sin(angle);
                    if (j == 0) path += std::to_string(px) + " " + std::to_string(py);
                    else path += " L " + std::to_string(px) + " " + std::to_string(py);
                }
                path += " Z";
                renderer.fill_path(path, fill);
                renderer.stroke_path(path, stroke, 1.5f);
                break;
            }
            case ShapeTemplate::Type::Star5:
            case ShapeTemplate::Type::Star6: {
                int points = (tmpl.type == ShapeTemplate::Type::Star5) ? 5 : 6;
                float outer = size / 2.5f;
                float inner = outer * 0.4f;

                std::string path = "M ";
                for (int j = 0; j < points * 2; ++j) {
                    float angle = -M_PI / 2 + j * M_PI / points;
                    float r = (j % 2 == 0) ? outer : inner;
                    float px = cx + r * std::cos(angle);
                    float py = cy + r * std::sin(angle);
                    if (j == 0) path += std::to_string(px) + " " + std::to_string(py);
                    else path += " L " + std::to_string(px) + " " + std::to_string(py);
                }
                path += " Z";
                renderer.fill_path(path, fill);
                renderer.stroke_path(path, stroke, 1.5f);
                break;
            }
            case ShapeTemplate::Type::Arrow: {
                float w = size * 0.8f;
                float h = size * 0.4f;
                std::string path = "M " + std::to_string(cx - w/2) + " " + std::to_string(cy) +
                                  " L " + std::to_string(cx + w/4) + " " + std::to_string(cy) +
                                  " L " + std::to_string(cx + w/4) + " " + std::to_string(cy - h/2) +
                                  " L " + std::to_string(cx + w/2) + " " + std::to_string(cy) +
                                  " L " + std::to_string(cx + w/4) + " " + std::to_string(cy + h/2) +
                                  " L " + std::to_string(cx + w/4) + " " + std::to_string(cy) + " Z";
                renderer.fill_path(path, fill);
                renderer.stroke_path(path, stroke, 1.5f);
                break;
            }
            case ShapeTemplate::Type::Ring: {
                float outer = size / 2.5f;
                renderer.draw_circle(cx, cy, outer, flex::Paint::none(), stroke, 3.0f);
                renderer.draw_circle(cx, cy, outer * 0.5f, flex::Paint::none(), stroke, 2.0f);
                break;
            }
            case ShapeTemplate::Type::Line:
                renderer.stroke_path(
                    "M " + std::to_string(cx - size/2) + " " + std::to_string(cy) +
                    " L " + std::to_string(cx + size/2) + " " + std::to_string(cy),
                    stroke, 2.0f);
                break;
        }
    }
}

bool ShapeCollectionPanel::handle_click(float screen_x, float screen_y) {
    if (!contains(screen_x, screen_y)) return false;

    float local_y = screen_y - y_;
    float current_y = 0;
    const char* cats[] = {"Basic", "Polygons", "Stars", "Special"};

    for (int i = 0; i < 4; ++i) {
        std::string cat = cats[i];

        if (local_y >= current_y && local_y < current_y + header_height_) {
            expanded_[cat] = !expanded_[cat];
            return true;
        }
        current_y += header_height_;

        if (expanded_[cat]) {
            float content_h = calc_section_height(cat);
            if (local_y >= current_y && local_y < current_y + content_h) {
                float local_x = screen_x - x_;
                int idx = hit_test_shape(local_x, local_y - current_y, cat);
                if (idx >= 0) {
                    auto it = categories_.find(cat);
                    if (it != categories_.end() && idx < static_cast<int>(it->second.size())) {
                        add_shape_to_canvas(shapes_[it->second[idx]]);
                        return true;
                    }
                }
            }
            current_y += content_h;
        }
    }

    return false;
}

int ShapeCollectionPanel::hit_test_shape(float local_x, float local_y, const std::string& category) const {
    auto it = categories_.find(category);
    if (it == categories_.end()) return -1;

    if (local_x < padding_ || local_y < padding_) return -1;

    int col = static_cast<int>((local_x - padding_) / cell_size_);
    int row = static_cast<int>((local_y - padding_) / cell_size_);

    if (col < 0 || col >= columns_) return -1;

    int idx = row * columns_ + col;
    if (idx < 0 || idx >= static_cast<int>(it->second.size())) return -1;

    return idx;
}

void ShapeCollectionPanel::add_shape_to_canvas(const ShapeTemplate& tmpl) {
    auto* layer = canvas_->content_root();
    if (!layer) return;

    auto layers = canvas_->get_all_layers();
    if (!layers.empty()) {
        layer = layers[0];
    }

    auto* allocator = canvas_->instance()->object_allocator();

    float vp_world_x = -canvas_->camera_pan_x() / canvas_->camera_zoom();
    float vp_world_y = -canvas_->camera_pan_y() / canvas_->camera_zoom();
    float vp_world_w = canvas_->width() / canvas_->camera_zoom();
    float vp_world_h = canvas_->height() / canvas_->camera_zoom();

    float cx = vp_world_x + vp_world_w / 2;
    float cy = vp_world_y + vp_world_h / 2;

    float size = 80.0f;
    auto* shape = flex::Shape::create(*allocator);

    switch (tmpl.type) {
        case ShapeTemplate::Type::Rectangle:
            shape->set_rect(size * 1.2f, size * 0.8f, 0);
            shape->set_position(cx - size * 0.6f, cy - size * 0.4f);
            break;
        case ShapeTemplate::Type::RoundedRect:
            shape->set_rect(size * 1.2f, size * 0.8f, 8);
            shape->set_position(cx - size * 0.6f, cy - size * 0.4f);
            break;
        case ShapeTemplate::Type::Circle:
            shape->set_circle(size / 2);
            shape->set_position(cx, cy);
            break;
        case ShapeTemplate::Type::Ellipse:
            shape->set_ellipse(size * 0.6f, size * 0.4f);
            shape->set_position(cx, cy);
            break;
        case ShapeTemplate::Type::Triangle:
            shape->set_polygon(3, size / 2);
            shape->set_position(cx, cy);
            break;
        case ShapeTemplate::Type::Diamond:
            shape->set_polygon(4, size / 2);
            shape->set_position(cx, cy);
            break;
        case ShapeTemplate::Type::Pentagon:
            shape->set_polygon(5, size / 2);
            shape->set_position(cx, cy);
            break;
        case ShapeTemplate::Type::Hexagon:
            shape->set_polygon(6, size / 2);
            shape->set_position(cx, cy);
            break;
        case ShapeTemplate::Type::Star5:
            shape->set_star(5, size / 2, size / 5);
            shape->set_position(cx, cy);
            break;
        case ShapeTemplate::Type::Star6:
            shape->set_star(6, size / 2, size / 4);
            shape->set_position(cx, cy);
            break;
        case ShapeTemplate::Type::Arrow:
            shape->set_triangle(size, size * 0.6f, flex::Direction::Right);
            shape->set_position(cx - size / 2, cy - size * 0.3f);
            break;
        case ShapeTemplate::Type::Ring:
            shape->set_ring(size / 2, size / 4);
            shape->set_position(cx, cy);
            break;
        case ShapeTemplate::Type::Line:
            shape->set_line(size, 0);
            shape->set_position(cx - size / 2, cy);
            break;
    }

    shape->set_fill(tmpl.fill);
    shape->set_stroke(tmpl.stroke, tmpl.stroke_width);

    if (tmpl.rough) {
        shape->set_rough(flex::RoughOptions::sketch());
    }

    layer->add_child(shape);

    if (on_shape_added_) {
        on_shape_added_(shape);
    }
}

} // namespace meta_editor
