#pragma once

#include <string>
#include <unordered_map>

namespace flex::modules::flexmaid {

struct IconData {
    std::string body;  // SVG path body
    int width = 24;
    int height = 24;
};

class IconManager {
public:
    static IconManager& instance();
    
    void load_from_json(const std::string& json_path);
    void load_builtin();
    
    const IconData* get(const std::string& name) const;
    std::string render_svg(const std::string& name, float x, float y, float size, const std::string& fill = "#ffffff") const;

private:
    IconManager() { load_builtin(); }
    std::unordered_map<std::string, IconData> icons_;
};

} // namespace flex::modules::flexmaid
