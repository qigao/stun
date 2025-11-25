/*
 * NanoVG CSS - Core Type System
 *
 * Typed CSS properties for 60fps performance.
 * NO string parsing at runtime - all conversions happen during CSS parsing.
 *
 * Philosophy:
 * - Parse once, use many times
 * - Type-safe at compile time
 * - Memory-efficient (no maps, no strings)
 * - Cache-friendly (struct of arrays, not array of maps)
 */

#ifndef NANOVG_CSS_TYPES_H
#define NANOVG_CSS_TYPES_H

#include <nanovg.h>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include <array>
#include <memory>
#include <string>

namespace nvgcss {

// ============================================================================
// CSS Length Type - NO MORE -1 SENTINEL VALUES
// ============================================================================

enum class LengthUnit : uint8_t {
    AUTO,      // Automatic sizing
    PX,        // Pixels (absolute)
    PERCENT,   // Percentage (relative to parent)
    EM,        // Relative to font size
    REM,       // Relative to root font size
    VW,        // Viewport width
    VH,        // Viewport height
};

struct Length {
    LengthUnit unit;
    float value;

    // Constructors (good taste: named constructors)
    static Length auto_() { return {LengthUnit::AUTO, 0.0f}; }
    static Length px(float v) { return {LengthUnit::PX, v}; }
    static Length percent(float v) { return {LengthUnit::PERCENT, v}; }
    static Length em(float v) { return {LengthUnit::EM, v}; }
    static Length rem(float v) { return {LengthUnit::REM, v}; }
    static Length vw(float v) { return {LengthUnit::VW, v}; }
    static Length vh(float v) { return {LengthUnit::VH, v}; }

    // Query (better than checking >= 0)
    bool is_auto() const { return unit == LengthUnit::AUTO; }
    bool is_absolute() const { return unit == LengthUnit::PX; }
    bool is_relative() const { return unit >= LengthUnit::PERCENT; }

    // Resolve to pixels (context = parent size, font_size, viewport)
    float resolve(float context, float font_size, float viewport_size) const {
        switch (unit) {
            case LengthUnit::AUTO:    return 0.0f;  // Caller handles auto
            case LengthUnit::PX:      return value;
            case LengthUnit::PERCENT: return value * context / 100.0f;
            case LengthUnit::EM:      return value * font_size;
            case LengthUnit::REM:     return value * font_size;  // TODO: root font
            case LengthUnit::VW:      return value * viewport_size / 100.0f;
            case LengthUnit::VH:      return value * viewport_size / 100.0f;
        }
        return 0.0f;
    }
};

// ============================================================================
// CSS Color Type - NO MORE STRING PARSING
// ============================================================================

// NVGcolor already exists, just use it directly
using Color = NVGcolor;

// ============================================================================
// CSS Gradient Type
// ============================================================================

struct GradientStop {
    float position;  // 0.0 to 1.0
    Color color;
};

enum class GradientType : uint8_t {
    LINEAR,
    RADIAL
};

struct Gradient {
    GradientType type;
    std::vector<GradientStop> stops;

    // Linear gradient
    float angle;  // degrees (0 = to top, 90 = to right, 180 = to bottom)

    // Radial gradient
    enum class Shape : uint8_t { CIRCLE, ELLIPSE };
    Shape shape;
    std::array<float, 2> center;  // [x%, y%] normalized 0-1

    Gradient() : type(GradientType::LINEAR), angle(180.0f), shape(Shape::ELLIPSE), center{0.5f, 0.5f} {}
};

// ============================================================================
// CSS Background
// ============================================================================

enum class BackgroundType : uint8_t {
    NONE,
    COLOR,
    GRADIENT,
    IMAGE
};

struct Background {
    BackgroundType type;

    // Union-like storage (only one active)
    Color color;
    std::shared_ptr<Gradient> gradient;  // shared because gradients are big
    int image_handle;  // NanoVG image handle
    std::string gradient_css;  // Raw CSS gradient string (for painter to parse)

    // Background positioning/sizing (for images)
    enum class Size : uint8_t { AUTO, COVER, CONTAIN, EXPLICIT };
    Size size_mode;
    std::array<Length, 2> size;  // width, height

    enum class Repeat : uint8_t { NO_REPEAT, REPEAT, REPEAT_X, REPEAT_Y };
    Repeat repeat;

