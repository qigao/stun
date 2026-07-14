#include "scene_clone.h"

#include "flex/core/fsm.h"
#include "flex/core/group.h"
#include "flex/core/image.h"
#include "flex/core/shape.h"
#include "flex/core/svg.h"
#include "flex/core/text.h"
#include <cmath>

namespace flex {
namespace runtime {
namespace {

void copy_common_node_properties(const Node *source, Node *target) {
  if (!source || !target) {
    return;
  }

  target->set_id(source->id());
  target->set_position(source->x(), source->y());
  target->set_scale(source->scale_x(), source->scale_y());
  target->set_rotation(source->rotation());
  target->set_opacity(source->opacity());
  target->set_visible(source->visible());
  target->set_focusable(source->focusable());

  for (const auto &tag : source->tags()) {
    target->add_tag(tag);
  }

  if (source->has_shadow()) {
    target->set_shadow(source->shadow());
  }
  if (source->has_blur()) {
    target->set_blur(source->blur());
  }

  if (const auto *styles = source->pseudo_class_styles()) {
    for (const auto &[name, style] : *styles) {
      target->add_pseudo_class_style(name, style);
    }
  }

  if (source->has_layout()) {
    if (source->width_is_percent()) target->set_width_percent(source->layout_width());
    else target->set_layout_width(source->layout_width());
    if (source->height_is_percent()) target->set_height_percent(source->layout_height());
    else target->set_layout_height(source->layout_height());

    target->set_flex(source->flex_grow(), source->flex_shrink(), source->flex_basis());
    target->set_flex_basis_auto(source->flex_basis_auto());
    target->set_align_self(source->align_self());
    target->set_position_absolute(source->position_absolute());
    target->set_anchor(source->anchor());
    target->set_position_mode(source->position_mode());
    target->set_z_index(source->z_index());
    target->set_box_sizing(source->box_sizing());
    target->set_margin(source->margin_top(), source->margin_right(),
                       source->margin_bottom(), source->margin_left());
    target->set_border_width(source->border_width_top(), source->border_width_right(),
                             source->border_width_bottom(), source->border_width_left());

    if (!std::isnan(source->position_top())) target->set_position_top(source->position_top());
    if (!std::isnan(source->position_right())) target->set_position_right(source->position_right());
    if (!std::isnan(source->position_bottom())) target->set_position_bottom(source->position_bottom());
    if (!std::isnan(source->position_left())) target->set_position_left(source->position_left());
  }
}

Node::SharedPtr clone_runtime_node(
    const Node *source,
    std::unordered_map<Node::RawPtr, Node::SharedPtr> *shared_node_index) {
  if (!source) {
    return nullptr;
  }

  Node::SharedPtr clone;
  switch (source->type()) {
  case NodeType::Group: {
    auto *source_group = static_cast<const Group *>(source);
    auto cloned_group = Group::create();
    cloned_group->set_clip(source_group->clip());
    cloned_group->set_clip_size(source_group->clip_width(), source_group->clip_height());
    cloned_group->set_layout(source_group->layout());
    cloned_group->set_flex_direction(source_group->flex_direction());
    cloned_group->set_justify_content(source_group->justify_content());
    cloned_group->set_align_items(source_group->align_items());
    cloned_group->set_flex_wrap(source_group->flex_wrap());
    cloned_group->set_gap(source_group->gap());
    cloned_group->set_padding(source_group->padding_top(), source_group->padding_right(),
                              source_group->padding_bottom(), source_group->padding_left());

    copy_common_node_properties(source, cloned_group.get());
    clone = cloned_group;

    for (auto *child : source_group->children()) {
      auto cloned_child = clone_runtime_node(child, shared_node_index);
      if (cloned_child) {
        cloned_group->add_child(cloned_child);
      }
    }
    break;
  }
  case NodeType::Shape: {
    auto *source_shape = static_cast<const Shape *>(source);
    auto cloned_shape = Shape::create();
    switch (source_shape->geometry_type()) {
    case GeometryType::Rect: {
      auto rect = source_shape->rect();
      cloned_shape->set_rect(rect.width, rect.height, rect.corner_radius);
      break;
    }
    case GeometryType::Circle:
      cloned_shape->set_circle(source_shape->circle().radius);
      break;
    case GeometryType::Ellipse: {
      auto ellipse = source_shape->ellipse();
      cloned_shape->set_ellipse(ellipse.rx, ellipse.ry);
      break;
    }
    case GeometryType::Polygon: {
      auto polygon = source_shape->polygon();
      cloned_shape->set_polygon(polygon.sides, polygon.radius);
      break;
    }
    case GeometryType::Star: {
      auto star = source_shape->star();
      cloned_shape->set_star(star.points, star.outer_radius, star.inner_radius);
      break;
    }
    case GeometryType::Line: {
      auto line = source_shape->line();
      cloned_shape->set_line(line.x2, line.y2);
      break;
    }
    case GeometryType::Ring: {
      auto ring = source_shape->ring();
      cloned_shape->set_ring(ring.outer_radius, ring.inner_radius);
      break;
    }
    case GeometryType::Triangle: {
      auto triangle = source_shape->triangle();
      cloned_shape->set_triangle(triangle.width, triangle.height, triangle.direction);
      break;
    }
    case GeometryType::Path:
      cloned_shape->set_path(source_shape->path().d);
      break;
    default:
      break;
    }

    if (source_shape->has_fill()) {
      auto fill = source_shape->fill();
      switch (fill.type) {
      case FillType::Solid:
        cloned_shape->set_fill(fill.color);
        break;
      case FillType::LinearGradient:
        cloned_shape->set_fill(fill.linear_gradient);
        break;
      case FillType::RadialGradient:
        cloned_shape->set_fill(fill.radial_gradient);
        break;
      }
    }
    if (source_shape->has_stroke()) {
      auto stroke = source_shape->stroke();
      switch (stroke.type) {
      case StrokeType::Solid:
        cloned_shape->set_stroke(stroke.color, stroke.width);
        break;
      case StrokeType::LinearGradient:
        cloned_shape->set_stroke(stroke.linear_gradient, stroke.width);
        break;
      case StrokeType::RadialGradient:
        cloned_shape->set_stroke(stroke.radial_gradient, stroke.width);
        break;
      }
    }
    cloned_shape->set_rough(source_shape->rough());
    copy_common_node_properties(source, cloned_shape.get());
    clone = cloned_shape;
    break;
  }
  case NodeType::Text: {
    auto *source_text = static_cast<const Text *>(source);
    auto cloned_text = Text::create();
    cloned_text->set_content(source_text->content());
    cloned_text->set_font_family(source_text->font_family());
    cloned_text->set_font_size(source_text->font_size());
    cloned_text->set_font_weight(source_text->font_weight());
    cloned_text->set_font_style(source_text->font_style());
    cloned_text->set_text_decoration(source_text->text_decoration());
    cloned_text->set_text_align(source_text->text_align());
    cloned_text->set_line_height(source_text->line_height());
    cloned_text->set_max_width(source_text->max_width());
    cloned_text->set_letter_spacing(source_text->letter_spacing());
    cloned_text->set_text_overflow(source_text->text_overflow());
    cloned_text->set_color(source_text->color());
    copy_common_node_properties(source, cloned_text.get());
    clone = cloned_text;
    break;
  }
  case NodeType::Image: {
    auto *source_image = static_cast<const Image *>(source);
    auto cloned_image = Image::create();
    cloned_image->set_src(source_image->src());
    cloned_image->set_width(source_image->width());
    cloned_image->set_height(source_image->height());
    cloned_image->set_fit(source_image->fit_mode());
    copy_common_node_properties(source, cloned_image.get());
    clone = cloned_image;
    break;
  }
  case NodeType::Svg: {
    auto *source_svg = static_cast<const Svg *>(source);
    auto cloned_svg = Svg::create();
    if (!source_svg->src().empty()) cloned_svg->set_src(source_svg->src());
    else cloned_svg->set_data(source_svg->data());
    cloned_svg->set_width(source_svg->width());
    cloned_svg->set_height(source_svg->height());
    copy_common_node_properties(source, cloned_svg.get());
    clone = cloned_svg;
    break;
  }
  default:
    return nullptr;
  }

  if (shared_node_index) {
    (*shared_node_index)[clone.get()] = clone;
  }
  return clone;
}

} // namespace

Scene::RawPtr clone_scene(
    const Scene *source,
    ArenaAllocator &arena,
    std::unordered_map<Node::RawPtr, Node::SharedPtr> *shared_node_index) {
  if (!source) {
    return nullptr;
  }

  auto *scene = Scene::create(source->width(), source->height(), arena);
  scene->set_background(source->background());

  for (auto *child : source->root()->children()) {
    auto cloned_child = clone_runtime_node(child, shared_node_index);
    if (cloned_child) {
      scene->add_child(cloned_child);
    }
  }
  return scene;
}

} // namespace runtime
} // namespace flex
