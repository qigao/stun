/*
 * NanoVG CSS - Main implementation
 */

// Enable memory profiling (comment out to disable)
#define CSSBOX_MEMORY_PROFILE 1

#include "cssbox_internal.h"
#include "lexbor_css_parser.h"
#include "cssbox_quadtree.h"
#include <fmtlog.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <vector>
#include <fstream>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
// Windows min/max macro conflict fix
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif
// ============================================================================
// Z-Index Rendering (Sprint 10)
// ============================================================================

/**
 * @brief Render order for z-index sorting
 *
 * Rendering order rules:
 * 1. Parents always paint before children (by depth)
 * 2. Siblings are sorted by z-index
 * 3. Same z-index: maintain document order (tree_order)
 */
struct RenderOrder {
    int depth;             // Tree depth (0 = root, 1 = child of root, etc.)
    bool is_positioned;    // position != static
    int z_index;           // From element->style.z_index (typed)
    int tree_order;        // Insertion order for stability

    bool operator<(const RenderOrder& other) const {
        // Rule 1: Parents before children (shallower depth first)
        if (depth != other.depth) {
            return depth < other.depth;
        }

        // Rule 2: Same depth (siblings) - sort by z-index
        // Non-positioned elements have implicit z-index=0
        int effective_z1 = is_positioned ? z_index : 0;
        int effective_z2 = other.is_positioned ? other.z_index : 0;

        if (effective_z1 != effective_z2) {
            return effective_z1 < effective_z2;
        }

        // Rule 3: Same z-index - maintain document order
        return tree_order < other.tree_order;
    }
};

/**
 * @brief Collect all visible elements for rendering
 */
static void collect_elements_for_render(
    cssboxRenderer* renderer,
    cssboxElement* element,
    std::vector<std::pair<cssboxElement*, RenderOrder>>& elements,
    int& tree_order,
    int depth = 0)
{
    if (!element->visible) return;

    // display: none removes element AND all descendants from render tree
    if (element->style.display == cssbox::Display::NONE) {
        if (element->id.find("page-") != std::string::npos) {
            logi("[RENDER] Skipping '{}' - display=NONE", element->id);
        }
        return;
    }

    RenderOrder order;
    order.depth = depth;
    // Use typed Position enum instead of deprecated string comparison
    order.is_positioned = (element->style.position != cssbox::Position::STATIC);
    order.z_index = element->style.z_index;
    order.tree_order = tree_order++;

    elements.push_back({element, order});

    // Recursively collect children (avoid cssboxGetChildren to skip vector copy)
    for (int child_id : element->children_internal_ids) {
        auto child_it = renderer->elements.find(child_id);
        if (child_it != renderer->elements.end()) {
            collect_elements_for_render(renderer, child_it->second.get(), elements, tree_order, depth + 1);
        }
    }
}

/**
 * @brief Compute absolute transform by multiplying parent chain
 *
 * Includes scroll offset from ancestors with overflow: scroll/auto
 */
static void compute_absolute_transform(cssboxRenderer* renderer, cssboxElement* element, float* abs_xform) {
    // Start with identity
    nvgTransformIdentity(abs_xform);

    // Collect parent chain (bottom-up)
    std::vector<cssboxElement*> chain;
    cssboxElement* curr = element;
    while (curr) {
        chain.push_back(curr);
        curr = cssboxGetParent(renderer, curr);  // Use safe accessor
    }

    // Apply transforms from root to element (top-down)
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        cssboxElement* elem = *it;

        // First apply element's own transform
        nvgTransformMultiply(abs_xform, elem->transform);

        // Apply SVG viewBox transform if present (affects children)
        if (it != chain.rbegin() && elem->type == "svg") {
            auto viewbox_it = elem->inline_style.find("viewBox");
            if (viewbox_it != elem->inline_style.end()) {
                float vb_x, vb_y, vb_w, vb_h;
                if (sscanf(viewbox_it->second.c_str(), "%f %f %f %f", &vb_x, &vb_y, &vb_w, &vb_h) == 4
                    && vb_w > 0 && vb_h > 0) {
                    // Get SVG container size from layout
                    float svg_width = elem->layout.width;
                    float svg_height = elem->layout.height;
                    if (svg_width > 0 && svg_height > 0) {
                        // Calculate scale
                        float scale_x = svg_width / vb_w;
                        float scale_y = svg_height / vb_h;

                        // Apply viewBox transform: translate → scale → remove viewBox offset
                        float viewbox_xform[6];
                        nvgTransformIdentity(viewbox_xform);

                        // Step 1: Translate to SVG container position
                        float translate1[6];
                        nvgTransformTranslate(translate1, elem->layout.x, elem->layout.y);
                        nvgTransformMultiply(viewbox_xform, translate1);

                        // Step 2: Scale from viewBox to container size
                        float scale[6];
                        nvgTransformScale(scale, scale_x, scale_y);
                        nvgTransformMultiply(viewbox_xform, scale);

                        // Step 3: Remove viewBox offset
                        float translate2[6];
                        nvgTransformTranslate(translate2, -vb_x, -vb_y);
                        nvgTransformMultiply(viewbox_xform, translate2);

                        // Apply to absolute transform
                        nvgTransformMultiply(abs_xform, viewbox_xform);
                    }
                }
            }
        }

        // Then apply scroll offset if this is a scroll container
        // (scroll affects children, so apply AFTER this element's transform)
        if (it != chain.rbegin() && (elem->scroll_x != 0.0f || elem->scroll_y != 0.0f)) {
            // Only apply scroll to descendants, not to the scroll container itself
            float scroll_xform[6];
            nvgTransformTranslate(scroll_xform, -elem->scroll_x, -elem->scroll_y);
            nvgTransformMultiply(abs_xform, scroll_xform);
        }
    }
}

// ============================================================================
// Retain Mode: Transform Cache Helpers
// ============================================================================

/**
 * @brief Invalidate cached absolute transform for element and all descendants
 */
static void invalidate_abs_transform_tree(cssboxRenderer* renderer, cssboxElement* element) {
    if (!element->abs_transform_valid_) return;  // Already invalid

    element->abs_transform_valid_ = false;

    // Invalidate all descendants
    for (int child_id : element->children_internal_ids) {
        auto it = renderer->elements.find(child_id);
        if (it != renderer->elements.end()) {
            invalidate_abs_transform_tree(renderer, it->second.get());
        }
    }
}

/**
 * @brief Invalidate all cached transforms in the renderer
 */
static void invalidate_all_transforms(cssboxRenderer* renderer) {
    for (auto& [id, elem] : renderer->elements) {
        elem->abs_transform_valid_ = false;
    }
}

/**
 * @brief Ensure element has valid cached absolute transform (lazy computation)
 */
static void ensure_abs_transform(cssboxRenderer* renderer, cssboxElement* element) {
    if (element->abs_transform_valid_) return;  // Cache hit

    // Compute using existing logic
    compute_absolute_transform(renderer, element, element->cached_abs_transform_);
    element->abs_transform_valid_ = true;
}

/**
 * @brief Rebuild the render list (collect + sort)
 */
