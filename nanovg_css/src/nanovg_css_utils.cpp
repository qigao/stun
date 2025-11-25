/*
 * NanoVG CSS - Utility functions
 */

#include "nanovg_css_internal.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <limits>  // Sprint 25: for std::numeric_limits

namespace nvgcss_utils {

// ============================================================================
// Sprint 24: CSS calc() Function - Expression Parser
// ============================================================================

// Token types for calc() expressions
enum class CalcTokenType {
    NUMBER,
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    LPAREN,
    RPAREN,
    END
};

struct CalcToken {
    CalcTokenType type;
    float value;
    std::string unit;
};

// Tokenizer for calc() expressions
std::vector<CalcToken> tokenize_calc(const std::string& expr) {
    std::vector<CalcToken> tokens;
    size_t i = 0;

    while (i < expr.length()) {
        // Skip whitespace
        while (i < expr.length() && std::isspace(expr[i])) i++;
        if (i >= expr.length()) break;

        // Operators and parentheses
        if (expr[i] == '+') {
            tokens.push_back({CalcTokenType::PLUS, 0, ""});
            i++;
        }
        else if (expr[i] == '-') {
            // Check if this is a negative number or subtraction operator
            // Negative if: start of expression, after operator, or after (
            bool is_negative = (tokens.empty() ||
                               tokens.back().type == CalcTokenType::PLUS ||
                               tokens.back().type == CalcTokenType::MINUS ||
                               tokens.back().type == CalcTokenType::MULTIPLY ||
                               tokens.back().type == CalcTokenType::DIVIDE ||
                               tokens.back().type == CalcTokenType::LPAREN);

            if (is_negative && i + 1 < expr.length() && (std::isdigit(expr[i + 1]) || expr[i + 1] == '.')) {
                // Parse as negative number
                i++;  // skip '-'
                size_t start = i;
                while (i < expr.length() && (std::isdigit(expr[i]) || expr[i] == '.')) {
                    i++;
                }
                float value = -std::stof(expr.substr(start, i - start));

                // Check for unit
                std::string unit;
                if (i < expr.length() && expr[i] == '%') {
                    unit = "%";
                    i++;
                } else if (i + 1 < expr.length() && expr.substr(i, 2) == "px") {
                    unit = "px";
                    i += 2;
                } else if (i + 1 < expr.length() && expr.substr(i, 2) == "em") {
                    unit = "em";
                    i += 2;
                } else if (i + 2 < expr.length() && expr.substr(i, 3) == "rem") {
                    unit = "rem";
                    i += 3;
                }

                tokens.push_back({CalcTokenType::NUMBER, value, unit});
            } else {
                tokens.push_back({CalcTokenType::MINUS, 0, ""});
                i++;
            }
        }
        else if (expr[i] == '*') {
            tokens.push_back({CalcTokenType::MULTIPLY, 0, ""});
            i++;
        }
        else if (expr[i] == '/') {
            tokens.push_back({CalcTokenType::DIVIDE, 0, ""});
            i++;
        }
        else if (expr[i] == '(') {
            tokens.push_back({CalcTokenType::LPAREN, 0, ""});
            i++;
        }
        else if (expr[i] == ')') {
            tokens.push_back({CalcTokenType::RPAREN, 0, ""});
            i++;
        }
        // Numbers
        else if (std::isdigit(expr[i]) || expr[i] == '.') {
            size_t start = i;
            while (i < expr.length() && (std::isdigit(expr[i]) || expr[i] == '.')) {
                i++;
            }
            float value = std::stof(expr.substr(start, i - start));

            // Check for unit
            std::string unit;
            if (i < expr.length() && expr[i] == '%') {
                unit = "%";
                i++;
            } else if (i + 1 < expr.length() && expr.substr(i, 2) == "px") {
                unit = "px";
                i += 2;
            } else if (i + 1 < expr.length() && expr.substr(i, 2) == "em") {
                unit = "em";
                i += 2;
            } else if (i + 2 < expr.length() && expr.substr(i, 3) == "rem") {
                unit = "rem";
                i += 3;
            }

            tokens.push_back({CalcTokenType::NUMBER, value, unit});
        }
        else {
            // Unknown character, skip it
            i++;
        }
    }

    tokens.push_back({CalcTokenType::END, 0, ""});
    return tokens;
}

// Recursive descent parser for calc() expressions
class CalcParser {
public:
    CalcParser(const std::vector<CalcToken>& tokens, float context_value, float font_size)
        : tokens_(tokens), pos_(0), context_value_(context_value), font_size_(font_size) {}

