#pragma once

#include <flexUI/element.h>

#include <algorithm>
#include <vector>

namespace flexUI::detail {

using flex::operator*;

struct CssRenderBounds {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  flex::Vec2 top_left{};
  flex::Vec2 top_right{};
  flex::Vec2 bottom_right{};
  flex::Vec2 bottom_left{};
};

inline flex::Transform local_css_render_transform(const Element* elem) {
  if (!elem) return flex::Transform{};
  auto* style = elem->computed_style;
  if (!style) return flex::make_translation(elem->x(), elem->y());

  float transform_scale_x = style->transform_scale_x;
  float transform_scale_y = style->transform_scale_y;
  if (transform_scale_x == 1.0f && transform_scale_y == 1.0f &&
      style->transform_scale != 1.0f) {
    transform_scale_x = style->transform_scale;
    transform_scale_y = style->transform_scale;
  }

  const float origin_x = style->transform_origin_x_percent
                             ? elem->width() * style->transform_origin_x
                             : style->transform_origin_x;
  const float origin_y = style->transform_origin_y_percent
                             ? elem->height() * style->transform_origin_y
                             : style->transform_origin_y;

  flex::Transform transform =
      flex::make_translation(elem->x() + style->transform_x,
                             elem->y() + style->transform_y);
  if (transform_scale_x != 1.0f || transform_scale_y != 1.0f) {
    transform = transform * flex::make_translation(origin_x, origin_y) *
                flex::make_scale(transform_scale_x, transform_scale_y) *
                flex::make_translation(-origin_x, -origin_y);
  }
  if (style->transform_rotate != 0.0f) {
    transform = transform * flex::make_translation(origin_x, origin_y) *
                flex::create_transform(0.0f, 0.0f, style->transform_rotate,
                                       1.0f, 1.0f) *
                flex::make_translation(-origin_x, -origin_y);
  }
  if (style->has_transform_matrix) {
    transform = transform * flex::make_translation(origin_x, origin_y) *
                style->transform_matrix *
                flex::make_translation(-origin_x, -origin_y);
  }
  return transform;
}

inline flex::Transform css_render_world_transform(const Element* elem) {
  if (!elem) return flex::Transform{};

  std::vector<const Element*> chain;
  for (const Element* current = elem; current; current = current->parent_elem()) {
    chain.push_back(current);
  }

  flex::Transform transform;
  for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
    const Element* current = *it;
    transform = transform * local_css_render_transform(current);
    if (current != elem &&
        (current->scroll_x() != 0.0f || current->scroll_y() != 0.0f)) {
      transform = transform *
                  flex::make_translation(-current->scroll_x(),
                                         -current->scroll_y());
    }
  }
  return transform;
}

inline flex::Vec2 css_render_to_local(const Element* elem,
                                      const flex::Vec2& world_pos) {
  return flex::inverse(css_render_world_transform(elem)) * world_pos;
}

inline flex::Transform css_render_content_world_transform(const Element* elem) {
  flex::Transform transform = css_render_world_transform(elem);
  if (elem && (elem->scroll_x() != 0.0f || elem->scroll_y() != 0.0f)) {
    transform = transform *
                flex::make_translation(-elem->scroll_x(), -elem->scroll_y());
  }
  return transform;
}

inline flex::Vec2 css_render_to_parent_content(const Element* elem,
                                               const flex::Vec2& world_pos) {
  const Element* parent = elem ? elem->parent_elem() : nullptr;
  if (!parent) {
    return world_pos;
  }
  return flex::inverse(css_render_content_world_transform(parent)) * world_pos;
}

inline CssRenderBounds css_render_rect_bounds(const Element* elem, float x,
                                              float y, float width,
                                              float height) {
  const flex::Transform transform = css_render_world_transform(elem);
  CssRenderBounds bounds;
  bounds.top_left = transform * flex::Vec2(x, y);
  bounds.top_right = transform * flex::Vec2(x + width, y);
  bounds.bottom_right = transform * flex::Vec2(x + width, y + height);
  bounds.bottom_left = transform * flex::Vec2(x, y + height);

  const float min_x = std::min(
      {bounds.top_left.x, bounds.top_right.x, bounds.bottom_right.x,
       bounds.bottom_left.x});
  const float max_x = std::max(
      {bounds.top_left.x, bounds.top_right.x, bounds.bottom_right.x,
       bounds.bottom_left.x});
  const float min_y = std::min(
      {bounds.top_left.y, bounds.top_right.y, bounds.bottom_right.y,
       bounds.bottom_left.y});
  const float max_y = std::max(
      {bounds.top_left.y, bounds.top_right.y, bounds.bottom_right.y,
       bounds.bottom_left.y});

  bounds.x = min_x;
  bounds.y = min_y;
  bounds.width = max_x - min_x;
  bounds.height = max_y - min_y;
  return bounds;
}

inline CssRenderBounds css_render_world_bounds(const Element* elem) {
  return css_render_rect_bounds(elem, 0.0f, 0.0f, elem->width(), elem->height());
}

}  // namespace flexUI::detail
