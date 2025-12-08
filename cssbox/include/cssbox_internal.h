/*
 * NanoVG CSS - Internal header
 *
 * Internal structures and classes for NanoVG CSS implementation.
 */

#ifndef cssbox_INTERNAL_H
#define cssbox_INTERNAL_H

// Windows min/max macro conflict fix
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <nanovg.h>
#include <cssbox.h>
#include <cssbox_types.h>  // NEW: Typed property system
#include <cssbox_filters.h>  // Filter context type
#include <cssbox_memory.h>  // Memory pool system
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include "cssbox_background.h"
#include "lexbor_css_parser.h"


// Windows macro cleanup (wingdi.h defines RELATIVE/ABSOLUTE)
#ifdef RELATIVE
#undef RELATIVE
#endif
#ifdef ABSOLUTE
#undef ABSOLUTE
#endif
// ============================================================================
// Forward Declarations
// ============================================================================

class cssboxPainter;
struct GradientData;  // Forward declaration for gradient cache

// ============================================================================
// Element Structure Types
// ============================================================================

 /**
 * @brief Simple 2D point for stroke geometry
 */
struct cssboxPoint {
    float x, y;
    cssboxPoint() : x(0.0f), y(0.0f) {}
    cssboxPoint(float px, float py) : x(px), y(py) {}
};

/**
 * @brief Line geometry
 */
struct cssboxLineGeometry {
    float x1, y1, x2, y2;
    bool defined;
    cssboxLineGeometry() : x1(0), y1(0), x2(0), y2(0), defined(false) {}
};

/**
 * @brief Circle/ellipse geometry
 */
struct cssboxCircleGeometry {
    float cx, cy, rx, ry;
    bool defined;
    cssboxCircleGeometry() : cx(0), cy(0), rx(0), ry(0), defined(false) {}
};

/**
 * @brief Rect geometry (retain mode: O(1) instead of inline_style lookup)
 */
struct cssboxRectGeometry {
    float x, y, width, height, rx, ry;
    bool defined;
    cssboxRectGeometry() : x(0), y(0), width(0), height(0), rx(0), ry(0), defined(false) {}
};

/**
 * @brief SVG Marker definition
 *
 * Markers are reusable graphical elements that can be placed at:
 * - marker-start: Beginning of a path
 * - marker-mid: Each vertex along a path  
 * - marker-end: End of a path
 */
struct cssboxMarker {
    std::string id;
    float markerWidth = 3.0f;
    float markerHeight = 3.0f;
    float refX = 0.0f;
    float refY = 0.0f;
    std::string orient = "0";  // "auto", "auto-start-reverse", or angle in degrees
    std::vector<int> children_internal_ids;  // Marker content (element internal IDs)
    
    cssboxMarker() = default;
};

/**
 * @brief SVG ClipPath definition
 *
 * Clip paths define a clipping region that determines what parts of an element are visible.
 * Content outside the clipping path is not rendered.
 */
struct cssboxClipPath {
    std::string id;
    std::vector<int> children_internal_ids;  // Shapes defining the clip region
    
    cssboxClipPath() = default;
};

/**
 * @brief SVG Pattern definition
 *
 * Patterns define repeating graphical content for fills and strokes.
 * Rendered to offscreen FBO and used as tiled image pattern.
 */
struct cssboxPattern {
    std::string id;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    std::string patternUnits = "objectBoundingBox";  // or "userSpaceOnUse"
    std::string patternContentUnits = "userSpaceOnUse";
    std::vector<int> children_internal_ids;  // Pattern content elements
    
    // FBO rendering cache
    int image_handle = -1;     // NanoVG image handle
    unsigned int fbo = 0;      // OpenGL framebuffer object
    unsigned int texture = 0;  // OpenGL texture
    bool needs_update = true;  // Dirty flag for re-rendering
    
    cssboxPattern() = default;
};

/**
 * @brief CSS Element (full internal definition)
 *
 * REFACTORED for 60fps:
 * - Typed properties instead of string maps
 * - Dirty flags for incremental updates
 * - No more void* pointers
 */
