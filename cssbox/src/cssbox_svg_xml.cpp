/*
 * NanoVG CSS - SVG XML Parser Implementation
 *
 * Parses SVG XML documents and creates cssbox elements.
 * Uses pugixml for XML parsing.
 *
 * Design Philosophy (Linus Style):
 * - Single attribute mapping table (no special cases per element)
 * - Recursive tree traversal (10-15 lines)
 * - Zero fallback logic (fail fast if XML is broken)
 * - Data structure first, code follows
 */

#include "cssbox_svg_xml.h"
#include "cssbox_internal.h"
#include <pugixml.hpp>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdio>

// ============================================================================
// SVG Attribute → CSS Property Mapping
// ============================================================================

struct SVGAttrMapping {
    const char* svg_attr;
    const char* css_prop;
    bool add_px_unit;  // SVG unitless numbers → CSS pixels
};

// Unified mapping table - no special cases
static const SVGAttrMapping ATTR_MAP[] = {
    // Position & dimensions
    {"x", "x", true},
    {"y", "y", true},
    {"cx", "cx", true},
    {"cy", "cy", true},
    {"r", "r", true},
    {"rx", "rx", true},
    {"ry", "ry", true},
    {"width", "width", true},
    {"height", "height", true},
    {"x1", "x1", true},
    {"y1", "y1", true},
    {"x2", "x2", true},
    {"y2", "y2", true},

    // SVG viewport
    {"viewBox", "viewBox", false},
    {"preserveAspectRatio", "preserveAspectRatio", false},

    // Fill & stroke
    {"fill", "fill", false},
    {"fill-opacity", "fill-opacity", false},
    {"stroke", "stroke", false},
    {"stroke-width", "stroke-width", true},
    {"stroke-opacity", "stroke-opacity", false},
    {"stroke-linecap", "stroke-linecap", false},
    {"stroke-linejoin", "stroke-linejoin", false},
    {"stroke-dasharray", "stroke-dasharray", false},
    {"stroke-dashoffset", "stroke-dashoffset", true},
    {"stroke-miterlimit", "stroke-miterlimit", false},

    // Path data
    {"d", "d", false},
    {"points", "points", false},

    // Transform
    {"transform", "transform", false},

    // Opacity
    {"opacity", "opacity", false},

    // Text
    {"font-size", "font-size", true},
    {"font-family", "font-family", false},
    {"font-weight", "font-weight", false},
    {"text-anchor", "text-anchor", false},

    // Markers
    {"marker-start", "marker-start", false},
    {"marker-mid", "marker-mid", false},
    {"marker-end", "marker-end", false},

    // Clipping
    {"clip-path", "clip-path", false},

    // Gradients (for defs)
    {"offset", "offset", false},
    {"stop-color", "stop-color", false},
    {"stop-opacity", "stop-opacity", false},

    // Marker attributes
    {"markerWidth", "markerWidth", true},
    {"markerHeight", "markerHeight", true},
    {"refX", "refX", true},
    {"refY", "refY", true},
    {"orient", "orient", false},

    // Gradient attributes
    {"gradientUnits", "gradientUnits", false},
    {"gradientTransform", "gradientTransform", false},

    // Sentinel
    {nullptr, nullptr, false}
};

// ============================================================================
// Attribute Mapping Helper
// ============================================================================

static void mapAttributes(cssboxElement* element, pugi::xml_node xml_node) {
    for (pugi::xml_attribute attr : xml_node.attributes()) {
        const char* attr_name = attr.name();
        const char* attr_value = attr.value();

        // Skip empty attributes
        if (!attr_value || attr_value[0] == '\0') continue;

        // Find in mapping table
        const SVGAttrMapping* mapping = nullptr;
        for (const SVGAttrMapping* m = ATTR_MAP; m->svg_attr != nullptr; ++m) {
            if (std::strcmp(attr_name, m->svg_attr) == 0) {
                mapping = m;
                break;
            }
        }

        if (mapping) {
            // Apply mapped property
            if (mapping->add_px_unit) {
                // SVG unitless → CSS pixels
                char buffer[64];
                std::snprintf(buffer, sizeof(buffer), "%spx", attr_value);
                element->inline_style[mapping->css_prop] = buffer;
            } else {
                element->inline_style[mapping->css_prop] = attr_value;
            }
        } else if (std::strcmp(attr_name, "id") == 0) {
            // ID handled separately during element creation
            continue;
        } else if (std::strcmp(attr_name, "class") == 0) {
            // CSS classes
            cssboxAddClass(element, attr_value);
        } else if (std::strcmp(attr_name, "style") == 0) {
            // Inline CSS style attribute (parse later if needed)
            element->inline_style["style"] = attr_value;
        }
    }
}

