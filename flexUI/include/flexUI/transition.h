/*
 * flexUI - CSS Transitions
 *
 * Supports CSS transition property for smooth animations:
 * - opacity, transform, background-color, etc.
 * - Easing functions: linear, ease, ease-in, ease-out, ease-in-out
 */

#ifndef FLEXUI_TRANSITION_H
#define FLEXUI_TRANSITION_H

#include <flexUI/types.h>
#include <cmath>
#include <string>
#include <map>
#include <vector>
#include <functional>

namespace flexUI {

// ============================================================================
// Easing Functions
// ============================================================================

namespace easing {

inline float linear(float t) { return t; }

inline float ease_in(float t) { return t * t; }

inline float ease_out(float t) { return t * (2.0f - t); }

inline float ease_in_out(float t) {
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

inline float ease_in_cubic(float t) { return t * t * t; }

inline float ease_out_cubic(float t) {
    float t1 = t - 1.0f;
    return t1 * t1 * t1 + 1.0f;
}

inline float ease_in_out_cubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

inline float ease_out_bounce(float t) {
    if (t < 1.0f / 2.75f) {
        return 7.5625f * t * t;
    } else if (t < 2.0f / 2.75f) {
        t -= 1.5f / 2.75f;
        return 7.5625f * t * t + 0.75f;
    } else if (t < 2.5f / 2.75f) {
        t -= 2.25f / 2.75f;
        return 7.5625f * t * t + 0.9375f;
    } else {
        t -= 2.625f / 2.75f;
        return 7.5625f * t * t + 0.984375f;
    }
}

using EasingFunction = float (*)(float);

inline EasingFunction get(EasingType type) {
    switch (type) {
        case EasingType::Linear: return linear;
        case EasingType::Ease: return ease_in_out;
        case EasingType::EaseIn: return ease_in;
        case EasingType::EaseOut: return ease_out;
        case EasingType::EaseInOut: return ease_in_out;
        case EasingType::CubicBezier: return ease_in_out;  // Fallback
        default: return ease_in_out;
    }
}

inline EasingFunction get(const std::string& name) {
    if (name == "linear") return linear;
    if (name == "ease") return ease_in_out;
    if (name == "ease-in") return ease_in;
    if (name == "ease-out") return ease_out;
    if (name == "ease-in-out") return ease_in_out;
    if (name == "ease-in-cubic") return ease_in_cubic;
    if (name == "ease-out-cubic") return ease_out_cubic;
    if (name == "ease-in-out-cubic") return ease_in_out_cubic;
    if (name == "ease-out-bounce") return ease_out_bounce;
    return ease_in_out;  // Default
}

} // namespace easing

// ============================================================================
// Transition Definition
// ============================================================================

struct TransitionDef {
    std::string property = "all";
    float duration_ms = 300.0f;
    float delay_ms = 0.0f;
    EasingType easing = EasingType::Ease;
    float bezier[4] = {0.25f, 0.1f, 0.25f, 1.0f};
};

/**
 * Parse CSS transition shorthand
 * Examples:
 *   "opacity 0.3s ease"
 *   "all 0.5s ease-in-out 0.1s"
 *   "transform 200ms linear"
 */
TransitionDef parse_transition(const std::string& value);

// ============================================================================
// Active Transition State
// ============================================================================

struct ActiveTransition {
    std::string property;
    float start_value;
    float end_value;
    float start_time_ms;
    float duration_ms;
    float delay_ms;
    easing::EasingFunction easing_fn;

    float current_value(float time_ms) const {
        float elapsed = time_ms - start_time_ms - delay_ms;
        if (elapsed < 0) return start_value;
        if (elapsed >= duration_ms) return end_value;
        float t = elapsed / duration_ms;
        float eased_t = easing_fn(t);
        return start_value + (end_value - start_value) * eased_t;
    }

    bool is_complete(float time_ms) const {
        return (time_ms - start_time_ms - delay_ms) >= duration_ms;
    }
};

// ============================================================================
// Transition Manager
// ============================================================================

class TransitionManager {
public:
    /**
     * Start a transition for an element property
     */
    void start(int element_id, const std::string& property,
               float from, float to, const TransitionDef& def, float current_time_ms);

    /**
     * Get current value for a property (interpolated if transitioning)
     */
    float get(int element_id, const std::string& property,
              float default_value, float current_time_ms);

    /**
     * Check if element has active transitions
     */
    bool has_active(int element_id, float current_time_ms);

    /**
     * Update and remove completed transitions
     */
    void update(float current_time_ms);

    /**
     * Check if any transitions are active
     */
    bool has_any_active() const { return !transitions_.empty(); }

    /**
     * Clear all transitions
     */
    void clear() { transitions_.clear(); }

private:
    std::map<std::string, ActiveTransition> transitions_;
};

} // namespace flexUI

#endif // FLEXUI_TRANSITION_H
