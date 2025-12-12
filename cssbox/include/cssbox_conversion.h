/*
 * NanoVG CSS - String to Type Conversion
 *
 * Converts CSS string values to typed properties.
 * Called ONCE during CSS parsing, NOT every frame.
 *
 * Philosophy:
 * - Parse at CSS load time, not render time
 * - Fail fast with clear error messages
 * - No silent fallbacks (if CSS is wrong, tell the user)
 */

#ifndef cssbox_CONVERSION_H
#define cssbox_CONVERSION_H

// Windows min/max macro conflict fix
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <nanovg.h>
#include "cssbox_types.h"
#include "css_keyword_lexer.h"
#include <string>
#include <string_view>
#include <optional>
#include <cctype>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <vector>

// Windows macro cleanup (wingdi.h defines RELATIVE/ABSOLUTE/ALTERNATE)
#ifdef RELATIVE
#undef RELATIVE
#endif
#ifdef ABSOLUTE
#undef ABSOLUTE
#endif
#ifdef ALTERNATE
#undef ALTERNATE
#endif

namespace cssbox {
namespace convert {

// ============================================================================
// Helper: String Parsing Utilities
// ============================================================================

inline std::string_view trim(std::string_view str) {
    size_t start = 0;
    while (start < str.size() && std::isspace(str[start])) ++start;

    size_t end = str.size();
    while (end > start && std::isspace(str[end - 1])) --end;

    return str.substr(start, end - start);
}

inline bool ends_with(std::string_view str, std::string_view suffix) {
    return str.size() >= suffix.size() &&
           str.substr(str.size() - suffix.size()) == suffix;
}

inline bool starts_with(std::string_view str, std::string_view prefix) {
    return str.size() >= prefix.size() &&
           str.substr(0, prefix.size()) == prefix;
}

// Helper: Convert to lowercase for case-insensitive comparison
inline std::string to_lowercase(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// ============================================================================
// Length Conversion
// ============================================================================

// Forward declaration for calc evaluation
struct CalcValue {
    float px_value = 0.0f;      // Absolute pixel component
    float percent_value = 0.0f;  // Percentage component
    bool has_percent = false;

    bool is_pure_px() const { return !has_percent; }
};

/**
 * @brief Evaluate a simple calc() expression
 *
 * Supports: calc(100% - 20px), calc(50px + 50px), calc(100% / 2)
 * Returns separated px and percent components for later resolution.
 */
inline std::optional<CalcValue> evaluate_calc_expr(std::string_view expr) {
    // Remove "calc(" prefix and ")" suffix
    if (!starts_with(expr, "calc(") || expr.back() != ')') {
        return std::nullopt;
    }
    expr = expr.substr(5, expr.size() - 6);  // Remove "calc(" and ")"
    expr = trim(expr);

    CalcValue result;

    // Simple tokenizer for calc expressions
    // Supports: value, value + value, value - value, value * number, value / number
    float current_value = 0.0f;
    bool current_is_percent = false;
    char pending_op = '+';  // Start with implicit +

    size_t i = 0;
    while (i < expr.size()) {
        // Skip whitespace
        while (i < expr.size() && std::isspace(expr[i])) i++;
        if (i >= expr.size()) break;

        char c = expr[i];

        // Check for operator
        if (c == '+' || c == '-' || c == '*' || c == '/') {
            // Apply pending operation
            if (pending_op == '+') {
                if (current_is_percent) {
                    result.percent_value += current_value;
                    result.has_percent = true;
                } else {
                    result.px_value += current_value;
                }
            } else if (pending_op == '-') {
                if (current_is_percent) {
                    result.percent_value -= current_value;
                    result.has_percent = true;
                } else {
                    result.px_value -= current_value;
                }
            }
            // * and / are handled inline with next value

            pending_op = c;
            current_value = 0.0f;
            current_is_percent = false;
            i++;
            continue;
        }

        // Parse number with unit
        size_t num_start = i;
        bool negative = false;
        if (c == '-') {
            negative = true;
            i++;
        }

        while (i < expr.size() && (std::isdigit(expr[i]) || expr[i] == '.')) i++;

        std::string num_str(expr.substr(num_start, i - num_start));
        float value = std::strtof(num_str.c_str(), nullptr);
        if (negative) value = -value;

        // Parse unit
        size_t unit_start = i;
        while (i < expr.size() && (std::isalpha(expr[i]) || expr[i] == '%')) i++;
        std::string_view unit = expr.substr(unit_start, i - unit_start);

        bool is_percent = (unit == "%");

        // Convert to px if not percent (simplified - assumes px for now)
        // Full implementation would handle em, rem, vw, vh
        if (unit == "em") {
            value *= 16.0f;  // Approximate, will be resolved properly later
        } else if (unit == "rem") {
            value *= 16.0f;
        }

        // Handle * and / operations
        if (pending_op == '*') {
            // Multiply current accumulated values
            result.px_value *= value;
            result.percent_value *= value;
            pending_op = '+';  // Reset for next iteration
            current_value = 0;
            continue;
        } else if (pending_op == '/') {
            if (value != 0) {
                result.px_value /= value;
                result.percent_value /= value;
            }
            pending_op = '+';
            current_value = 0;
            continue;
        }

        current_value = value;
        current_is_percent = is_percent;
    }

    // Apply final pending operation
    if (pending_op == '+') {
        if (current_is_percent) {
            result.percent_value += current_value;
            result.has_percent = true;
        } else {
            result.px_value += current_value;
        }
    } else if (pending_op == '-') {
        if (current_is_percent) {
            result.percent_value -= current_value;
            result.has_percent = true;
        } else {
            result.px_value -= current_value;
        }
    }

    return result;
}

inline std::optional<Length> parse_length(std::string_view str) {
    str = trim(str);
    if (str.empty()) return std::nullopt;

    // Special keyword
    if (str == "auto") return Length::auto_();

    // Handle calc() expressions
    if (starts_with(str, "calc(")) {
        auto calc_result = evaluate_calc_expr(str);
        if (calc_result) {
            if (calc_result->is_pure_px()) {
                return Length::px(calc_result->px_value);
            } else if (calc_result->px_value == 0.0f) {
                return Length::percent(calc_result->percent_value);
            } else {
                return Length::calc(calc_result->px_value, calc_result->percent_value);
            }
        }
        return std::nullopt;
    }

    std::string str_copy(str);
    char* end_ptr;
    float value = std::strtof(str_copy.c_str(), &end_ptr);

    // Use re2c lexer for unit matching
    size_t unit_offset = end_ptr - str_copy.c_str();
    std::string_view unit = trim(str.substr(unit_offset));

    // Empty unit = unitless number (treat as px for SVG compatibility)
    if (unit.empty()) {
        return Length::px(value);
    }

    switch (fast::parse_length_unit(unit)) {
        case CSS_UNIT_NONE:
        case CSS_UNIT_PX:      return Length::px(value);
        case CSS_UNIT_PERCENT: return Length::percent(value);
        case CSS_UNIT_EM:      return Length::em(value);
        case CSS_UNIT_REM:     return Length::rem(value);
        case CSS_UNIT_VW:      return Length::vw(value);
        case CSS_UNIT_VH:      return Length::vh(value);
        default:               return std::nullopt;
    }
}

// ============================================================================
// Color Conversion
// ============================================================================

inline uint8_t parse_hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

inline std::optional<Color> parse_color(std::string_view str) {
    str = trim(str);
    if (str.empty()) return std::nullopt;

    // Hex colors: #rgb, #rrggbb, #rrggbbaa
    if (str[0] == '#') {
        str = str.substr(1);

        if (str.size() == 3) {
            // #rgb -> #rrggbb
            uint8_t r = parse_hex_digit(str[0]) * 17;
            uint8_t g = parse_hex_digit(str[1]) * 17;
            uint8_t b = parse_hex_digit(str[2]) * 17;
            return nvgRGBA(r, g, b, 255);
        }
        else if (str.size() == 6) {
            // #rrggbb
            uint8_t r = parse_hex_digit(str[0]) * 16 + parse_hex_digit(str[1]);
            uint8_t g = parse_hex_digit(str[2]) * 16 + parse_hex_digit(str[3]);
            uint8_t b = parse_hex_digit(str[4]) * 16 + parse_hex_digit(str[5]);
            return nvgRGBA(r, g, b, 255);
        }
        else if (str.size() == 8) {
            // #rrggbbaa
            uint8_t r = parse_hex_digit(str[0]) * 16 + parse_hex_digit(str[1]);
            uint8_t g = parse_hex_digit(str[2]) * 16 + parse_hex_digit(str[3]);
            uint8_t b = parse_hex_digit(str[4]) * 16 + parse_hex_digit(str[5]);
            uint8_t a = parse_hex_digit(str[6]) * 16 + parse_hex_digit(str[7]);
            return nvgRGBA(r, g, b, a);
        }
    }

    // rgb() / rgba()
    if (starts_with(str, "rgb(") || starts_with(str, "rgba(")) {
        size_t start = str.find('(') + 1;
        size_t end = str.find(')');
        if (end == std::string_view::npos) return std::nullopt;

        std::string_view values = str.substr(start, end - start);

        // Parse comma-separated values
        int r = 0, g = 0, b = 0, a = 255;
        int count = 0;
        size_t pos = 0;

        while (pos < values.size() && count < 4) {
            size_t comma = values.find(',', pos);
            std::string_view val = trim(values.substr(pos, comma - pos));

            float num = std::strtof(val.data(), nullptr);
            if (count < 3) {
                // RGB: 0-255
                int channel = static_cast<int>(num);
                if (channel < 0) channel = 0;
                if (channel > 255) channel = 255;
                if (count == 0) r = channel;
                else if (count == 1) g = channel;
                else if (count == 2) b = channel;
            } else {
                // Alpha: 0.0-1.0
                a = static_cast<int>(num * 255.0f);
            }

            ++count;
            if (comma == std::string_view::npos) break;
            pos = comma + 1;
        }

        return nvgRGBA(r, g, b, a);
    }

    // Named colors via fast lexer
    auto named = fast::parse_named_color(str);
    if (named.valid) {
        return nvgRGBA(named.r, named.g, named.b, named.a);
    }

    return std::nullopt;
}

// ============================================================================
// Enum Conversions
// ============================================================================

inline Display parse_display(std::string_view str) {
    switch (fast::parse_display(str)) {
        case CSS_DISPLAY_BLOCK: return Display::BLOCK;
        case CSS_DISPLAY_FLEX:  return Display::FLEX;
        case CSS_DISPLAY_GRID:  return Display::GRID;
        case CSS_DISPLAY_NONE:  return Display::NONE;
        default: return Display::BLOCK;
    }
}

inline Position parse_position(std::string_view str) {
    switch (fast::parse_position(str)) {
        case CSS_POSITION_STATIC:   return Position::STATIC;
        case CSS_POSITION_RELATIVE: return Position::RELATIVE;
        case CSS_POSITION_ABSOLUTE: return Position::ABSOLUTE;
        case CSS_POSITION_FIXED:    return Position::FIXED;
        default: return Position::STATIC;
    }
}

inline BoxSizing parse_box_sizing(std::string_view str) {
    switch (fast::parse_box_sizing(str)) {
        case CSS_BOX_SIZING_BORDER_BOX:  return BoxSizing::BORDER_BOX;
        case CSS_BOX_SIZING_CONTENT_BOX: return BoxSizing::CONTENT_BOX;
        default: return BoxSizing::CONTENT_BOX;
    }
}

inline Overflow parse_overflow(std::string_view str) {
    switch (fast::parse_overflow(str)) {
        case CSS_OVERFLOW_VISIBLE: return Overflow::VISIBLE;
        case CSS_OVERFLOW_HIDDEN:  return Overflow::HIDDEN;
        case CSS_OVERFLOW_SCROLL:  return Overflow::SCROLL;
        case CSS_OVERFLOW_AUTO:    return Overflow::AUTO;
        default: return Overflow::VISIBLE;
    }
}

inline FlexDirection parse_flex_direction(std::string_view str) {
    switch (fast::parse_flex_direction(str)) {
        case CSS_FLEX_DIR_ROW:            return FlexDirection::ROW;
        case CSS_FLEX_DIR_ROW_REVERSE:    return FlexDirection::ROW_REVERSE;
        case CSS_FLEX_DIR_COLUMN:         return FlexDirection::COLUMN;
        case CSS_FLEX_DIR_COLUMN_REVERSE: return FlexDirection::COLUMN_REVERSE;
        default: return FlexDirection::ROW;
    }
}

inline FlexWrap parse_flex_wrap(std::string_view str) {
    switch (fast::parse_flex_wrap(str)) {
        case CSS_FLEX_WRAP_NOWRAP:       return FlexWrap::NOWRAP;
        case CSS_FLEX_WRAP_WRAP:         return FlexWrap::WRAP;
        case CSS_FLEX_WRAP_WRAP_REVERSE: return FlexWrap::WRAP_REVERSE;
        default: return FlexWrap::NOWRAP;
    }
}

inline JustifyContent parse_justify_content(std::string_view str) {
    switch (fast::parse_justify_content(str)) {
        case CSS_JUSTIFY_FLEX_START:    return JustifyContent::FLEX_START;
        case CSS_JUSTIFY_FLEX_END:      return JustifyContent::FLEX_END;
        case CSS_JUSTIFY_CENTER:        return JustifyContent::CENTER;
        case CSS_JUSTIFY_SPACE_BETWEEN: return JustifyContent::SPACE_BETWEEN;
        case CSS_JUSTIFY_SPACE_AROUND:  return JustifyContent::SPACE_AROUND;
        case CSS_JUSTIFY_SPACE_EVENLY:  return JustifyContent::SPACE_EVENLY;
        default: return JustifyContent::FLEX_START;
    }
}

inline AlignItems parse_align_items(std::string_view str) {
    switch (fast::parse_align_items(str)) {
        case CSS_ALIGN_ITEMS_STRETCH:    return AlignItems::STRETCH;
        case CSS_ALIGN_ITEMS_FLEX_START: return AlignItems::FLEX_START;
        case CSS_ALIGN_ITEMS_FLEX_END:   return AlignItems::FLEX_END;
        case CSS_ALIGN_ITEMS_CENTER:     return AlignItems::CENTER;
        case CSS_ALIGN_ITEMS_BASELINE:   return AlignItems::BASELINE;
        default: return AlignItems::STRETCH;
    }
}

inline AlignContent parse_align_content(std::string_view str) {
    switch (fast::parse_align_content(str)) {
        case CSS_ALIGN_CONTENT_STRETCH:       return AlignContent::STRETCH;
        case CSS_ALIGN_CONTENT_FLEX_START:    return AlignContent::FLEX_START;
        case CSS_ALIGN_CONTENT_FLEX_END:      return AlignContent::FLEX_END;
        case CSS_ALIGN_CONTENT_CENTER:        return AlignContent::CENTER;
        case CSS_ALIGN_CONTENT_SPACE_BETWEEN: return AlignContent::SPACE_BETWEEN;
        case CSS_ALIGN_CONTENT_SPACE_AROUND:  return AlignContent::SPACE_AROUND;
        default: return AlignContent::STRETCH;
    }
}

// Grid/Flex item self-alignment
inline AlignSelf parse_align_self(std::string_view str) {
    switch (fast::parse_self_align(str)) {
        case CSS_SELF_ALIGN_AUTO:     return AlignSelf::AUTO;
        case CSS_SELF_ALIGN_START:    return AlignSelf::START;
        case CSS_SELF_ALIGN_END:      return AlignSelf::END;
        case CSS_SELF_ALIGN_CENTER:   return AlignSelf::CENTER;
        case CSS_SELF_ALIGN_BASELINE: return AlignSelf::BASELINE;
        case CSS_SELF_ALIGN_STRETCH:  return AlignSelf::STRETCH;
        default: return AlignSelf::AUTO;
    }
}

inline JustifySelf parse_justify_self(std::string_view str) {
    switch (fast::parse_self_align(str)) {
        case CSS_SELF_ALIGN_AUTO:    return JustifySelf::AUTO;
        case CSS_SELF_ALIGN_START:   return JustifySelf::START;
        case CSS_SELF_ALIGN_END:     return JustifySelf::END;
        case CSS_SELF_ALIGN_CENTER:  return JustifySelf::CENTER;
        case CSS_SELF_ALIGN_STRETCH: return JustifySelf::STRETCH;
        default: return JustifySelf::AUTO;
    }
}

inline JustifyItems parse_justify_items(std::string_view str) {
    switch (fast::parse_self_align(str)) {
        case CSS_SELF_ALIGN_START:   return JustifyItems::START;
        case CSS_SELF_ALIGN_END:     return JustifyItems::END;
        case CSS_SELF_ALIGN_CENTER:  return JustifyItems::CENTER;
        case CSS_SELF_ALIGN_STRETCH: return JustifyItems::STRETCH;
        default: return JustifyItems::STRETCH;
    }
}

inline BorderStyle parse_border_style(std::string_view str) {
    switch (fast::parse_border_style(str)) {
        case CSS_BORDER_STYLE_NONE:   return BorderStyle::NONE;
        case CSS_BORDER_STYLE_SOLID:  return BorderStyle::SOLID;
        case CSS_BORDER_STYLE_DASHED: return BorderStyle::DASHED;
        case CSS_BORDER_STYLE_DOTTED: return BorderStyle::DOTTED;
        case CSS_BORDER_STYLE_DOUBLE: return BorderStyle::DOUBLE;
        case CSS_BORDER_STYLE_HIDDEN: return BorderStyle::HIDDEN;
        default: return BorderStyle::NONE;
    }
}

inline TextAlign parse_text_align(std::string_view str) {
    switch (fast::parse_text_align(str)) {
        case CSS_TEXT_ALIGN_LEFT:    return TextAlign::LEFT;
        case CSS_TEXT_ALIGN_CENTER:  return TextAlign::CENTER;
        case CSS_TEXT_ALIGN_RIGHT:   return TextAlign::RIGHT;
        case CSS_TEXT_ALIGN_JUSTIFY: return TextAlign::JUSTIFY;
        default: return TextAlign::LEFT;
    }
}

inline VerticalAlign parse_vertical_align(std::string_view str) {
    switch (fast::parse_vertical_align(str)) {
        case CSS_VERTICAL_ALIGN_TOP:      return VerticalAlign::TOP;
        case CSS_VERTICAL_ALIGN_MIDDLE:   return VerticalAlign::MIDDLE;
        case CSS_VERTICAL_ALIGN_BOTTOM:   return VerticalAlign::BOTTOM;
        case CSS_VERTICAL_ALIGN_BASELINE: return VerticalAlign::BASELINE;
        default: return VerticalAlign::TOP;
    }
}

inline FontWeight parse_font_weight(std::string_view str) {
    int result = fast::parse_font_weight(str);
    if (result >= 100 && result <= 900) {
        return static_cast<FontWeight>(result);
    }
    return FontWeight::NORMAL;
}

inline FontStyle parse_font_style(std::string_view str) {
    switch (fast::parse_font_style(str)) {
        case CSS_FONT_STYLE_NORMAL:  return FontStyle::NORMAL;
        case CSS_FONT_STYLE_ITALIC:  return FontStyle::ITALIC;
        case CSS_FONT_STYLE_OBLIQUE: return FontStyle::OBLIQUE;
        default: return FontStyle::NORMAL;
    }
}

inline TextDecoration parse_text_decoration(std::string_view str) {
    switch (fast::parse_text_decoration(str)) {
        case CSS_TEXT_DECORATION_NONE:         return TextDecoration::NONE;
        case CSS_TEXT_DECORATION_UNDERLINE:    return TextDecoration::UNDERLINE;
        case CSS_TEXT_DECORATION_OVERLINE:     return TextDecoration::OVERLINE;
        case CSS_TEXT_DECORATION_LINE_THROUGH: return TextDecoration::LINE_THROUGH;
        default: return TextDecoration::NONE;
    }
}

inline TextTransform parse_text_transform(std::string_view str) {
    switch (fast::parse_text_transform(str)) {
        case CSS_TEXT_TRANSFORM_NONE:       return TextTransform::NONE;
        case CSS_TEXT_TRANSFORM_UPPERCASE:  return TextTransform::UPPERCASE;
        case CSS_TEXT_TRANSFORM_LOWERCASE:  return TextTransform::LOWERCASE;
        case CSS_TEXT_TRANSFORM_CAPITALIZE: return TextTransform::CAPITALIZE;
        default: return TextTransform::NONE;
    }
}

inline SVGStroke::LineCap parse_stroke_linecap(std::string_view str) {
    switch (fast::parse_line_cap(str)) {
        case CSS_LINE_CAP_BUTT:   return SVGStroke::LineCap::BUTT;
        case CSS_LINE_CAP_ROUND:  return SVGStroke::LineCap::ROUND;
        case CSS_LINE_CAP_SQUARE: return SVGStroke::LineCap::SQUARE;
        default: return SVGStroke::LineCap::BUTT;
    }
}

inline SVGStroke::LineJoin parse_stroke_linejoin(std::string_view str) {
    switch (fast::parse_line_join(str)) {
        case CSS_LINE_JOIN_MITER: return SVGStroke::LineJoin::MITER;
        case CSS_LINE_JOIN_ROUND: return SVGStroke::LineJoin::ROUND;
        case CSS_LINE_JOIN_BEVEL: return SVGStroke::LineJoin::BEVEL;
        default: return SVGStroke::LineJoin::MITER;
    }
}

// ============================================================================
// Box Model Shorthand Parsing
// ============================================================================

/**
 * Parse CSS shorthand like "10px 20px 30px 40px" (top, right, bottom, left)
 * Supports 1-4 values:
 *   - 1 value: all sides
 *   - 2 values: [top/bottom, left/right]
 *   - 3 values: [top, left/right, bottom]
 *   - 4 values: [top, right, bottom, left]
 */
inline std::array<Length, 4> parse_box_sides(std::string_view str) {
    std::array<Length, 4> result{Length::px(0), Length::px(0), Length::px(0), Length::px(0)};

    std::vector<Length> values;
    size_t pos = 0;

    while (pos < str.size()) {
        // Skip whitespace
        while (pos < str.size() && std::isspace(str[pos])) ++pos;
        if (pos >= str.size()) break;

        // Find next whitespace
        size_t end = pos;
        while (end < str.size() && !std::isspace(str[end])) ++end;

        // Parse length
        auto len = parse_length(str.substr(pos, end - pos));
        if (len) values.push_back(*len);

        pos = end;
    }

    // Apply CSS shorthand rules
    if (values.size() == 1) {
        result[0] = result[1] = result[2] = result[3] = values[0];
    } else if (values.size() == 2) {
        result[0] = result[2] = values[0];  // top/bottom
        result[1] = result[3] = values[1];  // left/right
    } else if (values.size() == 3) {
        result[0] = values[0];  // top
        result[1] = result[3] = values[1];  // left/right
        result[2] = values[2];  // bottom
    } else if (values.size() >= 4) {
        result[0] = values[0];  // top
        result[1] = values[1];  // right
        result[2] = values[2];  // bottom
        result[3] = values[3];  // left
    }

    return result;
}

/**
 * Parse a grid size value (used inside minmax, etc.)
 * Supports: px, %, fr, auto, min-content, max-content
 */
inline GridSizeValue parse_grid_size_value(std::string_view str) {
    // Trim whitespace
    while (!str.empty() && std::isspace(str.front())) str.remove_prefix(1);
    while (!str.empty() && std::isspace(str.back())) str.remove_suffix(1);

    if (str.empty()) return GridSizeValue::auto_();

    if (str == "auto") return GridSizeValue::auto_();
    if (str == "min-content") return GridSizeValue::min_content();
    if (str == "max-content") return GridSizeValue::max_content();

    // Check for fr unit
    if (str.size() > 2 && str.substr(str.size() - 2) == "fr") {
        float value = 1.0f;
        try {
            std::string num_str(str.begin(), str.end() - 2);
            while (!num_str.empty() && std::isspace(num_str.back())) num_str.pop_back();
            if (!num_str.empty()) value = std::stof(num_str);
        } catch (...) {}
        return GridSizeValue::fr(value);
    }

    // Check for percentage
    if (!str.empty() && str.back() == '%') {
        try {
            std::string num_str(str.begin(), str.end() - 1);
            float value = std::stof(num_str);
            return GridSizeValue::percent(value);
        } catch (...) {}
    }

    // Try to parse as length (px, etc.)
    auto len = parse_length(str);
    if (len) {
        return GridSizeValue::px(len->value);
    }

    return GridSizeValue::auto_();
}

/**
 * Find matching closing parenthesis, handling nested parens
 */
inline size_t find_matching_paren(std::string_view str, size_t open_pos) {
    if (open_pos >= str.size() || str[open_pos] != '(') return std::string_view::npos;

    int depth = 1;
    for (size_t i = open_pos + 1; i < str.size(); i++) {
        if (str[i] == '(') depth++;
        else if (str[i] == ')') {
            depth--;
            if (depth == 0) return i;
        }
    }
    return std::string_view::npos;
}

/**
 * Parse a single GridTrack from string
 * Supports: px, fr, auto, min-content, max-content, minmax(), fit-content()
 */
inline std::optional<GridTrack> parse_single_track(std::string_view str) {
    // Trim whitespace
    while (!str.empty() && std::isspace(str.front())) str.remove_prefix(1);
    while (!str.empty() && std::isspace(str.back())) str.remove_suffix(1);

    if (str.empty()) return std::nullopt;

    // Check for minmax()
    if (str.size() >= 6 && str.substr(0, 6) == "minmax") {
        size_t open = str.find('(');
        size_t close = find_matching_paren(str, open);
        if (open != std::string_view::npos && close != std::string_view::npos) {
            std::string_view args = str.substr(open + 1, close - open - 1);
            size_t comma = args.find(',');
            if (comma != std::string_view::npos) {
                auto min_size = parse_grid_size_value(args.substr(0, comma));
                auto max_size = parse_grid_size_value(args.substr(comma + 1));
                return GridTrack::minmax(min_size, max_size);
            }
        }
        return std::nullopt;
    }

    // Check for fit-content()
    if (str.size() >= 11 && str.substr(0, 11) == "fit-content") {
        size_t open = str.find('(');
        size_t close = find_matching_paren(str, open);
        if (open != std::string_view::npos && close != std::string_view::npos) {
            std::string_view arg = str.substr(open + 1, close - open - 1);
            auto size = parse_grid_size_value(arg);
            if (size.type == GridSize::PX) {
                return GridTrack::fit_content(size.value);
            }
        }
        return std::nullopt;
    }

    // Simple values
    if (str == "auto") return GridTrack::auto_();
    if (str == "min-content") return GridTrack::min_content();
    if (str == "max-content") return GridTrack::max_content();

    // Check for fr unit
    if (str.size() > 2 && str.substr(str.size() - 2) == "fr") {
        float value = 1.0f;
        try {
            std::string num_str(str.begin(), str.end() - 2);
            while (!num_str.empty() && std::isspace(num_str.back())) num_str.pop_back();
            if (!num_str.empty()) value = std::stof(num_str);
        } catch (...) {}
        return GridTrack::fr(value);
    }

    // Try to parse as length (px, etc.)
    auto len = parse_length(str);
    if (len) {
        return GridTrack::px(len->value);
    }

    return std::nullopt;
}

/**
 * Parse a grid track list like "100px 200px 1fr" or "repeat(3, 1fr) 100px"
 * Examples: "100px 200px", "1fr 2fr", "auto 1fr", "minmax(100px, 1fr)"
 *           "repeat(3, 1fr)", "repeat(auto-fill, minmax(200px, 1fr))"
 *
 * Uses re2c-generated lexer for efficient tokenization.
 */

// Declaration for re2c-based parser (defined in css_grid_parser.cpp)
std::vector<GridTrack> parse_grid_track_list_re2c(const char* input);

inline std::vector<GridTrack> parse_grid_track_list(std::string_view str) {
    // Use re2c-based parser for better performance
    std::string input(str);
    return parse_grid_track_list_re2c(input.c_str());
}

// ============================================================================
// Transform Parsing
// ============================================================================

/**
 * Parse a single transform function, e.g., "rotate(45deg)" or "translate(10px, 20px)"
 * Uses re2c DFA lexer for function name matching.
 */
inline std::optional<TransformFunction> parse_transform_function(std::string_view str) {
    str = trim(str);
    if (str.empty()) return std::nullopt;

    // Find function name and arguments
    size_t paren_open = str.find('(');
    size_t paren_close = str.rfind(')');

    if (paren_open == std::string_view::npos || paren_close == std::string_view::npos) {
        return std::nullopt;
    }

    // Use re2c lexer for function name matching
    int func_type = fast::parse_transform_function(str.substr(0, paren_open));
    std::string_view args = str.substr(paren_open + 1, paren_close - paren_open - 1);

    // Check if this is an angle-based function
    bool is_angle_func = (func_type == CSS_TRANSFORM_ROTATE ||
                          func_type == CSS_TRANSFORM_ROTATEX ||
                          func_type == CSS_TRANSFORM_ROTATEY ||
                          func_type == CSS_TRANSFORM_ROTATEZ ||
                          func_type == CSS_TRANSFORM_SKEW ||
                          func_type == CSS_TRANSFORM_SKEWX ||
                          func_type == CSS_TRANSFORM_SKEWY);

    // Parse comma-separated values
    std::vector<float> values;
    size_t pos = 0;
    while (pos < args.size()) {
        while (pos < args.size() && (args[pos] == ' ' || args[pos] == ',' || args[pos] == '\t')) {
            pos++;
        }
        if (pos >= args.size()) break;

        size_t end = pos;
        while (end < args.size() && args[end] != ',' && args[end] != ' ' && args[end] != '\t') {
            end++;
        }

        std::string_view val_str = args.substr(pos, end - pos);
        std::string val_copy(val_str);
        char* endptr;
        float value = std::strtof(val_copy.c_str(), &endptr);

        // Convert angle units for rotation/skew functions
        if (is_angle_func) {
            std::string_view unit_str(endptr);
            if (unit_str.find("rad") != std::string_view::npos) {
                value = value * 180.0f / 3.14159265358979323846f;
            } else if (unit_str.find("turn") != std::string_view::npos) {
                value = value * 360.0f;
            }
        }

        values.push_back(value);
        pos = end;
    }

    // Create appropriate TransformFunction based on function type
    switch (func_type) {
        case CSS_TRANSFORM_TRANSLATE: {
            float x = values.size() > 0 ? values[0] : 0.0f;
            float y = values.size() > 1 ? values[1] : 0.0f;
            return TransformFunction::translate(x, y);
        }
        case CSS_TRANSFORM_TRANSLATEX:
            return TransformFunction::translateX(values.size() > 0 ? values[0] : 0.0f);
        case CSS_TRANSFORM_TRANSLATEY:
            return TransformFunction::translateY(values.size() > 0 ? values[0] : 0.0f);
        case CSS_TRANSFORM_ROTATE:
        case CSS_TRANSFORM_ROTATEX:
        case CSS_TRANSFORM_ROTATEY:
        case CSS_TRANSFORM_ROTATEZ:
            return TransformFunction::rotate(values.size() > 0 ? values[0] : 0.0f);
        case CSS_TRANSFORM_SCALE: {
            float sx = values.size() > 0 ? values[0] : 1.0f;
            float sy = values.size() > 1 ? values[1] : sx;
            return TransformFunction::scale(sx, sy);
        }
        case CSS_TRANSFORM_SCALEX:
            return TransformFunction::scaleX(values.size() > 0 ? values[0] : 1.0f);
        case CSS_TRANSFORM_SCALEY:
            return TransformFunction::scaleY(values.size() > 0 ? values[0] : 1.0f);
        case CSS_TRANSFORM_SKEWX:
            return TransformFunction::skewX(values.size() > 0 ? values[0] : 0.0f);
        case CSS_TRANSFORM_SKEWY:
            return TransformFunction::skewY(values.size() > 0 ? values[0] : 0.0f);
        case CSS_TRANSFORM_SKEW: {
            TransformFunction t;
            t.type = TransformType::SKEW;
            t.values[0] = values.size() > 0 ? values[0] : 0.0f;
            t.values[1] = values.size() > 1 ? values[1] : 0.0f;
            return t;
        }
        case CSS_TRANSFORM_MATRIX:
            if (values.size() >= 6) {
                return TransformFunction::matrix(values[0], values[1], values[2],
                                                 values[3], values[4], values[5]);
            }
            break;
        default:
            break;
    }

    return std::nullopt;
}

/**
 * Parse a complete CSS transform property value
 * E.g., "translate(10px, 20px) rotate(45deg) scale(1.5)"
 */
inline Transform parse_transform(std::string_view str) {
    Transform result;
    str = trim(str);

    if (str.empty() || str == "none") {
        return result;
    }

    size_t pos = 0;
    while (pos < str.size()) {
        // Skip whitespace
        while (pos < str.size() && std::isspace(str[pos])) {
            pos++;
        }
        if (pos >= str.size()) break;

        // Find the end of this function (closing paren)
        size_t func_start = pos;
        size_t paren_open = str.find('(', pos);
        if (paren_open == std::string_view::npos) break;

        // Find matching closing paren
        int depth = 1;
        size_t paren_close = paren_open + 1;
        while (paren_close < str.size() && depth > 0) {
            if (str[paren_close] == '(') depth++;
            else if (str[paren_close] == ')') depth--;
            paren_close++;
        }

        if (depth != 0) break;  // Unmatched parens

        // Extract and parse this function
        std::string_view func_str = str.substr(func_start, paren_close - func_start);
        auto fn = parse_transform_function(func_str);
        if (fn) {
            result.functions.push_back(*fn);
        }

        pos = paren_close;
    }

    return result;
}

/**
 * Parse transform-origin property
 * E.g., "center center", "left top", "50% 50%", "10px 20px"
 */
inline void parse_transform_origin(std::string_view str, float& origin_x, float& origin_y) {
    str = trim(str);
    origin_x = 0.5f;  // Default: center
    origin_y = 0.5f;

    if (str.empty()) return;

    auto parse_single_value = [](std::string_view val) -> float {
        val = trim(val);
        auto lower = to_lowercase(val);

        // Named values
        if (lower == "left" || lower == "top") return 0.0f;
        if (lower == "center") return 0.5f;
        if (lower == "right" || lower == "bottom") return 1.0f;

        // Percentage
        if (!val.empty() && val.back() == '%') {
            std::string num_str(val.substr(0, val.size() - 1));
            return std::strtof(num_str.c_str(), nullptr) / 100.0f;
        }

        // Pixel value - assume element size of 100 for normalization
        // (actual normalization should happen at render time with real dimensions)
        std::string num_str(val);
        return std::strtof(num_str.c_str(), nullptr) / 100.0f;
    };

    // Split by whitespace
    size_t space_pos = str.find(' ');
    if (space_pos == std::string_view::npos) {
        // Single value - applies to X, Y defaults to center
        origin_x = parse_single_value(str);
        origin_y = 0.5f;
    } else {
        // Two values
        origin_x = parse_single_value(str.substr(0, space_pos));
        origin_y = parse_single_value(str.substr(space_pos + 1));
    }
}

// ============================================================================
// CSS Transition Parsing
// ============================================================================

/**
 * Convert property name string to enum (uses re2c DFA lexer)
 */
inline TransitionProperty parse_transition_property(std::string_view name) {
    switch (fast::parse_transition_property(name)) {
        case CSS_TRANS_PROP_ALL:              return TransitionProperty::ALL;
        case CSS_TRANS_PROP_NONE:             return TransitionProperty::NONE;
        case CSS_TRANS_PROP_WIDTH:            return TransitionProperty::WIDTH;
        case CSS_TRANS_PROP_HEIGHT:           return TransitionProperty::HEIGHT;
        case CSS_TRANS_PROP_MIN_WIDTH:        return TransitionProperty::MIN_WIDTH;
        case CSS_TRANS_PROP_MIN_HEIGHT:       return TransitionProperty::MIN_HEIGHT;
        case CSS_TRANS_PROP_MAX_WIDTH:        return TransitionProperty::MAX_WIDTH;
        case CSS_TRANS_PROP_MAX_HEIGHT:       return TransitionProperty::MAX_HEIGHT;
        case CSS_TRANS_PROP_PADDING:          return TransitionProperty::PADDING;
        case CSS_TRANS_PROP_PADDING_TOP:      return TransitionProperty::PADDING_TOP;
        case CSS_TRANS_PROP_PADDING_RIGHT:    return TransitionProperty::PADDING_RIGHT;
        case CSS_TRANS_PROP_PADDING_BOTTOM:   return TransitionProperty::PADDING_BOTTOM;
        case CSS_TRANS_PROP_PADDING_LEFT:     return TransitionProperty::PADDING_LEFT;
        case CSS_TRANS_PROP_MARGIN:           return TransitionProperty::MARGIN;
        case CSS_TRANS_PROP_MARGIN_TOP:       return TransitionProperty::MARGIN_TOP;
        case CSS_TRANS_PROP_MARGIN_RIGHT:     return TransitionProperty::MARGIN_RIGHT;
        case CSS_TRANS_PROP_MARGIN_BOTTOM:    return TransitionProperty::MARGIN_BOTTOM;
        case CSS_TRANS_PROP_MARGIN_LEFT:      return TransitionProperty::MARGIN_LEFT;
        case CSS_TRANS_PROP_TOP:              return TransitionProperty::TOP;
        case CSS_TRANS_PROP_RIGHT:            return TransitionProperty::RIGHT;
        case CSS_TRANS_PROP_BOTTOM:           return TransitionProperty::BOTTOM;
        case CSS_TRANS_PROP_LEFT:             return TransitionProperty::LEFT;
        case CSS_TRANS_PROP_OPACITY:          return TransitionProperty::OPACITY;
        case CSS_TRANS_PROP_BACKGROUND_COLOR: return TransitionProperty::BACKGROUND_COLOR;
        case CSS_TRANS_PROP_COLOR:            return TransitionProperty::COLOR;
        case CSS_TRANS_PROP_BORDER_COLOR:     return TransitionProperty::BORDER_COLOR;
        case CSS_TRANS_PROP_BORDER_WIDTH:     return TransitionProperty::BORDER_WIDTH;
        case CSS_TRANS_PROP_BORDER_RADIUS:    return TransitionProperty::BORDER_RADIUS;
        case CSS_TRANS_PROP_TRANSFORM:        return TransitionProperty::TRANSFORM;
        case CSS_TRANS_PROP_FLEX_GROW:        return TransitionProperty::FLEX_GROW;
        case CSS_TRANS_PROP_FLEX_SHRINK:      return TransitionProperty::FLEX_SHRINK;
        case CSS_TRANS_PROP_GAP:              return TransitionProperty::GAP;
        case CSS_TRANS_PROP_FILL:             return TransitionProperty::FILL;
        case CSS_TRANS_PROP_STROKE:           return TransitionProperty::STROKE;
        case CSS_TRANS_PROP_STROKE_WIDTH:     return TransitionProperty::STROKE_WIDTH;
        case CSS_TRANS_PROP_FILTER:           return TransitionProperty::FILTER;
        default:                              return TransitionProperty::NONE;
    }
}

/**
 * Parse CSS time value (e.g., "0.3s", "300ms", "1s")
 * Returns time in seconds
 */
inline float parse_time(std::string_view str) {
    str = trim(str);
    if (str.empty()) return 0.0f;

    std::string s(str);

    // Check for milliseconds
    if (s.size() >= 2 && s.substr(s.size() - 2) == "ms") {
        return std::strtof(s.substr(0, s.size() - 2).c_str(), nullptr) / 1000.0f;
    }

    // Check for seconds
    if (s.size() >= 1 && s.back() == 's') {
        return std::strtof(s.substr(0, s.size() - 1).c_str(), nullptr);
    }

    // Plain number - assume seconds
    return std::strtof(s.c_str(), nullptr);
}

/**
 * Parse CSS timing function (uses re2c DFA lexer for keywords)
 * E.g., "ease", "linear", "ease-in-out", "cubic-bezier(0.4, 0, 0.2, 1)"
 */
inline TimingFunction parse_timing_function(std::string_view str) {
    str = trim(str);
    if (str.empty()) return TimingFunction::ease();

    // Try fast keyword matching first
    switch (fast::parse_timing_keyword(str)) {
        case CSS_TIMING_LINEAR:      return TimingFunction::linear();
        case CSS_TIMING_EASE:        return TimingFunction::ease();
        case CSS_TIMING_EASE_IN:     return TimingFunction::ease_in();
        case CSS_TIMING_EASE_OUT:    return TimingFunction::ease_out();
        case CSS_TIMING_EASE_IN_OUT: return TimingFunction::ease_in_out();
        default: break;  // Fall through to complex parsing
    }

    // cubic-bezier(x1, y1, x2, y2)
    auto lower = to_lowercase(str);
    if (lower.find("cubic-bezier") != std::string::npos) {
        size_t paren_start = str.find('(');
        size_t paren_end = str.rfind(')');
        if (paren_start != std::string::npos && paren_end != std::string::npos && paren_end > paren_start) {
            std::string params(str.substr(paren_start + 1, paren_end - paren_start - 1));

            // Parse 4 comma-separated values
            float values[4] = {0.25f, 0.1f, 0.25f, 1.0f};
            int idx = 0;
            size_t pos = 0;
            while (pos < params.size() && idx < 4) {
                size_t comma = params.find(',', pos);
                if (comma == std::string::npos) comma = params.size();
                std::string val_str = params.substr(pos, comma - pos);
                // Trim
                size_t start = val_str.find_first_not_of(" \t");
                size_t end = val_str.find_last_not_of(" \t");
                if (start != std::string::npos) {
                    val_str = val_str.substr(start, end - start + 1);
                }
                values[idx++] = std::strtof(val_str.c_str(), nullptr);
                pos = comma + 1;
            }

            return TimingFunction::cubic_bezier(values[0], values[1], values[2], values[3]);
        }
    }

    // steps(n, start|end)
    if (lower.find("steps") != std::string::npos) {
        size_t paren_start = str.find('(');
        size_t paren_end = str.rfind(')');
        if (paren_start != std::string::npos && paren_end != std::string::npos) {
            std::string params(str.substr(paren_start + 1, paren_end - paren_start - 1));
            size_t comma = params.find(',');
            int n = 1;
            bool start = false;
            if (comma != std::string::npos) {
                n = std::atoi(params.substr(0, comma).c_str());
                std::string direction = params.substr(comma + 1);
                start = direction.find("start") != std::string::npos;
            } else {
                n = std::atoi(params.c_str());
            }
            return TimingFunction::step(n, start);
        }
    }

    return TimingFunction::ease();
}

/**
 * Parse CSS transition shorthand
 * E.g., "opacity 0.3s ease", "all 0.5s ease-in-out 0.1s"
 * Can have multiple comma-separated transitions
 */
inline std::vector<Transition> parse_transitions(std::string_view str) {
    std::vector<Transition> result;
    str = trim(str);
    if (str.empty() || str == "none") return result;

    // Split by comma for multiple transitions
    std::string full(str);
    size_t pos = 0;
    while (pos < full.size()) {
        // Find next comma (not inside parentheses)
        size_t comma = std::string::npos;
        int depth = 0;
        for (size_t i = pos; i < full.size(); i++) {
            if (full[i] == '(') depth++;
            else if (full[i] == ')') depth--;
            else if (full[i] == ',' && depth == 0) {
                comma = i;
                break;
            }
        }

        std::string segment;
        if (comma != std::string::npos) {
            segment = full.substr(pos, comma - pos);
            pos = comma + 1;
        } else {
            segment = full.substr(pos);
            pos = full.size();
        }

        // Trim segment
        size_t start = segment.find_first_not_of(" \t");
        size_t end = segment.find_last_not_of(" \t");
        if (start == std::string::npos) continue;
        segment = segment.substr(start, end - start + 1);

        // Parse individual transition: property duration timing-function delay
        Transition t;
        t.property = TransitionProperty::ALL;
        t.duration = 0.0f;
        t.delay = 0.0f;
        t.timing = TimingFunction::ease();

        // Tokenize by whitespace (respecting parentheses)
        std::vector<std::string> tokens;
        size_t tok_start = 0;
        int paren_depth = 0;
        for (size_t i = 0; i <= segment.size(); i++) {
            if (i < segment.size()) {
                if (segment[i] == '(') paren_depth++;
                else if (segment[i] == ')') paren_depth--;
            }
            if ((i == segment.size() || (segment[i] == ' ' && paren_depth == 0)) && i > tok_start) {
                std::string tok = segment.substr(tok_start, i - tok_start);
                if (!tok.empty()) tokens.push_back(tok);
                tok_start = i + 1;
            }
        }

        // Process tokens
        int time_count = 0;
        for (const auto& tok : tokens) {
            // Check if it's a time value
            bool is_time = false;
            if (tok.find("ms") != std::string::npos ||
                (tok.back() == 's' && tok.find("step") == std::string::npos)) {
                is_time = true;
            } else {
                // Check if it starts with a digit or '.'
                for (char c : tok) {
                    if (std::isdigit(c) || c == '.') {
                        is_time = true;
                        break;
                    }
                    if (c != '-') break;
                }
            }

            if (is_time) {
                float time_val = parse_time(tok);
                if (time_count == 0) {
                    t.duration = time_val;
                } else {
                    t.delay = time_val;
                }
                time_count++;
            } else if (tok.find("cubic-bezier") != std::string::npos ||
                       tok.find("steps") != std::string::npos ||
                       tok == "linear" || tok == "ease" ||
                       tok == "ease-in" || tok == "ease-out" || tok == "ease-in-out") {
                t.timing = parse_timing_function(tok);
            } else {
                // Property name
                t.property = parse_transition_property(tok);
            }
        }

        if (t.duration > 0) {
            result.push_back(t);
        }
    }

    return result;
}

// ============================================================================
// CSS Animation Parsing
// ============================================================================

/**
 * Parse animation-direction (uses re2c DFA lexer)
 */
inline AnimationDirection parse_animation_direction(std::string_view str) {
    switch (fast::parse_animation_direction(str)) {
        case CSS_ANIM_DIR_REVERSE:           return AnimationDirection::REVERSE;
        case CSS_ANIM_DIR_ALTERNATE:         return AnimationDirection::ALTERNATE;
        case CSS_ANIM_DIR_ALTERNATE_REVERSE: return AnimationDirection::ALTERNATE_REVERSE;
        default:                             return AnimationDirection::NORMAL;
    }
}

/**
 * Parse animation-fill-mode (uses re2c DFA lexer)
 */
inline AnimationFillMode parse_animation_fill_mode(std::string_view str) {
    switch (fast::parse_animation_fill_mode(str)) {
        case CSS_ANIM_FILL_FORWARDS:  return AnimationFillMode::FORWARDS;
        case CSS_ANIM_FILL_BACKWARDS: return AnimationFillMode::BACKWARDS;
        case CSS_ANIM_FILL_BOTH:      return AnimationFillMode::BOTH;
        default:                      return AnimationFillMode::NONE;
    }
}

/**
 * Parse animation-play-state (uses re2c DFA lexer)
 */
inline AnimationPlayState parse_animation_play_state(std::string_view str) {
    switch (fast::parse_animation_play_state(str)) {
        case CSS_ANIM_PLAY_PAUSED: return AnimationPlayState::PAUSED;
        default:                   return AnimationPlayState::RUNNING;
    }
}

/**
 * Parse animation-iteration-count
 * E.g., "infinite", "3", "2.5"
 */
inline float parse_animation_iteration_count(std::string_view str) {
    str = trim(str);
    auto lower = to_lowercase(str);

    if (lower == "infinite") {
        return std::numeric_limits<float>::infinity();
    }

    return std::strtof(std::string(str).c_str(), nullptr);
}

/**
 * Parse CSS animation shorthand
 * E.g., "fadeIn 0.5s ease-in-out 0.1s infinite alternate forwards"
 * Order: name duration timing-function delay iteration-count direction fill-mode play-state
 * All except name are optional
 */
inline std::vector<Animation> parse_animations(std::string_view str) {
    std::vector<Animation> result;
    str = trim(str);
    if (str.empty() || str == "none") return result;

    // Split by comma for multiple animations
    std::string full(str);
    size_t pos = 0;
    while (pos < full.size()) {
        // Find next comma (not inside parentheses)
        size_t comma = std::string::npos;
        int depth = 0;
        for (size_t i = pos; i < full.size(); i++) {
            if (full[i] == '(') depth++;
            else if (full[i] == ')') depth--;
            else if (full[i] == ',' && depth == 0) {
                comma = i;
                break;
            }
        }

        std::string segment;
        if (comma != std::string::npos) {
            segment = full.substr(pos, comma - pos);
            pos = comma + 1;
        } else {
            segment = full.substr(pos);
            pos = full.size();
        }

        // Trim segment
        size_t start = segment.find_first_not_of(" \t");
        size_t end = segment.find_last_not_of(" \t");
        if (start == std::string::npos) continue;
        segment = segment.substr(start, end - start + 1);

        // Parse individual animation
        Animation anim;

        // Tokenize by whitespace (respecting parentheses)
        std::vector<std::string> tokens;
        size_t tok_start = 0;
        int paren_depth = 0;
        for (size_t i = 0; i <= segment.size(); i++) {
            if (i < segment.size()) {
                if (segment[i] == '(') paren_depth++;
                else if (segment[i] == ')') paren_depth--;
            }
            if ((i == segment.size() || (segment[i] == ' ' && paren_depth == 0)) && i > tok_start) {
                std::string tok = segment.substr(tok_start, i - tok_start);
                if (!tok.empty()) tokens.push_back(tok);
                tok_start = i + 1;
            }
        }

        // Process tokens
        int time_count = 0;
        for (const auto& tok : tokens) {
            std::string lower = tok;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            // Check if it's a time value
            bool is_time = false;
            if (tok.find("ms") != std::string::npos ||
                (tok.back() == 's' && tok.find("step") == std::string::npos &&
                 lower != "forwards" && lower != "backwards")) {
                is_time = true;
            }

            // Check timing functions
            bool is_timing = (tok.find("cubic-bezier") != std::string::npos ||
                              tok.find("steps") != std::string::npos ||
                              lower == "linear" || lower == "ease" ||
                              lower == "ease-in" || lower == "ease-out" || lower == "ease-in-out");

            // Check direction
            bool is_direction = (lower == "normal" || lower == "reverse" ||
                                 lower == "alternate" || lower == "alternate-reverse");

            // Check fill mode
            bool is_fill = (lower == "none" || lower == "forwards" ||
                            lower == "backwards" || lower == "both");

            // Check play state
            bool is_play_state = (lower == "running" || lower == "paused");

            // Check iteration count
            bool is_iteration = (lower == "infinite");
            if (!is_iteration && !is_time && !is_timing && !is_direction && !is_fill && !is_play_state) {
                // Check if it's a number (iteration count)
                bool all_digits = true;
                bool has_dot = false;
                for (char c : tok) {
                    if (c == '.') { has_dot = true; continue; }
                    if (!std::isdigit(c)) { all_digits = false; break; }
                }
                if (all_digits && (tok.find('.') != std::string::npos || !tok.empty())) {
                    // Could be iteration count or part of animation name
                    // If we already have a name and times, treat as iteration
                    if (!anim.name.empty() && time_count >= 1) {
                        is_iteration = true;
                    }
                }
            }

            if (is_time) {
                float time_val = parse_time(tok);
                if (time_count == 0) {
                    anim.duration = time_val;
                } else {
                    anim.delay = time_val;
                }
                time_count++;
            } else if (is_timing) {
                anim.timing = parse_timing_function(tok);
            } else if (is_direction) {
                anim.direction = parse_animation_direction(tok);
            } else if (is_fill) {
                anim.fill_mode = parse_animation_fill_mode(tok);
            } else if (is_play_state) {
                anim.play_state = parse_animation_play_state(tok);
            } else if (is_iteration || lower == "infinite") {
                anim.iteration_count = parse_animation_iteration_count(tok);
            } else if (anim.name.empty()) {
                // First unrecognized token is the animation name
                anim.name = tok;
            }
        }

        if (!anim.name.empty()) {
            result.push_back(anim);
        }
    }

    return result;
}

} // namespace convert
} // namespace cssbox

#endif // cssbox_CONVERSION_H
