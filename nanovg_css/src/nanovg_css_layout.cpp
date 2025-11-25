/*
 * NanoVG CSS - Layout engine
 *
 * Computes element positioning and dimensions.
 */

#include "nanovg_css_internal.h"
#include <cctype>
#include <sstream>

namespace {
float safe_stof(const std::string &value, float fallback = 0.f) {
  try {
    size_t processed = 0;
    float parsed = std::stof(value, &processed);
    return processed == 0 ? fallback : parsed;
  } catch (const std::exception&) {
    return fallback;
  }
}
} // namespace

NVGCSSLayoutEngine::NVGCSSLayoutEngine(float viewport_width, float viewport_height)
    : viewport_width_(viewport_width), viewport_height_(viewport_height) {}

void NVGCSSLayoutEngine::set_viewport(float width, float height) {
  viewport_width_ = width;
  viewport_height_ = height;
}

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
float apply_dimension_constraints(float value, float min_value, float max_value) {
  // CSS spec: if min > max, min wins
  // Check this first before applying constraints
  if (min_value >= 0 && max_value >= 0 && min_value > max_value) {
    return min_value; // min wins
  }

  // Apply maximum constraint first
  if (max_value >= 0) {
    value = std::min(value, max_value);
  }

  // Then apply minimum constraint
  if (min_value >= 0) {
    value = std::max(value, min_value);
  }

  return value;
}

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
 *
 * content-box (CSS default):
 *   - total dimensions ARE content dimensions
 *   - padding and border are ADDED to total dimensions when rendering
 *
 * border-box:
 *   - total dimensions INCLUDE padding and border
 *   - content dimensions = total - padding - border
 */
void calculate_content_dimensions(float total_width, float total_height,
                                  const float padding[4], // top, right, bottom, left
                                  const float border[4],  // top, right, bottom, left
                                  const std::string &box_sizing, float &content_width,
                                  float &content_height) {
  if (box_sizing == "border-box") {
    // Subtract padding and border from total dimensions
    float horizontal_padding = padding[1] + padding[3]; // right + left
    float vertical_padding = padding[0] + padding[2];   // top + bottom
    float horizontal_border = border[1] + border[3];    // right + left
    float vertical_border = border[0] + border[2];      // top + bottom

    content_width = total_width - horizontal_padding - horizontal_border;
    content_height = total_height - vertical_padding - vertical_border;

    // Ensure non-negative (CSS spec: content area cannot be negative)
    content_width = std::max(0.0f, content_width);
    content_height = std::max(0.0f, content_height);
  } else {
    // content-box (default): total dimensions ARE content dimensions
    content_width = total_width;
    content_height = total_height;
  }
}

// ============================================================================
// CSS Positioning (Sprint 9)
// ============================================================================

/**
 * @brief Find nearest positioned ancestor
 */
NVGCSSElement *find_positioned_ancestor(NVGCSSRenderer *renderer, NVGCSSElement *element) {
  if (!element)
    return nullptr;

  NVGCSSElement *parent = nvgcssGetParent(renderer, element); // Start with parent
  while (parent) {
    std::string pos = parent->explicit_style.position;
    if (pos == "relative" || pos == "absolute" || pos == "fixed") {
      return parent;
    }
    parent = nvgcssGetParent(renderer, parent);
  }
  return nullptr;
}

/**
 * @brief Apply CSS positioning (relative, absolute, fixed)
 */
