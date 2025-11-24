/*
 * NanoVG CSS - Main implementation
 */

#include "nanovg_css_internal.h"
#include "lexbor_css_parser.h"
#include <fmtlog.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <vector>
#include <fstream> 
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
    int z_index;           // From explicit_style
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

namespace {

std::string trim_float(float value) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << value;
    std::string str = oss.str();
    if (str.find('.') != std::string::npos) {
        while (!str.empty() && str.back() == '0') {
            str.pop_back();
        }
        if (!str.empty() && str.back() == '.') {
            str.pop_back();
        }
    }
    if (str.empty()) str = "0";
    return str;
}

std::string format_px_string(float value_px) {
    return trim_float(value_px) + "px";
}

std::string color_to_css_string(NVGcolor color) {
    auto clamp_channel = [](float c) -> int {
        int v = static_cast<int>(std::round(c * 255.0f));
        if (v < 0) v = 0;
        if (v > 255) v = 255;
        return v;
    };

    int r = clamp_channel(color.r);
    int g = clamp_channel(color.g);
    int b = clamp_channel(color.b);
    float a = std::clamp(color.a, 0.0f, 1.0f);

    std::ostringstream oss;
    oss << "rgba(" << r << ", " << g << ", " << b << ", "
        << std::fixed << std::setprecision(2) << a << ")";
    return oss.str();
}

std::string dash_array_to_css(const float* segments, int count) {
    if (!segments || count <= 0) {
        return "";
    }
    std::ostringstream oss;
    for (int i = 0; i < count; ++i) {
        float value = segments[i];
        if (value <= 0.0f) {
            continue;
        }
        if (oss.tellp() > 0) {
            oss << ' ';
        }
        oss << format_px_string(value);
    }
    return oss.str();
}

}  // namespace

/**
 * @brief Collect all visible elements for rendering
 */
static void collect_elements_for_render(
    NVGCSSRenderer* renderer,
    NVGCSSElement* element,
    std::vector<std::pair<NVGCSSElement*, RenderOrder>>& elements,
    int& tree_order,
    int depth = 0)
{
    if (!element->visible) return;

    RenderOrder order;
    order.depth = depth;
    order.is_positioned = (element->explicit_style.position != "static" &&
                          element->explicit_style.position != "");
    order.z_index = element->explicit_style.z_index;
    order.tree_order = tree_order++;

    elements.push_back({element, order});

    // Recursively collect children at increased depth
    // IMPORTANT: Copy children locally before recursing, because nvgcssGetChildren
    // uses a static cache that gets overwritten by recursive calls
    int child_count = 0;
    NVGCSSElement** children_ptr = nvgcssGetChildren(renderer, element, &child_count);
    std::vector<NVGCSSElement*> children_copy(children_ptr, children_ptr + child_count);
    for (NVGCSSElement* child : children_copy) {
        collect_elements_for_render(renderer, child, elements, tree_order, depth + 1);
    }
}

/**
 * @brief Compute absolute transform by multiplying parent chain
 */
static void compute_absolute_transform(NVGCSSRenderer* renderer, NVGCSSElement* element, float* abs_xform) {
    // Start with identity
    nvgTransformIdentity(abs_xform);

    // Collect parent chain (bottom-up)
    std::vector<NVGCSSElement*> chain;
    NVGCSSElement* curr = element;
    while (curr) {
        chain.push_back(curr);
        curr = nvgcssGetParent(renderer, curr);  // Use safe accessor
    }

    // Apply transforms from root to element (top-down)
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        // nvgTransformMultiply(dst, src) computes dst = dst * src
        nvgTransformMultiply(abs_xform, (*it)->transform);
    }
}

// ============================================================================
// Constructor Implementations
// ============================================================================

NVGCSSExplicitStyle::NVGCSSExplicitStyle() {
    width = height = -1.0f;
    min_width = min_height = -1.0f;
    max_width = max_height = -1.0f;
    x = y = -1.0f;
    position = "static";
    top = right = bottom = left = -1.0f;
    z_index = 0;
    box_sizing = "content-box";
    overflow = "visible";
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
    grid_auto_flow = "row";
    border_top_style = border_right_style = border_bottom_style = border_left_style = "solid";
}

NVGCSSComputedLayout::NVGCSSComputedLayout() {
    width = height = 0.0f;
    x = y = 0.0f;
    content_width = content_height = 0.0f;
    source = UNCOMPUTED;
    is_computed = false;
    for (int i = 0; i < 4; i++) {
        padding[i] = border[i] = margin[i] = border_radius[i] = 0.0f;
    }
}

