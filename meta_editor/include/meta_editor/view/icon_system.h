#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <flex/runtime/renderer.h>

namespace meta_editor {

/**
 * IconSystem - Manages loading and rendering SVG icons from JSON files.
 * Supports gis.json and material-symbols.json formats.
 */
class IconSystem {
public:
    IconSystem() = default;

    // Load icons from a JSON file (e.g., gis.json)
    bool load_json(const std::string& path);

    // Render an icon by name at the given position and size
    void render_icon(flex::Renderer& r, const std::string& name, float x, float y, float size, 
                     const flex::Color& color = {1.0f, 1.0f, 1.0f, 1.0f});

    // Check if an icon exists
    bool has_icon(const std::string& name) const;

private:
    struct IconData {
        std::vector<std::string> paths;  // pre-extracted SVG path 'd' attributes
        int width = 100;
        int height = 100;
    };

    std::unordered_map<std::string, IconData> icons_;
};

} // namespace meta_editor
