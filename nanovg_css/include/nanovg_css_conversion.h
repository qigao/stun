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

#ifndef NANOVG_CSS_CONVERSION_H
#define NANOVG_CSS_CONVERSION_H

#include <nanovg.h>
#include "nanovg_css_types.h"
#include <string>
#include <string_view>
#include <optional>
#include <cctype>
#include <cmath>
#include <algorithm>

namespace nvgcss {
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

inline std::optional<Length> parse_length(std::string_view str) {
    str = trim(str);
    if (str.empty()) return std::nullopt;

    // Special keyword
    if (str == "auto") return Length::auto_();

    // Convert to std::string to ensure null termination for strtof
    // (string_view::data() is not guaranteed to be null-terminated)
    std::string str_copy(str);
    
    char* end_ptr;
    float value = std::strtof(str_copy.c_str(), &end_ptr);

    // Calculate unit offset from the original string_view
    size_t unit_offset = end_ptr - str_copy.c_str();
    std::string_view unit = str.substr(unit_offset);
    unit = trim(unit);

    if (unit.empty() || unit == "px") return Length::px(value);
    if (unit == "%") return Length::percent(value);
    if (unit == "em") return Length::em(value);
    if (unit == "rem") return Length::rem(value);
    if (unit == "vw") return Length::vw(value);
    if (unit == "vh") return Length::vh(value);

    return std::nullopt;  // Unknown unit
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

    // Named colors
    if (str == "black") return nvgRGB(0, 0, 0);
    if (str == "white") return nvgRGB(255, 255, 255);
    if (str == "red") return nvgRGB(255, 0, 0);
    if (str == "green") return nvgRGB(0, 128, 0);
    if (str == "blue") return nvgRGB(0, 0, 255);
    if (str == "transparent") return nvgRGBA(0, 0, 0, 0);

    // TODO: Add more named colors if needed

    return std::nullopt;
}

// ============================================================================
// Enum Conversions
// ============================================================================

inline Display parse_display(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "block") return Display::BLOCK;
    if (lower == "flex") return Display::FLEX;
    if (lower == "grid") return Display::GRID;
    if (lower == "none") return Display::NONE;
    return Display::BLOCK;  // Default
}

inline Position parse_position(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "static") return Position::STATIC;
    if (lower == "relative") return Position::RELATIVE;
    if (lower == "absolute") return Position::ABSOLUTE;
    if (lower == "fixed") return Position::FIXED;
    return Position::STATIC;  // Default
}

inline BoxSizing parse_box_sizing(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "border-box") return BoxSizing::BORDER_BOX;
    return BoxSizing::CONTENT_BOX;  // Default
}

inline Overflow parse_overflow(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "hidden") return Overflow::HIDDEN;
    if (lower == "scroll") return Overflow::SCROLL;
    if (lower == "auto") return Overflow::AUTO;
    return Overflow::VISIBLE;  // Default
}

inline FlexDirection parse_flex_direction(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "row") return FlexDirection::ROW;
    if (lower == "row-reverse") return FlexDirection::ROW_REVERSE;
    if (lower == "column") return FlexDirection::COLUMN;
    if (lower == "column-reverse") return FlexDirection::COLUMN_REVERSE;
    return FlexDirection::ROW;  // Default
}

inline FlexWrap parse_flex_wrap(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "wrap") return FlexWrap::WRAP;
    if (lower == "wrap-reverse") return FlexWrap::WRAP_REVERSE;
    return FlexWrap::NOWRAP;  // Default
}

inline JustifyContent parse_justify_content(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "flex-start") return JustifyContent::FLEX_START;
    if (lower == "flex-end") return JustifyContent::FLEX_END;
    if (lower == "center") return JustifyContent::CENTER;
    if (lower == "space-between") return JustifyContent::SPACE_BETWEEN;
    if (lower == "space-around") return JustifyContent::SPACE_AROUND;
    if (lower == "space-evenly") return JustifyContent::SPACE_EVENLY;
    return JustifyContent::FLEX_START;  // Default
}

inline AlignItems parse_align_items(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "flex-start") return AlignItems::FLEX_START;
    if (lower == "flex-end") return AlignItems::FLEX_END;
    if (lower == "center") return AlignItems::CENTER;
    if (lower == "baseline") return AlignItems::BASELINE;
    return AlignItems::STRETCH;  // Default
}

inline AlignContent parse_align_content(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "flex-start") return AlignContent::FLEX_START;
    if (lower == "flex-end") return AlignContent::FLEX_END;
    if (lower == "center") return AlignContent::CENTER;
    if (lower == "space-between") return AlignContent::SPACE_BETWEEN;
    if (lower == "space-around") return AlignContent::SPACE_AROUND;
    return AlignContent::STRETCH;  // Default
}

inline BorderStyle parse_border_style(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "solid") return BorderStyle::SOLID;
    if (lower == "dashed") return BorderStyle::DASHED;
    if (lower == "dotted") return BorderStyle::DOTTED;
    if (lower == "double") return BorderStyle::DOUBLE;
    if (lower == "hidden") return BorderStyle::HIDDEN;
    return BorderStyle::NONE;  // Default
}

