/*
 * flexUI - ComputedStyle Implementation
 */

#include <flexUI/computed_style.h>
#include <flexUI/detail/css_length.h>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>

namespace flexUI {

// Helper to trim string
static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) return str;
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, (last - first + 1));
}

namespace {

uint64_t hash_combine(uint64_t seed, uint64_t value) {
  return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2));
}

bool ends_with_copy(const std::string& value, const std::string& suffix) {
  return value.size() >= suffix.size() &&
         value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<std::string> split_top_level_csv(const std::string& value) {
  std::vector<std::string> items;
  std::string current;
  int paren_depth = 0;

  for (char ch : value) {
    if (ch == '(') {
      ++paren_depth;
    } else if (ch == ')' && paren_depth > 0) {
      --paren_depth;
    }

    if (ch == ',' && paren_depth == 0) {
      const std::string item = detail::trim_css_copy(current);
      if (!item.empty()) {
        items.push_back(item);
      }
      current.clear();
      continue;
    }

    current.push_back(ch);
  }

  const std::string tail = detail::trim_css_copy(current);
  if (!tail.empty()) {
    items.push_back(tail);
  }
  return items;
}

std::string join_space_separated_tokens(const std::vector<std::string>& tokens) {
  std::ostringstream out;
  for (size_t i = 0; i < tokens.size(); ++i) {
    if (i > 0) {
      out << ' ';
    }
    out << tokens[i];
  }
  return out.str();
}

bool parse_serialized_variable_color(const std::string& value, Color& result) {
  std::istringstream ss(value);
  std::string token;
  int index = 0;

  while (std::getline(ss, token, ',') && index < 4) {
    const std::string trimmed = detail::trim_css_copy(token);
    if (trimmed.empty()) {
      return false;
    }

    char* end = nullptr;
    const float parsed = std::strtof(trimmed.c_str(), &end);
    if (end == trimmed.c_str() || (end && *end != '\0')) {
      return false;
    }

    float normalized = 0.0f;
    if (index < 3) {
      const float component = std::clamp(parsed, 0.0f, 255.0f);
      normalized = component / 255.0f;
    } else if (parsed > 1.0f) {
      normalized = std::clamp(parsed, 0.0f, 255.0f) / 255.0f;
    } else {
      normalized = std::clamp(parsed, 0.0f, 1.0f);
    }

    switch (index) {
      case 0: result.r = normalized; break;
      case 1: result.g = normalized; break;
      case 2: result.b = normalized; break;
      case 3: result.a = normalized; break;
    }
    ++index;
  }

  return index == 4;
}

Color parse_css_color_value(const std::string& value, const Color& fallback) {
  const std::string input = detail::lower_css_copy(detail::trim_css_copy(value));
  if (input.empty()) {
    return fallback;
  }

  if (input[0] == '#') {
    std::string hex = input.substr(1);
    if (hex.size() == 3) {
      hex = std::string(2, hex[0]) + std::string(2, hex[1]) +
            std::string(2, hex[2]);
    }
    if (hex.size() == 6) {
      unsigned int r = 0;
      unsigned int g = 0;
      unsigned int b = 0;
      std::sscanf(hex.c_str(), "%2x%2x%2x", &r, &g, &b);
      return Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
    }
    if (hex.size() == 8) {
      unsigned int r = 0;
      unsigned int g = 0;
      unsigned int b = 0;
      unsigned int a = 0;
      std::sscanf(hex.c_str(), "%2x%2x%2x%2x", &r, &g, &b, &a);
      return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
    }
  }

  const auto parse_alpha_token = [](const std::string& token) {
    if (token.empty()) {
      return 1.0f;
    }
    if (token.back() == '%') {
      return std::clamp(
          std::strtof(token.substr(0, token.size() - 1).c_str(), nullptr) /
              100.0f,
          0.0f, 1.0f);
    }
    return std::clamp(std::strtof(token.c_str(), nullptr), 0.0f, 1.0f);
  };

  const auto normalize_color_function_tokens = [](std::string inner) {
    for (char& c : inner) {
      if (c == ',' || c == '/') {
        c = ' ';
      }
    }
    return detail::split_css_tokens(inner);
  };

  if (input.rfind("light-dark(", 0) == 0 && input.back() == ')') {
    const std::string inner = input.substr(11, input.size() - 12);
    const auto items = split_top_level_csv(inner);
    if (!items.empty()) {
      return parse_css_color_value(items.front(), fallback);
    }
  }

  if (input.rfind("color-mix(", 0) == 0 && input.back() == ')') {
    struct MixPart {
      Color color;
      float weight = NAN;
    };

    const auto parse_mix_part = [&](const std::string& raw) -> MixPart {
      auto tokens = detail::split_css_tokens(detail::trim_css_copy(raw));
      float weight = NAN;
      if (!tokens.empty() && !tokens.back().empty() &&
          tokens.back().back() == '%') {
        weight = std::clamp(
            std::strtof(
                tokens.back().substr(0, tokens.back().size() - 1).c_str(),
                nullptr) /
                100.0f,
            0.0f, 1.0f);
        tokens.pop_back();
      }
      return {parse_css_color_value(join_space_separated_tokens(tokens), fallback),
              weight};
    };

    const std::string inner = input.substr(10, input.size() - 11);
    const auto items = split_top_level_csv(inner);
    if (items.size() >= 3) {
      MixPart first = parse_mix_part(items[1]);
      MixPart second = parse_mix_part(items[2]);
      if (std::isnan(first.weight) && std::isnan(second.weight)) {
        first.weight = 0.5f;
        second.weight = 0.5f;
      } else if (std::isnan(first.weight)) {
        first.weight = 1.0f - second.weight;
      } else if (std::isnan(second.weight)) {
        second.weight = 1.0f - first.weight;
      }

      const float sum = first.weight + second.weight;
      if (sum > 0.000001f) {
        first.weight /= sum;
        second.weight /= sum;
      }

      return Color(first.color.r * first.weight + second.color.r * second.weight,
                   first.color.g * first.weight + second.color.g * second.weight,
                   first.color.b * first.weight + second.color.b * second.weight,
                   first.color.a * first.weight + second.color.a * second.weight);
    }
  }

  const auto parse_rgb_component = [](const std::string& token) {
    if (!token.empty() && token.back() == '%') {
      return std::clamp(
          std::strtof(token.substr(0, token.size() - 1).c_str(), nullptr) /
              100.0f,
          0.0f, 1.0f);
    }
    return std::clamp(std::strtof(token.c_str(), nullptr) / 255.0f, 0.0f,
                      1.0f);
  };

  if (input.rfind("rgb", 0) == 0) {
    const size_t start = input.find('(');
    const size_t end = input.rfind(')');
    if (start != std::string::npos && end != std::string::npos && end > start) {
      const auto tokens = normalize_color_function_tokens(
          input.substr(start + 1, end - start - 1));
      if (tokens.size() >= 3) {
        return Color(parse_rgb_component(tokens[0]),
                     parse_rgb_component(tokens[1]),
                     parse_rgb_component(tokens[2]),
                     tokens.size() >= 4 ? parse_alpha_token(tokens[3]) : 1.0f);
      }
    }
  }

  const auto parse_percentage_token = [](const std::string& token) {
    if (!token.empty() && token.back() == '%') {
      return std::clamp(
          std::strtof(token.substr(0, token.size() - 1).c_str(), nullptr) /
              100.0f,
          0.0f, 1.0f);
    }
    return std::clamp(std::strtof(token.c_str(), nullptr), 0.0f, 1.0f);
  };

  const auto parse_angle_degrees = [](std::string token) {
    if (ends_with_copy(token, "deg")) {
      token = token.substr(0, token.size() - 3);
    } else if (ends_with_copy(token, "rad")) {
      return std::strtof(token.substr(0, token.size() - 3).c_str(), nullptr) *
             180.0f / 3.14159265358979323846f;
    } else if (ends_with_copy(token, "turn")) {
      return std::strtof(token.substr(0, token.size() - 4).c_str(), nullptr) *
             360.0f;
    }
    return std::strtof(token.c_str(), nullptr);
  };

  const auto hue_to_rgb = [](float p, float q, float t) {
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
  };

  if (input.rfind("hsl", 0) == 0) {
    const size_t start = input.find('(');
    const size_t end = input.rfind(')');
    if (start != std::string::npos && end != std::string::npos && end > start) {
      const auto tokens = normalize_color_function_tokens(
          input.substr(start + 1, end - start - 1));
      if (tokens.size() >= 3) {
        float h = std::fmod(std::strtof(tokens[0].c_str(), nullptr), 360.0f);
        if (h < 0.0f) h += 360.0f;
        h /= 360.0f;
        const float s = parse_percentage_token(tokens[1]);
        const float l = parse_percentage_token(tokens[2]);
        const float a = tokens.size() >= 4 ? parse_alpha_token(tokens[3]) : 1.0f;

        if (s <= 0.0f) {
          return Color(l, l, l, a);
        }

        const float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        const float p = 2.0f * l - q;
        return Color(hue_to_rgb(p, q, h + 1.0f / 3.0f),
                     hue_to_rgb(p, q, h),
                     hue_to_rgb(p, q, h - 1.0f / 3.0f), a);
      }
    }
  }

  const auto linear_to_srgb = [](float component) {
    component = std::clamp(component, 0.0f, 1.0f);
    if (component <= 0.0031308f) {
      return 12.92f * component;
    }
    return 1.055f * std::pow(component, 1.0f / 2.4f) - 0.055f;
  };

  const auto parse_ok_lightness_token = [](const std::string& token) {
    if (!token.empty() && token.back() == '%') {
      return std::clamp(
          std::strtof(token.substr(0, token.size() - 1).c_str(), nullptr) /
              100.0f,
          0.0f, 1.0f);
    }
    return std::clamp(std::strtof(token.c_str(), nullptr), 0.0f, 1.0f);
  };

  if (input.rfind("oklch", 0) == 0 || input.rfind("oklab", 0) == 0) {
    const bool is_oklch = input.rfind("oklch", 0) == 0;
    const size_t start = input.find('(');
    const size_t end = input.rfind(')');
    if (start != std::string::npos && end != std::string::npos && end > start) {
      const auto tokens = normalize_color_function_tokens(
          input.substr(start + 1, end - start - 1));
      if (tokens.size() >= 3) {
        const float l = parse_ok_lightness_token(tokens[0]);
        const float alpha =
            tokens.size() >= 4 ? parse_alpha_token(tokens[3]) : 1.0f;
        float ok_a = 0.0f;
        float ok_b = 0.0f;
        if (is_oklch) {
          const float c =
              std::max(std::strtof(tokens[1].c_str(), nullptr), 0.0f);
          const float h = parse_angle_degrees(tokens[2]) *
                          3.14159265358979323846f / 180.0f;
          ok_a = c * std::cos(h);
          ok_b = c * std::sin(h);
        } else {
          ok_a = std::strtof(tokens[1].c_str(), nullptr);
          ok_b = std::strtof(tokens[2].c_str(), nullptr);
        }

        const float l_ = l + 0.3963377774f * ok_a + 0.2158037573f * ok_b;
        const float m_ = l - 0.1055613458f * ok_a - 0.0638541728f * ok_b;
        const float s_ = l - 0.0894841775f * ok_a - 1.2914855480f * ok_b;
        const float l3 = l_ * l_ * l_;
        const float m3 = m_ * m_ * m_;
        const float s3 = s_ * s_ * s_;

        const float r_linear =
            4.0767416621f * l3 - 3.3077115913f * m3 + 0.2309699292f * s3;
        const float g_linear =
            -1.2684380046f * l3 + 2.6097574011f * m3 - 0.3413193965f * s3;
        const float b_linear =
            -0.0041960863f * l3 - 0.7034186147f * m3 + 1.7076147010f * s3;
        return Color(linear_to_srgb(r_linear), linear_to_srgb(g_linear),
                     linear_to_srgb(b_linear), alpha);
      }
    }
  }

  if (input == "transparent") return Color(0.0f, 0.0f, 0.0f, 0.0f);
  if (input == "black") return Color(0.0f, 0.0f, 0.0f, 1.0f);
  if (input == "white") return Color(1.0f, 1.0f, 1.0f, 1.0f);
  if (input == "red") return Color(1.0f, 0.0f, 0.0f, 1.0f);
  if (input == "green") return Color(0.0f, 0.5f, 0.0f, 1.0f);
  if (input == "blue") return Color(0.0f, 0.0f, 1.0f, 1.0f);
  if (input == "yellow") return Color(1.0f, 1.0f, 0.0f, 1.0f);
  if (input == "gray" || input == "grey") return Color(0.5f, 0.5f, 0.5f, 1.0f);

  return fallback;
}

} // namespace

