/*
 * flexUI - CSS Transitions Implementation
 */

#include <flexUI/transition.h>
#include <flex/core/cmeta_types.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace flexUI {

namespace {

bool transition_value_matches_type(const TransitionValue& value,
                                   const cmeta_type_desc* type) {
    if (!value.has_value() || !type ||
        !cmeta_type_equal(value.type, type)) {
        return false;
    }
    if (cmeta_type_equal(type, &cmeta_type_float)) {
        return std::holds_alternative<float>(value.value);
    }
    if (cmeta_type_equal(type, &flex::cmeta_type_color)) {
        return std::holds_alternative<Color>(value.value);
    }
    return false;
}

const void* transition_value_data(const TransitionValue& value) {
    if (const auto* scalar = std::get_if<float>(&value.value)) {
        return scalar;
    }
    if (const auto* color = std::get_if<Color>(&value.value)) {
        return color;
    }
    return nullptr;
}

void* transition_value_data(TransitionValue& value) {
    if (auto* scalar = std::get_if<float>(&value.value)) {
        return scalar;
    }
    if (auto* color = std::get_if<Color>(&value.value)) {
        return color;
    }
    return nullptr;
}

bool transition_float_equal(const void* lhs, const void* rhs) {
    if (!lhs || !rhs) return false;
    return std::fabs(*static_cast<const float*>(lhs) -
                     *static_cast<const float*>(rhs)) <= 0.0001f;
}

bool transition_float_interpolate(const void* from, const void* to,
                                  float t, void* out) {
    if (!from || !to || !out) return false;
    *static_cast<float*>(out) =
        flex::animation::interpolate(*static_cast<const float*>(from),
                                     *static_cast<const float*>(to), t);
    return true;
}

bool transition_color_equal(const void* lhs, const void* rhs) {
    if (!lhs || !rhs) return false;
    const auto& a = *static_cast<const Color*>(lhs);
    const auto& b = *static_cast<const Color*>(rhs);
    return std::fabs(a.r - b.r) <= 0.0001f &&
           std::fabs(a.g - b.g) <= 0.0001f &&
           std::fabs(a.b - b.b) <= 0.0001f &&
           std::fabs(a.a - b.a) <= 0.0001f;
}

bool transition_color_interpolate(const void* from, const void* to,
                                  float t, void* out) {
    if (!from || !to || !out) return false;
    const auto& a = *static_cast<const Color*>(from);
    const auto& b = *static_cast<const Color*>(to);
    auto& result = *static_cast<Color*>(out);
    result = {
        flex::animation::interpolate(a.r, b.r, t),
        flex::animation::interpolate(a.g, b.g, t),
        flex::animation::interpolate(a.b, b.b, t),
        flex::animation::interpolate(a.a, b.a, t),
    };
    return true;
}

const TransitionValueOps kFloatTransitionOps = {
    &cmeta_type_float,
    transition_float_equal,
    transition_float_interpolate,
};

const TransitionValueOps kColorTransitionOps = {
    &flex::cmeta_type_color,
    transition_color_equal,
    transition_color_interpolate,
};

float transition_eased_progress(EasingType easing_type,
                                easing::EasingFunction easing_fn,
                                const float bezier[4],
                                float t) {
    if (easing_type == EasingType::CubicBezier ||
        easing_type == EasingType::Ease) {
        return easing::evaluate_cubic_bezier(
            bezier[0], bezier[1], bezier[2], bezier[3], t);
    }
    return easing_fn(t);
}

std::string trim_copy(const std::string& value) {
    size_t start = 0;
    while (start < value.size() &&
           std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }

    size_t end = value.size();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }

    return value.substr(start, end - start);
}

bool is_transition_time_token(const std::string& part) {
    const bool ends_with_ms =
        part.size() >= 2 &&
        part.compare(part.size() - 2, 2, "ms") == 0;
    return part.size() >= 2 &&
           (ends_with_ms || part.back() == 's');
}

