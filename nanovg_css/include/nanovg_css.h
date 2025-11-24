/*
 * NanoVG CSS v2.0 - Clean CSS-First API
 *
 * A declarative CSS-driven rendering layer for NanoVG.
 * This is a clean redesign focused on CSS-first workflows.
 *
 * Copyright (c) 2025 NanoGUI Contributors
 * SPDX-License-Identifier: Zlib
 */

#ifndef NANOVG_CSS_V2_H
#define NANOVG_CSS_V2_H

#include <nanovg.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations
struct NVGCSSRenderer;
struct NVGCSSElement;
struct NVGCSSBox;

typedef struct NVGCSSRenderer NVGCSSRenderer;
typedef struct NVGCSSElement NVGCSSElement;

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
NVGCSSRenderer* nvgcssCreateRenderer(NVGcontext* vg);

/**
 * @brief Delete a CSS renderer and all its elements
 *
 * @param renderer Renderer to delete
 */
void nvgcssDeleteRenderer(NVGCSSRenderer* renderer);

/**
 * @brief Set viewport dimensions for layout calculations
 *
 * Call this when window is resized. Affects % units and viewport-relative units.
 *
 * @param renderer CSS renderer
 * @param width Viewport width in pixels
 * @param height Viewport height in pixels
 */
void nvgcssSetViewport(NVGCSSRenderer* renderer, float width, float height);

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
 *   nvgcssLoadCSSFile(renderer, "styles/app.css");
 */
int nvgcssLoadCSSFile(NVGCSSRenderer* renderer, const char* filepath);

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
 *   nvgcssParseCSS(renderer, css);
 */
int nvgcssParseCSS(NVGCSSRenderer* renderer, const char* css_text);

/**
 * @brief Hot-reload all CSS files
 *
 * Re-reads all files loaded via nvgcssLoadCSSFile(). Useful for development.
 * Files loaded via nvgcssParseCSS() are NOT reloaded.
 *
 * @param renderer CSS renderer
 * @return Number of files successfully reloaded
 */
int nvgcssReloadCSS(NVGCSSRenderer* renderer);

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
 *   nvgcssSetVariable(renderer, "--primary-color", "#4a90e2");
 *   nvgcssSetVariable(renderer, "--font-size", "16px");
 *
 *   // In CSS:
 *   .button { background: var(--primary-color); }
 */
void nvgcssSetVariable(NVGCSSRenderer* renderer, const char* name, const char* value);

/**
 * @brief Get a CSS variable value
 *
 * @param renderer CSS renderer
 * @param name Variable name
 * @param out_value Output buffer for value
 * @param max_len Maximum length of output buffer
 * @return 1 if variable found, 0 otherwise
 */
int nvgcssGetVariable(NVGCSSRenderer* renderer, const char* name,
                      char* out_value, int max_len);

/**
 * @brief Clear all CSS rules and variables
 *
 * @param renderer CSS renderer
 */
void nvgcssClearCSS(NVGCSSRenderer* renderer);

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
 *   NVGCSSElement* btn = nvgcssCreateElement(renderer, "submit-btn", "rect");
 */
NVGCSSElement* nvgcssCreateElement(NVGCSSRenderer* renderer,
                                   const char* id,
                                   const char* type);

/**
 * @brief Get element by ID
 *
 * @param renderer CSS renderer
 * @param id Element ID
 * @return Element pointer or NULL if not found
 */
NVGCSSElement* nvgcssGetElement(NVGCSSRenderer* renderer, const char* id);

/**
 * @brief Delete an element and all its children
 *
 * @param renderer CSS renderer
 * @param id Element ID to delete
 */
void nvgcssDeleteElement(NVGCSSRenderer* renderer, const char* id);

/**
 * @brief Clear all elements
 *
 * Useful for immediate-mode GUIs that rebuild tree every frame.
 *
 * @param renderer CSS renderer
 */
void nvgcssClearElements(NVGCSSRenderer* renderer);

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
 *   nvgcssAddClass(btn, "button");
 *   nvgcssAddClass(btn, "primary");
 *
 *   // CSS:
 *   .button { padding: 10px; }
 *   .primary { background: blue; }
 */
void nvgcssAddClass(NVGCSSElement* element, const char* class_name);

/**
 * @brief Remove a CSS class from element
 *
 * @param element Element to modify
 * @param class_name Class name (without the dot)
 */
void nvgcssRemoveClass(NVGCSSElement* element, const char* class_name);

/**
 * @brief Check if element has a CSS class
 *
 * @param element Element to check
 * @param class_name Class name (without the dot)
 * @return 1 if element has class, 0 otherwise
 */
int nvgcssHasClass(const NVGCSSElement* element, const char* class_name);

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
void nvgcssAppendChild(NVGCSSRenderer* renderer,
                      NVGCSSElement* parent,
                      NVGCSSElement* child);

/**
 * @brief Remove child from parent element
 *
 * @param renderer CSS renderer (needed for tree updates)
 * @param parent Parent element
 * @param child Child element to remove
 */
void nvgcssRemoveChild(NVGCSSRenderer* renderer,
                      NVGCSSElement* parent,
                      NVGCSSElement* child);

/**
 * @brief Get parent element
 *
 * @param renderer CSS renderer
 * @param element Child element
 * @return Parent element or NULL if element is root
 */
NVGCSSElement* nvgcssGetParent(NVGCSSRenderer* renderer, const NVGCSSElement* element);

/**
 * @brief Get child elements
 *
 * @param renderer CSS renderer
 * @param element Parent element
 * @param out_count Output parameter for number of children
 * @return Array of child elements (valid until next element mutation)
 */