    float parse() {
        float result = parse_expression();
        return result;
    }

private:
    // expression := term (('+' | '-') term)*
    float parse_expression() {
        float result = parse_term();

        while (current_token().type == CalcTokenType::PLUS ||
               current_token().type == CalcTokenType::MINUS) {
            CalcTokenType op = current_token().type;
            advance();
            float right = parse_term();

            if (op == CalcTokenType::PLUS) {
                result += right;
            } else {
                result -= right;
            }
        }

        return result;
    }

    // term := factor (('*' | '/') factor)*
    float parse_term() {
        float result = parse_factor();

        while (current_token().type == CalcTokenType::MULTIPLY ||
               current_token().type == CalcTokenType::DIVIDE) {
            CalcTokenType op = current_token().type;
            advance();
            float right = parse_factor();

            if (op == CalcTokenType::MULTIPLY) {
                result *= right;
            } else {
                if (right != 0.0f) {
                    result /= right;
                } else {
                    // Division by zero - return 0
                    result = 0.0f;
                }
            }
        }

        return result;
    }

    // factor := number unit? | '(' expression ')'
    float parse_factor() {
        if (current_token().type == CalcTokenType::LPAREN) {
            advance();  // consume '('
            float result = parse_expression();
            if (current_token().type == CalcTokenType::RPAREN) {
                advance();  // consume ')'
            }
            return result;
        }

        if (current_token().type == CalcTokenType::NUMBER) {
            float value = current_token().value;
            std::string unit = current_token().unit;
            advance();

            // Convert to pixels based on unit
            if (unit == "%") {
                return (value / 100.0f) * context_value_;
            } else if (unit == "px" || unit.empty()) {
                return value;
            } else if (unit == "em") {
                return value * font_size_;
            } else if (unit == "rem") {
                // Use font_size as base (assuming root font size)
                return value * font_size_;
            }
        }

        return 0.0f;
    }

    CalcToken current_token() const {
        if (pos_ < tokens_.size()) {
            return tokens_[pos_];
        }
        return {CalcTokenType::END, 0, ""};
    }

    void advance() {
        if (pos_ < tokens_.size()) {
            pos_++;
        }
    }

