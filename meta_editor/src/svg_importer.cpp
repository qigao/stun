/*
 * Meta Editor - SVG Importer Implementation
 *
 * Simple SVG parser - handles common elements, ignores advanced features.
 * Philosophy: Parse what we can edit, skip what we can't.
 */

#include "meta_editor/svg_importer.h"
#include "meta_editor/canvas.h"
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <cctype>

namespace meta_editor {

SvgImporter::SvgImporter(Canvas* canvas) : canvas_(canvas) {}

bool SvgImporter::import_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        error_ = "Cannot open file: " + path;
        return false;
    }

    std::stringstream ss;
    ss << file.rdbuf();
    return import_string(ss.str());
}

bool SvgImporter::import_string(const std::string& svg_content) {
    imported_nodes_.clear();
    error_.clear();

    const char* p = svg_content.c_str();

    // Skip to <svg> tag
    while (*p && !match(p, "<svg")) {
        ++p;
    }
    if (!*p) {
        error_ = "No <svg> tag found";
        return false;
    }

    // Skip svg attributes
    skip_to_tag_end(p);

    // Parse children
    ParseContext ctx;
    while (*p) {
        skip_whitespace(p);
        if (!*p) break;

        if (match(p, "</svg>")) break;

        if (*p == '<') {
            if (*(p + 1) == '/') break;  // Closing tag
            if (*(p + 1) == '!' || *(p + 1) == '?') {
                skip_to_tag_end(p);
                continue;
            }

            auto* node = parse_element(p, ctx);
            if (node) {
                canvas_->content_root()->add_child(node);
                imported_nodes_.push_back(node);
            }
        } else {
            ++p;
        }
    }

    return !imported_nodes_.empty();
}

flex::Node* SvgImporter::parse_element(const char*& p, const ParseContext& ctx) {
    skip_whitespace(p);
    if (*p != '<') return nullptr;
    ++p;

    if (match(p, "rect")) return parse_rect(p, ctx);
    if (match(p, "circle")) return parse_circle(p, ctx);
    if (match(p, "ellipse")) return parse_ellipse(p, ctx);
    if (match(p, "line")) return parse_line(p, ctx);
    if (match(p, "polyline")) return parse_polyline(p, ctx);
    if (match(p, "polygon")) return parse_polygon(p, ctx);
    if (match(p, "path")) return parse_path(p, ctx);
    if (match(p, "g")) return parse_group(p, ctx);

    // Unknown element - skip it
    skip_to_tag_end(p);
    return nullptr;
}

flex::Shape* SvgImporter::parse_rect(const char*& p, const ParseContext& parent_ctx) {
    ParseContext ctx = parent_ctx;
    float x = 0, y = 0, width = 0, height = 0, rx = 0;

    std::string name, value;
    while (parse_attribute(p, name, value)) {
        if (name == "x") x = std::stof(value);
        else if (name == "y") y = std::stof(value);
        else if (name == "width") width = std::stof(value);
        else if (name == "height") height = std::stof(value);
        else if (name == "rx" || name == "ry") rx = std::stof(value);
        else if (name == "fill") {
            if (value == "none") ctx.has_fill = false;
            else ctx.fill = parse_color(value);
        }
        else if (name == "stroke") {
            if (value == "none") ctx.has_stroke = false;
            else ctx.stroke = parse_color(value);
        }
        else if (name == "stroke-width") ctx.stroke_width = std::stof(value);
        else if (name == "style") apply_style(value, ctx);
        else if (name == "opacity") ctx.opacity = std::stof(value);
    }

    skip_to_tag_end(p);

    auto* allocator = canvas_->instance()->object_allocator();
    auto* shape = flex::Shape::create(*allocator);
    shape->set_rect(width, height, rx);
    shape->set_position(x, y);
    shape->set_opacity(ctx.opacity);

    if (ctx.has_fill) shape->set_fill(ctx.fill);
    if (ctx.has_stroke) shape->set_stroke(ctx.stroke, ctx.stroke_width);

    return shape;
}

flex::Shape* SvgImporter::parse_circle(const char*& p, const ParseContext& parent_ctx) {
    ParseContext ctx = parent_ctx;
    float cx = 0, cy = 0, r = 0;

    std::string name, value;
    while (parse_attribute(p, name, value)) {
        if (name == "cx") cx = std::stof(value);
        else if (name == "cy") cy = std::stof(value);
        else if (name == "r") r = std::stof(value);
        else if (name == "fill") {
            if (value == "none") ctx.has_fill = false;
            else ctx.fill = parse_color(value);
        }
        else if (name == "stroke") {
            if (value == "none") ctx.has_stroke = false;
            else ctx.stroke = parse_color(value);
        }
        else if (name == "stroke-width") ctx.stroke_width = std::stof(value);
        else if (name == "style") apply_style(value, ctx);
    }

    skip_to_tag_end(p);

    auto* allocator = canvas_->instance()->object_allocator();
    auto* shape = flex::Shape::create(*allocator);
    shape->set_circle(r);
    shape->set_position(cx, cy);

    if (ctx.has_fill) shape->set_fill(ctx.fill);
    if (ctx.has_stroke) shape->set_stroke(ctx.stroke, ctx.stroke_width);

    return shape;
}

