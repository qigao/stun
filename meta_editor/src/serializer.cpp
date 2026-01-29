/*
 * Meta Editor - Project Serialization Implementation
 */

#include "meta_editor/serializer.h"
#include "meta_editor/canvas.h"
#include <flex/runtime/shape.h>
#include <flex/runtime/text.h>
#include <flex/runtime/group.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace meta_editor {

Serializer::Serializer(Canvas* canvas) : canvas_(canvas) {}

bool Serializer::save(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << to_json();
    return true;
}

bool Serializer::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;
    std::stringstream buffer;
    buffer << file.rdbuf();
    return from_json(buffer.str());
}

std::string Serializer::to_json() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"version\": 1,\n";
    ss << "  \"width\": " << canvas_->width() << ",\n";
    ss << "  \"height\": " << canvas_->height() << ",\n";
    ss << "  \"layers\": [\n";

    auto layers = canvas_->get_all_layers();
    for (size_t i = 0; i < layers.size(); ++i) {
        ss << group_to_json(layers[i], 4);
        if (i < layers.size() - 1) ss << ",";
        ss << "\n";
    }

    ss << "  ]\n";
    ss << "}\n";
    return ss.str();
}

std::string Serializer::node_to_json(flex::Node* node, int indent) const {
    if (!node) return "null";

    if (node->type() == flex::NodeType::Shape) {
        return shape_to_json(static_cast<flex::Shape*>(node), indent);
    } else if (node->type() == flex::NodeType::Text) {
        return text_to_json(static_cast<flex::Text*>(node), indent);
    } else if (node->is_group()) {
        return group_to_json(static_cast<flex::Group*>(node), indent);
    }
    return "null";
}

std::string Serializer::shape_to_json(flex::Shape* shape, int indent) const {
    std::stringstream ss;
    std::string ind = indent_str(indent);
    std::string ind2 = indent_str(indent + 2);

    ss << ind << "{\n";
    ss << ind2 << "\"type\": \"shape\",\n";
    ss << ind2 << "\"x\": " << shape->x() << ",\n";
    ss << ind2 << "\"y\": " << shape->y() << ",\n";
    ss << ind2 << "\"visible\": " << (shape->visible() ? "true" : "false") << ",\n";

    // Geometry
    ss << ind2 << "\"geometry\": {\n";
    std::string ind3 = indent_str(indent + 4);

    switch (shape->geometry_type()) {
        case flex::GeometryType::Rect: {
            auto r = shape->rect();
            ss << ind3 << "\"type\": \"rect\",\n";
            ss << ind3 << "\"width\": " << r.width << ",\n";
            ss << ind3 << "\"height\": " << r.height << ",\n";
            ss << ind3 << "\"cornerRadius\": " << r.corner_radius << "\n";
            break;
        }
        case flex::GeometryType::Circle: {
            auto c = shape->circle();
            ss << ind3 << "\"type\": \"circle\",\n";
            ss << ind3 << "\"radius\": " << c.radius << "\n";
            break;
        }
        case flex::GeometryType::Ellipse: {
            auto e = shape->ellipse();
            ss << ind3 << "\"type\": \"ellipse\",\n";
            ss << ind3 << "\"rx\": " << e.rx << ",\n";
            ss << ind3 << "\"ry\": " << e.ry << "\n";
            break;
        }
        case flex::GeometryType::Polygon: {
            auto p = shape->polygon();
            ss << ind3 << "\"type\": \"polygon\",\n";
            ss << ind3 << "\"sides\": " << p.sides << ",\n";
            ss << ind3 << "\"radius\": " << p.radius << "\n";
            break;
        }
        case flex::GeometryType::Star: {
            auto s = shape->star();
            ss << ind3 << "\"type\": \"star\",\n";
            ss << ind3 << "\"points\": " << s.points << ",\n";
            ss << ind3 << "\"outerRadius\": " << s.outer_radius << ",\n";
            ss << ind3 << "\"innerRadius\": " << s.inner_radius << "\n";
            break;
        }
        case flex::GeometryType::Path: {
            auto p = shape->path();
            ss << ind3 << "\"type\": \"path\",\n";
            ss << ind3 << "\"d\": \"" << p.d << "\"\n";
            break;
        }
        case flex::GeometryType::Line: {
            auto l = shape->line();
            ss << ind3 << "\"type\": \"line\",\n";
            ss << ind3 << "\"x2\": " << l.x2 << ",\n";
            ss << ind3 << "\"y2\": " << l.y2 << "\n";
            break;
        }
        default:
            ss << ind3 << "\"type\": \"unknown\"\n";
            break;
    }
    ss << ind2 << "},\n";

    // Fill
    if (shape->has_fill()) {
        ss << ind2 << "\"fill\": " << color_to_json(shape->fill().color) << ",\n";
    } else {
        ss << ind2 << "\"fill\": null,\n";
    }

    // Stroke
    if (shape->has_stroke()) {
        ss << ind2 << "\"stroke\": {\n";
        ss << ind3 << "\"color\": " << color_to_json(shape->stroke().color) << ",\n";
        ss << ind3 << "\"width\": " << shape->stroke().width << "\n";
        ss << ind2 << "}\n";
    } else {
        ss << ind2 << "\"stroke\": null\n";
    }

    ss << ind << "}";
    return ss.str();
}