void apply_positioning(NVGCSSRenderer *renderer, NVGCSSElement *element, float viewport_width, float viewport_height) {
  std::string pos = element->explicit_style.position;

  if (pos == "static") {
    // No positioning, already handled by layout
    return;
  }

  // Ensure dimensions are in computed (for elements not laid out by flexbox/grid)
  if (!element->computed.is_computed) {
    // Copy explicit dimensions to computed
    if (element->explicit_style.width >= 0) {
      element->computed.width = element->explicit_style.width;
    } else {
      element->computed.width = 100.0f; // default
    }

    if (element->explicit_style.height >= 0) {
      element->computed.height = element->explicit_style.height;
    } else {
      element->computed.height = 100.0f; // default
    }

    // Copy explicit position to computed (for relative positioning base)
    // If no explicit position, inherit from parent as starting point
    if (element->explicit_style.x >= 0) {
      element->computed.x = element->explicit_style.x;
    } else {
      NVGCSSElement *parent = nvgcssGetParent(renderer, element);
      if (parent && parent->computed.is_computed) {
        element->computed.x = parent->computed.x;
      } else {
        element->computed.x = 0.0f;
      }
    }

    if (element->explicit_style.y >= 0) {
      element->computed.y = element->explicit_style.y;
    } else {
      NVGCSSElement *parent = nvgcssGetParent(renderer, element);
      if (parent && parent->computed.is_computed) {
        element->computed.y = parent->computed.y;
      } else {
        element->computed.y = 0.0f;
      }
    }

    element->computed.is_computed = true;
  }

  // Get current computed position (from layout)
  float x = element->computed.x;
  float y = element->computed.y;
  float width = element->computed.width;
  float height = element->computed.height;

  // For positioned elements without explicit x/y, inherit parent position
  // This needs to happen AFTER layout but BEFORE applying offsets
  if (pos != "static" && element->explicit_style.x < 0 && element->explicit_style.y < 0) {
    NVGCSSElement *parent = nvgcssGetParent(renderer, element);
    if (parent && parent->computed.is_computed) {
      x = parent->computed.x;
      y = parent->computed.y;
    }
  }

  if (pos == "relative") {
    // Relative to normal position
    if (element->explicit_style.left >= 0) {
      x += element->explicit_style.left;
    } else if (element->explicit_style.right >= 0) {
      x -= element->explicit_style.right;
    }

    if (element->explicit_style.top >= 0) {
      y += element->explicit_style.top;
    } else if (element->explicit_style.bottom >= 0) {
      y -= element->explicit_style.bottom;
    }
  } else if (pos == "absolute") {
    // Find nearest positioned ancestor
    NVGCSSElement *positioned_parent = find_positioned_ancestor(renderer, element);

    float container_x = 0;
    float container_y = 0;
    float container_width = viewport_width;
    float container_height = viewport_height;

    if (positioned_parent) {
      container_x = positioned_parent->computed.x;
      container_y = positioned_parent->computed.y;
      container_width = positioned_parent->computed.width;
      container_height = positioned_parent->computed.height;
    }

    // Position relative to container (computed.x/y are absolute screen coordinates)
    if (element->explicit_style.left >= 0) {
      x = container_x + element->explicit_style.left;
    } else if (element->explicit_style.right >= 0) {
      x = container_x + container_width - width - element->explicit_style.right;
    } else {
      // No left/right specified - default to left edge of container
      x = container_x;
    }

    if (element->explicit_style.top >= 0) {
      y = container_y + element->explicit_style.top;
    } else if (element->explicit_style.bottom >= 0) {
      y = container_y + container_height - height - element->explicit_style.bottom;
    } else {
      // No top/bottom specified - default to top edge of container
      y = container_y;
    }
  } else if (pos == "fixed") {
    // Position relative to viewport
    if (element->explicit_style.left >= 0) {
      x = element->explicit_style.left;
    } else if (element->explicit_style.right >= 0) {
      x = viewport_width - width - element->explicit_style.right;
    }

    if (element->explicit_style.top >= 0) {
      y = element->explicit_style.top;
    } else if (element->explicit_style.bottom >= 0) {
      y = viewport_height - height - element->explicit_style.bottom;
    }
  }

  // Update computed position
  element->computed.x = x;
  element->computed.y = y;
}

/**
 * @brief Apply positioning recursively to all elements
 */
void apply_positioning_recursive(NVGCSSRenderer *renderer, NVGCSSElement *element, float viewport_width,
                                 float viewport_height) {
  // Apply positioning to this element
  apply_positioning(renderer, element, viewport_width, viewport_height);

  // Recursively apply to children
  // IMPORTANT: Copy children locally before recursing, because nvgcssGetChildren
  // uses a static cache that gets overwritten by recursive calls
  int child_count = 0;
  NVGCSSElement** children_ptr = nvgcssGetChildren(renderer, element, &child_count);
  std::vector<NVGCSSElement*> children_copy(children_ptr, children_ptr + child_count);
  for (NVGCSSElement* child : children_copy) {
    apply_positioning_recursive(renderer, child, viewport_width, viewport_height);
  }
}

void NVGCSSLayoutEngine::compute_layout(const std::vector<NVGCSSElement *> &roots,
                                        NVGCSSRenderer *renderer) {

  for (size_t i = 0; i < roots.size(); i++) {
    auto *root = roots[i];

    try {
      // Compute TYPED style (60fps refactor - NO string maps!)
      root->style = renderer->stylesheet->compute_style_typed(
          root->id, root->type, root->classes, root->attributes, root->pseudo_states,
          root->inline_style, nullptr, root->child_index, root->total_siblings);

      compute_element_layout(root, renderer);

    } catch (const std::exception &e) {
      //
    }
  }

  // Sprint 9: Apply positioning (relative, absolute, fixed) after layout
  for (auto *root : roots) {
    apply_positioning_recursive(renderer, root, viewport_width_, viewport_height_);
  }
}

