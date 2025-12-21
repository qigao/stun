/*
 * Document Serializer
 *
 * Saves and loads editor documents in JSON format.
 */

#pragma once

#include "../model/document.h"
#include "../model/node.h"
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

namespace editor {

// Simple JSON serializer (no external dependencies)
class DocumentSerializer {
public:
    // Save document to file
    static bool save(Document* doc, const std::string& filepath) {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;

        file << "{\n";
        file << "  \"version\": 1,\n";
        file << "  \"name\": \"" << escapeString(doc->name()) << "\",\n";
        file << "  \"width\": " << doc->width() << ",\n";
        file << "  \"height\": " << doc->height() << ",\n";
        file << "  \"layers\": [\n";

        const auto& layers = doc->layers();
        for (size_t li = 0; li < layers.size(); ++li) {
            auto& layer = layers[li];
            file << "    {\n";
            file << "      \"name\": \"" << escapeString(layer->name()) << "\",\n";
            file << "      \"visible\": " << (layer->visible() ? "true" : "false") << ",\n";
            file << "      \"locked\": " << (layer->locked() ? "true" : "false") << ",\n";
            file << "      \"nodes\": [\n";

            auto& nodes = layer->nodes();
            for (size_t ni = 0; ni < nodes.size(); ++ni) {
                serializeNode(file, nodes[ni].get(), 8);
                if (ni < nodes.size() - 1) file << ",";
                file << "\n";
            }

            file << "      ]\n";
            file << "    }";
            if (li < layers.size() - 1) file << ",";
            file << "\n";
        }

        file << "  ]\n";
        file << "}\n";

        return true;
    }

    // Load document from file
    static bool load(Document* doc, const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json = buffer.str();

        // Parse JSON (simple parser)
        size_t pos = 0;

        // Find version
        auto version = findInt(json, "version", pos);
        if (version != 1) return false;

        // Find document properties
        doc->setName(findString(json, "name", pos));
        auto width = findInt(json, "width", pos);
        auto height = findInt(json, "height", pos);
        doc->setSize(static_cast<float>(width), static_cast<float>(height));

        // Clear existing layers
        doc->layers().clear();

        // Find layers array
        size_t layersStart = json.find("\"layers\"", pos);
        if (layersStart == std::string::npos) return false;

        size_t arrayStart = json.find('[', layersStart);
        size_t arrayEnd = findMatchingBracket(json, arrayStart);

        std::string layersJson = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

        // Parse each layer
        size_t layerPos = 0;
        while (true) {
            size_t objStart = layersJson.find('{', layerPos);
            if (objStart == std::string::npos) break;

            size_t objEnd = findMatchingBrace(layersJson, objStart);
            std::string layerJson = layersJson.substr(objStart, objEnd - objStart + 1);

            auto layer = parseLayer(layerJson);
            if (layer) {
                doc->addLayer(std::move(layer));
            }

            layerPos = objEnd + 1;
        }

        // If no layers were loaded, create a default one
        if (doc->layers().empty()) {
            doc->addLayer(std::make_unique<Layer>("Layer 1"));
        }

        return true;
    }

private:
    static void serializeNode(std::ofstream& file, EditorNode* node, int indent) {
        std::string ind(indent, ' ');

        file << ind << "{\n";
        file << ind << "  \"name\": \"" << escapeString(node->name()) << "\",\n";
        file << ind << "  \"x\": " << node->x() << ",\n";
        file << ind << "  \"y\": " << node->y() << ",\n";
        file << ind << "  \"scaleX\": " << node->scaleX() << ",\n";
        file << ind << "  \"scaleY\": " << node->scaleY() << ",\n";
        file << ind << "  \"rotation\": " << node->rotation() << ",\n";

        if (auto* shape = dynamic_cast<ShapeNode*>(node)) {
            file << ind << "  \"type\": \"shape\",\n";
            serializeShape(file, shape, indent + 2);
        } else if (auto* textNode = dynamic_cast<TextNode*>(node)) {
            file << ind << "  \"type\": \"text\",\n";
            serializeText(file, textNode, indent + 2);
        } else if (auto* group = dynamic_cast<GroupNode*>(node)) {
            file << ind << "  \"type\": \"group\",\n";
            file << ind << "  \"children\": [\n";
            auto& children = group->children();
            for (size_t i = 0; i < children.size(); ++i) {
                serializeNode(file, children[i].get(), indent + 4);
                if (i < children.size() - 1) file << ",";
                file << "\n";
            }
            file << ind << "  ]\n";
        }

        file << ind << "}";
    }