std::string Serializer::text_to_json(flex::Text* text, int indent) const {
    std::stringstream ss;
    std::string ind = indent_str(indent);
    std::string ind2 = indent_str(indent + 2);

    ss << ind << "{\n";
    ss << ind2 << "\"type\": \"text\",\n";
    ss << ind2 << "\"x\": " << text->x() << ",\n";
    ss << ind2 << "\"y\": " << text->y() << ",\n";
    ss << ind2 << "\"visible\": " << (text->visible() ? "true" : "false") << ",\n";
    ss << ind2 << "\"content\": \"" << text->content() << "\",\n";
    ss << ind2 << "\"fontFamily\": \"" << text->font_family() << "\",\n";
    ss << ind2 << "\"fontSize\": " << text->font_size() << ",\n";
    ss << ind2 << "\"color\": " << color_to_json(text->color()) << "\n";
    ss << ind << "}";
    return ss.str();
}

std::string Serializer::group_to_json(flex::Group* group, int indent) const {
    std::stringstream ss;
    std::string ind = indent_str(indent);
    std::string ind2 = indent_str(indent + 2);

    ss << ind << "{\n";
    ss << ind2 << "\"type\": \"group\",\n";
    ss << ind2 << "\"id\": \"" << group->id() << "\",\n";
    ss << ind2 << "\"x\": " << group->x() << ",\n";
    ss << ind2 << "\"y\": " << group->y() << ",\n";
    ss << ind2 << "\"visible\": " << (group->visible() ? "true" : "false") << ",\n";
    ss << ind2 << "\"children\": [\n";

    const auto& children = group->children();
    for (size_t i = 0; i < children.size(); ++i) {
        ss << node_to_json(children[i], indent + 4);
        if (i < children.size() - 1) ss << ",";
        ss << "\n";
    }

    ss << ind2 << "]\n";
    ss << ind << "}";
    return ss.str();
}

std::string Serializer::color_to_json(const flex::Color& color) const {
    std::stringstream ss;
    ss << "{\"r\": " << color.r << ", \"g\": " << color.g
       << ", \"b\": " << color.b << ", \"a\": " << color.a << "}";
    return ss.str();
}

std::string Serializer::indent_str(int level) const {
    return std::string(level, ' ');
}

// Simple JSON parsing
void Serializer::skip_whitespace(const std::string& json, size_t& pos) const {
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\n' || json[pos] == '\r' || json[pos] == '\t')) {
        pos++;
    }
}

bool Serializer::expect_char(const std::string& json, size_t& pos, char c) const {
    skip_whitespace(json, pos);
    if (pos < json.size() && json[pos] == c) {
        pos++;
        return true;
    }
    return false;
}

std::string Serializer::parse_string(const std::string& json, size_t& pos) const {
    skip_whitespace(json, pos);
    if (pos >= json.size() || json[pos] != '"') return "";
    pos++;  // Skip opening quote

    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            pos++;
            if (json[pos] == 'n') result += '\n';
            else if (json[pos] == 't') result += '\t';
            else result += json[pos];
        } else {
            result += json[pos];
        }
        pos++;
    }
    if (pos < json.size()) pos++;  // Skip closing quote
    return result;
}

