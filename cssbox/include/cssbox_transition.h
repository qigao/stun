/*
 * CSS Transition Manager
 *
 * Manages active CSS transitions for elements.
 * Tracks property changes, interpolates values, and updates styles.
 *
 * Usage:
 *   TransitionManager manager;
 *   manager.update(dt);  // Call each frame with delta time in seconds
 *   manager.on_property_change(element, property, old_value, new_value);
 */

#ifndef CSSBOX_TRANSITION_H
#define CSSBOX_TRANSITION_H

#include "cssbox_types.h"
#include "cssbox_conversion.h"
#include <unordered_map>
#include <functional>
#include <cmath>
#include <algorithm>

namespace cssbox {

// Forward declaration
struct cssboxElement;

// ============================================================================
// Keyframe Conversion (string-based to typed)
// ============================================================================

/**
 * @brief Convert a string-based keyframe to typed Keyframe
 *
 * Bridges the legacy string-based Keyframe (from cssbox_internal.h)
 * to the typed Keyframe (from cssbox_types.h)
 * Uses re2c DFA lexer for property name matching.
 */
inline Keyframe convert_keyframe(float position, const std::map<std::string, std::string>& properties) {
    Keyframe kf(position);

    for (const auto& [name, value] : properties) {
        switch (fast::parse_keyframe_property(name)) {
            case CSS_KF_PROP_OPACITY:
                kf.opacity = std::strtof(value.c_str(), nullptr);
                break;
            case CSS_KF_PROP_BACKGROUND_COLOR:
            case CSS_KF_PROP_BACKGROUND:
                kf.background_color = convert::parse_color(value);
                break;
            case CSS_KF_PROP_COLOR:
                kf.color = convert::parse_color(value);
                break;
            case CSS_KF_PROP_TRANSFORM:
                kf.transform = convert::parse_transform(value);
                break;
            case CSS_KF_PROP_WIDTH:
                kf.width = convert::parse_length(value);
                break;
            case CSS_KF_PROP_HEIGHT:
                kf.height = convert::parse_length(value);
                break;
            case CSS_KF_PROP_BORDER_RADIUS: {
                auto radii = convert::parse_box_sides(value);
                kf.border_radius = std::array<float, 4>{
                    radii[0].value, radii[1].value, radii[2].value, radii[3].value
                };
                break;
            }
            case CSS_KF_PROP_BORDER_COLOR:
                if (auto c = convert::parse_color(value)) {
                    kf.border_color = *c;
                }
                break;
            case CSS_KF_PROP_BORDER_WIDTH:
                if (auto len = convert::parse_length(value)) {
                    kf.border_width = len->value;
                }
                break;
            default:
                break;
        }
    }

    return kf;
}

/**
 * @brief Convert legacy KeyframeAnimation to typed KeyframesDefinition
 */
inline KeyframesDefinition convert_keyframe_animation(
    const std::string& name,
    const std::vector<std::pair<float, std::map<std::string, std::string>>>& keyframes
) {
    KeyframesDefinition def(name);
    for (const auto& [position, properties] : keyframes) {
        def.add_keyframe(convert_keyframe(position, properties));
    }
    return def;
}

// ============================================================================
// Property Value Variant - For storing animated values
// ============================================================================

struct AnimatedValue {
    enum class Type : uint8_t {
        NONE,
        FLOAT,
        LENGTH,
        COLOR,
        TRANSFORM
    };

    Type type = Type::NONE;

    union {
        float f;
        struct { LengthUnit unit; float value; } length;
        struct { float r, g, b, a; } color;
    } data;

    Transform transform_data;  // Can't be in union due to non-trivial type

    AnimatedValue() : type(Type::NONE) { data.f = 0; }

    static AnimatedValue from_float(float v) {
        AnimatedValue av;
        av.type = Type::FLOAT;
        av.data.f = v;
        return av;
    }

