/*
 * flexUI - LayoutManager Implementation
 */

#include <flexUI/layout_manager.h>
#include <flexUI/element.h>
#include <flexUI/computed_style.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace flexUI {

void LayoutManager::sync_to_flex(Element* elem, float container_w, float container_h) {
  if (!elem) return;

  auto* style = elem->computed_style;
  if (!style) return;

  // display:none
  if (style->display == Display::None) {
    elem->set_visible(false);
    return;
  }
  elem->set_visible(style->visibility == Visibility::Visible);
  elem->set_opacity(style->opacity);

  // Display mode
  if (style->display == Display::Flex) {
    elem->set_layout(flex::LayoutMode::Flex);
  } else if (style->display == Display::Block) {
    elem->set_layout(flex::LayoutMode::Flex);
    elem->set_flex_direction(flex::FlexDirection::Column);
  } else {
    elem->set_layout(flex::LayoutMode::None);
  }

  // Flex container properties
  if (style->display == Display::Flex) {
    elem->set_flex_direction(style->flex_direction);
    elem->set_justify_content(style->justify_content);
    elem->set_align_items(style->align_items);
  }
  elem->set_gap(style->gap);
  elem->set_padding(style->padding[0], style->padding[1],
                    style->padding[2], style->padding[3]);

  // Flex item properties
  float flex_grow = style->get_variable_float(Symbol("flex-grow"), 0.0f);
  float flex_shrink = style->get_variable_float(Symbol("flex-shrink"), 1.0f);
  float flex_basis = style->get_variable_float(Symbol("flex-basis"), 0.0f);
  elem->set_flex(flex_grow, flex_shrink, flex_basis);

  elem->set_position_mode(to_flex_position(style->position));
  elem->set_position_offsets(style->top, style->right, style->bottom, style->left);
  elem->set_z_index(style->z_index);
  elem->set_box_sizing(to_flex_box_sizing(style->box_sizing));
  elem->set_margin(style->margin[0], style->margin[1], style->margin[2], style->margin[3]);
  elem->set_border_width(style->border_width[0], style->border_width[1],
                         style->border_width[2], style->border_width[3]);

  // Width
  float width = 0;
  bool auto_width = false;
  if (style->width_is_percent && style->width > 0) {
    width = container_w * style->width / 100.0f;
    elem->set_layout_width(width);
  } else if (style->width > 0) {
    width = style->width;
    elem->set_layout_width(width);
  } else {
    auto_width = true;
    width = container_w;
    if (style->display == Display::Block) {
      elem->set_layout_width(width);
    } else {
      elem->set_layout_width(0);
    }
  }

  // Height
  float height = 0;
  bool auto_height = false;
  if (style->height_is_percent && style->height > 0) {
    height = container_h * style->height / 100.0f;
    elem->set_layout_height(height);
  } else if (style->height > 0) {
    height = style->height;
    elem->set_layout_height(height);
  } else {
    auto_height = true;
    height = elem->layout_height();
  }

  // Content area for children
  float content_w = std::max(0.0f, width - style->padding[1] - style->padding[3]);
  float content_h = std::max(0.0f, height - style->padding[0] - style->padding[2]);

  // Recurse children first (for auto sizing)
  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      sync_to_flex(child, content_w, content_h);
    }
  }

  // Auto height from children
  if (auto_height) {
    float children_height = 0;
    bool is_row = (style->display == Display::Flex) &&
                  (style->flex_direction == FlexDirection::Row ||
                   style->flex_direction == FlexDirection::RowReverse);
    int in_flow_count = 0;

    for (auto* node : elem->children()) {
      auto* child = static_cast<Element*>(node);
      if (!child || !child->is_visible()) continue;

      auto* cs = child->computed_style;
      if (cs && (cs->position == Position::Absolute || cs->position == Position::Fixed)) {
        continue;
      }

      float h = child->layout_height();
      if (cs) h += cs->margin[0] + cs->margin[2];

      if (is_row) {
        children_height = std::max(children_height, h);
      } else {
        children_height += h;
      }
      in_flow_count++;
    }

    if (!is_row && in_flow_count > 1) {
      children_height += style->gap * (in_flow_count - 1);
    }

    float computed_height = children_height + style->padding[0] + style->padding[2];

    // Minimum for text leaves (1.6x for proper line-height with descenders)
    if (elem->children().empty() && !elem->text().empty()) {
      float lh = style->font_size > 0 ? style->font_size * 1.6f : 24.0f;
      computed_height = std::max(computed_height, lh + style->padding[0] + style->padding[2]);
    } else if (computed_height <= 0 && elem->children().empty()) {
      computed_height = style->font_size > 0 ? style->font_size : 24.0f;
    }

    elem->set_layout_height(computed_height);
  }

  // Auto width from children
  if (auto_width && !elem->children().empty()) {
    float children_width = 0;
    bool is_row = (style->display == Display::Flex) &&
                  (style->flex_direction == FlexDirection::Row ||
                   style->flex_direction == FlexDirection::RowReverse);
    int in_flow_count = 0;

    for (auto* node : elem->children()) {
      auto* child = static_cast<Element*>(node);
      if (!child || !child->is_visible()) continue;

      auto* cs = child->computed_style;
      if (cs && (cs->position == Position::Absolute || cs->position == Position::Fixed)) {
        continue;
      }

      float w = child->layout_width();
      if (cs) w += cs->margin[1] + cs->margin[3];

      if (is_row) {
        children_width += w;
      } else {
        children_width = std::max(children_width, w);
      }
      in_flow_count++;
    }

    if (is_row && in_flow_count > 1) {
      children_width += style->gap * (in_flow_count - 1);
    }

    elem->set_layout_width(children_width + style->padding[1] + style->padding[3]);
  }

  // Auto width for text leaves
  if (auto_width && elem->children().empty() && !elem->text().empty()) {
    float fs = style->font_size > 0 ? style->font_size : 14.0f;
    float multiplier = 0.68f;
    if (style->font_weight >= FontWeight::Bold) multiplier = 0.85f;
    if (style->font_family.find("Consolas") != std::string::npos ||
        style->font_family.find("Courier") != std::string::npos) {
      multiplier = 0.65f;
    }

    float text_width = elem->text().length() * fs * multiplier + 10.0f;
    float computed_width = text_width + style->padding[1] + style->padding[3];
    elem->set_layout_width(std::max(computed_width, fs * 2));
  }
}

void LayoutManager::perform_layout(Element* elem) {
  if (!elem) return;

  elem->perform_layout();

  for (auto* node : elem->children()) {
    if (auto* child = static_cast<Element*>(node)) {
      perform_layout(child);
    }
  }
}

} // namespace flexUI