struct cssboxElement {
    // === Identity ===
    /**
     * @brief Unique internal identifier (auto-assigned, always unique)
     *
     * This is the PRIMARY KEY for element storage and tree relationships.
     * - Auto-incremented by renderer (starts at 1)
     * - Used for parent-child relationships (parent_internal_id, children_internal_ids)
     * - Guarantees uniqueness even when user doesn't provide an id
     *
     * DO NOT confuse with 'id' field below!
     */
    int internal_id;

    /**
     * @brief User-provided id (optional, for CSS #id selector)
     *
     * This is OPTIONAL and used ONLY for:
     * - CSS #id selector matching
     * - User-facing API (cssboxGetElement by id)
     *
     * Can be empty! Elements without user id still work via internal_id.
     */
    std::string id;

    std::string type;        // Element type (div, button, etc.)
    std::vector<std::string> classes;
    std::map<std::string, std::string> attributes;
    std::set<std::string> pseudo_states;

    // === NEW: Typed CSS properties (replaces string maps) ===
    cssbox::ComputedStyle style;         // Parsed CSS properties
    cssbox::ResolvedLayout layout;       // Resolved layout (in pixels)

    // === DEPRECATED: Old string-based system (will be removed) ===
    std::map<std::string, std::string> inline_style;  // DEPRECATED - still used by SVG XML parser

    // === NEW: Dirty flags for 60fps optimization ===
    cssbox::DirtyFlags dirty_flags = cssbox::DIRTY_ALL;

    // === Transform & Visibility ===
    float transform[6];
    float opacity;
    bool visible;

    // === Retain Mode: Cached Absolute Transform ===
    float cached_abs_transform_[6] = {1, 0, 0, 1, 0, 0};  // Identity
    bool abs_transform_valid_ = false;

    // === Scroll Offset (for overflow: scroll/auto containers) ===
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
    float content_height = 0.0f;  // Total content height (for scrollbar calculation)

    // === Tree Structure ===
    /**
     * @brief Parent's internal_id (-1 if root element)
     *
     * Tree relationships use internal_id (NOT user id) for reliability.
     * This allows elements without user id to still have parent-child relationships.
     */
    int parent_internal_id;

    /**
     * @brief Children's internal_ids
     *
     * Stores child elements by their internal_id for fast, reliable lookup.
     */
    std::vector<int> children_internal_ids;

    int child_index, total_siblings;  // Position among siblings

    // === Content ===
    std::string text_content;

    // === State pointers (Phase 2: will replace with std::unique_ptr<TransitionState>) ===
    void* transition_state;
    void* animation_state;

    // === DEPRECATED: Old shadow storage (kept for backward compat during refactor) ===
    std::vector<BoxShadow> box_shadows;      // DEPRECATED: will move to style.box_shadows
    std::vector<TextShadow> text_shadows;    // DEPRECATED: will move to typed system

    // === Geometry (for custom shapes) ===
    cssboxLineGeometry line_geometry;
    cssboxCircleGeometry circle_geometry;
    cssboxRectGeometry rect_geometry;
    std::vector<cssboxPoint> stroke_points;
    float stroke_salt;
    bool has_stroke_salt;

    // === Custom Rendering ===
    void (*custom_paint)(NVGcontext*, const cssboxElement*, const std::map<std::string, std::string>&);
    void* user_data;

    // === Gradient Cache (Performance Optimization) ===
    // Cache resolved gradient pointers to avoid repeated map lookups every frame
    const GradientData* cached_fill_gradient = nullptr;
    const GradientData* cached_stroke_gradient = nullptr;
    
    // === SVG Path Cache (Avoid re-parsing every frame) ===
    mutable std::string cached_path_d;           // Last parsed 'd' attribute value
    mutable void* cached_path_commands = nullptr; // std::vector<PathCommand>* - opaque to avoid header dep
    mutable std::string cached_points_str;       // Last parsed 'points' attribute
    mutable std::vector<std::pair<float,float>> cached_points;  // Parsed polygon points
    
    // Destructor to clean up opaque cache pointers
    ~cssboxElement();
};

// ============================================================================
// Gradient Data Structures (Phase 3)
// ============================================================================

/**
 * @brief Gradient color stop
 */
struct GradientStop {
    float position;    // 0.0 to 1.0
    NVGcolor color;
};

/**
 * @brief Complete gradient data
 */
