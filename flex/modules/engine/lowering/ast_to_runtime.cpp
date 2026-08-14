/*
 * Flex Engine - AST to Runtime Lowering (Implementation)
 *
 * Lowers parsed DSL AST objects into executable runtime objects.
 */

#include "flex/lowering/ast_to_runtime.h"
#include "semantic_validator.h"
#include "flex.h"
#include "flex/dsl/flex_ast.h"
#include "flex/dsl/flex_parser.h"
#include "flex/core/component.h"
#include "flex/core/debug.h"
#include "flex/core/expr_compiled.h"
#include "flex/core/fsm.h"
#include "flex/core/group.h"
#include "flex/core/image.h"
#include "flex/core/shape.h"
#include "flex/core/svg.h"
#include "flex/core/text.h"
#include "flex/core/timeline.h"
#include <algorithm>
#include <cctype>
#include <stdexcept>


namespace flex {

// ============================================================================
// Helper: Parse color from hex string - uses Color::from_hex from types.h
// ============================================================================

static inline Color parse_color_from_string(const std::string &color_str) {
  return Color::from_hex(color_str.c_str());
}

namespace {

bool is_color_literal(const std::string &value) {
  return !value.empty() && value[0] == '#';
}

uint32_t parse_color_rgba32(const std::string &color_str) {
  if (!is_color_literal(color_str)) {
    return 0x000000FF;
  }

  const char *hex = color_str.c_str() + 1;
  size_t len = color_str.length() - 1;

  uint32_t r = 0, g = 0, b = 0, a = 255;

  if (len == 6) {
    sscanf(hex, "%02x%02x%02x", &r, &g, &b);
  } else if (len == 8) {
    sscanf(hex, "%02x%02x%02x%02x", &r, &g, &b, &a);
  } else if (len == 3) {
    unsigned int r4, g4, b4;
    sscanf(hex, "%1x%1x%1x", &r4, &g4, &b4);
    r = r4 * 17;
    g = g4 * 17;
    b = b4 * 17;
  } else {
    return 0x000000FF;
  }

  return (r << 24) | (g << 16) | (b << 8) | a;
}

bool is_simple_identifier(const std::string &value) {
  if (value.empty()) {
    return false;
  }
  const unsigned char first = static_cast<unsigned char>(value[0]);
  if (!(std::isalpha(first) || value[0] == '_')) {
    return false;
  }
  for (size_t i = 1; i < value.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(value[i]);
    if (!(std::isalnum(ch) || value[i] == '_')) {
      return false;
    }
  }
  return true;
}

std::string trim_copy(const std::string &value) {
  const size_t start = value.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    return "";
  }
  const size_t end = value.find_last_not_of(" \t\r\n");
  return value.substr(start, end - start + 1);
}

bool resolve_component_prop_reference(const std::string &value, const Props &props,
                                      PropValue &resolved) {
  std::string name;

  if (value.size() >= 3 && value[0] == '$' && value[1] == '{' && value.back() == '}') {
    std::string expr = trim_copy(value.substr(2, value.size() - 3));
    if (!expr.empty() && expr[0] == '$') {
      expr.erase(expr.begin());
    }
    if (is_simple_identifier(expr)) {
      name = expr;
    }
  } else if (value.size() >= 3 && value[0] == '$' && value[1] == '(' && value.back() == ')') {
    std::string expr = trim_copy(value.substr(2, value.size() - 3));
    if (is_simple_identifier(expr)) {
      name = expr;
    }
  } else if (value.size() >= 2 && value[0] == '$' && value[1] != '{' && value[1] != '(') {
    std::string expr = value.substr(1);
    if (is_simple_identifier(expr)) {
      name = expr;
    }
  }

  if (name.empty()) {
    return false;
  }

  const auto it = props.find(name);
  if (it == props.end()) {
    return false;
  }

  resolved = it->second;
  return true;
}

PropValue to_component_prop_value(const parser::AstValue &value) {
  if (auto *fval = std::get_if<float>(&value)) {
    return *fval;
  }
  if (auto *bval = std::get_if<bool>(&value)) {
    return *bval;
  }
  if (auto *sval = std::get_if<std::string>(&value)) {
    if (is_color_literal(*sval)) {
      return parse_color_rgba32(*sval);
    }
    return *sval;
  }
  return std::string();
}

PropValue to_runtime_prop_value(const parser::AstValue &value) {
  if (auto *fval = std::get_if<float>(&value)) {
    return *fval;
  }
  if (auto *bval = std::get_if<bool>(&value)) {
    return *bval;
  }
  if (auto *sval = std::get_if<std::string>(&value)) {
    return *sval;
  }
  return std::string();
}

PropValue resolve_template_property_value(const parser::AstValue &value, const Props &props) {
  if (auto *fval = std::get_if<float>(&value)) {
    return *fval;
  }
  if (auto *bval = std::get_if<bool>(&value)) {
    return *bval;
  }
  if (auto *sval = std::get_if<std::string>(&value)) {
    PropValue resolved;
    if (resolve_component_prop_reference(*sval, props, resolved)) {
      return resolved;
    }
    return *sval;
  }
  return std::string();
}

PropValue coerce_component_runtime_prop(const PropValue &value) {
  if (auto *sval = std::get_if<std::string>(&value)) {
    if (is_color_literal(*sval)) {
      return parse_color_rgba32(*sval);
    }
  }
  return value;
}

bool try_get_float(const PropValue &value, float &out) {
  if (auto *fval = std::get_if<float>(&value)) {
    out = *fval;
    return true;
  }
  return false;
}

bool try_get_bool(const PropValue &value, bool &out) {
  if (auto *bval = std::get_if<bool>(&value)) {
    out = *bval;
    return true;
  }
  return false;
}

bool try_get_string(const PropValue &value, const std::string *&out) {
  if (auto *sval = std::get_if<std::string>(&value)) {
    out = sval;
    return true;
  }
  return false;
}

bool try_get_color(const PropValue &value, uint32_t &out) {
  if (auto *cval = std::get_if<uint32_t>(&value)) {
    out = *cval;
    return true;
  }
  if (auto *sval = std::get_if<std::string>(&value)) {
    if (is_color_literal(*sval)) {
      out = parse_color_rgba32(*sval);
      return true;
    }
  }
  return false;
}

Component::SharedPtr find_component(
    const std::map<std::string, Component::SharedPtr> &local_components,
    const std::string &name) {
  const auto it = local_components.find(name);
  if (it != local_components.end()) {
    return it->second;
  }
  return ComponentRegistry::instance().get(name);
}

bool parse_compiled_binding(const std::string &source, Binding &binding) {
  binding = parse_binding(source);
  return binding.type != BindingType::ExprTk ||
         compile_mir_expression(binding.expression, binding.compiled_expr);
}

struct SharedNodeOwner {
  ComponentNodePtr root;
  std::vector<ComponentNodePtr> retained;
};

ComponentNodePtr wrap_shared_tree(ComponentNodePtr root,
                                  std::vector<ComponentNodePtr> retained) {
  if (!root) {
    return nullptr;
  }
  if (retained.empty()) {
    return root;
  }
  auto owner = std::make_shared<SharedNodeOwner>();
  owner->root = std::move(root);
  owner->retained = std::move(retained);
  return ComponentNodePtr(owner, owner->root.get());
}

void apply_runtime_property(Node *node, const std::string &node_type, const std::string &key,
                            const PropValue &value) {
  if (!node) {
    return;
  }

  float fval = 0.0f;
  bool bval = false;
  const std::string *sval = nullptr;
  uint32_t color = 0;

  if (key == "x" && try_get_float(value, fval)) {
    node->set_x(fval);
  } else if (key == "y" && try_get_float(value, fval)) {
    node->set_y(fval);
  } else if (key == "width" && try_get_float(value, fval)) {
    node->set_layout_width(fval);
  } else if (key == "height" && try_get_float(value, fval)) {
    node->set_layout_height(fval);
  } else if (key == "opacity" && try_get_float(value, fval)) {
    node->set_opacity(fval);
  } else if (key == "rotation" && try_get_float(value, fval)) {
    node->set_rotation(fval);
  } else if (key == "scale" && try_get_float(value, fval)) {
    node->set_scale(fval);
  } else if (key == "scaleX" && try_get_float(value, fval)) {
    node->set_scale(fval, node->scale_y());
  } else if (key == "scaleY" && try_get_float(value, fval)) {
    node->set_scale(node->scale_x(), fval);
  } else if (key == "visible" && try_get_bool(value, bval)) {
    node->set_visible(bval);
  } else if (key == "id" && try_get_string(value, sval)) {
    node->set_id(*sval);
  } else if (key == "alignSelf" && try_get_string(value, sval)) {
    if (*sval == "auto") {
      node->set_align_self(AlignSelf::Auto);
    } else if (*sval == "start") {
      node->set_align_self(AlignSelf::Start);
    } else if (*sval == "end") {
      node->set_align_self(AlignSelf::End);
    } else if (*sval == "center") {
      node->set_align_self(AlignSelf::Center);
    } else if (*sval == "stretch") {
      node->set_align_self(AlignSelf::Stretch);
    }
  } else if (key == "position" && try_get_string(value, sval)) {
    if (*sval == "static") {
      node->set_position_mode(PositionMode::Static);
    } else if (*sval == "relative") {
      node->set_position_mode(PositionMode::Relative);
    } else if (*sval == "absolute") {
      node->set_position_mode(PositionMode::Absolute);
    } else if (*sval == "fixed") {
      node->set_position_mode(PositionMode::Fixed);
    }
  } else if (key == "flexGrow" && try_get_float(value, fval)) {
    node->set_flex_grow(fval);
  } else if (key == "flexShrink" && try_get_float(value, fval)) {
    node->set_flex_shrink(fval);
  } else if (key == "flexBasis" && try_get_float(value, fval)) {
    node->set_flex_basis(fval);
  } else if (key == "anchor" && try_get_string(value, sval)) {
    if (*sval == "topLeft") {
      node->set_anchor(Anchor::TopLeft);
    } else if (*sval == "top") {
      node->set_anchor(Anchor::Top);
    } else if (*sval == "topRight") {
      node->set_anchor(Anchor::TopRight);
    } else if (*sval == "left") {
      node->set_anchor(Anchor::Left);
    } else if (*sval == "center") {
      node->set_anchor(Anchor::Center);
    } else if (*sval == "right") {
      node->set_anchor(Anchor::Right);
    } else if (*sval == "bottomLeft") {
      node->set_anchor(Anchor::BottomLeft);
    } else if (*sval == "bottom") {
      node->set_anchor(Anchor::Bottom);
    } else if (*sval == "bottomRight") {
      node->set_anchor(Anchor::BottomRight);
    }
  }

  if (auto *shape = dynamic_cast<Shape *>(node)) {
    if (key == "width" && try_get_float(value, fval)) {
      if (node_type == "rect") {
        const auto rect = shape->rect();
        shape->set_rect(fval, rect.height, rect.corner_radius);
      }
    } else if (key == "height" && try_get_float(value, fval)) {
      if (node_type == "rect") {
        const auto rect = shape->rect();
        shape->set_rect(rect.width, fval, rect.corner_radius);
      }
    } else if ((key == "radius" || key == "cornerRadius") && try_get_float(value, fval)) {
      if (node_type == "circle") {
        shape->set_circle(fval);
      } else if (node_type == "rect") {
        const auto rect = shape->rect();
        shape->set_rect(rect.width, rect.height, fval);
      } else if (node_type == "polygon") {
        const auto polygon = shape->polygon();
        shape->set_polygon(polygon.sides, fval);
      } else if (node_type == "star") {
        const auto star = shape->star();
        shape->set_star(star.points, fval, star.inner_radius);
      }
    } else if (key == "sides" && try_get_float(value, fval)) {
      if (node_type == "polygon") {
        const auto polygon = shape->polygon();
        shape->set_polygon(static_cast<int>(fval), polygon.radius);
      }
    } else if (key == "points" && try_get_float(value, fval)) {
      if (node_type == "star") {
        const auto star = shape->star();
        shape->set_star(static_cast<int>(fval), star.outer_radius, star.inner_radius);
      }
    } else if (key == "rx" && try_get_float(value, fval)) {
      if (node_type == "ellipse") {
        const auto ellipse = shape->ellipse();
        shape->set_ellipse(fval, ellipse.ry);
      }
    } else if (key == "ry" && try_get_float(value, fval)) {
      if (node_type == "ellipse") {
        const auto ellipse = shape->ellipse();
        shape->set_ellipse(ellipse.rx, fval);
      }
    } else if (key == "fill" && try_get_color(value, color)) {
      shape->set_fill(color);
    } else if (key == "stroke" && try_get_color(value, color)) {
      shape->set_stroke(color, shape->stroke().width);
    } else if (key == "strokeWidth" && try_get_float(value, fval)) {
      const auto stroke = shape->stroke();
      shape->set_stroke(stroke.color, fval);
    } else if (key == "d" && try_get_string(value, sval)) {
      if (node_type == "path") {
        shape->set_path(*sval);
      }
    } else if (key == "x2" && try_get_float(value, fval)) {
      if (node_type == "line") {
        const auto line = shape->line();
        shape->set_line(fval, line.y2);
      }
    } else if (key == "y2" && try_get_float(value, fval)) {
      if (node_type == "line") {
        const auto line = shape->line();
        shape->set_line(line.x2, fval);
      }
    } else if (key == "outerRadius" && try_get_float(value, fval)) {
      if (node_type == "ring") {
        const auto ring = shape->ring();
        shape->set_ring(fval, ring.inner_radius);
      } else if (node_type == "star") {
        const auto star = shape->star();
        shape->set_star(star.points, fval, star.inner_radius);
      }
    } else if (key == "innerRadius" && try_get_float(value, fval)) {
      if (node_type == "ring") {
        const auto ring = shape->ring();
        shape->set_ring(ring.outer_radius, fval);
      } else if (node_type == "star") {
        const auto star = shape->star();
        shape->set_star(star.points, star.outer_radius, fval);
      }
    } else if (key == "direction" && try_get_string(value, sval)) {
      if (node_type == "triangle") {
        const auto triangle = shape->triangle();
        Direction direction = Direction::Right;
        if (*sval == "left") {
          direction = Direction::Left;
        } else if (*sval == "up") {
          direction = Direction::Up;
        } else if (*sval == "down") {
          direction = Direction::Down;
        }
        shape->set_triangle(triangle.width, triangle.height, direction);
      }
    }

    if (node_type == "triangle") {
      const auto triangle = shape->triangle();
      float width = triangle.width;
      float height = triangle.height;
      bool updated = false;
      if (key == "width" && try_get_float(value, fval)) {
        width = fval;
        updated = true;
      } else if (key == "height" && try_get_float(value, fval)) {
        height = fval;
        updated = true;
      }
      if (updated) {
        shape->set_triangle(width, height, triangle.direction);
      }
    }

    if (key == "roughness" && try_get_float(value, fval)) {
      auto opts = shape->rough();
      opts.roughness = fval;
      shape->set_rough(opts);
    } else if (key == "bowing" && try_get_float(value, fval)) {
      auto opts = shape->rough();
      opts.bowing = fval;
      shape->set_rough(opts);
    } else if (key == "roughSeed" && try_get_float(value, fval)) {
      auto opts = shape->rough();
      opts.seed = static_cast<unsigned int>(fval);
      shape->set_rough(opts);
    } else if (key == "fillStyle" && try_get_string(value, sval)) {
      auto opts = shape->rough();
      if (*sval == "solid") {
        opts.fill_style = RoughFillStyle::Solid;
      } else if (*sval == "hachure") {
        opts.fill_style = RoughFillStyle::Hachure;
      } else if (*sval == "zigzag") {
        opts.fill_style = RoughFillStyle::ZigZag;
      } else if (*sval == "crosshatch") {
        opts.fill_style = RoughFillStyle::CrossHatch;
      }
      shape->set_rough(opts);
    } else if (key == "hachureGap" && try_get_float(value, fval)) {
      auto opts = shape->rough();
      opts.hachure_gap = fval;
      shape->set_rough(opts);
    } else if (key == "hachureAngle" && try_get_float(value, fval)) {
      auto opts = shape->rough();
      opts.hachure_angle = fval;
      shape->set_rough(opts);
    }
  }

  if (auto *svg = dynamic_cast<Svg *>(node)) {
    if (key == "src" && try_get_string(value, sval)) {
      svg->set_src(*sval);
    } else if (key == "data" && try_get_string(value, sval)) {
      svg->set_data(*sval);
    }
  }

  if (auto *img = dynamic_cast<Image *>(node)) {
    if (key == "src" && try_get_string(value, sval)) {
      img->set_src(*sval);
    }
  }

  if (auto *text = dynamic_cast<Text *>(node)) {
    if (key == "content" && try_get_string(value, sval)) {
      text->set_content(*sval);
    } else if (key == "fontSize" && try_get_float(value, fval)) {
      text->set_font_size(fval);
    } else if (key == "fontFamily" && try_get_string(value, sval)) {
      text->set_font_family(*sval);
    } else if (key == "color" && try_get_color(value, color)) {
      text->set_color(color);
    } else if (key == "textAlign" && try_get_string(value, sval)) {
      if (*sval == "left") {
        text->set_text_align(TextAlign::Left);
      } else if (*sval == "center") {
        text->set_text_align(TextAlign::Center);
      } else if (*sval == "right") {
        text->set_text_align(TextAlign::Right);
      }
    } else if (key == "fontWeight" && try_get_string(value, sval)) {
      text->set_font_weight(*sval == "bold" ? FontWeight::Bold : FontWeight::Normal);
    } else if (key == "fontStyle" && try_get_string(value, sval)) {
      text->set_font_style(*sval == "italic" ? FontStyle::Italic : FontStyle::Normal);
    } else if (key == "textDecoration" && try_get_string(value, sval)) {
      TextDecoration decoration = TextDecoration::None;
      if (*sval == "underline") {
        decoration = TextDecoration::Underline;
      } else if (*sval == "strikethrough") {
        decoration = TextDecoration::Strikethrough;
      } else if (*sval == "overline") {
        decoration = TextDecoration::Overline;
      }
      text->set_text_decoration(decoration);
    } else if (key == "lineHeight" && try_get_float(value, fval)) {
      text->set_line_height(fval);
    } else if (key == "maxWidth" && try_get_float(value, fval)) {
      text->set_max_width(fval);
    }
  }

  if (auto *group = dynamic_cast<Group *>(node)) {
    if (key == "layout" && try_get_string(value, sval)) {
      if (*sval == "flex") {
        group->set_layout(LayoutMode::Flex);
      } else if (*sval == "none") {
        group->set_layout(LayoutMode::None);
      }
    } else if (key == "flexDirection" && try_get_string(value, sval)) {
      if (*sval == "row") {
        group->set_flex_direction(FlexDirection::Row);
      } else if (*sval == "column") {
        group->set_flex_direction(FlexDirection::Column);
      } else if (*sval == "rowReverse") {
        group->set_flex_direction(FlexDirection::RowReverse);
      } else if (*sval == "columnReverse") {
        group->set_flex_direction(FlexDirection::ColumnReverse);
      }
    } else if (key == "justifyContent" && try_get_string(value, sval)) {
      if (*sval == "start") {
        group->set_justify_content(JustifyContent::Start);
      } else if (*sval == "end") {
        group->set_justify_content(JustifyContent::End);
      } else if (*sval == "center") {
        group->set_justify_content(JustifyContent::Center);
      } else if (*sval == "spaceBetween") {
        group->set_justify_content(JustifyContent::SpaceBetween);
      } else if (*sval == "spaceAround") {
        group->set_justify_content(JustifyContent::SpaceAround);
      } else if (*sval == "spaceEvenly") {
        group->set_justify_content(JustifyContent::SpaceEvenly);
      }
    } else if (key == "alignItems" && try_get_string(value, sval)) {
      if (*sval == "start") {
        group->set_align_items(AlignItems::Start);
      } else if (*sval == "end") {
        group->set_align_items(AlignItems::End);
      } else if (*sval == "center") {
        group->set_align_items(AlignItems::Center);
      } else if (*sval == "stretch") {
        group->set_align_items(AlignItems::Stretch);
      }
    } else if (key == "flexWrap" && try_get_string(value, sval)) {
      if (*sval == "nowrap" || *sval == "noWrap") {
        group->set_flex_wrap(FlexWrap::NoWrap);
      } else if (*sval == "wrap") {
        group->set_flex_wrap(FlexWrap::Wrap);
      }
    } else if (key == "gap" && try_get_float(value, fval)) {
      group->set_gap(fval);
    } else if (key == "padding" && try_get_float(value, fval)) {
      group->set_padding(fval);
    } else if (key == "paddingTop" && try_get_float(value, fval)) {
      group->set_padding(fval, group->padding_right(), group->padding_bottom(),
                         group->padding_left());
    } else if (key == "paddingRight" && try_get_float(value, fval)) {
      group->set_padding(group->padding_top(), fval, group->padding_bottom(),
                         group->padding_left());
    } else if (key == "paddingBottom" && try_get_float(value, fval)) {
      group->set_padding(group->padding_top(), group->padding_right(), fval,
                         group->padding_left());
    } else if (key == "paddingLeft" && try_get_float(value, fval)) {
      group->set_padding(group->padding_top(), group->padding_right(), group->padding_bottom(),
                         fval);
    }
  }
}

void lower_pseudo_classes(
    Node *node, const std::unordered_map<std::string, parser::AstProps> &pseudo_classes) {
  if (!node) {
    return;
  }

  for (const auto &[pseudo_name, props] : pseudo_classes) {
    PseudoClassStyle style;
    for (const auto &[path, value] : props) {
      if (auto *fval = std::get_if<float>(&value)) {
        style.add_property(path, PropertyValue(*fval));
      } else if (auto *bval = std::get_if<bool>(&value)) {
        style.add_property(path, PropertyValue(*bval));
      } else if (auto *sval = std::get_if<std::string>(&value)) {
        if (is_color_literal(*sval)) {
          style.add_property(path, PropertyValue(parse_color_from_string(*sval)));
        } else {
          style.add_property(path, PropertyValue(*sval));
        }
      }
    }
    node->add_pseudo_class_style(pseudo_name, style);
  }
}

ComponentNodePtr instantiate_template_node(
    const std::shared_ptr<parser::AstNode> &ast_node, const Props &props,
    const std::map<std::string, Component::SharedPtr> &local_components) {
  ComponentNodePtr root;
  const std::string &type = ast_node->type;

  if (type == "group") {
    root = std::make_shared<Group>();
  } else if (type == "rect") {
    auto shape = std::make_shared<Shape>();
    shape->set_rect(100, 100);
    root = shape;
  } else if (type == "circle") {
    auto shape = std::make_shared<Shape>();
    shape->set_circle(50);
    root = shape;
  } else if (type == "ellipse") {
    auto shape = std::make_shared<Shape>();
    shape->set_ellipse(50, 25);
    root = shape;
  } else if (type == "polygon") {
    auto shape = std::make_shared<Shape>();
    shape->set_polygon(5, 50);
    root = shape;
  } else if (type == "star") {
    auto shape = std::make_shared<Shape>();
    shape->set_star(5, 50, 25);
    root = shape;
  } else if (type == "text") {
    auto text = std::make_shared<Text>();
    text->set_content("Text");
    root = text;
  } else if (type == "image" || type == "img") {
    root = std::make_shared<Image>();
  } else if (type == "svg") {
    root = std::make_shared<Svg>();
  } else if (type == "path") {
    auto shape = std::make_shared<Shape>();
    shape->set_path("");
    root = shape;
  } else if (type == "line") {
    auto shape = std::make_shared<Shape>();
    shape->set_line(0, 0);
    root = shape;
  } else if (type == "ring") {
    auto shape = std::make_shared<Shape>();
    shape->set_ring(50, 25);
    root = shape;
  } else if (type == "triangle") {
    auto shape = std::make_shared<Shape>();
    shape->set_triangle(20, 20, Direction::Right);
    root = shape;
  } else {
    auto component = find_component(local_components, type);
    if (!component) {
      return nullptr;
    }

    static const std::set<std::string> node_props = {"x",       "y",        "width",   "height",
                                                     "opacity", "rotation", "visible", "id"};
    Props component_props;
    for (const auto &[key, ast_value] : ast_node->properties) {
      if (node_props.count(key) && !component->has_prop(key)) {
        continue;
      }
      component_props[key] =
          coerce_component_runtime_prop(resolve_template_property_value(ast_value, props));
    }

    root = component->instantiate(component_props);
    if (!root) {
      return nullptr;
    }

    root->set_id(ast_node->id);
    for (const auto &[key, ast_value] : ast_node->properties) {
      if (node_props.count(key) && component->has_prop(key)) {
        continue;
      }
      apply_runtime_property(root.get(), type, key, resolve_template_property_value(ast_value, props));
    }
    lower_pseudo_classes(root.get(), ast_node->pseudo_classes);
    return root;
  }

  if (!root) {
    return nullptr;
  }

  root->set_id(ast_node->id);
  for (const auto &[key, ast_value] : ast_node->properties) {
    apply_runtime_property(root.get(), type, key, resolve_template_property_value(ast_value, props));
  }
  lower_pseudo_classes(root.get(), ast_node->pseudo_classes);

  std::vector<ComponentNodePtr> children;
  if (auto *group = dynamic_cast<Group *>(root.get())) {
    for (const auto &child_ast : ast_node->children) {
      auto child = instantiate_template_node(child_ast, props, local_components);
      if (!child) {
        return nullptr;
      }
      group->add_child(child);
      children.push_back(child);
    }
  }

  return wrap_shared_tree(root, std::move(children));
}

} // namespace

