/*
 * NanoVG CSS v2.0 - Clean CSS-First API
 *
 * A declarative CSS-driven rendering layer for NanoVG.
 * This is a clean redesign focused on CSS-first workflows.
 *
 * Copyright (c) 2025 NanoGUI Contributors
 * SPDX-License-Identifier: Zlib
 */

#ifndef cssbox_V2_H
#define cssbox_V2_H

// Windows min/max macro conflict fix
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <nanovg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct cssboxRenderer;
struct cssboxElement;

typedef struct cssboxRenderer cssboxRenderer;
typedef struct cssboxElement cssboxElement;

// ============================================================================
// Error Handling
// ============================================================================

/**
 * @brief Error severity levels
 */
typedef enum cssboxErrorLevel {
    CSSBOX_ERROR_WARNING = 0,  /**< Non-fatal warning (e.g., unknown CSS property) */
    CSSBOX_ERROR_ERROR = 1,    /**< Error that may affect rendering (e.g., parse error) */
    CSSBOX_ERROR_FATAL = 2     /**< Fatal error (e.g., out of memory) */
} cssboxErrorLevel;

/**
 * @brief Error callback function type
 *
 * @param level Error severity level
 * @param message Error message (valid only during callback)
 * @param user_data User-provided context pointer
 */
typedef void (*cssboxErrorCallback)(cssboxErrorLevel level, const char* message, void* user_data);

/**
 * @brief Set error callback for a renderer
 *
 * The callback will be invoked when errors occur during CSS parsing,
 * layout computation, or rendering. If no callback is set, errors
 * are silently ignored (current default behavior).
 *
 * @param renderer CSS renderer
 * @param callback Error callback function (NULL to disable)
 * @param user_data User-provided context passed to callback
 *
 * @example
 *   void my_error_handler(cssboxErrorLevel level, const char* msg, void* data) {
 *       printf("[cssbox %s] %s\n", level == CSSBOX_ERROR_WARNING ? "WARN" : "ERROR", msg);
 *   }
 *   cssboxSetErrorCallback(renderer, my_error_handler, NULL);
 */
void cssboxSetErrorCallback(cssboxRenderer* renderer, cssboxErrorCallback callback, void* user_data);

// ============================================================================
// Core Types
// ============================================================================

/**
 * @brief Box model for CSS layout
 *
 * Represents the computed position and dimensions of an element including
 * padding, border, and margin (similar to CSS box model).
 */
struct cssboxBox {
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
    int inset;            // Is it an inset shadow? (bool in C)
};

/**
 * @brief CSS text-shadow representation (Sprint 28)
 *
 * Represents a single text shadow with offset, blur, and color.
 */
struct TextShadow {
    float offset_x;       // Horizontal offset
    float offset_y;       // Vertical offset
    float blur_radius;    // Blur amount
    NVGcolor color;       // Shadow color
};

// ============================================================================
// Core Philosophy: CSS-First
// ============================================================================
//
// This API enforces CSS-driven development:
// 1. ALL styling comes from CSS files/text
// 2. C API only manages structure (elements, tree, content)
// 3. Dynamic behavior uses CSS classes and pseudo-states
// 4. NO imperative styling (no setStyle, setColor, etc.)
//
// Example workflow:
//   1. Load CSS file(s)
//   2. Create elements with IDs/classes
//   3. CSS handles ALL visual styling
//   4. Use pseudo-states for interactivity
// ============================================================================

// ============================================================================
// Renderer Lifecycle
// ============================================================================

/**
 * @brief Create a CSS renderer
 *
 * @param vg NanoVG context (must remain valid for renderer's lifetime)
 * @return Renderer instance or NULL on failure
 */
cssboxRenderer* cssboxCreateRenderer(NVGcontext* vg);

/**
 * @brief Delete a CSS renderer and all its elements
 *
 * @param renderer Renderer to delete
 */