void NVGCSSLayoutEngine::compute_element_layout(
    NVGCSSElement *element,
    NVGCSSRenderer *renderer) {
  static int recursion_depth = 0;
  static int element_count = 0;

  recursion_depth++;
  element_count++;

  if (recursion_depth > 100) {

    recursion_depth--;
    return;
  }

  // 60fps Optimization: Check dirty flags - skip if layout is clean
  if (!(element->dirty_flags & nvgcss::DIRTY_LAYOUT)) {
    // Layout is clean - skip computation, but still process children
    // (children might be dirty even if parent is clean)
    int child_count = 0;
    NVGCSSElement** children_ptr = nvgcssGetChildren(renderer, element, &child_count);
    std::vector<NVGCSSElement*> children_copy(children_ptr, children_ptr + child_count);
    for (auto* child : children_copy) {
      compute_element_layout(child, renderer);
    }
    recursion_depth--;
    return;
  }

  // Step 1: Compute box model
  try {
    compute_box_model(element, renderer);
  } catch (const std::exception &e) {
    // Log error but continue - element will use default box model
    recursion_depth--;
    return;
  }

  // Step 2: Check for flexbox or grid layout
  // NEW: Use typed property instead of string comparison
  if (element->style.display == nvgcss::Display::GRID) {

    // FIX: Apply positioning to the grid container BEFORE laying out children
    // This ensures children are positioned relative to the correct parent coordinates
    if (element->explicit_style.position != "static") {
      apply_positioning(renderer, element, viewport_width_, viewport_height_);
    }

    // Pre-compute children's box models
    // IMPORTANT: Copy children locally before processing, because nvgcssGetChildren
    // uses a static cache that gets overwritten by recursive calls
    int child_count = 0;
    NVGCSSElement** children_ptr = nvgcssGetChildren(renderer, element, &child_count);
    std::vector<NVGCSSElement*> children_copy(children_ptr, children_ptr + child_count);
    for (auto* child : children_copy) {
      // Compute typed style
      child->style = renderer->stylesheet->compute_style_typed(
          child->id, child->type, child->classes, child->attributes, child->pseudo_states,
          child->inline_style, &element->style, child->child_index, child->total_siblings);

      compute_box_model(child, renderer);
    }

    // Use grid layout for this container
    compute_grid_layout(element, renderer);

    // FIX: Recursively layout ALL children (not just flex/grid containers)
    // Children may have their own children that need layout, or percentage-based dimensions
    for (auto* child : children_copy) {
      // Typed style already computed above
      compute_element_layout(child, renderer);
    }

    // Apply transforms to this element
    compute_transforms(element, renderer);

    recursion_depth--;
    return;
  }
  // Check for flexbox layout
  else if (element->style.display == nvgcss::Display::FLEX) {

    // FIX: Apply positioning to the flex container BEFORE laying out children
    // This ensures children are positioned relative to the correct parent coordinates
    if (element->explicit_style.position != "static") {
      apply_positioning(renderer, element, viewport_width_, viewport_height_);
    }

    // IMPORTANT: Pre-compute children's box models so flexbox knows their sizes
    // Copy children locally before processing, because nvgcssGetChildren
    // uses a static cache that gets overwritten by recursive calls
    int child_count = 0;
    NVGCSSElement** children_ptr = nvgcssGetChildren(renderer, element, &child_count);
    std::vector<NVGCSSElement*> children_copy(children_ptr, children_ptr + child_count);
    for (auto* child : children_copy) {
      // Compute typed style
      child->style = renderer->stylesheet->compute_style_typed(
          child->id, child->type, child->classes, child->attributes, child->pseudo_states,
          child->inline_style, &element->style, child->child_index, child->total_siblings);

      compute_box_model(child, renderer);
    }

    // Use flexbox layout for this container
    compute_flexbox_layout(element, renderer);

    // FIX: Recursively layout ALL children (not just flex/grid containers)
    // Children may have their own children that need layout, or percentage-based dimensions
    for (auto* child : children_copy) {
      // Typed style already computed above
      compute_element_layout(child, renderer);
    }

    // Apply transforms to this element
    compute_transforms(element, renderer);

    // 60fps: Clear layout dirty flag - computation complete
    element->dirty_flags &= ~nvgcss::DIRTY_LAYOUT;

    recursion_depth--;
    return;
  }

  // Recursively layout children
  // IMPORTANT: Copy children locally before processing, because nvgcssGetChildren
  // uses a static cache that gets overwritten by recursive calls
  int child_count = 0;
  NVGCSSElement** children_ptr = nvgcssGetChildren(renderer, element, &child_count);
  std::vector<NVGCSSElement*> children_copy(children_ptr, children_ptr + child_count);
  for (auto* child : children_copy) {
    try {
      // Compute typed style
      child->style = renderer->stylesheet->compute_style_typed(
          child->id, child->type, child->classes, child->attributes, child->pseudo_states,
          child->inline_style, &element->style, child->child_index, child->total_siblings);

      compute_element_layout(child, renderer);
    } catch (const std::exception &e) {
    }
  }

  // Step 4: Apply transforms
  compute_transforms(element, renderer);

  // 60fps: Clear layout dirty flag - computation complete
  element->dirty_flags &= ~nvgcss::DIRTY_LAYOUT;

  recursion_depth--;
}

