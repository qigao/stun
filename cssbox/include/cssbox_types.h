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

#ifndef cssbox_TYPES_H
#define cssbox_TYPES_H

// Windows macro conflict fixes (must be before includes)
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <nanovg.h>
#include <cstdint>
#include <optional>
#include <variant>
#include <vector>
#include <array>
#include <memory>
#include <string>
#include <cmath>
#include <unordered_map>

// Windows macro cleanup (must be after includes)
// wingdi.h defines RELATIVE and ABSOLUTE as macros
#ifdef RELATIVE
#undef RELATIVE
#endif
#ifdef ABSOLUTE
#undef ABSOLUTE
#endif

namespace cssbox {

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
    CALC,      // calc() expression (stores px + percent components)
};

struct Length {
    LengthUnit unit;
    float value;
    float calc_percent = 0.0f;  // For CALC: percentage component

    // Constructors (good taste: named constructors)
    static Length auto_() { return {LengthUnit::AUTO, 0.0f, 0.0f}; }
    static Length px(float v) { return {LengthUnit::PX, v, 0.0f}; }
    static Length percent(float v) { return {LengthUnit::PERCENT, v, 0.0f}; }
    static Length em(float v) { return {LengthUnit::EM, v, 0.0f}; }
    static Length rem(float v) { return {LengthUnit::REM, v, 0.0f}; }
    static Length vw(float v) { return {LengthUnit::VW, v, 0.0f}; }
    static Length vh(float v) { return {LengthUnit::VH, v, 0.0f}; }
    static Length calc(float px_val, float percent_val) {
        return {LengthUnit::CALC, px_val, percent_val};
    }

    // Query (better than checking >= 0)
    bool is_auto() const { return unit == LengthUnit::AUTO; }
    bool is_absolute() const { return unit == LengthUnit::PX; }
    bool is_relative() const { return unit >= LengthUnit::PERCENT && unit != LengthUnit::CALC; }
    bool is_calc() const { return unit == LengthUnit::CALC; }

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
            case LengthUnit::CALC:
                // calc() stores px in value and percent in calc_percent
                return value + (calc_percent * context / 100.0f);
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
// CSS Text Shadow
// ============================================================================

struct TextShadow {
    float offset_x;
    float offset_y;
    float blur_radius;
    Color color;

    TextShadow() : offset_x(0), offset_y(0), blur_radius(0), color{0,0,0,0} {}
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

// Grid/Flex item self-alignment (align-self, justify-self)
enum class AlignSelf : uint8_t {
    AUTO,       // Inherit from container's align-items
    START,
    END,
    CENTER,
    STRETCH,
    BASELINE
};

enum class JustifySelf : uint8_t {
    AUTO,       // Inherit from container's justify-items
    START,
    END,
    CENTER,
    STRETCH
};

// Grid container item alignment (justify-items)
enum class JustifyItems : uint8_t {
    START,
    END,
    CENTER,
    STRETCH     // Default
};

// ============================================================================
// CSS Grid Properties
// ============================================================================

// Size type for minmax() parameters
enum class GridSize : uint8_t {
    PX,           // Fixed pixel value
    FR,           // Flexible fraction
    PERCENT,      // Percentage of container
    AUTO,         // auto
    MIN_CONTENT,  // min-content
    MAX_CONTENT,  // max-content
};

// Single size value (used in minmax, fit-content)
struct GridSizeValue {
    GridSize type = GridSize::AUTO;
    float value = 0;

    static GridSizeValue px(float v) { return {GridSize::PX, v}; }
    static GridSizeValue fr(float v) { return {GridSize::FR, v}; }
    static GridSizeValue percent(float v) { return {GridSize::PERCENT, v}; }
    static GridSizeValue auto_() { return {GridSize::AUTO, 0}; }
    static GridSizeValue min_content() { return {GridSize::MIN_CONTENT, 0}; }
    static GridSizeValue max_content() { return {GridSize::MAX_CONTENT, 0}; }
};

struct GridTrack {
    enum class Type : uint8_t {
        PX,           // Fixed pixel: 100px
        FR,           // Flexible: 1fr
        AUTO,         // auto
        MINMAX,       // minmax(min, max)
        FIT_CONTENT,  // fit-content(limit)
        MIN_CONTENT,  // min-content
        MAX_CONTENT,  // max-content
    };