// ============================================================================
// SVG Element → CSS Element Type Mapping
// ============================================================================

static const char* mapElementType(const char* svg_tag) {
    // Direct 1:1 mapping for supported elements
    static const struct {
        const char* svg;
        const char* css;
    } TYPE_MAP[] = {
        {"svg", "svg"},
        {"g", "group"},
        {"rect", "rect"},
        {"circle", "circle"},
        {"ellipse", "ellipse"},
        {"line", "line"},
        {"path", "path"},
        {"polygon", "polygon"},
        {"polyline", "polyline"},
        {"text", "text"},
        {"defs", "defs"},
        {"linearGradient", "linearGradient"},
        {"radialGradient", "radialGradient"},
        {"stop", "stop"},
        {"marker", "marker"},
        {"clipPath", "clipPath"},
        {"pattern", "pattern"},
        {nullptr, nullptr}
    };

    for (int i = 0; TYPE_MAP[i].svg != nullptr; ++i) {
        if (std::strcmp(svg_tag, TYPE_MAP[i].svg) == 0) {
            return TYPE_MAP[i].css;
        }
    }

    // Unknown element → generic group
    return "group";
}

// ============================================================================
// Recursive XML Tree Parser
// ============================================================================

static int element_counter = 0;  // For auto-generated IDs