    std::vector<CalcToken> tokens_;
    size_t pos_;
    float context_value_;
    float font_size_;
};

// Parse and evaluate calc() expression
float parse_calc_expression(const std::string& expr, float context_value, float font_size) {
    try {
        auto tokens = tokenize_calc(expr);
        CalcParser parser(tokens, context_value, font_size);
        return parser.parse();
    } catch (const std::exception&) {
        // Parse error - return 0
        return 0.0f;
    }
}

// ============================================================================
// Sprint 25: min() / max() / clamp() Functions
// ============================================================================

/**
 * @brief Split function arguments by comma, respecting nested functions
 *
 * Handles nested functions like: "calc(100% - 20px), 800px"
 */
std::vector<std::string> split_function_args(const std::string& args) {
    std::vector<std::string> result;
    int paren_depth = 0;
    size_t start = 0;

    for (size_t i = 0; i < args.length(); i++) {
        if (args[i] == '(') {
            paren_depth++;
        } else if (args[i] == ')') {
            paren_depth--;
        } else if (args[i] == ',' && paren_depth == 0) {
            // Found argument separator at top level
            std::string arg = args.substr(start, i - start);
            result.push_back(trim(arg));
            start = i + 1;
        }
    }

    // Add last argument
    if (start < args.length()) {
        std::string arg = args.substr(start);
        result.push_back(trim(arg));
    }

    return result;
}

/**
 * @brief Parse and evaluate min() function
 *
 * Returns the smallest value from all arguments
 * Example: min(100px, 50%, 200px)
 */
float parse_min_function(const std::string& args_str, float context_value, float font_size) {
    auto args = split_function_args(args_str);

    if (args.empty()) {
        return 0.0f;
    }

    float result = std::numeric_limits<float>::max();

    for (const auto& arg : args) {
        // Recursively parse each argument (handles calc(), nested min/max, etc.)
        float value = parse_length(arg, context_value);
        result = std::min(result, value);
    }

    return result;
}

/**
 * @brief Parse and evaluate max() function
 *
 * Returns the largest value from all arguments
 * Example: max(100px, 50%, 200px)
 */
float parse_max_function(const std::string& args_str, float context_value, float font_size) {
    auto args = split_function_args(args_str);

    if (args.empty()) {
        return 0.0f;
    }

    float result = std::numeric_limits<float>::lowest();

    for (const auto& arg : args) {
        float value = parse_length(arg, context_value);
        result = std::max(result, value);
    }

    return result;
}

/**
 * @brief Parse and evaluate clamp() function
 *
 * Clamps a value between minimum and maximum
 * Syntax: clamp(min, preferred, max)
 * Example: clamp(12px, 2vw, 24px)
 */
float parse_clamp_function(const std::string& args_str, float context_value, float font_size) {
    auto args = split_function_args(args_str);

    if (args.size() != 3) {
        return 0.0f;  // clamp() requires exactly 3 arguments
    }

    float min_val = parse_length(args[0], context_value);
    float preferred_val = parse_length(args[1], context_value);
    float max_val = parse_length(args[2], context_value);

    // Clamp: max(min, min(preferred, max))
    return std::max(min_val, std::min(preferred_val, max_val));
}

// Helper: trim whitespace
std::string trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

// Helper: check suffix
bool ends_with(const std::string& str, const std::string& suffix) {
    if (suffix.length() > str.length()) return false;
    return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}

// Helper: split string
std::vector<std::string> split(const std::string& str, char delim) {
    std::vector<std::string> result;
    std::stringstream ss(str);
    std::string item;
    while (std::getline(ss, item, delim)) {
        result.push_back(trim(item));
    }
    return result;
}

// Parse color - minimal implementation
NVGcolor parse_color(const std::string& color_str) {
    std::string color = trim(color_str);

    // Named colors (minimal set)
    if (color == "red") return nvgRGB(255, 0, 0);
    if (color == "green") return nvgRGB(0, 255, 0);
    if (color == "blue") return nvgRGB(0, 0, 255);
    if (color == "white") return nvgRGB(255, 255, 255);
    if (color == "black") return nvgRGB(0, 0, 0);
    if (color == "transparent") return nvgRGBA(0, 0, 0, 0);
    if (color == "purple") return nvgRGB(128, 0, 128);
    if (color == "orange") return nvgRGB(255, 165, 0);
    if (color == "brown") return nvgRGB(165, 42, 42);

    // Hex colors: #rgb or #rrggbb
    if (color[0] == '#') {
        if (color.length() == 4) {
            // #rgb
            int r = std::stoi(color.substr(1, 1), nullptr, 16) * 17;
            int g = std::stoi(color.substr(2, 1), nullptr, 16) * 17;
            int b = std::stoi(color.substr(3, 1), nullptr, 16) * 17;
            return nvgRGB(r, g, b);
        } else if (color.length() == 7) {
            // #rrggbb
            int r = std::stoi(color.substr(1, 2), nullptr, 16);
            int g = std::stoi(color.substr(3, 2), nullptr, 16);
            int b = std::stoi(color.substr(5, 2), nullptr, 16);
            return nvgRGB(r, g, b);
        }
    }

    // rgb(r, g, b)
    if (color.substr(0, 4) == "rgb(") {
        size_t start = color.find('(');
        size_t end = color.find(')');
        if (start != std::string::npos && end != std::string::npos) {
            std::string values = color.substr(start + 1, end - start - 1);
            auto parts = split(values, ',');
            if (parts.size() == 3) {
                int r = std::stoi(parts[0]);
                int g = std::stoi(parts[1]);
                int b = std::stoi(parts[2]);
                return nvgRGB(r, g, b);
            }
        }
    }

    // rgba(r, g, b, a)
    if (color.substr(0, 5) == "rgba(") {
        size_t start = color.find('(');
        size_t end = color.find(')');
        if (start != std::string::npos && end != std::string::npos) {
            std::string values = color.substr(start + 1, end - start - 1);
            auto parts = split(values, ',');
            if (parts.size() == 4) {
                int r = std::stoi(parts[0]);
                int g = std::stoi(parts[1]);
                int b = std::stoi(parts[2]);
                float a = std::stof(parts[3]);
                return nvgRGBA(r, g, b, (int)(a * 255));
            }
        }
    }

    // Default: white
    return nvgRGB(255, 255, 255);
}

// Parse length - minimal implementation
float parse_length(const std::string& length_str, float context_value) {
    std::string str = trim(length_str);

    if (str.empty() || str == "auto") {
        return 0.0f;
    }

    // Sprint 25: Check for min() function
    if (str.length() >= 6 && str.substr(0, 4) == "min(" && str.back() == ')') {
        std::string args = str.substr(4, str.length() - 5);
        return parse_min_function(args, context_value, 16.0f);
    }

    // Sprint 25: Check for max() function
    if (str.length() >= 6 && str.substr(0, 4) == "max(" && str.back() == ')') {
        std::string args = str.substr(4, str.length() - 5);
        return parse_max_function(args, context_value, 16.0f);
    }

    // Sprint 25: Check for clamp() function
    if (str.length() >= 9 && str.substr(0, 6) == "clamp(" && str.back() == ')') {
        std::string args = str.substr(6, str.length() - 7);
        return parse_clamp_function(args, context_value, 16.0f);
    }

    // Sprint 24: Check for calc() function
    if (str.length() >= 7 && str.substr(0, 5) == "calc(" && str.back() == ')') {
        std::string calc_expr = str.substr(5, str.length() - 6);
        return parse_calc_expression(calc_expr, context_value);
    }

    // px (or no unit)
    if (ends_with(str, "px")) {
        return std::stof(str.substr(0, str.length() - 2));
    }

    // Percentage
    if (ends_with(str, "%")) {
        float percent = std::stof(str.substr(0, str.length() - 1));
        return (percent / 100.0f) * context_value;
    }

    // No unit - assume pixels
    try {
        return std::stof(str);
    } catch (const std::exception&) {
        // Parse error, return 0
        return 0.0f;
    }
}

// ============================================================================
// Animation & Transition Utilities (Phase 4)
// ============================================================================

// Apply easing function
float apply_easing(float t, EasingFunction easing) {
    // Clamp t to [0, 1]
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    switch (easing) {
        case EasingFunction::LINEAR:
            return t;

        case EasingFunction::EASE:
            // cubic-bezier(0.25, 0.1, 0.25, 1.0)
            return t * t * (3.0f - 2.0f * t);  // Smoothstep approximation

        case EasingFunction::EASE_IN:
            // cubic-bezier(0.42, 0, 1.0, 1.0)
            return t * t;

        case EasingFunction::EASE_OUT:
            // cubic-bezier(0, 0, 0.58, 1.0)
            return t * (2.0f - t);

        case EasingFunction::EASE_IN_OUT:
            // cubic-bezier(0.42, 0, 0.58, 1.0)
            if (t < 0.5f) {
                return 2.0f * t * t;
            } else {
                return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
            }

        case EasingFunction::CUBIC_BEZIER:
            // TODO: Custom cubic-bezier implementation
            return t;

        default:
            return t;
    }
}

// Interpolate float
float interpolate_float(float start, float end, float t) {
    return start + (end - start) * t;
}

// Interpolate color
NVGcolor interpolate_color(const NVGcolor& start, const NVGcolor& end, float t) {
    NVGcolor result;
    result.r = interpolate_float(start.r, end.r, t);
    result.g = interpolate_float(start.g, end.g, t);
    result.b = interpolate_float(start.b, end.b, t);
    result.a = interpolate_float(start.a, end.a, t);
    return result;
}

// Interpolate CSS value
std::string interpolate_value(const std::string& start_value,
                               const std::string& end_value,
                               float t) {
    std::string start_trimmed = trim(start_value);
    std::string end_trimmed = trim(end_value);

    // Try to interpolate as numbers (with optional units)
    try {
        // Check if both values are colors
        if ((start_trimmed[0] == '#' || start_trimmed.substr(0, 3) == "rgb") &&
            (end_trimmed[0] == '#' || end_trimmed.substr(0, 3) == "rgb")) {
            // Color interpolation
            NVGcolor start_color = parse_color(start_trimmed);
            NVGcolor end_color = parse_color(end_trimmed);
            NVGcolor result = interpolate_color(start_color, end_color, t);

            // Convert back to rgba string
            char buf[64];
            snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%.2f)",
                    (int)(result.r * 255),
                    (int)(result.g * 255),
                    (int)(result.b * 255),
                    result.a);
            return buf;
        }