    Type type = Type::AUTO;
    float value = 0;              // for PX/FR/FIT_CONTENT
    GridSizeValue min_size;       // for MINMAX
    GridSizeValue max_size;       // for MINMAX

    // Named grid lines that come BEFORE this track
    // Example: [sidebar-start] 200px -> line_names = {"sidebar-start"}
    std::vector<std::string> line_names;

    // Simple constructors
    static GridTrack px(float v) { return {Type::PX, v, {}, {}, {}}; }
    static GridTrack fr(float v) { return {Type::FR, v, {}, {}, {}}; }
    static GridTrack auto_() { return {Type::AUTO, 0, {}, {}, {}}; }
    static GridTrack min_content() { return {Type::MIN_CONTENT, 0, {}, {}, {}}; }
    static GridTrack max_content() { return {Type::MAX_CONTENT, 0, {}, {}, {}}; }
    static GridTrack fit_content(float limit) { return {Type::FIT_CONTENT, limit, {}, {}, {}}; }

    // minmax() with flexible parameters
    static GridTrack minmax(GridSizeValue min, GridSizeValue max) {
        GridTrack t;
        t.type = Type::MINMAX;
        t.min_size = min;
        t.max_size = max;
        return t;
    }

    // Convenience: minmax(px, px)
    static GridTrack minmax(float min_px, float max_px) {
        return minmax(GridSizeValue::px(min_px), GridSizeValue::px(max_px));
    }
};

// repeat() function support
struct GridRepeat {
    enum class CountType : uint8_t {
        INTEGER,    // repeat(3, ...)
        AUTO_FILL,  // repeat(auto-fill, ...)
        AUTO_FIT,   // repeat(auto-fit, ...)
    };

    CountType count_type = CountType::INTEGER;
    int count = 1;                        // for INTEGER type
    std::vector<GridTrack> tracks;        // tracks to repeat

    static GridRepeat integer(int n, std::vector<GridTrack> t) {
        return {CountType::INTEGER, n, std::move(t)};
    }
    static GridRepeat auto_fill(std::vector<GridTrack> t) {
        return {CountType::AUTO_FILL, 0, std::move(t)};
    }
    static GridRepeat auto_fit(std::vector<GridTrack> t) {
        return {CountType::AUTO_FIT, 0, std::move(t)};
    }
};

// A template item can be either a track or a repeat
struct GridTemplateItem {
    enum class Kind : uint8_t { TRACK, REPEAT };
    Kind kind = Kind::TRACK;
    GridTrack track;
    GridRepeat repeat;

    static GridTemplateItem from_track(GridTrack t) {
        GridTemplateItem item;
        item.kind = Kind::TRACK;
        item.track = t;
        return item;
    }
    static GridTemplateItem from_repeat(GridRepeat r) {
        GridTemplateItem item;
        item.kind = Kind::REPEAT;
        item.repeat = std::move(r);
        return item;
    }
};

struct GridPlacement {
    int start;  // 1-based, -1 = auto
    int end;    // 1-based, -1 = auto
    int span;   // >= 1

    // Named line references (used when start/end is -1 but name is set)
    // Example: grid-column: sidebar-start / content-end
    std::string start_name;
    std::string end_name;

    GridPlacement() : start(-1), end(-1), span(1) {}

    // Check if using named lines
    bool has_named_start() const { return !start_name.empty(); }
    bool has_named_end() const { return !end_name.empty(); }
};

/**
 * @brief Grid template area definition
 *
 * Represents a named area in grid-template-areas.
 * Example: "header header" "sidebar main" creates areas for header, sidebar, main
 */
struct GridAreaDef {
    int row_start = 0;   // 0-based row index
    int col_start = 0;   // 0-based column index
    int row_end = 1;     // Exclusive end row
    int col_end = 1;     // Exclusive end column

    GridAreaDef() = default;
    GridAreaDef(int rs, int cs, int re, int ce)
        : row_start(rs), col_start(cs), row_end(re), col_end(ce) {}
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

enum class VerticalAlign : uint8_t {
    TOP,
    MIDDLE,
    BOTTOM,
    BASELINE
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
// CSS Transform System
// ============================================================================

enum class TransformType : uint8_t {
    TRANSLATE,
    TRANSLATE_X,
    TRANSLATE_Y,
    ROTATE,
    SCALE,
    SCALE_X,
    SCALE_Y,
    SKEW,
    SKEW_X,
    SKEW_Y,
    MATRIX
};

struct TransformFunction {
    TransformType type;
    float values[6];  // Up to 6 values for matrix()