    static void serializeText(std::ofstream& file, TextNode* textNode, int indent) {
        std::string ind(indent, ' ');
        auto* text = textNode->text();
        if (!text) return;

        file << ind << "\"content\": \"" << escapeString(text->content()) << "\",\n";
        file << ind << "\"fontFamily\": \"" << escapeString(text->font_family()) << "\",\n";
        file << ind << "\"fontSize\": " << text->font_size() << ",\n";
        file << ind << "\"fontWeight\": " << (text->font_weight() == flex::FontWeight::Bold ? "\"bold\"" : "\"normal\"") << ",\n";
        file << ind << "\"fontStyle\": " << (text->font_style() == flex::FontStyle::Italic ? "\"italic\"" : "\"normal\"") << ",\n";

        auto color = text->color();
        file << ind << "\"colorR\": " << color.r << ",\n";
        file << ind << "\"colorG\": " << color.g << ",\n";
        file << ind << "\"colorB\": " << color.b << ",\n";
        file << ind << "\"colorA\": " << color.a << "\n";
    }

    static void serializeShape(std::ofstream& file, ShapeNode* shapeNode, int indent) {
        std::string ind(indent, ' ');
        auto* flexShape = shapeNode->shape();
        if (!flexShape) return;

        auto type = flexShape->geometry_type();

        file << ind << "\"shapeType\": ";
        switch (type) {
            case flex::GeometryType::Rect: {
                auto rect = flexShape->rect();
                file << "\"rect\",\n";
                file << ind << "\"width\": " << rect.width << ",\n";
                file << ind << "\"height\": " << rect.height << ",\n";
                file << ind << "\"cornerRadius\": " << rect.corner_radius << ",\n";
                break;
            }
            case flex::GeometryType::Ellipse: {
                auto ellipse = flexShape->ellipse();
                file << "\"ellipse\",\n";
                file << ind << "\"rx\": " << ellipse.rx << ",\n";
                file << ind << "\"ry\": " << ellipse.ry << ",\n";
                break;
            }
            case flex::GeometryType::Circle: {
                auto circle = flexShape->circle();
                file << "\"circle\",\n";
                file << ind << "\"radius\": " << circle.radius << ",\n";
                break;
            }
            case flex::GeometryType::Line: {
                auto line = flexShape->line();
                file << "\"line\",\n";
                file << ind << "\"x2\": " << line.x2 << ",\n";
                file << ind << "\"y2\": " << line.y2 << ",\n";
                break;
            }
            case flex::GeometryType::Path:
                file << "\"path\",\n";
                file << ind << "\"path\": \"" << escapeString(flexShape->path().d) << "\",\n";
                break;
            default:
                file << "\"unknown\",\n";
                break;
        }

        // Serialize fill
        if (flexShape->has_fill()) {
            auto fill = flexShape->fill();
            file << ind << "\"hasFill\": true,\n";
            file << ind << "\"fillR\": " << fill.color.r << ",\n";
            file << ind << "\"fillG\": " << fill.color.g << ",\n";
            file << ind << "\"fillB\": " << fill.color.b << ",\n";
            file << ind << "\"fillA\": " << fill.color.a << ",\n";
        } else {
            file << ind << "\"hasFill\": false,\n";
        }

        // Serialize stroke
        if (flexShape->has_stroke()) {
            auto stroke = flexShape->stroke();
            file << ind << "\"hasStroke\": true,\n";
            file << ind << "\"strokeR\": " << stroke.color.r << ",\n";
            file << ind << "\"strokeG\": " << stroke.color.g << ",\n";
            file << ind << "\"strokeB\": " << stroke.color.b << ",\n";
            file << ind << "\"strokeA\": " << stroke.color.a << ",\n";
            file << ind << "\"strokeWidth\": " << stroke.width << "\n";
        } else {
            file << ind << "\"hasStroke\": false\n";
        }
    }

    static std::unique_ptr<Layer> parseLayer(const std::string& json) {
        size_t pos = 0;
        std::string name = findString(json, "name", pos);
        bool visible = findBool(json, "visible", pos);
        bool locked = findBool(json, "locked", pos);

        auto layer = std::make_unique<Layer>(name);
        layer->setVisible(visible);
        layer->setLocked(locked);

        // Find nodes array
        size_t nodesStart = json.find("\"nodes\"", pos);
        if (nodesStart != std::string::npos) {
            size_t arrayStart = json.find('[', nodesStart);
            size_t arrayEnd = findMatchingBracket(json, arrayStart);
            std::string nodesJson = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

            // Parse each node
            size_t nodePos = 0;
            while (true) {
                size_t objStart = nodesJson.find('{', nodePos);
                if (objStart == std::string::npos) break;

                size_t objEnd = findMatchingBrace(nodesJson, objStart);
                std::string nodeJson = nodesJson.substr(objStart, objEnd - objStart + 1);

                auto node = parseNode(nodeJson);
                if (node) {
                    layer->addNode(std::move(node));
                }

                nodePos = objEnd + 1;
            }
        }

        return layer;
    }