// ============================================================================
// AST to Runtime Converter - Implementation
// ============================================================================

// Helper macro to access Definition::Impl from void*
#define IMPL static_cast<Definition::Impl *>(impl_)

AstToRuntimeConverter::AstToRuntimeConverter(void *definition_impl) : impl_(definition_impl) {}

void AstToRuntimeConverter::convert(const parser::AstProgram &program) {
  FLEX_LOGD("Converting AST to runtime objects...");

  const auto validation = lowering::validate_runtime_program(program);
  if (!validation.ok) {
    IMPL->has_error = true;
    IMPL->error_message = validation.message;
    return;
  }

  IMPL->parsed_variables.clear();
  for (const auto &declaration : program.constants) {
    if (!declaration.is_variable) {
      continue;
    }
    IMPL->parsed_variables.emplace(declaration.name, declaration.value);
  }

  IMPL->dsl_components.clear();
  for (const auto &ast_component : program.components) {
    auto component = Component::create(ast_component->name);
    for (const auto &[name, value] : ast_component->default_props) {
      component->add_prop(name, to_component_prop_value(value));
    }

    std::vector<std::shared_ptr<parser::AstNode>> template_children;
    template_children.reserve(ast_component->children.size());
    for (const auto &child : ast_component->children) {
      template_children.push_back(child ? child->clone() : nullptr);
    }

    auto *local_components = &IMPL->dsl_components;
    component->set_builder([template_children, local_components](const Props &props) -> ComponentNodePtr {
      std::vector<ComponentNodePtr> top_level_nodes;
      top_level_nodes.reserve(template_children.size());

      for (const auto &child_ast : template_children) {
        if (!child_ast) {
          continue;
        }
        auto node = instantiate_template_node(child_ast, props, *local_components);
        if (!node) {
          return nullptr;
        }
        top_level_nodes.push_back(node);
      }

      if (top_level_nodes.empty()) {
        return std::make_shared<Group>();
      }

      if (top_level_nodes.size() == 1) {
        return top_level_nodes.front();
      }

      auto root = std::make_shared<Group>();
      for (const auto &child : top_level_nodes) {
        root->add_child(child);
      }
      return wrap_shared_tree(root, std::move(top_level_nodes));
    });

    IMPL->dsl_components[ast_component->name] = component;
  }

  // Convert main scene
  if (program.scene) {
    IMPL->scene = convert_scene(program.scene);
  }

  // Convert machines
  if (!program.machines.empty()) {
    convert_machines(program.machines);
  }

  // Convert animations to Timelines
  if (!program.animations.empty()) {
    convert_animations(program.animations);
  }

  // Convert assets
  if (program.assets) {
    convert_assets(*program.assets);
  }

  FLEX_LOGD("Conversion complete!");
}