void cssboxDeleteRenderer(cssboxRenderer* renderer);

/**
 * @brief Set viewport dimensions for layout calculations
 *
 * Call this when window is resized. Affects % units and viewport-relative units.
 *
 * @param renderer CSS renderer
 * @param width Viewport width in pixels
 * @param height Viewport height in pixels
 */
void cssboxSetViewport(cssboxRenderer* renderer, float width, float height);

// ============================================================================
// CSS Management (PRIMARY API)
// ============================================================================

/**
 * @brief Load CSS from file path
 *
 * This is the RECOMMENDED way to load styles. File path is stored for hot-reload.
 *
 * @param renderer CSS renderer
 * @param filepath Path to CSS file
 * @return 1 on success, 0 on failure (file not found or parse error)
 *
 * @example
 *   cssboxLoadCSSFile(renderer, "styles/app.css");
 */
int cssboxLoadCSSFile(cssboxRenderer* renderer, const char* filepath);

/**
 * @brief Parse CSS from text string
 *
 * Use this for embedded CSS or dynamically generated styles.
 *
 * @param renderer CSS renderer
 * @param css_text CSS text to parse
 * @return 1 on success, 0 on parse error
 *
 * @example
 *   const char* css = ".button { background: blue; }";
 *   cssboxParseCSS(renderer, css);
 */
int cssboxParseCSS(cssboxRenderer* renderer, const char* css_text);

/**
 * @brief Hot-reload all CSS files
 *
 * Re-reads all files loaded via cssboxLoadCSSFile(). Useful for development.
 * Files loaded via cssboxParseCSS() are NOT reloaded.
 *
 * @param renderer CSS renderer
 * @return Number of files successfully reloaded
 */
int cssboxReloadCSS(cssboxRenderer* renderer);

/**
 * @brief Set a CSS variable
 *
 * CSS variables enable dynamic theming without reloading CSS.
 * Variables can be referenced in CSS using var(--name).
 *
 * @param renderer CSS renderer
 * @param name Variable name (must start with "--", e.g., "--primary-color")
 * @param value Variable value (e.g., "#4a90e2", "16px", "bold")
 *
 * @example
 *   cssboxSetVariable(renderer, "--primary-color", "#4a90e2");
 *   cssboxSetVariable(renderer, "--font-size", "16px");
 *
 *   // In CSS:
 *   .button { background: var(--primary-color); }
 */
void cssboxSetVariable(cssboxRenderer* renderer, const char* name, const char* value);

/**
 * @brief Get a CSS variable value
 *
 * @param renderer CSS renderer
 * @param name Variable name
 * @param out_value Output buffer for value
 * @param max_len Maximum length of output buffer
 * @return 1 if variable found, 0 otherwise
 */
int cssboxGetVariable(cssboxRenderer* renderer, const char* name,
                      char* out_value, int max_len);

/**
 * @brief Clear all CSS rules and variables
 *
 * @param renderer CSS renderer
 */
void cssboxClearCSS(cssboxRenderer* renderer);

// ============================================================================
// Element Management (Structure Only - No Styling)
// ============================================================================

/**
 * @brief Create a new element
 *
 * Elements are the building blocks of the document tree.
 * ALL styling should come from CSS, not programmatic calls.
 *
 * @param renderer CSS renderer
 * @param id Element ID (must be unique, used for CSS #id selectors)
 * @param type Element type (used for CSS type selectors: "rect", "circle", "text", "group")
 * @return Created element or NULL on failure (duplicate ID)
 *
 * @example
 *   cssboxElement* btn = cssboxCreateElement(renderer, "submit-btn", "rect");
 */
cssboxElement* cssboxCreateElement(cssboxRenderer* renderer,
                                   const char* id,
                                   const char* type);

/**
 * @brief Get element by ID
 *
 * @param renderer CSS renderer
 * @param id Element ID
 * @return Element pointer or NULL if not found
 */