double Serializer::parse_number(const std::string& json, size_t& pos) const {
    skip_whitespace(json, pos);
    size_t start = pos;
    if (pos < json.size() && (json[pos] == '-' || json[pos] == '+')) pos++;
    while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '.')) pos++;
    if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
        pos++;
        if (pos < json.size() && (json[pos] == '-' || json[pos] == '+')) pos++;
        while (pos < json.size() && isdigit(json[pos])) pos++;
    }
    return std::stod(json.substr(start, pos - start));
}

bool Serializer::parse_bool(const std::string& json, size_t& pos) const {
    skip_whitespace(json, pos);
    if (json.substr(pos, 4) == "true") { pos += 4; return true; }
    if (json.substr(pos, 5) == "false") { pos += 5; return false; }
    return false;
}

std::string Serializer::parse_key(const std::string& json, size_t& pos) const {
    std::string key = parse_string(json, pos);
    skip_whitespace(json, pos);
    if (pos < json.size() && json[pos] == ':') pos++;
    return key;
}

bool Serializer::from_json(const std::string& json) {
    size_t pos = 0;
    skip_whitespace(json, pos);

    if (!expect_char(json, pos, '{')) return false;

    // Clear existing content
    auto layers = canvas_->get_all_layers();
    for (auto* layer : layers) {
        // Clear children
        while (!layer->children().empty()) {
            layer->remove_child(layer->children()[0]);
        }
    }

    // Parse root object
    while (pos < json.size() && json[pos] != '}') {
        std::string key = parse_key(json, pos);
        skip_whitespace(json, pos);

        if (key == "version") {
            parse_number(json, pos);
        } else if (key == "width") {
            // canvas_->set_width(parse_number(json, pos));  // If available
            parse_number(json, pos);
        } else if (key == "height") {
            // canvas_->set_height(parse_number(json, pos));
            parse_number(json, pos);
        } else if (key == "layers") {
            if (!expect_char(json, pos, '[')) return false;

            size_t layer_idx = 0;
            while (pos < json.size() && json[pos] != ']') {
                skip_whitespace(json, pos);
                if (json[pos] == ',') { pos++; continue; }

                flex::Group* target_layer = nullptr;
                if (layer_idx < layers.size()) {
                    target_layer = layers[layer_idx];
                } else {
                    target_layer = canvas_->create_layer("Layer " + std::to_string(layer_idx + 1));
                }

                // Parse layer as group
                flex::Group* loaded = json_to_group(json, pos);
                if (loaded && target_layer) {
                    // Copy children from loaded group to target layer
                    for (auto* child : loaded->children()) {
                        target_layer->add_child(child);
                    }
                }
                layer_idx++;
            }
            expect_char(json, pos, ']');
        }

        skip_whitespace(json, pos);
        if (pos < json.size() && json[pos] == ',') pos++;
    }

    return true;
}

flex::Node* Serializer::json_to_node(const std::string& json, size_t& pos) {
    skip_whitespace(json, pos);
    if (json.substr(pos, 4) == "null") { pos += 4; return nullptr; }

    // Peek at type
    size_t save_pos = pos;
    if (!expect_char(json, pos, '{')) return nullptr;

    while (pos < json.size() && json[pos] != '}') {
        std::string key = parse_key(json, pos);
        skip_whitespace(json, pos);

        if (key == "type") {
            std::string type = parse_string(json, pos);
            pos = save_pos;  // Reset to start of object

            if (type == "shape") return json_to_shape(json, pos);
            if (type == "text") return json_to_text(json, pos);
            if (type == "group") return json_to_group(json, pos);
            return nullptr;
        }

        // Skip value
        // Simple skip - won't handle nested objects perfectly
        int depth = 0;
        while (pos < json.size()) {
            if (json[pos] == '{' || json[pos] == '[') depth++;
            else if (json[pos] == '}' || json[pos] == ']') {
                if (depth == 0) break;
                depth--;
            } else if (json[pos] == ',' && depth == 0) break;
            else if (json[pos] == '"') { parse_string(json, pos); continue; }
            pos++;
        }
        if (pos < json.size() && json[pos] == ',') pos++;
    }

    return nullptr;
}