void NVGCSSLayoutEngine::compute_box_model(NVGCSSElement *element,
                                           NVGCSSRenderer *renderer) {
  static int box_model_count = 0;
  box_model_count++;

  // NEW: Use typed position property (no string comparison!)
  // Map typed enum to legacy string (temporary - will remove explicit_style later)
  switch (element->style.position) {
    case nvgcss::Position::STATIC:   element->explicit_style.position = "static"; break;
    case nvgcss::Position::RELATIVE: element->explicit_style.position = "relative"; break;
    case nvgcss::Position::ABSOLUTE: element->explicit_style.position = "absolute"; break;
    case nvgcss::Position::FIXED:    element->explicit_style.position = "fixed"; break;
  }

  // NEW: Use typed properties for positioning
  bool is_positioned = (element->style.position != nvgcss::Position::STATIC);

  // Parse positioning offsets from typed properties
  if (!is_positioned) {
    // Static: left/top are absolute coordinates
    // Also check for non-standard "x" property for backward compatibility
    auto x_it = element->inline_style.find("x");
    if (x_it != element->inline_style.end()) {
      float x = nvgcss_utils::parse_length(x_it->second, viewport_width_);
      element->computed.x = x;
      element->explicit_style.x = x;
    } else if (!element->style.left.is_auto()) {
      float x = element->style.left.resolve(viewport_width_, root_font_size_, viewport_width_);
      element->computed.x = x;
      element->explicit_style.x = x;
    } else {
      element->explicit_style.x = -1.0f; // auto
    }

    // Also check for non-standard "y" property for backward compatibility
    auto y_it = element->inline_style.find("y");
    if (y_it != element->inline_style.end()) {
      float y = nvgcss_utils::parse_length(y_it->second, viewport_height_);
      element->computed.y = y;
      element->explicit_style.y = y;
    } else if (!element->style.top.is_auto()) {
      float y = element->style.top.resolve(viewport_height_, root_font_size_, viewport_height_);
      element->computed.y = y;
      element->explicit_style.y = y;
    } else {
      element->explicit_style.y = -1.0f; // auto
    }
  } else {
    // Positioned: offsets will be applied later by apply_positioning
    element->explicit_style.x = -1.0f;
    element->explicit_style.y = -1.0f;
  }

  // Dimensions (parse as "total" dimensions - may include padding/border depending on box-sizing)
  float total_width = 100.0f;  // Default
  float total_height = 100.0f; // Default

  // Sprint 19: For percentage widths, use parent's content width as context
  float context_width = viewport_width_;
  float context_height = viewport_height_;

  NVGCSSElement *parent = nvgcssGetParent(renderer, element);
  if (parent && parent->computed.is_computed) {
    // Use parent's computed content dimensions for percentage resolution
    context_width = parent->computed.content_width;
    context_height = parent->computed.content_height;
  }

  // FIX: If element was already laid out by flexbox/grid, preserve its intrinsic dimensions
  // Only parse dimensions from CSS if not already computed
  bool preserve_width = (element->computed.is_computed &&
                        (element->computed.source == NVGCSSComputedLayout::FLEXBOX ||
                         element->computed.source == NVGCSSComputedLayout::GRID));
  bool preserve_height = (element->computed.is_computed &&
                         (element->computed.source == NVGCSSComputedLayout::FLEXBOX ||
                          element->computed.source == NVGCSSComputedLayout::GRID));

  // NEW: Use TYPED properties (NO string parsing!)
  if (!element->style.width.is_auto()) {
    total_width = element->style.width.resolve(context_width, root_font_size_, viewport_width_);
    element->explicit_style.width = total_width;
  } else {
    element->explicit_style.width = -1.0f; // auto
  }

  if (!element->style.height.is_auto()) {
    total_height = element->style.height.resolve(context_height, root_font_size_, viewport_height_);
    element->explicit_style.height = total_height;
  } else {
    element->explicit_style.height = -1.0f; // auto
  }

  // NEW: Padding from TYPED properties (already resolved in ComputedStyle)
  for (int i = 0; i < 4; i++) {
    element->explicit_style.padding[i] = element->style.padding[i].resolve(
        context_width, root_font_size_, viewport_width_);
  }

  // NEW: Margin from TYPED properties
  for (int i = 0; i < 4; i++) {
    element->explicit_style.margin[i] = element->style.margin[i].resolve(
        context_width, root_font_size_, viewport_width_);
  }

  // NEW: Border width from TYPED properties (already in pixels in ComputedStyle)
  for (int i = 0; i < 4; i++) {
    element->explicit_style.border_width[i] = element->style.border.width[i];
  }

  // NEW: Box-sizing from TYPED property (enum to string for backward compat)
  switch (element->style.box_sizing) {
    case nvgcss::BoxSizing::BORDER_BOX:
      element->explicit_style.box_sizing = "border-box";
      break;
    case nvgcss::BoxSizing::CONTENT_BOX:
    default:
      element->explicit_style.box_sizing = "content-box";
      break;
  }

  // Sprint 19: Calculate content dimensions based on box-sizing
  float content_width, content_height;
  calculate_content_dimensions(total_width, total_height, element->explicit_style.padding,
                               element->explicit_style.border_width, // Sprint 35: Use parsed border width
                               element->explicit_style.box_sizing, content_width, content_height);

  // Store content dimensions in computed
  // FIX: Preserve flexbox/grid computed dimensions to avoid layout loop
  if (!preserve_width) {
    element->computed.width = content_width;
  }
  if (!preserve_height) {
    element->computed.height = content_height;
  }

  // NEW: Min/Max dimensions from TYPED properties
  if (!element->style.min_width.is_auto()) {
    element->explicit_style.min_width = element->style.min_width.resolve(
        viewport_width_, root_font_size_, viewport_width_);
  } else {
    element->explicit_style.min_width = -1.0f; // no minimum
  }

  if (!element->style.max_width.is_auto()) {
    element->explicit_style.max_width = element->style.max_width.resolve(
        viewport_width_, root_font_size_, viewport_width_);
  } else {
    element->explicit_style.max_width = -1.0f; // no maximum
  }

  if (!element->style.min_height.is_auto()) {
    element->explicit_style.min_height = element->style.min_height.resolve(
        viewport_height_, root_font_size_, viewport_height_);
  } else {
    element->explicit_style.min_height = -1.0f; // no minimum
  }

  if (!element->style.max_height.is_auto()) {
    element->explicit_style.max_height = element->style.max_height.resolve(
        viewport_height_, root_font_size_, viewport_height_);
  } else {
    element->explicit_style.max_height = -1.0f; // no maximum
  }

  // Sprint 18: Apply min/max constraints to computed dimensions
  // FIX: Only apply constraints if not already computed by flexbox/grid
  if (!preserve_width) {
    element->computed.width = apply_dimension_constraints(element->computed.width, element->explicit_style.min_width,
                                            element->explicit_style.max_width);
  }
  if (!preserve_height) {
    element->computed.height = apply_dimension_constraints(element->computed.height, element->explicit_style.min_height,
                                             element->explicit_style.max_height);
  }

  // FIX: Initialize computed.x and computed.y for positioned elements
  // These will be adjusted later by apply_positioning, but need initial values
  if (element->explicit_style.position != "static") {
    // For positioned elements, initialize to parent's position or 0
    NVGCSSElement *parent = nvgcssGetParent(renderer, element);
    if (element->explicit_style.x >= 0) {
      element->computed.x = element->explicit_style.x;
    } else if (parent && parent->computed.is_computed) {
      element->computed.x = parent->computed.x;
    } else {
      element->computed.x = 0.0f;
    }

    if (element->explicit_style.y >= 0) {
      element->computed.y = element->explicit_style.y;
    } else if (parent && parent->computed.is_computed) {
      element->computed.y = parent->computed.y;
    } else {
      element->computed.y = 0.0f;
    }
  }

  // Sprint 18: Write constrained dimensions to computed for regular elements
  // (Grid and Flexbox will override this with their own layouts)
  if (!element->computed.is_computed) {
    element->computed.content_width = element->computed.width;
    element->computed.content_height = element->computed.height;
    element->computed.source = NVGCSSComputedLayout::CSS_EXPLICIT;
    element->computed.is_computed = true;
  }

  // NEW: Border radius - resolve percentages marked as negative values
  // CSS parser stores percentages as negative to defer resolution until layout
  for (int i = 0; i < 4; i++) {
    float radius = element->style.border.radius[i];
    if (radius < 0) {
      // Negative value marks a percentage - resolve based on element dimensions
      // Use the smaller of width/height for circular corners (CSS spec)
      float radius_context = std::min(element->computed.width, element->computed.height);
      element->explicit_style.border_radius[i] = (-radius / 100.0f) * radius_context;
    } else {
      // Already resolved (px, em, etc.)
      element->explicit_style.border_radius[i] = radius;
    }
  }
  // NEW: Border styles from TYPED properties (enum to string for backward compat)
  auto border_style_to_string = [](nvgcss::BorderStyle s) -> std::string {
    switch (s) {
      case nvgcss::BorderStyle::SOLID: return "solid";
      case nvgcss::BorderStyle::DASHED: return "dashed";
      case nvgcss::BorderStyle::DOTTED: return "dotted";
      case nvgcss::BorderStyle::DOUBLE: return "double";
      case nvgcss::BorderStyle::HIDDEN: return "hidden";
      case nvgcss::BorderStyle::NONE:
      default: return "none";
    }
  };

  element->explicit_style.border_top_style = border_style_to_string(element->style.border.style[0]);
  element->explicit_style.border_right_style = border_style_to_string(element->style.border.style[1]);
  element->explicit_style.border_bottom_style = border_style_to_string(element->style.border.style[2]);
  element->explicit_style.border_left_style = border_style_to_string(element->style.border.style[3]);

  // NEW: Border colors from TYPED properties (NVGcolor to string for backward compat)
  // TODO: Refactor explicit_style to use NVGcolor directly instead of strings
  auto color_to_hex = [](NVGcolor c) -> std::string {
    char buf[32];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x", (int)(c.r * 255), (int)(c.g * 255), (int)(c.b * 255));
    return std::string(buf);
  };

  element->explicit_style.border_top_color = color_to_hex(element->style.border.color[0]);
  element->explicit_style.border_right_color = color_to_hex(element->style.border.color[1]);
  element->explicit_style.border_bottom_color = color_to_hex(element->style.border.color[2]);
  element->explicit_style.border_left_color = color_to_hex(element->style.border.color[3]);

  // NEW: Visibility from TYPED property
  element->visible = (element->style.display != nvgcss::Display::NONE);

  // TODO: Move box-shadow and text-shadow to typed property system
  // Not critical for 60fps - these are not in hot path
  element->box_shadows.clear();
  element->text_shadows.clear();

  // Grid properties: Read from typed system (grid.cpp reads element->style.grid_template_rows directly)
  // Fallback to inline_style for backward compatibility with tests
  if (element->inline_style.count("grid-template-rows")) {
    element->explicit_style.grid_template_rows = element->inline_style.at("grid-template-rows");
  }
  if (element->inline_style.count("grid-template-columns")) {
    element->explicit_style.grid_template_columns = element->inline_style.at("grid-template-columns");
  }
  if (element->inline_style.count("grid-auto-rows")) {
    element->explicit_style.grid_auto_rows = element->inline_style.at("grid-auto-rows");
  }
  if (element->inline_style.count("grid-auto-columns")) {
    element->explicit_style.grid_auto_columns = element->inline_style.at("grid-auto-columns");
  }

  // grid-auto-rows/columns not in typed system yet - read from CSS computed style
  // TODO: Add to typed ComputedStyle in future phase
  if (element->explicit_style.grid_auto_rows.empty() ||
      element->explicit_style.grid_auto_columns.empty()) {
    auto css_style = renderer->stylesheet->compute_style(
        element->id, element->type, element->classes, element->attributes,
        element->pseudo_states, element->inline_style, {},
        element->child_index, element->total_siblings);
    if (element->explicit_style.grid_auto_rows.empty()) {
      auto it = css_style.find("grid-auto-rows");
      if (it != css_style.end()) {
        element->explicit_style.grid_auto_rows = it->second;
      }
    }
    if (element->explicit_style.grid_auto_columns.empty()) {
      auto it = css_style.find("grid-auto-columns");
      if (it != css_style.end()) {
        element->explicit_style.grid_auto_columns = it->second;
      }
    }
  }

  // Gap Shorthand (Sprint 21) - Parse BEFORE individual gap properties
  if (element->inline_style.count("gap")) {
    std::string gap_value = element->inline_style.at("gap");
    auto gap_parts = nvgcss_utils::split(gap_value, ' ');

    if (gap_parts.size() == 1) {
      // Single value - applies to both row and column
      if (gap_parts[0].find("px") != std::string::npos) {
        float gap = safe_stof(gap_parts[0].substr(0, gap_parts[0].find("px")));
        element->explicit_style.grid_row_gap = gap;
        element->explicit_style.grid_column_gap = gap;
      }
    } else if (gap_parts.size() == 2) {
      // Two values - row-gap column-gap
      if (gap_parts[0].find("px") != std::string::npos) {
        element->explicit_style.grid_row_gap =
            safe_stof(gap_parts[0].substr(0, gap_parts[0].find("px")));
      }
      if (gap_parts[1].find("px") != std::string::npos) {
        element->explicit_style.grid_column_gap =
            safe_stof(gap_parts[1].substr(0, gap_parts[1].find("px")));
      }
    }
  }

  // Longhand properties override shorthand
  if (element->inline_style.count("row-gap") || element->inline_style.count("grid-row-gap")) {
    std::string gap_str = element->inline_style.count("row-gap") ? element->inline_style.at("row-gap") : element->inline_style.at("grid-row-gap");
    if (gap_str.find("px") != std::string::npos) {
      element->explicit_style.grid_row_gap = safe_stof(gap_str.substr(0, gap_str.find("px")));
    }
  }
  if (element->inline_style.count("column-gap") || element->inline_style.count("grid-column-gap")) {
    std::string gap_str =
        element->inline_style.count("column-gap") ? element->inline_style.at("column-gap") : element->inline_style.at("grid-column-gap");
    if (gap_str.find("px") != std::string::npos) {
      element->explicit_style.grid_column_gap = safe_stof(gap_str.substr(0, gap_str.find("px")));
    }
  }

  // Parse grid item properties into explicit_style (Sprint 12: with span support, Sprint 26: skip
  // line names)
  auto parse_grid_value = [](const std::string &val, int &line, int &span) {
    if (val == "auto" || val.empty())
      return;

    // Check for "span N" syntax
    size_t span_pos = val.find("span");
    if (span_pos != std::string::npos) {
      try {
        // Extract number after "span"
        std::string num_str = val.substr(span_pos + 4);
        // Remove whitespace
        num_str.erase(0, num_str.find_first_not_of(" 	"));
        span = std::stoi(num_str);
        line = -1; // Mark as auto-placed with span
      } catch (const std::exception&) {
        // Invalid span value, default to 1
        span = 1;
      }
    } else {
      // Check if it's a number (Sprint 26: skip line names)
      bool is_number = true;
      for (size_t i = 0; i < val.length(); i++) {
        char c = val[i];
        if (std::isspace(c))
          continue; // Skip whitespace
        if (i == 0 && c == '-')
          continue; // Allow negative sign
        if (!std::isdigit(c)) {
          is_number = false;
          break;
        }
      }

      if (is_number) {
        try {
          line = std::stoi(val);
          span = 1;
        } catch (const std::exception&) {
          // Invalid line number - leave at default
        }
      }
      // If not a number, it's a line name - leave line as 0 (will be resolved in grid layout)
    }
  };

  // Parse shorthand grid-row and grid-column first (can be overridden by longhands)
  if (element->inline_style.count("grid-row")) {
    std::string value = element->inline_style.at("grid-row");
    size_t slash_pos = value.find('/');
    if (slash_pos != std::string::npos) {
      // grid-row: start / end
      std::string start_str = value.substr(0, slash_pos);
      std::string end_str = value.substr(slash_pos + 1);
      // Trim whitespace
      start_str.erase(0, start_str.find_first_not_of(" \t"));
      start_str.erase(start_str.find_last_not_of(" \t") + 1);
      end_str.erase(0, end_str.find_first_not_of(" \t"));
      end_str.erase(end_str.find_last_not_of(" \t") + 1);
      parse_grid_value(start_str, element->explicit_style.grid_row_start,
                       element->explicit_style.grid_row_span);
      parse_grid_value(end_str, element->explicit_style.grid_row_end,
                       element->explicit_style.grid_row_span);
    } else {
      // grid-row: start (implies span 1)
      parse_grid_value(value, element->explicit_style.grid_row_start,
                       element->explicit_style.grid_row_span);
    }
  }
  if (element->inline_style.count("grid-column")) {
    std::string value = element->inline_style.at("grid-column");
    size_t slash_pos = value.find('/');
    if (slash_pos != std::string::npos) {
      // grid-column: start / end
      std::string start_str = value.substr(0, slash_pos);
      std::string end_str = value.substr(slash_pos + 1);
      // Trim whitespace
      start_str.erase(0, start_str.find_first_not_of(" \t"));
      start_str.erase(start_str.find_last_not_of(" \t") + 1);
      end_str.erase(0, end_str.find_first_not_of(" \t"));
      end_str.erase(end_str.find_last_not_of(" \t") + 1);
      parse_grid_value(start_str, element->explicit_style.grid_column_start,
                       element->explicit_style.grid_column_span);
      parse_grid_value(end_str, element->explicit_style.grid_column_end,
                       element->explicit_style.grid_column_span);
    } else {
      // grid-column: start (implies span 1)
      parse_grid_value(value, element->explicit_style.grid_column_start,
                       element->explicit_style.grid_column_span);
    }
  }

  // Longhand properties override shorthand
  if (element->inline_style.count("grid-row-start")) {
    parse_grid_value(element->inline_style.at("grid-row-start"), element->explicit_style.grid_row_start,
                     element->explicit_style.grid_row_span);
  }
  if (element->inline_style.count("grid-row-end")) {
    // For -end properties, if it contains "span", update the span field
    parse_grid_value(element->inline_style.at("grid-row-end"), element->explicit_style.grid_row_end,
                     element->explicit_style.grid_row_span);
  }
  if (element->inline_style.count("grid-column-start")) {
    parse_grid_value(element->inline_style.at("grid-column-start"), element->explicit_style.grid_column_start,
                     element->explicit_style.grid_column_span);
  }
  if (element->inline_style.count("grid-column-end")) {
    // For -end properties, if it contains "span", update the span field
    parse_grid_value(element->inline_style.at("grid-column-end"), element->explicit_style.grid_column_end,
                     element->explicit_style.grid_column_span);
  }

  // Grid template areas (Sprint 13)
  if (element->inline_style.count("grid-template-areas")) {
    element->explicit_style.grid_template_areas = element->inline_style.at("grid-template-areas");
  }
  if (element->inline_style.count("grid-area")) {
    element->explicit_style.grid_area = element->inline_style.at("grid-area");
  }

  if (element->inline_style.count("grid-auto-flow")) {
    element->explicit_style.grid_auto_flow = element->inline_style.at("grid-auto-flow");
  }

  // grid-template-areas/grid-area/grid-auto-flow not in typed system yet - read from CSS computed style
  // TODO: Add to typed ComputedStyle in future phase
  if (element->explicit_style.grid_template_areas.empty() ||
      element->explicit_style.grid_area.empty() ||
      element->explicit_style.grid_auto_flow.empty()) {
    auto css_style = renderer->stylesheet->compute_style(
        element->id, element->type, element->classes, element->attributes,
        element->pseudo_states, element->inline_style, {},
        element->child_index, element->total_siblings);
    if (element->explicit_style.grid_template_areas.empty()) {
      auto it = css_style.find("grid-template-areas");
      if (it != css_style.end()) {
        element->explicit_style.grid_template_areas = it->second;
      }
    }
    if (element->explicit_style.grid_area.empty()) {
      auto it = css_style.find("grid-area");
      if (it != css_style.end()) {
        element->explicit_style.grid_area = it->second;
      }
    }
    if (element->explicit_style.grid_auto_flow.empty()) {
      auto it = css_style.find("grid-auto-flow");
      if (it != css_style.end()) {
        element->explicit_style.grid_auto_flow = it->second;
      }
    }
  }

  // NEW: Positioning offsets from TYPED properties
  if (!element->style.top.is_auto()) {
    element->explicit_style.top = element->style.top.resolve(
        viewport_height_, root_font_size_, viewport_height_);
  } else {
    element->explicit_style.top = -1.0f; // auto
  }

  if (!element->style.right.is_auto()) {
    element->explicit_style.right = element->style.right.resolve(
        viewport_width_, root_font_size_, viewport_width_);
  } else {
    element->explicit_style.right = -1.0f; // auto
  }

  if (!element->style.bottom.is_auto()) {
    element->explicit_style.bottom = element->style.bottom.resolve(
        viewport_height_, root_font_size_, viewport_height_);
  } else {
    element->explicit_style.bottom = -1.0f; // auto
  }

  if (!element->style.left.is_auto()) {
    element->explicit_style.left = element->style.left.resolve(
        viewport_width_, root_font_size_, viewport_width_);
  } else {
    element->explicit_style.left = -1.0f; // auto
  }

  // NEW: Z-index from TYPED property
  element->explicit_style.z_index = element->style.z_index;

  // NEW: Overflow from TYPED properties (enum to string for backward compat)
  auto overflow_to_string = [](nvgcss::Overflow o) -> std::string {
    switch (o) {
      case nvgcss::Overflow::HIDDEN: return "hidden";
      case nvgcss::Overflow::SCROLL: return "scroll";
      case nvgcss::Overflow::AUTO: return "auto";
      case nvgcss::Overflow::VISIBLE:
      default: return "visible";
    }
  };

  element->explicit_style.overflow_x = overflow_to_string(element->style.overflow_x);
  element->explicit_style.overflow_y = overflow_to_string(element->style.overflow_y);
  // Set overflow shorthand to x value (convention)
  element->explicit_style.overflow = element->explicit_style.overflow_x;

  // NEW: Opacity from TYPED property
  element->opacity = element->style.opacity;

  // Copy box model properties from explicit_style to computed
  // This ensures computed values are available for rendering
  for (int i = 0; i < 4; i++) {
    element->computed.padding[i] = element->explicit_style.padding[i];
    element->computed.margin[i] = element->explicit_style.margin[i];
    element->computed.border[i] = element->explicit_style.border_width[i];
    element->computed.border_radius[i] = element->explicit_style.border_radius[i];
  }
}