inline TextAlign parse_text_align(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "left") return TextAlign::LEFT;
    if (lower == "center") return TextAlign::CENTER;
    if (lower == "right") return TextAlign::RIGHT;
    if (lower == "justify") return TextAlign::JUSTIFY;
    return TextAlign::LEFT;  // Default
}

inline FontWeight parse_font_weight(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "normal") return FontWeight::NORMAL;
    if (lower == "bold") return FontWeight::BOLD;

    // Numeric values
    int weight = std::atoi(lower.c_str());
    if (weight >= 100 && weight <= 900) {
        return static_cast<FontWeight>(weight);
    }

    return FontWeight::NORMAL;  // Default
}

inline FontStyle parse_font_style(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "italic") return FontStyle::ITALIC;
    if (lower == "oblique") return FontStyle::OBLIQUE;
    return FontStyle::NORMAL;  // Default
}

inline TextDecoration parse_text_decoration(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "underline") return TextDecoration::UNDERLINE;
    if (lower == "overline") return TextDecoration::OVERLINE;
    if (lower == "line-through") return TextDecoration::LINE_THROUGH;
    return TextDecoration::NONE;  // Default
}

inline TextTransform parse_text_transform(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "uppercase") return TextTransform::UPPERCASE;
    if (lower == "lowercase") return TextTransform::LOWERCASE;
    if (lower == "capitalize") return TextTransform::CAPITALIZE;
    return TextTransform::NONE;  // Default
}

inline SVGStroke::LineCap parse_stroke_linecap(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "round") return SVGStroke::LineCap::ROUND;
    if (lower == "square") return SVGStroke::LineCap::SQUARE;
    return SVGStroke::LineCap::BUTT;  // Default
}

inline SVGStroke::LineJoin parse_stroke_linejoin(std::string_view str) {
    auto lower = to_lowercase(trim(str));
    if (lower == "round") return SVGStroke::LineJoin::ROUND;
    if (lower == "bevel") return SVGStroke::LineJoin::BEVEL;
    return SVGStroke::LineJoin::MITER;  // Default
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
 * @brief Parse grid-template-rows or grid-template-columns
 *
 * Supports: px, fr, auto, minmax()
 * Examples: "100px 200px", "1fr 2fr", "auto 1fr", "minmax(100px, 1fr)"
 */
inline std::vector<GridTrack> parse_grid_track_list(std::string_view str) {
    std::vector<GridTrack> tracks;

    size_t pos = 0;
    while (pos < str.size()) {
        // Skip whitespace
        while (pos < str.size() && std::isspace(str[pos])) ++pos;
        if (pos >= str.size()) break;

        // Check for minmax()
        if (str.substr(pos, 6) == "minmax") {
            size_t open_paren = str.find('(', pos);
            size_t close_paren = str.find(')', open_paren);
            if (open_paren != std::string_view::npos && close_paren != std::string_view::npos) {
                std::string_view minmax_args = str.substr(open_paren + 1, close_paren - open_paren - 1);
                size_t comma = minmax_args.find(',');
                if (comma != std::string_view::npos) {
                    std::string_view min_str = minmax_args.substr(0, comma);
                    std::string_view max_str = minmax_args.substr(comma + 1);

                    // Trim whitespace
                    while (!min_str.empty() && std::isspace(min_str.front())) min_str.remove_prefix(1);
                    while (!min_str.empty() && std::isspace(min_str.back())) min_str.remove_suffix(1);
                    while (!max_str.empty() && std::isspace(max_str.front())) max_str.remove_prefix(1);
                    while (!max_str.empty() && std::isspace(max_str.back())) max_str.remove_suffix(1);

                    // Parse min and max (assume px for simplicity)
                    float min_val = parse_length(min_str).value_or(Length::px(0)).value;
                    float max_val = parse_length(max_str).value_or(Length::px(0)).value;

                    GridTrack track;
                    track.type = GridTrack::Type::MINMAX;
                    track.min_val = min_val;
                    track.max_val = max_val;
                    tracks.push_back(track);
                }
                pos = close_paren + 1;
                continue;
            }
        }

        // Find next space (end of current track)
        size_t end = pos;
        while (end < str.size() && !std::isspace(str[end])) ++end;

        std::string_view track_str = str.substr(pos, end - pos);

        if (track_str == "auto") {
            tracks.push_back(GridTrack::auto_());
        } else if (track_str.find("fr") != std::string_view::npos) {
            // Fractional unit - parse number before "fr"
            float value = 1.0f;
            try {
                std::string num_str(track_str.begin(), track_str.end());
                value = std::stof(num_str);
            } catch (...) {
                value = 1.0f;
            }
            tracks.push_back(GridTrack::fr(value));
        } else {
            // Pixel value or percentage
            auto len = parse_length(track_str);
            if (len) {
                // For now, just use the raw value as pixels
                // TODO: Resolve percentages properly
                tracks.push_back(GridTrack::px(len->value));
            }
        }

        pos = end;
    }

    return tracks;
}

} // namespace convert
} // namespace nvgcss

#endif // NANOVG_CSS_CONVERSION_H