    // Named constructors
    static TransformFunction translate(float x, float y) {
        TransformFunction t;
        t.type = TransformType::TRANSLATE;
        t.values[0] = x;
        t.values[1] = y;
        return t;
    }

    static TransformFunction translateX(float x) {
        TransformFunction t;
        t.type = TransformType::TRANSLATE_X;
        t.values[0] = x;
        return t;
    }

    static TransformFunction translateY(float y) {
        TransformFunction t;
        t.type = TransformType::TRANSLATE_Y;
        t.values[0] = y;
        return t;
    }

    static TransformFunction rotate(float angle_deg) {
        TransformFunction t;
        t.type = TransformType::ROTATE;
        t.values[0] = angle_deg;
        return t;
    }

    static TransformFunction scale(float sx, float sy) {
        TransformFunction t;
        t.type = TransformType::SCALE;
        t.values[0] = sx;
        t.values[1] = sy;
        return t;
    }

    static TransformFunction scaleX(float sx) {
        TransformFunction t;
        t.type = TransformType::SCALE_X;
        t.values[0] = sx;
        return t;
    }

    static TransformFunction scaleY(float sy) {
        TransformFunction t;
        t.type = TransformType::SCALE_Y;
        t.values[0] = sy;
        return t;
    }

    static TransformFunction skewX(float angle_deg) {
        TransformFunction t;
        t.type = TransformType::SKEW_X;
        t.values[0] = angle_deg;
        return t;
    }

    static TransformFunction skewY(float angle_deg) {
        TransformFunction t;
        t.type = TransformType::SKEW_Y;
        t.values[0] = angle_deg;
        return t;
    }

    static TransformFunction matrix(float a, float b, float c, float d, float e, float f) {
        TransformFunction t;
        t.type = TransformType::MATRIX;
        t.values[0] = a;
        t.values[1] = b;
        t.values[2] = c;
        t.values[3] = d;
        t.values[4] = e;
        t.values[5] = f;
        return t;
    }
};

struct Transform {
    std::vector<TransformFunction> functions;
    float origin_x = 0.5f;  // 0-1 normalized (0.5 = center)
    float origin_y = 0.5f;

    bool empty() const { return functions.empty(); }

    // Compute the composed 2D affine matrix [a, b, c, d, e, f]
    // | a c e |   | x |   | a*x + c*y + e |
    // | b d f | * | y | = | b*x + d*y + f |
    // | 0 0 1 |   | 1 |   |       1       |
    void compose_matrix(float result[6]) const {
        // Start with identity
        result[0] = 1.0f; result[1] = 0.0f;
        result[2] = 0.0f; result[3] = 1.0f;
        result[4] = 0.0f; result[5] = 0.0f;

        for (const auto& fn : functions) {
            float m[6];
            function_to_matrix(fn, m);
            multiply_matrix(result, m, result);
        }
    }

private:
    static void function_to_matrix(const TransformFunction& fn, float m[6]) {
        // Initialize to identity
        m[0] = 1.0f; m[1] = 0.0f;
        m[2] = 0.0f; m[3] = 1.0f;
        m[4] = 0.0f; m[5] = 0.0f;

        constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

        switch (fn.type) {
            case TransformType::TRANSLATE:
                m[4] = fn.values[0];
                m[5] = fn.values[1];
                break;

            case TransformType::TRANSLATE_X:
                m[4] = fn.values[0];
                break;

            case TransformType::TRANSLATE_Y:
                m[5] = fn.values[0];
                break;

            case TransformType::ROTATE: {
                float rad = fn.values[0] * DEG_TO_RAD;
                float c = std::cos(rad);
                float s = std::sin(rad);
                m[0] = c;  m[1] = s;
                m[2] = -s; m[3] = c;
                break;
            }

            case TransformType::SCALE:
                m[0] = fn.values[0];
                m[3] = fn.values[1];
                break;

            case TransformType::SCALE_X:
                m[0] = fn.values[0];
                break;

            case TransformType::SCALE_Y:
                m[3] = fn.values[0];
                break;

            case TransformType::SKEW_X: {
                float rad = fn.values[0] * DEG_TO_RAD;
                m[2] = std::tan(rad);
                break;
            }

            case TransformType::SKEW_Y: {
                float rad = fn.values[0] * DEG_TO_RAD;
                m[1] = std::tan(rad);
                break;
            }

            case TransformType::SKEW: {
                float radX = fn.values[0] * DEG_TO_RAD;
                float radY = fn.values[1] * DEG_TO_RAD;
                m[1] = std::tan(radY);
                m[2] = std::tan(radX);
                break;
            }

            case TransformType::MATRIX:
                m[0] = fn.values[0];
                m[1] = fn.values[1];
                m[2] = fn.values[2];
                m[3] = fn.values[3];
                m[4] = fn.values[4];
                m[5] = fn.values[5];
                break;
        }
    }

