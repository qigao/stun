#include <whiteboard/ddf/svg_importer.h>
#include <whiteboard/ddf/ddf_document.h>
#include <whiteboard/ddf/shape_layer.h>
#include <whiteboard/ddf/style_layer.h>
#include <pugixml.hpp>
#include <sstream>
#include <cmath>
#include <algorithm>

namespace whiteboard {
namespace ddf {

// Helper functions for parsing SVG attributes

static float parse_float(const std::string& str, float default_value = 0.0f) {
    try {
        return std::stof(str);
    } catch (...) {
        return default_value;
    }
}

static std::string parse_style_attribute(const std::string& style_str) {
    // Parse inline style attribute like "fill: red; stroke: blue;"
    // Returns a semicolon-separated string for easier processing
    return style_str;
}

static std::map<std::string, std::string> parse_style_to_map(const std::string& style_str) {
    std::map<std::string, std::string> result;
    std::istringstream ss(style_str);
    std::string item;
    
    while (std::getline(ss, item, ';')) {
        size_t colon_pos = item.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = item.substr(0, colon_pos);
            std::string value = item.substr(colon_pos + 1);
            
            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t\n\r"));
            key.erase(key.find_last_not_of(" \t\n\r") + 1);
            value.erase(0, value.find_first_not_of(" \t\n\r"));
            value.erase(value.find_last_not_of(" \t\n\r") + 1);
            
            if (!key.empty() && !value.empty()) {
                result[key] = value;
            }
        }
    }
    
    return result;
}

static std::string normalize_style_property(const std::string& key) {
    // Convert SVG style names to DDF style names
    if (key == "stroke-width") return "stroke_width";
    if (key == "font-size") return "font_size";
    if (key == "font-family") return "font_family";
    if (key == "font-weight") return "font_weight";
    return key;
}

static std::vector<float> parse_transform(const std::string& transform_str) {
    // Parse SVG transform attribute
    // For simplicity, we'll support basic matrix transforms
    // Returns a 6-element vector [a, b, c, d, e, f] for matrix(a,b,c,d,e,f)
    std::vector<float> result;
    
    if (transform_str.empty()) {
        return result;
    }
    
    // Look for matrix(...) or translate(...) etc.
    size_t matrix_pos = transform_str.find("matrix(");
    if (matrix_pos != std::string::npos) {
        size_t start = matrix_pos + 7;
        size_t end = transform_str.find(')', start);
        if (end != std::string::npos) {
            std::string values = transform_str.substr(start, end - start);
            std::istringstream ss(values);
            std::string value;
            while (std::getline(ss, value, ',')) {
                result.push_back(parse_float(value));
            }
        }
    } else {
        // Handle translate(x, y)
        size_t translate_pos = transform_str.find("translate(");
        if (translate_pos != std::string::npos) {
            size_t start = translate_pos + 10;
            size_t end = transform_str.find(')', start);
            if (end != std::string::npos) {
                std::string values = transform_str.substr(start, end - start);
                std::istringstream ss(values);
                float x = 0, y = 0;
                ss >> x;
                if (ss.peek() == ',') ss.ignore();
                ss >> y;
                // Create translation matrix: [1, 0, 0, 1, x, y]
                result = {1.0f, 0.0f, 0.0f, 1.0f, x, y};
            }
        }
    }
    
    return result;
}

// Implementation class
class SVGImporter::Impl {
public:
    bool import(const std::string& svg_string, DDFDocument& doc, std::string& error) {
        pugi::xml_document xml_doc;
        pugi::xml_parse_result result = xml_doc.load_string(svg_string.c_str());
        
        if (!result) {
            error = "Failed to parse SVG: " + std::string(result.description());
            return false;
        }
        
        pugi::xml_node svg_node = xml_doc.child("svg");
        if (!svg_node) {
            error = "No <svg> root element found";
            return false;
        }
        
        // Clear existing shapes
        doc.shape_layer().clear();
        
        // Process SVG elements
        shape_id_counter_ = 0;
        process_node(svg_node, doc, "");
        
        return true;
    }

private:
    int shape_id_counter_ = 0;
    
    std::string generate_shape_id() {
        return "imported_shape_" + std::to_string(++shape_id_counter_);
    }
    
