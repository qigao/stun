#include <render_helpers.h>
#include <fstream>
#include <sstream>

namespace flex::modules::infographic {

static std::string s_illus_dir = "assets/illus/";

void set_illus_directory(const std::string& dir) { s_illus_dir = dir; }
const std::string& get_illus_directory() { return s_illus_dir; }

flex::Color to_color(const std::string& hex) {
    if (hex.empty() || hex[0] != '#') return flex::Color(0.2f, 0.4f, 0.8f);
    uint32_t val = std::stoul(hex.substr(1), nullptr, 16);
    return flex::Color(((val >> 16) & 0xFF) / 255.0f,
                       ((val >> 8) & 0xFF) / 255.0f,
                       (val & 0xFF) / 255.0f);
}

flex::Color get_palette_color(const Theme& theme, size_t index) {
    if (theme.palette.empty()) {
        static const char* defaults[] = {"#3498db", "#2ecc71", "#e74c3c", "#f39c12", "#9b59b6", "#1abc9c"};
        return to_color(defaults[index % 6]);
    }
    return to_color(theme.palette[index % theme.palette.size()]);
}

flex::Svg* create_icon(const std::string& icon_name, float size, const std::string& color, 
                       flex::ArenaAllocator& arena) {
    if (!has_icon(icon_name)) return nullptr;
    
    auto svg = arena.create<flex::Svg>();
    svg->set_data(get_icon_svg(icon_name, size, color));
    svg->set_width(size);
    svg->set_height(size);
    return svg;
}

static std::string load_illus_svg(const std::string& illus_name) {
    std::string path = s_illus_dir + illus_name + ".svg";
    std::ifstream file(path);
    if (!file) return "";
    std::stringstream buf;
    buf << file.rdbuf();
    return buf.str();
}

flex::Node* create_illus(const std::string& illus_name, float w, float h, 
                         flex::ArenaAllocator& arena) {
    std::string svg_data = load_illus_svg(illus_name);
    
    if (!svg_data.empty()) {
        auto svg = arena.create<flex::Svg>();
        svg->set_data(svg_data);
        svg->set_width(w);
        svg->set_height(h);
        return svg;
    }
    
    // Placeholder
    auto group = flex::Group::create(arena);
    
    auto bg = arena.create<flex::Shape>();
    bg->set_rect(w, h, 8.0f);
    bg->set_fill(flex::Color(0.9f, 0.9f, 0.9f));
    bg->set_stroke(flex::Color(0.7f, 0.7f, 0.7f), 1.0f);
    group->add_child(bg);
    
    auto label = arena.create<flex::Text>();
    label->set_content("[" + illus_name + "]");
    label->set_font_size(10.0f);
    label->set_color(flex::Color(0.5f, 0.5f, 0.5f));
    label->set_position(w / 2, h / 2);
    label->set_anchor(flex::Anchor::Center);
    group->add_child(label);
    
    return group;
}

} // namespace flex::modules::infographic
