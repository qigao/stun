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
#include <flexUI/detail/style_property_registry.h>
#include <flex/animation.h>
#include <flex/core/types.h>
#include <cmath>
#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <variant>
#include <utility>

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

inline float sample_bezier_x(float t, float x1, float x2) {
    return 3.0f * (1.0f - t) * (1.0f - t) * t * x1 + 3.0f * (1.0f - t) * t * t * x2 + t * t * t;
}

inline float sample_bezier_y(float t, float y1, float y2) {
    return 3.0f * (1.0f - t) * (1.0f - t) * t * y1 + 3.0f * (1.0f - t) * t * t * y2 + t * t * t;
}

inline float sample_bezier_derivative_x(float t, float x1, float x2) {
    return 3.0f * (1.0f - 3.0f * x2 + 3.0f * x1) * t * t + 6.0f * (x2 - 2.0f * x1) * t + 3.0f * x1;
}

inline float evaluate_cubic_bezier(float x1, float y1, float x2, float y2, float x) {
    float t = x;
    for (int i = 0; i < 8; ++i) {
        float x_est = sample_bezier_x(t, x1, x2) - x;
        if (std::abs(x_est) < 1e-6f) {
            break;
        }
        float dx = sample_bezier_derivative_x(t, x1, x2);
        if (std::abs(dx) < 1e-6f) {
            break;
        }
        t -= x_est / dx;
    }
    if (t < 0.0f || t > 1.0f) {
        float low = 0.0f, high = 1.0f;
        t = x;
        for (int i = 0; i < 12; ++i) {
            float x_est = sample_bezier_x(t, x1, x2);
            if (std::abs(x_est - x) < 1e-5f) {
                break;
            }
            if (x_est < x) {
                low = t;
            } else {
                high = t;
            }
            t = (low + high) * 0.5f;
        }
    }
    return sample_bezier_y(t, y1, y2);
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

struct AnimationKeyframeStep {
    float offset = 0.0f;
    std::map<std::string, std::string> properties;
};

struct AnimationDef {
    float duration_ms = 0.0f;
    float delay_ms = 0.0f;
    EasingType easing = EasingType::Ease;
    float iteration_count = 1.0f;
    bool infinite = false;
    AnimationFillMode fill_mode = AnimationFillMode::None;
    AnimationDirection direction = AnimationDirection::Normal;
    AnimationPlayState play_state = AnimationPlayState::Running;
};

struct AnimationValuePoint {
    float offset = 0.0f;
    float value = 0.0f;
};

/**
 * Parse CSS transition shorthand
 * Examples:
 *   "opacity 0.3s ease"
 *   "all 0.5s ease-in-out 0.1s"
 *   "transform 200ms linear"
 */
TransitionDef parse_transition(const std::string& value);

/**
 * Parse a comma-separated CSS transition list
 * Examples:
 *   "opacity 0.3s ease, transform 200ms linear"
 *   "all 0.5s ease-in-out 0.1s, opacity 100ms linear"
 */
std::vector<TransitionDef> parse_transition_list(const std::string& value);

// ============================================================================
// Active Transition State
// ============================================================================

struct TransitionValue {
    const cmeta_type_desc* type = nullptr;
    flex::AnimValue value{};

    bool has_value() const noexcept { return type != nullptr; }
};

struct TransitionValueOps {
    const cmeta_type_desc* type = nullptr;
    bool (*equal)(const void*, const void*) = nullptr;
    bool (*interpolate)(const void*, const void*, float, void*) = nullptr;
};

const TransitionValueOps* transition_type_ops(
    const cmeta_type_desc* type) noexcept;

struct ActiveTransition {
    const detail::StylePropertyDesc* property = nullptr;
    TransitionValue start_value{};
    TransitionValue end_value{};
    const TransitionValueOps* ops = nullptr;
    float start_time_ms = 0.0f;
    float duration_ms = 0.0f;
    float delay_ms = 0.0f;
    easing::EasingFunction easing_fn = easing::ease_in_out;
    EasingType easing_type = EasingType::Ease;
    float bezier[4] = {0.25f, 0.1f, 0.25f, 1.0f};

    TransitionValue current_value(float time_ms) const;
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
    void start(std::uintptr_t element_id, const std::string& property,
               float from, float to, const TransitionDef& def, float current_time_ms);

    bool start_typed(std::uintptr_t element_id,
                     const detail::StylePropertyDesc& property,
                     const TransitionValue& from,
                     const TransitionValue& to,
                     const TransitionDef& def,
                     float current_time_ms);

    bool start_float(std::uintptr_t element_id,
                     const detail::StylePropertyDesc& property,
                     float previous_value,
                     float target_value,
                     const TransitionDef& def,
                     float current_time_ms);

    bool start_color(std::uintptr_t element_id,
                     const detail::StylePropertyDesc& property,
                     const Color& previous_value,
                     const Color& target_value,
                     const TransitionDef& def,
                     float current_time_ms);

    /**
     * Get current value for a property (interpolated if transitioning)
     */
    float get(std::uintptr_t element_id, const std::string& property,
              float default_value, float current_time_ms);

    TransitionValue get_typed(std::uintptr_t element_id,
                              const detail::StylePropertyDesc& property,
                              const TransitionValue& default_value,
                              float current_time_ms) const;

    float get_float(std::uintptr_t element_id,
                    const detail::StylePropertyDesc& property,
                    float default_value,
                    float current_time_ms) const;

    Color get_color(std::uintptr_t element_id,
                    const detail::StylePropertyDesc& property,
                    const Color& default_value,
                    float current_time_ms) const;

    /**
     * Check if element has active transitions
     */
    bool has_active(std::uintptr_t element_id, float current_time_ms);

    /**
     * Union StylePropertyImpact metadata for active typed transitions on one
     * element. The manager remains UI-object agnostic; Box/Element consume the
     * returned impact.
     */
    std::uint32_t active_impact(std::uintptr_t element_id,
                                float current_time_ms) const noexcept;

    /**
     * Update and remove completed transitions
     */
    void update(float current_time_ms);

    /**
     * Check if any transitions are active
     */
    bool has_any_active() const { return !transitions_.empty(); }
    void clear_element(std::uintptr_t element_id);

    /**
     * Clear all transitions
     */
    void clear() { transitions_.clear(); }

private:
    using TypedTransitionKey =
        std::pair<std::uintptr_t, detail::StylePropertyId>;

    std::map<TypedTransitionKey, ActiveTransition> transitions_;
};

struct TypedAnimationPoint {
    float offset = 0.0f;
    TransitionValue value{};
};

struct TypedActiveAnimation {
    const detail::StylePropertyDesc* property = nullptr;
    std::vector<TypedAnimationPoint> keyframes;
    const TransitionValueOps* ops = nullptr;
    float start_time_ms = 0.0f;
    float duration_ms = 0.0f;
    float delay_ms = 0.0f;
    float iteration_count = 1.0f;
    bool infinite = false;
    AnimationFillMode fill_mode = AnimationFillMode::None;
    AnimationDirection direction = AnimationDirection::Normal;
    AnimationPlayState play_state = AnimationPlayState::Running;
    bool paused = false;
    float paused_at_ms = 0.0f;
    float total_paused_ms = 0.0f;
    easing::EasingFunction easing_fn = easing::ease_in_out;

    TransitionValue current_value(const TransitionValue& default_value,
                                  float time_ms) const;
    bool is_active(float time_ms) const;
    bool retains_fill_value() const;
    void set_paused(bool should_pause, float current_time_ms);
};

struct ActiveAnimation {
    std::string property;
    std::vector<AnimationValuePoint> keyframes;
    float start_time_ms = 0.0f;
    float duration_ms = 0.0f;
    float delay_ms = 0.0f;
    float iteration_count = 1.0f;
    bool infinite = false;
    AnimationFillMode fill_mode = AnimationFillMode::None;
    AnimationDirection direction = AnimationDirection::Normal;
    AnimationPlayState play_state = AnimationPlayState::Running;
    bool paused = false;
    float paused_at_ms = 0.0f;
    float total_paused_ms = 0.0f;
    easing::EasingFunction easing_fn = easing::ease_in_out;

    float current_value(float default_value, float time_ms) const;
    bool is_active(float time_ms) const;
    bool retains_fill_value() const;
    void set_paused(bool should_pause, float current_time_ms);
};

class AnimationManager {
public:
    void start(std::uintptr_t element_id, const std::string& property,
               const std::vector<AnimationValuePoint>& keyframes,
               const AnimationDef& def, float current_time_ms);

    bool start_typed(std::uintptr_t element_id,
                     const detail::StylePropertyDesc& property,
                     const std::vector<TypedAnimationPoint>& keyframes,
                     const AnimationDef& def,
                     float current_time_ms);

    bool start_float(std::uintptr_t element_id,
                     const detail::StylePropertyDesc& property,
                     const std::vector<AnimationValuePoint>& keyframes,
                     const AnimationDef& def,
                     float current_time_ms);

    TransitionValue get_typed(std::uintptr_t element_id,
                              const detail::StylePropertyDesc& property,
                              const TransitionValue& default_value,
                              float current_time_ms) const;

    float get_float(std::uintptr_t element_id,
                    const detail::StylePropertyDesc& property,
                    float default_value,
                    float current_time_ms) const;

    Color get_color(std::uintptr_t element_id,
                    const detail::StylePropertyDesc& property,
                    const Color& default_value,
                    float current_time_ms) const;

    float get(std::uintptr_t element_id, const std::string& property,
              float default_value, float current_time_ms) const;

    bool has_active(std::uintptr_t element_id, float current_time_ms) const;
    std::uint32_t active_impact(std::uintptr_t element_id,
                                float current_time_ms) const noexcept;
    bool has_effect(std::uintptr_t element_id, float current_time_ms) const;
    bool has_any_effects() const {
        return !animations_.empty() || !typed_animations_.empty();
    }
    bool has_any_active(float current_time_ms) const;
    void set_play_state(std::uintptr_t element_id, AnimationPlayState play_state,
                        float current_time_ms);
    void clear_element(std::uintptr_t element_id);
    void update(float current_time_ms);
    void clear() {
        animations_.clear();
        typed_animations_.clear();
    }

private:
    using TypedAnimationKey =
        std::pair<std::uintptr_t, detail::StylePropertyId>;

    std::map<std::string, ActiveAnimation> animations_;
    std::map<TypedAnimationKey, TypedActiveAnimation> typed_animations_;
};

} // namespace flexUI

#endif // FLEXUI_TRANSITION_H