NVGCSSElement** nvgcssGetChildren(NVGCSSRenderer* renderer,
                                  const NVGCSSElement* element,
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
void nvgcssSetText(NVGCSSElement* element, const char* text);

/**
 * @brief Get text content from element
 *
 * @param element Element
 * @return Text content (or empty string if not a text element)
 */
const char* nvgcssGetText(const NVGCSSElement* element);

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
 *   nvgcssSetPseudoState(btn, "hover", 1);
 *
 *   // CSS handles visual change:
 *   .button:hover { background: darkblue; }
 */
void nvgcssSetPseudoState(NVGCSSElement* element, const char* state, int active);

/**
 * @brief Check if element has pseudo-state
 *
 * @param element Element to check
 * @param state State name
 * @return 1 if state is active, 0 otherwise
 */
int nvgcssHasPseudoState(const NVGCSSElement* element, const char* state);

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
void nvgcssUpdate(NVGCSSRenderer* renderer, float delta_time);

/**
 * @brief Compute layout for all elements
 *
 * Computes positions and dimensions based on CSS.
 * Automatically called by nvgcssRender(), but can be called manually
 * if you need layout info before rendering.
 *
 * @param renderer CSS renderer
 */
void nvgcssComputeLayout(NVGCSSRenderer* renderer);

/**
 * @brief Render all elements
 *
 * Renders entire document tree with computed CSS styles.
 * Must be called within nvgBeginFrame() / nvgEndFrame().
 *
 * @param renderer CSS renderer
 */
void nvgcssRender(NVGCSSRenderer* renderer);

// ============================================================================
// Dirty Flags (60fps Optimization)
// ============================================================================

/**
 * @brief Dirty flag bits for incremental updates
 */
enum NVGCSSDirtyFlags {
    NVGCSS_DIRTY_NONE      = 0,
    NVGCSS_DIRTY_STYLE     = 1 << 0,  /**< CSS properties changed */
    NVGCSS_DIRTY_LAYOUT    = 1 << 1,  /**< Layout needs recomputation */
    NVGCSS_DIRTY_TRANSFORM = 1 << 2,  /**< Transform changed */
    NVGCSS_DIRTY_CHILDREN  = 1 << 3,  /**< Children list changed */
    NVGCSS_DIRTY_ALL       = 0xFFFFFFFF
};

/**
 * @brief Mark an element as dirty
 *
 * When CSS properties change (e.g., via animation or user interaction),
 * call this to mark the element for re-layout and re-render.
 *
 * @param element Element to mark
 * @param flags Combination of NVGCSSDirtyFlags
 */
void nvgcssMarkDirty(NVGCSSElement* element, int flags);

/**
 * @brief Mark all elements as dirty
 *
 * Call this after major changes (e.g., CSS reload, viewport resize).
 *
 * @param renderer CSS renderer
 * @param flags Combination of NVGCSSDirtyFlags
 */
void nvgcssMarkAllDirty(NVGCSSRenderer* renderer, int flags);

/**
 * @brief Check if element is dirty
 *
 * @param element Element to check
 * @param flags Flags to check (any bit set = dirty)
 * @return 1 if any specified flag is set, 0 otherwise
 */
int nvgcssIsDirty(const NVGCSSElement* element, int flags);

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
 * Supports: named colors, hex (#rgb, #rrggbb, #rrggbbaa), rgb(), rgba(), hsl(), hsla()
 *
 * @param color_str CSS color string
 * @return NVGcolor
 */
NVGcolor nvgcssParseColor(const char* color_str);

/**
 * @brief Parse CSS length string to pixels
 *
 * Supports: px, %, em, rem, vw, vh
 *
 * @param length_str CSS length string (e.g., "10px", "50%")
 * @param context_value Context value for relative units (e.g., parent width for %)
 * @return Length in pixels
 */
float nvgcssParseLength(const char* length_str, float context_value);

#ifdef __cplusplus
}
#endif

// ============================================================================
// C++ Convenience API
// ============================================================================

#ifdef __cplusplus
#include <string>  // For std::string in C++ API

namespace nvgcss {

/**
 * @brief C++ wrapper for NVGCSSRenderer with RAII
 */
class Renderer {
public:
    explicit Renderer(NVGcontext* vg) : renderer_(nvgcssCreateRenderer(vg)) {}
    ~Renderer() { nvgcssDeleteRenderer(renderer_); }

    // No copy
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Move semantics
    Renderer(Renderer&& other) noexcept : renderer_(other.renderer_) {
        other.renderer_ = nullptr;
    }

    // CSS Management
    bool load_css_file(const std::string& filepath) {
        return nvgcssLoadCSSFile(renderer_, filepath.c_str()) != 0;
    }

    bool parse_css(const std::string& css) {
        return nvgcssParseCSS(renderer_, css.c_str()) != 0;
    }

    int reload_css() {
        return nvgcssReloadCSS(renderer_);
    }

    void set_variable(const std::string& name, const std::string& value) {
        nvgcssSetVariable(renderer_, name.c_str(), value.c_str());
    }

    void clear_css() {
        nvgcssClearCSS(renderer_);
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

    void clear_elements() {
        nvgcssClearElements(renderer_);
    }

    // Rendering
    void set_viewport(float width, float height) {
        nvgcssSetViewport(renderer_, width, height);
    }

    void update(float delta_time) {
        nvgcssUpdate(renderer_, delta_time);
    }

    void compute_layout() {
        nvgcssComputeLayout(renderer_);
    }

    void render() {
        nvgcssRender(renderer_);
    }

    // Get raw pointer (for C API calls)
    NVGCSSRenderer* get() { return renderer_; }
    const NVGCSSRenderer* get() const { return renderer_; }

private:
    NVGCSSRenderer* renderer_;
};

} // namespace nvgcss
#endif // __cplusplus

#endif // NANOVG_CSS_V2_H