    static EditorNode::Ptr parseNode(const std::string& json) {
        size_t pos = 0;
        std::string name = findString(json, "name", pos);
        float x = findFloat(json, "x", pos);
        float y = findFloat(json, "y", pos);
        float scaleX = findFloat(json, "scaleX", pos);
        float scaleY = findFloat(json, "scaleY", pos);
        float rotation = findFloat(json, "rotation", pos);
        std::string type = findString(json, "type", pos);

        EditorNode::Ptr node;

        if (type == "shape") {
            std::string shapeType = findString(json, "shapeType", pos);
            auto shape = flex::Shape::create();

            if (shapeType == "rect") {
                float w = findFloat(json, "width", pos);
                float h = findFloat(json, "height", pos);
                float cornerRadius = findFloat(json, "cornerRadius", pos);
                shape->set_position(x, y);
                shape->set_rect(w, h, cornerRadius);
            } else if (shapeType == "ellipse") {
                float rx = findFloat(json, "rx", pos);
                float ry = findFloat(json, "ry", pos);
                shape->set_position(x, y);
                shape->set_ellipse(rx, ry);
            } else if (shapeType == "circle") {
                float radius = findFloat(json, "radius", pos);
                shape->set_position(x, y);
                shape->set_circle(radius);
            } else if (shapeType == "line") {
                float x2 = findFloat(json, "x2", pos);
                float y2 = findFloat(json, "y2", pos);
                shape->set_position(x, y);
                shape->set_line(x2, y2);
            } else if (shapeType == "path") {
                std::string pathData = findString(json, "path", pos);
                shape->set_position(x, y);
                shape->set_path(pathData);
            }

            // Load fill
            if (findBool(json, "hasFill", pos)) {
                float r = findFloat(json, "fillR", pos);
                float g = findFloat(json, "fillG", pos);
                float b = findFloat(json, "fillB", pos);
                float a = findFloat(json, "fillA", pos);
                shape->set_fill(flex::Color{r, g, b, a});
            }

            // Load stroke
            if (findBool(json, "hasStroke", pos)) {
                float r = findFloat(json, "strokeR", pos);
                float g = findFloat(json, "strokeG", pos);
                float b = findFloat(json, "strokeB", pos);
                float a = findFloat(json, "strokeA", pos);
                float width = findFloat(json, "strokeWidth", pos);
                shape->set_stroke(flex::Color{r, g, b, a}, width);
            }

            node = ShapeNode::create(shape);
        } else if (type == "group") {
            auto group = flex::Group::create();
            auto groupNode = GroupNode::create(group);

            // Parse children recursively
            size_t childrenStart = json.find("\"children\"", pos);
            if (childrenStart != std::string::npos) {
                size_t arrayStart = json.find('[', childrenStart);
                size_t arrayEnd = findMatchingBracket(json, arrayStart);
                std::string childrenJson = json.substr(arrayStart + 1, arrayEnd - arrayStart - 1);

                size_t childPos = 0;
                while (true) {
                    size_t objStart = childrenJson.find('{', childPos);
                    if (objStart == std::string::npos) break;

                    size_t objEnd = findMatchingBrace(childrenJson, objStart);
                    std::string childJson = childrenJson.substr(objStart, objEnd - objStart + 1);

                    auto child = parseNode(childJson);
                    if (child) {
                        groupNode->addChild(std::move(child));
                    }

                    childPos = objEnd + 1;
                }
            }
            node = std::move(groupNode);
        } else if (type == "text") {
            auto text = flex::Text::create();
            text->set_position(x, y);

            std::string content = findString(json, "content", pos);
            text->set_content(content);

            std::string fontFamily = findString(json, "fontFamily", pos);
            if (!fontFamily.empty()) {
                text->set_font_family(fontFamily);
            }

            float fontSize = findFloat(json, "fontSize", pos);
            if (fontSize > 0) {
                text->set_font_size(fontSize);
            }

            std::string fontWeight = findString(json, "fontWeight", pos);
            if (fontWeight == "bold") {
                text->set_font_weight(flex::FontWeight::Bold);
            }

            std::string fontStyle = findString(json, "fontStyle", pos);
            if (fontStyle == "italic") {
                text->set_font_style(flex::FontStyle::Italic);
            }

            float r = findFloat(json, "colorR", pos);
            float g = findFloat(json, "colorG", pos);
            float b = findFloat(json, "colorB", pos);
            float a = findFloat(json, "colorA", pos);
            text->set_color(flex::Color{r, g, b, a});

            node = TextNode::create(text);
        }

        if (node) {
            node->setName(name);
            node->setPosition(x, y);
            node->setScale(scaleX, scaleY);
            node->setRotation(rotation);
        }

        return node;
    }