void AstToRuntimeConverter::convert_machines(
    const std::vector<std::shared_ptr<parser::AstMachine>> &machines) {
  FLEX_LOGD("Converting {} state machine(s)...", machines.size());

  for (const auto &ast_machine : machines) {
    auto runtime_machine = convert_machine(*ast_machine);
    IMPL->machines.push_back(runtime_machine);

    FLEX_LOGD("Converted machine: {}", runtime_machine->name());

    // Convert layers
    for (const auto &ast_layer : ast_machine->layers) {
      convert_layer(runtime_machine.get(), ast_layer);
    }
  }
}

void AstToRuntimeConverter::convert_animations(
    const std::vector<std::shared_ptr<parser::AstAnim>> &animations) {
  FLEX_LOGD("Converting {} animation(s) to Timeline...", animations.size());

  IMPL->timelines.clear();

  for (const auto &ast_anim : animations) {
    auto timeline = convert_animation(*ast_anim);
    IMPL->timelines.push_back(timeline);

    FLEX_LOGD("Converted animation to Timeline: {}", timeline->name());
  }
}

RuntimeStateMachine::SharedPtr
AstToRuntimeConverter::convert_machine(const parser::AstMachine &machine) {
  auto runtime_machine = std::make_shared<RuntimeStateMachine>(machine.name);

  // Layers will be added in convert_layer

  return runtime_machine;
}