float parse_transition_time_ms(const std::string& part) {
    float val = std::strtof(part.c_str(), nullptr);
    const bool ends_with_ms =
        part.size() >= 2 &&
        part.compare(part.size() - 2, 2, "ms") == 0;
    if (!ends_with_ms) {
        val *= 1000.0f;
    }
    return val;
}

TransitionDef parse_single_transition(const std::string& value) {
    TransitionDef def;
    std::string normalized;
    int paren_depth = 0;
    for (char ch : value) {
        if (ch == '(') {
            paren_depth++;
            normalized.push_back(ch);
        } else if (ch == ')') {
            if (paren_depth > 0) paren_depth--;
            normalized.push_back(ch);
        } else if (std::isspace(static_cast<unsigned char>(ch))) {
            if (paren_depth == 0) {
                normalized.push_back(ch);
            }
        } else {
            normalized.push_back(ch);
        }
    }

    std::istringstream stream(normalized);
    std::string part;
    bool has_duration = false;
    bool has_delay = false;

    while (stream >> part) {
        if (part.empty()) continue;

        if (is_transition_time_token(part)) {
            const float val = parse_transition_time_ms(part);
            if (!has_duration) {
                def.duration_ms = val;
                has_duration = true;
            } else if (!has_delay) {
                def.delay_ms = val;
                has_delay = true;
            }
        } else if (part.find("ease") != std::string::npos ||
                   part == "linear" ||
                   part.find("cubic") != std::string::npos) {
            if (part.find("cubic-bezier(") == 0) {
                def.easing = EasingType::CubicBezier;
                size_t open_paren = part.find('(');
                size_t close_paren = part.find(')');
                if (open_paren != std::string::npos && close_paren != std::string::npos && close_paren > open_paren) {
                    std::string params = part.substr(open_paren + 1, close_paren - open_paren - 1);
                    std::replace(params.begin(), params.end(), ',', ' ');
                    std::istringstream param_stream(params);
                    param_stream >> def.bezier[0] >> def.bezier[1] >> def.bezier[2] >> def.bezier[3];
                }
            }
            else if (part == "linear") def.easing = EasingType::Linear;
            else if (part == "ease") def.easing = EasingType::Ease;
            else if (part == "ease-in") def.easing = EasingType::EaseIn;
            else if (part == "ease-out") def.easing = EasingType::EaseOut;
            else if (part == "ease-in-out") def.easing = EasingType::EaseInOut;
            else def.easing = EasingType::Ease;
        } else {
            def.property = part;
        }
    }

    if (def.property.empty()) {
        def.property = "all";
    }

    return def;
}

float sample_animation_keyframes(const std::vector<AnimationValuePoint>& keyframes,
                                 float progress) {
    if (keyframes.empty()) {
        return 0.0f;
    }
    if (progress <= keyframes.front().offset) {
        return keyframes.front().value;
    }
    if (progress >= keyframes.back().offset) {
        return keyframes.back().value;
    }

    for (size_t i = 1; i < keyframes.size(); ++i) {
        const auto& prev = keyframes[i - 1];
        const auto& next = keyframes[i];
        if (progress <= next.offset) {
            const float span = next.offset - prev.offset;
            if (span <= 0.0001f) {
                return next.value;
            }
            const float local_t = (progress - prev.offset) / span;
            return flex::animation::interpolate(prev.value, next.value, local_t);
        }
    }

    return keyframes.back().value;
}

bool animation_cycle_is_reversed(AnimationDirection direction, int iteration_index) {
    switch (direction) {
    case AnimationDirection::Reverse:
        return true;
    case AnimationDirection::Alternate:
        return (iteration_index % 2) == 1;
    case AnimationDirection::AlternateReverse:
        return (iteration_index % 2) == 0;
    case AnimationDirection::Normal:
    default:
        return false;
    }
}

float animation_effective_progress(AnimationDirection direction,
                                   easing::EasingFunction easing_fn,
                                   int iteration_index,
                                   float raw_t) {
    const float eased_t = easing_fn(std::clamp(raw_t, 0.0f, 1.0f));
    return animation_cycle_is_reversed(direction, iteration_index)
               ? (1.0f - eased_t)
               : eased_t;
}