        // Try numeric interpolation
        float start_num = 0.0f, end_num = 0.0f;
        std::string unit;

        // Parse start value
        bool start_is_num = false;
        if (ends_with(start_trimmed, "px")) {
            start_num = std::stof(start_trimmed.substr(0, start_trimmed.length() - 2));
            unit = "px";
            start_is_num = true;
        } else if (ends_with(start_trimmed, "%")) {
            start_num = std::stof(start_trimmed.substr(0, start_trimmed.length() - 1));
            unit = "%";
            start_is_num = true;
        } else if (ends_with(start_trimmed, "deg")) {
            start_num = std::stof(start_trimmed.substr(0, start_trimmed.length() - 3));
            unit = "deg";
            start_is_num = true;
        } else {
            // Try plain number
            start_num = std::stof(start_trimmed);
            start_is_num = true;
        }

        // Parse end value
        bool end_is_num = false;
        if (ends_with(end_trimmed, "px")) {
            end_num = std::stof(end_trimmed.substr(0, end_trimmed.length() - 2));
            end_is_num = true;
        } else if (ends_with(end_trimmed, "%")) {
            end_num = std::stof(end_trimmed.substr(0, end_trimmed.length() - 1));
            end_is_num = true;
        } else if (ends_with(end_trimmed, "deg")) {
            end_num = std::stof(end_trimmed.substr(0, end_trimmed.length() - 3));
            end_is_num = true;
        } else {
            end_num = std::stof(end_trimmed);
            end_is_num = true;
        }

