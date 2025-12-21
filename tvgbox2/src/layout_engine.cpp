/*
 * tvgbox2 - LayoutEngine Implementation
 *
 * Three-pass layout: sizes -> positions -> render list
 */

#include <tvgbox2/layout_engine.h>
#include <algorithm>
#include <cmath>

namespace tvgbox2 {

// ============================================================================
// Public API
// ============================================================================

void LayoutEngine::layout(Element* root, float viewport_width, float viewport_height) {
  if (!root) return;

  viewport_w_ = viewport_width;
  viewport_h_ = viewport_height;

  // Pass 1: Compute all sizes (top-down)
  compute_sizes(root, viewport_width, viewport_height);

  // Set root position
  root->x_ = 0;
  root->y_ = 0;

  // Pass 2: Compute all positions (top-down)
  compute_positions(root, 0, 0);

  // Pass 3: Build z-ordered render list
  render_list_.clear();
  build_render_list(root);
}

// ============================================================================
// Pass 1: Size Computation
// ============================================================================

void LayoutEngine::compute_sizes(Element* elem, float container_w, float container_h) {
  if (!elem) return;

  auto* style = elem->computed_style;
  if (!style) return;

  // Skip display:none
  if (style->display == Display::None) return;

  // Resolve width
  elem->width_ = resolve_width(elem, container_w);

  // Resolve height (may be auto, resolved after children)
  float specified_h = resolve_height(elem, container_h);
  elem->height_ = specified_h > 0 ? specified_h : 0;

  // Get content area for children
  float content_w = elem->width_;
  float content_h = elem->height_;
  get_content_box(style, content_w, content_h);

  // Compute children sizes
  for (auto* child : elem->children) {
    if (!child) continue;

    // Out-of-flow elements use containing block size
    if (is_out_of_flow(child)) {
      compute_sizes(child, elem->width_, elem->height_);
    } else {
      compute_sizes(child, content_w, content_h);
    }
  }

  // Auto height: compute from children
  if (specified_h <= 0 && !elem->children.empty()) {
    float children_height = 0;
    bool is_flex = (style->display == Display::Flex);
    bool is_row = (style->flex_direction == FlexDirection::Row ||
                   style->flex_direction == FlexDirection::RowReverse);

    int in_flow_count = 0;

    if (is_flex && is_row) {
      // Row flex: height = max child height
      for (auto* child : elem->children) {
        if (!child || is_out_of_flow(child)) continue;
        auto* cs = child->computed_style;
        float h = child->height_;
        if (cs) h += cs->margin[0] + cs->margin[2];
        children_height = std::max(children_height, h);
        in_flow_count++;
      }
    } else {
      // Block or column flex: height = sum of children
      for (auto* child : elem->children) {
        if (!child || is_out_of_flow(child)) continue;
        auto* cs = child->computed_style;
        float h = child->height_;
        if (cs) h += cs->margin[0] + cs->margin[2];
        children_height += h;
        in_flow_count++;
      }
      // Add gap for column flex (only between in-flow elements)
      if (is_flex && in_flow_count > 1) {
        children_height += style->gap * (in_flow_count - 1);
      }
    }

    elem->height_ = children_height + style->padding[0] + style->padding[2];
  }

  // Minimum height for leaf nodes
  if (elem->height_ <= 0 && elem->children.empty()) {
    elem->height_ = style->font_size > 0 ? style->font_size * 1.5f : 24.0f;
    elem->height_ += style->padding[0] + style->padding[2];
  }
}

float LayoutEngine::resolve_width(Element* elem, float available_width) {
  auto* style = elem->computed_style;
  if (!style) return available_width;

  float width = style->width;

  // Handle percentage width
  if (style->width_is_percent && width > 0) {
    width = available_width * (width / 100.0f);
  }
  // Auto width = full available width for block elements
  else if (width <= 0) {
    if (style->display == Display::Block || style->display == Display::Flex) {
      width = available_width;
    } else {
      // Inline: content-based (simplified)
      width = style->font_size * elem->text_content.length() * 0.6f;
      width += style->padding[1] + style->padding[3];
      if (width < 20) width = 20;
    }
  }

  // border-box: width already includes padding+border
  // content-box: add padding+border
  if (style->box_sizing == BoxSizing::ContentBox && !style->width_is_percent) {
    width += style->padding[1] + style->padding[3];
    width += style->border_width[1] + style->border_width[3];
  }

  return width;
}

float LayoutEngine::resolve_height(Element* elem, float available_height) {
  auto* style = elem->computed_style;
  if (!style) return 0;

  float height = style->height;

  // Handle percentage height
  if (style->height_is_percent && height > 0) {
    height = available_height * (height / 100.0f);
  }

  // Auto height = 0, will be computed from children
  if (height <= 0) return 0;

  // border-box: height already includes padding+border
  // content-box: add padding+border
  if (style->box_sizing == BoxSizing::ContentBox && !style->height_is_percent) {
    height += style->padding[0] + style->padding[2];
    height += style->border_width[0] + style->border_width[2];
  }

  return height;
}

void LayoutEngine::get_content_box(const ComputedStyle* style, float& width, float& height) {
  if (!style) return;

  width -= style->padding[1] + style->padding[3];
  height -= style->padding[0] + style->padding[2];

  if (width < 0) width = 0;
  if (height < 0) height = 0;
}

// ============================================================================
// Pass 2: Position Computation
// ============================================================================

void LayoutEngine::compute_positions(Element* elem, float parent_x, float parent_y) {
  if (!elem) return;

  auto* style = elem->computed_style;
  if (!style || style->display == Display::None) return;

  // Layout normal-flow children
  layout_normal_flow(elem);

  // Position absolute/fixed children
  layout_positioned(elem);

  // Apply relative offset to this element
  if (style->position == Position::Relative) {
    apply_relative_offset(elem);
  }

  // Recurse to children
  for (auto* child : elem->children) {
    compute_positions(child, elem->x_, elem->y_);
  }
}

void LayoutEngine::layout_normal_flow(Element* elem) {
  auto* style = elem->computed_style;
  if (!style) return;

  if (style->display == Display::Flex) {
    layout_flex(elem);
  } else {
    layout_block(elem);
  }
}

void LayoutEngine::layout_positioned(Element* elem) {
  auto* style = elem->computed_style;
  if (!style) return;

  for (auto* child : elem->children) {
    if (!child) continue;

    auto* cs = child->computed_style;
    if (!cs) continue;

    if (cs->position != Position::Absolute && cs->position != Position::Fixed) {
      continue;
    }

    // Containing block
    float cb_x, cb_y, cb_w, cb_h;

    if (cs->position == Position::Fixed) {
      // Fixed: relative to viewport
      cb_x = 0;
      cb_y = 0;
      cb_w = viewport_w_;
      cb_h = viewport_h_;
    } else {
      // Absolute: relative to positioned ancestor (this element)
      cb_x = elem->x_;
      cb_y = elem->y_;
      cb_w = elem->width_;
      cb_h = elem->height_;
    }

    // Resolve position from offsets
    float top = cs->top;
    float right = cs->right;
    float bottom = cs->bottom;
    float left = cs->left;

    // Horizontal position
    if (!std::isnan(left)) {
      child->x_ = cb_x + left;
    } else if (!std::isnan(right)) {
      child->x_ = cb_x + cb_w - right - child->width_;
    } else {
      child->x_ = cb_x;
    }

    // Vertical position
    if (!std::isnan(top)) {
      child->y_ = cb_y + top;
    } else if (!std::isnan(bottom)) {
      child->y_ = cb_y + cb_h - bottom - child->height_;
    } else {
      child->y_ = cb_y;
    }

    // Stretch if both offsets specified and no explicit size
    if (!std::isnan(left) && !std::isnan(right) && cs->width <= 0) {
      child->width_ = cb_w - left - right;
    }
    if (!std::isnan(top) && !std::isnan(bottom) && cs->height <= 0) {
      child->height_ = cb_h - top - bottom;
    }
  }
}

void LayoutEngine::apply_relative_offset(Element* elem) {
  auto* style = elem->computed_style;
  if (!style) return;

  if (!std::isnan(style->left)) {
    elem->x_ += style->left;
  } else if (!std::isnan(style->right)) {
    elem->x_ -= style->right;
  }

  if (!std::isnan(style->top)) {
    elem->y_ += style->top;
  } else if (!std::isnan(style->bottom)) {
    elem->y_ -= style->bottom;
  }
}

// ============================================================================
// Flexbox Layout
// ============================================================================

void LayoutEngine::layout_flex(Element* container) {
  auto* style = container->computed_style;
  if (!style) return;

  bool is_row = (style->flex_direction == FlexDirection::Row ||
                 style->flex_direction == FlexDirection::RowReverse);
  bool is_reverse = (style->flex_direction == FlexDirection::RowReverse ||
                     style->flex_direction == FlexDirection::ColumnReverse);

  // Collect flex items (skip out-of-flow)
  auto items = collect_flex_items(container, is_row);
  if (items.empty()) return;

  // Content area
  float content_w = container->width_ - style->padding[1] - style->padding[3];
  float content_h = container->height_ - style->padding[0] - style->padding[2];

  // Available main axis space (minus gaps)
  float gap = style->gap;
  float total_gap = items.size() > 1 ? gap * (items.size() - 1) : 0;
  float available_main = (is_row ? content_w : content_h) - total_gap;

  // Resolve flexible lengths
  resolve_flexible_lengths(items, available_main);

  // Position items
  position_flex_items(container, items, is_row, is_reverse, content_w, content_h);
}

std::vector<LayoutEngine::FlexItem> LayoutEngine::collect_flex_items(Element* container, bool is_row) {
  std::vector<FlexItem> items;

  for (auto* child : container->children) {
    if (!child || is_out_of_flow(child)) continue;

    FlexItem item;
    item.elem = child;

    auto* cs = child->computed_style;
    if (cs) {
      item.flex_grow = cs->get_variable_float("flex-grow", 0.0f);
      item.flex_shrink = cs->get_variable_float("flex-shrink", 1.0f);

      if (is_row) {
        // Use computed width as base size (already calculated in compute_sizes)
        item.base_size = child->width_;
        item.cross_size = child->height_;
        item.min_size = cs->get_variable_float("min-width", 0.0f);
        item.max_size = cs->get_variable_float("max-width", 999999.0f);
        item.margin_main_start = cs->margin[3];
        item.margin_main_end = cs->margin[1];
        item.margin_cross_start = cs->margin[0];
        item.margin_cross_end = cs->margin[2];
      } else {
        // Use computed height as base size
        item.base_size = child->height_;
        item.cross_size = child->width_;
        item.min_size = cs->get_variable_float("min-height", 0.0f);
        item.max_size = cs->get_variable_float("max-height", 999999.0f);
        item.margin_main_start = cs->margin[0];
        item.margin_main_end = cs->margin[2];
        item.margin_cross_start = cs->margin[3];
        item.margin_cross_end = cs->margin[1];
      }
    } else {
      item.flex_grow = 0;
      item.flex_shrink = 1;
      item.base_size = is_row ? child->width_ : child->height_;
      item.cross_size = is_row ? child->height_ : child->width_;
      item.min_size = 0;
      item.max_size = 999999.0f;
      item.margin_main_start = 0;
      item.margin_main_end = 0;
      item.margin_cross_start = 0;
      item.margin_cross_end = 0;
    }

    item.final_size = item.base_size;
    items.push_back(item);
  }

  return items;
}

void LayoutEngine::resolve_flexible_lengths(std::vector<FlexItem>& items, float available_space) {
  if (items.empty()) return;

  float total_base = 0;
  for (const auto& item : items) {
    total_base += item.base_size + item.margin_main_start + item.margin_main_end;
  }

  float free_space = available_space - total_base;

  if (free_space > 0) {
    // Grow
    float total_grow = 0;
    for (const auto& item : items) total_grow += item.flex_grow;

    if (total_grow > 0) {
      for (auto& item : items) {
        float growth = (item.flex_grow / total_grow) * free_space;
        item.final_size = item.base_size + growth;
        item.final_size = std::max(item.min_size, std::min(item.max_size, item.final_size));
      }
    }
  } else if (free_space < 0) {
    // Shrink
    float total_shrink = 0;
    for (const auto& item : items) {
      total_shrink += item.flex_shrink * item.base_size;
    }

    if (total_shrink > 0) {
      for (auto& item : items) {
        float shrink = (item.flex_shrink * item.base_size / total_shrink) * (-free_space);
        item.final_size = item.base_size - shrink;
        item.final_size = std::max(item.min_size, std::min(item.max_size, item.final_size));
      }
    }
  }
}

void LayoutEngine::position_flex_items(Element* container, std::vector<FlexItem>& items,
                                       bool is_row, bool is_reverse,
                                       float content_w, float content_h) {
  auto* style = container->computed_style;
  if (!style || items.empty()) return;

  float gap = style->gap;
  float available_main = is_row ? content_w : content_h;
  float available_cross = is_row ? content_h : content_w;

  // Total main size
  float total_main = 0;
  for (const auto& item : items) {
    total_main += item.final_size + item.margin_main_start + item.margin_main_end;
  }
  float total_gap = items.size() > 1 ? gap * (items.size() - 1) : 0;
  float free_space = available_main - total_main - total_gap;

  // Main axis start position
  float main_start = is_row ? style->padding[3] : style->padding[0];
  float item_gap = gap;

  switch (style->justify_content) {
    case JustifyContent::FlexStart:
      break;
    case JustifyContent::FlexEnd:
      main_start += free_space;
      break;
    case JustifyContent::Center:
      main_start += free_space / 2.0f;
      break;
    case JustifyContent::SpaceBetween:
      if (items.size() > 1) {
        item_gap = gap + free_space / (items.size() - 1);
      }
      break;
    case JustifyContent::SpaceAround:
      if (!items.empty()) {
        float space = free_space / items.size();
        main_start += space / 2.0f;
        item_gap = gap + space;
      }
      break;
    case JustifyContent::SpaceEvenly:
      if (!items.empty()) {
        float space = free_space / (items.size() + 1);
        main_start += space;
        item_gap = gap + space;
      }
      break;
  }

  // Handle reverse
  if (is_reverse) {
    main_start = (is_row ? style->padding[3] : style->padding[0]) + available_main;
  }

  float cross_start = is_row ? style->padding[0] : style->padding[3];
  float main_pos = main_start;

  for (size_t i = 0; i < items.size(); i++) {
    auto& item = items[is_reverse ? (items.size() - 1 - i) : i];

    // Cross axis position
    float cross_pos = cross_start + item.margin_cross_start;
    float item_cross = item.cross_size;

    switch (style->align_items) {
      case AlignItems::FlexStart:
        break;
      case AlignItems::FlexEnd:
        cross_pos = cross_start + available_cross - item_cross - item.margin_cross_end;
        break;
      case AlignItems::Center:
        cross_pos = cross_start + (available_cross - item_cross) / 2.0f;
        break;
      case AlignItems::Stretch:
        item_cross = available_cross - item.margin_cross_start - item.margin_cross_end;
        break;
      case AlignItems::Baseline:
        break;
    }

    // Position
    if (is_reverse) {
      main_pos -= item.margin_main_end + item.final_size;
    } else {
      main_pos += item.margin_main_start;
    }

    if (is_row) {
      // Store relative position (relative to container)
      item.elem->x_ = main_pos;
      item.elem->y_ = cross_pos;
      // Only update width if flex changed it
      if (item.final_size != item.base_size) {
        item.elem->width_ = item.final_size;
      }
      if (style->align_items == AlignItems::Stretch && item.cross_size <= 0) {
        item.elem->height_ = item_cross;
      }
    } else {
      // Store relative position (relative to container)
      item.elem->x_ = cross_pos;
      item.elem->y_ = main_pos;
      // Only update height if flex changed it
      if (item.final_size != item.base_size) {
        item.elem->height_ = item.final_size;
      }
      if (style->align_items == AlignItems::Stretch && item.cross_size <= 0) {
        item.elem->width_ = item_cross;
      }
    }

    // Next position
    if (is_reverse) {
      main_pos -= item.margin_main_start + item_gap;
    } else {
      main_pos += item.final_size + item.margin_main_end + item_gap;
    }
  }
}

// ============================================================================
// Block Layout
// ============================================================================

void LayoutEngine::layout_block(Element* container) {
  auto* style = container->computed_style;
  if (!style) return;

  float pad_top = style->padding[0];
  float pad_left = style->padding[3];

  float y_offset = pad_top;

  for (auto* child : container->children) {
    if (!child || is_out_of_flow(child)) continue;

    auto* cs = child->computed_style;
    float m_top = cs ? cs->margin[0] : 0;
    float m_left = cs ? cs->margin[3] : 0;
    float m_bottom = cs ? cs->margin[2] : 0;

    // Store relative position (relative to container)
    child->x_ = pad_left + m_left;
    child->y_ = y_offset + m_top;

    y_offset += m_top + child->height_ + m_bottom;
  }
}

// ============================================================================
// Pass 3: Stacking Context
// ============================================================================

void LayoutEngine::build_render_list(Element* elem) {
  if (!elem) return;

  auto* style = elem->computed_style;
  if (!style || style->display == Display::None) return;

  // Collect all elements in this stacking context
  struct StackEntry {
    Element* elem;
    int z_index;
  };

  std::vector<StackEntry> negative_z;   // z-index < 0
  std::vector<Element*> in_flow;        // z-index = 0, in-flow
  std::vector<StackEntry> positive_z;   // z-index > 0

  // Add this element first (background)
  render_list_.push_back(elem);

  // Categorize children
  for (auto* child : elem->children) {
    if (!child) continue;

    auto* cs = child->computed_style;
    if (!cs || cs->display == Display::None) continue;

    int z = cs->z_index;

    if (is_positioned(child) && z != 0) {
      if (z < 0) {
        negative_z.push_back({child, z});
      } else {
        positive_z.push_back({child, z});
      }
    } else {
      in_flow.push_back(child);
    }
  }

  // Sort by z-index
  std::sort(negative_z.begin(), negative_z.end(),
            [](const StackEntry& a, const StackEntry& b) { return a.z_index < b.z_index; });
  std::sort(positive_z.begin(), positive_z.end(),
            [](const StackEntry& a, const StackEntry& b) { return a.z_index < b.z_index; });

  // Paint order: negative z -> in-flow -> positive z
  for (const auto& entry : negative_z) {
    build_render_list(entry.elem);
  }
  for (auto* child : in_flow) {
    build_render_list(child);
  }
  for (const auto& entry : positive_z) {
    build_render_list(entry.elem);
  }
}

bool LayoutEngine::creates_stacking_context(Element* elem) {
  if (!elem) return false;

  auto* style = elem->computed_style;
  if (!style) return false;

  // Elements that create stacking context:
  // - position: fixed/sticky
  // - position: absolute/relative with z-index != auto
  // - opacity < 1
  // - transform != none

  if (style->position == Position::Fixed) return true;
  if (is_positioned(elem) && style->z_index != 0) return true;
  if (style->opacity < 1.0f) return true;
  if (style->transform_scale != 1.0f || style->transform_rotate != 0) return true;

  return false;
}

// ============================================================================
// Utilities
// ============================================================================

bool LayoutEngine::is_out_of_flow(Element* elem) {
  if (!elem || !elem->computed_style) return false;

  Position pos = elem->computed_style->position;
  return pos == Position::Absolute || pos == Position::Fixed;
}

bool LayoutEngine::is_positioned(Element* elem) {
  if (!elem || !elem->computed_style) return false;

  Position pos = elem->computed_style->position;
  return pos != Position::Static;
}

} // namespace tvgbox2