cssboxElement* cssboxGetElement(cssboxRenderer* renderer, const char* id);

/**
 * @brief Delete an element and all its children
 *
 * @param renderer CSS renderer
 * @param id Element ID to delete
 */
void cssboxDeleteElement(cssboxRenderer* renderer, const char* id);

/**
 * @brief Clear all elements
 *
 * Useful for immediate-mode GUIs that rebuild tree every frame.
 *
 * @param renderer CSS renderer
 */
void cssboxClearElements(cssboxRenderer* renderer);

/**
 * @brief Shrink memory usage by clearing internal caches
 *
 * Call this when memory is tight or after major scene changes.
 * Clears: style cache, gradient cache, unused image cache entries.
 *
 * @param renderer CSS renderer
 */
void cssboxShrinkMemory(cssboxRenderer* renderer);

// ============================================================================
// SVG Marker Management
// ============================================================================

/**
 * @brief Create an SVG marker definition
 *
 * Markers are reusable graphical elements that can be placed at path endpoints
 * and vertices. Common uses include arrow heads, dots, and decorations.
 *
 * @param renderer CSS renderer
 * @param id Marker ID (used in marker-start/mid/end: url(#id))
 * @param markerWidth Marker viewport width
 * @param markerHeight Marker viewport height
 * @param refX Reference point X (where marker attaches to path)
 * @param refY Reference point Y (where marker attaches to path)
 * @param orient Orientation: "auto", "auto-start-reverse", or angle in degrees
 * @return Created marker element (add children to define marker shape)
 *
 * @example
 *   // Create arrow marker
 *   cssboxElement* arrow = cssboxCreateMarker(renderer, "arrow", 10, 10, 10, 5, "auto");
 *   cssboxElement* path = cssboxCreateElement(renderer, "arrow-shape", "path");
 *   cssboxSetStyle(path, "d", "M 0 0 L 10 5 L 0 10 Z");
 *   cssboxSetStyle(path, "fill", "black");
 *   cssboxAppendChild(renderer, arrow, path);
 *
 *   // Use marker on line
 *   cssboxElement* line = cssboxCreateElement(renderer, "line1", "line");
 *   cssboxSetStyle(line, "marker-end", "url(#arrow)");
 */
cssboxElement* cssboxCreateMarker(cssboxRenderer* renderer,
                                   const char* id,
                                   float markerWidth,
                                   float markerHeight,
                                   float refX,
                                   float refY,
                                   const char* orient);

/**
 * @brief Create an SVG clip path definition
 *
 * Clip paths define a clipping region that determines what parts of an element are visible.
 * Add shapes (rect, circle, path, etc.) as children to define the clipping region.
 *
 * @param renderer Renderer instance
 * @param id Clip path ID (referenced via clip-path: url(#id))
 * @return Clip path element (add shapes as children)
 *
 * @example
 *   // Create circular clip path
 *   cssboxElement* clip = cssboxCreateClipPath(renderer, "circle-clip");
 *   cssboxElement* circle = cssboxCreateElement(renderer, "c1", "circle");
 *   cssboxSetInlineStyle(renderer, circle, "r", "50px");
 *   cssboxAppendChild(renderer, clip, circle);
 *
 *   // Apply to element
 *   cssboxElement* rect = cssboxCreateElement(renderer, "r1", "rect");
 *   cssboxSetInlineStyle(renderer, rect, "clip-path", "url(#circle-clip)");
 */
cssboxElement* cssboxCreateClipPath(cssboxRenderer* renderer, const char* id);

