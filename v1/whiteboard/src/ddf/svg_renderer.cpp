#include "whiteboard/ddf/svg_renderer.h"
#include "whiteboard/ddf/ddf_document.h"
#include "whiteboard/ddf/shape_layer.h"
#include "whiteboard/ddf/connector_layer.h"
#include "whiteboard/ddf/style_layer.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>

namespace whiteboard {
namespace ddf {

std::string SVGRenderer::render(const DDFDocument& doc) {
    current_doc_ = &doc;
    generated_markers_.clear();
    
    std::ostringstream svg;
    
    // SVG header
    svg << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    svg << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
    svg << "xmlns:xlink=\"http://www.w3.org/1999/xlink\" ";
    
    // Calculate bounding box for viewBox
    // For now, use a default size - could be calculated from shapes
    svg << "viewBox=\"0 0 1000 1000\" ";
    svg << "width=\"1000\" height=\"1000\">\n";
    
    // Add metadata
    const auto& metadata = doc.metadata();
    if (!metadata.empty()) {
        svg << get_indent(1) << "<metadata>\n";
        for (const auto& [key, value] : metadata) {
            svg << get_indent(2) << "<" << escape_xml(key) << ">" 
                << escape_xml(value) << "</" << escape_xml(key) << ">\n";
        }
        svg << get_indent(1) << "</metadata>\n";
    }
    
    // Add defs section for markers and other reusable elements
    std::ostringstream defs;
    defs << get_indent(1) << "<defs>\n";
    
    // Generate arrow markers for connectors
    const auto& connector_layer = doc.connector_layer();
    auto connectors = connector_layer.get_all_connectors();
    
    for (const auto* connector : connectors) {
        if (connector) {
            std::string color = "#000000";
            auto it = connector->style.find("stroke");
            if (it != connector->style.end()) {
                color = it->second;
            }
            
            if (connector->arrow_start != ArrowType::None) {
                std::string marker_id = "arrow_start_" + connector->id;
                if (generated_markers_.find(marker_id) == generated_markers_.end()) {
                    std::string type = arrow_type_to_string(connector->arrow_start);
                    defs << generate_arrow_marker(marker_id, color, type);
                    generated_markers_[marker_id] = true;
                }
            }
            
            if (connector->arrow_end != ArrowType::None) {
                std::string marker_id = "arrow_end_" + connector->id;
                if (generated_markers_.find(marker_id) == generated_markers_.end()) {
                    std::string type = arrow_type_to_string(connector->arrow_end);
                    defs << generate_arrow_marker(marker_id, color, type);
                    generated_markers_[marker_id] = true;
                }
            }
        }
    }
    
    defs << get_indent(1) << "</defs>\n";
    svg << defs.str();
    
    // Render all root shapes (shapes without parents)
    const auto& shape_layer = doc.shape_layer();
    auto root_shapes = shape_layer.get_root_shapes();
    
    for (const auto* shape : root_shapes) {
        if (shape) {
            svg << render_shape(*shape, 1);
        }
    }
    
    // Render all connectors (reuse connector_layer and connectors from above)
    for (const auto* connector : connectors) {
        if (connector) {
            svg << render_connector(*connector, 1);
        }
    }
    
    // Close SVG
    svg << "</svg>\n";
    
    current_doc_ = nullptr;
    return svg.str();
}

std::string SVGRenderer::render_shape(const Shape& shape, int indent) {
    // Dispatch to appropriate rendering method based on shape type
    if (shape.type == "rect") {
        return render_rect(shape, indent);
    } else if (shape.type == "circle") {
        return render_circle(shape, indent);
    } else if (shape.type == "ellipse") {
        return render_ellipse(shape, indent);
    } else if (shape.type == "path") {
        return render_path(shape, indent);
    } else if (shape.type == "text") {
        return render_text(shape, indent);
    } else if (shape.type == "line") {
        return render_line(shape, indent);
    } else if (shape.type == "polygon") {
        return render_polygon(shape, indent);
    } else if (shape.type == "polyline") {
        return render_polyline(shape, indent);
    } else if (shape.type == "group") {
        return render_group(shape, *current_doc_, indent);
    } else if (shape.type == "boolean_group") {
        return render_boolean_group(shape, *current_doc_, indent);
    }
    
    return "";
}

std::string SVGRenderer::render_rect(const Shape& shape, int indent) {
    std::ostringstream svg;
    svg << get_indent(indent) << "<rect";
    
    // Add ID
    if (!shape.id.empty()) {
        svg << " id=\"" << escape_xml(shape.id) << "\"";
    }
    
    // Add classes
    if (!shape.classes.empty()) {
        svg << " class=\"";
        for (size_t i = 0; i < shape.classes.size(); ++i) {
            if (i > 0) svg << " ";
            svg << escape_xml(shape.classes[i]);
        }
        svg << "\"";
    }
    
    // Add geometry
    auto it = shape.geometry.find("x");
    if (it != shape.geometry.end()) {
        svg << " x=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("y");
    if (it != shape.geometry.end()) {
        svg << " y=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("width");
    if (it != shape.geometry.end()) {
        svg << " width=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("height");
    if (it != shape.geometry.end()) {
        svg << " height=\"" << it->second << "\"";
    }
    
    // Add rounded corners if present
    it = shape.geometry.find("rx");
    if (it != shape.geometry.end()) {
        svg << " rx=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("ry");
    if (it != shape.geometry.end()) {
        svg << " ry=\"" << it->second << "\"";
    }
    
    // Add transform
    if (!shape.transform.empty()) {
        svg << " transform=\"" << format_transform(shape.transform) << "\"";
    }
    
    // Add style
    if (!shape.inline_style.empty()) {
        svg << " style=\"" << format_style(shape.inline_style) << "\"";
    }
    
    svg << " />\n";
    return svg.str();
}

std::string SVGRenderer::render_circle(const Shape& shape, int indent) {
    std::ostringstream svg;
    svg << get_indent(indent) << "<circle";
    
    if (!shape.id.empty()) {
        svg << " id=\"" << escape_xml(shape.id) << "\"";
    }
    
    if (!shape.classes.empty()) {
        svg << " class=\"";
        for (size_t i = 0; i < shape.classes.size(); ++i) {
            if (i > 0) svg << " ";
            svg << escape_xml(shape.classes[i]);
        }
        svg << "\"";
    }
    
    auto it = shape.geometry.find("cx");
    if (it != shape.geometry.end()) {
        svg << " cx=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("cy");
    if (it != shape.geometry.end()) {
        svg << " cy=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("r");
    if (it != shape.geometry.end()) {
        svg << " r=\"" << it->second << "\"";
    }
    
    if (!shape.transform.empty()) {
        svg << " transform=\"" << format_transform(shape.transform) << "\"";
    }
    
    if (!shape.inline_style.empty()) {
        svg << " style=\"" << format_style(shape.inline_style) << "\"";
    }
    
    svg << " />\n";
    return svg.str();
}

std::string SVGRenderer::render_ellipse(const Shape& shape, int indent) {
    std::ostringstream svg;
    svg << get_indent(indent) << "<ellipse";
    
    if (!shape.id.empty()) {
        svg << " id=\"" << escape_xml(shape.id) << "\"";
    }
    
    if (!shape.classes.empty()) {
        svg << " class=\"";
        for (size_t i = 0; i < shape.classes.size(); ++i) {
            if (i > 0) svg << " ";
            svg << escape_xml(shape.classes[i]);
        }
        svg << "\"";
    }
    
    auto it = shape.geometry.find("cx");
    if (it != shape.geometry.end()) {
        svg << " cx=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("cy");
    if (it != shape.geometry.end()) {
        svg << " cy=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("rx");
    if (it != shape.geometry.end()) {
        svg << " rx=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("ry");
    if (it != shape.geometry.end()) {
        svg << " ry=\"" << it->second << "\"";
    }
    
    if (!shape.transform.empty()) {
        svg << " transform=\"" << format_transform(shape.transform) << "\"";
    }
    
    if (!shape.inline_style.empty()) {
        svg << " style=\"" << format_style(shape.inline_style) << "\"";
    }
    
    svg << " />\n";
    return svg.str();
}


std::string SVGRenderer::render_path(const Shape& shape, int indent) {
    std::ostringstream svg;
    svg << get_indent(indent) << "<path";
    
    if (!shape.id.empty()) {
        svg << " id=\"" << escape_xml(shape.id) << "\"";
    }
    
    if (!shape.classes.empty()) {
        svg << " class=\"";
        for (size_t i = 0; i < shape.classes.size(); ++i) {
            if (i > 0) svg << " ";
            svg << escape_xml(shape.classes[i]);
        }
        svg << "\"";
    }
    
    // Path data should be stored in text field
    if (!shape.text.empty()) {
        svg << " d=\"" << escape_xml(shape.text) << "\"";
    }
    
    if (!shape.transform.empty()) {
        svg << " transform=\"" << format_transform(shape.transform) << "\"";
    }
    
    if (!shape.inline_style.empty()) {
        svg << " style=\"" << format_style(shape.inline_style) << "\"";
    }
    
    svg << " />\n";
    return svg.str();
}

std::string SVGRenderer::render_text(const Shape& shape, int indent) {
    std::ostringstream svg;
    svg << get_indent(indent) << "<text";
    
    if (!shape.id.empty()) {
        svg << " id=\"" << escape_xml(shape.id) << "\"";
    }
    
    if (!shape.classes.empty()) {
        svg << " class=\"";
        for (size_t i = 0; i < shape.classes.size(); ++i) {
            if (i > 0) svg << " ";
            svg << escape_xml(shape.classes[i]);
        }
        svg << "\"";
    }
    
    auto it = shape.geometry.find("x");
    if (it != shape.geometry.end()) {
        svg << " x=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("y");
    if (it != shape.geometry.end()) {
        svg << " y=\"" << it->second << "\"";
    }
    
    if (!shape.transform.empty()) {
        svg << " transform=\"" << format_transform(shape.transform) << "\"";
    }
    
    if (!shape.inline_style.empty()) {
        svg << " style=\"" << format_style(shape.inline_style) << "\"";
    }
    
    svg << ">";
    svg << escape_xml(shape.text);
    svg << "</text>\n";
    
    return svg.str();
}

std::string SVGRenderer::render_line(const Shape& shape, int indent) {
    std::ostringstream svg;
    svg << get_indent(indent) << "<line";
    
    if (!shape.id.empty()) {
        svg << " id=\"" << escape_xml(shape.id) << "\"";
    }
    
    if (!shape.classes.empty()) {
        svg << " class=\"";
        for (size_t i = 0; i < shape.classes.size(); ++i) {
            if (i > 0) svg << " ";
            svg << escape_xml(shape.classes[i]);
        }
        svg << "\"";
    }
    
    auto it = shape.geometry.find("x1");
    if (it != shape.geometry.end()) {
        svg << " x1=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("y1");
    if (it != shape.geometry.end()) {
        svg << " y1=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("x2");
    if (it != shape.geometry.end()) {
        svg << " x2=\"" << it->second << "\"";
    }
    
    it = shape.geometry.find("y2");
    if (it != shape.geometry.end()) {
        svg << " y2=\"" << it->second << "\"";
    }
    
    if (!shape.transform.empty()) {
        svg << " transform=\"" << format_transform(shape.transform) << "\"";
    }
    
    if (!shape.inline_style.empty()) {
        svg << " style=\"" << format_style(shape.inline_style) << "\"";
    }
    
    svg << " />\n";
    return svg.str();
}

std::string SVGRenderer::render_polygon(const Shape& shape, int indent) {
    std::ostringstream svg;
    svg << get_indent(indent) << "<polygon";
    
    if (!shape.id.empty()) {
        svg << " id=\"" << escape_xml(shape.id) << "\"";
    }
    
    if (!shape.classes.empty()) {
        svg << " class=\"";
        for (size_t i = 0; i < shape.classes.size(); ++i) {
            if (i > 0) svg << " ";
            svg << escape_xml(shape.classes[i]);
        }
        svg << "\"";
    }
    
    // Points should be stored in text field as "x1,y1 x2,y2 x3,y3"
    if (!shape.text.empty()) {
        svg << " points=\"" << escape_xml(shape.text) << "\"";
    }
    
    if (!shape.transform.empty()) {
        svg << " transform=\"" << format_transform(shape.transform) << "\"";
    }
    
    if (!shape.inline_style.empty()) {
        svg << " style=\"" << format_style(shape.inline_style) << "\"";
    }
    
    svg << " />\n";
    return svg.str();
}

std::string SVGRenderer::render_polyline(const Shape& shape, int indent) {
    std::ostringstream svg;
    svg << get_indent(indent) << "<polyline";
    
    if (!shape.id.empty()) {
        svg << " id=\"" << escape_xml(shape.id) << "\"";
    }
    
    if (!shape.classes.empty()) {
        svg << " class=\"";
        for (size_t i = 0; i < shape.classes.size(); ++i) {
            if (i > 0) svg << " ";
            svg << escape_xml(shape.classes[i]);
        }
        svg << "\"";
    }
    
    // Points should be stored in text field as "x1,y1 x2,y2 x3,y3"
    if (!shape.text.empty()) {
        svg << " points=\"" << escape_xml(shape.text) << "\"";
    }
    
    if (!shape.transform.empty()) {
        svg << " transform=\"" << format_transform(shape.transform) << "\"";
    }
    
    if (!shape.inline_style.empty()) {
        svg << " style=\"" << format_style(shape.inline_style) << "\"";
    }
    
    svg << " />\n";
    return svg.str();
}

std::string SVGRenderer::render_group(const Shape& shape, const DDFDocument& doc, int indent) {
    std::ostringstream svg;
    svg << get_indent(indent) << "<g";
    
    if (!shape.id.empty()) {
        svg << " id=\"" << escape_xml(shape.id) << "\"";
    }
    
    if (!shape.classes.empty()) {
        svg << " class=\"";
        for (size_t i = 0; i < shape.classes.size(); ++i) {
            if (i > 0) svg << " ";
            svg << escape_xml(shape.classes[i]);
        }
        svg << "\"";
    }
    
    if (!shape.transform.empty()) {
        svg << " transform=\"" << format_transform(shape.transform) << "\"";
    }
    
    if (!shape.inline_style.empty()) {
        svg << " style=\"" << format_style(shape.inline_style) << "\"";
    }
    
    svg << ">\n";
    
    // Render children
    const auto& shape_layer = doc.shape_layer();
    for (const auto& child_id : shape.child_shape_ids) {
        const auto* child = shape_layer.get_shape(child_id);
        if (child) {
            svg << render_shape(*child, indent + 1);
        }
    }
    
    svg << get_indent(indent) << "</g>\n";
    return svg.str();
}

std::string SVGRenderer::render_boolean_group(const Shape& shape, const DDFDocument& doc, int indent) {
    std::ostringstream svg;
    
    const auto& shape_layer = doc.shape_layer();
    
    // Get boolean operation type from inline_style or geometry
    std::string operation = "union";  // default
    auto op_it = shape.inline_style.find("boolean-operation");
    if (op_it != shape.inline_style.end()) {
        operation = op_it->second;
    }
    
    // Get children
    std::vector<const Shape*> children;
    for (const auto& child_id : shape.child_shape_ids) {
        const auto* child = shape_layer.get_shape(child_id);
        if (child) {
            children.push_back(child);
        }
    }
    
    if (children.empty()) {
        return "";
    }
    
    svg << get_indent(indent) << "<!-- Boolean group: " << escape_xml(shape.id) 
        << " operation: " << operation << " -->\n";
    
    if (operation == "subtract" && children.size() >= 2) {
        // Use SVG mask for subtract operation
        std::string mask_id = shape.id + "_mask";
        
        // Define mask
        svg << get_indent(indent) << "<defs>\n";
        svg << get_indent(indent + 1) << "<mask id=\"" << mask_id << "\">\n";
        
        // First child is white (visible)
        svg << get_indent(indent + 2) << "<g style=\"fill: white; stroke: white;\">\n";
        svg << render_shape(*children[0], indent + 3);
        svg << get_indent(indent + 2) << "</g>\n";
        
        // Subsequent children are black (hidden)
        for (size_t i = 1; i < children.size(); ++i) {
            svg << get_indent(indent + 2) << "<g style=\"fill: black; stroke: black;\">\n";
            svg << render_shape(*children[i], indent + 3);
            svg << get_indent(indent + 2) << "</g>\n";
        }
        
        svg << get_indent(indent + 1) << "</mask>\n";
        svg << get_indent(indent) << "</defs>\n";
        
        // Apply mask to first child
        svg << get_indent(indent) << "<g";
        if (!shape.id.empty()) {
            svg << " id=\"" << escape_xml(shape.id) << "\"";
        }
        if (!shape.transform.empty()) {
            svg << " transform=\"" << format_transform(shape.transform) << "\"";
        }
        svg << " mask=\"url(#" << mask_id << ")\">\n";
        svg << render_shape(*children[0], indent + 1);
        svg << get_indent(indent) << "</g>\n";
        
    } else if (operation == "intersect" && children.size() >= 2) {
        // Use SVG clip-path for intersect operation
        std::string clip_id = shape.id + "_clip";
        
        // Define clip-path from first child
        svg << get_indent(indent) << "<defs>\n";
        svg << get_indent(indent + 1) << "<clipPath id=\"" << clip_id << "\">\n";
        svg << render_shape(*children[0], indent + 2);
        svg << get_indent(indent + 1) << "</clipPath>\n";
        svg << get_indent(indent) << "</defs>\n";
        
        // Apply clip-path to subsequent children
        svg << get_indent(indent) << "<g";
        if (!shape.id.empty()) {
            svg << " id=\"" << escape_xml(shape.id) << "\"";
        }
        if (!shape.transform.empty()) {
            svg << " transform=\"" << format_transform(shape.transform) << "\"";
        }
        svg << " clip-path=\"url(#" << clip_id << ")\">\n";
        
        for (size_t i = 1; i < children.size(); ++i) {
            svg << render_shape(*children[i], indent + 1);
        }
        
        svg << get_indent(indent) << "</g>\n";
        
    } else {
        // Union or exclude - render as regular group
        svg << get_indent(indent) << "<g";
        
        if (!shape.id.empty()) {
            svg << " id=\"" << escape_xml(shape.id) << "\"";
        }
        
        if (!shape.classes.empty()) {
            svg << " class=\"";
            for (size_t i = 0; i < shape.classes.size(); ++i) {
                if (i > 0) svg << " ";
                svg << escape_xml(shape.classes[i]);
            }
            svg << "\"";
        }
        
        if (!shape.transform.empty()) {
            svg << " transform=\"" << format_transform(shape.transform) << "\"";
        }
        
        if (!shape.inline_style.empty()) {
            svg << " style=\"" << format_style(shape.inline_style) << "\"";
        }
        
        svg << ">\n";
        
        // Render all children
        for (const auto* child : children) {
            svg << render_shape(*child, indent + 1);
        }
        
        svg << get_indent(indent) << "</g>\n";
    }
    
    return svg.str();
}

std::string SVGRenderer::render_connector(const Connector& connector, int indent) {
    std::ostringstream svg;
    
    // Generate path from connector points
    if (connector.path_points.empty()) {
        return "";
    }
    
    svg << get_indent(indent) << "<path";
    
    if (!connector.id.empty()) {
        svg << " id=\"" << escape_xml(connector.id) << "\"";
    }
    
    svg << " d=\"" << generate_path_data(connector.path_points) << "\"";
    
    // Add style
    if (!connector.style.empty()) {
        svg << " style=\"" << format_style(connector.style) << "\"";
    }
    
    // Add arrow markers
    if (connector.arrow_start != ArrowType::None) {
        std::string marker_id = "arrow_start_" + connector.id;
        std::string color = "#000000";
        auto it = connector.style.find("stroke");
        if (it != connector.style.end()) {
            color = it->second;
        }
        svg << " marker-start=\"url(#" << marker_id << ")\"";
    }
    
    if (connector.arrow_end != ArrowType::None) {
        std::string marker_id = "arrow_end_" + connector.id;
        std::string color = "#000000";
        auto it = connector.style.find("stroke");
        if (it != connector.style.end()) {
            color = it->second;
        }
        svg << " marker-end=\"url(#" << marker_id << ")\"";
    }
    
    svg << " />\n";
    
    // Render label if present
    if (connector.label) {
        const auto& label = *connector.label;
        
        // Calculate position along path
        size_t num_points = connector.path_points.size() / 2;
        if (num_points >= 2) {
            size_t idx = static_cast<size_t>(label.position * (num_points - 1));
            idx = std::min(idx, num_points - 1);
            
            float x = connector.path_points[idx * 2] + label.offset_x;
            float y = connector.path_points[idx * 2 + 1] + label.offset_y;
            
            svg << get_indent(indent) << "<text";
            svg << " x=\"" << x << "\"";
            svg << " y=\"" << y << "\"";
            svg << " text-anchor=\"middle\"";
            svg << ">";
            svg << escape_xml(label.text);
            svg << "</text>\n";
        }
    }
    
    return svg.str();
}

// Helper methods

std::string SVGRenderer::get_indent(int level) const {
    return std::string(level * 2, ' ');
}

std::string SVGRenderer::format_style(const std::map<std::string, std::string>& style) const {
    std::ostringstream ss;
    bool first = true;
    for (const auto& [key, value] : style) {
        if (!first) ss << "; ";
        ss << key << ": " << value;
        first = false;
    }
    return ss.str();
}

std::string SVGRenderer::format_transform(const std::vector<float>& transform) const {
    if (transform.size() != 6) {
        return "";
    }
    
    std::ostringstream ss;
    ss << "matrix(" 
       << transform[0] << "," << transform[1] << ","
       << transform[2] << "," << transform[3] << ","
       << transform[4] << "," << transform[5] << ")";
    return ss.str();
}

std::string SVGRenderer::escape_xml(const std::string& str) const {
    std::string result;
    result.reserve(str.size());
    
    for (char c : str) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default:   result += c; break;
        }
    }
    
    return result;
}

std::string SVGRenderer::generate_arrow_marker(const std::string& id, 
                                               const std::string& color,
                                               const std::string& type) {
    std::ostringstream svg;
    
    svg << get_indent(2) << "<marker id=\"" << id << "\" ";
    svg << "markerWidth=\"10\" markerHeight=\"10\" ";
    svg << "refX=\"5\" refY=\"5\" ";
    svg << "orient=\"auto\">\n";
    
    if (type == "arrow") {
        svg << get_indent(3) << "<path d=\"M 0 0 L 10 5 L 0 10 z\" ";
        svg << "fill=\"" << color << "\" />\n";
    } else if (type == "diamond") {
        svg << get_indent(3) << "<path d=\"M 0 5 L 5 0 L 10 5 L 5 10 z\" ";
        svg << "fill=\"" << color << "\" />\n";
    } else if (type == "circle") {
        svg << get_indent(3) << "<circle cx=\"5\" cy=\"5\" r=\"3\" ";
        svg << "fill=\"" << color << "\" />\n";
    } else if (type == "square") {
        svg << get_indent(3) << "<rect x=\"2\" y=\"2\" width=\"6\" height=\"6\" ";
        svg << "fill=\"" << color << "\" />\n";
    }
    
    svg << get_indent(2) << "</marker>\n";
    
    return svg.str();
}

std::string SVGRenderer::generate_path_data(const std::vector<float>& points) const {
    if (points.size() < 2) {
        return "";
    }
    
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    
    // Move to first point
    ss << "M " << points[0] << " " << points[1];
    
    // Line to subsequent points
    for (size_t i = 2; i < points.size(); i += 2) {
        ss << " L " << points[i] << " " << points[i + 1];
    }
    
    return ss.str();
}

std::string SVGRenderer::arrow_type_to_string(ArrowType type) const {
    switch (type) {
        case ArrowType::Arrow: return "arrow";
        case ArrowType::Diamond: return "diamond";
        case ArrowType::Circle: return "circle";
        case ArrowType::Square: return "square";
        case ArrowType::None:
        default: return "none";
    }
}

} // namespace ddf
} // namespace whiteboard