static cssboxElement* parseNode(
    cssboxRenderer* renderer,
    pugi::xml_node xml_node,
    cssboxElement* parent,
    const cssbox::SVGXMLParser::Options& options
) {
    // Get element type
    const char* tag_name = xml_node.name();
    const char* element_type = mapElementType(tag_name);

    // Generate ID
    std::string element_id;
    pugi::xml_attribute id_attr = xml_node.attribute("id");
    if (id_attr && options.preserve_ids) {
        element_id = options.id_prefix + id_attr.value();
    } else {
        char buffer[64];
        std::snprintf(buffer, sizeof(buffer), "%selem_%d",
                     options.id_prefix.c_str(), element_counter++);
        element_id = buffer;
    }

    // Create element
    cssboxElement* element = cssboxCreateElement(renderer, element_id.c_str(), element_type);
    if (!element) {
        // ID collision - skip this element
        return nullptr;
    }

    // Map attributes
    mapAttributes(element, xml_node);

    // Auto-add "svg-element" class to all SVG elements for CSS targeting
    if (std::strcmp(element_type, "svg") == 0 ||
        std::strcmp(element_type, "group") == 0 ||
        std::strcmp(element_type, "circle") == 0 ||
        std::strcmp(element_type, "rect") == 0 ||
        std::strcmp(element_type, "ellipse") == 0 ||
        std::strcmp(element_type, "line") == 0 ||
        std::strcmp(element_type, "path") == 0 ||
        std::strcmp(element_type, "polygon") == 0 ||
        std::strcmp(element_type, "polyline") == 0) {
        cssboxAddClass(element, "svg-element");
    }

    // Handle text content for <text> elements
    if (std::strcmp(tag_name, "text") == 0) {
        const char* text = xml_node.child_value();
        if (text && text[0] != '\0') {
            cssboxSetText(element, text);
        }
    }

    // Attach to parent
    if (parent) {
        cssboxAppendChild(renderer, parent, element);
    }

    // Handle gradient definitions (linearGradient, radialGradient)
    printf("[SVG_XML] parseNode: tag='%s' id='%s'\n", tag_name, id_attr ? id_attr.value() : "(no-id)");
    
    if (std::strcmp(tag_name, "linearGradient") == 0 || std::strcmp(tag_name, "radialGradient") == 0) {
        printf("[SVG_XML] Found gradient element! tag='%s'\n", tag_name);
        GradientData gradient;
        gradient.type = (std::strcmp(tag_name, "linearGradient") == 0) ? GradientData::LINEAR : GradientData::RADIAL;
        
        // Parse gradient stops
        for (pugi::xml_node stop_node : xml_node.children("stop")) {
            GradientStop stop;
            
            // Parse offset
            pugi::xml_attribute offset_attr = stop_node.attribute("offset");
            if (offset_attr) {
                std::string offset_str = offset_attr.value();
                if (offset_str.back() == '%') {
                    stop.position = std::strtof(offset_str.c_str(), nullptr) / 100.0f;
                } else {
                    stop.position = std::strtof(offset_str.c_str(), nullptr);
                }
            }
            
            // Parse stop-color
            pugi::xml_attribute color_attr = stop_node.attribute("stop-color");
            if (color_attr) {
                stop.color = cssbox_utils::parse_color(color_attr.value());
            }
            
            // Parse stop-opacity
            pugi::xml_attribute opacity_attr = stop_node.attribute("stop-opacity");
            if (opacity_attr) {
                stop.color.a = std::strtof(opacity_attr.value(), nullptr);
            }
            
            gradient.stops.push_back(stop);
        }
        
        // Store gradient in renderer registry
        if (id_attr) {
            std::string grad_id = id_attr.value();
            renderer->gradients_[grad_id] = gradient;
            printf("[SVG_XML] Stored gradient id='%s' type=%s stops=%zu\n", 
                   grad_id.c_str(), 
                   (gradient.type == GradientData::LINEAR ? "linear" : "radial"),
                   gradient.stops.size());
        }
    }
    
    // Recursively parse children
    for (pugi::xml_node child : xml_node.children()) {
        if (child.type() == pugi::node_element) {
            parseNode(renderer, child, element, options);
        }
    }

    return element;
}

// ============================================================================
// C API Implementation
// ============================================================================

cssboxElement* cssboxLoadSVG(
    cssboxRenderer* renderer,
    const char* svg_xml,
    cssboxElement* parent
) {
    if (!renderer || !svg_xml) return nullptr;

    // Parse XML
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(svg_xml);

    if (!result) {
        fprintf(stderr, "SVG XML parse error: %s at offset %td\n",
                result.description(), result.offset);
        return nullptr;
    }

    // Find <svg> root
    pugi::xml_node svg_root = doc.child("svg");
    if (!svg_root) {
        fprintf(stderr, "SVG XML error: No <svg> root element found\n");
        return nullptr;
    }

    // Parse tree
    cssbox::SVGXMLParser::Options options;
    element_counter = 0;  // Reset counter
    cssboxElement* root = parseNode(renderer, svg_root, parent, options);
    
    // Build gradient cache for all elements (performance optimization)
    if (root) {
        cssbox::SVGXMLParser::resolve_gradient_references(renderer, root);
    }
    
    return root;
}

cssboxElement* cssboxLoadSVGFile(
    cssboxRenderer* renderer,
    const char* filepath,
    cssboxElement* parent
) {
    if (!renderer || !filepath) return nullptr;

    // Read file
    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        fprintf(stderr, "SVG file error: Cannot open '%s'\n", filepath);
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    return cssboxLoadSVG(renderer, content.c_str(), parent);
}

char* cssboxExtractSVGStyles(const char* svg_xml) {
    if (!svg_xml) return nullptr;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(svg_xml);
    if (!result) return nullptr;

    // Find all <style> elements using XPath
    std::stringstream css_output;
    pugi::xpath_node_set style_nodes = doc.select_nodes("//style");
    for (pugi::xpath_node xpath_node : style_nodes) {
        pugi::xml_node style_node = xpath_node.node();
        css_output << style_node.child_value() << "\n";
    }

    std::string css_str = css_output.str();
    if (css_str.empty()) return nullptr;

    // Use strdup (caller must free with free())
    return strdup(css_str.c_str());
}