    // String utilities
    static std::string escapeString(const std::string& s) {
        std::string result;
        for (char c : s) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default: result += c; break;
            }
        }
        return result;
    }

    static std::string unescapeString(const std::string& s) {
        std::string result;
        for (size_t i = 0; i < s.length(); ++i) {
            if (s[i] == '\\' && i + 1 < s.length()) {
                switch (s[i + 1]) {
                    case '"': result += '"'; ++i; break;
                    case '\\': result += '\\'; ++i; break;
                    case 'n': result += '\n'; ++i; break;
                    case 'r': result += '\r'; ++i; break;
                    case 't': result += '\t'; ++i; break;
                    default: result += s[i]; break;
                }
            } else {
                result += s[i];
            }
        }
        return result;
    }

    // JSON parsing utilities
    static std::string findString(const std::string& json, const std::string& key, size_t& pos) {
        std::string search = "\"" + key + "\"";
        size_t keyPos = json.find(search, pos);
        if (keyPos == std::string::npos) return "";

        size_t colonPos = json.find(':', keyPos);
        if (colonPos == std::string::npos) return "";

        size_t startQuote = json.find('"', colonPos + 1);
        if (startQuote == std::string::npos) return "";

        size_t endQuote = startQuote + 1;
        while (endQuote < json.length()) {
            if (json[endQuote] == '"' && json[endQuote - 1] != '\\') break;
            ++endQuote;
        }

        pos = endQuote + 1;
        return unescapeString(json.substr(startQuote + 1, endQuote - startQuote - 1));
    }

    static int findInt(const std::string& json, const std::string& key, size_t& pos) {
        std::string search = "\"" + key + "\"";
        size_t keyPos = json.find(search, pos);
        if (keyPos == std::string::npos) return 0;

        size_t colonPos = json.find(':', keyPos);
        if (colonPos == std::string::npos) return 0;

        size_t numStart = colonPos + 1;
        while (numStart < json.length() && (json[numStart] == ' ' || json[numStart] == '\n')) ++numStart;

        size_t numEnd = numStart;
        while (numEnd < json.length() && (std::isdigit(json[numEnd]) || json[numEnd] == '-')) ++numEnd;

        pos = numEnd;
        return std::stoi(json.substr(numStart, numEnd - numStart));
    }

    static float findFloat(const std::string& json, const std::string& key, size_t& pos) {
        std::string search = "\"" + key + "\"";
        size_t keyPos = json.find(search, pos);
        if (keyPos == std::string::npos) return 0.0f;

        size_t colonPos = json.find(':', keyPos);
        if (colonPos == std::string::npos) return 0.0f;

        size_t numStart = colonPos + 1;
        while (numStart < json.length() && (json[numStart] == ' ' || json[numStart] == '\n')) ++numStart;

        size_t numEnd = numStart;
        while (numEnd < json.length() && (std::isdigit(json[numEnd]) || json[numEnd] == '-' || json[numEnd] == '.')) ++numEnd;

        pos = numEnd;
        return std::stof(json.substr(numStart, numEnd - numStart));
    }

    static bool findBool(const std::string& json, const std::string& key, size_t& pos) {
        std::string search = "\"" + key + "\"";
        size_t keyPos = json.find(search, pos);
        if (keyPos == std::string::npos) return false;

        size_t colonPos = json.find(':', keyPos);
        if (colonPos == std::string::npos) return false;

        size_t valStart = colonPos + 1;
        while (valStart < json.length() && (json[valStart] == ' ' || json[valStart] == '\n')) ++valStart;

        pos = valStart + 4;
        return json.substr(valStart, 4) == "true";
    }

    static size_t findMatchingBracket(const std::string& json, size_t start) {
        int depth = 1;
        for (size_t i = start + 1; i < json.length(); ++i) {
            if (json[i] == '[') ++depth;
            else if (json[i] == ']') --depth;
            if (depth == 0) return i;
        }
        return std::string::npos;
    }

    static size_t findMatchingBrace(const std::string& json, size_t start) {
        int depth = 1;
        for (size_t i = start + 1; i < json.length(); ++i) {
            if (json[i] == '{') ++depth;
            else if (json[i] == '}') --depth;
            if (depth == 0) return i;
        }
        return std::string::npos;
    }
};

} // namespace editor
