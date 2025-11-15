/*
 * NanoVG CSS - CSS-driven rendering for NanoVG
 *
 * Provides a declarative CSS-based layer on top of NanoVG's imperative Canvas API.
 * Uses lexbor for CSS parsing and supports a subset of CSS3 properties.
 *
 * Copyright (c) 2025 NanoGUI Contributors
 *
 * SPDX-License-Identifier: Zlib
 */

#ifndef NANOVG_CSS_H
#define NANOVG_CSS_H

#include <nanovg.h>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>

// Forward declarations
struct NVGCSSElement;
struct NVGCSSRenderer;
struct NVGCSSBox;

// ============================================================================
// Core Types
// ============================================================================

/**
 * @brief Box model for CSS layout
 *
 * Represents the computed position and dimensions of an element including
 * padding, border, and margin (similar to CSS box model).
 */
struct NVGCSSBox {
    float x, y;              // Position
    float width, height;     // Content dimensions

    // Box model (top, right, bottom, left)
    float padding[4];
    float margin[4];
    float border_width[4];

    // Border radius (top-left, top-right, bottom-right, bottom-left)
    float border_radius[4];
};

/**
 * @brief CSS box-shadow representation (Sprint 27)
 *
 * Represents a single box shadow with all its parameters.
 */
struct BoxShadow {
    float offset_x;       // Horizontal offset
    float offset_y;       // Vertical offset
    float blur_radius;    // Blur amount
    float spread_radius;  // Spread (expansion) amount
    NVGcolor color;       // Shadow color
    bool inset;           // Is it an inset shadow?

    BoxShadow() : offset_x(0), offset_y(0), blur_radius(0),
                  spread_radius(0), inset(false) {
        color = nvgRGBA(0, 0, 0, 128);  // Default: semi-transparent black
    }
};

/**
 * @brief CSS text-shadow representation (Sprint 28)
 *
 * Represents a single text shadow with offset, blur, and color.
 * Simpler than box-shadow (no spread-radius or inset).
 */
struct TextShadow {
    float offset_x;       // Horizontal offset
    float offset_y;       // Vertical offset
    float blur_radius;    // Blur amount
    NVGcolor color;       // Shadow color

    TextShadow() : offset_x(0), offset_y(0), blur_radius(0) {
        color = nvgRGBA(0, 0, 0, 128);  // Default: semi-transparent black
    }
};

/**
 * @brief Explicit CSS properties (input, never overwritten by layout)
 *
 * Stores explicit values from CSS. Uses -1.0f for "auto" or unset values.
 * This structure represents INPUT to the layout algorithm.
 */
struct NVGCSSExplicitStyle {
    // Dimensions (-1 = auto/unset)
    float width;
    float height;
    float min_width;
    float min_height;
    float max_width;
    float max_height;

    // Position (-1 = auto)
    float x;
    float y;

    // Positioning (Sprint 9)
    std::string position;  // static | relative | absolute | fixed
    float top;             // offset from top (-1 = auto)
    float right;           // offset from right (-1 = auto)
    float bottom;          // offset from bottom (-1 = auto)
    float left;            // offset from left (-1 = auto)
    int z_index;           // stacking order (0 = auto)

    // Box model
    std::string box_sizing;  // "content-box" (default) | "border-box" (Sprint 19)
    float padding[4];      // top, right, bottom, left
    float margin[4];
    float border_width[4];
    float border_radius[4];

    // Overflow (Sprint 20)
    std::string overflow;    // "visible" | "hidden" | "scroll" | "auto"
    std::string overflow_x;  // X-axis overflow
    std::string overflow_y;  // Y-axis overflow

    // Flex item properties
    float flex_grow;
    float flex_shrink;
    float flex_basis;      // -1 = auto
    int order;

    // Grid container properties (PHASE 4 SPRINT 5)
    std::string grid_template_rows;
    std::string grid_template_columns;
    std::string grid_auto_rows;      // Implicit row sizing
    std::string grid_auto_columns;   // Implicit column sizing
    float grid_row_gap;
    float grid_column_gap;

    // Grid item properties (PHASE 4 SPRINT 5)
    int grid_row_start;
    int grid_row_end;
    int grid_column_start;
    int grid_column_end;