// ============================================================================
// C++ API Implementation
// ============================================================================

namespace cssbox {

cssboxElement* SVGXMLParser::parse(
    cssboxRenderer* renderer,
    const std::string& svg_xml,
    const Options& options
) {
    if (!renderer || svg_xml.empty()) return nullptr;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(svg_xml.c_str());

    if (!result) {
        fprintf(stderr, "SVG XML parse error: %s at offset %td\n",
                result.description(), result.offset);
        return nullptr;
    }

    pugi::xml_node svg_root = doc.child("svg");
    if (!svg_root) {
        fprintf(stderr, "SVG XML error: No <svg> root element found\n");
        return nullptr;
    }

    element_counter = 0;
    return parseNode(renderer, svg_root, nullptr, options);
}

cssboxElement* SVGXMLParser::parseFile(
    cssboxRenderer* renderer,
    const std::string& filepath,
    const Options& options
) {
    if (!renderer || filepath.empty()) return nullptr;

    std::ifstream file(filepath, std::ios::binary);
    if (!file) {
        fprintf(stderr, "SVG file error: Cannot open '%s'\n", filepath.c_str());
        return nullptr;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    return parse(renderer, buffer.str(), options);
}

// ============================================================================
// Gradient Cache Resolution (Performance Optimization)
// ============================================================================

/**
 * @brief Recursively resolve gradient references and cache pointers
 * 
 * Called once after SVG parsing to build gradient cache.
 * Eliminates O(log n) map lookups every frame.
 */
void SVGXMLParser::resolve_gradient_references(cssboxRenderer* renderer, cssboxElement* element) {
    if (!renderer || !element) return;
    
    // Check fill property for gradient reference
    auto fill_it = element->inline_style.find("fill");
    if (fill_it != element->inline_style.end() && 
        fill_it->second.find("url(#") != std::string::npos) {
        
        // Extract gradient ID from url(#id)
        size_t start_pos = fill_it->second.find("#");
        size_t end_pos = fill_it->second.find(")");
        if (start_pos != std::string::npos && end_pos != std::string::npos && start_pos < end_pos) {
            std::string gradient_id = fill_it->second.substr(start_pos + 1, end_pos - start_pos - 1);
            
            // Look up gradient and cache pointer
            auto grad_it = renderer->gradients_.find(gradient_id);
            if (grad_it != renderer->gradients_.end()) {
                element->cached_fill_gradient = &grad_it->second;
                printf("[GRADIENT_CACHE] Cached gradient '%s' for element id='%s' type='%s'\n",
                       gradient_id.c_str(), element->id.c_str(), element->type.c_str());
            } else {
                printf("[GRADIENT_CACHE] WARNING: Gradient '%s' not found for element id='%s'\n",
                       gradient_id.c_str(), element->id.c_str());
            }
        }
    }
    
    // Check stroke property for gradient reference
    auto stroke_it = element->inline_style.find("stroke");
    if (stroke_it != element->inline_style.end() && 
        stroke_it->second.find("url(#") != std::string::npos) {
        
        // Extract gradient ID from url(#id)
        size_t start_pos = stroke_it->second.find("#");
        size_t end_pos = stroke_it->second.find(")");
        if (start_pos != std::string::npos && end_pos != std::string::npos && start_pos < end_pos) {
            std::string gradient_id = stroke_it->second.substr(start_pos + 1, end_pos - start_pos - 1);
            
            // Look up gradient and cache pointer
            auto grad_it = renderer->gradients_.find(gradient_id);
            if (grad_it != renderer->gradients_.end()) {
                element->cached_stroke_gradient = &grad_it->second;
            }
        }
    }
    
    // Recursively process children
    for (int child_id : element->children_internal_ids) {
        auto it = renderer->elements.find(child_id);
        if (it != renderer->elements.end()) {
            resolve_gradient_references(renderer, it->second.get());
        }
    }
}

} // namespace cssbox