/**
 * @brief Create an SVG pattern definition
 *
 * Patterns define repeating graphical content for fills and strokes.
 * Add shapes as children to define the pattern content.
 * Pattern will be rendered to an offscreen FBO and used as a tiled image.
 *
 * @param renderer Renderer instance
 * @param id Pattern ID (referenced via fill: url(#id))
 * @param x Pattern x offset
 * @param y Pattern y offset
 * @param width Pattern tile width
 * @param height Pattern tile height
 * @return Pattern element (add shapes as children)
 *
 * @example
 *   // Create dot pattern
 *   cssboxElement* pattern = cssboxCreatePattern(renderer, "dots", 0, 0, 20, 20);
 *   cssboxElement* circle = cssboxCreateElement(renderer, "dot", "circle");
 *   cssboxSetInlineStyle(renderer, circle, "cx", "10px");
 *   cssboxSetInlineStyle(renderer, circle, "cy", "10px");
 *   cssboxSetInlineStyle(renderer, circle, "r", "5px");
 *   cssboxSetInlineStyle(renderer, circle, "fill", "blue");
 *   cssboxAppendChild(renderer, pattern, circle);
 *
 *   // Apply to element
 *   cssboxElement* rect = cssboxCreateElement(renderer, "r1", "rect");
 *   cssboxSetInlineStyle(renderer, rect, "fill", "url(#dots)");
 */
cssboxElement* cssboxCreatePattern(cssboxRenderer* renderer,
                                   const char* id,
                                   float x, float y,
                                   float width, float height);

// ============================================================================
// CSS Class Management (For CSS Targeting)
// ============================================================================

/**
 * @brief Add a CSS class to element
 *
 * Classes are the PRIMARY way to style elements from CSS.
 *
 * @param element Element to modify
 * @param class_name Class name (without the dot)
 *
 * @example
 *   cssboxAddClass(btn, "button");
 *   cssboxAddClass(btn, "primary");
 *
 *   // CSS:
 *   .button { padding: 10px; }
 *   .primary { background: blue; }
 */
void cssboxAddClass(cssboxElement* element, const char* class_name);

/**
 * @brief Remove a CSS class from element
 *
 * @param element Element to modify
 * @param class_name Class name (without the dot)
 */
void cssboxRemoveClass(cssboxElement* element, const char* class_name);

/**
 * @brief Check if element has a CSS class
 *
 * @param element Element to check
 * @param class_name Class name (without the dot)
 * @return 1 if element has class, 0 otherwise
 */
int cssboxHasClass(const cssboxElement* element, const char* class_name);

// ============================================================================
// Tree Manipulation
// ============================================================================

/**
 * @brief Append child to parent element
 *
 * @param renderer CSS renderer (needed for tree updates)
 * @param parent Parent element
 * @param child Child element to append
 */
void cssboxAppendChild(cssboxRenderer* renderer,
                      cssboxElement* parent,
                      cssboxElement* child);

/**
 * @brief Remove child from parent element
 *
 * @param renderer CSS renderer (needed for tree updates)
 * @param parent Parent element
 * @param child Child element to remove
 */
void cssboxRemoveChild(cssboxRenderer* renderer,
                      cssboxElement* parent,
                      cssboxElement* child);

/**
 * @brief Get parent element
 *
 * @param renderer CSS renderer
 * @param element Child element
 * @return Parent element or NULL if element is root
 */
cssboxElement* cssboxGetParent(cssboxRenderer* renderer, const cssboxElement* element);

/**
 * @brief Get child elements
 *
 * @param renderer CSS renderer
 * @param element Parent element
 * @param out_count Output parameter for number of children
 * @return Array of child elements (valid until next element mutation)
 */
cssboxElement** cssboxGetChildren(cssboxRenderer* renderer,
                                  const cssboxElement* element,
                                  int* out_count);

// ============================================================================
// Content Management
// ============================================================================

/**
 * @brief Set text content for text elements
 *
 * @param element Element (should be type="text")
 * @param text Text content to display
 */
void cssboxSetText(cssboxElement* element, const char* text);

/**
 * @brief Get text content from element
 *
 * @param element Element
 * @return Text content (or empty string if not a text element)
 */
const char* cssboxGetText(const cssboxElement* element);