flex::Shape* SvgImporter::parse_ellipse(const char*& p, const ParseContext& parent_ctx) {
    ParseContext ctx = parent_ctx;
    float cx = 0, cy = 0, rx = 0, ry = 0;

    std::string name, value;
    while (parse_attribute(p, name, value)) {
        if (name == "cx") cx = std::stof(value);
        else if (name == "cy") cy = std::stof(value);
        else if (name == "rx") rx = std::stof(value);
        else if (name == "ry") ry = std::stof(value);
        else if (name == "fill") {
            if (value == "none") ctx.has_fill = false;
            else ctx.fill = parse_color(value);
        }
        else if (name == "stroke") {
            if (value == "none") ctx.has_stroke = false;
            else ctx.stroke = parse_color(value);
        }
        else if (name == "stroke-width") ctx.stroke_width = std::stof(value);
        else if (name == "style") apply_style(value, ctx);
    }

    skip_to_tag_end(p);

    auto* allocator = canvas_->instance()->object_allocator();
    auto* shape = flex::Shape::create(*allocator);
    shape->set_ellipse(rx, ry);
    shape->set_position(cx, cy);

    if (ctx.has_fill) shape->set_fill(ctx.fill);
    if (ctx.has_stroke) shape->set_stroke(ctx.stroke, ctx.stroke_width);

    return shape;
}

flex::Shape* SvgImporter::parse_line(const char*& p, const ParseContext& parent_ctx) {
    ParseContext ctx = parent_ctx;
    ctx.has_fill = false;  // Lines don't have fill
    float x1 = 0, y1 = 0, x2 = 0, y2 = 0;

    std::string name, value;
    while (parse_attribute(p, name, value)) {
        if (name == "x1") x1 = std::stof(value);
        else if (name == "y1") y1 = std::stof(value);
        else if (name == "x2") x2 = std::stof(value);
        else if (name == "y2") y2 = std::stof(value);
        else if (name == "stroke") {
            if (value == "none") ctx.has_stroke = false;
            else ctx.stroke = parse_color(value);
        }
        else if (name == "stroke-width") ctx.stroke_width = std::stof(value);
        else if (name == "style") apply_style(value, ctx);
    }

    skip_to_tag_end(p);

    auto* allocator = canvas_->instance()->object_allocator();
    auto* shape = flex::Shape::create(*allocator);
    shape->set_line(x2 - x1, y2 - y1);
    shape->set_position(x1, y1);

    if (ctx.has_stroke) shape->set_stroke(ctx.stroke, ctx.stroke_width);

    return shape;
}

flex::Shape* SvgImporter::parse_polyline(const char*& p, const ParseContext& parent_ctx) {
    ParseContext ctx = parent_ctx;
    ctx.has_fill = false;
    std::string points_str;

    std::string name, value;
    while (parse_attribute(p, name, value)) {
        if (name == "points") points_str = value;
        else if (name == "stroke") {
            if (value == "none") ctx.has_stroke = false;
            else ctx.stroke = parse_color(value);
        }
        else if (name == "stroke-width") ctx.stroke_width = std::stof(value);
        else if (name == "style") apply_style(value, ctx);
    }

    skip_to_tag_end(p);

    auto pts = parse_points(points_str);
    if (pts.size() < 4) return nullptr;

    // Convert to path
    std::stringstream path_d;
    path_d << "M " << pts[0] << " " << pts[1];
    for (size_t i = 2; i < pts.size(); i += 2) {
        path_d << " L " << pts[i] << " " << pts[i + 1];
    }

    auto* allocator = canvas_->instance()->object_allocator();
    auto* shape = flex::Shape::create(*allocator);
    shape->set_path(path_d.str());

    if (ctx.has_stroke) shape->set_stroke(ctx.stroke, ctx.stroke_width);

    return shape;
}