std::string ComputedStyle::resolve_variable_value(const std::string& value, int depth) const {
    if (depth > 10) return value; // Prevent recursion limit
    
    std::string result = value;
    size_t var_pos = 0;
    
    // Iterate over all var() occurrences
    // Note: This simple parser handles "var(--a)" but assumes no nested parenthesis inside fallback unless careful.
    // For robust CSS parsing we might need more, but for this demo:
    while ((var_pos = result.find("var(", var_pos)) != std::string::npos) {
        size_t open_paren = var_pos + 4;
        
        // Handle nested parentheses in fallback (e.g. var(--a, var(--b)))
        // Simple counter approach
        int paren_depth = 1;
        size_t current = open_paren;
        size_t found_close = std::string::npos;
        
        while (current < result.size()) {
            if (result[current] == '(') paren_depth++;
            else if (result[current] == ')') paren_depth--;
            
            if (paren_depth == 0) {
                found_close = current;
                break;
            }
            current++;
        }
        
        if (found_close == std::string::npos) break; // Malformed
        
        std::string inner = result.substr(open_paren, found_close - open_paren);
        
        // Parse name and fallback
        std::string name;
        std::string fallback;
        
        size_t comma = inner.find(',');
        if (comma != std::string::npos) {
            name = trim(inner.substr(0, comma));
            fallback = trim(inner.substr(comma + 1));
        } else {
            name = trim(inner);
        }
        
        // Resolve value
        std::string resolved_val;
        bool found = false;
        
        // Look up variable
        auto it = variables.find(Symbol(name));
        if (it != variables.end()) {
            resolved_val = resolve_variable_value(it->second, depth + 1);
            found = true;
        } else if (!fallback.empty()) {
            resolved_val = resolve_variable_value(fallback, depth + 1);
            found = true;
        }
        
        if (found) {
            result.replace(var_pos, found_close - var_pos + 1, resolved_val);
            // Don't advance var_pos, we might need to resolve what we just replaced if it contained vars 
            // (though we already recursively resolved, so we should be good, but safe to stay or advance carefully)
            // But if resolved_val contains "var(", infinite loop?
            // resolve_variable_value is recursive, so resolved_val should be fully resolved.
            // So we can advance.
            var_pos += resolved_val.length();
        } else {
             // Not found and no fallback, leave as is or empty? CSS says invalid at computed value time usually triggers unset/initial.
             // We'll leave it empty to avoid garbage parsing
             result.replace(var_pos, found_close - var_pos + 1, "");
        }
    }
    
    return result;
}