/**
 * @brief Set inline style property with proper dirty flag handling
 *
 * Use this instead of directly modifying element->inline_style to ensure
 * render list and transform caches are properly invalidated.
 *
 * @param renderer Renderer context
 * @param element Element to modify
 * @param property CSS property name (e.g., "display", "transform", "opacity")
 * @param value CSS property value
 *
 * @example
 *   cssboxSetInlineStyle(renderer, elem, "display", "none");
 *   cssboxSetInlineStyle(renderer, elem, "transform", "rotate(45deg)");
 */
void cssboxSetInlineStyle(cssboxRenderer* renderer, cssboxElement* element,
                          const char* property, const char* value);

/**
 * @brief Remove inline style property
 *
 * @param renderer Renderer context
 * @param element Element to modify
 * @param property CSS property name to remove
 */
void cssboxRemoveInlineStyle(cssboxRenderer* renderer, cssboxElement* element,
                             const char* property);

// ============================================================================
// Dynamic State Management (For CSS Pseudo-Classes)
// ============================================================================

/**
 * @brief Set pseudo-state for CSS pseudo-class selectors
 *
 * Pseudo-states enable interactive styling via CSS :hover, :active, etc.
 *
 * @param element Element to modify
 * @param state State name ("hover", "active", "focus", "disabled", etc.)
 * @param active 1 to activate, 0 to deactivate
 *
 * @example
 *   // User hovers over button
 *   cssboxSetPseudoState(btn, "hover", 1);
 *
 *   // CSS handles visual change:
 *   .button:hover { background: darkblue; }
 */
void cssboxSetPseudoState(cssboxElement* element, const char* state, int active);

/**
 * @brief Set pseudo-state with proper dirty flag handling (Retain Mode)
 *
 * Use this instead of cssboxSetPseudoState when you need render list
 * invalidation for pseudo-states that can affect display/visibility.
 *
 * @param renderer Renderer context
 * @param element Element to modify
 * @param state State name
 * @param active 1 to activate, 0 to deactivate
 */
void cssboxSetPseudoStateEx(cssboxRenderer* renderer, cssboxElement* element,
                            const char* state, int active);

/**
 * @brief Check if element has pseudo-state
 *
 * @param element Element to check
 * @param state State name
 * @return 1 if state is active, 0 otherwise
 */
int cssboxHasPseudoState(const cssboxElement* element, const char* state);

// ============================================================================
// Scroll Support
// ============================================================================

/**
 * @brief Set scroll offset for an element
 *
 * For elements with overflow: scroll/auto, this sets the scroll position.
 * Children will be offset by this amount during rendering.
 *
 * @param element Element to scroll
 * @param scroll_x Horizontal scroll offset (positive = scrolled right)
 * @param scroll_y Vertical scroll offset (positive = scrolled down)
 */
void cssboxSetScroll(cssboxElement* element, float scroll_x, float scroll_y);

/**
 * @brief Get scroll offset for an element
 *
 * @param element Element to query
 * @param out_scroll_x Output horizontal scroll offset
 * @param out_scroll_y Output vertical scroll offset
 */
void cssboxGetScroll(const cssboxElement* element, float* out_scroll_x, float* out_scroll_y);

/**
 * @brief Set content height for scroll calculations
 *
 * This is the total height of the content inside the element.
 * Used to calculate scrollbar size and maximum scroll offset.
 *
 * @param element Element to set content height for
 * @param content_height Total content height in pixels
 */
void cssboxSetContentHeight(cssboxElement* element, float content_height);

/**
 * @brief Get content height
 *
 * @param element Element to query
 * @return Content height in pixels
 */
float cssboxGetContentHeight(const cssboxElement* element);

// ============================================================================
// Rendering Pipeline
// ============================================================================

/**
 * @brief Update animations and transitions
 *
 * Call this every frame with delta time to animate CSS transitions and @keyframes.
 *
 * @param renderer CSS renderer
 * @param delta_time Time elapsed since last update (in seconds)
 */