flex::Shape* SvgImporter::parse_polygon(const char*& p, const ParseContext& parent_ctx) {
    ParseContext ctx = parent_ctx;
    std::string points_str;

    std::string name, value;
    while (parse_attribute(p, name, value)) {
        if (name == "points") points_str = value;
        else if (name == "fill") {
            if (value == "none") ctx.has_fill = false;
            else ctx.fill = parse_color(value);
        }
        else if (name == "stroke") {
            if (value == "none") ctx.has_stroke = false;
            else ctx.stroke = parse_color(value);
        }
        else if (name == "stroke-width") ctx.stroke_width = std::stof(value);
        else if (name == "style") apply_style(value, ctx);
    }

    skip_to_tag_end(p);

    auto pts = parse_points(points_str);
    if (pts.size() < 6) return nullptr;

    // Convert to closed path
    std::stringstream path_d;
    path_d << "M " << pts[0] << " " << pts[1];
    for (size_t i = 2; i < pts.size(); i += 2) {
        path_d << " L " << pts[i] << " " << pts[i + 1];
    }
    path_d << " Z";

    auto* allocator = canvas_->instance()->object_allocator();
    auto* shape = flex::Shape::create(*allocator);
    shape->set_path(path_d.str());

    if (ctx.has_fill) shape->set_fill(ctx.fill);
    if (ctx.has_stroke) shape->set_stroke(ctx.stroke, ctx.stroke_width);

    return shape;
}

flex::Shape* SvgImporter::parse_path(const char*& p, const ParseContext& parent_ctx) {
    ParseContext ctx = parent_ctx;
    std::string d;

    std::string name, value;
    while (parse_attribute(p, name, value)) {
        if (name == "d") d = value;
        else if (name == "fill") {
            if (value == "none") ctx.has_fill = false;
            else ctx.fill = parse_color(value);
        }
        else if (name == "stroke") {
            if (value == "none") ctx.has_stroke = false;
            else ctx.stroke = parse_color(value);
        }
        else if (name == "stroke-width") ctx.stroke_width = std::stof(value);
        else if (name == "style") apply_style(value, ctx);
    }

    skip_to_tag_end(p);

    if (d.empty()) return nullptr;

    auto* allocator = canvas_->instance()->object_allocator();
    auto* shape = flex::Shape::create(*allocator);
    shape->set_path(d);

    if (ctx.has_fill) shape->set_fill(ctx.fill);
    if (ctx.has_stroke) shape->set_stroke(ctx.stroke, ctx.stroke_width);

    return shape;
}

flex::Group* SvgImporter::parse_group(const char*& p, const ParseContext& parent_ctx) {
    ParseContext ctx = parent_ctx;
    std::string id;
    float tx = 0, ty = 0;

    std::string name, value;
    while (parse_attribute(p, name, value)) {
        if (name == "id") id = value;
        else if (name == "transform") {
            // Simple translate() parsing
            size_t pos = value.find("translate(");
            if (pos != std::string::npos) {
                const char* tp = value.c_str() + pos + 10;
                tx = std::strtof(tp, const_cast<char**>(&tp));
                while (*tp && (*tp == ',' || *tp == ' ')) ++tp;
                ty = std::strtof(tp, nullptr);
            }
        }
        else if (name == "fill") {
            if (value == "none") ctx.has_fill = false;
            else ctx.fill = parse_color(value);
        }
        else if (name == "stroke") {
            if (value == "none") ctx.has_stroke = false;
            else ctx.stroke = parse_color(value);
        }
        else if (name == "stroke-width") ctx.stroke_width = std::stof(value);
        else if (name == "opacity") ctx.opacity = std::stof(value);
    }

    // Check for self-closing tag
    skip_whitespace(p);
    bool self_closing = false;
    if (*p == '/') {
        self_closing = true;
        ++p;
    }
    if (*p == '>') ++p;

    auto* allocator = canvas_->instance()->object_allocator();
    auto* group = flex::Group::create(*allocator);
    if (!id.empty()) group->set_id(id);
    group->set_position(tx, ty);
    group->set_opacity(ctx.opacity);

    if (self_closing) return group;

    // Parse children
    while (*p) {
        skip_whitespace(p);
        if (match(p, "</g>")) break;

        if (*p == '<') {
            if (*(p + 1) == '/') break;
            auto* child = parse_element(p, ctx);
            if (child) group->add_child(child);
        } else {
            ++p;
        }
    }

    return group;
}

bool SvgImporter::parse_attribute(const char*& p, std::string& name, std::string& value) {
    skip_whitespace(p);

    if (*p == '/' || *p == '>' || !*p) return false;

    // Read name
    name.clear();
    while (*p && *p != '=' && !std::isspace(*p) && *p != '/' && *p != '>') {
        name += *p++;
    }

    if (name.empty()) return false;

    skip_whitespace(p);
    if (*p != '=') {
        value.clear();
        return true;
    }
    ++p;  // skip '='

    skip_whitespace(p);
    char quote = *p;
    if (quote != '"' && quote != '\'') return false;
    ++p;

    value.clear();
    while (*p && *p != quote) {
        value += *p++;
    }
    if (*p == quote) ++p;

    return true;
}