    std::array<Length, 2> position;  // x, y

    Background()
        : type(BackgroundType::NONE)
        , color{0,0,0,0}
        , image_handle(-1)
        , size_mode(Size::AUTO)
        , size{Length::auto_(), Length::auto_()}
        , repeat(Repeat::NO_REPEAT)
        , position{Length::percent(0), Length::percent(0)}
    {}

    static Background none() { return Background{}; }
    static Background solid(Color c) {
        Background bg;
        bg.type = BackgroundType::COLOR;
        bg.color = c;
        return bg;
    }
};

// ============================================================================
// CSS Box Shadow
// ============================================================================

struct BoxShadow {
    float offset_x, offset_y;
    float blur_radius;
    float spread_radius;
    Color color;
    bool inset;

    BoxShadow() : offset_x(0), offset_y(0), blur_radius(0), spread_radius(0),
                  color{0,0,0,0}, inset(false) {}
};

// ============================================================================
// CSS Border
// ============================================================================

enum class BorderStyle : uint8_t {
    NONE,
    SOLID,
    DASHED,
    DOTTED,
    DOUBLE,
    HIDDEN
};

struct Border {
    std::array<float, 4> width;   // top, right, bottom, left
    std::array<Color, 4> color;   // per-side colors
    std::array<BorderStyle, 4> style;  // per-side styles
    std::array<float, 4> radius;  // top-left, top-right, bottom-right, bottom-left

    Border() : width{0,0,0,0}, radius{0,0,0,0} {
        color.fill(nvgRGBA(0,0,0,255));
        style.fill(BorderStyle::NONE);
    }
};

// ============================================================================
// CSS Display & Positioning
// ============================================================================

enum class Display : uint8_t {
    BLOCK,
    FLEX,
    GRID,
    NONE
};

enum class Position : uint8_t {
    STATIC,
    RELATIVE,
    ABSOLUTE,
    FIXED
};

enum class BoxSizing : uint8_t {
    CONTENT_BOX,
    BORDER_BOX
};

enum class Overflow : uint8_t {
    VISIBLE,
    HIDDEN,
    SCROLL,
    AUTO
};

// ============================================================================
// CSS Flexbox Properties
// ============================================================================

enum class FlexDirection : uint8_t {
    ROW,
    ROW_REVERSE,
    COLUMN,
    COLUMN_REVERSE
};

enum class FlexWrap : uint8_t {
    NOWRAP,
    WRAP,
    WRAP_REVERSE
};

enum class JustifyContent : uint8_t {
    FLEX_START,
    FLEX_END,
    CENTER,
    SPACE_BETWEEN,
    SPACE_AROUND,
    SPACE_EVENLY
};

enum class AlignItems : uint8_t {
    FLEX_START,
    FLEX_END,
    CENTER,
    STRETCH,
    BASELINE
};

enum class AlignContent : uint8_t {
    FLEX_START,
    FLEX_END,
    CENTER,
    STRETCH,
    SPACE_BETWEEN,
    SPACE_AROUND
};

// ============================================================================
// CSS Grid Properties
// ============================================================================

struct GridTrack {
    enum class Type : uint8_t { PX, FR, AUTO, MINMAX };
    Type type;
    float value;          // for PX/FR
    float min_val, max_val;  // for MINMAX

    static GridTrack px(float v) { return {Type::PX, v, 0, 0}; }
    static GridTrack fr(float v) { return {Type::FR, v, 0, 0}; }
    static GridTrack auto_() { return {Type::AUTO, 0, 0, 0}; }
    static GridTrack minmax(float min, float max) { return {Type::MINMAX, 0, min, max}; }
};

struct GridPlacement {
    int start;  // 1-based, -1 = auto
    int end;    // 1-based, -1 = auto
    int span;   // >= 1