float animation_terminal_progress(const ActiveAnimation& animation) {
    if (animation.infinite) {
        return animation_effective_progress(animation.direction, animation.easing_fn, 0, 0.0f);
    }

    float whole_iterations = 0.0f;
    const float fractional = std::modf(std::max(animation.iteration_count, 0.0f),
                                       &whole_iterations);
    if (fractional > 0.0001f) {
        return animation_effective_progress(animation.direction, animation.easing_fn,
                                            static_cast<int>(whole_iterations),
                                            fractional);
    }

    const int last_iteration_index =
        std::max(static_cast<int>(whole_iterations) - 1, 0);
    return animation_effective_progress(animation.direction, animation.easing_fn,
                                        last_iteration_index, 1.0f);
}

} // namespace

const TransitionValueOps* transition_type_ops(
    const cmeta_type_desc* type) noexcept {
    if (type && cmeta_type_equal(type, &cmeta_type_float)) {
        return &kFloatTransitionOps;
    }
    if (type && cmeta_type_equal(type, &flex::cmeta_type_color)) {
        return &kColorTransitionOps;
    }
    return nullptr;
}

TransitionValue ActiveTransition::current_value(float time_ms) const {
    if (!property || !ops ||
        !transition_value_matches_type(start_value, property->type) ||
        !transition_value_matches_type(end_value, property->type)) {
        return {};
    }

    const float elapsed = time_ms - start_time_ms - delay_ms;
    if (elapsed < 0.0f) return start_value;
    if (elapsed >= duration_ms || duration_ms <= 0.0f) return end_value;

    TransitionValue out = start_value;
    void* out_data = transition_value_data(out);
    const void* from_data = transition_value_data(start_value);
    const void* to_data = transition_value_data(end_value);
    if (!out_data || !from_data || !to_data) return {};

    const float eased_t = transition_eased_progress(
        easing_type, easing_fn, bezier, elapsed / duration_ms);
    if (!ops->interpolate(from_data, to_data, eased_t, out_data)) return {};
    return out;
}

// ============================================================================
// Parse CSS transition shorthand
// ============================================================================

TransitionDef parse_transition(const std::string& value) {
    return parse_single_transition(value);
}

std::vector<TransitionDef> parse_transition_list(const std::string& value) {
    std::vector<TransitionDef> defs;
    std::string current;
    int paren_depth = 0;

    for (char ch : value) {
        if (ch == '(') {
            ++paren_depth;
            current.push_back(ch);
            continue;
        }
        if (ch == ')') {
            if (paren_depth > 0) {
                --paren_depth;
            }
            current.push_back(ch);
            continue;
        }
        if (ch == ',' && paren_depth == 0) {
            const std::string item = trim_copy(current);
            if (!item.empty()) {
                defs.push_back(parse_single_transition(item));
            }
            current.clear();
            continue;
        }

        current.push_back(ch);
    }

    const std::string tail = trim_copy(current);
    if (!tail.empty()) {
        defs.push_back(parse_single_transition(tail));
    }

    return defs;
}

// ============================================================================
// TransitionManager Implementation
// ============================================================================

void TransitionManager::start(std::uintptr_t element_id,
                              const std::string& property,
                              float from, float to,
                              const TransitionDef& def,
                              float current_time_ms) {
    const auto* descriptor = detail::style_property_find(property);
    if (!descriptor) return;
    start_float(element_id, *descriptor, from, to, def, current_time_ms);
}