void AstToRuntimeConverter::convert_layer(RuntimeStateMachine *machine,
                                          const parser::AstLayer &layer) {
  FLEX_LOGD("  Converting layer: {}", layer.name);

  machine->add_layer(layer.name);
  auto runtime_layer = machine->get_layer(layer.name);

  const auto validate_expression = [this](const std::string &expression,
                                          const std::string &context) {
    std::shared_ptr<void> compiled;
    if (expression.empty() || !compile_mir_expression(expression, compiled)) {
      if (!IMPL->has_error) {
        IMPL->has_error = true;
        IMPL->error_message = "Invalid MIR expression for " + context + ": " + expression;
      }
      return false;
    }
    return true;
  };

  // Convert states
  for (const auto &ast_state : layer.states) {
    // Convert AstStateAction -> RuntimeStateAction
    std::vector<RuntimeStateAction> actions;
    for (const auto &ast_action : ast_state.actions) {
      if (!validate_expression(ast_action.expression,
                               "state action #" + ast_action.node_id + "." +
                                   ast_action.property)) {
        return;
      }
      actions.push_back({ast_action.node_id, ast_action.property, ast_action.expression});
    }

    for (const auto &[name, expression] : ast_state.animation_params) {
      if (!validate_expression(expression,
                               "animation parameter '" + name + "' in state '" +
                                   ast_state.name + "'")) {
        return;
      }
    }

    runtime_layer->add_state(ast_state.name, ast_state.initial, ast_state.animation,
                             ast_state.play_audio, ast_state.stop_audio,
                             actions, ast_state.animation_params);
    FLEX_LOGD("    Added state: {}{}{}{}{}", ast_state.name, ast_state.initial ? " (initial)" : "",
              ast_state.animation.empty() ? "" : " animation=\"" + ast_state.animation + "\"",
              ast_state.play_audio.empty() ? "" : " play=\"" + ast_state.play_audio + "\"",
              ast_state.stop_audio.empty() ? "" : " stop=\"" + ast_state.stop_audio + "\"");
  }

  // Convert transitions
  for (const auto &ast_trans : layer.transitions) {
    if (!ast_trans.condition_expr.empty() &&
        !validate_expression(ast_trans.condition_expr,
                             "transition '" + ast_trans.from_state + " -> " +
                                 ast_trans.to_state + "'")) {
      return;
    }
    runtime_layer->add_transition(ast_trans.from_state, ast_trans.to_state, ast_trans.condition_expr);

    if (!ast_trans.condition_expr.empty()) {
      FLEX_LOGD("    Added transition: {} -> {} when {}", ast_trans.from_state,
                ast_trans.to_state, ast_trans.condition_expr);
    } else {
      FLEX_LOGD("    Added transition: {} -> {}", ast_trans.from_state, ast_trans.to_state);
    }
  }
}

