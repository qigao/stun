/*
 * Meta Editor - SVG Importer
 *
 * Parse SVG files into editable flex::Node tree.
 * Supports: rect, circle, ellipse, line, polyline, polygon, path, g (groups)
 */

#pragma once

#include <flex.h>
#include <string>
#include <vector>

namespace meta_editor {

class Canvas;

class SvgImporter {
public:
    explicit SvgImporter(Canvas* canvas);

    // Import from file
    bool import_file(const std::string& path);

    // Import from SVG string
    bool import_string(const std::string& svg_content);

    // Get imported nodes (after import)
    const std::vector<flex::Node*>& imported_nodes() const { return imported_nodes_; }

    // Error message if import failed
    const std::string& error() const { return error_; }

private:
    struct ParseContext {
        flex::Color fill = {0.8f, 0.8f, 0.8f, 1.0f};
        flex::Color stroke = {0, 0, 0, 1};
        float stroke_width = 1.0f;
        bool has_fill = true;
        bool has_stroke = true;
        float opacity = 1.0f;
    };

    flex::Node* parse_element(const char*& p, const ParseContext& ctx);
    flex::Shape* parse_rect(const char*& p, const ParseContext& ctx);
    flex::Shape* parse_circle(const char*& p, const ParseContext& ctx);
    flex::Shape* parse_ellipse(const char*& p, const ParseContext& ctx);
    flex::Shape* parse_line(const char*& p, const ParseContext& ctx);
    flex::Shape* parse_polyline(const char*& p, const ParseContext& ctx);
    flex::Shape* parse_polygon(const char*& p, const ParseContext& ctx);
    flex::Shape* parse_path(const char*& p, const ParseContext& ctx);
    flex::Group* parse_group(const char*& p, const ParseContext& ctx);

    // Attribute parsing
    bool parse_attribute(const char*& p, std::string& name, std::string& value);
    void apply_style(const std::string& style, ParseContext& ctx);
    flex::Color parse_color(const std::string& str);
    std::vector<float> parse_points(const std::string& str);

    // Utilities
    void skip_whitespace(const char*& p);
    void skip_to_tag_end(const char*& p);
    bool match(const char*& p, const char* str);
    std::string read_until(const char*& p, char delim);

    Canvas* canvas_;
    std::vector<flex::Node*> imported_nodes_;
    std::string error_;
};

} // namespace meta_editor
