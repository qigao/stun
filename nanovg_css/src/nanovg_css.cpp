/*
 * NanoVG CSS - Main implementation
 */

#include "nanovg_css_internal.h"
#include "lexbor_css_parser.h"
#include <algorithm>
#include <vector> 
// ============================================================================
// Z-Index Rendering (Sprint 10)
// ============================================================================

/**
 * @brief Render order for z-index sorting
 */
struct RenderOrder {
    bool is_positioned;    // position != static
    int z_index;           // From explicit_style
    int tree_order;        // Insertion order for stability

    bool operator<(const RenderOrder& other) const {
        // Non-positioned elements render first
        if (is_positioned != other.is_positioned) {
            return !is_positioned;  // false < true, so non-positioned first
        }

        // Among positioned elements, sort by z-index
        if (is_positioned) {
            if (z_index != other.z_index) {
                return z_index < other.z_index;
            }
        }

        // Same category/z-index: maintain tree order
        return tree_order < other.tree_order;
    }
};

/**
 * @brief Collect all visible elements for rendering
 */
static void collect_elements_for_render(
    NVGCSSElement* element,
    std::vector<std::pair<NVGCSSElement*, RenderOrder>>& elements,
    int& tree_order)
{
    if (!element->visible) return;

    RenderOrder order;
    order.is_positioned = (element->explicit_style.position != "static" &&
                          element->explicit_style.position != "");
    order.z_index = element->explicit_style.z_index;
    order.tree_order = tree_order++;

    elements.push_back({element, order});

    // Recursively collect children
    for (auto* child : element->children) {
        collect_elements_for_render(child, elements, tree_order);
    }
}

/**
 * @brief Compute absolute transform by multiplying parent chain
 */
static void compute_absolute_transform(NVGCSSElement* element, float* abs_xform) {
    // Start with identity
    nvgTransformIdentity(abs_xform);

    // Collect parent chain (bottom-up)
    std::vector<NVGCSSElement*> chain;
    NVGCSSElement* curr = element;
    while (curr) {
        chain.push_back(curr);
        curr = curr->parent;
    }

    // Apply transforms from root to element (top-down)
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
        // nvgTransformMultiply(dst, src) computes dst = dst * src
        nvgTransformMultiply(abs_xform, (*it)->transform);
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

void nvgcssAddRule(NVGCSSRenderer* renderer,
                   const char* selector,
                   const char* property,
                   const char* value) {
    std::map<std::string, std::string> props;
    props[property] = value;
    renderer->stylesheet->add_rule(selector, props);
}

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
    element->id = id;
    element->type = type;
    element->parent = nullptr;
    element->visible = true;
    element->opacity = 1.0f;
    element->user_data = nullptr;
    element->transition_state = nullptr;  // Phase 4: Will be created when transition is specified

    // Sprint 30: Initialize child position fields
    element->child_index = 0;
    element->total_siblings = 1;  // Will be updated when added to parent

    // Initialize transform to identity
    nvgTransformIdentity(element->transform);

    // Initialize box
    element->box = {};

    NVGCSSElement* ptr = element.get();
    renderer->elements[id] = std::move(element);
    renderer->root_elements.push_back(ptr);
    renderer->layout_dirty = true;

    return ptr;
}

NVGCSSElement* nvgcssGetElement(NVGCSSRenderer* renderer, const char* id) {
    auto it = renderer->elements.find(id);
    if (it != renderer->elements.end()) {
        return it->second.get();
    }
    return nullptr;
}

void nvgcssDeleteElement(NVGCSSRenderer* renderer, const char* id) {
    auto it = renderer->elements.find(id);
    if (it != renderer->elements.end()) {
        NVGCSSElement* element = it->second.get();

        // Remove from root elements
        auto root_it = std::find(renderer->root_elements.begin(),
                                 renderer->root_elements.end(),
                                 element);
        if (root_it != renderer->root_elements.end()) {
            renderer->root_elements.erase(root_it);
        }

        // Remove from parent
        if (element->parent) {
            auto& siblings = element->parent->children;
            auto sibling_it = std::find_if(siblings.begin(), siblings.end(),
                [element](NVGCSSElement* child) {
                    return child == element;
                });
            if (sibling_it != siblings.end()) {
                siblings.erase(sibling_it);
            }
        }

        renderer->elements.erase(it);
        renderer->layout_dirty = true;
    }
}

