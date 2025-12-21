/*
 * Document Exporter
 *
 * Exports editor documents to various formats (SVG, PNG).
 */

#pragma once

#include "../model/document.h"
#include "../model/node.h"
#include <string>
#include <fstream>
#include <sstream>

namespace editor {

// Export format options
enum class ExportFormat {
    SVG,
    PNG
};

// Export options
struct ExportOptions {
    float scale = 1.0f;           // Scale factor for raster export
    bool include_background = true;
    bool selected_only = false;
};

// Document Exporter
class Exporter {
public:
    // Export to SVG
    static bool exportSVG(Document* doc, const std::string& filepath, const ExportOptions& opts = {}) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        float w = doc->width();
        float h = doc->height();

        // SVG header
        file << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        file << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
        file << "width=\"" << w << "\" height=\"" << h << "\" ";
        file << "viewBox=\"0 0 " << w << " " << h << "\">\n";

        // Background
        if (opts.include_background) {
            auto bg = doc->backgroundColor();
            file << "  <rect width=\"100%\" height=\"100%\" fill=\"";
            file << colorToHex(bg) << "\"/>\n";
        }

        // Export layers
        for (const auto& layer : doc->layers()) {
            if (!layer->visible()) continue;

            file << "  <g id=\"" << escapeXml(layer->name()) << "\">\n";

            for (const auto& node : layer->nodes()) {
                exportNodeSVG(file, node.get(), 4);
            }

            file << "  </g>\n";
        }

        file << "</svg>\n";
        return true;
    }

    // Export to PNG (requires ThorVG runtime)
    static bool exportPNG(Document* doc, const std::string& filepath, const ExportOptions& opts = {}) {
        // PNG export requires rendering to offscreen buffer via ThorVG
        // This is a placeholder - actual implementation needs ThorVG canvas setup

        // For now, create a minimal PNG file header to indicate the format
        // Real implementation would:
        // 1. Create ThorVG SwCanvas
        // 2. Render document to canvas
        // 3. Get pixel buffer
        // 4. Encode as PNG using stb_image_write or similar

        (void)doc;
        (void)filepath;
        (void)opts;

        // Return false to indicate PNG export not yet implemented
        // Users can use SVG export and convert externally
        return false;
    }

    // Generate SVG string (for clipboard, etc.)
    static std::string toSVGString(Document* doc, const ExportOptions& opts = {}) {
        std::stringstream ss;

        float w = doc->width();
        float h = doc->height();

        ss << "<svg xmlns=\"http://www.w3.org/2000/svg\" ";
        ss << "width=\"" << w << "\" height=\"" << h << "\" ";
        ss << "viewBox=\"0 0 " << w << " " << h << "\">\n";

        if (opts.include_background) {
            auto bg = doc->backgroundColor();
            ss << "  <rect width=\"100%\" height=\"100%\" fill=\"";
            ss << colorToHex(bg) << "\"/>\n";
        }

        for (const auto& layer : doc->layers()) {
            if (!layer->visible()) continue;

            ss << "  <g id=\"" << escapeXml(layer->name()) << "\">\n";

            for (const auto& node : layer->nodes()) {
                exportNodeSVGStream(ss, node.get(), 4);
            }

            ss << "  </g>\n";
        }

        ss << "</svg>";
        return ss.str();
    }

private:
    static void exportNodeSVG(std::ofstream& file, EditorNode* node, int indent) {
        std::stringstream ss;
        exportNodeSVGStream(ss, node, indent);
        file << ss.str();
    }

    static void exportNodeSVGStream(std::stringstream& ss, EditorNode* node, int indent) {
        std::string ind(indent, ' ');

        if (auto* shape = dynamic_cast<ShapeNode*>(node)) {
            exportShapeSVG(ss, shape, indent);
        } else if (auto* textNode = dynamic_cast<TextNode*>(node)) {
            exportTextSVG(ss, textNode, indent);
        } else if (auto* group = dynamic_cast<GroupNode*>(node)) {
            ss << ind << "<g";
            writeTransform(ss, node);
            ss << ">\n";

            for (const auto& child : group->children()) {
                exportNodeSVGStream(ss, child.get(), indent + 2);
            }

            ss << ind << "</g>\n";
        }
    }