void SvgImporter::apply_style(const std::string& style, ParseContext& ctx) {
    // Parse CSS-like style: "fill:#ff0000;stroke:none;stroke-width:2"
    std::istringstream ss(style);
    std::string item;

    while (std::getline(ss, item, ';')) {
        size_t colon = item.find(':');
        if (colon == std::string::npos) continue;

        std::string prop = item.substr(0, colon);
        std::string val = item.substr(colon + 1);

        // Trim
        while (!prop.empty() && std::isspace(prop.front())) prop.erase(0, 1);
        while (!prop.empty() && std::isspace(prop.back())) prop.pop_back();
        while (!val.empty() && std::isspace(val.front())) val.erase(0, 1);
        while (!val.empty() && std::isspace(val.back())) val.pop_back();

        if (prop == "fill") {
            if (val == "none") ctx.has_fill = false;
            else ctx.fill = parse_color(val);
        }
        else if (prop == "stroke") {
            if (val == "none") ctx.has_stroke = false;
            else ctx.stroke = parse_color(val);
        }
        else if (prop == "stroke-width") {
            ctx.stroke_width = std::stof(val);
        }
        else if (prop == "opacity") {
            ctx.opacity = std::stof(val);
        }
    }
}

flex::Color SvgImporter::parse_color(const std::string& str) {
    if (str.empty()) return {0, 0, 0, 1};

    // Named colors (common ones)
    if (str == "black") return {0, 0, 0, 1};
    if (str == "white") return {1, 1, 1, 1};
    if (str == "red") return {1, 0, 0, 1};
    if (str == "green") return {0, 0.5f, 0, 1};
    if (str == "blue") return {0, 0, 1, 1};
    if (str == "yellow") return {1, 1, 0, 1};
    if (str == "cyan") return {0, 1, 1, 1};
    if (str == "magenta") return {1, 0, 1, 1};
    if (str == "gray" || str == "grey") return {0.5f, 0.5f, 0.5f, 1};
    if (str == "orange") return {1, 0.65f, 0, 1};
    if (str == "purple") return {0.5f, 0, 0.5f, 1};
    if (str == "pink") return {1, 0.75f, 0.8f, 1};

    // Hex color
    if (str[0] == '#') {
        unsigned int hex = 0;
        if (str.length() == 4) {
            // #RGB -> #RRGGBB
            std::sscanf(str.c_str(), "#%1x%1x%1x", &hex, &hex, &hex);
            int r = (hex >> 8) & 0xF;
            int g = (hex >> 4) & 0xF;
            int b = hex & 0xF;
            return {(r * 17) / 255.0f, (g * 17) / 255.0f, (b * 17) / 255.0f, 1};
        }
        std::sscanf(str.c_str(), "#%x", &hex);
        return {
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >> 8) & 0xFF) / 255.0f,
            (hex & 0xFF) / 255.0f,
            1.0f
        };
    }

    // rgb(r, g, b)
    if (str.substr(0, 4) == "rgb(") {
        int r, g, b;
        std::sscanf(str.c_str(), "rgb(%d,%d,%d)", &r, &g, &b);
        return {r / 255.0f, g / 255.0f, b / 255.0f, 1.0f};
    }

    // rgba(r, g, b, a)
    if (str.substr(0, 5) == "rgba(") {
        int r, g, b;
        float a;
        std::sscanf(str.c_str(), "rgba(%d,%d,%d,%f)", &r, &g, &b, &a);
        return {r / 255.0f, g / 255.0f, b / 255.0f, a};
    }

    return {0, 0, 0, 1};
}

std::vector<float> SvgImporter::parse_points(const std::string& str) {
    std::vector<float> result;
    const char* p = str.c_str();

    while (*p) {
        while (*p && (*p == ' ' || *p == ',' || *p == '\t' || *p == '\n')) ++p;
        if (!*p) break;

        char* end;
        float val = std::strtof(p, &end);
        if (end == p) break;
        result.push_back(val);
        p = end;
    }

    return result;
}

void SvgImporter::skip_whitespace(const char*& p) {
    while (*p && std::isspace(*p)) ++p;
}

void SvgImporter::skip_to_tag_end(const char*& p) {
    while (*p && *p != '>') ++p;
    if (*p == '>') ++p;
}

bool SvgImporter::match(const char*& p, const char* str) {
    const char* start = p;
    while (*str && *p == *str) {
        ++p;
        ++str;
    }
    if (*str == '\0') return true;
    p = start;
    return false;
}

std::string SvgImporter::read_until(const char*& p, char delim) {
    std::string result;
    while (*p && *p != delim) {
        result += *p++;
    }
    return result;
}

} // namespace meta_editor
