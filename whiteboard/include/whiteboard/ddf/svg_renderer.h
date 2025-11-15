#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <map>

namespace whiteboard {
namespace ddf {

// Forward declarations
class DDFDocument;
struct Shape;
struct Connector;
enum class ArrowType;

/**
 * @brief SVG renderer for exporting DDF documents to SVG format
 * 
 * The SVGRenderer converts DDF documents to valid SVG output while preserving
 * visual fidelity. It handles shapes, groups, transforms, styles, and connectors.
 */
class SVGRenderer {
public:
    SVGRenderer() = default;
    ~SVGRenderer() = default;

    /**
     * @brief Render a complete DDF document to SVG string
     * @param doc The DDF document to render
     * @return Complete SVG document as string
     */
    std::string render(const DDFDocument& doc);

    /**
     * @brief Render a single shape to SVG element
     * @param shape The shape to render
     * @param indent Indentation level for formatting
     * @return SVG element as string
     */
    std::string render_shape(const Shape& shape, int indent = 0);

    /**
     * @brief Render a connector to SVG path
     * @param connector The connector to render
     * @param indent Indentation level for formatting
     * @return SVG path element as string
     */
    std::string render_connector(const Connector& connector, int indent = 0);

private:
    // Shape rendering methods
    std::string render_rect(const Shape& shape, int indent);
    std::string render_circle(const Shape& shape, int indent);
    std::string render_ellipse(const Shape& shape, int indent);
    std::string render_path(const Shape& shape, int indent);
    std::string render_text(const Shape& shape, int indent);
    std::string render_line(const Shape& shape, int indent);
    std::string render_polygon(const Shape& shape, int indent);
    std::string render_polyline(const Shape& shape, int indent);
    std::string render_group(const Shape& shape, const DDFDocument& doc, int indent);

    // Boolean group rendering (masks and clip-paths)
    std::string render_boolean_group(const Shape& shape, const DDFDocument& doc, int indent);

    // Helper methods
    std::string get_indent(int level) const;
    std::string format_style(const std::map<std::string, std::string>& style) const;
    std::string format_transform(const std::vector<float>& transform) const;
    std::string escape_xml(const std::string& str) const;
    
    // Arrow marker generation
    std::string generate_arrow_marker(const std::string& id, const std::string& color, 
                                     const std::string& type);
    
    // Path generation for connectors
    std::string generate_path_data(const std::vector<float>& points) const;

    // Track generated marker IDs to avoid duplicates
    std::map<std::string, bool> generated_markers_;
    
    // Current document being rendered (for context)
    const DDFDocument* current_doc_ = nullptr;
    
    // Helper to convert ArrowType enum to string
    std::string arrow_type_to_string(ArrowType type) const;
};

} // namespace ddf
} // namespace whiteboard
