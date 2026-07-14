/*
 * flexUI - CSS Transitions Implementation
 */

#include <flexUI/transition.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <sstream>

namespace flexUI {

namespace {

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

void TransitionManager::start(std::uintptr_t element_id, const std::string& property,
                              float from, float to, const TransitionDef& def,
                              float current_time_ms) {
    ActiveTransition trans;
    trans.property = property;
    trans.start_value = from;
    trans.end_value = to;
    trans.start_time_ms = current_time_ms;
    trans.duration_ms = def.duration_ms;
    trans.delay_ms = def.delay_ms;
    trans.easing_type = def.easing;
    std::copy(std::begin(def.bezier), std::end(def.bezier), std::begin(trans.bezier));
    trans.easing_fn = easing::get(def.easing);

    std::string key = std::to_string(element_id) + ":" + property;
    transitions_[key] = trans;
}

float TransitionManager::get(std::uintptr_t element_id, const std::string& property,
                             float default_value, float current_time_ms) {
    std::string key = std::to_string(element_id) + ":" + property;
    auto it = transitions_.find(key);
    if (it == transitions_.end()) {
        return default_value;
    }
    return it->second.current_value(current_time_ms);
}

bool TransitionManager::has_active(std::uintptr_t element_id, float current_time_ms) {
    std::string prefix = std::to_string(element_id) + ":";
    for (auto& [key, trans] : transitions_) {
        if (key.find(prefix) == 0 && !trans.is_complete(current_time_ms)) {
            return true;
        }
    }
    return false;
}

void TransitionManager::clear_element(std::uintptr_t element_id) {
    const std::string prefix = std::to_string(element_id) + ":";
    for (auto it = transitions_.begin(); it != transitions_.end();) {
        if (it->first.find(prefix) == 0) {
            it = transitions_.erase(it);
        } else {
            ++it;
        }
    }
}

void TransitionManager::update(float current_time_ms) {
    for (auto it = transitions_.begin(); it != transitions_.end(); ) {
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