    // Matrix multiplication: result = a * b
    static void multiply_matrix(const float a[6], const float b[6], float result[6]) {
        float temp[6];
        temp[0] = a[0] * b[0] + a[2] * b[1];
        temp[1] = a[1] * b[0] + a[3] * b[1];
        temp[2] = a[0] * b[2] + a[2] * b[3];
        temp[3] = a[1] * b[2] + a[3] * b[3];
        temp[4] = a[0] * b[4] + a[2] * b[5] + a[4];
        temp[5] = a[1] * b[4] + a[3] * b[5] + a[5];

        result[0] = temp[0];
        result[1] = temp[1];
        result[2] = temp[2];
        result[3] = temp[3];
        result[4] = temp[4];
        result[5] = temp[5];
    }
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
    GridTrack grid_auto_rows = GridTrack::auto_();    // Implicit row sizing
    GridTrack grid_auto_columns = GridTrack::auto_(); // Implicit column sizing
    Length grid_row_gap = Length::px(0);
    Length grid_column_gap = Length::px(0);
    GridPlacement grid_row;
    GridPlacement grid_column;

    // Named grid lines after the last track
    // Example: [sidebar-start] 200px [sidebar-end] -> trailing = {"sidebar-end"}
    std::vector<std::string> grid_row_trailing_names;
    std::vector<std::string> grid_column_trailing_names;

    // Grid template areas (container property)
    // Maps area name -> GridAreaDef for "grid-template-areas" CSS property
    // Example: "header header" "sidebar main" "footer footer"
    std::unordered_map<std::string, GridAreaDef> grid_template_areas;

    // Grid area name (item property)
    // The named area this item should be placed in (from "grid-area" CSS property)
    std::string grid_area;

    // Grid/Flex item alignment
    JustifyItems justify_items = JustifyItems::STRETCH;  // Container default
    AlignSelf align_self = AlignSelf::AUTO;              // Item alignment (cross-axis)
    JustifySelf justify_self = JustifySelf::AUTO;        // Item alignment (main-axis)

    // === Text ===
    Color color = nvgRGBA(0, 0, 0, 255);
    float font_size = 16.0f;  // Always in px after resolution
    FontWeight font_weight = FontWeight::NORMAL;
    FontStyle font_style = FontStyle::NORMAL;
    TextAlign text_align = TextAlign::LEFT;
    VerticalAlign vertical_align = VerticalAlign::TOP;  // Retain mode: O(1) lookup
    std::string font_family = "sans-serif";  // Keep string for NanoVG API
    TextDecoration text_decoration = TextDecoration::NONE;
    TextTransform text_transform = TextTransform::NONE;
    float letter_spacing = 0.0f;  // Extra space between characters (px)
    float word_spacing = 0.0f;    // Extra space between words (px)
    std::vector<TextShadow> text_shadows;  // CSS text-shadow

    // === Pseudo-element content (::before/::after) ===
    std::string content;          // CSS content property value
    bool has_content = false;     // True if content property is set (even if empty string)

    // === SVG Properties ===
    SVGFill svg_fill;
    SVGStroke svg_stroke;

    // === Filter Effects ===
    std::vector<struct FilterEffect> filters;

    // === Transform ===
    Transform transform;

    // === Transition Properties ===
    std::vector<struct Transition> transitions;

