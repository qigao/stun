/*
 * tvgbox2 - LayoutEngine
 *
 * Three-pass layout engine:
 *   Pass 1: compute_sizes - Calculate all element sizes
 *   Pass 2: compute_positions - Calculate positions (normal flow + positioned)
 *   Pass 3: build_render_list - Build z-ordered render list
 */

#ifndef TVGBOX2_LAYOUT_ENGINE_H
#define TVGBOX2_LAYOUT_ENGINE_H

#include "element.h"
#include "computed_style.h"
#include <vector>

namespace tvgbox2 {

/**
 * LayoutEngine - CSS-compliant layout computation
 *
 * Features:
 * - Box model with box-sizing support
 * - Flexbox (direction, justify-content, align-items, gap)
 * - Positioning (static, relative, absolute, fixed)
 * - Z-index stacking contexts
 * - Overflow clipping
 */
class LayoutEngine {
public:
  LayoutEngine() = default;

  /**
   * Main entry point: compute layout for entire tree
   */
  void layout(Element* root, float viewport_width, float viewport_height);

  /**
   * Get render list (z-ordered elements for painting)
   */
  const std::vector<Element*>& render_list() const { return render_list_; }

private:
  // Viewport dimensions (for fixed positioning and viewport units)
  float viewport_w_ = 0;
  float viewport_h_ = 0;

  // Z-ordered render list
  std::vector<Element*> render_list_;

  // ========== Pass 1: Size Computation ==========

  /**
   * Compute sizes for element and all descendants
   */
  void compute_sizes(Element* elem, float container_w, float container_h);

  /**
   * Resolve width considering box-sizing
   */
  float resolve_width(Element* elem, float available_width);

  /**
   * Resolve height considering box-sizing
   */
  float resolve_height(Element* elem, float available_height);

  /**
   * Get content box dimensions from border box
   */
  void get_content_box(const ComputedStyle* style, float& width, float& height);

  // ========== Pass 2: Position Computation ==========

  /**
   * Compute positions for element and all descendants
   */
  void compute_positions(Element* elem, float parent_x, float parent_y);

  /**
   * Layout children in normal flow (block or flex)
   */
  void layout_normal_flow(Element* elem);

  /**
   * Position absolutely/fixed positioned children
   */
  void layout_positioned(Element* elem);

  /**
   * Apply relative positioning offset
   */
  void apply_relative_offset(Element* elem);

  // ========== Flexbox ==========

  struct FlexItem {
    Element* elem;
    float base_size;
    float flex_grow;
    float flex_shrink;
    float min_size;
    float max_size;
    float final_size;
    float cross_size;
    float margin_main_start;
    float margin_main_end;
    float margin_cross_start;
    float margin_cross_end;
  };

  void layout_flex(Element* container);
  std::vector<FlexItem> collect_flex_items(Element* container, bool is_row);
  void resolve_flexible_lengths(std::vector<FlexItem>& items, float available_space);
  void position_flex_items(Element* container, std::vector<FlexItem>& items,
                          bool is_row, bool is_reverse,
                          float content_w, float content_h);

  // ========== Block Layout ==========

  void layout_block(Element* container);

  // ========== Pass 3: Stacking Context ==========

  /**
   * Build z-ordered render list
   */
  void build_render_list(Element* elem);

  /**
   * Check if element creates a stacking context
   */
  bool creates_stacking_context(Element* elem);

  // ========== Utilities ==========

  bool is_out_of_flow(Element* elem);
  bool is_positioned(Element* elem);
};

} // namespace tvgbox2

#endif // TVGBOX2_LAYOUT_ENGINE_H