Timeline::SharedPtr AstToRuntimeConverter::convert_animation(const parser::AstAnim &anim) {
  // Create Timeline using Definition's arena allocator
  auto timeline = Timeline::create(anim.name.c_str(), IMPL->object_alloc);

  // Set properties
  timeline->set_duration(anim.duration);
  timeline->set_loop_mode(parse_loop_mode(anim.loop_mode));

  // Convert tracks
  for (const auto &ast_track : anim.tracks) {
    auto track = timeline->add_track(ast_track.property.c_str());

    const bool has_vec2_keyframes = std::any_of(
        ast_track.keyframes.begin(), ast_track.keyframes.end(),
        [](const parser::AstKeyframe &keyframe) {
          return std::holds_alternative<parser::AstVec2>(keyframe.value);
        });
    const bool all_vec2_keyframes = !ast_track.keyframes.empty() && std::all_of(
        ast_track.keyframes.begin(), ast_track.keyframes.end(),
        [](const parser::AstKeyframe &keyframe) {
          return std::holds_alternative<parser::AstVec2>(keyframe.value);
        });
    const bool has_spatial_tangents = std::any_of(
        ast_track.keyframes.begin(), ast_track.keyframes.end(),
        [](const parser::AstKeyframe &keyframe) {
          return keyframe.spatial_tangents.has_value();
        });
    const bool all_have_spatial_tangents = !ast_track.keyframes.empty() && std::all_of(
        ast_track.keyframes.begin(), ast_track.keyframes.end(),
        [](const parser::AstKeyframe &keyframe) {
          return keyframe.spatial_tangents.has_value();
        });
    const auto property_separator = ast_track.property.rfind('/');
    const std::string property_name = property_separator == std::string::npos
                                          ? ast_track.property
                                          : ast_track.property.substr(property_separator + 1);

    if (has_vec2_keyframes && (!all_vec2_keyframes || property_name != "position")) {
      IMPL->has_error = true;
      IMPL->error_message = "Animation '" + anim.name + "' track '" +
                            ast_track.property +
                            "' must use only vec2 keyframes and target 'position'";
      return timeline;
    }

    if (ast_track.spatial_interpolation == parser::AstSpatialInterpolation::CatmullRom) {
      if (!all_vec2_keyframes || ast_track.keyframes.size() < 2) {
        IMPL->has_error = true;
        IMPL->error_message = "Animation '" + anim.name + "' track '" +
                              ast_track.property +
                              "' requires at least two vec2 keyframes for catmullRom";
        return timeline;
      }
      track->set_spatial_interpolation(SpatialInterpolation::CatmullRom);
    }

    if (ast_track.spatial_interpolation == parser::AstSpatialInterpolation::CubicBezier) {
      if (!all_vec2_keyframes || ast_track.keyframes.size() < 2 ||
          !all_have_spatial_tangents) {
        IMPL->has_error = true;
        IMPL->error_message = "Animation '" + anim.name + "' track '" +
                              ast_track.property +
                              "' requires at least two bezier(position, inTangent, "
                              "outTangent) keyframes for cubicBezier";
        return timeline;
      }
      track->set_spatial_interpolation(SpatialInterpolation::CubicBezier);
    } else if (has_spatial_tangents) {
      IMPL->has_error = true;
      IMPL->error_message = "Animation '" + anim.name + "' track '" +
                            ast_track.property +
                            "' uses bezier keyframes without interpolation: cubicBezier";
      return timeline;
    }

    if (!ast_track.numeric_expression.empty()) {
      if (has_vec2_keyframes) {
        IMPL->has_error = true;
        IMPL->error_message = "Animation '" + anim.name + "' track '" +
                              ast_track.property +
                              "' cannot combine a scalar MIR expression with vec2 keyframes";
        return timeline;
      }
      try {
        track->set_numeric_expression(ast_track.numeric_expression);
      } catch (const std::invalid_argument &error) {
        if (!IMPL->has_error) {
          IMPL->has_error = true;
          IMPL->error_message = "Invalid MIR expression for animation '" + anim.name +
                                "' track '" + ast_track.property + "': " + error.what();
        }
        return timeline;
      }
    }

    // Add keyframes
    for (const auto &ast_kf : ast_track.keyframes) {
      if (auto *fval = std::get_if<float>(&ast_kf.value)) {
        track->add_keyframe(ast_kf.time, *fval);
      } else if (auto *sval = std::get_if<std::string>(&ast_kf.value)) {
        // Check if it's a color string
        if (!sval->empty() && (*sval)[0] == '#') {
          Color color = parse_color_from_string(*sval);
          track->add_keyframe(ast_kf.time, color);
        } else {
          track->add_keyframe(ast_kf.time, sval->c_str());
        }
      } else if (auto *bval = std::get_if<bool>(&ast_kf.value)) {
        track->add_keyframe(ast_kf.time, *bval ? 1.0f : 0.0f);
      } else if (auto *vval = std::get_if<parser::AstVec2>(&ast_kf.value)) {
        if (ast_kf.spatial_tangents) {
          const auto &tangents = *ast_kf.spatial_tangents;
          track->add_spatial_keyframe(
              ast_kf.time, Vec2{vval->x, vval->y},
              Vec2{tangents.in.x, tangents.in.y},
              Vec2{tangents.out.x, tangents.out.y});
        } else {
          track->add_keyframe(ast_kf.time, Vec2{vval->x, vval->y});
        }
      }
    }
  }

  for (const auto &trigger : anim.triggers) {
    timeline->add_trigger(trigger.time, trigger.event.c_str());
  }

  // DSL lowering produces an executable program eagerly. Programmatic
  // timelines retain the same API and compile lazily on first execution.
  timeline->program();

  return timeline;
}