    static AnimatedValue from_length(const Length& l) {
        AnimatedValue av;
        av.type = Type::LENGTH;
        av.data.length.unit = l.unit;
        av.data.length.value = l.value;
        return av;
    }

    static AnimatedValue from_color(const Color& c) {
        AnimatedValue av;
        av.type = Type::COLOR;
        av.data.color.r = c.r;
        av.data.color.g = c.g;
        av.data.color.b = c.b;
        av.data.color.a = c.a;
        return av;
    }

    static AnimatedValue from_transform(const Transform& t) {
        AnimatedValue av;
        av.type = Type::TRANSFORM;
        av.transform_data = t;
        return av;
    }

    float as_float() const { return data.f; }

    Length as_length() const {
        return {data.length.unit, data.length.value};
    }

    Color as_color() const {
        return nvgRGBAf(data.color.r, data.color.g, data.color.b, data.color.a);
    }

    const Transform& as_transform() const { return transform_data; }
};

// ============================================================================
// Active Transition - Runtime state for a single animating property
// ============================================================================

struct ActiveTransition {
    TransitionProperty property;
    AnimatedValue start_value;
    AnimatedValue end_value;
    TimingFunction timing;

    float duration;           // Total duration in seconds
    float delay;              // Delay before starting
    float elapsed;            // Time elapsed since trigger (including delay)

    bool is_active() const {
        return elapsed < (delay + duration);
    }

    bool is_started() const {
        return elapsed >= delay;
    }

    float progress() const {
        if (elapsed < delay) return 0.0f;
        float t = (elapsed - delay) / duration;
        return std::min(1.0f, std::max(0.0f, t));
    }

    float eased_progress() const {
        return timing.evaluate(progress());
    }
};

// ============================================================================
// Interpolation Functions
// ============================================================================

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

inline Color lerp_color(const Color& a, const Color& b, float t) {
    return nvgRGBAf(
        lerp(a.r, b.r, t),
        lerp(a.g, b.g, t),
        lerp(a.b, b.b, t),
        lerp(a.a, b.a, t)
    );
}

inline Length lerp_length(const Length& a, const Length& b, float t) {
    // Can only interpolate same units; otherwise snap at 50%
    if (a.unit != b.unit) {
        return (t < 0.5f) ? a : b;
    }
    return {a.unit, lerp(a.value, b.value, t)};
}

inline AnimatedValue interpolate(const AnimatedValue& a, const AnimatedValue& b, float t) {
    if (a.type != b.type) {
        return (t < 0.5f) ? a : b;
    }

    switch (a.type) {
        case AnimatedValue::Type::FLOAT:
            return AnimatedValue::from_float(lerp(a.data.f, b.data.f, t));

        case AnimatedValue::Type::LENGTH:
            return AnimatedValue::from_length(lerp_length(a.as_length(), b.as_length(), t));

        case AnimatedValue::Type::COLOR:
            return AnimatedValue::from_color(lerp_color(a.as_color(), b.as_color(), t));

        case AnimatedValue::Type::TRANSFORM: {
            // Interpolate transform matrices
            float m1[6], m2[6], result[6];
            a.transform_data.compose_matrix(m1);
            b.transform_data.compose_matrix(m2);
            for (int i = 0; i < 6; i++) {
                result[i] = lerp(m1[i], m2[i], t);
            }
            // Create transform from interpolated matrix
            Transform tr;
            tr.functions.push_back(TransformFunction::matrix(
                result[0], result[1], result[2], result[3], result[4], result[5]
            ));
            return AnimatedValue::from_transform(tr);
        }

        default:
            return a;
    }
}

// ============================================================================
// Transition Manager
// ============================================================================

class TransitionManager {
public:
    using ElementId = uint64_t;

    TransitionManager() = default;