flex::Shape* Serializer::json_to_shape(const std::string& json, size_t& pos) {
    if (!expect_char(json, pos, '{')) return nullptr;

    auto* allocator = canvas_->instance()->object_allocator();
    auto* shape = flex::Shape::create(*allocator);

    float x = 0, y = 0;
    std::string geom_type;
    float width = 0, height = 0, radius = 0, rx = 0, ry = 0;
    float corner_radius = 0, outer_radius = 0, inner_radius = 0;
    int sides = 0, points = 0;
    float x2 = 0, y2 = 0;
    std::string path_d;
    flex::Color fill_color, stroke_color;
    float stroke_width = 1;
    bool has_fill = false, has_stroke = false;

    while (pos < json.size() && json[pos] != '}') {
        std::string key = parse_key(json, pos);
        skip_whitespace(json, pos);

        if (key == "type") {
            parse_string(json, pos);  // Already know it's "shape"
        } else if (key == "x") {
            x = static_cast<float>(parse_number(json, pos));
        } else if (key == "y") {
            y = static_cast<float>(parse_number(json, pos));
        } else if (key == "visible") {
            shape->set_visible(parse_bool(json, pos));
        } else if (key == "geometry") {
            expect_char(json, pos, '{');
            while (pos < json.size() && json[pos] != '}') {
                std::string gkey = parse_key(json, pos);
                skip_whitespace(json, pos);
                if (gkey == "type") geom_type = parse_string(json, pos);
                else if (gkey == "width") width = static_cast<float>(parse_number(json, pos));
                else if (gkey == "height") height = static_cast<float>(parse_number(json, pos));
                else if (gkey == "radius") radius = static_cast<float>(parse_number(json, pos));
                else if (gkey == "cornerRadius") corner_radius = static_cast<float>(parse_number(json, pos));
                else if (gkey == "rx") rx = static_cast<float>(parse_number(json, pos));
                else if (gkey == "ry") ry = static_cast<float>(parse_number(json, pos));
                else if (gkey == "sides") sides = static_cast<int>(parse_number(json, pos));
                else if (gkey == "points") points = static_cast<int>(parse_number(json, pos));
                else if (gkey == "outerRadius") outer_radius = static_cast<float>(parse_number(json, pos));
                else if (gkey == "innerRadius") inner_radius = static_cast<float>(parse_number(json, pos));
                else if (gkey == "x2") x2 = static_cast<float>(parse_number(json, pos));
                else if (gkey == "y2") y2 = static_cast<float>(parse_number(json, pos));
                else if (gkey == "d") path_d = parse_string(json, pos);
                skip_whitespace(json, pos);
                if (pos < json.size() && json[pos] == ',') pos++;
            }
            expect_char(json, pos, '}');
        } else if (key == "fill") {
            skip_whitespace(json, pos);
            if (json.substr(pos, 4) == "null") { pos += 4; }
            else {
                expect_char(json, pos, '{');
                while (pos < json.size() && json[pos] != '}') {
                    std::string fkey = parse_key(json, pos);
                    if (fkey == "r") fill_color.r = static_cast<float>(parse_number(json, pos));
                    else if (fkey == "g") fill_color.g = static_cast<float>(parse_number(json, pos));
                    else if (fkey == "b") fill_color.b = static_cast<float>(parse_number(json, pos));
                    else if (fkey == "a") fill_color.a = static_cast<float>(parse_number(json, pos));
                    skip_whitespace(json, pos);
                    if (pos < json.size() && json[pos] == ',') pos++;
                }
                expect_char(json, pos, '}');
                has_fill = true;
            }
        } else if (key == "stroke") {
            skip_whitespace(json, pos);
            if (json.substr(pos, 4) == "null") { pos += 4; }
            else {
                expect_char(json, pos, '{');
                while (pos < json.size() && json[pos] != '}') {
                    std::string skey = parse_key(json, pos);
                    if (skey == "color") {
                        expect_char(json, pos, '{');
                        while (pos < json.size() && json[pos] != '}') {
                            std::string ckey = parse_key(json, pos);
                            if (ckey == "r") stroke_color.r = static_cast<float>(parse_number(json, pos));
                            else if (ckey == "g") stroke_color.g = static_cast<float>(parse_number(json, pos));
                            else if (ckey == "b") stroke_color.b = static_cast<float>(parse_number(json, pos));
                            else if (ckey == "a") stroke_color.a = static_cast<float>(parse_number(json, pos));
                            skip_whitespace(json, pos);
                            if (pos < json.size() && json[pos] == ',') pos++;
                        }
                        expect_char(json, pos, '}');
                    } else if (skey == "width") {
                        stroke_width = static_cast<float>(parse_number(json, pos));
                    }
                    skip_whitespace(json, pos);
                    if (pos < json.size() && json[pos] == ',') pos++;
                }
                expect_char(json, pos, '}');
                has_stroke = true;
            }
        }

        skip_whitespace(json, pos);
        if (pos < json.size() && json[pos] == ',') pos++;
    }
    expect_char(json, pos, '}');

    // Apply geometry
    if (geom_type == "rect") shape->set_rect(width, height, corner_radius);
    else if (geom_type == "circle") shape->set_circle(radius);
    else if (geom_type == "ellipse") shape->set_ellipse(rx, ry);
    else if (geom_type == "polygon") shape->set_polygon(sides, radius);
    else if (geom_type == "star") shape->set_star(points, outer_radius, inner_radius);
    else if (geom_type == "path") shape->set_path(path_d);
    else if (geom_type == "line") shape->set_line(x2, y2);

    shape->set_position(x, y);
    if (has_fill) shape->set_fill(fill_color);
    if (has_stroke) shape->set_stroke(stroke_color, stroke_width);

    return shape;
}