struct GradientData {
    enum Type { LINEAR, RADIAL } type;
    std::vector<GradientStop> stops;

    // Linear gradient data
    float angle = 180.0f;  // Default: to bottom

    // Radial gradient data
    std::string shape = "ellipse";  // circle or ellipse
    std::string position = "center center";  // e.g., "at top left", "at 50% 50%"

    GradientData() : type(LINEAR) {}
};

// ============================================================================
// Visual Effects (Sprint 27)
// ============================================================================

// Note: BoxShadow struct is now defined in cssbox.h (public header)
// since it's used in cssboxElement which is part of the public API

// ============================================================================
// Animation & Transition System (Phase 4)
// ============================================================================

/**
 * @brief Easing function type
 */
enum class EasingFunction {
    LINEAR,
    EASE,
    EASE_IN,
    EASE_OUT,
    EASE_IN_OUT,
    CUBIC_BEZIER  // Custom cubic-bezier
};

/**
 * @brief Single property transition
 */
struct Transition {
    std::string property;       // Property being transitioned
    float start_time;           // When transition started
    float duration;             // Transition duration in seconds
    EasingFunction easing;      // Easing function
    std::string start_value;    // Starting value
    std::string end_value;      // Target value
    bool active;                // Is this transition running?

    // Custom cubic-bezier control points (only used when easing == CUBIC_BEZIER)
    float bezier_x1 = 0.0f, bezier_y1 = 0.0f;
    float bezier_x2 = 1.0f, bezier_y2 = 1.0f;

    Transition() : start_time(0), duration(0), easing(EasingFunction::LINEAR), active(false) {}
};

/**
 * @brief Element transition state
 */
struct TransitionState {
    std::map<std::string, Transition> active_transitions;  // Active transitions by property name
    std::map<std::string, std::string> previous_values;    // Previous computed values

    // Transition specification from CSS
    std::string transition_property;  // "all" or comma-separated properties
    float transition_duration;        // Duration in seconds
    EasingFunction transition_easing; // Easing function

    // Custom cubic-bezier control points (when transition_easing == CUBIC_BEZIER)
    float bezier_x1 = 0.0f, bezier_y1 = 0.0f;
    float bezier_x2 = 1.0f, bezier_y2 = 1.0f;

    TransitionState() : transition_property("none"), transition_duration(0), transition_easing(EasingFunction::EASE) {}
};

/**
 * @brief Single keyframe in an animation
 */
struct Keyframe {
    float position;  // 0.0 to 1.0 (0% to 100%)
    std::map<std::string, std::string> properties;  // CSS properties at this keyframe

    Keyframe() : position(0.0f) {}
    Keyframe(float pos) : position(pos) {}
};

/**
 * @brief Complete keyframe animation definition (@keyframes)
 */
struct KeyframeAnimation {
    std::string name;                    // Animation name
    std::vector<Keyframe> keyframes;     // Keyframes in order

    KeyframeAnimation() = default;
    KeyframeAnimation(const std::string& n) : name(n) {}

    // Get interpolated properties at a specific position [0, 1]
    std::map<std::string, std::string> get_properties_at(float position) const;
    
    // Get all property names that are animated by this animation
    std::set<std::string> get_animated_properties() const {
        std::set<std::string> props;
        for (const auto& kf : keyframes) {
            for (const auto& [prop, value] : kf.properties) {
                props.insert(prop);
            }
        }
        return props;
    }
};

/**
 * @brief Single running animation instance
 */
struct RunningAnimation {
    std::string animation_name;       // Name of keyframe animation
    float start_time;                 // When animation started
    float duration;                   // Animation duration in seconds
    int iteration_count;              // Number of iterations (-1 = infinite)
    EasingFunction easing;            // Timing function
    std::string direction;            // normal, reverse, alternate, alternate-reverse
    float delay;                      // Animation delay in seconds
    std::string fill_mode;            // none, forwards, backwards, both

    // Runtime state
    int current_iteration;            // Current iteration number
    bool active;                      // Is this animation running?

    RunningAnimation() : start_time(0), duration(0), iteration_count(1),
                        easing(EasingFunction::EASE), direction("normal"),
                        delay(0), fill_mode("none"), current_iteration(0), active(false) {}
};