void nvgcssClearElements(NVGCSSRenderer* renderer) {
    if (!renderer) return;
    renderer->elements.clear();
    renderer->root_elements.clear();
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

int nvgcssHasClass(const NVGCSSElement* element, const char* class_name) {
    auto it = std::find(element->classes.begin(), element->classes.end(), class_name);
    return (it != element->classes.end()) ? 1 : 0;
}

void nvgcssSetAttribute(NVGCSSElement* element,
                        const char* name,
                        const char* value) {
    element->attributes[name] = value;
}

void nvgcssSetStyle(NVGCSSElement* element,
                    const char* property,
                    const char* value) {
    element->inline_style[property] = value;
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
static void update_child_indices(NVGCSSElement* parent) {
    if (!parent) return;

    int total = static_cast<int>(parent->children.size());
    for (int i = 0; i < total; i++) {
        parent->children[i]->child_index = i;
        parent->children[i]->total_siblings = total;
    }
}

void nvgcssAppendChild(NVGCSSRenderer* renderer, NVGCSSElement* parent, NVGCSSElement* child) {
    // Remove from previous parent if any
    if (child->parent) {
        auto& siblings = child->parent->children;
        auto it = std::find(siblings.begin(), siblings.end(), child);
        if (it != siblings.end()) {
            siblings.erase(it);
        }
        // Sprint 30: Update indices of old parent's children
        update_child_indices(child->parent);
    }

    // Remove from root_elements since it's now a child
    auto root_it = std::find(renderer->root_elements.begin(),
                             renderer->root_elements.end(), child);
    if (root_it != renderer->root_elements.end()) {
        renderer->root_elements.erase(root_it);
    }

    // Add to new parent
    child->parent = parent;
    parent->children.push_back(child);

    // Sprint 30: Update indices of new parent's children
    update_child_indices(parent);
}

void nvgcssRemoveChild(NVGCSSElement* parent, NVGCSSElement* child) {
    auto& children = parent->children;
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end()) {
        children.erase(it);
        child->parent = nullptr;

        // Sprint 30: Update indices of remaining children
        update_child_indices(parent);
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
        for (auto* child : element->children) {
            update_element(child);
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

void nvgcssRender(NVGCSSRenderer* renderer) {
 

    // Compute layout if dirty
    if (renderer->layout_dirty || renderer->style_dirty) {
 
        nvgcssComputeLayout(renderer);
 
    }

    // Phase 1: Collect all elements for rendering
    std::vector<std::pair<NVGCSSElement*, RenderOrder>> elements;
    int tree_order = 0;
    for (auto* root : renderer->root_elements) {
        collect_elements_for_render(root, elements, tree_order);
    }

    // Phase 2: Sort by render order (non-positioned first, then by z-index)
    std::sort(elements.begin(), elements.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        });

    // Phase 3: Render in sorted order
    for (const auto& [element, order] : elements) {
        // Get computed style
        auto computed_style = renderer->stylesheet->compute_style(
            element->id,
            element->type,
            element->classes,
            element->attributes,
            element->pseudo_states,
            element->inline_style, {},
            element->child_index, element->total_siblings
        );

        // Compute absolute transform (parent chain)
        float abs_transform[6];
        compute_absolute_transform(element, abs_transform);

        // Temporarily replace element transform with absolute
        float original_transform[6];
        memcpy(original_transform, element->transform, sizeof(float) * 6);
        memcpy(element->transform, abs_transform, sizeof(float) * 6);

        // Paint this element
        renderer->painter->paint_element(element, computed_style);

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

        // Get computed style
        auto computed_style = renderer->stylesheet->compute_style(
            elem->id,
            elem->type,
            elem->classes,
            elem->attributes,
            elem->pseudo_states,
            elem->inline_style, {},
            elem->child_index, elem->total_siblings
        );

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

        // Paint this element
        renderer->painter->paint_element(elem, computed_style);

        // Render children (within scissor region if set)
        for (auto* child : elem->children) {
            render_tree(child);
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