        if (start_is_num && end_is_num) {
            float result = interpolate_float(start_num, end_num, t);
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f%s", result, unit.c_str());
            return buf;
        }
    } catch (const std::exception&) {
        // Not numeric, fall through
    }

    // Can't interpolate - return end value when t >= 0.5
    return (t >= 0.5f) ? end_value : start_value;
}

// Parse transition property
bool parse_transition(const std::string& transition_css,
                     std::string& out_property,
                     float& out_duration,
                     EasingFunction& out_easing) {
    std::string css = trim(transition_css);
    if (css.empty() || css == "none") {
        return false;
    }

    // Parse: "property duration timing-function"
    // Example: "all 0.3s ease-in-out"
    // Example: "opacity 0.5s linear"

    auto parts = split(css, ' ');
    if (parts.empty()) return false;

    // First part: property
    out_property = parts[0];

    // Second part: duration
    if (parts.size() > 1) {
        std::string duration_str = parts[1];
        if (ends_with(duration_str, "s")) {
            out_duration = std::stof(duration_str.substr(0, duration_str.length() - 1));
        } else if (ends_with(duration_str, "ms")) {
            out_duration = std::stof(duration_str.substr(0, duration_str.length() - 2)) / 1000.0f;
        } else {
            out_duration = std::stof(duration_str);  // Assume seconds
        }
    } else {
        out_duration = 0.0f;
    }

    // Third part: easing function
    if (parts.size() > 2) {
        std::string easing_str = parts[2];
        if (easing_str == "linear") {
            out_easing = EasingFunction::LINEAR;
        } else if (easing_str == "ease") {
            out_easing = EasingFunction::EASE;
        } else if (easing_str == "ease-in") {
            out_easing = EasingFunction::EASE_IN;
        } else if (easing_str == "ease-out") {
            out_easing = EasingFunction::EASE_OUT;
        } else if (easing_str == "ease-in-out") {
            out_easing = EasingFunction::EASE_IN_OUT;
        } else {
            out_easing = EasingFunction::EASE;  // Default
        }
    } else {
        out_easing = EasingFunction::EASE;  // Default
    }

    return true;
}

// ============================================================================
// Box Shadow Parsing (Sprint 27)
// ============================================================================

/**
 * @brief Check if a token looks like a color
 */
bool is_color_token(const std::string& token) {
    if (token.empty()) return false;

    // Hex color
    if (token[0] == '#') return true;

    // rgb() or rgba()
    if (token.length() >= 4 && token.substr(0, 3) == "rgb") return true;

    // Common named colors
    static const char* named_colors[] = {
        "black", "white", "red", "green", "blue",
        "yellow", "cyan", "magenta", "gray", "transparent"
    };

    for (const char* color : named_colors) {
        if (token == color) return true;
    }

    return false;
}

/**
 * @brief Split string by comma, respecting parentheses (for rgba)
 */
std::vector<std::string> split_by_comma_respecting_parens(const std::string& str) {
    std::vector<std::string> result;
    int depth = 0;
    size_t start = 0;

    for (size_t i = 0; i < str.length(); i++) {
        if (str[i] == '(') {
            depth++;
        } else if (str[i] == ')') {
            depth--;
        } else if (str[i] == ',' && depth == 0) {
            std::string part = str.substr(start, i - start);
            result.push_back(trim(part));
            start = i + 1;
        }
    }

    // Add last part
    if (start < str.length()) {
        std::string part = str.substr(start);
        result.push_back(trim(part));
    }

    return result;
}