    static void exportShapeSVG(std::stringstream& ss, ShapeNode* shapeNode, int indent) {
        std::string ind(indent, ' ');
        auto* shape = shapeNode->shape();
        if (!shape) return;

        auto geoType = shape->geometry_type();

        switch (geoType) {
            case flex::GeometryType::Rect: {
                auto rect = shape->rect();
                ss << ind << "<rect";
                ss << " x=\"" << shapeNode->x() << "\"";
                ss << " y=\"" << shapeNode->y() << "\"";
                ss << " width=\"" << rect.width << "\"";
                ss << " height=\"" << rect.height << "\"";
                if (rect.corner_radius > 0) {
                    ss << " rx=\"" << rect.corner_radius << "\"";
                    ss << " ry=\"" << rect.corner_radius << "\"";
                }
                writeFillStroke(ss, shape);
                writeNodeTransform(ss, shapeNode);
                ss << "/>\n";
                break;
            }

            case flex::GeometryType::Circle: {
                auto circle = shape->circle();
                ss << ind << "<circle";
                ss << " cx=\"" << shapeNode->x() << "\"";
                ss << " cy=\"" << shapeNode->y() << "\"";
                ss << " r=\"" << circle.radius << "\"";
                writeFillStroke(ss, shape);
                writeNodeTransform(ss, shapeNode);
                ss << "/>\n";
                break;
            }

            case flex::GeometryType::Ellipse: {
                auto ellipse = shape->ellipse();
                ss << ind << "<ellipse";
                ss << " cx=\"" << shapeNode->x() << "\"";
                ss << " cy=\"" << shapeNode->y() << "\"";
                ss << " rx=\"" << ellipse.rx << "\"";
                ss << " ry=\"" << ellipse.ry << "\"";
                writeFillStroke(ss, shape);
                writeNodeTransform(ss, shapeNode);
                ss << "/>\n";
                break;
            }

            case flex::GeometryType::Line: {
                auto line = shape->line();
                ss << ind << "<line";
                ss << " x1=\"" << shapeNode->x() << "\"";
                ss << " y1=\"" << shapeNode->y() << "\"";
                ss << " x2=\"" << (shapeNode->x() + line.x2) << "\"";
                ss << " y2=\"" << (shapeNode->y() + line.y2) << "\"";
                writeFillStroke(ss, shape);
                writeNodeTransform(ss, shapeNode);
                ss << "/>\n";
                break;
            }

            case flex::GeometryType::Path: {
                auto path = shape->path();
                ss << ind << "<path";
                ss << " d=\"" << escapeXml(path.d) << "\"";
                writeFillStroke(ss, shape);
                writeTransform(ss, shapeNode);
                ss << "/>\n";
                break;
            }

            case flex::GeometryType::Polygon: {
                auto poly = shape->polygon();
                ss << ind << "<polygon";
                ss << " points=\"";
                // Generate polygon points
                float cx = shapeNode->x();
                float cy = shapeNode->y();
                for (int i = 0; i < poly.sides; ++i) {
                    float angle = -3.14159265f / 2 + 2 * 3.14159265f * i / poly.sides;
                    float px = cx + poly.radius * std::cos(angle);
                    float py = cy + poly.radius * std::sin(angle);
                    if (i > 0) ss << " ";
                    ss << px << "," << py;
                }
                ss << "\"";
                writeFillStroke(ss, shape);
                writeNodeTransform(ss, shapeNode);
                ss << "/>\n";
                break;
            }

            default:
                break;
        }
    }

