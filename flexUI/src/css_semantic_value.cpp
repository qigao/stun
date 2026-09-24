#include <flexUI/detail/css_semantic_value.h>
#include <flexUI/detail/css_length.h>

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace flexUI::detail {
namespace {

std::string_view trim_ascii(std::string_view value) noexcept {
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.front()))) {
        value.remove_prefix(1);
    }
    while (!value.empty() &&
           std::isspace(static_cast<unsigned char>(value.back()))) {
        value.remove_suffix(1);
    }
    return value;
}

std::string ascii_lower_copy(std::string_view value) {
    std::string result(value);
    for (char& ch : result) {
        ch = static_cast<char>(
            std::tolower(static_cast<unsigned char>(ch)));
    }
    return result;
}

bool is_css_wide_keyword(std::string_view value) noexcept {
    return value == "inherit" || value == "initial" || value == "unset" ||
           value == "revert" || value == "revert-layer";
}

bool contains_deferred_function(std::string_view value) noexcept {
    return value.find("var(") != std::string_view::npos ||
           value.find("calc(") != std::string_view::npos ||
           value.find("env(") != std::string_view::npos;
}

bool is_digit(char ch) noexcept {
    return ch >= '0' && ch <= '9';
}

std::string trim_copy(const std::string& value) {
    const std::size_t start = value.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return {};
    const std::size_t end = value.find_last_not_of(" \t\n\r");
    return value.substr(start, end - start + 1);
}

std::string to_lower_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
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
            const std::string item = trim_copy(current);
            if (!item.empty()) items.push_back(item);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    const std::string tail = trim_copy(current);
    if (!tail.empty()) items.push_back(tail);
    return items;
}

std::vector<std::string> split_css_tokens(const std::string& value) {
    std::vector<std::string> tokens;
    std::string current;
    int paren_depth = 0;
    for (char ch : value) {
        if (ch == '(') {
            ++paren_depth;
        } else if (ch == ')' && paren_depth > 0) {
            --paren_depth;
        }
        if (std::isspace(static_cast<unsigned char>(ch)) && paren_depth == 0) {
            const std::string token = trim_copy(current);
            if (!token.empty()) tokens.push_back(token);
            current.clear();
            continue;
        }
        current.push_back(ch);
    }
    const std::string tail = trim_copy(current);
    if (!tail.empty()) tokens.push_back(tail);
    return tokens;
}

std::string join_space_separated_tokens(const std::vector<std::string>& tokens) {
    std::string joined;
    for (const auto& token : tokens) {
        if (!joined.empty()) joined.push_back(' ');
        joined += token;
    }
    return joined;
}

bool full_float_token(const std::string& token) {
    if (token.empty()) return false;
    char* end = nullptr;
    errno = 0;
    std::strtof(token.c_str(), &end);
    return end != token.c_str() && end && *end == '\0' && errno != ERANGE;
}

bool is_hex_color(const std::string& input) {
    if (input.empty() || input.front() != '#') return false;
    const std::size_t digits = input.size() - 1;
    if (digits != 3 && digits != 6 && digits != 8) return false;
    return std::all_of(input.begin() + 1, input.end(), [](unsigned char ch) {
        return std::isxdigit(ch) != 0;
    });
}

bool is_supported_color_function(const std::string& input) {
    const auto has_function_shape = [&](const char* prefix) {
        return input.rfind(prefix, 0) == 0 && !input.empty() &&
               input.back() == ')';
    };

    if (has_function_shape("light-dark(")) {
        const auto items =
            split_top_level_csv(input.substr(11, input.size() - 12));
        return !items.empty();
    }
    if (has_function_shape("color-mix(")) {
        const auto items =
            split_top_level_csv(input.substr(10, input.size() - 11));
        return items.size() >= 3;
    }

    const auto function_tokens = [&](std::size_t start) {
        const std::size_t end = input.rfind(')');
        if (start == std::string::npos || end == std::string::npos ||
            end <= start) {
            return std::vector<std::string>{};
        }
        std::string inner = input.substr(start + 1, end - start - 1);
        for (char& ch : inner) {
            if (ch == ',' || ch == '/') ch = ' ';
        }
        return split_css_tokens(inner);
    };

    if (input.rfind("rgb", 0) == 0) {
        return function_tokens(input.find('(')).size() >= 3;
    }
    if (input.rfind("hsl", 0) == 0) {
        return function_tokens(input.find('(')).size() >= 3;
    }
    if (input.rfind("oklch", 0) == 0 || input.rfind("oklab", 0) == 0) {
        return function_tokens(input.find('(')).size() >= 3;
    }
    return false;
}