/**
 * @brief Parse a single box-shadow definition
 *
 * Syntax: [inset?] <offset-x> <offset-y> <blur-radius?> <spread-radius?> <color?>
 * Example: "2px 4px 10px 2px rgba(0,0,0,0.3)"
 */
BoxShadow parse_single_box_shadow(const std::string& shadow_str) {
    BoxShadow shadow;

    // Tokenize by spaces (but keep rgba() together)
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < shadow_str.length()) {
        // Skip whitespace
        while (i < shadow_str.length() && std::isspace(shadow_str[i])) i++;
        if (i >= shadow_str.length()) break;

        // Start token
        size_t token_start = i;

        // Check for function (rgb, rgba, calc, etc.)
        if (shadow_str.find('(', i) != std::string::npos) {
            size_t paren_pos = shadow_str.find('(', i);
            if (paren_pos == i || (paren_pos - i <= 10 && shadow_str.substr(i, paren_pos - i).find_first_of(" \t") == std::string::npos)) {
                // It's a function - find matching closing paren
                i = paren_pos + 1;
                int depth = 1;
                while (i < shadow_str.length() && depth > 0) {
                    if (shadow_str[i] == '(') depth++;
                    else if (shadow_str[i] == ')') depth--;
                    i++;
                }
                tokens.push_back(trim(shadow_str.substr(token_start, i - token_start)));
                continue;
            }
        }

        // Regular token - read until space
        while (i < shadow_str.length() && !std::isspace(shadow_str[i])) {
            i++;
        }

        tokens.push_back(trim(shadow_str.substr(token_start, i - token_start)));
    }

    // Parse tokens
    size_t token_idx = 0;

    // Check for "inset" keyword
    if (token_idx < tokens.size() && tokens[token_idx] == "inset") {
        shadow.inset = true;
        token_idx++;
    }

    // Parse offset-x (required)
    if (token_idx < tokens.size()) {
        shadow.offset_x = parse_length(tokens[token_idx], 0);
        token_idx++;
    }

    // Parse offset-y (required)
    if (token_idx < tokens.size()) {
        shadow.offset_y = parse_length(tokens[token_idx], 0);
        token_idx++;
    }

    // Parse blur-radius (optional)
    if (token_idx < tokens.size() && !is_color_token(tokens[token_idx])) {
        shadow.blur_radius = parse_length(tokens[token_idx], 0);
        token_idx++;
    }

    // Parse spread-radius (optional)
    if (token_idx < tokens.size() && !is_color_token(tokens[token_idx])) {
        shadow.spread_radius = parse_length(tokens[token_idx], 0);
        token_idx++;
    }

    // Parse color (optional - remaining tokens)
    if (token_idx < tokens.size()) {
        // Collect remaining tokens as color
        std::string color_str;
        while (token_idx < tokens.size()) {
            if (!color_str.empty()) color_str += " ";
            color_str += tokens[token_idx];
            token_idx++;
        }
        shadow.color = parse_color(color_str);
    }

    return shadow;
}

/**
 * @brief Parse CSS box-shadow property
 *
 * Supports multiple shadows separated by commas.
 * Example: "0 2px 4px rgba(0,0,0,0.1), 0 8px 16px rgba(0,0,0,0.2)"
 */
std::vector<BoxShadow> parse_box_shadow(const std::string& shadow_css) {
    std::vector<BoxShadow> shadows;

    std::string trimmed = trim(shadow_css);
    if (trimmed.empty() || trimmed == "none") {
        return shadows;  // Empty vector for no shadows
    }

    // Split by comma (respecting parentheses in rgba())
    std::vector<std::string> shadow_strings = split_by_comma_respecting_parens(trimmed);

    for (const auto& shadow_str : shadow_strings) {
        if (!shadow_str.empty() && shadow_str != "none") {
            shadows.push_back(parse_single_box_shadow(shadow_str));
        }
    }

    return shadows;
}

// ============================================================================
// Text Shadow Parsing (Sprint 28)
// ============================================================================

/**
 * @brief Parse a single text-shadow definition
 *
 * Syntax: <offset-x> <offset-y> <blur-radius>? <color>?
 * Example: "2px 4px 10px rgba(0,0,0,0.3)"
 */
