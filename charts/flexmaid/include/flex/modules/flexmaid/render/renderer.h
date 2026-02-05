#pragma once

#include "../layout/layout_engine.h"
#include <string>

namespace flex::modules::flexmaid {

struct Theme {
    std::string background = "#FFFFFF";
    std::string primary_color = "#F8FAFF";
    std::string primary_text_color = "#1C2430";
    std::string primary_border_color = "#C7D2E5";
    std::string line_color = "#7A8AA6";
    std::string secondary_color = "#F0F4FF";
    std::string tertiary_color = "#E8EEFF";
    std::string edge_label_background = "#FFFFFF";
    std::string cluster_background = "#F8FAFF";
    std::string cluster_border = "#C7D2E5";
    
    std::string font_family = "Inter, system-ui, sans-serif";
    float font_size = 14.0f;
    
    // Sequence diagram colors
    std::string sequence_actor_fill = "#F8FAFF";
    std::string sequence_actor_border = "#C7D2E5";
    std::string sequence_actor_line = "#7A8AA6";
    std::string sequence_activation_fill = "#E8EEFF";
    std::string sequence_activation_border = "#C7D2E5";
    std::string sequence_note_fill = "#FFF8DC";
    std::string sequence_note_border = "#C7D2E5";
    
    static Theme modern();
    static Theme mermaid_default();
};

class Renderer {
public:
    virtual ~Renderer() = default;
    
    /// Render diagram layout
    virtual void render(const DiagramLayout& layout, const Theme& theme) = 0;
    
    /// Save as SVG
    virtual bool save_svg(const std::string& path) = 0;
    
    /// Save as PNG
    virtual bool save_png(const std::string& path, int width, int height) = 0;
    
    /// Get rendered content as SVG string
    virtual std::string to_svg_string() = 0;
};

} // namespace flex::modules::flexmaid
