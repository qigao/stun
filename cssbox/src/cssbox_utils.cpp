/*
 * NanoVG CSS - Utility functions
 */

#include "cssbox_internal.h"
#include "cssbox_conversion.h"  // Unified parsing implementation
#include <algorithm>
#include <cctype>
#include <cstring>
#include <sstream>
#include <limits>  // Sprint 25: for std::numeric_limits

// Include re2c-generated lexer
extern "C" {
#include "css_calc_lexer_gen.c"
}

namespace cssbox_utils {

// ============================================================================
// Sprint 24: CSS calc() Function - Expression Parser (re2c-based)
// ============================================================================

// Recursive descent parser using re2c-generated lexer
class CalcParser {
public:
    CalcParser(const char* expr, float context_value, float font_size)
        : context_value_(context_value), font_size_(font_size) {
        CSSCalcLexer_init(&lexer_, expr);
        advance();  // Prime the first token
    }

    float parse() {
        return parse_expression();
    }

private:
    // expression := term (('+' | '-') term)*
    float parse_expression() {
        float result = parse_term();

        while (current_.type == CALC_PLUS || current_.type == CALC_MINUS) {
            CSSCalcTokenType op = current_.type;
            advance();
            float right = parse_term();

            if (op == CALC_PLUS) {
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

        while (current_.type == CALC_MULTIPLY || current_.type == CALC_DIVIDE) {
            CSSCalcTokenType op = current_.type;
            advance();
            float right = parse_factor();

            if (op == CALC_MULTIPLY) {
                result *= right;
            } else {
                if (right != 0.0f) {
                    result /= right;
                } else {
                    result = 0.0f;
                }
            }
        }

        return result;
    }

    // factor := number | '(' expression ')'
    float parse_factor() {
        // Handle unary minus
        bool negate = false;
        if (current_.type == CALC_MINUS) {
            negate = true;
            advance();
        }

        float result = 0.0f;

        if (current_.type == CALC_LPAREN) {
            advance();  // consume '('
            result = parse_expression();
            if (current_.type == CALC_RPAREN) {
                advance();  // consume ')'
            }
        }
        else if (CSSCalcToken_is_number(current_.type)) {
            result = CSSCalcToken_to_pixels(&current_, context_value_, font_size_,
                                            context_value_, context_value_);
            advance();
        }

        return negate ? -result : result;
    }

    void advance() {
        current_ = CSSCalcLexer_next_token(&lexer_);
    }

    CSSCalcLexer lexer_;
    CSSCalcToken current_;
    float context_value_;
    float font_size_;
};

// Parse and evaluate calc() expression using re2c lexer
float parse_calc_expression(const std::string& expr, float context_value, float font_size) {
    try {
        CalcParser parser(expr.c_str(), context_value, font_size);
        return parser.parse();
    } catch (const std::exception&) {
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

// Parse color - delegates to unified implementation in cssbox_conversion.h
NVGcolor parse_color(const std::string& color_str) {
    auto result = cssbox::convert::parse_color(color_str);
    if (result) {
        return *result;
    }
    // Default: white (for backward compatibility)
    return nvgRGB(255, 255, 255);
}

// Parse length - handles calc/min/max/clamp, delegates simple cases to conversion.h
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

    // Delegate simple cases to unified implementation
    auto length = cssbox::convert::parse_length(str);
    if (length) {
        // Resolve Length to pixels based on unit type
        switch (length->unit) {
            case cssbox::LengthUnit::PX:
                return length->value;
            case cssbox::LengthUnit::PERCENT:
                return (length->value / 100.0f) * context_value;
            case cssbox::LengthUnit::EM:
                return length->value * 16.0f;  // Assume 16px base font
            case cssbox::LengthUnit::REM:
                return length->value * 16.0f;  // Assume 16px root font
            case cssbox::LengthUnit::VW:
            case cssbox::LengthUnit::VH:
                return length->value * (context_value / 100.0f);
            case cssbox::LengthUnit::AUTO:
            default:
                return 0.0f;
        }
    }

    // Fallback: try parsing as plain number (pixels)
    try {
        return std::stof(str);
    } catch (const std::exception&) {
        return 0.0f;
    }
}

// ============================================================================
// Cubic Bezier Implementation
// ============================================================================

/**
 * Evaluate a cubic bezier curve at parameter t.
 * Control points: P0=(0,0), P1=(x1,y1), P2=(x2,y2), P3=(1,1)
 *
 * The bezier curve maps input time t to output progress y.
 * We need to find the y value for a given x (time).
 */

// Helper: Evaluate bezier polynomial
static float bezier_sample(float t, float a, float b, float c) {
    return ((a * t + b) * t + c) * t;
}

// Helper: Derivative of bezier polynomial
static float bezier_slope(float t, float a, float b, float c) {
    return (3.0f * a * t + 2.0f * b) * t + c;
}

/**
 * Evaluate cubic-bezier easing function.
 *
 * @param t Input time [0, 1]
 * @param x1, y1 First control point
 * @param x2, y2 Second control point
 * @return Output progress [0, 1]
 */
float cubic_bezier(float t, float x1, float y1, float x2, float y2) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    // Coefficients for x(t) = ax*t^3 + bx*t^2 + cx*t
    // Derived from Bernstein form: (1-t)^3*0 + 3*(1-t)^2*t*x1 + 3*(1-t)*t^2*x2 + t^3*1
    float cx = 3.0f * x1;
    float bx = 3.0f * (x2 - x1) - cx;
    float ax = 1.0f - cx - bx;

    // Coefficients for y(t)
    float cy = 3.0f * y1;
    float by = 3.0f * (y2 - y1) - cy;
    float ay = 1.0f - cy - by;

    // Newton-Raphson iteration to solve for parameter given x
    // We need to find bezier_t such that x(bezier_t) = t
    float bezier_t = t;  // Initial guess

    // Newton iterations (usually converges in 4-8 iterations)
    for (int i = 0; i < 8; ++i) {
        float x = bezier_sample(bezier_t, ax, bx, cx) - t;
        if (std::abs(x) < 1e-6f) break;

        float dx = bezier_slope(bezier_t, ax, bx, cx);
        if (std::abs(dx) < 1e-6f) break;

        bezier_t -= x / dx;
    }

    // Clamp to valid range
    if (bezier_t < 0.0f) bezier_t = 0.0f;
    if (bezier_t > 1.0f) bezier_t = 1.0f;

    // Return y(bezier_t)
    return bezier_sample(bezier_t, ay, by, cy);
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
            return cubic_bezier(t, 0.25f, 0.1f, 0.25f, 1.0f);

        case EasingFunction::EASE_IN:
            // cubic-bezier(0.42, 0, 1.0, 1.0)
            return cubic_bezier(t, 0.42f, 0.0f, 1.0f, 1.0f);

        case EasingFunction::EASE_OUT:
            // cubic-bezier(0, 0, 0.58, 1.0)
            return cubic_bezier(t, 0.0f, 0.0f, 0.58f, 1.0f);

        case EasingFunction::EASE_IN_OUT:
            // cubic-bezier(0.42, 0, 0.58, 1.0)
            return cubic_bezier(t, 0.42f, 0.0f, 0.58f, 1.0f);

        case EasingFunction::CUBIC_BEZIER:
            // For custom cubic-bezier, use apply_easing_with_bezier() instead
            // Fall back to ease curve
            return cubic_bezier(t, 0.25f, 0.1f, 0.25f, 1.0f);

        default:
            return t;
    }
}

/**
 * Apply custom cubic-bezier easing with explicit control points.
 */
float apply_easing_with_bezier(float t, float x1, float y1, float x2, float y2) {
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return cubic_bezier(t, x1, y1, x2, y2);
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
// Helper: Parse transform function (e.g., "scale(1.2)" -> {"scale", "1.2"})
static std::pair<std::string, std::string> parse_transform_function(const std::string& transform) {
    size_t open_paren = transform.find('(');
    size_t close_paren = transform.find(')');

    if (open_paren == std::string::npos || close_paren == std::string::npos) {
        return {"", ""};
    }

    std::string func_name = transform.substr(0, open_paren);
    std::string args = transform.substr(open_paren + 1, close_paren - open_paren - 1);

    return {trim(func_name), trim(args)};
}

// Helper: Interpolate transform functions
static std::string interpolate_transform(const std::string& start_value,
                                         const std::string& end_value,
                                         float t) {
    try {
        // Parse start transform
        auto [start_func, start_args] = parse_transform_function(start_value);
        auto [end_func, end_args] = parse_transform_function(end_value);

        // Validate parsing
        if (start_func.empty() || end_func.empty() || start_args.empty() || end_args.empty()) {
            return (t >= 0.5f) ? end_value : start_value;
        }

        // Functions must match
        if (start_func != end_func) {
            return (t >= 0.5f) ? end_value : start_value;
        }

        // Interpolate based on function type
        if (start_func == "scale") {
            // scale(X) or scale(X, Y)
            if (start_args.empty() || end_args.empty()) {
                return (t >= 0.5f) ? end_value : start_value;
            }
            float start_num = std::stof(start_args);
            float end_num = std::stof(end_args);
            float result = interpolate_float(start_num, end_num, t);

            char buf[64];
            snprintf(buf, sizeof(buf), "scale(%.3f)", result);
            return buf;
        }
        else if (start_func == "rotate") {
            // rotate(Xdeg)
            std::string start_clean = start_args;
            std::string end_clean = end_args;

            // Remove 'deg' suffix
            if (ends_with(start_clean, "deg")) {
                start_clean = start_clean.substr(0, start_clean.length() - 3);
            }
            if (ends_with(end_clean, "deg")) {
                end_clean = end_clean.substr(0, end_clean.length() - 3);
            }

            // Validate after trimming
            start_clean = trim(start_clean);
            end_clean = trim(end_clean);
            if (start_clean.empty() || end_clean.empty()) {
                return (t >= 0.5f) ? end_value : start_value;
            }

            float start_angle = std::stof(start_clean);
            float end_angle = std::stof(end_clean);
            float result = interpolate_float(start_angle, end_angle, t);

            char buf[64];
            snprintf(buf, sizeof(buf), "rotate(%.2fdeg)", result);
            return buf;
        }
        else if (start_func == "translate") {
        // translate(Xpx, Ypx) - parse two values
        // Simple implementation: assume format "Xpx, Ypx" or "Xpx"
        auto split_args = [](const std::string& args) -> std::pair<float, float> {
            size_t comma = args.find(',');
            if (comma != std::string::npos) {
                std::string x_str = args.substr(0, comma);
                std::string y_str = args.substr(comma + 1);

                // Remove 'px' suffix
                if (ends_with(x_str, "px")) x_str = x_str.substr(0, x_str.length() - 2);
                if (ends_with(y_str, "px")) y_str = y_str.substr(0, y_str.length() - 2);

                x_str = trim(x_str);
                y_str = trim(y_str);
                if (x_str.empty() || y_str.empty()) {
                    return {0.0f, 0.0f};
                }
                return {std::stof(x_str), std::stof(y_str)};
            } else {
                // Single value
                std::string x_str = args;
                if (ends_with(x_str, "px")) x_str = x_str.substr(0, x_str.length() - 2);
                x_str = trim(x_str);
                if (x_str.empty()) {
                    return {0.0f, 0.0f};
                }
                return {std::stof(x_str), 0.0f};
            }
        };

        auto [start_x, start_y] = split_args(start_args);
        auto [end_x, end_y] = split_args(end_args);

        float result_x = interpolate_float(start_x, end_x, t);
        float result_y = interpolate_float(start_y, end_y, t);

        char buf[64];
        snprintf(buf, sizeof(buf), "translate(%.2fpx, %.2fpx)", result_x, result_y);
        return buf;
        }

        // Unknown transform function - snap at t=0.5
        return (t >= 0.5f) ? end_value : start_value;
    } catch (const std::exception& e) {
        // Failed to parse or interpolate transform - snap at t=0.5
        return (t >= 0.5f) ? end_value : start_value;
    }
}

// Forward declaration for box-shadow interpolation
BoxShadow parse_single_box_shadow(const std::string& shadow_str);

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

        // Check if both values are transform functions
        if ((start_trimmed.find("scale(") != std::string::npos ||
             start_trimmed.find("rotate(") != std::string::npos ||
             start_trimmed.find("translate(") != std::string::npos) &&
            (end_trimmed.find("scale(") != std::string::npos ||
             end_trimmed.find("rotate(") != std::string::npos ||
             end_trimmed.find("translate(") != std::string::npos)) {
            // Transform interpolation
            return interpolate_transform(start_trimmed, end_trimmed, t);
        }

        // Check if both values look like box-shadows (contain px values or start with numbers)
        // Box shadow format: [inset?] <offset-x> <offset-y> [blur] [spread] [color]
        auto looks_like_shadow = [](const std::string& s) {
            if (s.empty() || s == "none") return true;  // "none" is valid
            // Must contain at least 2 numeric values (offset-x, offset-y)
            int num_count = 0;
            size_t i = 0;
            while (i < s.length()) {
                // Skip whitespace
                while (i < s.length() && std::isspace(s[i])) i++;
                if (i >= s.length()) break;
                // Check if this token starts with a digit or minus sign
                if (std::isdigit(s[i]) || (s[i] == '-' && i + 1 < s.length() && std::isdigit(s[i + 1]))) {
                    num_count++;
                }
                // Skip to next whitespace or end
                while (i < s.length() && !std::isspace(s[i])) {
                    if (s[i] == '(') {
                        int depth = 1;
                        i++;
                        while (i < s.length() && depth > 0) {
                            if (s[i] == '(') depth++;
                            else if (s[i] == ')') depth--;
                            i++;
                        }
                    } else {
                        i++;
                    }
                }
            }
            return num_count >= 2;
        };

        if (looks_like_shadow(start_trimmed) && looks_like_shadow(end_trimmed)) {
            // Handle "none" cases
            if (start_trimmed == "none" && end_trimmed == "none") {
                return "none";
            }
            // If transitioning from/to "none", snap at t=0.5
            if (start_trimmed == "none" || end_trimmed == "none") {
                return (t >= 0.5f) ? end_trimmed : start_trimmed;
            }

            // Parse both shadows (only handle single shadows for now)
            BoxShadow start_shadow = parse_single_box_shadow(start_trimmed);
            BoxShadow end_shadow = parse_single_box_shadow(end_trimmed);

            // Interpolate each component
            float offset_x = interpolate_float(start_shadow.offset_x, end_shadow.offset_x, t);
            float offset_y = interpolate_float(start_shadow.offset_y, end_shadow.offset_y, t);
            float blur = interpolate_float(start_shadow.blur_radius, end_shadow.blur_radius, t);
            float spread = interpolate_float(start_shadow.spread_radius, end_shadow.spread_radius, t);
            NVGcolor color = interpolate_color(start_shadow.color, end_shadow.color, t);

            // Build result string
            char buf[128];
            snprintf(buf, sizeof(buf), "%.1fpx %.1fpx %.1fpx %.1fpx rgba(%d,%d,%d,%.2f)",
                    offset_x, offset_y, blur, spread,
                    (int)(color.r * 255), (int)(color.g * 255), (int)(color.b * 255), color.a);
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
                     EasingFunction& out_easing,
                     float& out_bezier_x1,
                     float& out_bezier_y1,
                     float& out_bezier_x2,
                     float& out_bezier_y2) {
    std::string css = trim(transition_css);
    if (css.empty() || css == "none") {
        return false;
    }

    // Initialize bezier defaults
    out_bezier_x1 = 0.0f;
    out_bezier_y1 = 0.0f;
    out_bezier_x2 = 1.0f;
    out_bezier_y2 = 1.0f;

    // Parse: "property duration timing-function [delay]"
    // Example: "all 0.3s ease-in-out"
    // Example: "opacity 0.5s cubic-bezier(0.4, 0, 0.2, 1)"

    // Handle cubic-bezier specially since it contains commas and parentheses
    std::string easing_str;
    size_t cubic_start = css.find("cubic-bezier(");
    if (cubic_start != std::string::npos) {
        size_t cubic_end = css.find(')', cubic_start);
        if (cubic_end != std::string::npos) {
            easing_str = css.substr(cubic_start, cubic_end - cubic_start + 1);
            // Remove cubic-bezier from css for further parsing
            css = css.substr(0, cubic_start) + css.substr(cubic_end + 1);
        }
    }

    auto parts = split(css, ' ');
    if (parts.empty()) return false;

    // First part: property
    out_property = parts[0];

    // Second part: duration
    if (parts.size() > 1) {
        std::string duration_str = parts[1];
        try {
            if (ends_with(duration_str, "ms")) {
                if (duration_str.length() > 2) {
                    out_duration = std::stof(duration_str.substr(0, duration_str.length() - 2)) / 1000.0f;
                } else {
                    out_duration = 0.0f;
                }
            } else if (ends_with(duration_str, "s")) {
                if (duration_str.length() > 1) {
                    out_duration = std::stof(duration_str.substr(0, duration_str.length() - 1));
                } else {
                    out_duration = 0.0f;
                }
            } else {
                out_duration = std::stof(duration_str);  // Assume seconds
            }
        } catch (const std::exception&) {
            out_duration = 0.0f;
        }
    } else {
        out_duration = 0.0f;
    }

    // Parse easing function
    if (!easing_str.empty()) {
        // Parse cubic-bezier(x1, y1, x2, y2)
        size_t paren_start = easing_str.find('(');
        size_t paren_end = easing_str.find(')');
        if (paren_start != std::string::npos && paren_end != std::string::npos) {
            std::string args = easing_str.substr(paren_start + 1, paren_end - paren_start - 1);
            auto values = split(args, ',');
            if (values.size() >= 4) {
                try {
                    out_bezier_x1 = std::stof(trim(values[0]));
                    out_bezier_y1 = std::stof(trim(values[1]));
                    out_bezier_x2 = std::stof(trim(values[2]));
                    out_bezier_y2 = std::stof(trim(values[3]));
                    out_easing = EasingFunction::CUBIC_BEZIER;
                } catch (const std::exception&) {
                    out_easing = EasingFunction::EASE;
                }
            } else {
                out_easing = EasingFunction::EASE;
            }
        } else {
            out_easing = EasingFunction::EASE;
        }
    } else if (parts.size() > 2) {
        // Check for named easing function
        std::string easing_name = parts[2];
        if (easing_name == "linear") {
            out_easing = EasingFunction::LINEAR;
        } else if (easing_name == "ease") {
            out_easing = EasingFunction::EASE;
        } else if (easing_name == "ease-in") {
            out_easing = EasingFunction::EASE_IN;
        } else if (easing_name == "ease-out") {
            out_easing = EasingFunction::EASE_OUT;
        } else if (easing_name == "ease-in-out") {
            out_easing = EasingFunction::EASE_IN_OUT;
        } else {
            out_easing = EasingFunction::EASE;  // Default
        }
    } else {
        out_easing = EasingFunction::EASE;  // Default
    }

    return true;
}

// Overload for backward compatibility (without bezier output params)
bool parse_transition(const std::string& transition_css,
                     std::string& out_property,
                     float& out_duration,
                     EasingFunction& out_easing) {
    float bx1, by1, bx2, by2;
    return parse_transition(transition_css, out_property, out_duration, out_easing,
                           bx1, by1, bx2, by2);
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

} // namespace cssbox_utils
