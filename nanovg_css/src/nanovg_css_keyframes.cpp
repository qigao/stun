/*
 * NanoVG CSS - Keyframe Animations (Phase 4 Sprint 2)
 */

#include "nanovg_css_internal.h"
#include <algorithm>

// ============================================================================
// KeyframeAnimation Implementation
// ============================================================================

std::map<std::string, std::string> KeyframeAnimation::get_properties_at(float position) const {
    if (keyframes.empty()) {
        return {};
    }

    // Clamp position to [0, 1]
    position = std::max(0.0f, std::min(1.0f, position));

    // Find the two keyframes to interpolate between
    const Keyframe* start_frame = nullptr;
    const Keyframe* end_frame = nullptr;

    for (size_t i = 0; i < keyframes.size(); i++) {
        if (keyframes[i].position <= position) {
            start_frame = &keyframes[i];
        }
        if (keyframes[i].position >= position && end_frame == nullptr) {
            end_frame = &keyframes[i];
        }
    }

    // Edge cases
    if (!start_frame && !end_frame) {
        return {};  // No keyframes
    }
    if (!start_frame) {
        return end_frame->properties;  // Before first keyframe
    }
    if (!end_frame) {
        return start_frame->properties;  // After last keyframe
    }
    if (start_frame == end_frame) {
        return start_frame->properties;  // Exactly on a keyframe
    }

    // Interpolate between start_frame and end_frame
    float segment_start = start_frame->position;
    float segment_end = end_frame->position;
    float segment_length = segment_end - segment_start;
    float t = segment_length > 0.0f ? (position - segment_start) / segment_length : 0.0f;

    // Combine properties from both frames
    std::map<std::string, std::string> result = start_frame->properties;

    // For each property in end_frame, interpolate if it exists in start_frame
    for (const auto& [prop, end_val] : end_frame->properties) {
        auto start_it = start_frame->properties.find(prop);
        if (start_it != start_frame->properties.end()) {
            // Both frames have this property - interpolate
            result[prop] = nvgcss_utils::interpolate_value(start_it->second, end_val, t);
        } else {
            // Only end frame has it - snap at t=0.5
            result[prop] = (t >= 0.5f) ? end_val : start_it->second;
        }
    }

    return result;
}

// ============================================================================
// SimpleStyleSheet Keyframe Methods
// ============================================================================

void SimpleStyleSheet::add_keyframe_animation(const KeyframeAnimation& animation) {
    keyframe_animations_[animation.name] = animation;
}

const KeyframeAnimation* SimpleStyleSheet::get_keyframe_animation(const std::string& name) const {
    auto it = keyframe_animations_.find(name);
    if (it != keyframe_animations_.end()) {
        return &it->second;
    }
    return nullptr;
}

// ============================================================================
// @keyframes Parsing Helper (Phase 4 Sprint 2)
// ============================================================================

/**
 * @brief Parse @keyframes rule from CSS
 * @param keyframes_css CSS text for keyframes (content between @keyframes name { and })
 * @param name Animation name
 * @return Parsed KeyframeAnimation or empty if parsing failed
 */
KeyframeAnimation parse_keyframes_rule(const std::string& keyframes_css, const std::string& name) {
    KeyframeAnimation animation(name);

    // Parse keyframes like:
    // 0% { opacity: 1; }
    // 50% { opacity: 0.5; }
    // 100% { opacity: 0; }

    size_t pos = 0;
    while (pos < keyframes_css.length()) {
        // Skip whitespace
        while (pos < keyframes_css.length() && std::isspace(keyframes_css[pos])) {
            pos++;
        }
        if (pos >= keyframes_css.length()) break;

        // Find percentage (e.g., "0%", "50%", "100%", or "from"/"to")
        size_t percent_start = pos;
        size_t percent_end = pos;

        // Check for keywords first
        if (keyframes_css.substr(pos, 4) == "from") {
            percent_end = pos + 4;
            pos = percent_end;
        } else if (keyframes_css.substr(pos, 2) == "to") {
            percent_end = pos + 2;
            pos = percent_end;
        } else {
            // Find number and %
            while (percent_end < keyframes_css.length() &&
                   (std::isdigit(keyframes_css[percent_end]) ||
                    keyframes_css[percent_end] == '.' ||
                    keyframes_css[percent_end] == '%')) {
                percent_end++;
            }
            pos = percent_end;
        }

        if (percent_start == percent_end) break;

        std::string percent_str = keyframes_css.substr(percent_start, percent_end - percent_start);

        // Trim
        percent_str.erase(0, percent_str.find_first_not_of(" \t\n\r"));
        percent_str.erase(percent_str.find_last_not_of(" \t\n\r") + 1);

        // Convert to position [0, 1]
        float position = 0.0f;
        if (percent_str == "from") {
            position = 0.0f;
        } else if (percent_str == "to") {
            position = 1.0f;
        } else {
            // Parse percentage
            size_t percent_sign = percent_str.find('%');
            if (percent_sign != std::string::npos) {
                std::string num_str = percent_str.substr(0, percent_sign);
                if (!num_str.empty()) {
                    try {
                        float percent_value = std::stof(num_str);
                        position = percent_value / 100.0f;
                    } catch (const std::exception&) {
                        position = 0.0f;  // Default to 0% on error
                    }
                }
            }
        }

        // Skip whitespace before {
        while (pos < keyframes_css.length() && std::isspace(keyframes_css[pos])) {
            pos++;
        }

        // Find { }
        size_t brace_open = keyframes_css.find('{', pos);
        if (brace_open == std::string::npos) break;

        size_t brace_close = keyframes_css.find('}', brace_open);
        if (brace_close == std::string::npos) break;

        // Extract properties
        std::string properties_str = keyframes_css.substr(brace_open + 1, brace_close - brace_open - 1);

        // Parse properties
        std::map<std::string, std::string> properties;

        size_t prop_pos = 0;
        while (prop_pos < properties_str.length()) {
            // Find next semicolon
            size_t semi_pos = properties_str.find(';', prop_pos);
            std::string declaration;

            if (semi_pos != std::string::npos) {
                declaration = properties_str.substr(prop_pos, semi_pos - prop_pos);
                prop_pos = semi_pos + 1;
            } else {
                declaration = properties_str.substr(prop_pos);
                prop_pos = properties_str.length();
            }

            // Find colon
            size_t colon_pos = declaration.find(':');
            if (colon_pos == std::string::npos) continue;

            std::string prop = declaration.substr(0, colon_pos);
            std::string value = declaration.substr(colon_pos + 1);

            // Trim
            prop.erase(0, prop.find_first_not_of(" \t\n\r"));
            prop.erase(prop.find_last_not_of(" \t\n\r") + 1);
            value.erase(0, value.find_first_not_of(" \t\n\r"));
            value.erase(value.find_last_not_of(" \t\n\r") + 1);

            if (!prop.empty() && !value.empty()) {
                properties[prop] = value;
            }
        }

        // Create keyframe
        Keyframe keyframe(position);
        keyframe.properties = properties;
        animation.keyframes.push_back(keyframe);

        // Move to next keyframe
        pos = brace_close + 1;
    }

    // Sort keyframes by position
    std::sort(animation.keyframes.begin(), animation.keyframes.end(),
              [](const Keyframe& a, const Keyframe& b) {
                  return a.position < b.position;
              });

    return animation;
}

