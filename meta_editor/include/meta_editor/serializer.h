/*
 * Meta Editor - Project Serialization
 *
 * Save/Load projects to JSON format.
 */

#pragma once

#include <flex.h>
#include <string>

namespace meta_editor {

class Canvas;

class Serializer {
public:
    explicit Serializer(Canvas* canvas);

    // Save project to JSON file
    bool save(const std::string& path) const;

    // Load project from JSON file
    bool load(const std::string& path);

    // Serialize to JSON string
    std::string to_json() const;

    // Deserialize from JSON string
    bool from_json(const std::string& json);

private:
    std::string node_to_json(flex::Node* node, int indent) const;
    std::string shape_to_json(flex::Shape* shape, int indent) const;
    std::string text_to_json(flex::Text* text, int indent) const;
    std::string group_to_json(flex::Group* group, int indent) const;
    std::string color_to_json(const flex::Color& color) const;
    std::string indent_str(int level) const;

    flex::Node* json_to_node(const std::string& json, size_t& pos);
    flex::Shape* json_to_shape(const std::string& json, size_t& pos);
    flex::Text* json_to_text(const std::string& json, size_t& pos);
    flex::Group* json_to_group(const std::string& json, size_t& pos);

    // JSON parsing helpers
    std::string parse_string(const std::string& json, size_t& pos) const;
    double parse_number(const std::string& json, size_t& pos) const;
    bool parse_bool(const std::string& json, size_t& pos) const;
    void skip_whitespace(const std::string& json, size_t& pos) const;
    bool expect_char(const std::string& json, size_t& pos, char c) const;
    std::string parse_key(const std::string& json, size_t& pos) const;

    Canvas* canvas_;
};

} // namespace meta_editor