bool is_concrete_color_spelling(const std::string& input) {
    const auto serialized = split_top_level_csv(input);
    if (serialized.size() == 4 &&
        std::all_of(serialized.begin(), serialized.end(),
                    [](const std::string& token) {
                        return full_float_token(trim_copy(token));
                    })) {
        return true;
    }

    if (is_hex_color(input) || is_supported_color_function(input)) {
        return true;
    }

    return input == "transparent" || input == "black" || input == "white" ||
           input == "red" || input == "green" || input == "blue" ||
           input == "yellow" || input == "gray" || input == "grey";
}

Color parse_color_legacy(const std::string& value) {
    const std::string input = to_lower_copy(trim_copy(value));
    if (input.empty())
      return Color(0.0f, 0.0f, 0.0f, 0.0f);

    // Widget bridge variables historically store RGBA as four comma-separated
    // components. Accept that representation after var() resolution so the
    // same token can feed standard CSS color properties on semantic parts.
    const auto serialized = split_top_level_csv(input);
    if (serialized.size() == 4) {
      float channels[4] = {};
      bool valid = true;
      for (size_t i = 0; i < serialized.size(); ++i) {
        const std::string token = trim_copy(serialized[i]);
        char* end = nullptr;
        channels[i] = std::strtof(token.c_str(), &end);
        if (end == token.c_str() || (end && *end != '\0')) {
          valid = false;
          break;
        }
      }
      if (valid) {
        constexpr float channel_max = 255.0f;
        const auto rgb = [channel_max](float channel) {
          return std::clamp(channel, 0.0f, channel_max) / channel_max;
        };
        const float alpha = channels[3] > 1.0f
                                ? rgb(channels[3])
                                : std::clamp(channels[3], 0.0f, 1.0f);
        return Color(rgb(channels[0]), rgb(channels[1]), rgb(channels[2]),
                     alpha);
      }
    }

    // Hex color
    if (input[0] == '#') {
      std::string hex = input.substr(1);
      if (hex.size() == 3) {
        // #RGB -> #RRGGBB
        hex = std::string(2, hex[0]) + std::string(2, hex[1]) + std::string(2, hex[2]);
      }
      if (hex.size() == 6) {
        unsigned int r, g, b;
        sscanf(hex.c_str(), "%2x%2x%2x", &r, &g, &b);
        return Color(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
      }
      if (hex.size() == 8) {
        unsigned int r, g, b, a;
        sscanf(hex.c_str(), "%2x%2x%2x%2x", &r, &g, &b, &a);
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
      return split_css_tokens(inner);
    };

    if (input.rfind("light-dark(", 0) == 0 && input.back() == ')') {
      const std::string inner = input.substr(11, input.size() - 12);
      const auto items = split_top_level_csv(inner);
      if (!items.empty()) {
        // flexUI currently has a stable light default unless explicit media rules
        // override theme tokens.
        return parse_color_legacy(items.front());
      }
    }

    if (input.rfind("color-mix(", 0) == 0 && input.back() == ')') {
      struct MixPart {
        Color color;
        float weight = NAN;
      };

      const auto parse_mix_part = [&](const std::string& raw) -> MixPart {
        auto tokens = split_css_tokens(trim_copy(raw));
        float weight = NAN;
        if (!tokens.empty() && !tokens.back().empty() && tokens.back().back() == '%') {
          weight = std::clamp(
              std::strtof(tokens.back().substr(0, tokens.back().size() - 1).c_str(),
                          nullptr) /
                  100.0f,
              0.0f, 1.0f);
          tokens.pop_back();
        }
        return {parse_color_legacy(join_space_separated_tokens(tokens)), weight};
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

    // rgb()/rgba() support both comma and modern slash-alpha syntaxes.
    if (input.rfind("rgb", 0) == 0) {
      const size_t start = input.find('(');
      const size_t end = input.rfind(')');
      if (start != std::string::npos && end != std::string::npos && end > start) {
        const auto tokens =
            normalize_color_function_tokens(input.substr(start + 1, end - start - 1));
        if (tokens.size() >= 3) {
          return Color(parse_rgb_component(tokens[0]), parse_rgb_component(tokens[1]),
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

    const auto parse_oklch_lightness_token = [](const std::string& token) {
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

    // hsl()/hsla() is required for shadcn-style hsl(var(--token) / alpha).
    if (input.rfind("hsl", 0) == 0) {
      const size_t start = input.find('(');
      const size_t end = input.rfind(')');
      if (start != std::string::npos && end != std::string::npos && end > start) {
        const auto tokens =
            normalize_color_function_tokens(input.substr(start + 1, end - start - 1));
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

    const auto linear_to_srgb = [](float value) {
      value = std::clamp(value, 0.0f, 1.0f);
      if (value <= 0.0031308f) {
        return 12.92f * value;
      }
      return 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
    };

    // OKLCH is used by modern Tailwind/shadcn theme tokens.
    if (input.rfind("oklch", 0) == 0) {
      const size_t start = input.find('(');
      const size_t end = input.rfind(')');
      if (start != std::string::npos && end != std::string::npos && end > start) {
        const auto tokens =
            normalize_color_function_tokens(input.substr(start + 1, end - start - 1));
        if (tokens.size() >= 3) {
          const float l = parse_oklch_lightness_token(tokens[0]);
          const float c = std::max(std::strtof(tokens[1].c_str(), nullptr), 0.0f);
          const float h = parse_angle_degrees(tokens[2]) *
                          3.14159265358979323846f / 180.0f;
          const float a = tokens.size() >= 4 ? parse_alpha_token(tokens[3]) : 1.0f;

          const float ok_a = c * std::cos(h);
          const float ok_b = c * std::sin(h);
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
                       linear_to_srgb(b_linear), a);
        }
      }
    }

    if (input.rfind("oklab", 0) == 0) {
      const size_t start = input.find('(');
      const size_t end = input.rfind(')');
      if (start != std::string::npos && end != std::string::npos && end > start) {
        const auto tokens =
            normalize_color_function_tokens(input.substr(start + 1, end - start - 1));
        if (tokens.size() >= 3) {
          const float l = parse_oklch_lightness_token(tokens[0]);
          const float ok_a = std::strtof(tokens[1].c_str(), nullptr);
          const float ok_b = std::strtof(tokens[2].c_str(), nullptr);
          const float alpha = tokens.size() >= 4 ? parse_alpha_token(tokens[3]) : 1.0f;

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

    // Named colors (common ones) - now using float [0.0-1.0]
    if (input == "transparent")
      return Color(0.0f, 0.0f, 0.0f, 0.0f);
    if (input == "black")
      return Color(0.0f, 0.0f, 0.0f, 1.0f);
    if (input == "white")
      return Color(1.0f, 1.0f, 1.0f, 1.0f);
    if (input == "red")
      return Color(1.0f, 0.0f, 0.0f, 1.0f);
    if (input == "green")
      return Color(0.0f, 0.5f, 0.0f, 1.0f); // CSS green is #008000
    if (input == "blue")
      return Color(0.0f, 0.0f, 1.0f, 1.0f);
    if (input == "yellow")
      return Color(1.0f, 1.0f, 0.0f, 1.0f);
    if (input == "gray" || input == "grey")
      return Color(0.5f, 0.5f, 0.5f, 1.0f);

    return Color(0.0f, 0.0f, 0.0f, 1.0f); // Default: black
  }


} // namespace

CssLiteralResult<float> parse_css_number_literal(
    std::string_view raw_value) noexcept {
    const std::string_view trimmed = trim_ascii(raw_value);
    if (trimmed.empty()) {
        return {};
    }

    const std::string lowered = ascii_lower_copy(trimmed);
    if (is_css_wide_keyword(lowered) ||
        contains_deferred_function(lowered) ||
        (!lowered.empty() && lowered.back() == '%')) {
        return {CssLiteralState::Deferred, 0.0f};
    }

    // CSS <number> grammar:
    //   [+-]?([0-9]*\.[0-9]+|[0-9]+)([eE][+-]?[0-9]+)?
    // Validate the grammar before strtof so C-only spellings such as
    // hexadecimal floats, infinities and NaNs cannot enter the CSS cache.
    std::size_t i = 0;
    if (trimmed[i] == '+' || trimmed[i] == '-') {
        ++i;
    }
    if (i == trimmed.size()) {
        return {};
    }

    const std::size_t integer_begin = i;
    while (i < trimmed.size() && is_digit(trimmed[i])) {
        ++i;
    }
    const bool has_integer_digits = i > integer_begin;

    bool has_fraction_digits = false;
    if (i < trimmed.size() && trimmed[i] == '.') {
        ++i;
        const std::size_t fraction_begin = i;
        while (i < trimmed.size() && is_digit(trimmed[i])) {
            ++i;
        }
        has_fraction_digits = i > fraction_begin;
        if (!has_fraction_digits) {
            return {};
        }
    }

    if (!has_integer_digits && !has_fraction_digits) {
        return {};
    }

    if (i < trimmed.size() &&
        (trimmed[i] == 'e' || trimmed[i] == 'E')) {
        ++i;
        if (i < trimmed.size() &&
            (trimmed[i] == '+' || trimmed[i] == '-')) {
            ++i;
        }
        const std::size_t exponent_begin = i;
        while (i < trimmed.size() && is_digit(trimmed[i])) {
            ++i;
        }
        if (i == exponent_begin) {
            return {};
        }
    }

    if (i != trimmed.size()) {
        return {};
    }

    const std::string value(trimmed);
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(value.c_str(), &end);
    if (end != value.c_str() + value.size() ||
        errno == ERANGE || !std::isfinite(parsed)) {
        return {};
    }

    return {CssLiteralState::Concrete, parsed};
}

CssLiteralResult<float> parse_css_angle_literal(
    std::string_view raw_value) noexcept {
    const std::string_view trimmed = trim_ascii(raw_value);
    if (trimmed.empty()) {
        return {};
    }

    const std::string lowered = ascii_lower_copy(trimmed);
    if (is_css_wide_keyword(lowered) ||
        contains_deferred_function(lowered) ||
        lowered.rfind("min(", 0) == 0 ||
        lowered.rfind("max(", 0) == 0 ||
        lowered.rfind("clamp(", 0) == 0) {
        return {CssLiteralState::Deferred, 0.0f};
    }

    const auto parse_unit = [&](std::string_view unit, float scale)
        -> CssLiteralResult<float> {
        if (lowered.size() <= unit.size() ||
            lowered.compare(lowered.size() - unit.size(), unit.size(), unit) != 0) {
            return {};
        }
        const auto number = parse_css_number_literal(
            std::string_view(lowered).substr(0, lowered.size() - unit.size()));
        if (!number.is_concrete()) {
            return number.is_deferred()
                       ? CssLiteralResult<float>{CssLiteralState::Deferred, 0.0f}
                       : CssLiteralResult<float>{};
        }
        return {CssLiteralState::Concrete, number.value * scale};
    };

    if (const auto turn = parse_unit("turn", 360.0f); turn.is_concrete()) {
        return turn;
    }
    if (const auto grad = parse_unit("grad", 0.9f); grad.is_concrete()) {
        return grad;
    }
    if (const auto deg = parse_unit("deg", 1.0f); deg.is_concrete()) {
        return deg;
    }
    if (const auto rad =
            parse_unit("rad", 180.0f / 3.14159265358979323846f);
        rad.is_concrete()) {
        return rad;
    }

    return parse_css_number_literal(lowered);
}

CssLiteralResult<Color> parse_css_color_literal(
    std::string_view raw_value) noexcept {
    const std::string_view trimmed = trim_ascii(raw_value);
    if (trimmed.empty()) {
        return {};
    }

    const std::string lowered = ascii_lower_copy(trimmed);
    if (is_css_wide_keyword(lowered) ||
        contains_deferred_function(lowered) ||
        lowered.find("currentcolor") != std::string::npos) {
        return {CssLiteralState::Deferred, {}};
    }

    if (!is_concrete_color_spelling(lowered)) {
        return {};
    }

    return {CssLiteralState::Concrete,
            parse_color_legacy(std::string(trimmed))};
}

CssLiteralResult<float> parse_css_context_independent_length_literal(
    std::string_view raw_value) noexcept {
    const std::string_view trimmed = trim_ascii(raw_value);
    if (trimmed.empty()) {
        return {};
    }

    const std::string lowered = ascii_lower_copy(trimmed);
    if (is_css_wide_keyword(lowered) ||
        contains_deferred_function(lowered) ||
        lowered.rfind("min(", 0) == 0 ||
        lowered.rfind("max(", 0) == 0 ||
        lowered.rfind("clamp(", 0) == 0) {
        return {CssLiteralState::Deferred, 0.0f};
    }

    const auto parse_known_unit = [&](std::string_view unit,
                                      bool deferred)
        -> CssLiteralResult<float> {
        if (lowered.size() <= unit.size() ||
            lowered.compare(lowered.size() - unit.size(), unit.size(), unit) != 0) {
            return {};
        }
        const auto number = parse_css_number_literal(
            std::string_view(lowered).substr(0, lowered.size() - unit.size()));
        if (!number.is_concrete()) {
            return {};
        }
        if (deferred) {
            return {CssLiteralState::Deferred, 0.0f};
        }
        const float parsed = parse_css_length(lowered);
        return std::isfinite(parsed)
                   ? CssLiteralResult<float>{CssLiteralState::Concrete, parsed}
                   : CssLiteralResult<float>{};
    };

    if (const auto px = parse_known_unit("px", false);
        px.is_concrete()) {
        return px;
    }

    static constexpr std::string_view kDeferredUnits[] = {
        "rem", "em", "%", "vmin", "vmax",
        "svw", "svh", "svi", "svb",
        "lvw", "lvh", "lvi", "lvb",
        "dvw", "dvh", "dvi", "dvb",
        "vw", "vh", "vi", "vb",
    };
    for (const auto unit : kDeferredUnits) {
        const auto result = parse_known_unit(unit, true);
        if (result.is_deferred()) {
            return result;
        }
    }

    const auto number = parse_css_number_literal(lowered);
    if (!number.is_concrete()) {
        return {};
    }
    const float parsed = parse_css_length(lowered);
    return std::isfinite(parsed)
               ? CssLiteralResult<float>{CssLiteralState::Concrete, parsed}
               : CssLiteralResult<float>{};
}

CssLiteralResult<float> parse_css_border_width_literal(
    std::string_view raw_value) noexcept {
    const std::string lowered = ascii_lower_copy(trim_ascii(raw_value));
    if (lowered == "thin") {
        return {CssLiteralState::Concrete, 1.0f};
    }
    if (lowered == "medium") {
        return {CssLiteralState::Concrete, 3.0f};
    }
    if (lowered == "thick") {
        return {CssLiteralState::Concrete, 5.0f};
    }
    return parse_css_context_independent_length_literal(raw_value);
}

} // namespace flexUI::detail