    // === Animation Properties ===
    std::vector<struct Animation> animations;
};

// ============================================================================
// CSS Transitions
// ============================================================================

/**
 * @brief CSS timing function for transitions and animations
 *
 * All timing functions are ultimately cubic-bezier curves.
 * Named functions are pre-defined cubic-bezier values.
 */
struct TimingFunction {
    enum class Type : uint8_t {
        LINEAR,       // cubic-bezier(0, 0, 1, 1)
        EASE,         // cubic-bezier(0.25, 0.1, 0.25, 1.0) - default
        EASE_IN,      // cubic-bezier(0.42, 0, 1.0, 1.0)
        EASE_OUT,     // cubic-bezier(0, 0, 0.58, 1.0)
        EASE_IN_OUT,  // cubic-bezier(0.42, 0, 0.58, 1.0)
        CUBIC_BEZIER, // Custom cubic-bezier(x1, y1, x2, y2)
        STEPS,        // steps(n, start|end)
    };

    Type type = Type::EASE;

    // Cubic bezier control points (x1, y1, x2, y2)
    // Only used when type == CUBIC_BEZIER
    float x1 = 0.25f, y1 = 0.1f, x2 = 0.25f, y2 = 1.0f;

    // Steps parameters (only used when type == STEPS)
    int steps = 1;
    bool step_start = false;  // true = step-start, false = step-end

    // Named constructors
    static TimingFunction linear() { return {Type::LINEAR, 0, 0, 1, 1, 1, false}; }
    static TimingFunction ease() { return {Type::EASE, 0.25f, 0.1f, 0.25f, 1.0f, 1, false}; }
    static TimingFunction ease_in() { return {Type::EASE_IN, 0.42f, 0, 1.0f, 1.0f, 1, false}; }
    static TimingFunction ease_out() { return {Type::EASE_OUT, 0, 0, 0.58f, 1.0f, 1, false}; }
    static TimingFunction ease_in_out() { return {Type::EASE_IN_OUT, 0.42f, 0, 0.58f, 1.0f, 1, false}; }
    static TimingFunction cubic_bezier(float x1, float y1, float x2, float y2) {
        return {Type::CUBIC_BEZIER, x1, y1, x2, y2, 1, false};
    }
    static TimingFunction step(int n, bool start = false) {
        return {Type::STEPS, 0, 0, 1, 1, n, start};
    }

    /**
     * @brief Evaluate the timing function at time t (0 to 1)
     * @param t Progress through the transition (0 = start, 1 = end)
     * @return Eased value (usually 0 to 1, but can overshoot for some curves)
     */
    float evaluate(float t) const {
        if (t <= 0) return 0;
        if (t >= 1) return 1;

        switch (type) {
            case Type::LINEAR:
                return t;

            case Type::STEPS: {
                if (step_start) {
                    return std::ceil(t * steps) / steps;
                } else {
                    return std::floor(t * steps) / steps;
                }
            }

            case Type::EASE:
            case Type::EASE_IN:
            case Type::EASE_OUT:
            case Type::EASE_IN_OUT:
            case Type::CUBIC_BEZIER:
            default:
                return evaluate_cubic_bezier(t);
        }
    }

private:
    // Solve cubic bezier using Newton-Raphson method
    float evaluate_cubic_bezier(float t) const {
        // Get the actual control points based on type
        float cx1 = x1, cy1 = y1, cx2 = x2, cy2 = y2;

        switch (type) {
            case Type::EASE:
                cx1 = 0.25f; cy1 = 0.1f; cx2 = 0.25f; cy2 = 1.0f;
                break;
            case Type::EASE_IN:
                cx1 = 0.42f; cy1 = 0.0f; cx2 = 1.0f; cy2 = 1.0f;
                break;
            case Type::EASE_OUT:
                cx1 = 0.0f; cy1 = 0.0f; cx2 = 0.58f; cy2 = 1.0f;
                break;
            case Type::EASE_IN_OUT:
                cx1 = 0.42f; cy1 = 0.0f; cx2 = 0.58f; cy2 = 1.0f;
                break;
            default:
                break;
        }

        // Newton-Raphson to find parameter s where bezier_x(s) = t
        float s = t;  // Initial guess
        for (int i = 0; i < 8; i++) {
            float x = bezier(s, cx1, cx2) - t;
            if (std::abs(x) < 0.0001f) break;
            float dx = bezier_derivative(s, cx1, cx2);
            if (std::abs(dx) < 0.0001f) break;
            s -= x / dx;
        }

        // Clamp s to [0, 1]
        s = std::max(0.0f, std::min(1.0f, s));

        // Return y value at parameter s
        return bezier(s, cy1, cy2);
    }