void cssboxUpdate(cssboxRenderer* renderer, float delta_time);

/**
 * @brief Compute layout for all elements
 *
 * Computes positions and dimensions based on CSS.
 * Automatically called by cssboxRender(), but can be called manually
 * if you need layout info before rendering.
 *
 * @param renderer CSS renderer
 */
void cssboxComputeLayout(cssboxRenderer* renderer);

/**
 * @brief Prepare SVG patterns for rendering (pre-pass)
 *
 * Call this BEFORE nvgBeginFrame() to pre-render any dirty SVG patterns.
 * This is required because pattern rendering uses its own NVG frame internally.
 *
 * @param renderer CSS renderer
 */
void cssboxPreparePatterns(cssboxRenderer* renderer);

/**
 * @brief Render all elements
 *
 * Renders entire document tree with computed CSS styles.
 * Must be called within nvgBeginFrame() / nvgEndFrame().
 *
 * @param renderer CSS renderer
 */
void cssboxRender(cssboxRenderer* renderer);

/**
 * @brief Check if painting is needed (retain mode optimization)
 *
 * Call this BEFORE nvgBeginFrame() to check if rendering is needed.
 * If returns 0, you can skip the entire frame (nvgBeginFrame/cssboxRender/nvgEndFrame)
 * to save GPU resources when UI is static.
 *
 * @param renderer CSS renderer
 * @return 1 if paint needed, 0 if can skip
 */
int cssboxNeedsPaint(cssboxRenderer* renderer);

/**
 * @brief Mark renderer as needing repaint
 *
 * Call this when external changes require a repaint (e.g., window exposed after minimize)
 *
 * @param renderer CSS renderer
 */
void cssboxInvalidatePaint(cssboxRenderer* renderer);

// ============================================================================
// Dirty Flags (60fps Optimization)
// ============================================================================

/**
 * @brief Dirty flag bits for incremental updates
 */
enum cssboxDirtyFlags {
    cssbox_DIRTY_NONE      = 0,
    cssbox_DIRTY_STYLE     = 1 << 0,  /**< CSS properties changed */
    cssbox_DIRTY_LAYOUT    = 1 << 1,  /**< Layout needs recomputation */
    cssbox_DIRTY_TRANSFORM = 1 << 2,  /**< Transform changed */
    cssbox_DIRTY_CHILDREN  = 1 << 3,  /**< Children list changed */
    cssbox_DIRTY_ALL       = 0xFFFFFFFF
};

/**
 * @brief Mark an element as dirty
 *
 * When CSS properties change (e.g., via animation or user interaction),
 * call this to mark the element for re-layout and re-render.
 *
 * @param element Element to mark
 * @param flags Combination of cssboxDirtyFlags
 */
void cssboxMarkDirty(cssboxElement* element, int flags);

/**
 * @brief Mark all elements as dirty
 *
 * Call this after major changes (e.g., CSS reload, viewport resize).
 *
 * @param renderer CSS renderer
 * @param flags Combination of cssboxDirtyFlags
 */
void cssboxMarkAllDirty(cssboxRenderer* renderer, int flags);

/**
 * @brief Check if element is dirty
 *
 * @param element Element to check
 * @param flags Flags to check (any bit set = dirty)
 * @return 1 if any specified flag is set, 0 otherwise
 */
int cssboxIsDirty(const cssboxElement* element, int flags);

// ============================================================================
// Query Computed Styles (Read-Only)
// ============================================================================

/**
 * @brief Get computed style property value
 *
 * Returns the FINAL computed value after CSS cascade, inheritance, and layout.
 *
 * @param renderer CSS renderer
 * @param element Element to query
 * @param property CSS property name (e.g., "background-color", "width")
 * @param out_value Output buffer for value
 * @param max_len Maximum length of output buffer
 * @return 1 if property found, 0 otherwise
 */