bool TransitionManager::start_typed(
    std::uintptr_t element_id,
    const detail::StylePropertyDesc& property,
    const TransitionValue& from,
    const TransitionValue& to,
    const TransitionDef& def,
    float current_time_ms) {
    const auto* ops = transition_type_ops(property.type);
    if ((property.flags & detail::STYLE_PROPERTY_TRANSITIONABLE) == 0u ||
        !ops || !ops->equal || !ops->interpolate ||
        !transition_value_matches_type(from, property.type) ||
        !transition_value_matches_type(to, property.type)) {
        return false;
    }

    const void* from_data = transition_value_data(from);
    const void* to_data = transition_value_data(to);
    if (!from_data || !to_data || ops->equal(from_data, to_data)) return false;

    ActiveTransition trans;
    trans.property = &property;
    trans.start_value = from;
    trans.end_value = to;
    trans.ops = ops;
    trans.start_time_ms = current_time_ms;
    trans.duration_ms = def.duration_ms;
    trans.delay_ms = def.delay_ms;
    trans.easing_type = def.easing;
    std::copy(std::begin(def.bezier), std::end(def.bezier),
              std::begin(trans.bezier));
    trans.easing_fn = easing::get(def.easing);

    transitions_[{element_id, property.id}] = std::move(trans);
    return true;
}

TransitionValue TransitionManager::get_typed(
    std::uintptr_t element_id,
    const detail::StylePropertyDesc& property,
    const TransitionValue& default_value,
    float current_time_ms) const {
    if (!transition_value_matches_type(default_value, property.type)) {
        return default_value;
    }

    const auto it = transitions_.find({element_id, property.id});
    if (it == transitions_.end() || !it->second.property ||
        it->second.property->id != property.id ||
        !cmeta_type_equal(it->second.property->type, property.type)) {
        return default_value;
    }

    const TransitionValue current = it->second.current_value(current_time_ms);
    return current.has_value() ? current : default_value;
}

bool TransitionManager::start_float(
    std::uintptr_t element_id,
    const detail::StylePropertyDesc& property,
    float previous_value,
    float target_value,
    const TransitionDef& def,
    float current_time_ms) {
    if (!property.type || !cmeta_type_equal(property.type, &cmeta_type_float)) {
        return false;
    }
    const TransitionValue baseline{&cmeta_type_float, flex::AnimValue{previous_value}};
    const TransitionValue current =
        get_typed(element_id, property, baseline, current_time_ms);
    const TransitionValue target{&cmeta_type_float, flex::AnimValue{target_value}};
    return start_typed(element_id, property, current, target, def,
                       current_time_ms);
}

bool TransitionManager::start_color(
    std::uintptr_t element_id,
    const detail::StylePropertyDesc& property,
    const Color& previous_value,
    const Color& target_value,
    const TransitionDef& def,
    float current_time_ms) {
    if (!property.type ||
        !cmeta_type_equal(property.type, &flex::cmeta_type_color)) {
        return false;
    }
    const TransitionValue baseline{
        &flex::cmeta_type_color, flex::AnimValue{previous_value}};
    const TransitionValue current =
        get_typed(element_id, property, baseline, current_time_ms);
    const TransitionValue target{
        &flex::cmeta_type_color, flex::AnimValue{target_value}};
    return start_typed(element_id, property, current, target, def,
                       current_time_ms);
}

float TransitionManager::get_float(
    std::uintptr_t element_id,
    const detail::StylePropertyDesc& property,
    float default_value,
    float current_time_ms) const {
    if (!property.type || !cmeta_type_equal(property.type, &cmeta_type_float)) {
        return default_value;
    }
    const TransitionValue baseline{&cmeta_type_float, flex::AnimValue{default_value}};
    const TransitionValue current =
        get_typed(element_id, property, baseline, current_time_ms);
    const auto* value = std::get_if<float>(&current.value);
    return value ? *value : default_value;
}

Color TransitionManager::get_color(
    std::uintptr_t element_id,
    const detail::StylePropertyDesc& property,
    const Color& default_value,
    float current_time_ms) const {
    if (!property.type ||
        !cmeta_type_equal(property.type, &flex::cmeta_type_color)) {
        return default_value;
    }
    const TransitionValue baseline{
        &flex::cmeta_type_color, flex::AnimValue{default_value}};
    const TransitionValue current =
        get_typed(element_id, property, baseline, current_time_ms);
    const auto* value = std::get_if<Color>(&current.value);
    return value ? *value : default_value;
}