    // Grid item spanning (PHASE 4 SPRINT 12)
    int grid_row_span;
    int grid_column_span;

    // Grid template areas (PHASE 4 SPRINT 13)
    std::string grid_template_areas;  // Template definition (container)
    std::string grid_area;            // Area name (item)
    // Grid auto-flow (PHASE 4 SPRINT 32)
    std::string grid_auto_flow;       // "row" | "column" | "dense" | "row dense" | "column dense"
    // Background images (Sprint 31)
    std::string background_image;     // URL path (e.g., "url(path/to/image.png)")
    std::string background_size;      // "auto" | "cover" | "contain" | explicit dimensions
    std::string background_position;  // e.g., "center", "top left", "50% 50%"
    std::string background_repeat;    // "repeat" | "no-repeat" | "repeat-x" | "repeat-y"
    // Border styles (Sprint 33)
    std::string border_top_style;
    std::string border_right_style;
    std::string border_bottom_style;
    std::string border_left_style;
    // Border colors (Sprint 34)
    std::string border_top_color;
    std::string border_right_color;
    std::string border_bottom_color;
    std::string border_left_color;


    // Constructor: initialize to "auto"
    NVGCSSExplicitStyle() {
        width = height = -1.0f;
        min_width = min_height = -1.0f;
        max_width = max_height = -1.0f;
        x = y = -1.0f;
        position = "static";
        top = right = bottom = left = -1.0f;
        z_index = 0;
        box_sizing = "content-box";  // CSS default (Sprint 19)
        overflow = "visible";         // CSS default (Sprint 20)
        overflow_x = "visible";
        overflow_y = "visible";
        flex_grow = 0.0f;
        flex_shrink = 1.0f;
        flex_basis = -1.0f;
        order = 0;
        grid_row_gap = grid_column_gap = 0.0f;
        grid_row_start = grid_row_end = -1;
        grid_row_span = grid_column_span = 1;
        grid_column_start = grid_column_end = -1;
        for (int i = 0; i < 4; i++) {
            padding[i] = margin[i] = border_width[i] = border_radius[i] = 0.0f;
        }
        background_size = "auto";
        background_position = "0% 0%";
        background_repeat = "repeat";
        grid_auto_flow = "row";  // CSS default
        border_top_style = border_right_style = border_bottom_style = border_left_style = "solid";  // CSS default
    }
};

/**
 * @brief Computed layout results (output, computed by layout engine)
 *
 * Stores the final computed values after layout. This structure represents
 * OUTPUT from the layout algorithm.
 */
struct NVGCSSComputedLayout {
    // Final dimensions (always set after layout)
    float width;
    float height;

    // Final position (always set after layout)
    float x;
    float y;

    // Resolved box model
    float content_width;
    float content_height;
    float padding[4];      // top, right, bottom, left
    float border[4];
    float margin[4];
    float border_radius[4];

    // Layout metadata
    enum LayoutSource {
        UNCOMPUTED,        // Not yet computed
        CSS_EXPLICIT,      // From CSS explicit values
        FLEXBOX,           // Computed by flexbox
        GRID,              // Computed by grid (future)
        FLOW,              // Computed by flow layout (future)
        ABSOLUTE,          // Absolute positioning (future)
        RELATIVE           // Relative positioning (future)
    };
    LayoutSource source;

    bool is_computed;      // True if layout has been computed

    // Constructor: initialize to zero/uncomputed
    NVGCSSComputedLayout() {
        width = height = 0.0f;
        x = y = 0.0f;
        content_width = content_height = 0.0f;
        source = UNCOMPUTED;
        is_computed = false;
        for (int i = 0; i < 4; i++) {
            padding[i] = border[i] = margin[i] = border_radius[i] = 0.0f;
        }
    }
};

/**
 * @brief CSS element (DOM-like node)
 *
 * Represents a drawable element with CSS styling.
 * Similar to HTML elements but simplified for graphics rendering.
 */
struct NVGCSSElement {
    // Identity
    std::string id;          // Element ID (for #id selector)
    std::string type;        // Element type: "rect", "circle", "path", "text", "group"

    // Styling
    std::vector<std::string> classes;  // CSS classes (for .class selector)
    std::map<std::string, std::string> attributes;  // Custom attributes
    std::map<std::string, std::string> inline_style;  // Inline styles (highest priority)