std::string ComputedStyle::get_variable(Symbol name, const std::string& default_value) const {
    auto it = variables.find(name);
    if (it == variables.end()) return default_value;
    return resolve_variable_value(it->second);
}

Color ComputedStyle::get_variable_color(Symbol name,
                                        const Color& default_color) const {
  auto it = variables.find(name);
  if (it == variables.end()) {
    return default_color;
  }

  const std::string& raw_value = it->second;
  auto cache_it = color_cache.find(name);
  if (raw_value.find("var(") == std::string::npos &&
      cache_it != color_cache.end() && cache_it->second.resolved == raw_value) {
    return cache_it->second.color;
  }

  // Resolve var() references
  std::string value = resolve_variable_value(raw_value);
  if (value.empty()) return default_color;
  if (cache_it != color_cache.end() && cache_it->second.resolved == value) {
    return cache_it->second.color;
  }

  Color result = default_color;
  if (parse_serialized_variable_color(value, result)) {
    color_cache[name] = CachedColorValue{value, result};
    return result;
  }
  result = parse_css_color_value(value, default_color);
  if (result.r != default_color.r || result.g != default_color.g ||
      result.b != default_color.b || result.a != default_color.a) {
    color_cache[name] = CachedColorValue{value, result};
  }
  return result;
}

float ComputedStyle::get_variable_float(Symbol name, float default_value) const {
    auto it = variables.find(name);
    if (it == variables.end()) return default_value;

    const std::string& raw_value = it->second;
    auto cache_it = float_cache.find(name);
    if (raw_value.find("var(") == std::string::npos &&
        cache_it != float_cache.end() && cache_it->second.resolved == raw_value) {
        return cache_it->second.value;
    }

    std::string value = resolve_variable_value(raw_value);
    if (value.empty()) return default_value;
    if (cache_it != float_cache.end() && cache_it->second.resolved == value) {
        return cache_it->second.value;
    }

    const float result = std::strtof(value.c_str(), nullptr);
    float_cache[name] = CachedFloatValue{value, result};
    return result;
}

uint64_t ComputedStyle::variables_signature() const {
    uint64_t seed = 1469598103934665603ULL;
    seed = hash_combine(seed, static_cast<uint64_t>(variables.size()));
    for (const auto& [name, value] : variables) {
        const uint64_t item =
            hash_combine(static_cast<uint64_t>(name.id),
                         static_cast<uint64_t>(std::hash<std::string>{}(value)));
        seed ^= item;
    }
    return seed;
}

} // namespace flexUI