/**
 * @brief Element animation state
 */
struct AnimationState {
    std::vector<RunningAnimation> running_animations;  // Active animations on this element

    AnimationState() = default;
};

// ============================================================================
// Flexbox Layout System (Phase 4 Sprint 3)
// ============================================================================

/**
 * @brief Flex item information for layout
 */
struct FlexItem {
    cssboxElement* element;           // The element being laid out

    // CSS properties
    float flex_grow;                  // Growth factor (default: 0)
    float flex_shrink;                // Shrink factor (default: 1)
    float flex_basis;                 // Base size (default: auto = -1)
    std::string align_self;           // Override container's align-items
    int order;                        // Display order (default: 0)

    // Computed during layout
    float main_size;                  // Size on main axis
    float cross_size;                 // Size on cross axis
    float main_position;              // Position on main axis
    float cross_position;             // Position on cross axis
    float hypothetical_main_size;     // Size before flex
    float scaled_flex_shrink;         // For flex-shrink calculation
    bool frozen;                      // True if size is finalized

    FlexItem() : element(nullptr), flex_grow(0), flex_shrink(1), flex_basis(-1),
                 align_self("auto"), order(0), main_size(0), cross_size(0),
                 main_position(0), cross_position(0), hypothetical_main_size(0),
                 scaled_flex_shrink(0), frozen(false) {}
};

/**
 * @brief Flex line (for wrapping)
 */
struct FlexLine {
    std::vector<FlexItem> items;      // Items in this line
    float main_size;                  // Total size on main axis
    float cross_size;                 // Size on cross axis
    float cross_position;             // Position on cross axis
    float remaining_space;            // Free space on main axis

    FlexLine() : main_size(0), cross_size(0), cross_position(0), remaining_space(0) {}
};

/**
 * @brief Flexbox container properties
 */
struct FlexContainer {
    // CSS properties
    std::string direction;            // row, column, row-reverse, column-reverse
    std::string wrap;                 // nowrap, wrap, wrap-reverse
    std::string justify_content;      // flex-start, flex-end, center, space-between, etc.
    std::string align_items;          // flex-start, flex-end, center, stretch, baseline
    std::string align_content;        // flex-start, flex-end, center, stretch, etc.
    float gap;                        // Spacing between items

    // Computed
    bool is_horizontal;               // true for row, false for column
    bool is_reverse;                  // true for *-reverse directions

    FlexContainer() : direction("row"), wrap("nowrap"), justify_content("flex-start"),
                     align_items("stretch"), align_content("flex-start"), gap(0),
                     is_horizontal(true), is_reverse(false) {}
};

// ============================================================================
// Simple CSS Stylesheet
// ============================================================================

/**
 * @brief Simple CSS stylesheet for Phase 1
 *
 * Stores CSS rules and computes styles by matching selectors.
 * Phase 1: Simple selector matching only (class, type, id).
 */
class SimpleStyleSheet {
public:
    SimpleStyleSheet() = default;

    /**
     * @brief Parse CSS string and extract rules
     */
    bool parse_css(const std::string& css);

    /**
     * @brief Add a single CSS rule
     */
    void add_rule(const std::string& selector,
                  const std::map<std::string, std::string>& properties);

    /**
     * @brief Compute style for an element
     */
    std::map<std::string, std::string> compute_style(
        const std::string& id,
        const std::string& type,
        const std::vector<std::string>& classes,
        const std::map<std::string, std::string>& attributes,
        const std::set<std::string>& pseudo_states,
        const std::map<std::string, std::string>& inline_style,
        const std::map<std::string, std::string>& parent_style = {},
        int child_index = 0,
        int total_siblings = 1);

    /**
     * @brief Set CSS variable
     */
    void set_variable(const std::string& name, const std::string& value);

    /**
     * @brief Add keyframe animation
     */
    void add_keyframe_animation(const KeyframeAnimation& animation);

    /**
     * @brief Get keyframe animation by name
     */
    const KeyframeAnimation* get_keyframe_animation(const std::string& name) const;

private:
    struct CSSRule {
        std::string selector;
        std::map<std::string, std::string> properties;
        int specificity;
    };

