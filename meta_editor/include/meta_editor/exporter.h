/*
 * Meta Editor - Export functionality
 *
 * SVG, PNG, and clipboard export for sharing designs.
 */

#pragma once

#include <flex.h>
#include <string>
#include <vector>
#include <cstdint>

namespace meta_editor {

class Canvas;

class Exporter {
public:
    explicit Exporter(Canvas* canvas);

    // Export entire canvas to SVG string
    std::string to_svg() const;

    // Export selected nodes to SVG string
    std::string selection_to_svg(const std::vector<flex::Node*>& nodes) const;

    // Save SVG to file
    bool save_svg(const std::string& path) const;

    // Save PNG from pixel buffer (ARGB format)
    bool save_png(const std::string& path, const uint32_t* buffer, int width, int height) const;

    // Copy SVG to clipboard (platform-specific)
    bool copy_to_clipboard(const std::string& svg) const;

private:
    std::string node_to_svg(flex::Node* node, int indent = 2) const;
    std::string shape_to_svg(flex::Shape* shape, int indent) const;
    std::string group_to_svg(flex::Group* group, int indent) const;
    std::string color_to_css(const flex::Color& color) const;
    std::string indent_str(int level) const;

    Canvas* canvas_;
};

} // namespace meta_editor
