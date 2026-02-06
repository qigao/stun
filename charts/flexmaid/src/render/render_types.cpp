#include "render/render_types.h"

namespace flex {
namespace modules {
namespace flexmaid {

Theme Theme::light() {
    Theme t;
    t.background_color = "#ffffff";
    t.primary_color = "#0066cc";
    t.secondary_color = "#f4f4f4";
    t.text_color = "#333333";
    t.line_color = "#333333";
    t.font_family = "Arial, sans-serif";
    t.font_size = 14;
    t.line_width = 2.0f;
    return t;
}

Theme Theme::dark() {
    Theme t;
    t.background_color = "#1e1e1e";
    t.primary_color = "#3794ff";
    t.secondary_color = "#2d2d2d";
    t.text_color = "#e0e0e0";
    t.line_color = "#999999";
    t.font_family = "Arial, sans-serif";
    t.font_size = 14;
    t.line_width = 2.0f;
    return t;
}

Theme Theme::modern() {
    Theme t;
    t.background_color = "#f8f9fa";
    t.primary_color = "#4c6ef5";
    t.secondary_color = "#e9ecef";
    t.text_color = "#212529";
    t.line_color = "#495057";
    t.font_family = "Inter, system-ui, sans-serif";
    t.font_size = 14;
    t.line_width = 1.5f;
    return t;
}

Theme Theme::official() {
    return light();
}

} // namespace flexmaid
} // namespace modules
} // namespace flex