    std::vector<CSSRule> rules_;
    std::map<std::string, std::string> variables_;
    std::map<std::string, KeyframeAnimation> keyframe_animations_;  // Phase 4 Sprint 2

    // Simple selector matching (Phase 1)
    bool matches_selector(const std::string& selector,
                         const std::string& id,
                         const std::string& type,
                         const std::vector<std::string>& classes,
                         const std::set<std::string>& pseudo_states,
                         int child_index = 0,
                         int total_siblings = 1) const;

    int calculate_specificity(const std::string& selector) const;

    std::string resolve_variables(const std::string& value) const;
};

// ============================================================================
// Internal Structures
// ============================================================================

/**
 * @brief Internal renderer state
 */
struct cssboxRenderer {
    NVGcontext* vg;

    // Memory management (high-performance arena allocator)
    cssbox::MemoryArena arena_;
    cssbox::ObjectPool<TransitionState> transition_pool_;
    cssbox::ObjectPool<AnimationState> animation_pool_;
    size_t frame_mark_ = 0;

    // Stylesheet management (using lexbor-powered EnhancedStyleSheet)
    std::unique_ptr<cssbox::lexbor::EnhancedStyleSheet> stylesheet;

    // === Element Tree (Internal ID System) ===
    /**
     * @brief Auto-incrementing ID counter (0 reserved for invalid)
     *
     * Every element gets a unique internal_id from this counter.
     * Starts at 1, increments on each cssboxCreateElement() call.
     */
    int next_internal_id = 1;

    /**
     * @brief Primary element storage: internal_id -> element
     *
     * This is the AUTHORITATIVE storage for all elements.
     * Key = internal_id (always unique, never empty, never conflicts)
     */
    std::map<int, std::unique_ptr<cssboxElement>> elements;

    /**
     * @brief User id lookup table: user id -> internal_id
     *
     * Optional mapping for CSS #id selector support.
     * Only contains entries for elements with non-empty user id.
     */
    std::map<std::string, int> id_to_internal_id;

    std::vector<cssboxElement*> root_elements;  // Top-level elements

    // Rendering components
    std::unique_ptr<cssboxPainter> painter;

    // Viewport
    float viewport_width = 800.0f;
    float viewport_height = 600.0f;

    // Time management (Phase 4)
    float current_time = 0.0f;  // Current time in seconds

    // Dirty flags
    bool layout_dirty = true;
    bool style_dirty = true;

    // === Retain Mode: Render List Cache ===
    std::vector<cssboxElement*> render_list_;  // Sorted by z-index, rebuilt only when dirty
    bool render_list_dirty_ = true;

    // === Retain Mode: Paint Skip ===
    // When nothing changed, skip entire paint loop (massive GPU savings)
    bool paint_dirty_ = true;

    // === Retain Mode: Animated Elements Tracking ===
    std::unordered_set<int> animated_elements_;  // internal_ids of elements with active animations

    // Image cache (Sprint 31)
    std::unordered_map<std::string, int> image_cache;  // path -> NanoVG image handle

    // Marker registry (SVG markers)
    std::unordered_map<std::string, cssboxMarker> markers_;  // id -> marker definition
    
    // ClipPath registry (SVG clipping paths)
    std::unordered_map<std::string, cssboxClipPath> clip_paths_;  // id -> clip path definition
    
    // Pattern registry (SVG patterns)
    std::unordered_map<std::string, cssboxPattern> patterns_;  // id -> pattern definition
    
    // Gradient registry (SVG gradients)
    std::unordered_map<std::string, GradientData> gradients_;  // id -> gradient definition

    // === Retain Mode: CSS Gradient Cache ===
    // Parsed gradient data for CSS gradient strings (avoid re-parsing every frame)
    std::unordered_map<std::string, GradientData> css_gradient_cache_;

    // CSS file tracking (v2 API: for hot-reload)
    std::vector<std::string> css_files;  // Loaded CSS file paths

    cssboxRenderer(NVGcontext* vg);
    ~cssboxRenderer();
};

// ============================================================================
// cssboxPainter - Translates CSS properties to NanoVG calls
// ============================================================================

class cssboxPainter {
public:
    cssboxPainter(NVGcontext* vg, cssboxRenderer* renderer);
    ~cssboxPainter();