static void rebuild_render_list(cssboxRenderer* renderer) {
    std::vector<std::pair<cssboxElement*, RenderOrder>> elements;
    int tree_order = 0;

    for (auto* root : renderer->root_elements) {
        collect_elements_for_render(renderer, root, elements, tree_order);
    }

    // Sort by render order
    std::sort(elements.begin(), elements.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

    // Extract sorted element pointers to render_list_
    renderer->render_list_.clear();
    renderer->render_list_.reserve(elements.size());
    for (const auto& [elem, order] : elements) {
        renderer->render_list_.push_back(elem);
    }
}

// ============================================================================
// Constructor Implementations
// ============================================================================

// ============================================================================
// cssboxRenderer Implementation
// ============================================================================

cssboxRenderer::cssboxRenderer(NVGcontext* vg)
    : vg(vg),
      arena_(256 * 1024),  // 256KB arena (grows on demand)
      transition_pool_(arena_),
      animation_pool_(arena_) {
    stylesheet = std::make_unique<cssbox::lexbor::EnhancedStyleSheet>();
    painter = std::make_unique<cssboxPainter>(vg, this);
    current_time = 0.0f;
    frame_mark_ = 0;
}

cssboxRenderer::~cssboxRenderer() {
    // Clean up transition and animation states using object pools
    for (auto& [id, element] : elements) {
        if (element->transition_state) {
            transition_pool_.deallocate(static_cast<TransitionState*>(element->transition_state));
            element->transition_state = nullptr;
        }
        if (element->animation_state) {
            animation_pool_.deallocate(static_cast<AnimationState*>(element->animation_state));
            element->animation_state = nullptr;
        }
    }
}

// ============================================================================
// C API Implementation
// ============================================================================

cssboxRenderer* cssboxCreateRenderer(NVGcontext* vg) {
    CSSBOX_MEM_CHECKPOINT("before_renderer_create");
    auto* renderer = new cssboxRenderer(vg);
    CSSBOX_MEM_CHECKPOINT("after_renderer_create");
    logi("[MEM] Renderer created: {:.2f} MB", cssbox::get_process_memory_mb());
    return renderer;
}

void cssboxDeleteRenderer(cssboxRenderer* renderer) {
    CSSBOX_MEM_CHECKPOINT("before_renderer_delete");
    delete renderer;
    CSSBOX_MEM_CHECKPOINT("after_renderer_delete");
}

// CSS Management

int cssboxParseCSS(cssboxRenderer* renderer, const char* css) {
    CSSBOX_MEM_CHECKPOINT("before_css_parse");
    bool success = renderer->stylesheet->parse_css(css);
    if (success) {
        cssboxMarkAllDirty(renderer, cssbox_DIRTY_STYLE);
    }
    CSSBOX_MEM_CHECKPOINT("after_css_parse");
    logi("[MEM] CSS parsed: {:.2f} MB (rules: {})",
         cssbox::get_process_memory_mb(),
         renderer->stylesheet->get_rules().size());
    return success ? 1 : 0;
}

void cssboxClearCSS(cssboxRenderer* renderer) {
    renderer->stylesheet = std::make_unique<cssbox::lexbor::EnhancedStyleSheet>();
    renderer->css_gradient_cache_.clear();  // Clear cached gradients
    renderer->style_dirty = true;
}

void cssboxSetVariable(cssboxRenderer* renderer,
                       const char* name,
                       const char* value) {
    renderer->stylesheet->set_variable(name, value);
    renderer->style_dirty = true;
}

// Element Management

cssboxElement* cssboxCreateElement(cssboxRenderer* renderer,
                                   const char* id,
                                   const char* type) {
    auto element = std::make_unique<cssboxElement>();

    // Assign unique internal_id
    element->internal_id = renderer->next_internal_id++;
    element->id = id ? id : "";  // User-provided id (optional)
    element->type = type;
    element->parent_internal_id = -1;  // -1 = root element
    element->visible = true;
    element->opacity = 1.0f;
    element->user_data = nullptr;
    element->transition_state = nullptr;  // Phase 4: Will be created when transition is specified

    // Sprint 30: Initialize child position fields
    element->child_index = 0;
    element->total_siblings = 1;  // Will be updated when added to parent

    // Initialize transform to identity
    nvgTransformIdentity(element->transform);
    
    // Phase 3: Mark new element as dirty (needs style computation and layout)
    element->dirty_flags = cssbox::DIRTY_ALL;

    cssboxElement* ptr = element.get();
    int internal_id = element->internal_id;

    // Store element by internal_id
    renderer->elements[internal_id] = std::move(element);

    // Map user id to internal_id (if user provided an id)
    if (id && id[0] != '\0') {
        renderer->id_to_internal_id[id] = internal_id;
    }

    renderer->root_elements.push_back(ptr);
    renderer->layout_dirty = true;
    renderer->render_list_dirty_ = true;  // Retain mode: tree changed

    return ptr;
}

cssboxElement* cssboxCreateMarker(cssboxRenderer* renderer,
                                   const char* id,
                                   float markerWidth,
                                   float markerHeight,
                                   float refX,
                                   float refY,
                                   const char* orient) {
    if (!renderer || !id) return nullptr;
    
    // Create marker definition
    cssboxMarker marker;
    marker.id = id;
    marker.markerWidth = markerWidth;
    marker.markerHeight = markerHeight;
    marker.refX = refX;
    marker.refY = refY;
    marker.orient = orient ? orient : "0";
    
    // Store marker in registry
    renderer->markers_[id] = marker;
    
    // Create a marker element (container for marker content)
    cssboxElement* marker_element = cssboxCreateElement(renderer, id, "marker");
    
    return marker_element;
}

cssboxElement* cssboxCreateClipPath(cssboxRenderer* renderer, const char* id) {
    if (!renderer || !id) return nullptr;
    
    // Create clip path definition
    cssboxClipPath clip_path;
    clip_path.id = id;
    
    // Store in registry
    renderer->clip_paths_[id] = clip_path;
    
    // Create clip path element (container for clip shapes)
    cssboxElement* clip_element = cssboxCreateElement(renderer, id, "clipPath");
    
    return clip_element;
}

cssboxElement* cssboxCreatePattern(cssboxRenderer* renderer,
                                   const char* id,
                                   float x, float y,
                                   float width, float height) {
    if (!renderer || !id) return nullptr;
    
    // Create pattern definition
    cssboxPattern pattern;
    pattern.id = id;
    pattern.x = x;
    pattern.y = y;
    pattern.width = width;
    pattern.height = height;
    
    // Store in registry
    renderer->patterns_[id] = pattern;
    
    // Create pattern element (container for pattern content)
    cssboxElement* pattern_element = cssboxCreateElement(renderer, id, "pattern");
    
    return pattern_element;
}

/**
 * @brief Internal helper: Get element by internal_id
 *
 * This is the CORE lookup function used throughout the codebase.
 * All tree operations (parent/child lookups) use this instead of user id.
 *
 * @param renderer The renderer
 * @param internal_id Unique internal identifier
 * @return Element pointer or nullptr if not found
 */
static cssboxElement* get_element_by_internal_id(cssboxRenderer* renderer, int internal_id) {
    auto it = renderer->elements.find(internal_id);
    if (it != renderer->elements.end()) {
        return it->second.get();
    }
    return nullptr;
}

// Public API: Get element by user-provided id (for backward compatibility)
cssboxElement* cssboxGetElement(cssboxRenderer* renderer, const char* id) {
    if (!id || id[0] == '\0') {
        return nullptr;  // Empty id
    }

    auto it = renderer->id_to_internal_id.find(id);
    if (it != renderer->id_to_internal_id.end()) {
        return get_element_by_internal_id(renderer, it->second);
    }
    return nullptr;
}

cssboxElement* cssboxGetParent(cssboxRenderer* renderer, const cssboxElement* element) {
    if (!renderer || !element || element->parent_internal_id < 0) {
        return nullptr;
    }
    return get_element_by_internal_id(renderer, element->parent_internal_id);
}

cssboxElement** cssboxGetChildren(cssboxRenderer* renderer,
                                  const cssboxElement* element,
                                  int* out_count) {
    // Static storage for returned pointers (valid until next call)
    static thread_local std::vector<cssboxElement*> children_cache;
    children_cache.clear();

    if (!renderer || !element || !out_count) {
        if (out_count) *out_count = 0;
        return nullptr;
    }

    for (int child_internal_id : element->children_internal_ids) {
        cssboxElement* child = get_element_by_internal_id(renderer, child_internal_id);
        if (child) {
            children_cache.push_back(child);
        }
    }

    *out_count = static_cast<int>(children_cache.size());
    return children_cache.empty() ? nullptr : children_cache.data();
}

void cssboxDeleteElement(cssboxRenderer* renderer, const char* id) {
    // Look up internal_id by user id
    auto id_it = renderer->id_to_internal_id.find(id);
    if (id_it == renderer->id_to_internal_id.end()) {
        return;  // Element not found
    }

    int internal_id = id_it->second;
    auto it = renderer->elements.find(internal_id);
    if (it != renderer->elements.end()) {
        cssboxElement* element = it->second.get();

        // Remove from root elements
        auto root_it = std::find(renderer->root_elements.begin(),
                                 renderer->root_elements.end(),
                                 element);
        if (root_it != renderer->root_elements.end()) {
            renderer->root_elements.erase(root_it);
        }

        // Remove from parent's children list
        if (element->parent_internal_id >= 0) {
            cssboxElement* parent = get_element_by_internal_id(renderer, element->parent_internal_id);
            if (parent) {
                auto& children_ids = parent->children_internal_ids;
                auto child_it = std::find(children_ids.begin(), children_ids.end(), internal_id);
                if (child_it != children_ids.end()) {
                    children_ids.erase(child_it);
                }
            }
        }

        // Remove from id_to_internal_id map (if element has user id)
        if (!element->id.empty()) {
            renderer->id_to_internal_id.erase(element->id);
        }

        renderer->elements.erase(it);
        renderer->layout_dirty = true;
        renderer->render_list_dirty_ = true;  // Retain mode: tree changed
    }
}

void cssboxClearElements(cssboxRenderer* renderer) {
    if (!renderer) return;
    renderer->elements.clear();
    renderer->id_to_internal_id.clear();
    renderer->root_elements.clear();
    renderer->render_list_.clear();  // Retain mode: clear cached list
    renderer->next_internal_id = 1;  // Reset ID counter
    renderer->layout_dirty = true;
    renderer->style_dirty = true;
    renderer->render_list_dirty_ = true;  // Retain mode: tree changed
}

void cssboxShrinkMemory(cssboxRenderer* renderer) {
    if (!renderer) return;

    // Clear style cache (will be rebuilt on demand)
    renderer->stylesheet->clear_cache();

    // Clear gradient caches
    renderer->css_gradient_cache_.clear();

    // Clear pattern FBOs that aren't actively used
    for (auto& [id, pattern] : renderer->patterns_) {
        if (pattern.image_handle >= 0) {
            nvgDeleteImage(renderer->vg, pattern.image_handle);
            pattern.image_handle = -1;
            pattern.needs_update = true;
        }
    }

    // Shrink animated elements set
    renderer->animated_elements_.clear();

    // Force style recomputation
    renderer->style_dirty = true;
}

void cssboxAddClass(cssboxElement* element, const char* class_name) {
    auto it = std::find(element->classes.begin(), element->classes.end(), class_name);
    if (it == element->classes.end()) {
        element->classes.push_back(class_name);
        element->dirty_flags |= cssbox::DIRTY_STYLE | cssbox::DIRTY_LAYOUT;
    }
}

void cssboxRemoveClass(cssboxElement* element, const char* class_name) {
    auto it = std::find(element->classes.begin(), element->classes.end(), class_name);
    if (it != element->classes.end()) {
        element->classes.erase(it);
        element->dirty_flags |= cssbox::DIRTY_STYLE | cssbox::DIRTY_LAYOUT;
    }
}

void cssboxSetLineGeometry(cssboxElement* element,
                           float x1, float y1,
                           float x2, float y2) {
    if (!element) return;
    element->line_geometry.x1 = x1;
    element->line_geometry.y1 = y1;
    element->line_geometry.x2 = x2;
    element->line_geometry.y2 = y2;
    element->line_geometry.defined = true;
}

void cssboxClearLineGeometry(cssboxElement* element) {
    if (!element) return;
    element->line_geometry = cssboxLineGeometry();
}

void cssboxSetCircleGeometry(cssboxElement* element,
                             float cx, float cy,
                             float radius) {
    if (!element) return;
    element->circle_geometry.cx = cx;
    element->circle_geometry.cy = cy;
    element->circle_geometry.rx = radius;
    element->circle_geometry.ry = radius;
    element->circle_geometry.defined = true;
}

void cssboxSetEllipseGeometry(cssboxElement* element,
                              float cx, float cy,
                              float rx, float ry) {
    if (!element) return;
    element->circle_geometry.cx = cx;
    element->circle_geometry.cy = cy;
    element->circle_geometry.rx = rx;
    element->circle_geometry.ry = ry;
    element->circle_geometry.defined = true;
}

void cssboxClearCircleGeometry(cssboxElement* element) {
    if (!element) return;
    element->circle_geometry = cssboxCircleGeometry();
}

int cssboxHasClass(const cssboxElement* element, const char* class_name) {
    auto it = std::find(element->classes.begin(), element->classes.end(), class_name);
    return (it != element->classes.end()) ? 1 : 0;
}

// Text Content (Phase 3)

void cssboxSetText(cssboxElement* element, const char* text) {
    element->text_content = text;
}

const char* cssboxGetText(const cssboxElement* element) {
    return element->text_content.c_str();
}

void cssboxSetInlineStyle(cssboxRenderer* renderer, cssboxElement* element,
                          const char* property, const char* value) {
    if (!renderer || !element || !property || !value) return;

    std::string prop(property);
    std::string old_value;
    auto it = element->inline_style.find(prop);
    if (it != element->inline_style.end()) {
        old_value = it->second;
    }

    // Set the new value
    element->inline_style[prop] = value;

    // Check if value actually changed
    if (old_value == value) return;

    // Mark element dirty
    element->dirty_flags |= cssbox::DIRTY_STYLE;

    // Properties that affect render list
    if (prop == "display" || prop == "visibility") {
        renderer->render_list_dirty_ = true;
        renderer->layout_dirty = true;
    }
    // Properties that affect z-index ordering
    else if (prop == "z-index") {
        renderer->render_list_dirty_ = true;
    }
    // Properties that affect transform
    else if (prop == "transform") {
        element->abs_transform_valid_ = false;
        invalidate_abs_transform_tree(renderer, element);
    }
    // Layout-affecting properties
    else if (prop == "width" || prop == "height" || prop == "margin" ||
             prop == "padding" || prop == "position" || prop == "top" ||
             prop == "left" || prop == "right" || prop == "bottom" ||
             prop == "flex-grow" || prop == "flex-shrink" || prop == "flex-basis") {
        renderer->layout_dirty = true;
    }
}

void cssboxRemoveInlineStyle(cssboxRenderer* renderer, cssboxElement* element,
                             const char* property) {
    if (!renderer || !element || !property) return;

    std::string prop(property);
    auto it = element->inline_style.find(prop);
    if (it == element->inline_style.end()) return;  // Nothing to remove

    element->inline_style.erase(it);
    element->dirty_flags |= cssbox::DIRTY_STYLE;

    // Same invalidation logic as set
    if (prop == "display" || prop == "visibility") {
        renderer->render_list_dirty_ = true;
        renderer->layout_dirty = true;
    } else if (prop == "z-index") {
        renderer->render_list_dirty_ = true;
    } else if (prop == "transform") {
        element->abs_transform_valid_ = false;
        invalidate_abs_transform_tree(renderer, element);
    } else if (prop == "width" || prop == "height" || prop == "margin" ||
               prop == "padding" || prop == "position" || prop == "top" ||
               prop == "left" || prop == "right" || prop == "bottom" ||
               prop == "flex-grow" || prop == "flex-shrink" || prop == "flex-basis") {
        renderer->layout_dirty = true;
    }
}

// State Management

void cssboxSetPseudoState(cssboxElement* element,
                          const char* state,
                          int active) {
    if (!element || !state) return;

    bool changed = false;
    if (active) {
        changed = element->pseudo_states.insert(state).second;
    } else {
        changed = (element->pseudo_states.erase(state) > 0);
    }

    if (changed) {
        element->dirty_flags |= cssbox::DIRTY_STYLE;
    }
}

void cssboxSetPseudoStateEx(cssboxRenderer* renderer, cssboxElement* element,
                            const char* state, int active) {
    if (!renderer || !element || !state) return;

    bool changed = false;
    if (active) {
        changed = element->pseudo_states.insert(state).second;
    } else {
        changed = (element->pseudo_states.erase(state) > 0);
    }

    if (changed) {
        element->dirty_flags |= cssbox::DIRTY_STYLE;
        // Pseudo-state changes can affect display/visibility through CSS rules
        // e.g., :hover { display: none; }
        // Mark style and paint as dirty
        renderer->style_dirty = true;
        renderer->paint_dirty_ = true;
    }
}

int cssboxHasPseudoState(const cssboxElement* element, const char* state) {
    return element->pseudo_states.count(state) > 0 ? 1 : 0;
}

// Scroll Support

void cssboxSetScroll(cssboxElement* element, float scroll_x, float scroll_y) {
    if (!element) return;
    element->scroll_x = scroll_x;
    element->scroll_y = scroll_y;
}

void cssboxGetScroll(const cssboxElement* element, float* out_scroll_x, float* out_scroll_y) {
    if (!element) return;
    if (out_scroll_x) *out_scroll_x = element->scroll_x;
    if (out_scroll_y) *out_scroll_y = element->scroll_y;
}

void cssboxSetContentHeight(cssboxElement* element, float content_height) {
    if (!element) return;
    element->content_height = content_height;
}

float cssboxGetContentHeight(const cssboxElement* element) {
    if (!element) return 0.0f;
    return element->content_height;
}

// Tree Manipulation

// Sprint 30: Helper function to update child indices
static void update_child_indices(cssboxRenderer* renderer, cssboxElement* parent) {
    if (!parent || !renderer) return;

    int child_count = 0;
    cssboxElement** children = cssboxGetChildren(renderer, parent, &child_count);

    for (int i = 0; i < child_count; i++) {
        children[i]->child_index = i;
        children[i]->total_siblings = child_count;
    }
}

void cssboxAppendChild(cssboxRenderer* renderer, cssboxElement* parent, cssboxElement* child) {
    // Remove from previous parent if any
    if (child->parent_internal_id >= 0) {
        cssboxElement* old_parent = get_element_by_internal_id(renderer, child->parent_internal_id);
        if (old_parent) {
            auto& children_ids = old_parent->children_internal_ids;
            auto it = std::find(children_ids.begin(), children_ids.end(), child->internal_id);
            if (it != children_ids.end()) {
                children_ids.erase(it);
            }
            // Sprint 30: Update indices of old parent's children
            update_child_indices(renderer, old_parent);
        }
    }

    // Remove from root_elements since it's now a child
    auto root_it = std::find(renderer->root_elements.begin(),
                             renderer->root_elements.end(), child);
    if (root_it != renderer->root_elements.end()) {
        renderer->root_elements.erase(root_it);
    }

    // Add to new parent
    child->parent_internal_id = parent->internal_id;
    parent->children_internal_ids.push_back(child->internal_id);

    // Sprint 30: Update indices of new parent's children
    update_child_indices(renderer, parent);
    
    // SVG Markers: If parent is a marker, update marker registry
    if (parent->type == "marker" && !parent->id.empty()) {
        auto marker_it = renderer->markers_.find(parent->id);
        if (marker_it != renderer->markers_.end()) {
            marker_it->second.children_internal_ids = parent->children_internal_ids;
        }
    }
    
    // SVG ClipPaths: If parent is a clipPath, update clip path registry
    if (parent->type == "clipPath" && !parent->id.empty()) {
        auto clip_it = renderer->clip_paths_.find(parent->id);
        if (clip_it != renderer->clip_paths_.end()) {
            clip_it->second.children_internal_ids = parent->children_internal_ids;
        }
    }
    
    // SVG Patterns: If parent is a pattern, update pattern registry
    if (parent->type == "pattern" && !parent->id.empty()) {
        auto pattern_it = renderer->patterns_.find(parent->id);
        if (pattern_it != renderer->patterns_.end()) {
            pattern_it->second.children_internal_ids = parent->children_internal_ids;
            pattern_it->second.needs_update = true;  // Mark for re-rendering
        }
    }
    
    // Phase 3: Mark dirty
    parent->dirty_flags |= cssbox::DIRTY_CHILDREN | cssbox::DIRTY_LAYOUT;
    child->dirty_flags |= cssbox::DIRTY_ALL;
    renderer->render_list_dirty_ = true;  // Retain mode: tree changed
    invalidate_abs_transform_tree(renderer, child);  // Retain mode: transform chain changed
}

void cssboxRemoveChild(cssboxRenderer* renderer, cssboxElement* parent, cssboxElement* child) {
    if (!renderer || !parent || !child) return;

    auto& children_ids = parent->children_internal_ids;
    auto it = std::find(children_ids.begin(), children_ids.end(), child->internal_id);
    if (it != children_ids.end()) {
        children_ids.erase(it);
        child->parent_internal_id = -1;  // Back to root

        // Sprint 30: Update indices of remaining children
        update_child_indices(renderer, parent);

        // Phase 3: Mark parent dirty
        parent->dirty_flags |= cssbox::DIRTY_CHILDREN | cssbox::DIRTY_LAYOUT;
        renderer->render_list_dirty_ = true;  // Retain mode: tree changed
        invalidate_abs_transform_tree(renderer, child);  // Retain mode: transform chain changed
    }
}

// Rendering

void cssboxUpdate(cssboxRenderer* renderer, float delta_time) {
    // Update current time
    renderer->current_time += delta_time;

    // === Retain Mode: Early exit if nothing to update ===
    // Skip entire traversal if no animations running and no style changes pending
    if (renderer->animated_elements_.empty() && !renderer->style_dirty) {
        return;
    }

    // Helper function to get or create transition state
    auto get_transition_state = [renderer](cssboxElement* element) -> TransitionState* {
        if (!element->transition_state) {
            element->transition_state = renderer->transition_pool_.allocate();
        }
        return static_cast<TransitionState*>(element->transition_state);
    };

    // Update all elements
    std::function<void(cssboxElement*)> update_element = [&](cssboxElement* element) {
        if (!element->visible) return;

        // === Retain Mode Optimization ===
        // Check if element needs style update:
        // 1. Has DIRTY_STYLE flag (style inputs changed)
        // 2. Has active transition (needs change detection)
        // 3. Has active animation (needs interpolation)
        bool needs_style_update = (element->dirty_flags & cssbox::DIRTY_STYLE) != 0;
        bool has_active_transition = false;
        bool has_active_animation = false;

        if (element->transition_state) {
            TransitionState* ts = static_cast<TransitionState*>(element->transition_state);
            for (const auto& [prop, trans] : ts->active_transitions) {
                if (trans.active) {
                    has_active_transition = true;
                    break;
                }
            }
        }
        if (element->animation_state) {
            AnimationState* as = static_cast<AnimationState*>(element->animation_state);
            for (const auto& anim : as->running_animations) {
                if (anim.active) {
                    has_active_animation = true;
                    break;
                }
            }
        }

        // Skip style computation for static elements
        if (!needs_style_update && !has_active_transition && !has_active_animation) {
            // Still recurse to children (they might need updates)
            for (int child_id : element->children_internal_ids) {
                auto child_it = renderer->elements.find(child_id);
                if (child_it != renderer->elements.end()) {
                    update_element(child_it->second.get());
                }
            }
            return;
        }

        TransitionState* trans_state = nullptr;

        // Compute typed style
        cssbox::Background old_background = element->style.background;

        element->style = renderer->stylesheet->compute_style_typed(
            element->id,
            element->type,
            element->classes,
            element->attributes,
            element->pseudo_states,
            element->inline_style,
            nullptr,
            element->child_index,
            element->total_siblings
        );

        // Preserve manually-set background
        bool css_has_no_background = (element->style.background.type == cssbox::BackgroundType::NONE ||
                                      (element->style.background.type == cssbox::BackgroundType::COLOR &&
                                       element->style.background.color.a == 0.0f));
        bool had_manual_color = (old_background.type == cssbox::BackgroundType::COLOR ||
                                 old_background.color.a > 0.0f);

        if (css_has_no_background && had_manual_color) {
            element->style.background = old_background;
            if (element->style.background.type == cssbox::BackgroundType::NONE) {
                element->style.background.type = cssbox::BackgroundType::COLOR;
            }
        }

        // Clear DIRTY_STYLE since we just computed it
        element->dirty_flags &= ~cssbox::DIRTY_STYLE;

        // Get string-based style for transition/animation system
        auto computed_style = renderer->stylesheet->compute_style(
            element->id,
            element->type,
            element->classes,
            element->attributes,
            element->pseudo_states,
            element->inline_style, {},
            element->child_index, element->total_siblings
        );

        // Check for transition specification
        auto transition_it = computed_style.find("transition");
        if (transition_it != computed_style.end()) {
            if (!trans_state) trans_state = get_transition_state(element);

            // Parse transition specification
            std::string prop;
            float duration;
            EasingFunction easing;
            if (cssbox_utils::parse_transition(transition_it->second, prop, duration, easing)) {
                trans_state->transition_property = prop;
                trans_state->transition_duration = duration;
                trans_state->transition_easing = easing;
            }

            // Check for property changes and start transitions
            for (const auto& [property, value] : computed_style) {
                if (property == "transition") continue;  // Skip transition property itself

                // Check if this property should be transitioned
                bool should_transition = (trans_state->transition_property == "all" ||
                                        trans_state->transition_property == property);

                if (should_transition && trans_state->transition_duration > 0.0f) {
                    // Skip if transition is already active for this property
                    auto active_it = trans_state->active_transitions.find(property);
                    bool has_active_transition = (active_it != trans_state->active_transitions.end() &&
                                                 active_it->second.active);

                    if (!has_active_transition) {
                        // Check if value changed
                        auto prev_it = trans_state->previous_values.find(property);

                        if (prev_it != trans_state->previous_values.end() && prev_it->second != value) {
                            // Value changed - start transition
                            Transition trans;
                            trans.property = property;
                            trans.start_time = renderer->current_time;
                            trans.duration = trans_state->transition_duration;
                            trans.easing = trans_state->transition_easing;
                            trans.start_value = prev_it->second;  // Use previous value as start
                            trans.end_value = value;
                            trans.active = true;

                            trans_state->active_transitions[property] = trans;
                            has_active_transition = true;  // Now we have an active transition

                            // Retain Mode: Track animated element
                            renderer->animated_elements_.insert(element->internal_id);
                        }

                        // Store current value for next frame (only if no transition started)
                        if (!has_active_transition) {
                            trans_state->previous_values[property] = value;
                        }
                    }
                }
            }

            // Update active transitions
            for (auto it = trans_state->active_transitions.begin();
                 it != trans_state->active_transitions.end(); ) {
                Transition& trans = it->second;

                if (!trans.active) {
                    ++it;
                    continue;
                }

                // Calculate progress
                float elapsed = renderer->current_time - trans.start_time;
                float t = elapsed / trans.duration;

                if (t >= 1.0f) {
                    // Transition complete
                    trans.active = false;
                    // Update previous_values to final value
                    trans_state->previous_values[trans.property] = trans.end_value;
                    ++it;
                } else {
                    // Interpolate
                    float eased_t = cssbox_utils::apply_easing(t, trans.easing);
                    std::string interpolated = cssbox_utils::interpolate_value(
                        trans.start_value,
                        trans.end_value,
                        eased_t
                    );

                    // Apply interpolated value as inline style (temporary, for this frame)
                    element->inline_style[trans.property] = interpolated;
                    ++it;
                }
            }
        }

        // ====================================================================
        // Phase 4 Sprint 2: Update Keyframe Animations
        // ====================================================================

        // Helper function to get or create animation state
        auto get_animation_state = [renderer](cssboxElement* elem) -> AnimationState* {
            if (!elem->animation_state) {
                elem->animation_state = renderer->animation_pool_.allocate();
            }
            return static_cast<AnimationState*>(elem->animation_state);
        };

        // Check for animation-name property
        auto anim_name_it = computed_style.find("animation-name");
        if (anim_name_it != computed_style.end() && !anim_name_it->second.empty()) {
            AnimationState* anim_state = get_animation_state(element);

            // Check if we need to start a new animation
            bool animation_exists = false;
            for (const auto& anim : anim_state->running_animations) {
                if (anim.animation_name == anim_name_it->second) {
                    animation_exists = true;
                    break;
                }
            }

            // Start animation if it doesn't exist
            if (!animation_exists) {
                RunningAnimation new_anim = parse_animation_from_style(computed_style, renderer->current_time);
                if (new_anim.active) {
                    logi("[ANIMATION] Starting animation '{}' on element id='{}' duration={}s iterations={}",
                         new_anim.animation_name, element->id, new_anim.duration, new_anim.iteration_count);
                    anim_state->running_animations.push_back(new_anim);

                    // Retain Mode: Track animated element
                    renderer->animated_elements_.insert(element->internal_id);
                }
            }
        }

        // Update active animations
        if (element->animation_state) {
            AnimationState* anim_state = static_cast<AnimationState*>(element->animation_state);

            for (auto& anim : anim_state->running_animations) {
                if (!anim.active) {
                    // For fill-mode forwards/both, we still apply final properties
                    if (anim.fill_mode == "forwards" || anim.fill_mode == "both") {
                        // Apply final keyframe properties
                        const KeyframeAnimation* kf_anim =
                            renderer->stylesheet->get_keyframe_animation(anim.animation_name);
                        if (kf_anim) {
                            auto props = kf_anim->get_properties_at(1.0f);  // Final position
                            for (const auto& [prop, value] : props) {
                                element->inline_style[prop] = value;
                            }
                        }
                    }
                    continue;
                }

                // Calculate elapsed time
                float elapsed = renderer->current_time - anim.start_time - anim.delay;
                if (elapsed < 0) continue;  // Still in delay period

                // Calculate position in animation [0, 1]
                float t = elapsed / anim.duration;

                // Handle iteration
                int iteration = (int)t;
                float position = t - iteration;  // Fractional part

                // Check if animation finished
                if (iteration >= anim.iteration_count && anim.iteration_count != -1) {
                    anim.active = false;

                    // Apply fill-mode
                    if (anim.fill_mode == "forwards" || anim.fill_mode == "both") {
                        position = 1.0f;  // Stay at end
                    } else {
                        continue;  // Don't apply any properties
                    }
                }

                // Handle direction
                if (anim.direction == "reverse") {
                    position = 1.0f - position;
                } else if (anim.direction == "alternate") {
                    if (iteration % 2 == 1) position = 1.0f - position;
                } else if (anim.direction == "alternate-reverse") {
                    if (iteration % 2 == 0) position = 1.0f - position;
                }

                // Apply easing
                float eased_position = cssbox_utils::apply_easing(position, anim.easing);

                // Get interpolated properties from keyframe animation
                const KeyframeAnimation* kf_anim =
                    renderer->stylesheet->get_keyframe_animation(anim.animation_name);
                if (kf_anim) {
                    auto props = kf_anim->get_properties_at(eased_position);

                    // Apply to element inline style (for this frame)
                    for (const auto& [prop, value] : props) {
                        element->inline_style[prop] = value;
                    }
                } else {
                    logw("[ANIMATION] Keyframe animation '{}' not found for element id='{}'",
                         anim.animation_name, element->id);
                }

                // Update iteration count
                if (anim.active) {
                    anim.current_iteration = iteration;
                }
            }

            // Remove finished animations (if not infinite and fill-mode is none)
            anim_state->running_animations.erase(
                std::remove_if(anim_state->running_animations.begin(),
                             anim_state->running_animations.end(),
                             [](const RunningAnimation& a) {
                                 return !a.active && a.fill_mode == "none";
                             }),
                anim_state->running_animations.end()
            );
        }


        // Recursively update children (avoid cssboxGetChildren to skip vector copy)
        for (int child_id : element->children_internal_ids) {
            auto child_it = renderer->elements.find(child_id);
            if (child_it != renderer->elements.end()) {
                update_element(child_it->second.get());
            }
        }
    };

    // Update all root elements
    for (auto* root : renderer->root_elements) {
        update_element(root);
    }

    // Retain Mode: Clean up animated_elements_ - remove elements with no active animations
    // Only iterate over animated_elements_, not all elements
    for (auto it = renderer->animated_elements_.begin(); it != renderer->animated_elements_.end(); ) {
        int internal_id = *it;
        auto elem_it = renderer->elements.find(internal_id);
        if (elem_it == renderer->elements.end()) {
            // Element was deleted
            it = renderer->animated_elements_.erase(it);
            continue;
        }

        cssboxElement* element = elem_it->second.get();
        bool still_active = false;

        // Check transitions
        if (element->transition_state) {
            TransitionState* trans_state = static_cast<TransitionState*>(element->transition_state);
            for (const auto& [prop, trans] : trans_state->active_transitions) {
                if (trans.active) {
                    still_active = true;
                    break;
                }
            }
        }

        // Check animations
        if (!still_active && element->animation_state) {
            AnimationState* anim_state = static_cast<AnimationState*>(element->animation_state);
            for (const auto& anim : anim_state->running_animations) {
                if (anim.active || anim.fill_mode == "forwards" || anim.fill_mode == "both") {
                    still_active = true;
                    break;
                }
            }
        }

        if (!still_active) {
            it = renderer->animated_elements_.erase(it);
        } else {
            ++it;
        }
    }

    if (!renderer->animated_elements_.empty()) {
        renderer->layout_dirty = true;
    }
}

// ============================================================================
// Dirty Flags API (60fps Optimization)
// ============================================================================

void cssboxMarkDirty(cssboxElement* element, int flags) {
    if (!element) return;

    // Map public flags to internal flags
    if (flags & cssbox_DIRTY_STYLE)     element->dirty_flags |= cssbox::DIRTY_STYLE;
    if (flags & cssbox_DIRTY_LAYOUT)    element->dirty_flags |= cssbox::DIRTY_LAYOUT;
    if (flags & cssbox_DIRTY_TRANSFORM) element->dirty_flags |= cssbox::DIRTY_TRANSFORM;
    if (flags & cssbox_DIRTY_CHILDREN)  element->dirty_flags |= cssbox::DIRTY_CHILDREN;
}

void cssboxMarkAllDirty(cssboxRenderer* renderer, int flags) {
    if (!renderer) return;

    for (auto& [id, element] : renderer->elements) {
        cssboxMarkDirty(element.get(), flags);
    }

    // Also mark renderer-level flags
    if (flags & cssbox_DIRTY_LAYOUT) renderer->layout_dirty = true;
    if (flags & cssbox_DIRTY_STYLE)  renderer->style_dirty = true;
}

int cssboxIsDirty(const cssboxElement* element, int flags) {
    if (!element) return 0;

    // Check if any requested flag is set
    if ((flags & cssbox_DIRTY_STYLE)     && (element->dirty_flags & cssbox::DIRTY_STYLE))     return 1;
    if ((flags & cssbox_DIRTY_LAYOUT)    && (element->dirty_flags & cssbox::DIRTY_LAYOUT))    return 1;
    if ((flags & cssbox_DIRTY_TRANSFORM) && (element->dirty_flags & cssbox::DIRTY_TRANSFORM)) return 1;
    if ((flags & cssbox_DIRTY_CHILDREN)  && (element->dirty_flags & cssbox::DIRTY_CHILDREN))  return 1;

    return 0;
}

void cssboxPreparePatterns(cssboxRenderer* renderer) {
    if (!renderer || !renderer->vg || !renderer->painter) return;

    // Pre-render dirty patterns before the main NVG frame
    // This is necessary because pattern rendering uses its own nvgBeginFrame/nvgEndFrame
    renderer->painter->prepare_patterns();
}

void cssboxRender(cssboxRenderer* renderer) {
    // Phase 1: Compute layout if dirty
    if (renderer->layout_dirty || renderer->style_dirty) {
        cssboxComputeLayout(renderer);
        // Layout changes invalidate all cached transforms
        invalidate_all_transforms(renderer);
        // Layout changes also invalidate render list (positions may affect clipping)
        renderer->render_list_dirty_ = true;
        // Layout/style changes require repaint
        renderer->paint_dirty_ = true;
    }

    // Phase 2: Rebuild render list if dirty
    if (renderer->render_list_dirty_) {
        rebuild_render_list(renderer);
        renderer->render_list_dirty_ = false;
        // Render list changes require repaint
        renderer->paint_dirty_ = true;
    }

    // === Retain Mode: Skip painting if nothing changed ===
    // This is the key optimization - when UI is static, GPU usage drops to near zero
    bool has_animations = !renderer->animated_elements_.empty();
    if (!renderer->paint_dirty_ && !has_animations) {
        return;  // Nothing to paint, skip entire render loop
    }

    // Phase 3: Apply animation overrides (ONLY for animated elements)
    for (int internal_id : renderer->animated_elements_) {
        auto it = renderer->elements.find(internal_id);
        if (it == renderer->elements.end()) continue;
        cssboxElement* element = it->second.get();

        // Check inline_style for animation overrides
        auto border_radius_it = element->inline_style.find("border-radius");
        if (border_radius_it != element->inline_style.end()) {
            float radius = cssbox_utils::parse_length(border_radius_it->second, 100.0f);
            for (int i = 0; i < 4; ++i) {
                element->layout.radius[i] = radius;
            }
        }

        // Apply transform from inline_style (set by animations)
        auto transform_it = element->inline_style.find("transform");
        if (transform_it != element->inline_style.end()) {
            const std::string& transform_str = transform_it->second;

            // Reset to identity
            element->transform[0] = 1.0f; element->transform[1] = 0.0f;
            element->transform[2] = 0.0f; element->transform[3] = 1.0f;
            element->transform[4] = 0.0f; element->transform[5] = 0.0f;

            // Parse and apply transform
            if (transform_str.find("rotate") != std::string::npos) {
                size_t start = transform_str.find('(');
                size_t end = transform_str.find("deg");
                if (start != std::string::npos && end != std::string::npos) {
                    float angle_deg = std::stof(transform_str.substr(start + 1, end - start - 1));
                    float angle_rad = angle_deg * (M_PI / 180.0f);
                    element->transform[0] = std::cos(angle_rad);
                    element->transform[1] = std::sin(angle_rad);
                    element->transform[2] = -std::sin(angle_rad);
                    element->transform[3] = std::cos(angle_rad);
                }
            } else if (transform_str.find("scale") != std::string::npos) {
                size_t start = transform_str.find('(');
                size_t end = transform_str.find(')');
                if (start != std::string::npos && end != std::string::npos) {
                    float scale = std::stof(transform_str.substr(start + 1, end - start - 1));
                    element->transform[0] = scale;
                    element->transform[3] = scale;
                }
            } else if (transform_str.find("translateY") != std::string::npos) {
                size_t start = transform_str.find('(');
                size_t end = transform_str.find("px");
                if (start != std::string::npos && end != std::string::npos) {
                    element->transform[5] = std::stof(transform_str.substr(start + 1, end - start - 1));
                }
            } else if (transform_str.find("translateX") != std::string::npos) {
                size_t start = transform_str.find('(');
                size_t end = transform_str.find("px");
                if (start != std::string::npos && end != std::string::npos) {
                    element->transform[4] = std::stof(transform_str.substr(start + 1, end - start - 1));
                }
            }

            // Transform changed, invalidate cached abs_transform
            element->abs_transform_valid_ = false;
        }
    }

    // Phase 5: Render using cached list and transforms
    for (cssboxElement* element : renderer->render_list_) {
        // Lazy compute absolute transform
        ensure_abs_transform(renderer, element);

        // Use cached transform directly
        float original_transform[6];
        memcpy(original_transform, element->transform, sizeof(float) * 6);
        memcpy(element->transform, element->cached_abs_transform_, sizeof(float) * 6);

        renderer->painter->paint_element(element);

        memcpy(element->transform, original_transform, sizeof(float) * 6);
    }

    // Clear paint dirty flag after successful paint
    renderer->paint_dirty_ = false;
}

int cssboxNeedsPaint(cssboxRenderer* renderer) {
    if (!renderer) return 0;

    // Need paint if any dirty flag is set
    if (renderer->layout_dirty || renderer->style_dirty ||
        renderer->render_list_dirty_ || renderer->paint_dirty_) {
        return 1;
    }

    // Need paint if there are active animations
    if (!renderer->animated_elements_.empty()) {
        return 1;
    }

    return 0;  // Static UI, can skip frame
}

void cssboxInvalidatePaint(cssboxRenderer* renderer) {
    if (!renderer) return;
    renderer->paint_dirty_ = true;
}

void cssboxRenderElement(cssboxRenderer* renderer, const char* id) {
    cssboxElement* element = cssboxGetElement(renderer, id);
    if (!element) return;

    // Recursively render element tree
    std::function<void(cssboxElement*)> render_tree = [&](cssboxElement* elem) {
        if (!elem->visible) return;
        if (elem->style.display == cssbox::Display::NONE) return;

        // Sprint 22: Check if overflow clipping is needed (use typed Overflow enum)
        bool has_overflow_clip = (elem->style.overflow_x != cssbox::Overflow::VISIBLE ||
                                  elem->style.overflow_y != cssbox::Overflow::VISIBLE);

        // Check if this element has scroll offset
        bool has_scroll = (elem->scroll_x != 0.0f || elem->scroll_y != 0.0f);

        // Apply scissor clipping if overflow is hidden/scroll/auto
        if (has_overflow_clip || has_scroll) {
            nvgSave(renderer->painter->get_context());
            nvgScissor(renderer->painter->get_context(),
                      elem->layout.x,
                      elem->layout.y,
                      elem->layout.width,
                      elem->layout.height);
        }

        // Paint this element (uses elem->style, already computed in layout phase)
        renderer->painter->paint_element(elem);

        // Apply scroll offset before rendering children
        if (has_scroll) {
            nvgTranslate(renderer->painter->get_context(), -elem->scroll_x, -elem->scroll_y);
        }

        // Render children (avoid cssboxGetChildren to skip vector copy)
        for (int child_id : elem->children_internal_ids) {
            auto child_it = renderer->elements.find(child_id);
            if (child_it != renderer->elements.end()) {
                render_tree(child_it->second.get());
            }
        }

        // Restore state if we set scissor or scroll
        if (has_overflow_clip || has_scroll) {
            nvgRestore(renderer->painter->get_context());
        }
    };

    render_tree(element);
}

void cssboxComputeLayout(cssboxRenderer* renderer) {
    CSSBOX_MEM_CHECKPOINT("layout_start");

    // Sync viewport dimensions to stylesheet for @media queries
    bool viewport_changed = renderer->stylesheet->set_viewport(renderer->viewport_width, renderer->viewport_height);

    // If viewport changed, styles must be recomputed (media queries may now match differently)
    if (viewport_changed) {
        renderer->style_dirty = true;
    }

    // Ensure styles are up-to-date before layout
    if (renderer->style_dirty) {
        std::function<void(cssboxElement*)> update_style = [&](cssboxElement* element) {
            if (!element->visible) return;

            // DEBUG: Log classes and inline styles for page elements
            // DEBUG: Log SVG elements inline_style
            if (element->type == "svg" || element->type == "circle" || element->type == "rect") {
                std::string class_list;
                for (const auto& cls : element->classes) {
                    class_list += "'" + cls + "' ";
                }
                // auto display_it = element->inline_style.find("display");
                // std::string inline_display = (display_it != element->inline_style.end())
                //     ? display_it->second : "(not set)";
                // logi("[LAYOUT] SVG Element id='{}' type='{}' classes=[{}] inline_style[display]='{}'",
                //      element->id, element->type, class_list, inline_display);
            }

            if (element->id.find("page-") != std::string::npos) {
                std::string class_list;
                for (const auto& cls : element->classes) {
                    class_list += "'" + cls + "' ";
                }
                auto display_it = element->inline_style.find("display");
                std::string inline_display = (display_it != element->inline_style.end())
                    ? display_it->second : "(not set)";
                logi("[LAYOUT] Element id='{}' classes=[{}] inline_style[display]='{}'",
                     element->id, class_list, inline_display);
            }

            // Compute typed style
            element->style = renderer->stylesheet->compute_style_typed(
                element->id,
                element->type,
                element->classes,
                element->attributes,
                element->pseudo_states,
                element->inline_style,
                nullptr,  // TODO: parent style
                element->child_index,
                element->total_siblings
            );

            // DEBUG: Log computed display and flex-grow for key elements
            if (element->id == "content" || element->id == "root" || element->id == "header" ||
                element->id.find("page-") != std::string::npos) {
                // logi("[LAYOUT] Element id='{}' computed display={} flex_grow={:.1f}",
                //      element->id, (int)element->style.display, element->style.flex_grow);
            }

            // Recursively update children (avoid cssboxGetChildren to skip vector copy)
            for (int child_id : element->children_internal_ids) {
                auto child_it = renderer->elements.find(child_id);
                if (child_it != renderer->elements.end()) {
                    update_style(child_it->second.get());
                }
            }
        };

        for (auto* root : renderer->root_elements) {
            update_style(root);
        }
    }

    // Use quadtree layout engine for all layouts (flex, grid, and block)
    cssbox::QuadtreeLayoutEngine qtEngine(renderer->viewport_width, renderer->viewport_height);
    
    for (auto* root : renderer->root_elements) {
        cssbox::LayoutNode* tree = qtEngine.build_tree(root, renderer);
        qtEngine.compute_layout(tree, renderer);
        qtEngine.write_to_elements(tree);
        delete tree;
    }

    // Compute border-radius from typed style to deprecated computed layout
    // This only runs when layout is dirty (not every frame)
    for (auto& [id, element] : renderer->elements) {
        for (int i = 0; i < 4; ++i) {
            if (element->style.border.radius[i] < 0) {
                // Negative value = percentage
                float percent = -element->style.border.radius[i];
                float size = std::min(element->layout.width, element->layout.height);
                element->layout.radius[i] = (percent / 100.0f) * size;
            } else {
                element->layout.radius[i] = element->style.border.radius[i];
            }
        }
    }

    renderer->layout_dirty = false;
    renderer->style_dirty = false;

    CSSBOX_MEM_CHECKPOINT("layout_end");
    static int layout_count = 0;
    if (++layout_count <= 3) {  // Log first 3 layouts only
        logi("[MEM] Layout #{}: {:.2f} MB (elements: {}, cache: {})",
             layout_count,
             cssbox::get_process_memory_mb(),
             renderer->elements.size(),
             renderer->stylesheet->get_cache_stats().size);
    }
}

int cssboxGetComputedStyle(cssboxRenderer* renderer,
                           const cssboxElement* element,
                           const char* property,
                           char* out_value,
                           int max_len) {
    auto computed_style = renderer->stylesheet->compute_style(
        element->id,
        element->type,
        element->classes,
        element->attributes,
        element->pseudo_states,
        element->inline_style, {},
            element->child_index, element->total_siblings
        );

    auto it = computed_style.find(property);
    if (it != computed_style.end()) {
        strncpy(out_value, it->second.c_str(), max_len - 1);
        out_value[max_len - 1] = '\0';
        return 1;
    }

    return 0;
}

// Utility Functions

NVGcolor cssboxParseColor(const char* color_str) {
    return cssbox_utils::parse_color(color_str);
}

float cssboxParseLength(const char* length_str, float context_value) {
    return cssbox_utils::parse_length(length_str, context_value);
}

void cssboxSetViewport(cssboxRenderer* renderer, float width, float height) {
    // Retain mode: Only mark dirty when viewport actually changes
    // This is critical - apps often call this every frame!
    if (renderer->viewport_width == width && renderer->viewport_height == height) {
        return;  // No change, skip dirty marking
    }

    renderer->viewport_width = width;
    renderer->viewport_height = height;
    renderer->layout_dirty = true;
    renderer->style_dirty = true;  // @media queries may change element styles
}

// ============================================================================
// V2 API: CSS File Loading and Hot-Reload
// ============================================================================

namespace {
std::string read_file(const char* filepath) {
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file) {
        return "";
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}
} // anonymous namespace

int cssboxLoadCSSFile(cssboxRenderer* renderer, const char* filepath) {
    if (!renderer || !filepath) return 0;

    // Read file
    std::string css_text = read_file(filepath);
    if (css_text.empty()) {
        return 0; // File not found or empty
    }

    // Parse CSS
    bool success = renderer->stylesheet->parse_css(css_text);
    if (!success) {
        return 0; // Parse error
    }

    // Store filepath for hot-reload
    renderer->css_files.push_back(filepath);
    renderer->style_dirty = true;

    return 1;
}

int cssboxReloadCSS(cssboxRenderer* renderer) {
    if (!renderer) return 0;

    // Clear existing CSS
    renderer->stylesheet = std::make_unique<cssbox::lexbor::EnhancedStyleSheet>();

    // Reload all files
    int success_count = 0;
    for (const auto& filepath : renderer->css_files) {
        std::string css_text = read_file(filepath.c_str());
        if (!css_text.empty()) {
            if (renderer->stylesheet->parse_css(css_text)) {
                success_count++;
            }
        }
    }

    renderer->style_dirty = true;
    renderer->layout_dirty = true;
    
    // Phase 3: Mark all elements dirty
    cssboxMarkAllDirty(renderer, cssbox_DIRTY_ALL);

    return success_count;
}

int cssboxGetVariable(cssboxRenderer* renderer, const char* name,
                      char* out_value, int max_len) {
    if (!renderer || !name || !out_value || max_len <= 0) return 0;

    // Query variable from stylesheet
    const auto& var_map = renderer->stylesheet->get_variable_resolver().get_all_variables();
    auto it = var_map.find(name);

    if (it != var_map.end()) {
        strncpy_s(out_value, max_len, it->second.c_str(), max_len - 1);
        out_value[max_len - 1] = '\0';
        return 1;
    }

    return 0;
}

// ============================================================================
// Memory Profiling API
// ============================================================================

void cssboxPrintMemoryProfile(cssboxRenderer* renderer) {
    printf("\n");
    printf("============================================\n");
    printf("       CSSBOX Memory Usage Report\n");
    printf("============================================\n");
    printf("\n");

    // Process memory
    float process_mb = cssbox::get_process_memory_mb();
    printf("Process RSS:         %8.2f MB\n", process_mb);
    printf("\n");

    if (renderer) {
        // Arena memory
        printf("Memory Arena:\n");
        printf("  Used:              %8.2f KB\n", renderer->arena_.used() / 1024.0f);
        printf("  Peak:              %8.2f KB\n", renderer->arena_.peak() / 1024.0f);
        printf("  Available:         %8.2f KB\n", renderer->arena_.available() / 1024.0f);
        printf("\n");

        // Element statistics
        size_t element_count = renderer->elements.size();
        size_t estimated_element_mem = element_count * sizeof(cssboxElement);
        printf("Elements:\n");
        printf("  Count:             %8zu\n", element_count);
        printf("  Struct size:       %8zu bytes\n", sizeof(cssboxElement));
        printf("  Estimated total:   %8.2f KB\n", estimated_element_mem / 1024.0f);
        printf("\n");

        // Style cache
        auto cache_stats = renderer->stylesheet->get_cache_stats();
        printf("Style Cache:\n");
        printf("  Entries:           %8zu\n", cache_stats.size);
        printf("  Hits:              %8zu\n", cache_stats.hits);
        printf("  Misses:            %8zu\n", cache_stats.misses);
        printf("  Hit rate:          %8.1f%%\n", cache_stats.hit_rate() * 100.0f);
        printf("\n");

        // CSS rules
        printf("CSS Rules:           %8zu\n", renderer->stylesheet->get_rules().size());
        printf("CSS Variables:       %8zu\n",
               renderer->stylesheet->get_variable_resolver().get_all_variables().size());
        printf("\n");

        // Gradient cache
        printf("Gradient Cache:      %8zu entries\n", renderer->css_gradient_cache_.size());
        printf("Image Cache:         %8zu entries\n", renderer->image_cache.size());
        printf("Patterns:            %8zu\n", renderer->patterns_.size());
        printf("Markers:             %8zu\n", renderer->markers_.size());
        printf("Clip Paths:          %8zu\n", renderer->clip_paths_.size());
        printf("\n");

        // Render list
        printf("Render List:         %8zu elements\n", renderer->render_list_.size());
        printf("Animated Elements:   %8zu\n", renderer->animated_elements_.size());
    }

    printf("============================================\n");
    printf("\n");

#ifdef CSSBOX_MEMORY_PROFILE
    cssbox::MemoryProfiler::instance().log_all();
#endif
}

float cssboxGetMemoryUsageMB(cssboxRenderer* renderer) {
    (void)renderer;  // May use renderer-specific stats in future
    return cssbox::get_process_memory_mb();
}