// ============================================================================
// NVGCSSRenderer Implementation
// ============================================================================

NVGCSSRenderer::NVGCSSRenderer(NVGcontext* vg) : vg(vg) {
    stylesheet = std::make_unique<nanovg_css::lexbor::EnhancedStyleSheet>();
    painter = std::make_unique<NVGCSSPainter>(vg, this);
    layout_engine = std::make_unique<NVGCSSLayoutEngine>(viewport_width, viewport_height);
    current_time = 0.0f;
}

NVGCSSRenderer::~NVGCSSRenderer() {
    // Clean up transition states
    for (auto& [id, element] : elements) {
        if (element->transition_state) {
            delete static_cast<TransitionState*>(element->transition_state);
            element->transition_state = nullptr;
        }
    }
}

// ============================================================================
// C API Implementation
// ============================================================================

NVGCSSRenderer* nvgcssCreateRenderer(NVGcontext* vg) {
    return new NVGCSSRenderer(vg);
}

void nvgcssDeleteRenderer(NVGCSSRenderer* renderer) {
    delete renderer;
}

// CSS Management

int nvgcssParseCSS(NVGCSSRenderer* renderer, const char* css) {
    return renderer->stylesheet->parse_css(css) ? 1 : 0;
}

// ============================================================================
// DEPRECATED v1 API - Removed in v2.0
// ============================================================================
// Use nvgcssLoadCSSFile() or nvgcssParseCSS() instead.
// See MIGRATION_V2.md for migration guide.
// ============================================================================

#if 0  // DEPRECATED - Commented out in v2.0
void nvgcssAddRule(NVGCSSRenderer* renderer,
                   const char* selector,
                   const char* property,
                   const char* value) {
    std::map<std::string, std::string> props;
    props[property] = value;
    renderer->stylesheet->add_rule(selector, props);
}
#endif

void nvgcssClearCSS(NVGCSSRenderer* renderer) {
    renderer->stylesheet = std::make_unique<nanovg_css::lexbor::EnhancedStyleSheet>();
    renderer->style_dirty = true;
}

void nvgcssSetVariable(NVGCSSRenderer* renderer,
                       const char* name,
                       const char* value) {
    renderer->stylesheet->set_variable(name, value);
    renderer->style_dirty = true;
}

// Element Management