float TransitionManager::get(std::uintptr_t element_id,
                             const std::string& property,
                             float default_value,
                             float current_time_ms) {
    if (const auto* descriptor = detail::style_property_find(property)) {
        return get_float(element_id, *descriptor, default_value, current_time_ms);
    }

    // Compatibility read adapter for historical color-component callers.
    if (property.size() > 2 && property[property.size() - 2] == '-') {
        const char component = property.back();
        if (component == 'r' || component == 'g' ||
            component == 'b' || component == 'a') {
            const std::string base = property.substr(0, property.size() - 2);
            if (const auto* descriptor = detail::style_property_find(base);
                descriptor && descriptor->type &&
                cmeta_type_equal(descriptor->type, &flex::cmeta_type_color)) {
                Color fallback{};
                if (component == 'r') fallback.r = default_value;
                if (component == 'g') fallback.g = default_value;
                if (component == 'b') fallback.b = default_value;
                if (component == 'a') fallback.a = default_value;
                const Color current =
                    get_color(element_id, *descriptor, fallback, current_time_ms);
                if (component == 'r') return current.r;
                if (component == 'g') return current.g;
                if (component == 'b') return current.b;
                return current.a;
            }
        }
    }
    return default_value;
}

bool TransitionManager::has_active(std::uintptr_t element_id,
                                   float current_time_ms) {
    for (const auto& [key, trans] : transitions_) {
        if (key.first == element_id && !trans.is_complete(current_time_ms)) {
            return true;
        }
    }
    return false;
}

void TransitionManager::clear_element(std::uintptr_t element_id) {
    for (auto it = transitions_.begin(); it != transitions_.end();) {
        if (it->first.first == element_id) {
            it = transitions_.erase(it);
        } else {
            ++it;
        }
    }
}

void TransitionManager::update(float current_time_ms) {
    for (auto it = transitions_.begin(); it != transitions_.end();) {
        if (it->second.is_complete(current_time_ms)) {
            it = transitions_.erase(it);
        } else {
            ++it;
        }
    }
}

float ActiveAnimation::current_value(float default_value, float time_ms) const {
    if (keyframes.empty()) {
        return default_value;
    }

    const float effective_time_ms = paused ? paused_at_ms : time_ms;
    const float local_time =
        effective_time_ms - start_time_ms - delay_ms - total_paused_ms;
    if (duration_ms <= 0.0f) {
        return keyframes.back().value;
    }

    if (local_time < 0.0f) {
        if (fill_mode == AnimationFillMode::Backwards ||
            fill_mode == AnimationFillMode::Both) {
            return sample_animation_keyframes(
                keyframes,
                animation_effective_progress(direction, easing_fn, 0, 0.0f));
        }
        return default_value;
    }

    const float cycle_duration = duration_ms;
    const float total_duration = cycle_duration * iteration_count;
    if (!infinite && local_time >= total_duration) {
        if (fill_mode == AnimationFillMode::Forwards ||
            fill_mode == AnimationFillMode::Both) {
            return sample_animation_keyframes(keyframes,
                                              animation_terminal_progress(*this));
        }
        return default_value;
    }

    float cycle_time = local_time;
    if (infinite || cycle_duration > 0.0f) {
        cycle_time = std::fmod(std::max(local_time, 0.0f), cycle_duration);
    }
    const int iteration_index =
        cycle_duration > 0.0f ? std::max(static_cast<int>(local_time / cycle_duration), 0)
                              : 0;
    const float raw_t =
        cycle_duration > 0.0f ? std::clamp(cycle_time / cycle_duration, 0.0f, 1.0f)
                              : 1.0f;
    return sample_animation_keyframes(
        keyframes,
        animation_effective_progress(direction, easing_fn, iteration_index, raw_t));
}

bool ActiveAnimation::is_active(float time_ms) const {
    if (paused) {
        return true;
    }
    const float local_time = time_ms - start_time_ms - delay_ms - total_paused_ms;
    if (local_time < 0.0f) {
        return true;
    }
    if (infinite) {
        return true;
    }
    return local_time < duration_ms * iteration_count;
}

