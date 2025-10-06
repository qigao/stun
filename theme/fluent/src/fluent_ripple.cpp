#include <nanogui/fluent_ripple.h>
#include <chrono>
#include <nanogui/fluent_easing.h>
#include <nanogui/opengl.h>
#include <cmath>

NAMESPACE_BEGIN(nanogui)

void FluentRipple::start(const Vector2f &pos, const Vector2f &bounds) {
    // Calculate max radius to cover the entire bounds from click position
    float dx1 = pos.x();
    float dx2 = bounds.x() - pos.x();
    float dy1 = pos.y();
    float dy2 = bounds.y() - pos.y();
    
    float max_radius = std::sqrt(
        std::max(dx1*dx1 + dy1*dy1, std::max(dx2*dx2 + dy2*dy2,
        std::max(dx1*dx1 + dy2*dy2, dx2*dx2 + dy1*dy1)))
    );
    
    m_ripples.emplace_back(pos, max_radius);
}

void FluentRipple::draw(NVGcontext *ctx, const Vector2i &widget_pos) {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = m_ripples.begin(); it != m_ripples.end();) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - it->start_time).count();
        
        float progress = std::min(1.f, elapsed / (float)m_duration);
        
        // Remove completed ripples
        if (progress >= 1.f) {
            it = m_ripples.erase(it);
            continue;
        }
        
        // Apply easing for natural expansion
        float eased = FluentEasing::ease(FluentEasing::Curve::Decelerated, progress);
        float radius = it->max_radius * eased;
        
        // Fade out in the last 40% of animation
        float alpha = progress < 0.6f ? 1.f : (1.f - (progress - 0.6f) / 0.4f);
        
        // Draw ripple
        nvgBeginPath(ctx);
        nvgCircle(ctx, 
                  widget_pos.x() + it->origin.x(), 
                  widget_pos.y() + it->origin.y(), 
                  radius);
        
        Color ripple_color = m_color;
        ripple_color[3] *= alpha;
        nvgFillColor(ctx, ripple_color);
        nvgFill(ctx);
        
        ++it;
    }
}

NAMESPACE_END(nanogui)
