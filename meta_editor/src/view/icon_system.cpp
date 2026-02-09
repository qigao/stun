#include "meta_editor/view/icon_system.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cstring>

using json = nlohmann::json;

namespace meta_editor {

static std::vector<std::string> extract_paths(const std::string& body) {
    std::vector<std::string> paths;
    size_t pos = 0;
    while ((pos = body.find(" d=\"", pos)) != std::string::npos) {
        pos += 4; // skip ' d="'
        size_t end = body.find('"', pos);
        if (end == std::string::npos) break;
        paths.push_back(body.substr(pos, end - pos));
        pos = end;
    }
    return paths;
}

bool IconSystem::load_json(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    try {
        json j;
        file >> j;

        if (!j.contains("icons")) return false;

        auto& icons_obj = j["icons"];
        int default_width = 100;
        int default_height = 100;

        if (j.contains("info") && j["info"].contains("height")) {
            default_height = j["info"]["height"].get<int>();
            default_width = default_height;
        }

        for (auto it = icons_obj.begin(); it != icons_obj.end(); ++it) {
            auto& val = it.value();
            if (!val.contains("body")) continue;

            IconData data;
            data.paths = extract_paths(val["body"].get<std::string>());
            data.width = val.value("width", default_width);
            data.height = val.value("height", default_height);
            if (!data.paths.empty())
                icons_[it.key()] = std::move(data);
        }

        // Inline aliases (parent inside icons object)
        for (auto it = icons_obj.begin(); it != icons_obj.end(); ++it) {
            auto& val = it.value();
            if (val.contains("body") || !val.contains("parent")) continue;
            auto parent = icons_.find(val["parent"].get<std::string>());
            if (parent != icons_.end())
                icons_[it.key()] = parent->second;
        }

        // Top-level aliases section
        if (j.contains("aliases")) {
            for (auto it = j["aliases"].begin(); it != j["aliases"].end(); ++it) {
                auto& val = it.value();
                if (!val.contains("parent")) continue;
                auto parent = icons_.find(val["parent"].get<std::string>());
                if (parent != icons_.end())
                    icons_[it.key()] = parent->second;
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

void IconSystem::render_icon(flex::Renderer& r, const std::string& name,
                             float x, float y, float size, const flex::Color& color) {
    auto it = icons_.find(name);
    if (it == icons_.end()) {
        fprintf(stderr, "Icon not found: %s\n", name.c_str());
        return;
    }

    const auto& data = it->second;
    float scale = size / (float)data.width;

    r.save();
    r.translate(x, y);
    r.scale(scale, scale);

    auto paint = flex::Paint::solid(color);
    for (const auto& d : data.paths)
        r.fill_path(d, paint);

    r.restore();
}

bool IconSystem::has_icon(const std::string& name) const {
    return icons_.find(name) != icons_.end();
}

} // namespace meta_editor