flex::Text* Serializer::json_to_text(const std::string& json, size_t& pos) {
    if (!expect_char(json, pos, '{')) return nullptr;

    auto* allocator = canvas_->instance()->object_allocator();
    auto* text = flex::Text::create(*allocator);

    while (pos < json.size() && json[pos] != '}') {
        std::string key = parse_key(json, pos);
        skip_whitespace(json, pos);

        if (key == "type") { parse_string(json, pos); }
        else if (key == "x") text->set_position(static_cast<float>(parse_number(json, pos)), text->y());
        else if (key == "y") text->set_position(text->x(), static_cast<float>(parse_number(json, pos)));
        else if (key == "visible") text->set_visible(parse_bool(json, pos));
        else if (key == "content") text->set_content(parse_string(json, pos));
        else if (key == "fontFamily") text->set_font_family(parse_string(json, pos));
        else if (key == "fontSize") text->set_font_size(static_cast<float>(parse_number(json, pos)));
        else if (key == "color") {
            flex::Color c;
            expect_char(json, pos, '{');
            while (pos < json.size() && json[pos] != '}') {
                std::string ckey = parse_key(json, pos);
                if (ckey == "r") c.r = static_cast<float>(parse_number(json, pos));
                else if (ckey == "g") c.g = static_cast<float>(parse_number(json, pos));
                else if (ckey == "b") c.b = static_cast<float>(parse_number(json, pos));
                else if (ckey == "a") c.a = static_cast<float>(parse_number(json, pos));
                skip_whitespace(json, pos);
                if (pos < json.size() && json[pos] == ',') pos++;
            }
            expect_char(json, pos, '}');
            text->set_color(c);
        }

        skip_whitespace(json, pos);
        if (pos < json.size() && json[pos] == ',') pos++;
    }
    expect_char(json, pos, '}');

    return text;
}

flex::Group* Serializer::json_to_group(const std::string& json, size_t& pos) {
    if (!expect_char(json, pos, '{')) return nullptr;

    auto* allocator = canvas_->instance()->object_allocator();
    auto* group = flex::Group::create(*allocator);

    while (pos < json.size() && json[pos] != '}') {
        std::string key = parse_key(json, pos);
        skip_whitespace(json, pos);

        if (key == "type") { parse_string(json, pos); }
        else if (key == "id") group->set_id(parse_string(json, pos));
        else if (key == "x") group->set_position(static_cast<float>(parse_number(json, pos)), group->y());
        else if (key == "y") group->set_position(group->x(), static_cast<float>(parse_number(json, pos)));
        else if (key == "visible") group->set_visible(parse_bool(json, pos));
        else if (key == "children") {
            expect_char(json, pos, '[');
            while (pos < json.size() && json[pos] != ']') {
                skip_whitespace(json, pos);
                if (json[pos] == ',') { pos++; continue; }
                if (json[pos] == ']') break;

                flex::Node* child = json_to_node(json, pos);
                if (child) group->add_child(child);
            }
            expect_char(json, pos, ']');
        }

        skip_whitespace(json, pos);
        if (pos < json.size() && json[pos] == ',') pos++;
    }
    expect_char(json, pos, '}');

    return group;
}

} // namespace meta_editor