    // State
    std::set<std::string> pseudo_states;  // "hover", "active", "focus", "disabled"

    // Layout - NEW: Separated input/output (Phase 4 Sprint 4)
    NVGCSSExplicitStyle explicit_style;   // Input: CSS properties (never overwritten by layout)
    NVGCSSComputedLayout computed;        // Output: Computed layout results

    // Layout - DEPRECATED: Old unified storage (kept for backward compatibility)
    NVGCSSBox box;                        // DEPRECATED: Use computed instead

    float transform[6];      // NanoVG transform matrix
    float opacity;           // Computed opacity [0, 1]
    bool visible;            // Display: none sets this to false

    // Tree structure
    NVGCSSElement* parent;
    std::vector<NVGCSSElement*> children;  // Non-owning pointers (owned by renderer->elements)

    // Sprint 30: Structural pseudo-class support
    int child_index;        // Position among siblings (0-based)
    int total_siblings;     // Total number of siblings

    // Text content (for type == "text")
    std::string text_content;

    // Transition state (Phase 4)
    void* transition_state;  // Points to TransitionState (opaque for C compatibility)
    
    // Animation state (Phase 4 Sprint 2)
    void* animation_state;  // Points to AnimationState (opaque for C compatibility)

    // Visual effects (Sprint 27 + Sprint 28)
    std::vector<BoxShadow> box_shadows;   // Box shadows
    std::vector<TextShadow> text_shadows; // Text shadows

    // Custom rendering callback (optional)
    // Called after CSS styles are applied but before children are rendered
    void (*custom_paint)(NVGcontext*, const NVGCSSElement*, const std::map<std::string, std::string>&);

    // User data
    void* user_data;
};

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Main API
// ============================================================================

/**
 * @brief Create a CSS renderer
 *
 * @param vg NanoVG context
 * @return CSS renderer instance (must be freed with nvgcssDeleteRenderer)
 */
NVGCSSRenderer* nvgcssCreateRenderer(NVGcontext* vg);

/**
 * @brief Delete a CSS renderer
 *
 * @param renderer CSS renderer to delete
 */
void nvgcssDeleteRenderer(NVGCSSRenderer* renderer);

// ============================================================================
// CSS Management
// ============================================================================

/**
 * @brief Parse CSS stylesheet
 *
 * Parses CSS text and adds rules to the renderer's stylesheet.
 * Supports CSS3 syntax via lexbor parser.
 *
 * @param renderer CSS renderer
 * @param css CSS text to parse
 * @return 1 on success, 0 on parse error
 */
int nvgcssParseCSS(NVGCSSRenderer* renderer, const char* css);

/**
 * @brief Add a single CSS rule
 *
 * @param renderer CSS renderer
 * @param selector CSS selector (e.g., ".button", "#myid", "rect:hover")
 * @param property CSS property name (e.g., "background-color")
 * @param value CSS property value (e.g., "red", "#ff0000", "rgb(255, 0, 0)")
 */
void nvgcssAddRule(NVGCSSRenderer* renderer,
                   const char* selector,
                   const char* property,
                   const char* value);

/**
 * @brief Clear all CSS rules
 *
 * @param renderer CSS renderer
 */
void nvgcssClearCSS(NVGCSSRenderer* renderer);

/**
 * @brief Set a CSS variable
 *
 * CSS variables can be used in stylesheets via var(--name).
 * Example: var(--primary-color)
 *
 * @param renderer CSS renderer
 * @param name Variable name (e.g., "--primary-color")
 * @param value Variable value (e.g., "#4a90e2")
 */
void nvgcssSetVariable(NVGCSSRenderer* renderer,
                       const char* name,
                       const char* value);

// ============================================================================
// Element Management
// ============================================================================

/**
 * @brief Create a new element
 *
 * @param renderer CSS renderer
 * @param id Element ID (must be unique)
 * @param type Element type ("rect", "circle", "path", "text", "group")
 * @return Pointer to created element (owned by renderer)
 */
NVGCSSElement* nvgcssCreateElement(NVGCSSRenderer* renderer,
                                   const char* id,
                                   const char* type);