NVGCSSElement* nvgcssCreateElement(NVGCSSRenderer* renderer,
                                   const char* id,
                                   const char* type) {
    auto element = std::make_unique<NVGCSSElement>();

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

    NVGCSSElement* ptr = element.get();
    int internal_id = element->internal_id;

    // Store element by internal_id
    renderer->elements[internal_id] = std::move(element);

    // Map user id to internal_id (if user provided an id)
    if (id && id[0] != '\0') {
        renderer->id_to_internal_id[id] = internal_id;
    }

    renderer->root_elements.push_back(ptr);
    renderer->layout_dirty = true;

    return ptr;
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
static NVGCSSElement* get_element_by_internal_id(NVGCSSRenderer* renderer, int internal_id) {
    auto it = renderer->elements.find(internal_id);
    if (it != renderer->elements.end()) {
        return it->second.get();
    }
    return nullptr;
}

// Public API: Get element by user-provided id (for backward compatibility)
NVGCSSElement* nvgcssGetElement(NVGCSSRenderer* renderer, const char* id) {
    if (!id || id[0] == '\0') {
        return nullptr;  // Empty id
    }

    auto it = renderer->id_to_internal_id.find(id);
    if (it != renderer->id_to_internal_id.end()) {
        return get_element_by_internal_id(renderer, it->second);
    }
    return nullptr;
}

NVGCSSElement* nvgcssGetParent(NVGCSSRenderer* renderer, const NVGCSSElement* element) {
    if (!renderer || !element || element->parent_internal_id < 0) {
        return nullptr;
    }
    return get_element_by_internal_id(renderer, element->parent_internal_id);
}

NVGCSSElement** nvgcssGetChildren(NVGCSSRenderer* renderer,
                                  const NVGCSSElement* element,
                                  int* out_count) {
    // Static storage for returned pointers (valid until next call)
    static thread_local std::vector<NVGCSSElement*> children_cache;
    children_cache.clear();

    if (!renderer || !element || !out_count) {
        if (out_count) *out_count = 0;
        return nullptr;
    }

    for (int child_internal_id : element->children_internal_ids) {
        NVGCSSElement* child = get_element_by_internal_id(renderer, child_internal_id);
        if (child) {
            children_cache.push_back(child);
        }
    }

    *out_count = static_cast<int>(children_cache.size());
    return children_cache.empty() ? nullptr : children_cache.data();
}

void nvgcssDeleteElement(NVGCSSRenderer* renderer, const char* id) {
    // Look up internal_id by user id
    auto id_it = renderer->id_to_internal_id.find(id);
    if (id_it == renderer->id_to_internal_id.end()) {
        return;  // Element not found
    }

    int internal_id = id_it->second;
    auto it = renderer->elements.find(internal_id);
    if (it != renderer->elements.end()) {
        NVGCSSElement* element = it->second.get();

        // Remove from root elements
        auto root_it = std::find(renderer->root_elements.begin(),
                                 renderer->root_elements.end(),
                                 element);
        if (root_it != renderer->root_elements.end()) {
            renderer->root_elements.erase(root_it);
        }

        // Remove from parent's children list
        if (element->parent_internal_id >= 0) {
            NVGCSSElement* parent = get_element_by_internal_id(renderer, element->parent_internal_id);
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
    }
}

void nvgcssClearElements(NVGCSSRenderer* renderer) {
    if (!renderer) return;
    renderer->elements.clear();
    renderer->id_to_internal_id.clear();
    renderer->root_elements.clear();
    renderer->next_internal_id = 1;  // Reset ID counter
    renderer->layout_dirty = true;
    renderer->style_dirty = true;
}

void nvgcssAddClass(NVGCSSElement* element, const char* class_name) {
    auto it = std::find(element->classes.begin(), element->classes.end(), class_name);
    if (it == element->classes.end()) {
        element->classes.push_back(class_name);
    }
}

void nvgcssRemoveClass(NVGCSSElement* element, const char* class_name) {
    auto it = std::find(element->classes.begin(), element->classes.end(), class_name);
    if (it != element->classes.end()) {
        element->classes.erase(it);
    }
}

// ============================================================================
// DEPRECATED v1 API - Stroke Styling Functions Removed in v2.0
// ============================================================================
// Use CSS properties instead:
//   stroke: <color>;
//   stroke-width: <length>;
//   stroke-linecap: butt | round | square;
//   stroke-dasharray: <length> [<length>]*;
//   stroke-dashoffset: <length>;
// See MIGRATION_V2.md for examples.
// ============================================================================

#if 0  // DEPRECATED - Commented out in v2.0
void nvgcssSetStrokeColor(NVGCSSElement* element, NVGcolor color) {
    if (!element) return;
    element->inline_style["stroke"] = color_to_css_string(color);
}

void nvgcssSetStrokeWidth(NVGCSSElement* element, float width_px) {
    if (!element) return;
    element->inline_style["stroke-width"] = format_px_string(width_px);
}

void nvgcssSetStrokeLineCap(NVGCSSElement* element, const char* cap) {
    if (!element || !cap) return;
    element->inline_style["stroke-linecap"] = cap;
}

void nvgcssSetStrokeDashArray(NVGCSSElement* element,
                              const float* segments,
                              int count) {
    if (!element) return;
    std::string value = dash_array_to_css(segments, count);
    if (value.empty()) {
        element->inline_style.erase("stroke-dasharray");
    } else {
        element->inline_style["stroke-dasharray"] = value;
    }
}

void nvgcssSetStrokeDashOffset(NVGCSSElement* element, float offset_px) {
    if (!element) return;
    element->inline_style["stroke-dashoffset"] = format_px_string(offset_px);
}
#endif

void nvgcssSetLineGeometry(NVGCSSElement* element,
                           float x1, float y1,
                           float x2, float y2) {
    if (!element) return;
    element->line_geometry.x1 = x1;
    element->line_geometry.y1 = y1;
    element->line_geometry.x2 = x2;
    element->line_geometry.y2 = y2;
    element->line_geometry.defined = true;
}

void nvgcssClearLineGeometry(NVGCSSElement* element) {
    if (!element) return;
    element->line_geometry = NVGCSSLineGeometry();
}

void nvgcssSetCircleGeometry(NVGCSSElement* element,
                             float cx, float cy,
                             float radius) {
    if (!element) return;
    element->circle_geometry.cx = cx;
    element->circle_geometry.cy = cy;
    element->circle_geometry.rx = radius;
    element->circle_geometry.ry = radius;
    element->circle_geometry.defined = true;
}

void nvgcssSetEllipseGeometry(NVGCSSElement* element,
                              float cx, float cy,
                              float rx, float ry) {
    if (!element) return;
    element->circle_geometry.cx = cx;
    element->circle_geometry.cy = cy;
    element->circle_geometry.rx = rx;
    element->circle_geometry.ry = ry;
    element->circle_geometry.defined = true;
}

void nvgcssClearCircleGeometry(NVGCSSElement* element) {
    if (!element) return;
    element->circle_geometry = NVGCSSCircleGeometry();
}

// ============================================================================
// DEPRECATED v1 API - Advanced Stroke Functions Removed in v2.0
// ============================================================================
// nvgcssSetStrokePoints() and nvgcssSetStrokeSalt() are low-level APIs
// that manipulate freehand path geometry. These should be managed through
// application logic rather than as CSS properties.
// ============================================================================

#if 0  // DEPRECATED - Commented out in v2.0
void nvgcssSetStrokePoints(NVGCSSElement* element,
                           const NVGCSSPoint* points,
                           int count) {
    if (!element) return;
    element->stroke_points.clear();
    if (!points || count <= 0) {
        return;
    }
    element->stroke_points.reserve(count);
    for (int i = 0; i < count; ++i) {
        element->stroke_points.push_back(points[i]);
    }
}

void nvgcssSetStrokeSalt(NVGCSSElement* element, float salt) {
    if (!element) return;
    element->stroke_salt = salt;
    element->has_stroke_salt = true;
}
#endif

int nvgcssHasClass(const NVGCSSElement* element, const char* class_name) {
    auto it = std::find(element->classes.begin(), element->classes.end(), class_name);
    return (it != element->classes.end()) ? 1 : 0;
}

// Text Content (Phase 3)

void nvgcssSetText(NVGCSSElement* element, const char* text) {
    element->text_content = text;
}

const char* nvgcssGetText(const NVGCSSElement* element) {
    return element->text_content.c_str();
}

// State Management

void nvgcssSetPseudoState(NVGCSSElement* element,
                          const char* state,
                          int active) {
    if (active) {
        element->pseudo_states.insert(state);
    } else {
        element->pseudo_states.erase(state);
    }
}

int nvgcssHasPseudoState(const NVGCSSElement* element, const char* state) {
    return element->pseudo_states.count(state) > 0 ? 1 : 0;
}

// Tree Manipulation

// Sprint 30: Helper function to update child indices
static void update_child_indices(NVGCSSRenderer* renderer, NVGCSSElement* parent) {
    if (!parent || !renderer) return;

    int child_count = 0;
    NVGCSSElement** children = nvgcssGetChildren(renderer, parent, &child_count);

    for (int i = 0; i < child_count; i++) {
        children[i]->child_index = i;
        children[i]->total_siblings = child_count;
    }
}

void nvgcssAppendChild(NVGCSSRenderer* renderer, NVGCSSElement* parent, NVGCSSElement* child) {
    // Remove from previous parent if any
    if (child->parent_internal_id >= 0) {
        NVGCSSElement* old_parent = get_element_by_internal_id(renderer, child->parent_internal_id);
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
}

void nvgcssRemoveChild(NVGCSSRenderer* renderer, NVGCSSElement* parent, NVGCSSElement* child) {
    if (!renderer || !parent || !child) return;

    auto& children_ids = parent->children_internal_ids;
    auto it = std::find(children_ids.begin(), children_ids.end(), child->internal_id);
    if (it != children_ids.end()) {
        children_ids.erase(it);
        child->parent_internal_id = -1;  // Back to root

        // Sprint 30: Update indices of remaining children
        update_child_indices(renderer, parent);
    }
}

// Rendering

void nvgcssUpdate(NVGCSSRenderer* renderer, float delta_time) {
    // Update current time
    renderer->current_time += delta_time;

    // Helper function to get or create transition state
    auto get_transition_state = [](NVGCSSElement* element) -> TransitionState* {
        if (!element->transition_state) {
            element->transition_state = new TransitionState();
        }
        return static_cast<TransitionState*>(element->transition_state);
    };

    // Update all elements
    std::function<void(NVGCSSElement*)> update_element = [&](NVGCSSElement* element) {
        if (!element->visible) return;

        TransitionState* trans_state = nullptr;
        std::map<std::string, std::string> saved_inline_style;

        // First, restore any transition-modified inline styles from previous frame
        if (element->transition_state) {
            trans_state = static_cast<TransitionState*>(element->transition_state);

            // Remove transition-applied values from inline_style
            for (const auto& [prop, trans] : trans_state->active_transitions) {
                if (trans.active) {
                    auto it = element->inline_style.find(prop);
                    if (it != element->inline_style.end()) {
                        element->inline_style.erase(it);
                    }
                }
            }
        }

        // Get computed style (now without transition-applied inline styles)
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
            if (nvgcss_utils::parse_transition(transition_it->second, prop, duration, easing)) {
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
                    float eased_t = nvgcss_utils::apply_easing(t, trans.easing);
                    std::string interpolated = nvgcss_utils::interpolate_value(
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
        auto get_animation_state = [](NVGCSSElement* elem) -> AnimationState* {
            if (!elem->animation_state) {
                elem->animation_state = new AnimationState();
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
                    anim_state->running_animations.push_back(new_anim);
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
                float eased_position = nvgcss_utils::apply_easing(position, anim.easing);

                // Get interpolated properties from keyframe animation
                const KeyframeAnimation* kf_anim =
                    renderer->stylesheet->get_keyframe_animation(anim.animation_name);
                if (kf_anim) {
                    auto props = kf_anim->get_properties_at(eased_position);

                    // Apply to element inline style (for this frame)
                    for (const auto& [prop, value] : props) {
                        element->inline_style[prop] = value;
                    }
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


        // Recursively update children
        int child_count = 0;
        NVGCSSElement** children = nvgcssGetChildren(renderer, element, &child_count);
        for (int i = 0; i < child_count; ++i) {
            update_element(children[i]);
        }
    };

    // Update all root elements
    for (auto* root : renderer->root_elements) {
        update_element(root);
    }

    // Mark layout as dirty if any transitions are active
    bool has_active_transitions = false;
    for (const auto& [id, element] : renderer->elements) {
        if (element->transition_state) {
            TransitionState* trans_state = static_cast<TransitionState*>(element->transition_state);
            for (const auto& [prop, trans] : trans_state->active_transitions) {
                if (trans.active) {
                    has_active_transitions = true;
                    break;
                }
            }
            if (has_active_transitions) break;
        }
    }

    if (has_active_transitions) {
        renderer->layout_dirty = true;
    }
}

// ============================================================================
// Dirty Flags API (60fps Optimization)
// ============================================================================

void nvgcssMarkDirty(NVGCSSElement* element, int flags) {
    if (!element) return;

    // Map public flags to internal flags
    if (flags & NVGCSS_DIRTY_STYLE)     element->dirty_flags |= nvgcss::DIRTY_STYLE;
    if (flags & NVGCSS_DIRTY_LAYOUT)    element->dirty_flags |= nvgcss::DIRTY_LAYOUT;
    if (flags & NVGCSS_DIRTY_TRANSFORM) element->dirty_flags |= nvgcss::DIRTY_TRANSFORM;
    if (flags & NVGCSS_DIRTY_CHILDREN)  element->dirty_flags |= nvgcss::DIRTY_CHILDREN;
}

void nvgcssMarkAllDirty(NVGCSSRenderer* renderer, int flags) {
    if (!renderer) return;

    for (auto& [id, element] : renderer->elements) {
        nvgcssMarkDirty(element.get(), flags);
    }

    // Also mark renderer-level flags
    if (flags & NVGCSS_DIRTY_LAYOUT) renderer->layout_dirty = true;
    if (flags & NVGCSS_DIRTY_STYLE)  renderer->style_dirty = true;
}

int nvgcssIsDirty(const NVGCSSElement* element, int flags) {
    if (!element) return 0;

    // Check if any requested flag is set
    if ((flags & NVGCSS_DIRTY_STYLE)     && (element->dirty_flags & nvgcss::DIRTY_STYLE))     return 1;
    if ((flags & NVGCSS_DIRTY_LAYOUT)    && (element->dirty_flags & nvgcss::DIRTY_LAYOUT))    return 1;
    if ((flags & NVGCSS_DIRTY_TRANSFORM) && (element->dirty_flags & nvgcss::DIRTY_TRANSFORM)) return 1;
    if ((flags & NVGCSS_DIRTY_CHILDREN)  && (element->dirty_flags & nvgcss::DIRTY_CHILDREN))  return 1;

    return 0;
}

void nvgcssRender(NVGCSSRenderer* renderer) {
 

    // Compute layout if dirty
    if (renderer->layout_dirty || renderer->style_dirty) {
 
        nvgcssComputeLayout(renderer);
 
    }

    // Phase 1: Collect all elements for rendering
    std::vector<std::pair<NVGCSSElement*, RenderOrder>> elements;
    int tree_order = 0;
    for (auto* root : renderer->root_elements) {
        collect_elements_for_render(renderer, root, elements, tree_order);
    }

    // Phase 2: Sort by render order (non-positioned first, then by z-index)
    std::sort(elements.begin(), elements.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

    // Phase 3: Render in sorted order
    for (const auto& [element, order] : elements) {
        // DEBUG: Log render order (skip grid items for clarity)
        if (element->id.find("grid") == std::string::npos) {
            // printf("[RENDER] Painting '%s' (depth=%d, z_index=%d, tree_order=%d, is_positioned=%d)\n",
            //        element->id.c_str(), order.depth, order.z_index, order.tree_order, order.is_positioned);
        }

        // Compute absolute transform (parent chain)
        float abs_transform[6];
        compute_absolute_transform(renderer, element, abs_transform);

        // Temporarily replace element transform with absolute
        float original_transform[6];
        memcpy(original_transform, element->transform, sizeof(float) * 6);
        memcpy(element->transform, abs_transform, sizeof(float) * 6);

        // Paint this element (uses element->style, already computed in layout phase)
        renderer->painter->paint_element(element);

        // Restore original transform
        memcpy(element->transform, original_transform, sizeof(float) * 6);
    }
}

void nvgcssRenderElement(NVGCSSRenderer* renderer, const char* id) {
    NVGCSSElement* element = nvgcssGetElement(renderer, id);
    if (!element) return;

    // Recursively render element tree
    std::function<void(NVGCSSElement*)> render_tree = [&](NVGCSSElement* elem) {
        if (!elem->visible) return;

        // Sprint 22: Check if overflow clipping is needed
        bool has_overflow_clip = (elem->explicit_style.overflow_x != "visible" ||
                                  elem->explicit_style.overflow_y != "visible");

        // Apply scissor clipping if overflow is hidden/scroll/auto
        if (has_overflow_clip) {
            nvgSave(renderer->painter->get_context());
            nvgScissor(renderer->painter->get_context(),
                      elem->computed.x,
                      elem->computed.y,
                      elem->computed.width,
                      elem->computed.height);
        }

        // Paint this element (uses elem->style, already computed in layout phase)
        renderer->painter->paint_element(elem);

        // Render children (within scissor region if set)
        int child_count = 0;
        NVGCSSElement** children = nvgcssGetChildren(renderer, elem, &child_count);
        for (int i = 0; i < child_count; ++i) {
            render_tree(children[i]);
        }

        // Restore state if we set scissor
        if (has_overflow_clip) {
            nvgRestore(renderer->painter->get_context());
        }
    };

    render_tree(element);
}

void nvgcssComputeLayout(NVGCSSRenderer* renderer) { 
    renderer->layout_engine->compute_layout(renderer->root_elements, renderer);
    renderer->layout_dirty = false;
    renderer->style_dirty = false;
}

int nvgcssGetComputedStyle(NVGCSSRenderer* renderer,
                           const NVGCSSElement* element,
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

NVGcolor nvgcssParseColor(const char* color_str) {
    return nvgcss_utils::parse_color(color_str);
}

float nvgcssParseLength(const char* length_str, float context_value) {
    return nvgcss_utils::parse_length(length_str, context_value);
}

void nvgcssSetViewport(NVGCSSRenderer* renderer, float width, float height) {
    renderer->viewport_width = width;
    renderer->viewport_height = height;
    renderer->layout_engine->set_viewport(width, height);
    renderer->layout_dirty = true;
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

int nvgcssLoadCSSFile(NVGCSSRenderer* renderer, const char* filepath) {
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

int nvgcssReloadCSS(NVGCSSRenderer* renderer) {
    if (!renderer) return 0;

    // Clear existing CSS
    renderer->stylesheet = std::make_unique<nanovg_css::lexbor::EnhancedStyleSheet>();

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

    return success_count;
}

int nvgcssGetVariable(NVGCSSRenderer* renderer, const char* name,
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
