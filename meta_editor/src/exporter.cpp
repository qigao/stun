/*
 * Meta Editor - Export Implementation
 */

#include "meta_editor/exporter.h"
#include "meta_editor/canvas.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif
namespace meta_editor {

Exporter::Exporter(Canvas* canvas) : canvas_(canvas) {}

std::string Exporter::to_svg() const {
    std::stringstream ss;

    float w = canvas_->width();
    float h = canvas_->height();

    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ss << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
    ss << "width=\"" << w << "\" height=\"" << h << "\" ";
    ss << "viewBox=\"0 0 " << w << " " << h << "\">\n";

    // Export content root
    auto* root = canvas_->content_root();
    if (root) {
        for (auto* child : root->children()) {
            ss << node_to_svg(child, 2);
        }
    }

    ss << "</svg>\n";
    return ss.str();
}

std::string Exporter::selection_to_svg(const std::vector<flex::Node*>& nodes) const {
    if (nodes.empty()) return "";

    // Calculate bounds of selection
    float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
    for (auto* node : nodes) {
        auto bounds = node->world_bounds();
        min_x = std::min(min_x, bounds.x);
        min_y = std::min(min_y, bounds.y);
        max_x = std::max(max_x, bounds.x + bounds.width);
        max_y = std::max(max_y, bounds.y + bounds.height);
    }

    float w = max_x - min_x + 20;  // 10px padding
    float h = max_y - min_y + 20;

    std::stringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    ss << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
    ss << "width=\"" << w << "\" height=\"" << h << "\" ";
    ss << "viewBox=\"" << (min_x - 10) << " " << (min_y - 10) << " " << w << " " << h << "\">\n";

    for (auto* node : nodes) {
        ss << node_to_svg(node, 2);
    }

    ss << "</svg>\n";
    return ss.str();
}

bool Exporter::save_svg(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;

    file << to_svg();
    return true;
}

bool Exporter::save_png(const std::string& path, const uint32_t* buffer, int width, int height) const {
    if (!buffer || width <= 0 || height <= 0) return false;

    // Convert ARGB (ThorVG format) to RGBA (PNG format)
    std::vector<uint8_t> rgba(width * height * 4);
    for (int i = 0; i < width * height; i++) {
        uint32_t c = buffer[i];
        rgba[i * 4 + 0] = (c >> 16) & 0xFF;  // R
        rgba[i * 4 + 1] = (c >> 8) & 0xFF;   // G
        rgba[i * 4 + 2] = c & 0xFF;          // B
        rgba[i * 4 + 3] = (c >> 24) & 0xFF;  // A
    }

    return stbi_write_png(path.c_str(), width, height, 4, rgba.data(), width * 4) != 0;
}

bool Exporter::copy_to_clipboard(const std::string& svg) const {
    // Platform-specific clipboard implementation would go here
    // For now, return false (not implemented)
    return false;
}

std::string Exporter::node_to_svg(flex::Node* node, int indent) const {
    if (!node || !node->visible()) return "";

    if (node->type() == flex::NodeType::Shape) {
        return shape_to_svg(static_cast<flex::Shape*>(node), indent);
    } else if (node->is_group()) {
        return group_to_svg(static_cast<flex::Group*>(node), indent);
    }

    return "";
}

std::string Exporter::shape_to_svg(flex::Shape* shape, int indent) const {
    std::stringstream ss;
    std::string ind = indent_str(indent);

    float x = shape->x();
    float y = shape->y();

    // Build style string
    std::string style;
    if (!shape->has_fill()) {
        style += "fill:none;";
    } else {
        auto fill = shape->fill();
        style += "fill:" + color_to_css(fill.color) + ";";
    }

    if (!shape->has_stroke()) {
        style += "stroke:none;";
    } else {
        auto stroke = shape->stroke();
        style += "stroke:" + color_to_css(stroke.color) + ";";
        style += "stroke-width:" + std::to_string(stroke.width) + ";";
    }

    switch (shape->geometry_type()) {
        case flex::GeometryType::Rect: {
            auto r = shape->rect();
            ss << ind << "<rect ";
            ss << "x=\"" << x << "\" y=\"" << y << "\" ";
            ss << "width=\"" << r.width << "\" height=\"" << r.height << "\" ";
            if (r.corner_radius > 0) {
                ss << "rx=\"" << r.corner_radius << "\" ";
            }
            ss << "style=\"" << style << "\"";
            ss << "/>\n";
            break;
        }

        case flex::GeometryType::Circle: {
            auto c = shape->circle();
            ss << ind << "<circle ";
            ss << "cx=\"" << x << "\" cy=\"" << y << "\" ";
            ss << "r=\"" << c.radius << "\" ";
            ss << "style=\"" << style << "\"";
            ss << "/>\n";
            break;
        }

        case flex::GeometryType::Ellipse: {
            auto e = shape->ellipse();
            ss << ind << "<ellipse ";
            ss << "cx=\"" << x << "\" cy=\"" << y << "\" ";
            ss << "rx=\"" << e.rx << "\" ry=\"" << e.ry << "\" ";
            ss << "style=\"" << style << "\"";
            ss << "/>\n";
            break;
        }

        case flex::GeometryType::Line: {
            auto l = shape->line();
            ss << ind << "<line ";
            ss << "x1=\"" << x << "\" y1=\"" << y << "\" ";
            ss << "x2=\"" << (x + l.x2) << "\" y2=\"" << (y + l.y2) << "\" ";
            ss << "style=\"" << style << "\"";
            ss << "/>\n";
            break;
        }

        case flex::GeometryType::Path: {
            auto p = shape->path();
            ss << ind << "<path ";
            ss << "d=\"" << p.d << "\" ";
            // Apply transform for position
            if (x != 0 || y != 0) {
                ss << "transform=\"translate(" << x << "," << y << ")\" ";
            }
            ss << "style=\"" << style << "\"";
            ss << "/>\n";
            break;
        }

        case flex::GeometryType::Polygon: {
            auto p = shape->polygon();
            // Generate polygon points
            std::stringstream pts;
            for (int i = 0; i < p.sides; ++i) {
                float angle = -M_PI / 2 + i * 2 * M_PI / p.sides;
                float px = x + p.radius * std::cos(angle);
                float py = y + p.radius * std::sin(angle);
                if (i > 0) pts << " ";
                pts << px << "," << py;
            }
            ss << ind << "<polygon ";
            ss << "points=\"" << pts.str() << "\" ";
            ss << "style=\"" << style << "\"";
            ss << "/>\n";
            break;
        }

        case flex::GeometryType::Star: {
            auto s = shape->star();
            // Generate star points
            std::stringstream pts;
            int total_points = s.points * 2;
            for (int i = 0; i < total_points; ++i) {
                float angle = -M_PI / 2 + i * M_PI / s.points;
                float r = (i % 2 == 0) ? s.outer_radius : s.inner_radius;
                float px = x + r * std::cos(angle);
                float py = y + r * std::sin(angle);
                if (i > 0) pts << " ";
                pts << px << "," << py;
            }
            ss << ind << "<polygon ";
            ss << "points=\"" << pts.str() << "\" ";
            ss << "style=\"" << style << "\"";
            ss << "/>\n";
            break;
        }

        default:
            // Fallback: render as rect using bounds
            auto bounds = shape->bounds();
            ss << ind << "<rect ";
            ss << "x=\"" << (x + bounds.x) << "\" y=\"" << (y + bounds.y) << "\" ";
            ss << "width=\"" << bounds.width << "\" height=\"" << bounds.height << "\" ";
            ss << "style=\"" << style << "\"";
            ss << "/>\n";
            break;
    }

    return ss.str();
}

std::string Exporter::group_to_svg(flex::Group* group, int indent) const {
    std::stringstream ss;
    std::string ind = indent_str(indent);

    ss << ind << "<g";
    if (!group->id().empty()) {
        ss << " id=\"" << group->id() << "\"";
    }
    if (group->x() != 0 || group->y() != 0) {
        ss << " transform=\"translate(" << group->x() << "," << group->y() << ")\"";
    }
    if (group->opacity() < 1.0f) {
        ss << " opacity=\"" << group->opacity() << "\"";
    }
    ss << ">\n";

    for (auto* child : group->children()) {
        ss << node_to_svg(child, indent + 2);
    }

    ss << ind << "</g>\n";
    return ss.str();
}

std::string Exporter::color_to_css(const flex::Color& color) const {
    int r = static_cast<int>(color.r * 255);
    int g = static_cast<int>(color.g * 255);
    int b = static_cast<int>(color.b * 255);

    if (color.a < 1.0f) {
        std::stringstream ss;
        ss << "rgba(" << r << "," << g << "," << b << "," << color.a << ")";
        return ss.str();
    }

    std::stringstream ss;
    ss << "#" << std::hex << std::setfill('0')
       << std::setw(2) << r
       << std::setw(2) << g
       << std::setw(2) << b;
    return ss.str();
}

std::string Exporter::indent_str(int level) const {
    return std::string(level, ' ');
}

} // namespace meta_editor
