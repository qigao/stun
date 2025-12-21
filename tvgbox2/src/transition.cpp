/*
 * tvgbox2 - CSS Transitions Implementation
 */

#include <tvgbox2/transition.h>
#include <sstream>

namespace tvgbox2 {

// ============================================================================
// Parse CSS transition shorthand
// ============================================================================

TransitionDef parse_transition(const std::string& value) {
    TransitionDef def;

    std::vector<std::string> parts;
    size_t start = 0;
    for (size_t i = 0; i <= value.size(); ++i) {
        if (i == value.size() || value[i] == ' ') {
            if (i > start) {
                parts.push_back(value.substr(start, i - start));
            }
            start = i + 1;
        }
    }

    for (const auto& part : parts) {
        if (part.empty()) continue;

        // Check if it's a duration (ends with 's' or 'ms')
        if (part.back() == 's') {
            float val = std::strtof(part.c_str(), nullptr);
            if (part.find("ms") != std::string::npos) {
                // Already in ms
            } else {
                val *= 1000.0f;
            }

            if (def.duration_ms == 300.0f) {
                def.duration_ms = val;
            } else {
                def.delay_ms = val;
            }
        }
        // Check if it's an easing function
        else if (part.find("ease") != std::string::npos ||
                 part == "linear" ||
                 part.find("cubic") != std::string::npos) {
            if (part == "linear") def.easing = EasingType::Linear;
            else if (part == "ease") def.easing = EasingType::Ease;
            else if (part == "ease-in") def.easing = EasingType::EaseIn;
            else if (part == "ease-out") def.easing = EasingType::EaseOut;
            else if (part == "ease-in-out") def.easing = EasingType::EaseInOut;
            else def.easing = EasingType::Ease;
        }
        // Otherwise it's a property name
        else {
            def.property = part;
        }
    }

    if (def.property.empty()) {
        def.property = "all";
    }

    return def;
}

// ============================================================================
// TransitionManager Implementation
// ============================================================================

void TransitionManager::start(int element_id, const std::string& property,
                              float from, float to, const TransitionDef& def,
                              float current_time_ms) {
    ActiveTransition trans;
    trans.property = property;
    trans.start_value = from;
    trans.end_value = to;
    trans.start_time_ms = current_time_ms;
    trans.duration_ms = def.duration_ms;
    trans.delay_ms = def.delay_ms;
    trans.easing_fn = easing::get(def.easing);

    std::string key = std::to_string(element_id) + ":" + property;
    transitions_[key] = trans;
}

float TransitionManager::get(int element_id, const std::string& property,
                             float default_value, float current_time_ms) {
    std::string key = std::to_string(element_id) + ":" + property;
    auto it = transitions_.find(key);
    if (it == transitions_.end()) {
        return default_value;
    }
    return it->second.current_value(current_time_ms);
}

bool TransitionManager::has_active(int element_id, float current_time_ms) {
    std::string prefix = std::to_string(element_id) + ":";
    for (auto& [key, trans] : transitions_) {
        if (key.find(prefix) == 0 && !trans.is_complete(current_time_ms)) {
            return true;
        }
    }
    return false;
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

} // namespace tvgbox2