    static void exportTextSVG(std::stringstream& ss, TextNode* textNode, int indent) {
        std::string ind(indent, ' ');
        auto* text = textNode->text();
        if (!text) return;

        ss << ind << "<text";
        ss << " x=\"" << textNode->x() << "\"";
        ss << " y=\"" << textNode->y() << "\"";
        ss << " font-family=\"" << escapeXml(text->font_family()) << "\"";
        ss << " font-size=\"" << text->font_size() << "\"";

        if (text->font_weight() == flex::FontWeight::Bold) {
            ss << " font-weight=\"bold\"";
        }
        if (text->font_style() == flex::FontStyle::Italic) {
            ss << " font-style=\"italic\"";
        }

        auto color = text->color();
        ss << " fill=\"" << colorToHex(Color{color.r, color.g, color.b, color.a}) << "\"";

        if (color.a < 1.0f) {
            ss << " fill-opacity=\"" << color.a << "\"";
        }

        writeNodeTransform(ss, textNode);
        ss << ">";
        ss << escapeXml(text->content());
        ss << "</text>\n";
    }

    static void writeFillStroke(std::stringstream& ss, flex::Shape* shape) {
        if (shape->has_fill()) {
            auto fill = shape->fill();
            if (fill.type == flex::FillType::Solid) {
                ss << " fill=\"" << colorToHex({fill.color.r, fill.color.g, fill.color.b, 1}) << "\"";
                if (fill.color.a < 1.0f) {
                    ss << " fill-opacity=\"" << fill.color.a << "\"";
                }
            } else if (fill.type == flex::FillType::LinearGradient) {
                // For gradients, would need to define in <defs> section
                // Simplified: use first stop color
                if (!fill.linear_gradient.stops.empty()) {
                    auto& c = fill.linear_gradient.stops[0].color;
                    ss << " fill=\"" << colorToHex({c.r, c.g, c.b, 1}) << "\"";
                }
            }
        } else {
            ss << " fill=\"none\"";
        }

        if (shape->has_stroke()) {
            auto stroke = shape->stroke();
            ss << " stroke=\"" << colorToHex({stroke.color.r, stroke.color.g, stroke.color.b, 1}) << "\"";
            ss << " stroke-width=\"" << stroke.width << "\"";
            if (stroke.color.a < 1.0f) {
                ss << " stroke-opacity=\"" << stroke.color.a << "\"";
            }
        }
    }

    static void writeTransform(std::stringstream& ss, EditorNode* node) {
        bool hasTransform = false;
        std::stringstream ts;

        if (node->x() != 0 || node->y() != 0) {
            ts << "translate(" << node->x() << " " << node->y() << ")";
            hasTransform = true;
        }

        if (node->rotation() != 0) {
            if (hasTransform) ts << " ";
            ts << "rotate(" << node->rotation() << ")";
            hasTransform = true;
        }

        if (node->scaleX() != 1 || node->scaleY() != 1) {
            if (hasTransform) ts << " ";
            ts << "scale(" << node->scaleX() << " " << node->scaleY() << ")";
            hasTransform = true;
        }

        if (hasTransform) {
            ss << " transform=\"" << ts.str() << "\"";
        }
    }

    static void writeNodeTransform(std::stringstream& ss, EditorNode* node) {
        // Only write rotation and scale, position is in element attributes
        bool hasTransform = false;
        std::stringstream ts;

        if (node->rotation() != 0) {
            ts << "rotate(" << node->rotation() << " " << node->x() << " " << node->y() << ")";
            hasTransform = true;
        }

        if (node->scaleX() != 1 || node->scaleY() != 1) {
            if (hasTransform) ts << " ";
            ts << "scale(" << node->scaleX() << " " << node->scaleY() << ")";
            hasTransform = true;
        }

        if (hasTransform) {
            ss << " transform=\"" << ts.str() << "\"";
        }
    }

    static std::string colorToHex(const Color& c) {
        char buf[8];
        int r = static_cast<int>(c.r * 255);
        int g = static_cast<int>(c.g * 255);
        int b = static_cast<int>(c.b * 255);
        snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
        return buf;
    }

    static std::string escapeXml(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '&': result += "&amp;"; break;
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '"': result += "&quot;"; break;
                case '\'': result += "&apos;"; break;
                default: result += c; break;
            }
        }
        return result;
    }
};

} // namespace editor