    GridPlacement() : start(-1), end(-1), span(1) {}
};

// ============================================================================
// CSS Text Properties
// ============================================================================

enum class TextAlign : uint8_t {
    LEFT,
    CENTER,
    RIGHT,
    JUSTIFY
};

enum class FontWeight : uint16_t {
    NORMAL = 400,
    BOLD = 700
};

enum class FontStyle : uint8_t {
    NORMAL,
    ITALIC,
    OBLIQUE
};

// ============================================================================
// CSS Text Decoration
// ============================================================================

enum class TextDecoration : uint8_t {
    NONE,
    UNDERLINE,
    OVERLINE,
    LINE_THROUGH
};

enum class TextTransform : uint8_t {
    NONE,
    UPPERCASE,
    LOWERCASE,
    CAPITALIZE
};

// ============================================================================
// CSS SVG Properties
// ============================================================================

enum class StrokeRendering : uint8_t {
    AUTO,      // Default smooth rendering
    ROUGH      // Hand-drawn style (RoughJS)
};

struct SVGStroke {
    Color color = nvgRGBA(0, 0, 0, 255);
    float width = 1.0f;
    
    enum class LineCap : uint8_t { BUTT, ROUND, SQUARE };
    LineCap line_cap = LineCap::BUTT;
    
    enum class LineJoin : uint8_t { MITER, ROUND, BEVEL };
    LineJoin line_join = LineJoin::MITER;
    
    float miter_limit = 4.0f;
    std::vector<float> dash_array;
    float dash_offset = 0.0f;
    bool enabled = false;
    
    // Rough rendering properties
    StrokeRendering rendering = StrokeRendering::AUTO;
    float roughness = 1.0f;      // 0-10, default 1
    float bowing = 1.0f;         // 0-10, default 1
    int stroke_count = 1;        // 1-5, number of overlapping strokes
    unsigned int seed = 0;       // Random seed for reproducibility
};

struct SVGFill {
    Color color = nvgRGBA(128, 128, 128, 255);
    bool enabled = true;
};

// ============================================================================
// COMPUTED STYLE STRUCT - The Heart of the System
// ============================================================================

/**
 * @brief Fully computed CSS properties (NO strings, NO parsing at runtime)
 *
 * This struct replaces map<string, string>.
 * All values are pre-parsed and ready to use.
 *
 * Size: ~400 bytes (vs potentially MBs for string maps)
 * Access: O(1) direct field access (vs O(log n) map lookup)
 * Type-safe: Compiler catches typos (vs runtime errors)
 */
struct ComputedStyle {
    // === Layout ===
    Display display = Display::BLOCK;
    Position position = Position::STATIC;
    BoxSizing box_sizing = BoxSizing::CONTENT_BOX;

    Length width = Length::auto_();
    Length height = Length::auto_();
    Length min_width = Length::auto_();
    Length min_height = Length::auto_();
    Length max_width = Length::auto_();
    Length max_height = Length::auto_();

    // Position offsets
    Length top = Length::auto_();
    Length right = Length::auto_();
    Length bottom = Length::auto_();
    Length left = Length::auto_();

    int z_index = 0;

    // === Box Model ===
    std::array<Length, 4> padding{Length::px(0), Length::px(0), Length::px(0), Length::px(0)};
    std::array<Length, 4> margin{Length::px(0), Length::px(0), Length::px(0), Length::px(0)};
    Border border;

    Overflow overflow_x = Overflow::VISIBLE;
    Overflow overflow_y = Overflow::VISIBLE;

    // === Visual ===
    Background background;
    std::vector<BoxShadow> box_shadows;
    float opacity = 1.0f;

    // === Flexbox ===
    FlexDirection flex_direction = FlexDirection::ROW;
    FlexWrap flex_wrap = FlexWrap::NOWRAP;
    JustifyContent justify_content = JustifyContent::FLEX_START;
    AlignItems align_items = AlignItems::STRETCH;
    AlignContent align_content = AlignContent::FLEX_START;

    float flex_grow = 0.0f;
    float flex_shrink = 1.0f;
    Length flex_basis = Length::auto_();
    int order = 0;
    Length gap = Length::px(0);

    // === Grid ===
    std::vector<GridTrack> grid_template_rows;
    std::vector<GridTrack> grid_template_columns;
    Length grid_row_gap = Length::px(0);
    Length grid_column_gap = Length::px(0);
    GridPlacement grid_row;
    GridPlacement grid_column;