bool ActiveAnimation::retains_fill_value() const {
    return fill_mode == AnimationFillMode::Forwards ||
           fill_mode == AnimationFillMode::Both;
}

void ActiveAnimation::set_paused(bool should_pause, float current_time_ms) {
    if (should_pause) {
        if (!paused) {
            paused = true;
            paused_at_ms = current_time_ms;
        }
        play_state = AnimationPlayState::Paused;
        return;
    }

    if (paused) {
        total_paused_ms += current_time_ms - paused_at_ms;
        paused = false;
        paused_at_ms = 0.0f;
    }
    play_state = AnimationPlayState::Running;
}

void AnimationManager::start(std::uintptr_t element_id, const std::string& property,
                             const std::vector<AnimationValuePoint>& keyframes,
                             const AnimationDef& def, float current_time_ms) {
    if (keyframes.empty() || def.duration_ms < 0.0f) {
        return;
    }

    ActiveAnimation animation;
    animation.property = property;
    animation.keyframes = keyframes;
    animation.start_time_ms = current_time_ms;
    animation.duration_ms = def.duration_ms;
    animation.delay_ms = def.delay_ms;
    animation.iteration_count = std::max(def.iteration_count, 0.0f);
    animation.infinite = def.infinite;
    animation.fill_mode = def.fill_mode;
    animation.direction = def.direction;
    animation.play_state = def.play_state;
    animation.paused = def.play_state == AnimationPlayState::Paused;
    animation.paused_at_ms = animation.paused ? current_time_ms : 0.0f;
    animation.total_paused_ms = 0.0f;
    animation.easing_fn = easing::get(def.easing);

    std::string key = std::to_string(element_id) + ":" + property;
    animations_[key] = std::move(animation);
}

float AnimationManager::get(std::uintptr_t element_id, const std::string& property,
                            float default_value, float current_time_ms) const {
    std::string key = std::to_string(element_id) + ":" + property;
    auto it = animations_.find(key);
    if (it == animations_.end()) {
        return default_value;
    }
    return it->second.current_value(default_value, current_time_ms);
}

bool AnimationManager::has_active(std::uintptr_t element_id,
                                  float current_time_ms) const {
    const std::string prefix = std::to_string(element_id) + ":";
    for (const auto& [key, animation] : animations_) {
        if (key.find(prefix) == 0 && animation.is_active(current_time_ms)) {
            return true;
        }
    }
    return false;
}

bool AnimationManager::has_effect(std::uintptr_t element_id,
                                  float current_time_ms) const {
    const std::string prefix = std::to_string(element_id) + ":";
    for (const auto& [key, animation] : animations_) {
        if (key.find(prefix) == 0 &&
            (animation.is_active(current_time_ms) ||
             animation.retains_fill_value())) {
            return true;
        }
    }
    return false;
}

bool AnimationManager::has_any_active(float current_time_ms) const {
    for (const auto& [key, animation] : animations_) {
        if (animation.is_active(current_time_ms)) {
            return true;
        }
    }
    return false;
}

void AnimationManager::set_play_state(std::uintptr_t element_id,
                                      AnimationPlayState play_state,
                                      float current_time_ms) {
    const std::string prefix = std::to_string(element_id) + ":";
    for (auto& [key, animation] : animations_) {
        if (key.find(prefix) == 0) {
            animation.set_paused(play_state == AnimationPlayState::Paused,
                                 current_time_ms);
        }
    }
}

void AnimationManager::clear_element(std::uintptr_t element_id) {
    const std::string prefix = std::to_string(element_id) + ":";
    for (auto it = animations_.begin(); it != animations_.end();) {
        if (it->first.find(prefix) == 0) {
            it = animations_.erase(it);
        } else {
            ++it;
        }
    }
}

void AnimationManager::update(float current_time_ms) {
    for (auto it = animations_.begin(); it != animations_.end();) {
        if (!it->second.is_active(current_time_ms) &&
            !it->second.retains_fill_value()) {
            it = animations_.erase(it);
        } else {
            ++it;
        }
    }
}

} // namespace flexUI
