#pragma once

#include "renderer.h"
#include <sstream>
#include <fstream>

namespace flex::modules::flexmaid {

/// Simple SVG string-based renderer
/// This is a temporary implementation until ThorVG integration is complete
class SimpleSVGRenderer : public Renderer {
public:
    void render(const DiagramLayout& layout, const Theme& theme) override;
    bool save_svg(const std::string& path) override;
    bool save_png(const std::string& path, int width, int height) override;
    std::string to_svg_string() override;
    
private:
    void render_node(std::ostringstream& oss, const NodeLayout& node);
    void render_rectangle(std::ostringstream& oss, const NodeLayout& node);
    void render_circle(std::ostringstream& oss, const NodeLayout& node);
    void render_diamond(std::ostringstream& oss, const NodeLayout& node);
    void render_stadium(std::ostringstream& oss, const NodeLayout& node);
    void render_hexagon(std::ostringstream& oss, const NodeLayout& node);
    void render_parallelogram(std::ostringstream& oss, const NodeLayout& node);
    void render_trapezoid(std::ostringstream& oss, const NodeLayout& node);
    void render_cylinder(std::ostringstream& oss, const NodeLayout& node);
    void render_subroutine(std::ostringstream& oss, const NodeLayout& node);
    void render_note(std::ostringstream& oss, const NodeLayout& node);
    void render_edge(std::ostringstream& oss, const EdgeLayout& edge);
    void render_text(std::ostringstream& oss, float x, float y, const std::vector<std::string>& lines);
    void render_text_left(std::ostringstream& oss, float x, float y, const std::string& text);
    
    DiagramLayout layout_;
    Theme theme_;
    std::string svg_content_;
};

} // namespace flex::modules::flexmaid