    void process_node(const pugi::xml_node& node, DDFDocument& doc, const std::string& parent_id) {
        for (pugi::xml_node child : node.children()) {
            std::string node_name = child.name();
            
            if (node_name == "rect") {
                process_rect(child, doc, parent_id);
            } else if (node_name == "circle") {
                process_circle(child, doc, parent_id);
            } else if (node_name == "ellipse") {
                process_ellipse(child, doc, parent_id);
            } else if (node_name == "path") {
                process_path(child, doc, parent_id);
            } else if (node_name == "text") {
                process_text(child, doc, parent_id);
            } else if (node_name == "line") {
                process_line(child, doc, parent_id);
            } else if (node_name == "polygon") {
                process_polygon(child, doc, parent_id);
            } else if (node_name == "polyline") {
                process_polyline(child, doc, parent_id);
            } else if (node_name == "g") {
                process_group(child, doc, parent_id);
            }
            // Ignore other elements like <defs>, <style>, etc.
        }
    }
    
    void extract_common_attributes(const pugi::xml_node& node, Shape& shape) {
        // Extract ID
        if (node.attribute("id")) {
            shape.id = node.attribute("id").value();
        } else {
            shape.id = generate_shape_id();
        }
        
        // Extract class
        if (node.attribute("class")) {
            std::string class_str = node.attribute("class").value();
            std::istringstream ss(class_str);
            std::string class_name;
            while (ss >> class_name) {
                shape.classes.push_back(class_name);
            }
        }
        
        // Extract inline styles
        if (node.attribute("style")) {
            std::string style_str = node.attribute("style").value();
            auto style_map = parse_style_to_map(style_str);
            for (const auto& [key, value] : style_map) {
                shape.inline_style[normalize_style_property(key)] = value;
            }
        }
        
        // Extract presentation attributes (fill, stroke, etc.)
        for (const char* attr : {"fill", "stroke", "opacity", "stroke-width", 
                                  "stroke-opacity", "fill-opacity"}) {
            if (node.attribute(attr)) {
                std::string key = normalize_style_property(attr);
                shape.inline_style[key] = node.attribute(attr).value();
            }
        }
        
        // Extract transform
        if (node.attribute("transform")) {
            shape.transform = parse_transform(node.attribute("transform").value());
        }
    }
    
    void process_rect(const pugi::xml_node& node, DDFDocument& doc, const std::string& parent_id) {
        Shape shape;
        shape.type = "rect";
        extract_common_attributes(node, shape);
        
        // Extract geometry
        shape.geometry["x"] = parse_float(node.attribute("x").value());
        shape.geometry["y"] = parse_float(node.attribute("y").value());
        shape.geometry["width"] = parse_float(node.attribute("width").value());
        shape.geometry["height"] = parse_float(node.attribute("height").value());
        
        if (node.attribute("rx")) {
            shape.geometry["rx"] = parse_float(node.attribute("rx").value());
        }
        if (node.attribute("ry")) {
            shape.geometry["ry"] = parse_float(node.attribute("ry").value());
        }
        
        if (!parent_id.empty()) {
            shape.parent_shape_id = parent_id;
        }
        
        doc.shape_layer().add_shape(shape);
    }
    
    void process_circle(const pugi::xml_node& node, DDFDocument& doc, const std::string& parent_id) {
        Shape shape;
        shape.type = "circle";
        extract_common_attributes(node, shape);
        
        // Extract geometry
        shape.geometry["cx"] = parse_float(node.attribute("cx").value());
        shape.geometry["cy"] = parse_float(node.attribute("cy").value());
        shape.geometry["r"] = parse_float(node.attribute("r").value());
        
        if (!parent_id.empty()) {
            shape.parent_shape_id = parent_id;
        }
        
        doc.shape_layer().add_shape(shape);
    }
    
    void process_ellipse(const pugi::xml_node& node, DDFDocument& doc, const std::string& parent_id) {
        Shape shape;
        shape.type = "ellipse";
        extract_common_attributes(node, shape);
        
        // Extract geometry
        shape.geometry["cx"] = parse_float(node.attribute("cx").value());
        shape.geometry["cy"] = parse_float(node.attribute("cy").value());
        shape.geometry["rx"] = parse_float(node.attribute("rx").value());
        shape.geometry["ry"] = parse_float(node.attribute("ry").value());
        
        if (!parent_id.empty()) {
            shape.parent_shape_id = parent_id;
        }
        
        doc.shape_layer().add_shape(shape);
    }
    