LoopMode AstToRuntimeConverter::parse_loop_mode(const std::string &mode_str) {
  if (mode_str == "loop") {
    return LoopMode::Loop;
  } else if (mode_str == "pingpong") {
    return LoopMode::PingPong;
  }
  return LoopMode::Once;
}

void AstToRuntimeConverter::convert_assets(const parser::AstAssets &assets) {
  FLEX_LOGD("Converting {} asset(s)...", assets.assets.size());

  IMPL->parsed_assets.clear();

  for (const auto &ast_asset : assets.assets) {
    Definition::Impl::ParsedAsset parsed;
    parsed.type = ast_asset.type;
    parsed.id = ast_asset.id;
    parsed.path = ast_asset.path;

    // Extract options
    for (const auto &[key, value] : ast_asset.options) {
      if (key == "loop") {
        if (auto *bval = std::get_if<bool>(&value)) {
          parsed.loop = *bval;
        }
      } else if (key == "volume") {
        if (auto *fval = std::get_if<float>(&value)) {
          parsed.volume = *fval;
        }
      } else if (key == "preload") {
        if (auto *bval = std::get_if<bool>(&value)) {
          parsed.preload = *bval;
        }
      }
    }

    IMPL->parsed_assets.push_back(parsed);
    if (parsed.type == "audio") {
      FLEX_LOGD("  Asset: {} {} = \"{}\" (loop={}, volume={})", parsed.type, parsed.id, parsed.path,
                parsed.loop ? "true" : "false", parsed.volume);
    } else {
      FLEX_LOGD("  Asset: {} {} = \"{}\"", parsed.type, parsed.id, parsed.path);
    }
  }
}

