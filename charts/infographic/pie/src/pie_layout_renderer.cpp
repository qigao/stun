#include <layout_renderer.h>
#include <render_helpers.h>
#include <sstream>
#include <cmath>

namespace flex::modules::infographic {

// Use double precision for angle calculations to avoid cumulative errors
static constexpr double PI = 3.14159265358979323846;

void PieLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    const double CX = 250, CY = 200, R = 120;
    
    double total = 0;
    for (const auto& item : info.items) {
        if (item->value) total += *item->value;
    }
    if (total <= 0) return;
    
    size_t idx = 0;
    for (const auto& item : info.items) {
        if (!item->value) continue;
        double start_angle = 0.0;
        double sweep = 0.0;
        double mid = 0.0;
        double radius = R;
        if (ctx.layout && ctx.layout->nodes.size() == info.items.size()) {
            const auto& node = ctx.layout->nodes[idx];
            start_angle = node.bounds.x;
            sweep = node.bounds.y;
            mid = node.bounds.height;
            radius = node.bounds.width;
        } else {
            double running = 0.0;
            for (size_t i = 0; i <= idx; ++i) {
                if (info.items[i]->value) running += *info.items[i]->value;
            }
            double prev = running - *item->value;
            start_angle = -PI / 2 + (prev / total) * 2 * PI;
            double end_angle = -PI / 2 + (running / total) * 2 * PI;
            sweep = end_angle - start_angle;
            mid = (start_angle + end_angle) / 2;
        }
        double end_angle = start_angle + sweep;
        
        std::ostringstream path;
        path.precision(10);
        path << "M " << CX << " " << CY;
        path << " L " << CX + radius * std::cos(start_angle) << " " << CY + radius * std::sin(start_angle);
        
        // Approximate arc with cubic bezier curves
        // For good approximation, we split into chunks of max PI/2
        double current_angle = start_angle;
        while (current_angle < end_angle - 1e-6) {
            double step = std::min(sweep, PI / 2.0);
            if (current_angle + step > end_angle) {
                step = end_angle - current_angle;
            }
            
            // k = 4/3 * tan(step/4)
            double k = (4.0/3.0) * std::tan(step/4.0) * radius;
            
            double x0 = CX + radius * std::cos(current_angle);
            double y0 = CY + radius * std::sin(current_angle);
            
            double x3 = CX + radius * std::cos(current_angle + step);
            double y3 = CY + radius * std::sin(current_angle + step);
            
            double cp1x = x0 - k * std::sin(current_angle);
            double cp1y = y0 + k * std::cos(current_angle);
            
            double cp2x = x3 + k * std::sin(current_angle + step); // Tangent direction
            double cp2y = y3 - k * std::cos(current_angle + step);
            
             // Derivative of cos is -sin. Tangent vector at angle theta is (-sin(theta), cos(theta)) 
             // We want +Tangent for p1 and -Tangent for p2
             
            path << " C " << cp1x << " " << cp1y << " " << cp2x << " " << cp2y << " " << x3 << " " << y3;
            
            current_angle += step;
        }
        
        path << " Z";
        
        auto slice = ctx.arena.create<flex::Shape>();
        slice->set_path(path.str());
        slice->set_fill(get_palette_color(info.theme, idx));
        ctx.root->add_child(slice);
        
        double lx = CX + (radius + 30) * std::cos(mid);
        double ly = CY + (radius + 30) * std::sin(mid);
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(item->label);
        label->set_font_size(12.0f);
        label->set_color(flex::Color(0.3f, 0.3f, 0.3f));
        label->set_position(static_cast<float>(lx), static_cast<float>(ly));
        label->set_anchor(flex::Anchor::Center);
        ctx.root->add_child(label);
        
        idx++;
    }
}

void DonutLayoutRenderer::render(const UnifiedInfographic& info, LayoutRenderContext& ctx) {
    const double CX = 250, CY = 200, R_OUTER = 120, R_INNER = 70;
    
    double total = 0;
    for (const auto& item : info.items) {
        if (item->value) total += *item->value;
    }
    if (total <= 0) return;
    
    size_t idx = 0;
    for (const auto& item : info.items) {
        if (!item->value) continue;
        double start_angle = 0.0;
        double sweep = 0.0;
        double mid = 0.0;
        double outer = R_OUTER;
        double inner = R_INNER;
        if (ctx.layout && ctx.layout->nodes.size() == info.items.size()) {
            const auto& node = ctx.layout->nodes[idx];
            start_angle = node.bounds.x;
            sweep = node.bounds.y;
            mid = node.bounds.height;
            outer = node.bounds.width;
            inner = std::max(10.0, outer - (R_OUTER - R_INNER));
        } else {
            double running = 0.0;
            for (size_t i = 0; i <= idx; ++i) {
                if (info.items[i]->value) running += *info.items[i]->value;
            }
            double prev = running - *item->value;
            start_angle = -PI / 2 + (prev / total) * 2 * PI;
            double end_angle = -PI / 2 + (running / total) * 2 * PI;
            sweep = end_angle - start_angle;
            mid = (start_angle + end_angle) / 2;
        }
        double end_angle = start_angle + sweep;
        
        double ox1 = CX + outer * std::cos(start_angle), oy1 = CY + outer * std::sin(start_angle);
        double ox2 = CX + outer * std::cos(end_angle), oy2 = CY + outer * std::sin(end_angle);
        double ix1 = CX + inner * std::cos(end_angle), iy1 = CY + inner * std::sin(end_angle);
        double ix2 = CX + inner * std::cos(start_angle), iy2 = CY + inner * std::sin(start_angle);
        
        std::ostringstream path;
        path.precision(10);
        path << "M " << ox1 << " " << oy1
             << " A " << outer << " " << outer << " 0 " << (sweep > PI ? 1 : 0) << " 1 " << ox2 << " " << oy2
             << " L " << ix1 << " " << iy1
             << " A " << inner << " " << inner << " 0 " << (sweep > PI ? 1 : 0) << " 0 " << ix2 << " " << iy2 << " Z";
        
        auto slice = ctx.arena.create<flex::Shape>();
        slice->set_path(path.str());
        slice->set_fill(get_palette_color(info.theme, idx));
        ctx.root->add_child(slice);
        
        double lx = CX + (outer + 25) * std::cos(mid);
        double ly = CY + (outer + 25) * std::sin(mid);
        auto label = ctx.arena.create<flex::Text>();
        label->set_content(item->label);
        label->set_font_size(11.0f);
        label->set_color(flex::Color(0.3f, 0.3f, 0.3f));
        label->set_position(static_cast<float>(lx), static_cast<float>(ly));
        label->set_anchor(flex::Anchor::Center);
        ctx.root->add_child(label);
        
        idx++;
    }
}

} // namespace flex::modules::infographic
