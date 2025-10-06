#pragma once

#include <nanogui/common.h>
#include <nanogui/vector.h>
#include <chrono>
#include <vector>

NAMESPACE_BEGIN(nanogui)

/**
 * @brief Fluent Design Ripple Effect
 * 
 * Provides touch feedback with expanding circular ripples.
 * Can be used standalone or integrated into interactive components.
 */
class NANOGUI_EXPORT FluentRipple {
public:
    struct Ripple {
        Vector2f origin;
        std::chrono::steady_clock::time_point start_time;
        float max_radius;
        bool fading;
        
        Ripple(const Vector2f &pos, float radius) 
            : origin(pos), start_time(std::chrono::steady_clock::now()),
              max_radius(radius), fading(false) {}
    };
    
    FluentRipple() : m_color(1.f, 1.f, 1.f, 0.3f), m_duration(600) {}
    
    /// Start a new ripple at the given position
    void start(const Vector2f &pos, const Vector2f &bounds);
    
    /// Update and draw all active ripples
    void draw(NVGcontext *ctx, const Vector2i &widget_pos);
    
    /// Check if any ripples are active
    bool is_animating() const { return !m_ripples.empty(); }
    
    /// Clear all ripples
    void clear() { m_ripples.clear(); }
    
    /// Set ripple color
    void set_color(const Color &color) { m_color = color; }
    
    /// Set animation duration in milliseconds
    void set_duration(int ms) { m_duration = ms; }
    
protected:
    std::vector<Ripple> m_ripples;
    Color m_color;
    int m_duration;
};

NAMESPACE_END(nanogui)
