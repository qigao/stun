#pragma once

namespace cssbox {
namespace defaults {

// CSS Standard Default Values
// Reference: https://www.w3.org/TR/CSS2/

// Typography
constexpr const char* FONT_SIZE = "16";           // CSS standard: 16px (medium)
constexpr const char* FONT_FAMILY = "sans-serif"; // CSS standard
constexpr const char* FONT_WEIGHT = "normal";     // CSS standard: 400
constexpr const char* TEXT_ALIGN = "left";        // CSS standard
constexpr const char* LINE_HEIGHT = "normal";     // CSS standard: ~1.2

// Colors
constexpr const char* COLOR = "black";            // CSS standard: inherited
constexpr const char* BACKGROUND = "transparent"; // CSS standard: transparent

// Opacity
constexpr const char* OPACITY = "1";              // CSS standard: fully opaque

// SVG Standard Default Values
// Reference: https://www.w3.org/TR/SVG2/

// SVG Fill & Stroke
constexpr const char* FILL = "none";              // SVG standard: none (not black!)
constexpr const char* STROKE = "none";            // SVG standard: none
constexpr const char* STROKE_WIDTH = "1";         // SVG standard: 1px

// Note: SVG shapes have NO default fill color in the standard.
// If you want a visible shape, you must explicitly set fill.
// This is different from HTML elements which have default backgrounds.

} // namespace defaults
} // namespace cssbox