    /**
     * @brief Paint an element (uses element->style typed properties)
     */
    void paint_element(const cssboxElement* element);

    /**
     * @brief Apply filter effects to element
     */
    void apply_filters(const cssboxElement* element, float& opacity);

    /**
     * @brief Render element with OpenGL filters applied
     */
    void render_with_filters(const cssboxElement* element, 
                            const cssboxBox& box,
                            std::function<void()> render_fn);

    /**
     * @brief Get NanoVG context (Sprint 22: for scissor clipping)
     */
    NVGcontext* get_context() { return vg_; }

    /**
     * @brief Parse gradient CSS (Sprint 29: exposed for testing)
     */
    GradientData parse_gradient(const std::string& gradient_css);

    /**
     * @brief Pre-render dirty patterns to FBO (must be called before nvgBeginFrame)
     *
     * This allows patterns to use their own nvgBeginFrame/nvgEndFrame without nesting.
     */
    void prepare_patterns();

    struct StrokeStyle {
        NVGcolor color;
        float width;
        int line_cap;
        int line_join;
        float miter_limit;
        std::vector<float> dash_array;
        float dash_offset;
        bool enabled;

        StrokeStyle()
            : color(nvgRGBA(0, 0, 0, 255)),
              width(1.0f),
              line_cap(NVG_BUTT),
              line_join(NVG_MITER),
              miter_limit(10.0f),
              dash_offset(0.0f),
              enabled(false) {}
    };

private:
    NVGcontext* vg_;
    cssboxRenderer* renderer_;  // Sprint 31: for image cache access
    cssboxFilterContext* filter_context_;  // OpenGL filter context

    // Property handlers (using typed properties from element->style)
    void apply_background(const cssboxElement* element,
                         const cssboxBox& box);

    // Sprint 27: Box shadow rendering
    void paint_box_shadows(const cssboxElement* element, const cssboxBox& box);

    // Gradient support
    NVGpaint create_gradient(const std::string& gradient_css,
                            const cssboxBox& box);

    // Phase 3: Enhanced gradient creation
    NVGpaint create_linear_gradient(const GradientData& gradient, const cssboxBox& box);
    NVGpaint create_radial_gradient(const GradientData& gradient, const cssboxBox& box);

    // Path generation
    void create_rounded_rect_path(const cssboxBox& box);

    // Typography support (Sprint 11) - using typed properties
    std::string compute_font_face(const cssboxElement* element);
    int compute_text_align(const cssboxElement* element);
    std::string apply_text_transform(const std::string& text, const std::string& transform);
    void apply_text_decoration(const cssboxElement* element,
                              float text_x, float text_y,
                              NVGcolor text_color);
    void paint_text_content(const cssboxElement* element,
                           float content_x, float content_y,
                           float content_width, float content_height);

    // Vector shape helpers - using typed/inline properties
    StrokeStyle resolve_stroke_style(const cssboxElement* element,
                                     const cssboxBox* box,
                                     float absolute_hint = 1.0f) const;
    void paint_rect_stroke(const cssboxElement* element,
                           const cssboxBox& box);
    void paint_line_shape(const cssboxElement* element,
                          const cssboxBox& box);
    void paint_circle_shape(const cssboxElement* element,
                            const cssboxBox& box);
    void paint_rect_shape(const cssboxElement* element,
                          const cssboxBox& box);
    void paint_svg_path(const cssboxElement* element,
                        const cssboxBox& box);
    void paint_polygon(const cssboxElement* element,
                       const cssboxBox& box);
    void paint_polyline(const cssboxElement* element,
                        const cssboxBox& box);
    void paint_freehand_path(const cssboxElement* element);
    
    // SVG Marker rendering
    void render_marker(const cssboxMarker& marker, float x, float y, float angle);
    
    // SVG Text-on-Path rendering
    void paint_text_path(const cssboxElement* element, const cssboxBox& box);
    
    // SVG Clipping
    void begin_clip_path(const cssboxClipPath& clip_path);
    void end_clip_path();
    
    // SVG Patterns
    int render_pattern_to_fbo(cssboxPattern& pattern);
    void cleanup_pattern_fbo(cssboxPattern& pattern);
    bool apply_pattern_fill(const std::string& fill_value, const cssboxBox& box);
};

