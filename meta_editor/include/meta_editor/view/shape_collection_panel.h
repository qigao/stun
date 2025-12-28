/*
 * Meta Editor - Shape Collection Panel
 *
 * A panel showing a library of shapes organized in accordion sections.
 * Click a shape to add it at the center of the viewport.
 */

#pragma once

#include "panel.h"
#include <string>
#include <vector>
#include <functional>
#include <map>

namespace meta_editor {

class Canvas;

// Shape template definition
struct ShapeTemplate {
    std::string name;
    std::string category;
    enum class Type {
        Rectangle,
        RoundedRect,
        Circle,
        Ellipse,
        Triangle,
        Diamond,
        Pentagon,
        Hexagon,
        Star5,
        Star6,
        Arrow,
        Line,
        Ring
    } type;

    flex::Color fill = {0.5f, 0.7f, 0.9f, 1.0f};
    flex::Color stroke = {0.0f, 0.0f, 0.0f, 1.0f};
    float stroke_width = 2.0f;
    bool rough = true;
};

class ShapeCollectionPanel : public Panel {
public:
    explicit ShapeCollectionPanel(Canvas* canvas);

    void render(flex::Renderer& renderer) override;
    bool handle_click(float screen_x, float screen_y);

    using ShapeAddedCallback = std::function<void(flex::Shape*)>;
    void set_shape_added_callback(ShapeAddedCallback cb) { on_shape_added_ = std::move(cb); }

protected:
    float content_height() const override;

private:
    void build_shape_library();
    void add_shape_to_canvas(const ShapeTemplate& tmpl);
    void render_section_header(flex::Renderer& renderer, const char* title,
                               const std::string& cat, float y);
    void render_shapes(flex::Renderer& renderer, const std::string& category, float y);
    float calc_section_height(const std::string& category) const;
    int hit_test_shape(float local_x, float local_y, const std::string& category) const;

    Canvas* canvas_;
    float cell_size_ = 36;
    float padding_ = 6;
    float header_height_ = 32;
    int columns_ = 4;

    std::vector<ShapeTemplate> shapes_;
    std::map<std::string, std::vector<size_t>> categories_;
    std::map<std::string, bool> expanded_;
    std::map<std::string, float> section_y_positions_;

    ShapeAddedCallback on_shape_added_;
};

} // namespace meta_editor
