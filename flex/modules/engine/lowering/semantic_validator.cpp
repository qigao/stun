#include "semantic_validator.h"

#include "flex/core/binding.h"
#include "flex/core/component.h"
#include "flex/core/expr_compiled.h"
#include "flex/core/expr_mir.h"
#include "flex/core/types.h"
#include "flex/dsl/flex_ast.h"
#include "property_internal.h"

#include <cmath>
#include <cctype>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace flex::lowering {
namespace {

enum class ValueKind {
  Number,
  String,
  Boolean,
  Color,
};

using KeyframeKind = detail::AnimatedPropertyValueKind;

struct PropertyRule {
  ValueKind kind;
  std::set<std::string> allowed_values;
};

using LocalComponents =
    std::unordered_map<std::string, const parser::AstComponent *>;
using VariableKinds = std::unordered_map<std::string, ValueKind>;

struct SceneCatalog {
  std::unordered_map<std::string, std::string> node_types;
  bool has_opaque_components = false;
};

SemanticValidationResult success() { return {}; }

SemanticValidationResult failure(std::string message) {
  return {false, std::move(message)};
}

bool is_shape_type(const std::string &type) {
  static const std::unordered_set<std::string> types = {
      "rect", "circle", "ellipse", "polygon", "star", "path", "line", "ring",
      "triangle"};
  return types.count(type) != 0;
}

bool is_builtin_node_type(const std::string &type) {
  return type == "group" || type == "text" || type == "image" || type == "img" ||
         type == "svg" || is_shape_type(type);
}

std::optional<PropertyRule> property_rule(const std::string &type,
                                          const std::string &name) {
  static const std::unordered_set<std::string> common_numbers = {
      "x",         "y",          "width",    "height",   "opacity",
      "rotation",  "scale",      "scaleX",   "scaleY",   "flexGrow",
      "flexShrink", "flexBasis"};
  if (common_numbers.count(name) != 0) {
    return PropertyRule{ValueKind::Number, {}};
  }
  if (name == "visible") {
    return PropertyRule{ValueKind::Boolean, {}};
  }
  if (name == "id") {
    return PropertyRule{ValueKind::String, {}};
  }
  if (name == "alignSelf") {
    return PropertyRule{ValueKind::String, {"auto", "start", "end", "center", "stretch"}};
  }
  if (name == "position") {
    return PropertyRule{ValueKind::String, {"static", "relative", "absolute", "fixed"}};
  }
  if (name == "anchor") {
    return PropertyRule{ValueKind::String,
                        {"topLeft", "top", "topRight", "left", "center", "right",
                         "bottomLeft", "bottom", "bottomRight"}};
  }

  if (type == "group") {
    if (name == "layout") {
      return PropertyRule{ValueKind::String, {"none", "flex"}};
    }
    if (name == "flexDirection") {
      return PropertyRule{ValueKind::String,
                          {"row", "column", "rowReverse", "columnReverse"}};
    }
    if (name == "justifyContent") {
      return PropertyRule{ValueKind::String,
                          {"start", "end", "center", "spaceBetween", "spaceAround",
                           "spaceEvenly"}};
    }
    if (name == "alignItems") {
      return PropertyRule{ValueKind::String, {"start", "end", "center", "stretch"}};
    }
    if (name == "flexWrap") {
      return PropertyRule{ValueKind::String, {"nowrap", "noWrap", "wrap"}};
    }
    static const std::unordered_set<std::string> group_numbers = {
        "gap", "padding", "paddingTop", "paddingRight", "paddingBottom", "paddingLeft"};
    if (group_numbers.count(name) != 0) {
      return PropertyRule{ValueKind::Number, {}};
    }
  }

  if (is_shape_type(type)) {
    if (name == "fill" || name == "stroke") {
      return PropertyRule{ValueKind::Color, {}};
    }
    static const std::unordered_set<std::string> shape_numbers = {
        "strokeWidth", "roughness", "bowing", "roughSeed", "hachureGap",
        "hachureAngle"};
    if (shape_numbers.count(name) != 0) {
      return PropertyRule{ValueKind::Number, {}};
    }
    if (name == "fillStyle") {
      return PropertyRule{ValueKind::String, {"solid", "hachure", "zigzag", "crosshatch"}};
    }
    if ((type == "rect" && (name == "radius" || name == "cornerRadius")) ||
        (type == "circle" && name == "radius") ||
        (type == "ellipse" && (name == "rx" || name == "ry")) ||
        (type == "polygon" && (name == "sides" || name == "radius")) ||
        (type == "star" &&
         (name == "points" || name == "radius" || name == "outerRadius" ||
          name == "innerRadius")) ||
        (type == "line" && (name == "x2" || name == "y2")) ||
        (type == "ring" && (name == "outerRadius" || name == "innerRadius"))) {
      return PropertyRule{ValueKind::Number, {}};
    }
    if (type == "path" && name == "d") {
      return PropertyRule{ValueKind::String, {}};
    }
    if (type == "triangle" && name == "direction") {
      return PropertyRule{ValueKind::String, {"left", "right", "up", "down"}};
    }
  }

  if (type == "text") {
    if (name == "content" || name == "fontFamily") {
      return PropertyRule{ValueKind::String, {}};
    }
    if (name == "color") {
      return PropertyRule{ValueKind::Color, {}};
    }
    if (name == "fontSize" || name == "lineHeight" || name == "maxWidth") {
      return PropertyRule{ValueKind::Number, {}};
    }
    if (name == "textAlign") {
      return PropertyRule{ValueKind::String, {"left", "center", "right"}};
    }
    if (name == "fontWeight") {
      return PropertyRule{ValueKind::String, {"normal", "bold"}};
    }
    if (name == "fontStyle") {
      return PropertyRule{ValueKind::String, {"normal", "italic"}};
    }
    if (name == "textDecoration") {
      return PropertyRule{ValueKind::String,
                          {"none", "underline", "strikethrough", "overline"}};
    }
  }

  if ((type == "image" || type == "img") && name == "src") {
    return PropertyRule{ValueKind::String, {}};
  }
  if (type == "svg" && (name == "src" || name == "data")) {
    return PropertyRule{ValueKind::String, {}};
  }

  return std::nullopt;
}

bool supports_animated_property(const std::string &node_type, PropertyID property) {
  if (is_shape_type(node_type)) {
    return detail::animated_property_supports(property,
                                               detail::PropertyTargetKind::Shape);
  }
  if (node_type == "text") {
    return detail::animated_property_supports(property,
                                               detail::PropertyTargetKind::Text);
  }
  if (node_type == "group") {
    return detail::animated_property_supports(property,
                                               detail::PropertyTargetKind::Group);
  }
  if (node_type == "image" || node_type == "img") {
    return detail::animated_property_supports(property,
                                               detail::PropertyTargetKind::Image);
  }
  if (node_type == "svg") {
    return detail::animated_property_supports(property,
                                               detail::PropertyTargetKind::Svg);
  }
  return false;
}

std::optional<KeyframeKind> animated_property_kind(PropertyID property) {
  const auto *descriptor = detail::animated_property_descriptor(property);
  return descriptor ? std::optional<KeyframeKind>(descriptor->value_kind)
                    : std::nullopt;
}

const char *keyframe_kind_name(KeyframeKind kind) {
  switch (kind) {
  case KeyframeKind::Scalar:
    return "scalar";
  case KeyframeKind::String:
    return "string";
  case KeyframeKind::Color:
    return "color";
  case KeyframeKind::Vec2:
    return "vec2";
  }
  return "value";
}

bool is_hex_color(const std::string &value) {
  if (value.size() != 4 && value.size() != 7 && value.size() != 9) {
    return false;
  }
  if (value[0] != '#') {
    return false;
  }
  for (size_t index = 1; index < value.size(); ++index) {
    if (!std::isxdigit(static_cast<unsigned char>(value[index]))) {
      return false;
    }
  }
  return true;
}

const char *value_kind_name(ValueKind kind) {
  switch (kind) {
  case ValueKind::Number:
    return "number";
  case ValueKind::String:
    return "string";
  case ValueKind::Boolean:
    return "boolean";
  case ValueKind::Color:
    return "color";
  }
  return "value";
}

ValueKind ast_value_kind(const parser::AstValue &value) {
  if (std::holds_alternative<float>(value)) {
    return ValueKind::Number;
  }
  if (std::holds_alternative<bool>(value)) {
    return ValueKind::Boolean;
  }
  return ValueKind::String;
}

ValueKind prop_value_kind(const PropValue &value) {
  if (std::holds_alternative<float>(value)) {
    return ValueKind::Number;
  }
  if (std::holds_alternative<bool>(value)) {
    return ValueKind::Boolean;
  }
  if (std::holds_alternative<uint32_t>(value)) {
    return ValueKind::Color;
  }
  return ValueKind::String;
}

bool binding_kind_matches(ValueKind actual, ValueKind expected) {
  return actual == expected || (expected == ValueKind::Color && actual == ValueKind::String);
}

SemanticValidationResult validate_expression(const std::string &expression,
                                             const std::string &context,
                                             const VariableKinds &variables) {
  std::shared_ptr<void> compiled;
  if (expression.empty() || !compile_mir_expression(expression, compiled)) {
    return failure("Invalid MIR expression for " + context + ": " + expression);
  }
  for (const auto &name : MirExpressionProgram::collect_variables(expression)) {
    const auto declaration = variables.find(name);
    if (declaration != variables.end() && declaration->second == ValueKind::String) {
      return failure("String variable '" + name + "' cannot be used in numeric " + context);
    }
  }
  return success();
}

SemanticValidationResult validate_binding(const std::string &source,
                                          const std::string &context,
                                          const VariableKinds &variables,
                                          std::optional<ValueKind> expected_kind = std::nullopt) {
  const Binding binding = parse_binding(source);
  if (binding.type == BindingType::Input) {
    if (source.size() == 1) {
      return failure("Invalid empty input binding for " + context);
    }
    if (expected_kind) {
      for (const auto &[name, kind] : variables) {
        if (Symbol(name) == binding.input_name && !binding_kind_matches(kind, *expected_kind)) {
          return failure("Variable '" + name + "' is " + value_kind_name(kind) +
                         " but " + context + " expects " +
                         value_kind_name(*expected_kind));
        }
      }
    }
    return success();
  }
  return validate_expression(binding.expression, "binding " + context, variables);
}

bool matches_kind(const parser::AstValue &value, ValueKind expected) {
  if (expected == ValueKind::Number) {
    const auto *number = std::get_if<float>(&value);
    return number && std::isfinite(*number);
  }
  if (expected == ValueKind::Boolean) {
    return std::holds_alternative<bool>(value);
  }
  const auto *text = std::get_if<std::string>(&value);
  if (!text) {
    return false;
  }
  return expected != ValueKind::Color || is_hex_color(*text);
}

SemanticValidationResult validate_property_value(const parser::AstValue &value,
                                                 const PropertyRule &rule,
                                                 const std::string &context,
                                                 bool component_template,
                                                 const VariableKinds &variables) {
  if (const auto *text = std::get_if<std::string>(&value);
      text && is_binding_string(*text)) {
    const Binding binding = parse_binding(*text);
    if (component_template && binding.type != BindingType::Input) {
      return failure("Computed binding is not supported inside a component template for " +
                     context);
    }
    return validate_binding(*text, context, variables, rule.kind);
  }

  if (!matches_kind(value, rule.kind)) {
    return failure("Property " + context + " expects " + value_kind_name(rule.kind));
  }

  if (!rule.allowed_values.empty()) {
    const auto &text = std::get<std::string>(value);
    if (rule.allowed_values.count(text) == 0) {
      return failure("Invalid value '" + text + "' for property " + context);
    }
  }
  return success();
}

const parser::AstComponent *find_local_component(const LocalComponents &components,
                                                  const std::string &name) {
  const auto it = components.find(name);
  return it == components.end() ? nullptr : it->second;
}

SemanticValidationResult validate_node(
    const std::shared_ptr<parser::AstNode> &node, const LocalComponents &local_components,
    bool component_template, const std::string &parent_context,
    const VariableKinds &variables, SceneCatalog *catalog) {
  if (!node) {
    return failure("Null node in " + parent_context);
  }

  const parser::AstComponent *local_component =
      find_local_component(local_components, node->type);
  const auto registered_component = ComponentRegistry::instance().get(node->type);
  if (!is_builtin_node_type(node->type) && !local_component && !registered_component) {
    return failure("Unknown node or component type '" + node->type + "' for node '" +
                   node->id + "'");
  }

  if (catalog) {
    if (node->id.empty()) {
      return failure("Scene nodes must have a non-empty ID");
    }
    if (!catalog->node_types.emplace(node->id, node->type).second) {
      return failure("Duplicate scene node ID '" + node->id + "'");
    }
    if (local_component || registered_component) {
      catalog->has_opaque_components = true;
    }
  }

  const std::string node_context = "'" + node->id + "' (" + node->type + ")";
  for (const auto &[name, value] : node->properties) {
    bool is_component_prop = false;
    bool component_allows_any = false;
    if (local_component) {
      is_component_prop = local_component->default_props.count(name) != 0;
      component_allows_any = local_component->default_props.empty();
    } else if (registered_component) {
      is_component_prop = registered_component->has_prop(name);
      component_allows_any = registered_component->props().empty();
    }

    if (is_component_prop) {
      if (const auto *text = std::get_if<std::string>(&value);
          text && is_binding_string(*text)) {
        if (component_template && parse_binding(*text).type != BindingType::Input) {
          return failure("Computed binding is not supported inside a component template for " +
                         node_context + "." + name);
        }
        std::optional<ValueKind> expected_kind;
        if (local_component) {
          expected_kind = ast_value_kind(local_component->default_props.at(name));
        } else if (const auto *prop = registered_component->get_prop_def(name)) {
          expected_kind = prop_value_kind(prop->default_value);
        }
        auto result = validate_binding(*text, "component property '" + name + "' on " +
                                                  node_context,
                                       variables, expected_kind);
        if (!result.ok) {
          return result;
        }
      }
      continue;
    }

    auto rule = property_rule(is_builtin_node_type(node->type) ? node->type : "", name);
    if (!rule && component_allows_any) {
      if (const auto *text = std::get_if<std::string>(&value);
          text && is_binding_string(*text)) {
        if (component_template && parse_binding(*text).type != BindingType::Input) {
          return failure("Computed binding is not supported inside a component template for " +
                         node_context + "." + name);
        }
        auto result = validate_binding(*text, "component property '" + name + "' on " +
                                                  node_context,
                                       variables);
        if (!result.ok) {
          return result;
        }
      }
      continue;
    }
    if (!rule) {
      return failure("Unknown property '" + name + "' on node " + node_context);
    }
    auto result = validate_property_value(value, *rule,
                                          "'" + name + "' on node " + node_context,
                                          component_template, variables);
    if (!result.ok) {
      return result;
    }

    if (const auto *text = std::get_if<std::string>(&value);
        text && is_binding_string(*text) && get_property_id(name.c_str()) == PropertyID::Unknown) {
      return failure("Property '" + name + "' on node " + node_context +
                     " does not support runtime binding");
    }
  }

  for (const auto &[pseudo_name, properties] : node->pseudo_classes) {
    for (const auto &[name, value] : properties) {
      auto rule = property_rule(node->type, name);
      if (!rule || get_property_id(name.c_str()) == PropertyID::Unknown) {
        return failure("Unknown or non-styleable property '" + name + "' in pseudo-class '" +
                       pseudo_name + "' on node " + node_context);
      }
      if (const auto *text = std::get_if<std::string>(&value);
          text && is_binding_string(*text)) {
        return failure("Bindings are not supported in pseudo-class '" + pseudo_name +
                       "' on node " + node_context);
      }
      auto result = validate_property_value(
          value, *rule, "'" + name + "' in pseudo-class '" + pseudo_name + "' on node " +
                            node_context,
          component_template, variables);
      if (!result.ok) {
        return result;
      }
    }
  }

  for (const auto &child : node->children) {
    auto result =
        validate_node(child, local_components, component_template, node_context, variables,
                      catalog);
    if (!result.ok) {
      return result;
    }
  }
  return success();
}

KeyframeKind keyframe_kind(const parser::AstKeyframeValue &value) {
  if (std::holds_alternative<float>(value) || std::holds_alternative<bool>(value)) {
    return KeyframeKind::Scalar;
  }
  if (std::holds_alternative<parser::AstVec2>(value)) {
    return KeyframeKind::Vec2;
  }
  const auto &text = std::get<std::string>(value);
  return is_hex_color(text) ? KeyframeKind::Color : KeyframeKind::String;
}

SemanticValidationResult validate_animation(const parser::AstAnim &animation,
                                            const VariableKinds &variables,
                                            const SceneCatalog &catalog) {
  if (animation.name.empty()) {
    return failure("Animation name cannot be empty");
  }
  if (!std::isfinite(animation.duration) || animation.duration < 0.0f) {
    return failure("Animation '" + animation.name + "' has invalid duration");
  }
  if (animation.loop_mode != "once" && animation.loop_mode != "loop" &&
      animation.loop_mode != "pingpong") {
    return failure("Animation '" + animation.name + "' has invalid loop mode '" +
                   animation.loop_mode + "'");
  }

  std::unordered_set<std::string> track_properties;
  for (const auto &track : animation.tracks) {
    if (track.property.empty()) {
      return failure("Animation '" + animation.name + "' has a track with no property");
    }
    if (!track_properties.insert(track.property).second) {
      return failure("Animation '" + animation.name + "' has duplicate track '" +
                     track.property + "'");
    }
    if (track.keyframes.empty()) {
      return failure("Animation '" + animation.name + "' track '" + track.property +
                     "' must contain at least one keyframe");
    }

    const auto separator = track.property.rfind('/');
    const std::string property_name =
        separator == std::string::npos ? track.property : track.property.substr(separator + 1);
    const PropertyID property_id = get_property_id(property_name.c_str());
    if (property_id == PropertyID::Unknown) {
      return failure("Animation '" + animation.name + "' targets unknown property '" +
                     track.property + "'");
    }
    if (!track.property.empty() && track.property[0] == '#') {
      if (separator == std::string::npos || separator <= 1 ||
          separator + 1 >= track.property.size()) {
        return failure("Animation '" + animation.name + "' has malformed target '" +
                       track.property + "'");
      }
      const std::string target_id = track.property.substr(1, separator - 1);
      const auto target = catalog.node_types.find(target_id);
      if (target == catalog.node_types.end()) {
        if (!catalog.has_opaque_components) {
          return failure("Animation '" + animation.name + "' targets unknown node '#" +
                         target_id + "'");
        }
      } else if (is_builtin_node_type(target->second) &&
                 !supports_animated_property(target->second, property_id)) {
        return failure("Animation '" + animation.name + "' property '" + property_name +
                       "' is not supported by node '#" + target_id + "' (" +
                       target->second + ")");
      }
    }

    std::optional<KeyframeKind> expected_kind;
    float previous_time = 0.0f;
    bool first = true;
    bool all_have_spatial_tangents = !track.keyframes.empty();
    bool has_spatial_tangents = false;
    for (const auto &keyframe : track.keyframes) {
      if (!std::isfinite(keyframe.time) || keyframe.time < 0.0f) {
        return failure("Animation '" + animation.name + "' track '" + track.property +
                       "' has a negative or non-finite keyframe time");
      }
      if (!first && keyframe.time <= previous_time) {
        return failure("Animation '" + animation.name + "' track '" + track.property +
                       "' keyframe times must be strictly increasing");
      }
      first = false;
      previous_time = keyframe.time;

      if (const auto *number = std::get_if<float>(&keyframe.value);
          number && !std::isfinite(*number)) {
        return failure("Animation '" + animation.name + "' track '" + track.property +
                       "' has a non-finite keyframe value");
      }
      if (const auto *position = std::get_if<parser::AstVec2>(&keyframe.value);
          position && (!std::isfinite(position->x) || !std::isfinite(position->y))) {
        return failure("Animation '" + animation.name + "' track '" + track.property +
                       "' has a non-finite vec2 keyframe value");
      }
      if (keyframe.spatial_tangents &&
          (!std::isfinite(keyframe.spatial_tangents->in.x) ||
           !std::isfinite(keyframe.spatial_tangents->in.y) ||
           !std::isfinite(keyframe.spatial_tangents->out.x) ||
           !std::isfinite(keyframe.spatial_tangents->out.y))) {
        return failure("Animation '" + animation.name + "' track '" + track.property +
                       "' has non-finite spatial tangents");
      }

      const KeyframeKind current_kind = keyframe_kind(keyframe.value);
      if (expected_kind && *expected_kind != current_kind) {
        return failure("Animation '" + animation.name + "' track '" + track.property +
                       "' mixes incompatible keyframe value types");
      }
      expected_kind = current_kind;
      has_spatial_tangents = has_spatial_tangents || keyframe.spatial_tangents.has_value();
      all_have_spatial_tangents =
          all_have_spatial_tangents && keyframe.spatial_tangents.has_value();
    }

    const bool is_vec2 = expected_kind && *expected_kind == KeyframeKind::Vec2;
    const auto property_kind = animated_property_kind(property_id);
    if (expected_kind && property_kind && *expected_kind != *property_kind) {
      return failure("Animation '" + animation.name + "' track '" + track.property +
                     "' expects " + keyframe_kind_name(*property_kind) +
                     " keyframes, got " + keyframe_kind_name(*expected_kind));
    }
    if (is_vec2 && property_name != "position") {
      return failure("Animation '" + animation.name + "' track '" + track.property +
                     "' must use only vec2 keyframes and target 'position'");
    }
    if (track.spatial_interpolation == parser::AstSpatialInterpolation::CatmullRom &&
        (!is_vec2 || track.keyframes.size() < 2)) {
      return failure("Animation '" + animation.name + "' track '" + track.property +
                     "' requires at least two vec2 keyframes for catmullRom");
    }
    if (track.spatial_interpolation == parser::AstSpatialInterpolation::CubicBezier &&
        (!is_vec2 || track.keyframes.size() < 2 || !all_have_spatial_tangents)) {
      return failure("Animation '" + animation.name + "' track '" + track.property +
                     "' requires at least two bezier(position, inTangent, outTangent) "
                     "keyframes for cubicBezier");
    }
    if (track.spatial_interpolation != parser::AstSpatialInterpolation::CubicBezier &&
        has_spatial_tangents) {
      return failure("Animation '" + animation.name + "' track '" + track.property +
                     "' uses bezier keyframes without interpolation: cubicBezier");
    }
    if (!track.numeric_expression.empty()) {
      if (expected_kind && *expected_kind != KeyframeKind::Scalar) {
        return failure("Animation '" + animation.name + "' track '" + track.property +
                       "' numeric expression requires scalar keyframes");
      }
      auto result = validate_expression(
          track.numeric_expression,
          "animation '" + animation.name + "' track '" + track.property + "'",
          variables);
      if (!result.ok) {
        return result;
      }
    }
  }

  for (const auto &trigger : animation.triggers) {
    if (!std::isfinite(trigger.time) || trigger.time < 0.0f || trigger.event.empty()) {
      return failure("Animation '" + animation.name + "' has an invalid trigger");
    }
  }
  return success();
}

SemanticValidationResult validate_machine(
    const parser::AstMachine &machine,
    const std::unordered_set<std::string> &animation_names,
    const VariableKinds &variables,
    const SceneCatalog &catalog) {
  if (machine.name.empty()) {
    return failure("State machine name cannot be empty");
  }
  std::unordered_set<std::string> layer_names;
  for (const auto &layer : machine.layers) {
    if (layer.name.empty() || !layer_names.insert(layer.name).second) {
      return failure("State machine '" + machine.name + "' has an empty or duplicate layer '" +
                     layer.name + "'");
    }

    std::unordered_set<std::string> state_names;
    size_t initial_count = 0;
    for (const auto &state : layer.states) {
      if (state.name.empty() || !state_names.insert(state.name).second) {
        return failure("Layer '" + layer.name + "' in machine '" + machine.name +
                       "' has an empty or duplicate state '" + state.name + "'");
      }
      initial_count += state.initial ? 1u : 0u;
      if (!state.animation.empty() && animation_names.count(state.animation) == 0) {
        return failure("State '" + state.name + "' in machine '" + machine.name +
                       "' references unknown animation '" + state.animation + "'");
      }
      for (const auto &action : state.actions) {
        const PropertyID property_id = get_property_id(action.property.c_str());
        if (action.node_id.empty() || property_id == PropertyID::Unknown) {
          return failure("State '" + state.name + "' in machine '" + machine.name +
                         "' has an invalid set action target");
        }
        const auto target = catalog.node_types.find(action.node_id);
        if (target == catalog.node_types.end()) {
          if (!catalog.has_opaque_components) {
            return failure("State '" + state.name + "' in machine '" + machine.name +
                           "' set action targets unknown node '#" + action.node_id + "'");
          }
        } else if (is_builtin_node_type(target->second) &&
                   !supports_animated_property(target->second, property_id)) {
          return failure("State '" + state.name + "' in machine '" + machine.name +
                         "' property '" + action.property +
                         "' is not supported by node '#" + action.node_id + "' (" +
                         target->second + ")");
        }
        const auto property_kind = animated_property_kind(property_id);
        if (property_kind && *property_kind != KeyframeKind::Scalar) {
          return failure("State '" + state.name + "' in machine '" + machine.name +
                         "' set action requires a scalar property, but '#" +
                         action.node_id + "." + action.property + "' is " +
                         keyframe_kind_name(*property_kind));
        }
        auto result = validate_expression(
            action.expression,
            "state action #" + action.node_id + "." + action.property, variables);
        if (!result.ok) {
          return result;
        }
      }
      for (const auto &[name, expression] : state.animation_params) {
        if (name.empty()) {
          return failure("State '" + state.name + "' in machine '" + machine.name +
                         "' has an unnamed animation parameter");
        }
        auto result = validate_expression(
            expression, "animation parameter '" + name + "' in state '" + state.name + "'",
            variables);
        if (!result.ok) {
          return result;
        }
      }
    }
    if (initial_count > 1) {
      return failure("Layer '" + layer.name + "' in machine '" + machine.name +
                     "' has more than one initial state");
    }

    for (const auto &transition : layer.transitions) {
      if (state_names.count(transition.from_state) == 0 ||
          state_names.count(transition.to_state) == 0) {
        return failure("Transition '" + transition.from_state + " -> " +
                       transition.to_state + "' in layer '" + layer.name +
                       "' references an unknown state");
      }
      if (!transition.condition_expr.empty()) {
        auto result = validate_expression(
            transition.condition_expr,
            "transition '" + transition.from_state + " -> " + transition.to_state + "'",
            variables);
        if (!result.ok) {
          return result;
        }
      }
    }
  }
  return success();
}

} // namespace