/**
 * @brief Get element by ID
 *
 * @param renderer CSS renderer
 * @param id Element ID
 * @return Pointer to element or NULL if not found
 */
NVGCSSElement* nvgcssGetElement(NVGCSSRenderer* renderer, const char* id);

/**
 * @brief Delete an element
 *
 * Recursively deletes element and all its children.
 *
 * @param renderer CSS renderer
 * @param id Element ID
 */
void nvgcssDeleteElement(NVGCSSRenderer* renderer, const char* id);
/**
 * @brief Clear all elements
 *
 * Removes all elements from the renderer. Useful for immediate-mode GUIs
 * that rebuild the UI tree every frame.
 *
 * @param renderer CSS renderer
 */
void nvgcssClearElements(NVGCSSRenderer* renderer);

/**
 * @brief Add a CSS class to an element
 *
 * @param element Element to modify
 * @param class_name CSS class name (without the dot)
 */
void nvgcssAddClass(NVGCSSElement* element, const char* class_name);

/**
 * @brief Remove a CSS class from an element
 *
 * @param element Element to modify
 * @param class_name CSS class name (without the dot)
 */
void nvgcssRemoveClass(NVGCSSElement* element, const char* class_name);

/**
 * @brief Check if element has a CSS class
 *
 * @param element Element to check
 * @param class_name CSS class name (without the dot)
 * @return 1 if element has class, 0 otherwise
 */
int nvgcssHasClass(const NVGCSSElement* element, const char* class_name);

/**
 * @brief Set element attribute
 *
 * @param element Element to modify
 * @param name Attribute name
 * @param value Attribute value
 */
void nvgcssSetAttribute(NVGCSSElement* element,
                        const char* name,
                        const char* value);

/**
 * @brief Set inline style property
 *
 * Inline styles have highest specificity and override all CSS rules.
 *
 * @param element Element to modify
 * @param property CSS property name
 * @param value CSS property value
 */
void nvgcssSetStyle(NVGCSSElement* element,
                    const char* property,
                    const char* value);

// ============================================================================
// Text Content (Phase 3)
// ============================================================================

/**
 * @brief Set text content for text elements
 *
 * @param element Element to set text on (should be type="text")
 * @param text Text content to display
 */
void nvgcssSetText(NVGCSSElement* element, const char* text);

/**
 * @brief Get text content from element
 *
 * @param element Element to get text from
 * @return Text content (or empty string if not a text element)
 */
const char* nvgcssGetText(const NVGCSSElement* element);

// ============================================================================
// State Management
// ============================================================================

/**
 * @brief Set pseudo-state on an element
 *
 * Pseudo-states trigger :hover, :active, :focus, etc. selectors.
 *
 * @param element Element to modify
 * @param state State name ("hover", "active", "focus", "disabled", etc.)
 * @param active 1 to activate state, 0 to deactivate
 */
void nvgcssSetPseudoState(NVGCSSElement* element,
                          const char* state,
                          int active);

/**
 * @brief Check if element has pseudo-state
 *
 * @param element Element to check
 * @param state State name
 * @return 1 if state is active, 0 otherwise
 */
int nvgcssHasPseudoState(const NVGCSSElement* element, const char* state);

// ============================================================================
// Tree Manipulation
// ============================================================================

/**
 * @brief Add child element
 *
 * @param renderer CSS renderer (needed to update root elements list)
 * @param parent Parent element
 * @param child Child element (ownership transferred to parent)
 */
void nvgcssAppendChild(NVGCSSRenderer* renderer, NVGCSSElement* parent, NVGCSSElement* child);

/**
 * @brief Remove child element
 *
 * @param parent Parent element
 * @param child Child element to remove
 */
void nvgcssRemoveChild(NVGCSSElement* parent, NVGCSSElement* child);

// ============================================================================
// Rendering
// ============================================================================

/**
 * @brief Update animations and transitions (Phase 4)
 *
 * Call this every frame with the time delta to update all active
 * transitions and animations.
 *
 * @param renderer CSS renderer
 * @param delta_time Time elapsed since last update (in seconds)
 */
void nvgcssUpdate(NVGCSSRenderer* renderer, float delta_time);