// ============================================================================
// Keyframe Animation Utilities (Phase 4 Sprint 2)
// ============================================================================

/**
 * @brief Parse @keyframes rule from CSS
 * @param keyframes_css CSS text for keyframes (content between @keyframes name { and })
 * @param name Animation name
 * @return Parsed KeyframeAnimation
 */
KeyframeAnimation parse_keyframes_rule(const std::string& keyframes_css, const std::string& name);

/**
 * @brief Parse animation properties from computed style
 * Creates a RunningAnimation from CSS properties
 * @param style Computed style map containing animation properties
 * @param current_time Current renderer time for start_time initialization
 * @return Initialized RunningAnimation
 */
RunningAnimation parse_animation_from_style(
    const std::map<std::string, std::string>& style,
    float current_time);

// ============================================================================
// Utility Functions
// ============================================================================

namespace cssbox_utils {

/**
 * @brief Parse CSS color to NVGcolor
 */
NVGcolor parse_color(const std::string& color_str);

/**
 * @brief Parse CSS length to pixels
 */
float parse_length(const std::string& length_str, float context_value);

/**
 * @brief Check if string ends with suffix
 */
bool ends_with(const std::string& str, const std::string& suffix);

/**
 * @brief Trim whitespace from string
 */
std::string trim(const std::string& str);

/**
 * @brief Split string by delimiter
 */
std::vector<std::string> split(const std::string& str, char delim);

// ============================================================================
// CSS calc() Function (Sprint 24)
// ============================================================================

/**
 * @brief Parse and evaluate CSS calc() expression
 *
 * @param expr The calc() expression (without "calc(" and ")")
 * @param context_value Context value for percentage calculations
 * @param font_size Font size for em/rem calculations (default 16.0f)
 * @return Computed value in pixels
 */
float parse_calc_expression(const std::string& expr, float context_value, float font_size = 16.0f);

// ============================================================================
// CSS min() / max() / clamp() Functions (Sprint 25)
// ============================================================================

/**
 * @brief Parse and evaluate CSS min() function
 *
 * Returns the smallest value from all arguments
 * @param args_str Comma-separated arguments
 * @param context_value Context value for percentage calculations
 * @param font_size Font size for em/rem calculations
 * @return Smallest value in pixels
 */
float parse_min_function(const std::string& args_str, float context_value, float font_size = 16.0f);

/**
 * @brief Parse and evaluate CSS max() function
 *
 * Returns the largest value from all arguments
 * @param args_str Comma-separated arguments
 * @param context_value Context value for percentage calculations
 * @param font_size Font size for em/rem calculations
 * @return Largest value in pixels
 */
float parse_max_function(const std::string& args_str, float context_value, float font_size = 16.0f);

/**
 * @brief Parse and evaluate CSS clamp() function
 *
 * Clamps a value between minimum and maximum
 * @param args_str Three comma-separated arguments: min, preferred, max
 * @param context_value Context value for percentage calculations
 * @param font_size Font size for em/rem calculations
 * @return Clamped value in pixels
 */
float parse_clamp_function(const std::string& args_str, float context_value, float font_size = 16.0f);

// ============================================================================
// Min/Max Dimension Constraints (Sprint 18)
// ============================================================================

/**
 * @brief Apply min/max constraints to a dimension
 *
 * @param value Computed dimension value
 * @param min_value Minimum constraint (-1 = no constraint)
 * @param max_value Maximum constraint (-1 = no constraint)
 * @return Clamped value
 *
 * Note: If min > max, min wins (CSS spec behavior)
 */
float apply_dimension_constraints(float value, float min_value, float max_value);

// ============================================================================
// Box-sizing Support (Sprint 19)
// ============================================================================

/**
 * @brief Calculate content dimensions based on box-sizing model
 *
 * @param total_width Total width (as specified in CSS)
 * @param total_height Total height (as specified in CSS)
 * @param padding Padding on all sides [top, right, bottom, left]
 * @param border Border width on all sides [top, right, bottom, left]
 * @param box_sizing "content-box" or "border-box"
 * @param[out] content_width Calculated content width
 * @param[out] content_height Calculated content height
 */
void calculate_content_dimensions(
    float total_width,
    float total_height,
    const float padding[4],
    const float border[4],
    const std::string& box_sizing,
    float& content_width,
    float& content_height);

// ============================================================================
// Animation & Transition Utilities (Phase 4)
// ============================================================================

/**
 * @brief Apply easing function to time value [0, 1]
 * @param t Time value between 0 and 1
 * @param easing Easing function to apply
 * @return Eased value between 0 and 1
 */
float apply_easing(float t, EasingFunction easing);

/**
 * @brief Interpolate between two float values
 */
float interpolate_float(float start, float end, float t);

/**
 * @brief Interpolate between two colors
 */
NVGcolor interpolate_color(const NVGcolor& start, const NVGcolor& end, float t);

/**
 * @brief Interpolate between two CSS property values
 * @param start_value Starting CSS value
 * @param end_value Ending CSS value
 * @param t Interpolation factor [0, 1]
 * @return Interpolated CSS value string
 */
std::string interpolate_value(const std::string& start_value,
                               const std::string& end_value,
                               float t);

/**
 * @brief Parse transition CSS property
 * @param transition_css CSS transition property value (e.g., "all 0.3s ease-in-out")
 * @param out_property Output: property name
 * @param out_duration Output: duration in seconds
 * @param out_easing Output: easing function
 * @return true if successfully parsed
 */
bool parse_transition(const std::string& transition_css,
                     std::string& out_property,
                     float& out_duration,
                     EasingFunction& out_easing);

/**
 * @brief Parse transition CSS property with cubic-bezier support
 * @param transition_css CSS transition property value (e.g., "all 0.3s cubic-bezier(0.4, 0, 0.2, 1)")
 * @param out_property Output: property name
 * @param out_duration Output: duration in seconds
 * @param out_easing Output: easing function
 * @param out_bezier_x1 Output: cubic-bezier x1 control point
 * @param out_bezier_y1 Output: cubic-bezier y1 control point
 * @param out_bezier_x2 Output: cubic-bezier x2 control point
 * @param out_bezier_y2 Output: cubic-bezier y2 control point
 * @return true if successfully parsed
 */
bool parse_transition(const std::string& transition_css,
                     std::string& out_property,
                     float& out_duration,
                     EasingFunction& out_easing,
                     float& out_bezier_x1,
                     float& out_bezier_y1,
                     float& out_bezier_x2,
                     float& out_bezier_y2);

// ============================================================================
// Box Shadow Parsing (Sprint 27)
// ============================================================================

/**
 * @brief Parse CSS box-shadow property
 *
 * Supports multiple shadows separated by commas.
 * @param shadow_css CSS box-shadow value
 * @return Vector of parsed shadows
 */
std::vector<BoxShadow> parse_box_shadow(const std::string& shadow_css);

// ============================================================================
// Text Shadow Parsing (Sprint 28)
// ============================================================================

/**
 * @brief Parse CSS text-shadow property
 *
 * Supports multiple shadows separated by commas.
 * @param shadow_css CSS text-shadow value
 * @return Vector of parsed shadows
 */
std::vector<TextShadow> parse_text_shadow(const std::string& shadow_css);

/**
 * @brief Parse CSS filter property
 *
 * Supports multiple filters separated by spaces.
 * @param filter_css CSS filter value (e.g., "blur(5px) brightness(1.2)")
 * @return Vector of parsed filter effects
 */
std::vector<cssbox::FilterEffect> parse_filter(const std::string& filter_css);
// ============================================================================
// Background Image Parsing (Sprint 31)
// ============================================================================

/**
 * @brief Parse background-image URL
 */
std::string parse_background_image(const std::string& image_css);

/**
 * @brief Parse background-size property
 */
void parse_background_size(const std::string& size_css,
                           BackgroundSize& mode,
                           float& width,
                           float& height);

/**
 * @brief Parse background-position property
 */
BackgroundPosition parse_background_position(const std::string& position_css);

/**
 * @brief Parse background-repeat property
 */
BackgroundRepeat parse_background_repeat(const std::string& repeat_css);

} // namespace cssbox_utils

#endif // cssbox_INTERNAL_H