SemanticValidationResult validate_runtime_program(const parser::AstProgram &program) {
  std::unordered_set<std::string> declaration_names;
  VariableKinds variables;
  for (const auto &declaration : program.constants) {
    if (declaration.name.empty() || !declaration_names.insert(declaration.name).second) {
      return failure("Top-level const/var names must be non-empty and unique");
    }
    if (const auto *number = std::get_if<float>(&declaration.value);
        number && !std::isfinite(*number)) {
      return failure("Top-level declaration '" + declaration.name +
                     "' has a non-finite numeric value");
    }
    if (declaration.is_variable) {
      variables.emplace(declaration.name, ast_value_kind(declaration.value));
    }
  }

  LocalComponents local_components;
  for (const auto &component : program.components) {
    if (!component || component->name.empty() ||
        !local_components.emplace(component->name, component.get()).second) {
      return failure("Component names must be non-empty and unique");
    }
  }

  for (const auto &[name, component] : local_components) {
    for (const auto &child : component->children) {
      auto result = validate_node(child, local_components, true, "component '" + name + "'",
                                  variables, nullptr);
      if (!result.ok) {
        return result;
      }
    }
  }

  SceneCatalog catalog;
  if (program.scene) {
    for (const auto &child : program.scene->children) {
      auto result = validate_node(child, local_components, false,
                                  "scene '" + program.scene->name + "'", variables,
                                  &catalog);
      if (!result.ok) {
        return result;
      }
    }
  }

  std::unordered_set<std::string> animation_names;
  for (const auto &animation : program.animations) {
    if (!animation || !animation_names.insert(animation->name).second) {
      return failure("Animation names must be non-empty and unique");
    }
    auto result = validate_animation(*animation, variables, catalog);
    if (!result.ok) {
      return result;
    }
  }

  std::unordered_set<std::string> machine_names;
  for (const auto &machine : program.machines) {
    if (!machine || !machine_names.insert(machine->name).second) {
      return failure("State machine names must be non-empty and unique");
    }
    auto result = validate_machine(*machine, animation_names, variables, catalog);
    if (!result.ok) {
      return result;
    }
  }

  return success();
}

} // namespace flex::lowering