    // Cubic bezier: B(t) = 3(1-t)²t·P1 + 3(1-t)t²·P2 + t³
    static float bezier(float t, float p1, float p2) {
        float t2 = t * t;
        float t3 = t2 * t;
        float mt = 1.0f - t;
        float mt2 = mt * mt;
        return 3.0f * mt2 * t * p1 + 3.0f * mt * t2 * p2 + t3;
    }

    // Derivative: B'(t) = 3(1-t)²·P1 + 6(1-t)t·(P2-P1) + 3t²·(1-P2)
    static float bezier_derivative(float t, float p1, float p2) {
        float mt = 1.0f - t;
        return 3.0f * mt * mt * p1 + 6.0f * mt * t * (p2 - p1) + 3.0f * t * t * (1.0f - p2);
    }
};

/**
 * @brief CSS Transition Property Identifier
 *
 * Used to identify which property a transition applies to.
 * "all" matches any animatable property.
 */
enum class TransitionProperty : uint16_t {
    NONE,
    ALL,              // transition: all

    // Layout
    WIDTH,
    HEIGHT,
    MIN_WIDTH,
    MIN_HEIGHT,
    MAX_WIDTH,
    MAX_HEIGHT,
    PADDING,
    PADDING_TOP,
    PADDING_RIGHT,
    PADDING_BOTTOM,
    PADDING_LEFT,
    MARGIN,
    MARGIN_TOP,
    MARGIN_RIGHT,
    MARGIN_BOTTOM,
    MARGIN_LEFT,

    // Position
    TOP,
    RIGHT,
    BOTTOM,
    LEFT,

    // Visual
    OPACITY,
    BACKGROUND_COLOR,
    COLOR,
    BORDER_COLOR,
    BORDER_WIDTH,
    BORDER_RADIUS,

    // Transform
    TRANSFORM,

    // Flex
    FLEX_GROW,
    FLEX_SHRINK,
    GAP,

    // SVG
    FILL,
    STROKE,
    STROKE_WIDTH,

    // Filters
    FILTER,
};

/**
 * @brief A single CSS transition definition
 *
 * Represents: transition: property duration timing-function delay
 * Example: transition: opacity 0.3s ease-in-out 0.1s
 */
struct Transition {
    TransitionProperty property = TransitionProperty::ALL;
    float duration = 0.0f;          // seconds
    float delay = 0.0f;             // seconds
    TimingFunction timing;

    Transition() = default;
    Transition(TransitionProperty prop, float dur, TimingFunction tf = TimingFunction::ease(), float del = 0.0f)
        : property(prop), duration(dur), delay(del), timing(tf) {}

    // Convenience constructors
    static Transition all(float duration, TimingFunction tf = TimingFunction::ease()) {
        return {TransitionProperty::ALL, duration, tf, 0.0f};
    }

    static Transition opacity(float duration, TimingFunction tf = TimingFunction::ease()) {
        return {TransitionProperty::OPACITY, duration, tf, 0.0f};
    }

    static Transition transform(float duration, TimingFunction tf = TimingFunction::ease()) {
        return {TransitionProperty::TRANSFORM, duration, tf, 0.0f};
    }

    static Transition background(float duration, TimingFunction tf = TimingFunction::ease()) {
        return {TransitionProperty::BACKGROUND_COLOR, duration, tf, 0.0f};
    }
};

// ============================================================================
// CSS Animations (@keyframes)
// ============================================================================

/**
 * @brief Animation direction for animation-direction property
 */
enum class AnimationDirection : uint8_t {
    NORMAL,           // 0% -> 100%
    REVERSE,          // 100% -> 0%
    ALTERNATE,        // 0% -> 100% -> 0% -> ...
    ALTERNATE_REVERSE // 100% -> 0% -> 100% -> ...
};

/**
 * @brief Animation fill mode for animation-fill-mode property
 */
enum class AnimationFillMode : uint8_t {
    NONE,      // No styles applied outside animation
    FORWARDS,  // Retain end state after animation
    BACKWARDS, // Apply start state during delay
    BOTH       // Both forwards and backwards
};

/**
 * @brief Animation play state
 */
enum class AnimationPlayState : uint8_t {
    RUNNING,
    PAUSED
};

/**
 * @brief A single keyframe in an animation
 *
 * Represents a point in time (0-100%) with associated style values.
 * Multiple properties can be animated at each keyframe.
 */
struct Keyframe {
    float percentage;  // 0.0 to 1.0 (0% to 100%)