int cssboxGetComputedStyle(cssboxRenderer* renderer,
                           const cssboxElement* element,
                           const char* property,
                           char* out_value,
                           int max_len);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Parse CSS color string to NVGcolor
 *
 * Supports: named colors, hex (#rgb, #rrggbb, #rrggbbaa), rgb(), rgba(), hsl(), hsla()
 *
 * @param color_str CSS color string
 * @return NVGcolor
 */
NVGcolor cssboxParseColor(const char* color_str);

/**
 * @brief Parse CSS length string to pixels
 *
 * Supports: px, %, em, rem, vw, vh
 *
 * @param length_str CSS length string (e.g., "10px", "50%")
 * @param context_value Context value for relative units (e.g., parent width for %)
 * @return Length in pixels
 */
float cssboxParseLength(const char* length_str, float context_value);

// ============================================================================
// Memory Profiling
// ============================================================================

/**
 * @brief Print detailed memory usage report
 *
 * Outputs a formatted report showing:
 * - Process RSS (total memory)
 * - Memory arena usage
 * - Element count and estimated memory
 * - Style cache statistics
 * - Various internal caches
 *
 * @param renderer CSS renderer (can be NULL for process-only stats)
 */
void cssboxPrintMemoryProfile(cssboxRenderer* renderer);

/**
 * @brief Get current process memory usage in megabytes
 *
 * @param renderer CSS renderer (currently unused, for future per-renderer stats)
 * @return Memory usage in MB
 */
float cssboxGetMemoryUsageMB(cssboxRenderer* renderer);

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ Convenience API
// ============================================================================

#ifdef __cplusplus
#include <string>  // For std::string in C++ API

namespace cssbox {

/**
 * @brief C++ wrapper for cssboxRenderer with RAII
 */
class Renderer {
public:
    explicit Renderer(NVGcontext* vg) : renderer_(cssboxCreateRenderer(vg)) {}
    ~Renderer() { cssboxDeleteRenderer(renderer_); }

    // No copy
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Move semantics
    Renderer(Renderer&& other) noexcept : renderer_(other.renderer_) {
        other.renderer_ = nullptr;
    }

    // CSS Management
    bool load_css_file(const std::string& filepath) {
        return cssboxLoadCSSFile(renderer_, filepath.c_str()) != 0;
    }

    bool parse_css(const std::string& css) {
        return cssboxParseCSS(renderer_, css.c_str()) != 0;
    }

    int reload_css() {
        return cssboxReloadCSS(renderer_);
    }

    void set_variable(const std::string& name, const std::string& value) {
        cssboxSetVariable(renderer_, name.c_str(), value.c_str());
    }

    void clear_css() {
        cssboxClearCSS(renderer_);
    }

    // Element Management
    cssboxElement* create_element(const std::string& id, const std::string& type) {
        return cssboxCreateElement(renderer_, id.c_str(), type.c_str());
    }

    cssboxElement* get_element(const std::string& id) {
        return cssboxGetElement(renderer_, id.c_str());
    }

    void delete_element(const std::string& id) {
        cssboxDeleteElement(renderer_, id.c_str());
    }

    void clear_elements() {
        cssboxClearElements(renderer_);
    }

    // Rendering
    void set_viewport(float width, float height) {
        cssboxSetViewport(renderer_, width, height);
    }

    void update(float delta_time) {
        cssboxUpdate(renderer_, delta_time);
    }

    void compute_layout() {
        cssboxComputeLayout(renderer_);
    }

    void render() {
        cssboxRender(renderer_);
    }

    // Get raw pointer (for C API calls)
    cssboxRenderer* get() { return renderer_; }
    const cssboxRenderer* get() const { return renderer_; }

private:
    cssboxRenderer* renderer_;
};

} // namespace cssbox
#endif // __cplusplus

#endif // cssbox_V2_H