TextShadow parse_single_text_shadow(const std::string& shadow_str) {
    TextShadow shadow;

    // Tokenize by spaces (but keep rgba() together)
    std::vector<std::string> tokens;
    size_t i = 0;
    while (i < shadow_str.length()) {
        // Skip whitespace
        while (i < shadow_str.length() && std::isspace(shadow_str[i])) i++;
        if (i >= shadow_str.length()) break;

        // Start token
        size_t token_start = i;

        // Check for function (rgb, rgba, calc, etc.)
        if (shadow_str.find('(', i) != std::string::npos) {
            size_t paren_pos = shadow_str.find('(', i);
            if (paren_pos == i || (paren_pos - i <= 10 && shadow_str.substr(i, paren_pos - i).find_first_of(" \t") == std::string::npos)) {
                // It's a function - find matching closing paren
                i = paren_pos + 1;
                int depth = 1;
                while (i < shadow_str.length() && depth > 0) {
                    if (shadow_str[i] == '(') depth++;
                    else if (shadow_str[i] == ')') depth--;
                    i++;
                }
                tokens.push_back(trim(shadow_str.substr(token_start, i - token_start)));
                continue;
            }
        }

        // Regular token - read until space
        while (i < shadow_str.length() && !std::isspace(shadow_str[i])) {
            i++;
        }

        tokens.push_back(trim(shadow_str.substr(token_start, i - token_start)));
    }

    // Parse tokens
    size_t token_idx = 0;

    // Parse offset-x (required)
    if (token_idx < tokens.size()) {
        shadow.offset_x = parse_length(tokens[token_idx], 0);
        token_idx++;
    }

    // Parse offset-y (required)
    if (token_idx < tokens.size()) {
        shadow.offset_y = parse_length(tokens[token_idx], 0);
        token_idx++;
    }

    // Parse blur-radius (optional)
    if (token_idx < tokens.size() && !is_color_token(tokens[token_idx])) {
        shadow.blur_radius = parse_length(tokens[token_idx], 0);
        token_idx++;
    }

    // Parse color (optional - remaining tokens)
    if (token_idx < tokens.size()) {
        // Collect remaining tokens as color
        std::string color_str;
        while (token_idx < tokens.size()) {
            if (!color_str.empty()) color_str += " ";
            color_str += tokens[token_idx];
            token_idx++;
        }
        shadow.color = parse_color(color_str);
    }

    return shadow;
}

/**
 * @brief Parse CSS text-shadow property
 *
 * Supports multiple shadows separated by commas.
 * Example: "1px 1px 2px rgba(0,0,0,0.3), 0 0 10px rgba(255,255,255,0.8)"
 */
std::vector<TextShadow> parse_text_shadow(const std::string& shadow_css) {
    std::vector<TextShadow> shadows;

    std::string trimmed = trim(shadow_css);
    if (trimmed.empty() || trimmed == "none") {
        return shadows;  // Empty vector for no shadows
    }

    // Split by comma (respecting parentheses in rgba())
    std::vector<std::string> shadow_strings = split_by_comma_respecting_parens(trimmed);

    for (const auto& shadow_str : shadow_strings) {
        if (!shadow_str.empty() && shadow_str != "none") {
            shadows.push_back(parse_single_text_shadow(shadow_str));
        }
    }

    return shadows;
}


// ============================================================================
// Background Image Parsing (Sprint 31)
// ============================================================================

/**
 * @brief Parse background-image URL
 *
 * Extracts the path from url(...) syntax
 * @param image_css CSS value (e.g., "url(path/to/image.png)")
 * @return Image path, or empty string if invalid
 */
std::string parse_background_image(const std::string& image_css) {
    std::string trimmed = trim(image_css);

    if (trimmed == "none" || trimmed.empty()) {
        return "";
    }

    // Check for url() syntax
    if (trimmed.find("url(") == 0) {
        size_t start = 4;  // After "url("
        size_t end = trimmed.find(')', start);

        if (end != std::string::npos) {
            std::string url = trimmed.substr(start, end - start);
            url = trim(url);

            // Remove quotes if present
            if ((url.front() == '"' && url.back() == '"') ||
                (url.front() == '\'' && url.back() == '\'')) {
                url = url.substr(1, url.length() - 2);
            }

            return url;
        }
    }

    return "";
}