    // Animated property values at this keyframe
    // Using optional to indicate which properties are set
    std::optional<float> opacity;
    std::optional<Color> background_color;
    std::optional<Color> color;
    std::optional<Transform> transform;
    std::optional<Length> width;
    std::optional<Length> height;
    std::optional<std::array<Length, 4>> padding;
    std::optional<std::array<Length, 4>> margin;
    std::optional<std::array<float, 4>> border_radius;
    std::optional<Color> border_color;
    std::optional<float> border_width;

    Keyframe(float pct = 0.0f) : percentage(pct) {}

    // Convenience constructors
    static Keyframe at(float pct) { return Keyframe(pct); }
    static Keyframe from() { return Keyframe(0.0f); }
    static Keyframe to() { return Keyframe(1.0f); }

    // Fluent API for setting properties
    Keyframe& with_opacity(float v) { opacity = v; return *this; }
    Keyframe& with_background(Color c) { background_color = c; return *this; }
    Keyframe& with_color(Color c) { color = c; return *this; }
    Keyframe& with_transform(Transform t) { transform = t; return *this; }
};

/**
 * @brief A named keyframes animation definition
 *
 * Represents @keyframes rule:
 * @keyframes fadeIn {
 *   0% { opacity: 0; }
 *   100% { opacity: 1; }
 * }
 */
struct KeyframesDefinition {
    std::string name;
    std::vector<Keyframe> keyframes;

    KeyframesDefinition() = default;
    KeyframesDefinition(const std::string& n) : name(n) {}

    // Add a keyframe (maintains sorted order by percentage)
    void add_keyframe(const Keyframe& kf) {
        auto it = std::lower_bound(keyframes.begin(), keyframes.end(), kf,
            [](const Keyframe& a, const Keyframe& b) {
                return a.percentage < b.percentage;
            });
        keyframes.insert(it, kf);
    }

    // Get keyframes surrounding a given progress (0-1)
    std::pair<const Keyframe*, const Keyframe*> get_surrounding(float progress) const {
        if (keyframes.empty()) return {nullptr, nullptr};
        if (keyframes.size() == 1) return {&keyframes[0], &keyframes[0]};

        for (size_t i = 0; i < keyframes.size() - 1; i++) {
            if (progress >= keyframes[i].percentage && progress <= keyframes[i + 1].percentage) {
                return {&keyframes[i], &keyframes[i + 1]};
            }
        }

        // Edge cases
        if (progress <= keyframes.front().percentage) {
            return {&keyframes.front(), &keyframes.front()};
        }
        return {&keyframes.back(), &keyframes.back()};
    }
};

/**
 * @brief CSS animation property definition
 *
 * Represents animation shorthand:
 * animation: fadeIn 0.5s ease-in-out 0.1s infinite alternate forwards;
 */
struct Animation {
    std::string name;                                    // animation-name (references @keyframes)
    float duration = 0.0f;                               // animation-duration (seconds)
    TimingFunction timing;                               // animation-timing-function
    float delay = 0.0f;                                  // animation-delay (seconds)
    float iteration_count = 1.0f;                        // animation-iteration-count (INFINITY for infinite)
    AnimationDirection direction = AnimationDirection::NORMAL;
    AnimationFillMode fill_mode = AnimationFillMode::NONE;
    AnimationPlayState play_state = AnimationPlayState::RUNNING;

    Animation() = default;
    Animation(const std::string& n, float dur, TimingFunction tf = TimingFunction::ease())
        : name(n), duration(dur), timing(tf) {}

    // Check if animation loops forever
    bool is_infinite() const {
        return iteration_count == std::numeric_limits<float>::infinity();
    }

    // Convenience constructors
    static Animation create(const std::string& name, float duration) {
        return Animation(name, duration);
    }

    static Animation infinite(const std::string& name, float duration) {
        Animation a(name, duration);
        a.iteration_count = std::numeric_limits<float>::infinity();
        return a;
    }
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
    float content_width, content_height;  // Inner content size (for scrollable containers)

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

} // namespace cssbox

#endif // cssbox_TYPES_H