// ============================================================================
// Animation Property Parsing (Phase 4 Sprint 2)
// ============================================================================

/**
 * @brief Parse animation duration (e.g., "2s", "500ms")
 */
float parse_animation_duration(const std::string& duration_str) {
    std::string trimmed = duration_str;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed.empty()) return 0.0f;

    try {
        if (trimmed.find("ms") != std::string::npos) {
            // Milliseconds
            std::string num_str = trimmed.substr(0, trimmed.find("ms"));
            if (num_str.empty()) return 0.0f;
            float ms = std::stof(num_str);
            return ms / 1000.0f;
        } else if (trimmed.find("s") != std::string::npos) {
            // Seconds
            std::string num_str = trimmed.substr(0, trimmed.find("s"));
            if (num_str.empty()) return 0.0f;
            return std::stof(num_str);
        } else {
            // Assume seconds if no unit
            return std::stof(trimmed);
        }
    } catch (const std::exception&) {
        return 0.0f;  // Default to 0 on parse error
    }
}

/**
 * @brief Parse animation iteration count (e.g., "3", "infinite")
 */
int parse_animation_iteration_count(const std::string& count_str) {
    std::string trimmed = count_str;
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if (trimmed.empty()) {
        return 1;  // Default to 1 iteration
    }

    if (trimmed == "infinite") {
        return -1;  // -1 means infinite
    } else {
        try {
            return std::stoi(trimmed);
        } catch (const std::exception&) {
            return 1;  // Default to 1 iteration on error
        }
    }
}

/**
 * @brief Parse animation properties from computed style
 * Creates a RunningAnimation from CSS properties
 */
RunningAnimation parse_animation_from_style(
    const std::map<std::string, std::string>& style,
    float current_time)
{
    RunningAnimation anim;

    // animation-name (required)
    auto it = style.find("animation-name");
    if (it != style.end()) {
        anim.animation_name = it->second;
    }

    // animation-duration (default: 0s)
    it = style.find("animation-duration");
    if (it != style.end()) {
        anim.duration = parse_animation_duration(it->second);
    } else {
        anim.duration = 0.0f;
    }

    // animation-timing-function (default: ease)
    it = style.find("animation-timing-function");
    if (it != style.end()) {
        std::string timing = it->second;
        if (timing == "linear") anim.easing = EasingFunction::LINEAR;
        else if (timing == "ease") anim.easing = EasingFunction::EASE;
        else if (timing == "ease-in") anim.easing = EasingFunction::EASE_IN;
        else if (timing == "ease-out") anim.easing = EasingFunction::EASE_OUT;
        else if (timing == "ease-in-out") anim.easing = EasingFunction::EASE_IN_OUT;
        else anim.easing = EasingFunction::EASE;
    } else {
        anim.easing = EasingFunction::EASE;
    }

    // animation-delay (default: 0s)
    it = style.find("animation-delay");
    if (it != style.end()) {
        anim.delay = parse_animation_duration(it->second);
    } else {
        anim.delay = 0.0f;
    }

    // animation-iteration-count (default: 1)
    it = style.find("animation-iteration-count");
    if (it != style.end()) {
        anim.iteration_count = parse_animation_iteration_count(it->second);
    } else {
        anim.iteration_count = 1;
    }

    // animation-direction (default: normal)
    it = style.find("animation-direction");
    if (it != style.end()) {
        anim.direction = it->second;
    } else {
        anim.direction = "normal";
    }

    // animation-fill-mode (default: none)
    it = style.find("animation-fill-mode");
    if (it != style.end()) {
        anim.fill_mode = it->second;
    } else {
        anim.fill_mode = "none";
    }

    // Initialize runtime state
    anim.start_time = current_time;
    anim.current_iteration = 0;
    anim.active = !anim.animation_name.empty() && anim.duration > 0.0f;

    return anim;
}