/**
 * @brief Render all elements
 *
 * Renders the entire element tree with computed CSS styles.
 * Call this within nvgBeginFrame() / nvgEndFrame().
 *
 * @param renderer CSS renderer
 */
void nvgcssRender(NVGCSSRenderer* renderer);

/**
 * @brief Render a specific element and its children
 *
 * @param renderer CSS renderer
 * @param id Element ID to render
 */
void nvgcssRenderElement(NVGCSSRenderer* renderer, const char* id);

/**
 * @brief Compute layout for all elements
 *
 * Computes box model, positions, and transforms based on CSS.
 * Automatically called by nvgcssRender() but can be called manually
 * if you need layout info before rendering.
 *
 * @param renderer CSS renderer
 */
void nvgcssComputeLayout(NVGCSSRenderer* renderer);

/**
 * @brief Get computed style for an element
 *
 * Returns the final computed style after cascade, inheritance, and specificity.
 *
 * @param renderer CSS renderer
 * @param element Element to query
 * @param property CSS property name
 * @param out_value Output buffer for property value
 * @param max_len Maximum length of output buffer
 * @return 1 if property found, 0 otherwise
 */
int nvgcssGetComputedStyle(NVGCSSRenderer* renderer,
                           const NVGCSSElement* element,
                           const char* property,
                           char* out_value,
                           int max_len);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Parse CSS color string to NVGcolor
 *
 * Supports: named colors, hex (#rgb, #rrggbb), rgb(), rgba(), hsl(), hsla()
 *
 * @param color_str CSS color string
 * @return NVGcolor
 */
NVGcolor nvgcssParseColor(const char* color_str);

/**
 * @brief Parse CSS length string to pixels
 *
 * Supports: px, %, em, rem (limited support for relative units)
 *
 * @param length_str CSS length string (e.g., "10px", "50%", "2em")
 * @param context_value Context value for relative units (e.g., parent width for %)
 * @return Length in pixels
 */
float nvgcssParseLength(const char* length_str, float context_value);

/**
 * @brief Set viewport size for percentage calculations
 *
 * @param renderer CSS renderer
 * @param width Viewport width
 * @param height Viewport height
 */
void nvgcssSetViewport(NVGCSSRenderer* renderer, float width, float height);

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ Convenience API
// ============================================================================


#ifdef __cplusplus
namespace nvgcss {

/**
 * @brief C++ wrapper for NVGCSSRenderer
 */
class Renderer {
public:
    Renderer(NVGcontext* vg) : renderer_(nvgcssCreateRenderer(vg)) {}
    ~Renderer() { nvgcssDeleteRenderer(renderer_); }

    // No copy
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Move semantics
    Renderer(Renderer&& other) noexcept : renderer_(other.renderer_) {
        other.renderer_ = nullptr;
    }

    // CSS Management
    bool parse_css(const std::string& css) {
        return nvgcssParseCSS(renderer_, css.c_str()) != 0;
    }

    void add_rule(const std::string& selector,
                  const std::string& property,
                  const std::string& value) {
        nvgcssAddRule(renderer_, selector.c_str(), property.c_str(), value.c_str());
    }

    void set_variable(const std::string& name, const std::string& value) {
        nvgcssSetVariable(renderer_, name.c_str(), value.c_str());
    }

    // Element Management
    NVGCSSElement* create_element(const std::string& id, const std::string& type) {
        return nvgcssCreateElement(renderer_, id.c_str(), type.c_str());
    }

    NVGCSSElement* get_element(const std::string& id) {
        return nvgcssGetElement(renderer_, id.c_str());
    }

    void delete_element(const std::string& id) {
        nvgcssDeleteElement(renderer_, id.c_str());
    }

    // Rendering
    void render() {
        nvgcssRender(renderer_);
    }

    void render_element(const std::string& id) {
        nvgcssRenderElement(renderer_, id.c_str());
    }

    void compute_layout() {
        nvgcssComputeLayout(renderer_);
    }

    void set_viewport(float width, float height) {
        nvgcssSetViewport(renderer_, width, height);
    }

    NVGCSSRenderer* get() { return renderer_; }

private:
    NVGCSSRenderer* renderer_;
};

} // namespace nvgcss
#endif // __cplusplus

#endif // NANOVG_CSS_H