    // === Text ===
    Color color = nvgRGBA(0, 0, 0, 255);
    float font_size = 16.0f;  // Always in px after resolution
    FontWeight font_weight = FontWeight::NORMAL;
    FontStyle font_style = FontStyle::NORMAL;
    TextAlign text_align = TextAlign::LEFT;
    std::string font_family = "sans-serif";  // Keep string for NanoVG API
    TextDecoration text_decoration = TextDecoration::NONE;
    TextTransform text_transform = TextTransform::NONE;

    // === SVG Properties ===
    SVGFill svg_fill;
    SVGStroke svg_stroke;

    // === Filter Effects ===
    std::vector<struct FilterEffect> filters;

    // === Transform (Phase 2) ===
    // TODO: Add transform properties when needed

    // === Transition/Animation (Phase 2) ===
    // TODO: Add animation properties when needed
};

// ============================================================================
// CSS Filter Effects
// ============================================================================

enum class FilterType : uint8_t {
    BLUR,
    BRIGHTNESS,
    CONTRAST,
    GRAYSCALE,
    HUE_ROTATE,
    INVERT,
    OPACITY,
    SATURATE,
    SEPIA
};

struct FilterEffect {
    FilterType type;
    float value;  // Interpretation depends on type

    FilterEffect(FilterType t, float v) : type(t), value(v) {}

    // Named constructors for clarity
    static FilterEffect blur(float radius) { return {FilterType::BLUR, radius}; }
    static FilterEffect brightness(float amount) { return {FilterType::BRIGHTNESS, amount}; }
    static FilterEffect contrast(float amount) { return {FilterType::CONTRAST, amount}; }
    static FilterEffect grayscale(float amount) { return {FilterType::GRAYSCALE, amount}; }
    static FilterEffect hue_rotate(float degrees) { return {FilterType::HUE_ROTATE, degrees}; }
    static FilterEffect invert(float amount) { return {FilterType::INVERT, amount}; }
    static FilterEffect opacity(float amount) { return {FilterType::OPACITY, amount}; }
    static FilterEffect saturate(float amount) { return {FilterType::SATURATE, amount}; }
    static FilterEffect sepia(float amount) { return {FilterType::SEPIA, amount}; }
};

// ============================================================================
// Resolved Layout (After Layout Engine)
// ============================================================================

/**
 * @brief Computed layout results in absolute pixels
 *
 * This is the OUTPUT of layout engine.
 * ComputedStyle is the INPUT.
 */
struct ResolvedLayout {
    float x, y;              // Absolute position (pixels)
    float width, height;     // Content box size (pixels)

    std::array<float, 4> padding;  // Resolved padding (pixels)
    std::array<float, 4> margin;   // Resolved margin (pixels)
    std::array<float, 4> border;   // Resolved border width (pixels)
    std::array<float, 4> radius;   // Resolved border radius (pixels)

    enum class Source : uint8_t {
        UNCOMPUTED,
        EXPLICIT,
        FLEXBOX,
        GRID,
        FLOW
    };
    Source source = Source::UNCOMPUTED;

    bool is_positioned = false;  // position != static
};

// ============================================================================
// Dirty Flags - For 60fps Optimization
// ============================================================================

enum DirtyFlags : uint32_t {
    DIRTY_NONE      = 0,
    DIRTY_STYLE     = 1 << 0,   // CSS properties changed
    DIRTY_LAYOUT    = 1 << 1,   // Need to recompute layout
    DIRTY_TRANSFORM = 1 << 2,   // Need to recompute transforms
    DIRTY_PAINT     = 1 << 3,   // Need to repaint
    DIRTY_CHILDREN  = 1 << 4,   // Children dirty (propagate)

    DIRTY_ALL = 0xFFFFFFFF
};

inline DirtyFlags operator|(DirtyFlags a, DirtyFlags b) {
    return static_cast<DirtyFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline DirtyFlags& operator|=(DirtyFlags& a, DirtyFlags b) {
    a = a | b;
    return a;
}

inline DirtyFlags operator&(DirtyFlags a, DirtyFlags b) {
    return static_cast<DirtyFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline DirtyFlags& operator&=(DirtyFlags& a, DirtyFlags b) {
    a = static_cast<DirtyFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
    return a;
}

inline DirtyFlags operator~(DirtyFlags a) {
    return static_cast<DirtyFlags>(~static_cast<uint32_t>(a));
}

} // namespace nvgcss

#endif // NANOVG_CSS_TYPES_H