uint32_t AstToRuntimeConverter::parse_color_rgba(const std::string &color_str) {
  return parse_color_rgba32(color_str);
}

Scene*
AstToRuntimeConverter::convert_scene(const std::shared_ptr<parser::AstScene> &scene) {
  Scene* scene_obj = Scene::create(scene->width, scene->height, IMPL->object_alloc);

  for (const auto &child_ast : scene->children) {
    auto child = convert_node(child_ast);
    if (child) {
      scene_obj->add_child(child);
    }
  }

  return scene_obj;
}

Node*
AstToRuntimeConverter::convert_node(const std::shared_ptr<parser::AstNode> &ast_node) {
  Node* node = nullptr;
  auto& arena = IMPL->object_alloc;

  bool is_component_instance = false;
  Component::SharedPtr component;
  const Component* component_def = nullptr;

  if (ast_node->type == "group") {
    node = Group::create(arena);
  } else if (ast_node->type == "rect") {
    auto* shape = Shape::create(arena);
    shape->set_rect(100, 100);
    node = shape;
  } else if (ast_node->type == "circle") {
    auto* shape = Shape::create(arena);
    shape->set_circle(50);
    node = shape;
  } else if (ast_node->type == "ellipse") {
    auto* shape = Shape::create(arena);
    shape->set_ellipse(50, 25);
    node = shape;
  } else if (ast_node->type == "polygon") {
    auto* shape = Shape::create(arena);
    shape->set_polygon(5, 50);
    node = shape;
  } else if (ast_node->type == "star") {
    auto* shape = Shape::create(arena);
    shape->set_star(5, 50, 25);
    node = shape;
  } else if (ast_node->type == "text") {
    auto* text = Text::create(arena);
    text->set_content("Text");
    node = text;
  } else if (ast_node->type == "image" || ast_node->type == "img") {
    node = Image::create(arena);
  } else if (ast_node->type == "svg") {
    node = Svg::create(arena);
  } else if (ast_node->type == "path") {
    auto* shape = Shape::create(arena);
    shape->set_path("");
    node = shape;
  } else if (ast_node->type == "line") {
    auto* shape = Shape::create(arena);
    shape->set_line(0, 0);
    node = shape;
  } else if (ast_node->type == "ring") {
    auto* shape = Shape::create(arena);
    shape->set_ring(50, 25);
    node = shape;
  } else if (ast_node->type == "triangle") {
    auto* shape = Shape::create(arena);
    shape->set_triangle(20, 20, Direction::Right);
    node = shape;
  } else {
    component = find_component(IMPL->dsl_components, ast_node->type);
    if (component) {
      is_component_instance = true;
      component_def = component.get();
      static const std::set<std::string> node_props = {"x",       "y",        "width",   "height",
                                                       "opacity", "rotation", "visible", "id"};

      Props props;
      std::map<std::string, Binding> prop_bindings;
      for (const auto &[key, ast_value] : ast_node->properties) {
        if (node_props.count(key) && !component->has_prop(key)) {
          continue;
        }

        if (auto *sval = std::get_if<std::string>(&ast_value)) {
          if (is_binding_string(*sval)) {
            Binding binding;
            if (!parse_compiled_binding(*sval, binding)) {
              IMPL->has_error = true;
              IMPL->error_message = "Invalid MIR expression for component property '" + key +
                                    "' on node '" + ast_node->id + "'";
              return nullptr;
            }
            prop_bindings[key] = std::move(binding);
            continue;
          }
        }

        props[key] = to_component_prop_value(ast_value);
      }

      auto shared_node = component->instantiate(props);
      if (shared_node) {
        IMPL->component_instances.push_back(shared_node);
        shared_node->set_id(ast_node->id);
        node = shared_node.get();
        if (!prop_bindings.empty()) {
          Definition::Impl::ParsedComponentBinding binding_def;
          binding_def.owner = IMPL;
          binding_def.component_name = component->name();
          binding_def.node_id = shared_node->id();
          binding_def.node = {IMPL, &binding_def.node_id};
          binding_def.component = component;
          binding_def.base_props = props;
          binding_def.prop_bindings = prop_bindings;
          IMPL->component_bindings.push_back(std::move(binding_def));
        }
      } else if (!IMPL->has_error) {
        IMPL->has_error = true;
        IMPL->error_message =
            "Component '" + component->name() + "' instantiation failed for node '" +
            ast_node->id + "'";
      }
    }
  }

  if (!node)
    return nullptr;

  node->set_id(ast_node->id);

  static const std::set<std::string> node_props = {"x",       "y",        "width",   "height",
                                                   "opacity", "rotation", "visible", "id"};

  for (const auto &[key, value] : ast_node->properties) {
    if (!is_component_instance) {
      if (auto sval = std::get_if<std::string>(&value)) {
        if (is_binding_string(*sval)) {
          Binding binding;
          if (!parse_compiled_binding(*sval, binding)) {
            IMPL->has_error = true;
            IMPL->error_message = "Invalid MIR expression for binding '" + key +
                                  "' on node '" + ast_node->id + "'";
            return nullptr;
          }
          IMPL->bindings.emplace_back(IMPL, node->id(), key, binding);
          continue;
        }
      }
    } else if (node_props.count(key)) {
      if (component_def && component_def->has_prop(key)) {
        continue;
      }
      if (auto sval = std::get_if<std::string>(&value)) {
        if (is_binding_string(*sval)) {
          Binding binding;
          if (!parse_compiled_binding(*sval, binding)) {
            IMPL->has_error = true;
            IMPL->error_message = "Invalid MIR expression for binding '" + key +
                                  "' on node '" + ast_node->id + "'";
            return nullptr;
          }
          IMPL->bindings.emplace_back(IMPL, node->id(), key, binding);
          continue;
        }
      }
    }
    apply_runtime_property(node, ast_node->type, key, to_runtime_prop_value(value));
  }

  lower_pseudo_classes(node, ast_node->pseudo_classes);

  for (const auto &child_ast : ast_node->children) {
    auto child = convert_node(child_ast);
    if (child) {
      if (auto* group = dynamic_cast<Group*>(node)) {
        group->add_child(child);
      }
    }
  }

  return node;
}

} // namespace flex