void NVGCSSLayoutEngine::position_children(NVGCSSElement *element) {
  // TODO: Flow and stack layouts in phase 2
}

void NVGCSSLayoutEngine::compute_transforms(NVGCSSElement *element,
                                            NVGCSSRenderer *renderer) {
  // Start with identity transform
  nvgTransformIdentity(element->transform);

  // TODO: Port transforms to typed property system
  // Transforms are complex and not critical for initial 60fps work
  // Will be added in future phase
}

float NVGCSSLayoutEngine::resolve_length(const std::string &value, float context_value,
                                         float font_size) {
  return nvgcss_utils::parse_length(value, context_value);
}

NVGCSSLayoutEngine::BoxValues NVGCSSLayoutEngine::parse_box_values(const std::string &value) {
  BoxValues result = {0, 0, 0, 0};

  // Sprint 24: Check if value contains calc() - if so, don't split by space
  // because calc() expressions can have spaces inside them
  if (value.find("calc(") != std::string::npos) {
    // Single value with calc() - apply to all sides
    float val = nvgcss_utils::parse_length(value, 0);
    result.top = result.right = result.bottom = result.left = val;
    return result;
  }

  auto parts = nvgcss_utils::split(value, ' ');

  if (parts.size() == 1) {
    // All sides same
    float val = nvgcss_utils::parse_length(parts[0], 0);
    result.top = result.right = result.bottom = result.left = val;
  } else if (parts.size() == 2) {
    // vertical horizontal
    float vertical = nvgcss_utils::parse_length(parts[0], 0);
    float horizontal = nvgcss_utils::parse_length(parts[1], 0);
    result.top = result.bottom = vertical;
    result.left = result.right = horizontal;
  } else if (parts.size() == 4) {
    // top right bottom left
    result.top = nvgcss_utils::parse_length(parts[0], 0);
    result.right = nvgcss_utils::parse_length(parts[1], 0);
    result.bottom = nvgcss_utils::parse_length(parts[2], 0);
    result.left = nvgcss_utils::parse_length(parts[3], 0);
  }

  return result;
}