/**
 * @brief Parse background-size property
 *
 * Parses CSS background-size values
 * @param size_css CSS value (e.g., "cover", "contain", "100px 50px")
 * @param[out] mode Background size mode
 * @param[out] width Explicit width (-1 if not explicit)
 * @param[out] height Explicit height (-1 if not explicit)
 */
void parse_background_size(const std::string& size_css,
                           BackgroundSize& mode,
                           float& width,
                           float& height) {
    std::string trimmed = trim(size_css);

    // Default: auto
    mode = BG_SIZE_AUTO;
    width = -1.0f;
    height = -1.0f;

    if (trimmed.empty() || trimmed == "auto") {
        mode = BG_SIZE_AUTO;
    } else if (trimmed == "cover") {
        mode = BG_SIZE_COVER;
    } else if (trimmed == "contain") {
        mode = BG_SIZE_CONTAIN;
    } else {
        // Explicit dimensions: "100px" or "100px 50px"
        mode = BG_SIZE_EXPLICIT;

        auto parts = split(trimmed, ' ');
        if (parts.size() >= 1) {
            width = parse_length(parts[0], 0);
        }
        if (parts.size() >= 2) {
            height = parse_length(parts[1], 0);
        } else {
            height = width;  // Square if only one dimension
        }
    }
}

/**
 * @brief Parse background-position property
 *
 * Parses CSS background-position values
 * @param position_css CSS value (e.g., "center", "top left", "50% 25%", "10px 20px")
 * @return BackgroundPosition struct with parsed values
 */
BackgroundPosition parse_background_position(const std::string& position_css) {
    BackgroundPosition pos;
    std::string trimmed = trim(position_css);

    if (trimmed.empty()) {
        return pos;  // Default: 0% 0%
    }

    auto parts = split(trimmed, ' ');

    if (parts.size() == 1) {
        // Single value: applies to X, Y is center
        std::string x_str = parts[0];

        if (x_str == "left") {
            pos.x = 0.0f;
            pos.x_is_percent = true;
        } else if (x_str == "center") {
            pos.x = 0.5f;
            pos.x_is_percent = true;
        } else if (x_str == "right") {
            pos.x = 1.0f;
            pos.x_is_percent = true;
        } else if (x_str.find('%') != std::string::npos) {
            pos.x = std::stof(x_str) / 100.0f;
            pos.x_is_percent = true;
        } else {
            pos.x = parse_length(x_str, 0);
            pos.x_is_percent = false;
        }

        // Y defaults to center
        pos.y = 0.5f;
        pos.y_is_percent = true;
    } else if (parts.size() >= 2) {
        // Two values: X and Y
        std::string x_str = parts[0];
        std::string y_str = parts[1];

        // Parse X
        if (x_str == "left") {
            pos.x = 0.0f;
            pos.x_is_percent = true;
        } else if (x_str == "center") {
            pos.x = 0.5f;
            pos.x_is_percent = true;
        } else if (x_str == "right") {
            pos.x = 1.0f;
            pos.x_is_percent = true;
        } else if (x_str.find('%') != std::string::npos) {
            pos.x = std::stof(x_str) / 100.0f;
            pos.x_is_percent = true;
        } else {
            pos.x = parse_length(x_str, 0);
            pos.x_is_percent = false;
        }

        // Parse Y
        if (y_str == "top") {
            pos.y = 0.0f;
            pos.y_is_percent = true;
        } else if (y_str == "center") {
            pos.y = 0.5f;
            pos.y_is_percent = true;
        } else if (y_str == "bottom") {
            pos.y = 1.0f;
            pos.y_is_percent = true;
        } else if (y_str.find('%') != std::string::npos) {
            pos.y = std::stof(y_str) / 100.0f;
            pos.y_is_percent = true;
        } else {
            pos.y = parse_length(y_str, 0);
            pos.y_is_percent = false;
        }
    }

    return pos;
}

/**
 * @brief Parse background-repeat property
 *
 * Parses CSS background-repeat values
 * @param repeat_css CSS value (e.g., "repeat", "no-repeat", "repeat-x")
 * @return BackgroundRepeat enum value
 */
BackgroundRepeat parse_background_repeat(const std::string& repeat_css) {
    std::string trimmed = trim(repeat_css);

    if (trimmed == "no-repeat") {
        return BG_NO_REPEAT;
    } else if (trimmed == "repeat-x") {
        return BG_REPEAT_X;
    } else if (trimmed == "repeat-y") {
        return BG_REPEAT_Y;
    } else {
        return BG_REPEAT;  // Default
    }
}

} // namespace nvgcss_utils
