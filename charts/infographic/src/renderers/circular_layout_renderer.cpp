#include <layout_renderer.h>
#include <render_helpers.h>
#include <sstream>
#include <cmath>
#ifndef M_PI
  #define M_PI 3.14159265358979323846264f
#endif

namespace flex::modules::infographic {

void CircularLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    const float CX = 280, CY = 220, R = 130, NODE_R = 35;
    const float ICON_SIZE = 20;
    size_t n = info.items.size();
    if (n == 0) return;
    
    for (size_t i = 0; i < n; ++i) {
        float angle = 2 * M_PI * i / n - M_PI / 2;
        float x = CX + R * std::cos(angle);
        float y = CY + R * std::sin(angle);
        
        size_t next = (i + 1) % n;
        float next_angle = 2 * M_PI * next / n - M_PI / 2;
        float nx = CX + R * std::cos(next_angle);
        float ny = CY + R * std::sin(next_angle);
        
        std::ostringstream path;
        path << "M " << x << " " << y << " A " << R << " " << R << " 0 0 1 " << nx << " " << ny;
        auto arc = ctx.arena.create<flex::Shape>();
        arc->set_path(path.str());
        arc->set_stroke(flex::Color(0.75f, 0.75f, 0.75f), 2.0f);
        ctx.root->add_child(arc);
        
        auto circle = ctx.arena.create<flex::Shape>();
        circle->set_circle(NODE_R);
        circle->set_fill(get_palette_color(info.theme, i));
        circle->set_position(x, y);
        ctx.root->add_child(circle);
        
        auto& item = info.items[i];
        if (item->icon && has_icon(*item->icon)) {
            auto icon = create_icon(*item->icon, ICON_SIZE, "#ffffff", ctx.arena);
            if (icon) {
                icon->set_position(x - ICON_SIZE / 2, y - ICON_SIZE / 2 - 8);
                ctx.root->add_child(icon);
            }
        }
        
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(item->label);
        label->set_font_size(11.0f);
        label->set_color(flex::Color(1, 1, 1));
        label->set_position(x, y + (item->icon ? 8 : 0));
        label->set_anchor(flex::Anchor::Center);
        ctx.root->add_child(label);
    }
}

} // namespace flex::modules::infographic
