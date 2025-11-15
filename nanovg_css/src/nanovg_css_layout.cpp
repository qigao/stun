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
  } catch (...) {
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
NVGCSSElement *find_positioned_ancestor(NVGCSSElement *element) {
  if (!element)
    return nullptr;

  element = element->parent; // Start with parent
  while (element) {
    std::string pos = element->explicit_style.position;
    if (pos == "relative" || pos == "absolute" || pos == "fixed") {
      return element;
    }
    element = element->parent;
  }
  return nullptr;
}

/**
 * @brief Apply CSS positioning (relative, absolute, fixed)
 */
void apply_positioning(NVGCSSElement *element, float viewport_width, float viewport_height) {
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
    } else if (element->parent && element->parent->computed.is_computed) {
      element->computed.x = element->parent->computed.x;
    } else {
      element->computed.x = 0.0f;
    }

    if (element->explicit_style.y >= 0) {
      element->computed.y = element->explicit_style.y;
    } else if (element->parent && element->parent->computed.is_computed) {
      element->computed.y = element->parent->computed.y;
    } else {
      element->computed.y = 0.0f;
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
    if (element->parent && element->parent->computed.is_computed) {
      x = element->parent->computed.x;
      y = element->parent->computed.y;
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
    NVGCSSElement *positioned_parent = find_positioned_ancestor(element);

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

    // Position relative to container
    if (element->explicit_style.left >= 0) {
      x = container_x + element->explicit_style.left;
    } else if (element->explicit_style.right >= 0) {
      x = container_x + container_width - width - element->explicit_style.right;
    }

    if (element->explicit_style.top >= 0) {
      y = container_y + element->explicit_style.top;
    } else if (element->explicit_style.bottom >= 0) {
      y = container_y + container_height - height - element->explicit_style.bottom;
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

  // Sync to box for compatibility
  element->box.x = x;
  element->box.y = y;
}

/**
 * @brief Apply positioning recursively to all elements
 */
void apply_positioning_recursive(NVGCSSElement *element, float viewport_width,
                                 float viewport_height) {
  // Apply positioning to this element
  apply_positioning(element, viewport_width, viewport_height);

  // Recursively apply to children
  for (auto *child : element->children) {
    apply_positioning_recursive(child, viewport_width, viewport_height);
  }
}

void NVGCSSLayoutEngine::compute_layout(const std::vector<NVGCSSElement *> &roots,
                                        NVGCSSRenderer *renderer) {

  for (size_t i = 0; i < roots.size(); i++) {
    auto *root = roots[i];

    try {
      // Get computed style for this element
      auto computed_style = renderer->stylesheet->compute_style(
          root->id, root->type, root->classes, root->attributes, root->pseudo_states,
          root->inline_style, {}, root->child_index, root->total_siblings);

      compute_element_layout(root, computed_style, renderer);

    } catch (const std::exception &e) {
      //
    }
  }

  // Sprint 9: Apply positioning (relative, absolute, fixed) after layout
  for (auto *root : roots) {
    apply_positioning_recursive(root, viewport_width_, viewport_height_);
  }
}

void NVGCSSLayoutEngine::compute_element_layout(
    NVGCSSElement *element, const std::map<std::string, std::string> &computed_style,
    NVGCSSRenderer *renderer) {
  static int recursion_depth = 0;
  static int element_count = 0;

  recursion_depth++;
  element_count++;

  if (recursion_depth > 100) {

    recursion_depth--;
    return;
  }

  // Step 1: Compute box model
  try {
    compute_box_model(element, computed_style);
  } catch (const std::exception &e) {

    recursion_depth--;
    return;
  } catch (...) {

    recursion_depth--;
    return;
  }

  // Step 2: Check for flexbox or grid layout
  auto display_it = computed_style.find("display");

  // PHASE 4 SPRINT 5: Check for grid layout
  if (display_it != computed_style.end() &&
      (display_it->second == "grid" || display_it->second == "inline-grid")) {

    // Pre-compute children's box models
    for (auto *child : element->children) {
      auto child_style = renderer->stylesheet->compute_style(
          child->id, child->type, child->classes, child->attributes, child->pseudo_states,
          child->inline_style, computed_style, child->child_index, child->total_siblings);
      compute_box_model(child, child_style);
    }

    // Use grid layout for this container
    compute_grid_layout(element, computed_style, renderer);

    // Apply transforms to this element
    compute_transforms(element, computed_style);

    recursion_depth--;
    return;
  }
  // Check for flexbox layout
  else if (display_it != computed_style.end() &&
           (display_it->second == "flex" || display_it->second == "inline-flex")) {

    // IMPORTANT: Pre-compute children's box models so flexbox knows their sizes
    for (auto *child : element->children) {
      auto child_style = renderer->stylesheet->compute_style(
          child->id, child->type, child->classes, child->attributes, child->pseudo_states,
          child->inline_style, computed_style, child->child_index, child->total_siblings);
      compute_box_model(child, child_style);
    }

    // Use flexbox layout for this container
    compute_flexbox_layout(element, computed_style, renderer);

    // Apply transforms to this element
    compute_transforms(element, computed_style);

    recursion_depth--;
    return;
  }

  // Step 4: Apply transforms
  compute_transforms(element, computed_style);

  // Recursively layout children
  for (auto *child : element->children) {
    try {
      auto child_style = renderer->stylesheet->compute_style(
          child->id, child->type, child->classes, child->attributes, child->pseudo_states,
          child->inline_style,
          computed_style, // parent style for inheritance
          child->child_index, child->total_siblings);

      compute_element_layout(child, child_style, renderer);
    } catch (const std::exception &e) {
    }
  }

  recursion_depth--;
}

void NVGCSSLayoutEngine::compute_box_model(NVGCSSElement *element,
                                           const std::map<std::string, std::string> &style) {
  static int box_model_count = 0;
  box_model_count++;

  auto &box = element->box;

  // Position (absolute by default)
  // Support both "left"/"top" (standard CSS) and "x"/"y" (legacy)
  if (style.count("left")) {
    std::string x_str = style.at("left");

    float x = resolve_length(x_str, viewport_width_, root_font_size_);

    box.x = x;
    element->explicit_style.x = x; // PHASE 4: Set explicit_style
  } else if (style.count("x")) {
    std::string x_str = style.at("x");

    float x = resolve_length(x_str, viewport_width_, root_font_size_);

    box.x = x;
    element->explicit_style.x = x; // PHASE 4: Set explicit_style
  } else {
    element->explicit_style.x = -1.0f; // auto
  }

  if (style.count("top")) {
    std::string y_str = style.at("top");

    float y = resolve_length(y_str, viewport_height_, root_font_size_);

    box.y = y;
    element->explicit_style.y = y; // PHASE 4: Set explicit_style
  } else if (style.count("y")) {
    std::string y_str = style.at("y");

    float y = resolve_length(y_str, viewport_height_, root_font_size_);

    box.y = y;
    element->explicit_style.y = y; // PHASE 4: Set explicit_style
  } else {
    element->explicit_style.y = -1.0f; // auto
  }

  // Dimensions (parse as "total" dimensions - may include padding/border depending on box-sizing)
  float total_width = 100.0f;  // Default
  float total_height = 100.0f; // Default

  // Sprint 19: For percentage widths, use parent's content width as context
  float context_width = viewport_width_;
  float context_height = viewport_height_;

  if (element->parent && element->parent->computed.is_computed) {
    // Use parent's computed content dimensions for percentage resolution
    context_width = element->parent->computed.content_width;
    context_height = element->parent->computed.content_height;
  }

  if (style.count("width")) {
    std::string width_str = style.at("width");

    total_width = resolve_length(width_str, context_width, root_font_size_);

    element->explicit_style.width = total_width; // PHASE 4: Set explicit_style
  } else {

    element->explicit_style.width = -1.0f; // auto
  }

  if (style.count("height")) {
    std::string height_str = style.at("height");

    total_height = resolve_length(height_str, context_height, root_font_size_);

    element->explicit_style.height = total_height; // PHASE 4: Set explicit_style
  } else {

    element->explicit_style.height = -1.0f; // auto
  }

  // Parse padding BEFORE applying box-sizing (needed for calculation)
  if (style.count("padding")) {
    auto padding = parse_box_values(style.at("padding"));
    box.padding[0] = padding.top;
    box.padding[1] = padding.right;
    box.padding[2] = padding.bottom;
    box.padding[3] = padding.left;

    // Sprint 24: Also store in explicit_style for test access
    element->explicit_style.padding[0] = padding.top;
    element->explicit_style.padding[1] = padding.right;
    element->explicit_style.padding[2] = padding.bottom;
    element->explicit_style.padding[3] = padding.left;
  } else {
    // Initialize to zero
    for (int i = 0; i < 4; i++) {
      box.padding[i] = 0.0f;
      element->explicit_style.padding[i] = 0.0f;
    }
  }

  // Sprint 37: Parse margin (shorthand with 1-4 values)
  if (style.count("margin")) {
    auto margin = parse_box_values(style.at("margin"));
    box.margin[0] = margin.top;
    box.margin[1] = margin.right;
    box.margin[2] = margin.bottom;
    box.margin[3] = margin.left;

    // Also store in explicit_style
    element->explicit_style.margin[0] = margin.top;
    element->explicit_style.margin[1] = margin.right;
    element->explicit_style.margin[2] = margin.bottom;
    element->explicit_style.margin[3] = margin.left;
  } else {
    // Initialize to zero
    for (int i = 0; i < 4; i++) {
      box.margin[i] = 0.0f;
      element->explicit_style.margin[i] = 0.0f;
    }
  }

  // Sprint 35: Parse border-width BEFORE box-sizing (needed for content calculation)
  if (style.count("border-width")) {
    auto border = parse_box_values(style.at("border-width"));
    box.border_width[0] = border.top;
    box.border_width[1] = border.right;
    box.border_width[2] = border.bottom;
    box.border_width[3] = border.left;

    // Also store in explicit_style for test access
    element->explicit_style.border_width[0] = border.top;
    element->explicit_style.border_width[1] = border.right;
    element->explicit_style.border_width[2] = border.bottom;
    element->explicit_style.border_width[3] = border.left;
  } else {
    // Initialize to zero if not set by any shorthand
    for (int i = 0; i < 4; i++) {
      box.border_width[i] = 0.0f;
      element->explicit_style.border_width[i] = 0.0f;
    }
  }

  // Parse box-sizing (Sprint 19)
  if (style.count("box-sizing")) {
    element->explicit_style.box_sizing = style.at("box-sizing");
  } else {
    element->explicit_style.box_sizing = "content-box"; // CSS default
  }

  // Sprint 19: Calculate content dimensions based on box-sizing
  float content_width, content_height;
  calculate_content_dimensions(total_width, total_height, box.padding,
                               box.border_width, // Sprint 35: Use parsed border width
                               element->explicit_style.box_sizing, content_width, content_height);

  // Store content dimensions in box
  box.width = content_width;
  box.height = content_height;

  // Min/Max Dimensions (Sprint 18) - Parse constraints
  if (style.count("min-width")) {
    float min_width = resolve_length(style.at("min-width"), viewport_width_, root_font_size_);
    element->explicit_style.min_width = min_width;
  } else {
    element->explicit_style.min_width = -1.0f; // no minimum
  }

  if (style.count("max-width")) {
    float max_width = resolve_length(style.at("max-width"), viewport_width_, root_font_size_);
    element->explicit_style.max_width = max_width;
  } else {
    element->explicit_style.max_width = -1.0f; // no maximum
  }

  if (style.count("min-height")) {
    float min_height = resolve_length(style.at("min-height"), viewport_height_, root_font_size_);
    element->explicit_style.min_height = min_height;
  } else {
    element->explicit_style.min_height = -1.0f; // no minimum
  }

  if (style.count("max-height")) {
    float max_height = resolve_length(style.at("max-height"), viewport_height_, root_font_size_);
    element->explicit_style.max_height = max_height;
  } else {
    element->explicit_style.max_height = -1.0f; // no maximum
  }

  // Sprint 18: Apply min/max constraints to computed dimensions
  box.width = apply_dimension_constraints(box.width, element->explicit_style.min_width,
                                          element->explicit_style.max_width);
  box.height = apply_dimension_constraints(box.height, element->explicit_style.min_height,
                                           element->explicit_style.max_height);

  // Sprint 18: Write constrained dimensions to computed for regular elements
  // (Grid and Flexbox will override this with their own layouts)
  if (!element->computed.is_computed) {
    element->computed.x = box.x;
    element->computed.y = box.y;
    element->computed.width = box.width;
    element->computed.height = box.height;
    element->computed.content_width = box.width;
    element->computed.content_height = box.height;
    element->computed.source = NVGCSSComputedLayout::CSS_EXPLICIT;
    element->computed.is_computed = true;
  }

  // Sprint 36: Border radius (shorthand with 1-4 values)
  // Order: top-left, top-right, bottom-right, bottom-left (clockwise from top-left)
  if (style.count("border-radius")) {
    std::string value = style.at("border-radius");
    auto parts = nvgcss_utils::split(value, ' ');

    // Border-radius percentages are relative to box width (CSS spec)
    if (parts.size() == 1) {
      // All corners same
      float r = resolve_length(parts[0], box.width, root_font_size_);
      box.border_radius[0] = box.border_radius[1] = box.border_radius[2] = box.border_radius[3] = r;
    } else if (parts.size() == 2) {
      // Diagonal: TL/BR, TR/BL
      float r1 = resolve_length(parts[0], box.width, root_font_size_);
      float r2 = resolve_length(parts[1], box.width, root_font_size_);
      box.border_radius[0] = r1; // top-left
      box.border_radius[1] = r2; // top-right
      box.border_radius[2] = r1; // bottom-right
      box.border_radius[3] = r2; // bottom-left
    } else if (parts.size() == 4) {
      // All four corners specified
      box.border_radius[0] = resolve_length(parts[0], box.width, root_font_size_); // top-left
      box.border_radius[1] = resolve_length(parts[1], box.width, root_font_size_); // top-right
      box.border_radius[2] = resolve_length(parts[2], box.width, root_font_size_); // bottom-right
      box.border_radius[3] = resolve_length(parts[3], box.width, root_font_size_); // bottom-left
    }
  } else {
    // Initialize to zero
    for (int i = 0; i < 4; i++) {
      box.border_radius[i] = 0.0f;
    }
  }

  // Individual corner properties override shorthand
  if (style.count("border-top-left-radius")) {
    box.border_radius[0] =
        resolve_length(style.at("border-top-left-radius"), box.width, root_font_size_);
  }
  if (style.count("border-top-right-radius")) {
    box.border_radius[1] =
        resolve_length(style.at("border-top-right-radius"), box.width, root_font_size_);
  }
  if (style.count("border-bottom-right-radius")) {
    box.border_radius[2] =
        resolve_length(style.at("border-bottom-right-radius"), box.width, root_font_size_);
  }
  if (style.count("border-bottom-left-radius")) {
    box.border_radius[3] =
        resolve_length(style.at("border-bottom-left-radius"), box.width, root_font_size_);
  }
  // Sprint 38: Full border shorthand (border: width style color)
  // Process before individual properties so they can override
  if (style.count("border")) {
    std::string value = style.at("border");

    // Parse border shorthand: width style color (any order, all optional)
    // Examples: "2px solid red", "solid", "3px", "dashed #ff0000"
    std::string width_str, style_str, color_str;

    // Split by whitespace
    std::vector<std::string> tokens;
    std::stringstream ss(value);
    std::string token;
    while (ss >> token) {
      tokens.push_back(token);
    }

    // Identify each component by type
    for (const auto &t : tokens) {
      // Check if it's a border style keyword
      if (t == "none" || t == "hidden" || t == "solid" || t == "dashed" || t == "dotted" ||
          t == "double") {
        style_str = t;
      }
      // Check if it's a color (starts with #, rgb, rgba, hsl, or is a named color)
      else if (t[0] == '#' || t.find("rgb") == 0 || t.find("hsl") == 0 || t == "red" ||
               t == "green" || t == "blue" || t == "black" || t == "white" || t == "gray" ||
               t == "yellow" || t == "cyan" || t == "magenta" || t == "orange" || t == "purple" ||
               t == "pink" || t == "brown" || t == "transparent") {
        color_str = t;
      }
      // Otherwise assume it's a width (with or without unit)
      else if (t.find("px") != std::string::npos ||
               std::isdigit(static_cast<unsigned char>(t[0]))) {
        width_str = t;
      }
    }

    // Apply to all four sides
    // Width
    if (!width_str.empty()) {
      auto border = parse_box_values(width_str);
      box.border_width[0] = border.top;
      box.border_width[1] = border.right;
      box.border_width[2] = border.bottom;
      box.border_width[3] = border.left;

      element->explicit_style.border_width[0] = border.top;
      element->explicit_style.border_width[1] = border.right;
      element->explicit_style.border_width[2] = border.bottom;
      element->explicit_style.border_width[3] = border.left;
    }

    // Style
    if (!style_str.empty()) {
      element->explicit_style.border_top_style = style_str;
      element->explicit_style.border_right_style = style_str;
      element->explicit_style.border_bottom_style = style_str;
      element->explicit_style.border_left_style = style_str;
    }

    // Color
    if (!color_str.empty()) {
      element->explicit_style.border_top_color = color_str;
      element->explicit_style.border_right_color = color_str;
      element->explicit_style.border_bottom_color = color_str;
      element->explicit_style.border_left_color = color_str;
    }
  }

  // Sprint 35: Parse border-width (shorthand with 1-4 values)
  // Process BEFORE per-side shorthands
  if (style.count("border-width")) {
    auto border = parse_box_values(style.at("border-width"));
    box.border_width[0] = border.top;
    box.border_width[1] = border.right;
    box.border_width[2] = border.bottom;
    box.border_width[3] = border.left;

    // Also store in explicit_style for test access
    element->explicit_style.border_width[0] = border.top;
    element->explicit_style.border_width[1] = border.right;
    element->explicit_style.border_width[2] = border.bottom;
    element->explicit_style.border_width[3] = border.left;
  } else {
    // Initialize to zero if not set by any shorthand
    bool has_any_border = false;
    for (int i = 0; i < 4; i++) {
      if (box.border_width[i] != 0.0f) {
        has_any_border = true;
        break;
      }
    }
    if (!has_any_border) {
      for (int i = 0; i < 4; i++) {
        box.border_width[i] = 0.0f;
        element->explicit_style.border_width[i] = 0.0f;
      }
    }
  }

  // Sprint 33: Border styles (shorthand with 1-4 values)
  if (style.count("border-style")) {
    std::string value = style.at("border-style");

    // Split by whitespace
    std::vector<std::string> styles;
    std::stringstream ss(value);
    std::string s;
    while (ss >> s) {
      styles.push_back(s);
    }

    // CSS box model order: top right bottom left
    if (styles.size() == 1) {
      // All sides
      element->explicit_style.border_top_style = styles[0];
      element->explicit_style.border_right_style = styles[0];
      element->explicit_style.border_bottom_style = styles[0];
      element->explicit_style.border_left_style = styles[0];
    } else if (styles.size() == 2) {
      // Vertical | Horizontal
      element->explicit_style.border_top_style = styles[0];
      element->explicit_style.border_bottom_style = styles[0];
      element->explicit_style.border_right_style = styles[1];
      element->explicit_style.border_left_style = styles[1];
    } else if (styles.size() == 3) {
      // Top | Horizontal | Bottom
      element->explicit_style.border_top_style = styles[0];
      element->explicit_style.border_right_style = styles[1];
      element->explicit_style.border_left_style = styles[1];
      element->explicit_style.border_bottom_style = styles[2];
    } else if (styles.size() >= 4) {
      // Top Right Bottom Left
      element->explicit_style.border_top_style = styles[0];
      element->explicit_style.border_right_style = styles[1];
      element->explicit_style.border_bottom_style = styles[2];
      element->explicit_style.border_left_style = styles[3];
    }
  }

  // Sprint 34: Border colors (shorthand with 1-4 values)
  if (style.count("border-color")) {
    std::string value = style.at("border-color");

    // Split by whitespace
    std::vector<std::string> colors;
    std::stringstream ss(value);
    std::string color;
    while (ss >> color) {
      colors.push_back(color);
    }

    // CSS box model order: top right bottom left
    if (colors.size() == 1) {
      // All sides
      element->explicit_style.border_top_color = colors[0];
      element->explicit_style.border_right_color = colors[0];
      element->explicit_style.border_bottom_color = colors[0];
      element->explicit_style.border_left_color = colors[0];
    } else if (colors.size() == 2) {
      // Vertical | Horizontal
      element->explicit_style.border_top_color = colors[0];
      element->explicit_style.border_bottom_color = colors[0];
      element->explicit_style.border_right_color = colors[1];
      element->explicit_style.border_left_color = colors[1];
    } else if (colors.size() == 3) {
      // Top | Horizontal | Bottom
      element->explicit_style.border_top_color = colors[0];
      element->explicit_style.border_right_color = colors[1];
      element->explicit_style.border_left_color = colors[1];
      element->explicit_style.border_bottom_color = colors[2];
    } else if (colors.size() >= 4) {
      // Top Right Bottom Left
      element->explicit_style.border_top_color = colors[0];
      element->explicit_style.border_right_color = colors[1];
      element->explicit_style.border_bottom_color = colors[2];
      element->explicit_style.border_left_color = colors[3];
    }
  }

  // Sprint 40: Per-side border shorthands (border-top, border-right, border-bottom, border-left)
  // Process AFTER main border shorthand AND property shorthands so they can override
  const char *sides[] = {"border-top", "border-right", "border-bottom", "border-left"};
  int side_indices[] = {0, 1, 2, 3}; // top, right, bottom, left

  for (int i = 0; i < 4; i++) {
    if (style.count(sides[i])) {
      std::string value = style.at(sides[i]);
      std::string width_str, style_str, color_str;

      // Split by whitespace
      std::vector<std::string> tokens;
      std::stringstream ss(value);
      std::string token;
      while (ss >> token) {
        tokens.push_back(token);
      }

      // Identify each component by type (same logic as main border shorthand)
      for (const auto &t : tokens) {
        if (t == "none" || t == "hidden" || t == "solid" || t == "dashed" || t == "dotted" ||
            t == "double") {
          style_str = t;
        } else if (t[0] == '#' || t.find("rgb") == 0 || t.find("hsl") == 0 || t == "red" ||
                   t == "green" || t == "blue" || t == "black" || t == "white" || t == "gray" ||
                   t == "yellow" || t == "cyan" || t == "magenta" || t == "orange" ||
                   t == "purple" || t == "pink" || t == "brown" || t == "transparent") {
          color_str = t;
        } else if (t.find("px") != std::string::npos ||
                   std::isdigit(static_cast<unsigned char>(t[0]))) {
          width_str = t;
        }
      }

      // Apply to the specific side
      int side_idx = side_indices[i];

      // Width
      if (!width_str.empty()) {
        float width = safe_stof(width_str.substr(0, width_str.find("px")));
        box.border_width[side_idx] = width;
        element->explicit_style.border_width[side_idx] = width;
      }

      // Style
      if (!style_str.empty()) {
        if (i == 0)
          element->explicit_style.border_top_style = style_str;
        else if (i == 1)
          element->explicit_style.border_right_style = style_str;
        else if (i == 2)
          element->explicit_style.border_bottom_style = style_str;
        else if (i == 3)
          element->explicit_style.border_left_style = style_str;
      }

      // Color
      if (!color_str.empty()) {
        if (i == 0)
          element->explicit_style.border_top_color = color_str;
        else if (i == 1)
          element->explicit_style.border_right_color = color_str;
        else if (i == 2)
          element->explicit_style.border_bottom_color = color_str;
        else if (i == 3)
          element->explicit_style.border_left_color = color_str;
      }
    }
  }

  // Individual sides override shorthand
  if (style.count("border-top-style")) {
    element->explicit_style.border_top_style = style.at("border-top-style");
  }
  if (style.count("border-right-style")) {
    element->explicit_style.border_right_style = style.at("border-right-style");
  }
  if (style.count("border-bottom-style")) {
    element->explicit_style.border_bottom_style = style.at("border-bottom-style");
  }
  if (style.count("border-left-style")) {
    element->explicit_style.border_left_style = style.at("border-left-style");
  }

  // Individual border color sides override shorthand
  if (style.count("border-top-color")) {
    element->explicit_style.border_top_color = style.at("border-top-color");
  }
  if (style.count("border-right-color")) {
    element->explicit_style.border_right_color = style.at("border-right-color");
  }
  if (style.count("border-bottom-color")) {
    element->explicit_style.border_bottom_color = style.at("border-bottom-color");
  }
  if (style.count("border-left-color")) {
    element->explicit_style.border_left_color = style.at("border-left-color");
  }

  // Visibility
  if (style.count("display") && style.at("display") == "none") {
    element->visible = false;
  } else {
    element->visible = true;
  }

  // Sprint 27: Parse box-shadow
  if (style.count("box-shadow")) {
    element->box_shadows = nvgcss_utils::parse_box_shadow(style.at("box-shadow"));
  } else {
    element->box_shadows.clear();
  }

  // Sprint 28: Parse text-shadow
  if (style.count("text-shadow")) {
    element->text_shadows = nvgcss_utils::parse_text_shadow(style.at("text-shadow"));
  } else {
    element->text_shadows.clear();
  }

  // PHASE 4 SPRINT 5: Parse grid container properties into explicit_style
  if (style.count("grid-template-rows")) {
    element->explicit_style.grid_template_rows = style.at("grid-template-rows");
  }
  if (style.count("grid-template-columns")) {
    element->explicit_style.grid_template_columns = style.at("grid-template-columns");
  }
  if (style.count("grid-auto-rows")) {
    element->explicit_style.grid_auto_rows = style.at("grid-auto-rows");
  }
  if (style.count("grid-auto-columns")) {
    element->explicit_style.grid_auto_columns = style.at("grid-auto-columns");
  }

  // Gap Shorthand (Sprint 21) - Parse BEFORE individual gap properties
  if (style.count("gap")) {
    std::string gap_value = style.at("gap");
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
  if (style.count("row-gap") || style.count("grid-row-gap")) {
    std::string gap_str = style.count("row-gap") ? style.at("row-gap") : style.at("grid-row-gap");
    if (gap_str.find("px") != std::string::npos) {
      element->explicit_style.grid_row_gap = safe_stof(gap_str.substr(0, gap_str.find("px")));
    }
  }
  if (style.count("column-gap") || style.count("grid-column-gap")) {
    std::string gap_str =
        style.count("column-gap") ? style.at("column-gap") : style.at("grid-column-gap");
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
      } catch (...) {
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
        } catch (...) {
        }
      }
      // If not a number, it's a line name - leave line as 0 (will be resolved in grid layout)
    }
  };

  // Parse shorthand grid-row and grid-column first (can be overridden by longhands)
  if (style.count("grid-row")) {
    std::string value = style.at("grid-row");
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
  if (style.count("grid-column")) {
    std::string value = style.at("grid-column");
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
  if (style.count("grid-row-start")) {
    parse_grid_value(style.at("grid-row-start"), element->explicit_style.grid_row_start,
                     element->explicit_style.grid_row_span);
  }
  if (style.count("grid-row-end")) {
    // For -end properties, if it contains "span", update the span field
    parse_grid_value(style.at("grid-row-end"), element->explicit_style.grid_row_end,
                     element->explicit_style.grid_row_span);
  }
  if (style.count("grid-column-start")) {
    parse_grid_value(style.at("grid-column-start"), element->explicit_style.grid_column_start,
                     element->explicit_style.grid_column_span);
  }
  if (style.count("grid-column-end")) {
    // For -end properties, if it contains "span", update the span field
    parse_grid_value(style.at("grid-column-end"), element->explicit_style.grid_column_end,
                     element->explicit_style.grid_column_span);
  }

  // Grid template areas (Sprint 13)
  if (style.count("grid-template-areas")) {
    element->explicit_style.grid_template_areas = style.at("grid-template-areas");
  }
  if (style.count("grid-area")) {
    element->explicit_style.grid_area = style.at("grid-area");
  }

  if (style.count("grid-auto-flow")) {
    element->explicit_style.grid_auto_flow = style.at("grid-auto-flow");
  }

  // Positioning (Sprint 9)
  if (style.count("position")) {
    element->explicit_style.position = style.at("position");
  }

  // Parse offset properties
  if (style.count("top")) {
    element->explicit_style.top =
        resolve_length(style.at("top"), viewport_height_, root_font_size_);
  }
  if (style.count("right")) {
    element->explicit_style.right =
        resolve_length(style.at("right"), viewport_width_, root_font_size_);
  }
  if (style.count("bottom")) {
    element->explicit_style.bottom =
        resolve_length(style.at("bottom"), viewport_height_, root_font_size_);
  }
  if (style.count("left")) {
    element->explicit_style.left =
        resolve_length(style.at("left"), viewport_width_, root_font_size_);
  }

  // Parse z-index
  if (style.count("z-index")) {
    std::string z_str = style.at("z-index");
    if (z_str != "auto") {
      try {
        element->explicit_style.z_index = std::stoi(z_str);
      } catch (...) {
        element->explicit_style.z_index = 0;
      }
    }
  }

  // Overflow (Sprint 20)
  if (style.count("overflow")) {
    element->explicit_style.overflow = style.at("overflow");
    // Shorthand sets both x and y
    element->explicit_style.overflow_x = style.at("overflow");
    element->explicit_style.overflow_y = style.at("overflow");
  } else {
    element->explicit_style.overflow = "visible";
    element->explicit_style.overflow_x = "visible";
    element->explicit_style.overflow_y = "visible";
  }

  // Longhand properties override shorthand
  if (style.count("overflow-x")) {
    element->explicit_style.overflow_x = style.at("overflow-x");
  }
  if (style.count("overflow-y")) {
    element->explicit_style.overflow_y = style.at("overflow-y");
  }

  // Opacity
  if (style.count("opacity")) {
    element->opacity = safe_stof(style.at("opacity"), 1.f);
  } else {
    element->opacity = 1.0f;
  }
}

void NVGCSSLayoutEngine::position_children(NVGCSSElement *element,
                                           const std::map<std::string, std::string> &style) {
  // TODO: Flow and stack layouts in phase 2
}

void NVGCSSLayoutEngine::compute_transforms(NVGCSSElement *element,
                                            const std::map<std::string, std::string> &style) {
  // Start with identity transform
  nvgTransformIdentity(element->transform);

  // Check for transform property
  auto it = style.find("transform");
  if (it == style.end())
    return;

  const std::string &transform_str = it->second;

  // Parse transform-origin (default: center center)
  float origin_x = element->box.x + element->box.width / 2.0f;
  float origin_y = element->box.y + element->box.height / 2.0f;

  auto origin_it = style.find("transform-origin");
  if (origin_it != style.end()) {
    std::istringstream origin_stream(origin_it->second);
    std::string x_str, y_str;
    origin_stream >> x_str >> y_str;

    // Parse X origin
    if (x_str == "left")
      origin_x = element->box.x;
    else if (x_str == "center")
      origin_x = element->box.x + element->box.width / 2.0f;
    else if (x_str == "right")
      origin_x = element->box.x + element->box.width;
    else if (x_str.find('%') != std::string::npos) {
      float percent = safe_stof(x_str) / 100.0f;
      origin_x = element->box.x + element->box.width * percent;
    } else if (x_str.find("px") != std::string::npos) {
      origin_x = element->box.x + safe_stof(x_str);
    }

    // Parse Y origin
    if (!y_str.empty()) {
      if (y_str == "top")
        origin_y = element->box.y;
      else if (y_str == "center")
        origin_y = element->box.y + element->box.height / 2.0f;
      else if (y_str == "bottom")
        origin_y = element->box.y + element->box.height;
      else if (y_str.find('%') != std::string::npos) {
        float percent = safe_stof(y_str) / 100.0f;
        origin_y = element->box.y + element->box.height * percent;
      } else if (y_str.find("px") != std::string::npos) {
        origin_y = element->box.y + safe_stof(y_str);
      }
    }
  }

  // Translate to origin
  float temp[6];
  nvgTransformTranslate(temp, -origin_x, -origin_y);
  nvgTransformMultiply(element->transform, temp);

  // Parse and apply transform functions
  size_t pos = 0;
  while (pos < transform_str.length()) {
    // Skip whitespace
    while (pos < transform_str.length() && std::isspace(transform_str[pos])) {
      pos++;
    }

    // Find function name
    size_t paren_pos = transform_str.find('(', pos);
    if (paren_pos == std::string::npos)
      break;

    std::string func_name = transform_str.substr(pos, paren_pos - pos);

    // Find matching closing parenthesis
    size_t close_paren = transform_str.find(')', paren_pos);
    if (close_paren == std::string::npos)
      break;

    std::string args = transform_str.substr(paren_pos + 1, close_paren - paren_pos - 1);

    // Parse arguments
    auto arg_list = nvgcss_utils::split(args, ',');

    // Apply transform based on function name
    float temp[6];
    if (func_name == "translate" && arg_list.size() >= 1) {
      float tx = resolve_length(arg_list[0], viewport_width_, root_font_size_);
      float ty = arg_list.size() >= 2
                     ? resolve_length(arg_list[1], viewport_height_, root_font_size_)
                     : 0.0f;

      nvgTransformTranslate(temp, tx, ty);
      nvgTransformMultiply(element->transform, temp);
    } else if (func_name == "translateX" && arg_list.size() >= 1) {
      float tx = resolve_length(arg_list[0], viewport_width_, root_font_size_);
      nvgTransformTranslate(temp, tx, 0);
      nvgTransformMultiply(element->transform, temp);
    } else if (func_name == "translateY" && arg_list.size() >= 1) {
      float ty = resolve_length(arg_list[0], viewport_height_, root_font_size_);
      nvgTransformTranslate(temp, 0, ty);
      nvgTransformMultiply(element->transform, temp);
    } else if (func_name == "rotate" && arg_list.size() >= 1) {
      std::string angle_str = arg_list[0];
      angle_str.erase(0, angle_str.find_first_not_of(" \t"));
      angle_str.erase(angle_str.find_last_not_of(" \t") + 1);

      float angle = 0.0f;
      if (angle_str.find("deg") != std::string::npos) {
        angle = safe_stof(angle_str) * 3.14159f / 180.0f; // Convert to radians
      } else if (angle_str.find("rad") != std::string::npos) {
        angle = safe_stof(angle_str);
      } else {
        angle = safe_stof(angle_str) * 3.14159f / 180.0f; // Assume degrees
      }

      nvgTransformRotate(temp, angle);
      nvgTransformMultiply(element->transform, temp);
    } else if (func_name == "scale" && arg_list.size() >= 1) {
      float sx = safe_stof(arg_list[0], 1.f);
      float sy = arg_list.size() >= 2 ? safe_stof(arg_list[1], sx) : sx;

      nvgTransformScale(temp, sx, sy);
      nvgTransformMultiply(element->transform, temp);
    } else if (func_name == "scaleX" && arg_list.size() >= 1) {
      float sx = safe_stof(arg_list[0], 1.f);
      nvgTransformScale(temp, sx, 1.0f);
      nvgTransformMultiply(element->transform, temp);
    } else if (func_name == "scaleY" && arg_list.size() >= 1) {
      float sy = safe_stof(arg_list[0], 1.f);
      nvgTransformScale(temp, 1.0f, sy);
      nvgTransformMultiply(element->transform, temp);
    } else if (func_name == "skewX" && arg_list.size() >= 1) {
      float angle = safe_stof(arg_list[0]) * 3.14159f / 180.0f;
      nvgTransformSkewX(temp, angle);
      nvgTransformMultiply(element->transform, temp);
    } else if (func_name == "skewY" && arg_list.size() >= 1) {
      float angle = safe_stof(arg_list[0]) * 3.14159f / 180.0f;
      nvgTransformSkewY(temp, angle);
      nvgTransformMultiply(element->transform, temp);
    }

    pos = close_paren + 1;
  }

  // Translate back from origin
  nvgTransformTranslate(temp, origin_x, origin_y);
  nvgTransformMultiply(element->transform, temp);
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