    void process_path(const pugi::xml_node& node, DDFDocument& doc, const std::string& parent_id) {
        Shape shape;
        shape.type = "path";
        extract_common_attributes(node, shape);
        
        // Extract path data
        if (node.attribute("d")) {
            shape.inline_style["d"] = node.attribute("d").value();
        }
        
        if (!parent_id.empty()) {
            shape.parent_shape_id = parent_id;
        }
        
        doc.shape_layer().add_shape(shape);
    }
    
    void process_text(const pugi::xml_node& node, DDFDocument& doc, const std::string& parent_id) {
        Shape shape;
        shape.type = "text";
        extract_common_attributes(node, shape);
        
        // Extract geometry
        shape.geometry["x"] = parse_float(node.attribute("x").value());
        shape.geometry["y"] = parse_float(node.attribute("y").value());
        
        // Extract text content
        shape.text = node.child_value();
        
        // Extract text-specific attributes
        for (const char* attr : {"font-size", "font-family", "font-weight", "text-anchor"}) {
            if (node.attribute(attr)) {
                std::string key = normalize_style_property(attr);
                shape.inline_style[key] = node.attribute(attr).value();
            }
        }
        
        if (!parent_id.empty()) {
            shape.parent_shape_id = parent_id;
        }
        
        doc.shape_layer().add_shape(shape);
    }
    
    void process_line(const pugi::xml_node& node, DDFDocument& doc, const std::string& parent_id) {
        Shape shape;
        shape.type = "line";
        extract_common_attributes(node, shape);
        
        // Extract geometry
        shape.geometry["x1"] = parse_float(node.attribute("x1").value());
        shape.geometry["y1"] = parse_float(node.attribute("y1").value());
        shape.geometry["x2"] = parse_float(node.attribute("x2").value());
        shape.geometry["y2"] = parse_float(node.attribute("y2").value());
        
        if (!parent_id.empty()) {
            shape.parent_shape_id = parent_id;
        }
        
        doc.shape_layer().add_shape(shape);
    }
    
    void process_polygon(const pugi::xml_node& node, DDFDocument& doc, const std::string& parent_id) {
        Shape shape;
        shape.type = "polygon";
        extract_common_attributes(node, shape);
        
        // Extract points
        if (node.attribute("points")) {
            shape.inline_style["points"] = node.attribute("points").value();
        }
        
        if (!parent_id.empty()) {
            shape.parent_shape_id = parent_id;
        }
        
        doc.shape_layer().add_shape(shape);
    }
    
    void process_polyline(const pugi::xml_node& node, DDFDocument& doc, const std::string& parent_id) {
        Shape shape;
        shape.type = "polyline";
        extract_common_attributes(node, shape);
        
        // Extract points
        if (node.attribute("points")) {
            shape.inline_style["points"] = node.attribute("points").value();
        }
        
        if (!parent_id.empty()) {
            shape.parent_shape_id = parent_id;
        }
        
        doc.shape_layer().add_shape(shape);
    }
    
    void process_group(const pugi::xml_node& node, DDFDocument& doc, const std::string& parent_id) {
        Shape group_shape;
        group_shape.type = "group";
        extract_common_attributes(node, group_shape);
        
        if (!parent_id.empty()) {
            group_shape.parent_shape_id = parent_id;
        }
        
        std::string group_id = group_shape.id;
        doc.shape_layer().add_shape(group_shape);
        
        // Process children with this group as parent
        process_node(node, doc, group_id);
        
        // Update group's child list
        auto* group = doc.shape_layer().get_shape(group_id);
        if (group) {
            // Find all shapes with this group as parent
            for (const auto* shape : doc.shape_layer().get_all_shapes()) {
                if (shape->parent_shape_id == group_id && shape->id != group_id) {
                    group->child_shape_ids.push_back(shape->id);
                }
            }
        }
    }
};

// SVGImporter public methods

bool SVGImporter::import(const std::string& svg_string, DDFDocument& doc) {
    Impl impl;
    return impl.import(svg_string, doc, last_error_);
}

} // namespace ddf
} // namespace whiteboard