    /**
     * @brief Update all active transitions
     * @param dt Delta time in seconds since last frame
     */
    void update(float dt) {
        for (auto& [element_id, transitions] : active_transitions_) {
            for (auto it = transitions.begin(); it != transitions.end(); ) {
                it->elapsed += dt;
                if (!it->is_active()) {
                    // Transition complete - apply final value
                    if (on_transition_end_) {
                        on_transition_end_(element_id, it->property, it->end_value);
                    }
                    it = transitions.erase(it);
                } else {
                    // Transition in progress - apply interpolated value
                    if (on_transition_update_) {
                        float t = it->eased_progress();
                        AnimatedValue current = interpolate(it->start_value, it->end_value, t);
                        on_transition_update_(element_id, it->property, current);
                    }
                    ++it;
                }
            }
        }

        // Clean up empty entries
        for (auto it = active_transitions_.begin(); it != active_transitions_.end(); ) {
            if (it->second.empty()) {
                it = active_transitions_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief Start a transition when a property changes
     */
    void start_transition(
        ElementId element_id,
        TransitionProperty property,
        const AnimatedValue& start,
        const AnimatedValue& end,
        const Transition& transition
    ) {
        if (transition.duration <= 0) {
            // No transition - apply immediately
            if (on_transition_end_) {
                on_transition_end_(element_id, property, end);
            }
            return;
        }

        // Cancel existing transition for this property
        cancel_transition(element_id, property);

        // Create new active transition
        ActiveTransition at;
        at.property = property;
        at.start_value = start;
        at.end_value = end;
        at.timing = transition.timing;
        at.duration = transition.duration;
        at.delay = transition.delay;
        at.elapsed = 0.0f;

        active_transitions_[element_id].push_back(at);
    }

    /**
     * @brief Cancel a specific transition
     */
    void cancel_transition(ElementId element_id, TransitionProperty property) {
        auto it = active_transitions_.find(element_id);
        if (it != active_transitions_.end()) {
            auto& transitions = it->second;
            transitions.erase(
                std::remove_if(transitions.begin(), transitions.end(),
                    [property](const ActiveTransition& t) {
                        return t.property == property;
                    }),
                transitions.end()
            );
        }
    }

    /**
     * @brief Cancel all transitions for an element
     */
    void cancel_all(ElementId element_id) {
        active_transitions_.erase(element_id);
    }

    /**
     * @brief Check if an element has active transitions
     */
    bool has_active_transitions(ElementId element_id) const {
        auto it = active_transitions_.find(element_id);
        return it != active_transitions_.end() && !it->second.empty();
    }

    /**
     * @brief Check if any transitions are active
     */
    bool has_any_active() const {
        return !active_transitions_.empty();
    }

    /**
     * @brief Get current interpolated value for a property
     * @return nullopt if no transition is active
     */
    std::optional<AnimatedValue> get_current_value(ElementId element_id, TransitionProperty property) const {
        auto it = active_transitions_.find(element_id);
        if (it == active_transitions_.end()) return std::nullopt;

        for (const auto& t : it->second) {
            if (t.property == property) {
                float progress = t.eased_progress();
                return interpolate(t.start_value, t.end_value, progress);
            }
        }
        return std::nullopt;
    }

    // Callbacks
    std::function<void(ElementId, TransitionProperty, const AnimatedValue&)> on_transition_update_;
    std::function<void(ElementId, TransitionProperty, const AnimatedValue&)> on_transition_end_;

private:
    std::unordered_map<ElementId, std::vector<ActiveTransition>> active_transitions_;
};

// ============================================================================
// Helper: Find transition for a property
// ============================================================================

inline const Transition* find_transition(const std::vector<Transition>& transitions, TransitionProperty property) {
    for (const auto& t : transitions) {
        if (t.property == TransitionProperty::ALL || t.property == property) {
            return &t;
        }
    }
    return nullptr;
}

// Note: parse_transition_property is defined in cssbox_conversion.h (uses re2c DFA lexer)

// ============================================================================
// Active Animation - Runtime state for a single animation
// ============================================================================

struct ActiveAnimation {
    std::string name;                    // @keyframes name
    const KeyframesDefinition* keyframes; // Pointer to keyframes definition

    float duration;                      // Duration per iteration (seconds)
    float delay;                         // Initial delay (seconds)
    float elapsed;                       // Total elapsed time (seconds)
    float iteration_count;               // Total iterations (infinity = infinite)

    TimingFunction timing;
    AnimationDirection direction;
    AnimationFillMode fill_mode;
    AnimationPlayState play_state;

    int current_iteration;               // Current iteration (0-based)
    bool completed;                      // Animation has finished all iterations

    ActiveAnimation()
        : keyframes(nullptr)
        , duration(0)
        , delay(0)
        , elapsed(0)
        , iteration_count(1)
        , direction(AnimationDirection::NORMAL)
        , fill_mode(AnimationFillMode::NONE)
        , play_state(AnimationPlayState::RUNNING)
        , current_iteration(0)
        , completed(false)
    {}

    bool is_infinite() const {
        return std::isinf(iteration_count);
    }

    bool is_paused() const {
        return play_state == AnimationPlayState::PAUSED;
    }

    bool in_delay() const {
        return elapsed < delay;
    }

    /**
     * @brief Get current progress within single iteration (0-1)
     */
    float iteration_progress() const {
        if (in_delay() || duration <= 0) return 0.0f;

        float active_time = elapsed - delay;
        float raw_progress = std::fmod(active_time, duration) / duration;

        // Handle direction
        bool reverse_this_iteration = false;
        switch (direction) {
            case AnimationDirection::NORMAL:
                reverse_this_iteration = false;
                break;
            case AnimationDirection::REVERSE:
                reverse_this_iteration = true;
                break;
            case AnimationDirection::ALTERNATE:
                reverse_this_iteration = (current_iteration % 2 == 1);
                break;
            case AnimationDirection::ALTERNATE_REVERSE:
                reverse_this_iteration = (current_iteration % 2 == 0);
                break;
        }

        return reverse_this_iteration ? (1.0f - raw_progress) : raw_progress;
    }

    /**
     * @brief Get eased progress (applies timing function)
     */
    float eased_progress() const {
        return timing.evaluate(iteration_progress());
    }
};

// ============================================================================
// Animation Manager
// ============================================================================

class AnimationManager {
public:
    using ElementId = uint64_t;
    using KeyframesRegistry = std::unordered_map<std::string, KeyframesDefinition>;

    AnimationManager() = default;

    /**
     * @brief Register a @keyframes definition
     */
    void register_keyframes(const KeyframesDefinition& kf) {
        keyframes_registry_[kf.name] = kf;
    }

    /**
     * @brief Get a @keyframes definition by name
     */
    const KeyframesDefinition* get_keyframes(const std::string& name) const {
        auto it = keyframes_registry_.find(name);
        return (it != keyframes_registry_.end()) ? &it->second : nullptr;
    }

    /**
     * @brief Update all active animations
     * @param dt Delta time in seconds
     */
    void update(float dt) {
        for (auto& [element_id, animations] : active_animations_) {
            for (auto it = animations.begin(); it != animations.end(); ) {
                // Skip paused animations
                if (it->is_paused()) {
                    ++it;
                    continue;
                }

                it->elapsed += dt;

                // Check if still in delay
                if (it->in_delay()) {
                    // Apply backwards fill if applicable
                    if (it->fill_mode == AnimationFillMode::BACKWARDS ||
                        it->fill_mode == AnimationFillMode::BOTH) {
                        apply_keyframe_values(element_id, *it, 0.0f);
                    }
                    ++it;
                    continue;
                }

                // Calculate current iteration
                float active_time = it->elapsed - it->delay;
                int new_iteration = static_cast<int>(active_time / it->duration);

                // Check if animation completed
                if (!it->is_infinite() && new_iteration >= static_cast<int>(it->iteration_count)) {
                    it->completed = true;
                    it->current_iteration = static_cast<int>(it->iteration_count) - 1;

                    // Apply forwards fill
                    if (it->fill_mode == AnimationFillMode::FORWARDS ||
                        it->fill_mode == AnimationFillMode::BOTH) {
                        // Apply final keyframe
                        float final_progress = (it->direction == AnimationDirection::REVERSE ||
                                                it->direction == AnimationDirection::ALTERNATE_REVERSE)
                                                ? 0.0f : 1.0f;
                        apply_keyframe_values(element_id, *it, final_progress);
                    }

                    if (on_animation_end_) {
                        on_animation_end_(element_id, it->name);
                    }

                    it = animations.erase(it);
                    continue;
                }

                it->current_iteration = new_iteration;

                // Apply current animated values
                float progress = it->eased_progress();
                apply_keyframe_values(element_id, *it, progress);

                // Callback for iteration change
                if (on_animation_iteration_ && it->current_iteration > 0 &&
                    new_iteration != it->current_iteration) {
                    on_animation_iteration_(element_id, it->name, new_iteration);
                }

                ++it;
            }
        }

        // Clean up empty entries
        for (auto it = active_animations_.begin(); it != active_animations_.end(); ) {
            if (it->second.empty()) {
                it = active_animations_.erase(it);
            } else {
                ++it;
            }
        }
    }

    /**
     * @brief Start an animation for an element
     */
    void start_animation(ElementId element_id, const Animation& animation) {
        const KeyframesDefinition* kf = get_keyframes(animation.name);
        if (!kf) {
            // Unknown keyframes - skip
            return;
        }

        // Cancel existing animation with same name
        cancel_animation(element_id, animation.name);

        ActiveAnimation aa;
        aa.name = animation.name;
        aa.keyframes = kf;
        aa.duration = animation.duration;
        aa.delay = animation.delay;
        aa.elapsed = 0.0f;
        aa.iteration_count = animation.iteration_count;
        aa.timing = animation.timing;
        aa.direction = animation.direction;
        aa.fill_mode = animation.fill_mode;
        aa.play_state = animation.play_state;
        aa.current_iteration = 0;
        aa.completed = false;

        active_animations_[element_id].push_back(aa);

        if (on_animation_start_) {
            on_animation_start_(element_id, animation.name);
        }
    }

    /**
     * @brief Cancel a specific animation
     */
    void cancel_animation(ElementId element_id, const std::string& name) {
        auto it = active_animations_.find(element_id);
        if (it != active_animations_.end()) {
            auto& anims = it->second;
            anims.erase(
                std::remove_if(anims.begin(), anims.end(),
                    [&name](const ActiveAnimation& a) {
                        return a.name == name;
                    }),
                anims.end()
            );
        }
    }

    /**
     * @brief Cancel all animations for an element
     */
    void cancel_all(ElementId element_id) {
        active_animations_.erase(element_id);
    }

    /**
     * @brief Pause/resume animation
     */
    void set_play_state(ElementId element_id, const std::string& name, AnimationPlayState state) {
        auto it = active_animations_.find(element_id);
        if (it != active_animations_.end()) {
            for (auto& anim : it->second) {
                if (anim.name == name) {
                    anim.play_state = state;
                    break;
                }
            }
        }
    }

    /**
     * @brief Check if an element has active animations
     */
    bool has_active_animations(ElementId element_id) const {
        auto it = active_animations_.find(element_id);
        return it != active_animations_.end() && !it->second.empty();
    }

    /**
     * @brief Check if any animations are active
     */
    bool has_any_active() const {
        return !active_animations_.empty();
    }

    /**
     * @brief Get current animated value for a property
     */
    template<typename T>
    std::optional<T> get_animated_value(ElementId element_id, const std::string& anim_name,
                                         std::function<std::optional<T>(const Keyframe&)> extractor) const {
        auto it = active_animations_.find(element_id);
        if (it == active_animations_.end()) return std::nullopt;

        for (const auto& anim : it->second) {
            if (anim.name == anim_name && anim.keyframes) {
                float progress = anim.eased_progress();
                return interpolate_keyframe_value(anim.keyframes, progress, extractor);
            }
        }
        return std::nullopt;
    }

    // Callbacks
    std::function<void(ElementId, const std::string&)> on_animation_start_;
    std::function<void(ElementId, const std::string&)> on_animation_end_;
    std::function<void(ElementId, const std::string&, int)> on_animation_iteration_;
    std::function<void(ElementId, const Keyframe&, float)> on_animation_update_;

private:
    KeyframesRegistry keyframes_registry_;
    std::unordered_map<ElementId, std::vector<ActiveAnimation>> active_animations_;

    /**
     * @brief Apply keyframe values at given progress
     */
    void apply_keyframe_values(ElementId element_id, const ActiveAnimation& anim, float progress) {
        if (!anim.keyframes || anim.keyframes->keyframes.empty()) return;

        // Get surrounding keyframes
        auto [kf_from, kf_to] = anim.keyframes->get_surrounding(progress);
        if (!kf_from || !kf_to) return;

        // Calculate local progress between keyframes
        float local_t = 0.0f;
        if (kf_from != kf_to && kf_to->percentage > kf_from->percentage) {
            local_t = (progress - kf_from->percentage) / (kf_to->percentage - kf_from->percentage);
        }

        // Create interpolated keyframe for callback
        Keyframe interpolated(progress);

        // Interpolate each property that's set in keyframes
        if (kf_from->opacity && kf_to->opacity) {
            interpolated.opacity = lerp(*kf_from->opacity, *kf_to->opacity, local_t);
        } else if (kf_from->opacity) {
            interpolated.opacity = kf_from->opacity;
        } else if (kf_to->opacity) {
            interpolated.opacity = kf_to->opacity;
        }

        if (kf_from->background_color && kf_to->background_color) {
            interpolated.background_color = lerp_color(*kf_from->background_color, *kf_to->background_color, local_t);
        }

        if (kf_from->color && kf_to->color) {
            interpolated.color = lerp_color(*kf_from->color, *kf_to->color, local_t);
        }

        if (kf_from->transform && kf_to->transform) {
            // Interpolate transform matrices
            float m1[6], m2[6], result[6];
            kf_from->transform->compose_matrix(m1);
            kf_to->transform->compose_matrix(m2);
            for (int i = 0; i < 6; i++) {
                result[i] = lerp(m1[i], m2[i], local_t);
            }
            Transform tr;
            tr.functions.push_back(TransformFunction::matrix(
                result[0], result[1], result[2], result[3], result[4], result[5]
            ));
            interpolated.transform = tr;
        }

        // Trigger callback with interpolated values
        if (on_animation_update_) {
            on_animation_update_(element_id, interpolated, progress);
        }
    }

    /**
     * @brief Interpolate a single keyframe value
     */
    template<typename T>
    std::optional<T> interpolate_keyframe_value(
        const KeyframesDefinition* kf,
        float progress,
        std::function<std::optional<T>(const Keyframe&)> extractor
    ) const {
        if (!kf || kf->keyframes.empty()) return std::nullopt;

        auto [kf_from, kf_to] = kf->get_surrounding(progress);
        if (!kf_from) return std::nullopt;

        auto val_from = extractor(*kf_from);
        if (!val_from) return std::nullopt;

        if (!kf_to || kf_from == kf_to) return val_from;

        auto val_to = extractor(*kf_to);
        if (!val_to) return val_from;

        // Calculate local progress
        float local_t = 0.0f;
        if (kf_to->percentage > kf_from->percentage) {
            local_t = (progress - kf_from->percentage) / (kf_to->percentage - kf_from->percentage);
        }

        // Default lerp (specialized for specific types below)
        return val_from; // Fallback - no interpolation for unknown types
    }
};

} // namespace cssbox

#endif // CSSBOX_TRANSITION_H
