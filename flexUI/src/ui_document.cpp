#include <flexUI/ui_document.h>

#include <flexUI/box.h>
#include <flexUI/element.h>
#include <flexUI/widget_registry.h>

#include <flex/dsl/flex_parser.h>
#include <flex/core/expr_mir.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace flexUI {

namespace {

using flex::parser::AstNode;
using flex::parser::AstUiDocument;

UiDocumentError make_error(UiDocumentErrorCode code, std::string message, int line = 0,
                           int column = 0) {
  return UiDocumentError{code, std::move(message), line, column};
}

bool is_string_property(const std::string &name) {
  return name == "class" || name == "classes" || name == "utility" || name == "utilities" ||
         name == "text" || name == "content" || name == "slot" ||
         name.rfind("var.", 0) == 0 || name.rfind("on.", 0) == 0 ||
         name.rfind("bind.", 0) == 0;
}

template <typename Properties>
bool has_alias_conflict(const Properties &properties, const char *first,
                        const char *second) {
  return properties.count(first) != 0 && properties.count(second) != 0;
}

template <typename Properties>
UiDocumentError validate_node_fields(
    const std::string &tag, const std::string &id,
    const Properties &properties, const UiDocumentLimits &limits,
    std::unordered_set<std::string> &ids) {
  if (tag.empty()) {
    return make_error(UiDocumentErrorCode::InvalidNode,
                      "UI nodes require a non-empty tag");
  }
  if (tag.size() > limits.max_string_bytes || id.size() > limits.max_string_bytes) {
    return make_error(UiDocumentErrorCode::StringLimitExceeded,
                      "UI node tag or id exceeds the string limit");
  }
  if (!id.empty() && !ids.insert(id).second) {
    return make_error(UiDocumentErrorCode::DuplicateElementId,
                      "duplicate UI element id: " + id);
  }
  if (properties.size() > limits.max_properties_per_node) {
    return make_error(UiDocumentErrorCode::PropertyLimitExceeded,
                      "UI node exceeds maximum property count: " + id);
  }
  if (has_alias_conflict(properties, "class", "classes") ||
      has_alias_conflict(properties, "utility", "utilities") ||
      has_alias_conflict(properties, "text", "content")) {
    return make_error(UiDocumentErrorCode::InvalidProperty,
                      "UI node contains conflicting property aliases: " + id);
  }

  if ((properties.count("layout_width") != 0) !=
      (properties.count("layout_height") != 0)) {
    return make_error(UiDocumentErrorCode::InvalidProperty,
                      "explicit layout size requires width and height together");
  }
  if (properties.count("slot") &&
      (properties.count("attr.data-slot") || properties.count("data-slot"))) {
    return make_error(UiDocumentErrorCode::InvalidProperty,
                      "UI slot cannot be overridden by a data-slot attribute");
  }

  for (const auto &[name, value] : properties) {
    if (name.empty() || name == "id" || name == "attr.id") {
      return make_error(UiDocumentErrorCode::InvalidProperty,
                        "invalid or reserved UI property on node: " + id);
    }
    if (name.size() > limits.max_string_bytes) {
      return make_error(UiDocumentErrorCode::StringLimitExceeded,
                        "UI property name exceeds the string limit");
    }
    if (const auto *text = std::get_if<std::string>(&value)) {
      if (text->size() > limits.max_string_bytes) {
        return make_error(UiDocumentErrorCode::StringLimitExceeded,
                          "UI property value exceeds the string limit: " + name);
      }
    }
    if (is_string_property(name) && !std::holds_alternative<std::string>(value)) {
      return make_error(UiDocumentErrorCode::InvalidProperty,
                        "UI property requires a string value: " + name);
    }
    if (name == "focusable" && !std::holds_alternative<bool>(value)) {
      return make_error(UiDocumentErrorCode::InvalidProperty, "focusable requires a bool value");
    }
    if (name == "layout_width" || name == "layout_height") {
      const auto *number = std::get_if<float>(&value);
      if (!number || !std::isfinite(*number) || *number <= 0.0F) {
        return make_error(UiDocumentErrorCode::InvalidProperty,
                          "explicit layout size requires finite positive floats");
      }
    }
    if (name == "tab_index") {
      const auto *number = std::get_if<float>(&value);
      const double converted = number ? static_cast<double>(*number) : 0.0;
      if (!number || !std::isfinite(converted) || std::floor(converted) != converted ||
          converted < static_cast<double>(std::numeric_limits<int>::min()) ||
          converted > static_cast<double>(std::numeric_limits<int>::max())) {
        return make_error(UiDocumentErrorCode::InvalidProperty,
                          "tab_index requires a finite integer value");
      }
    }
    if (name.rfind("var.", 0) == 0 && name.size() == 4) {
      return make_error(UiDocumentErrorCode::InvalidProperty,
                        "var. requires a CSS custom property name");
    }
    if (name.rfind("attr.", 0) == 0 && name.size() == 5) {
      return make_error(UiDocumentErrorCode::InvalidProperty, "attr. requires an attribute name");
    }
    if (name.rfind("on.", 0) == 0 && name.size() == 3) {
      return make_error(UiDocumentErrorCode::InvalidProperty, "on. requires an event name");
    }
  }
  return {};
}

UiDocumentError validate_node(const AstNode &node, const UiDocumentLimits &limits,
                              std::size_t depth, std::size_t &node_count,
                              std::unordered_set<std::string> &ids) {
  if (depth > limits.max_depth) {
    return make_error(UiDocumentErrorCode::DepthLimitExceeded,
                      "UI document exceeds maximum tree depth");
  }
  if (++node_count > limits.max_nodes) {
    return make_error(UiDocumentErrorCode::NodeLimitExceeded,
                      "UI document exceeds maximum node count");
  }
  auto error =
      validate_node_fields(node.type, node.id, node.properties, limits, ids);
  if (error) {
    return error;
  }
  for (const auto &duplicate : node.duplicate_properties) {
    if (duplicate.name.rfind("on.", 0) == 0) {
      return make_error(UiDocumentErrorCode::DuplicateEventBinding,
                        "duplicate UI event binding on element '" + node.id +
                            "': " + duplicate.name,
                        duplicate.source.line, duplicate.source.column);
    }
    if (duplicate.name.rfind("bind.", 0) == 0) {
      return make_error(UiDocumentErrorCode::DuplicateBindingTarget,
                        "duplicate UI binding target on element '" + node.id +
                            "': " + duplicate.name,
                        duplicate.source.line, duplicate.source.column);
    }
  }
  for (const auto &child : node.children) {
    if (!child) {
      return make_error(UiDocumentErrorCode::InvalidNode, "UI document contains a null child");
    }
    error = validate_node(*child, limits, depth + 1, node_count, ids);
    if (error) {
      return error;
    }
  }
  return {};
}

// Native chart/DOT projection is immutable typed UI input, not imperative
// mutation of a live Box. Reject nonfinite geometry or colors before any
// Element or Widget factory is invoked.
UiDocumentError validate_native_presentation(
    const UiNodeDefinition &node, const UiDocumentLimits &limits) {
  if (node.native_widget_key.size() > limits.max_string_bytes) {
    return make_error(UiDocumentErrorCode::StringLimitExceeded,
                      "native widget key exceeds the bounded string limit");
  }
  if (!node.presentation) return {};
  const auto &p = *node.presentation;
  if (p.absolute && p.relative) {
    return make_error(UiDocumentErrorCode::InvalidProperty,
                      "native UI position cannot be absolute and relative");
  }
  if (p.absolute && !p.has_bounds) {
    return make_error(UiDocumentErrorCode::InvalidProperty,
                      "native absolute positioning requires explicit bounds");
  }
  if (p.has_bounds &&
      (!std::isfinite(p.x) || !std::isfinite(p.y) ||
       !std::isfinite(p.width) || !std::isfinite(p.height) ||
       p.width <= 0.0f || p.height <= 0.0f)) {
    return make_error(UiDocumentErrorCode::InvalidProperty,
                      "native layout bounds must be finite and positive");
  }
  const auto color_ok = [](const std::optional<Color> &value) {
    if (!value) return true;
    const auto &v = *value;
    return std::isfinite(v.r) && std::isfinite(v.g) &&
           std::isfinite(v.b) && std::isfinite(v.a) &&
           v.r >= 0.0f && v.r <= 1.0f &&
           v.g >= 0.0f && v.g <= 1.0f &&
           v.b >= 0.0f && v.b <= 1.0f &&
           v.a >= 0.0f && v.a <= 1.0f;
  };
  if (!color_ok(p.background_color) ||
      !color_ok(p.border_color) || !color_ok(p.text_color)) {
    return make_error(UiDocumentErrorCode::InvalidProperty,
                      "native color components must be finite within [0, 1]");
  }
  if ((p.border_width &&
       (!std::isfinite(*p.border_width) || *p.border_width < 0.0f)) ||
      (p.border_radius &&
       (!std::isfinite(*p.border_radius) || *p.border_radius < 0.0f)) ||
      (p.opacity &&
       (!std::isfinite(*p.opacity) || *p.opacity < 0.0f ||
        *p.opacity > 1.0f)) ||
      (p.font_size &&
       (!std::isfinite(*p.font_size) || *p.font_size <= 0.0f)) ||
      (p.font_family && p.font_family->size() > limits.max_string_bytes)) {
    return make_error(UiDocumentErrorCode::InvalidProperty,
                      "invalid native border, opacity or font presentation");
  }
  return {};
}

UiDocumentError validate_definition_node(const UiNodeDefinition &node,
                                         const UiDocumentLimits &limits, std::size_t depth,
                                         std::size_t &node_count,
                                         std::unordered_set<std::string> &ids) {
  if (depth > limits.max_depth) {
    return make_error(UiDocumentErrorCode::DepthLimitExceeded,
                      "UI document exceeds maximum tree depth");
  }
  if (++node_count > limits.max_nodes) {
    return make_error(UiDocumentErrorCode::NodeLimitExceeded,
                      "UI document exceeds maximum node count");
  }
  auto error =
      validate_node_fields(node.tag, node.id, node.properties, limits, ids);
  if (error) {
    return error;
  }
  error = validate_native_presentation(node, limits);
  if (error) return error;
  for (const auto &child : node.children) {
    error = validate_definition_node(child, limits, depth + 1, node_count, ids);
    if (error) {
      return error;
    }
  }
  return {};
}

UiNodeDefinition copy_node(const AstNode &source) {
  UiNodeDefinition target;
  target.tag = source.type;
  target.id = source.id;
  target.properties = source.properties;
  for (const auto &[name, span] : source.property_spans) {
    target.property_spans.emplace(
        name, SourceSpan{span.line, span.column, span.length});
  }
  target.children.reserve(source.children.size());
  for (const auto &child : source.children) {
    target.children.push_back(copy_node(*child));
  }
  return target;
}

static constexpr std::pair<std::string_view, UiEventKind> kUiEvents[] = {
    {"click", UiEventKind::Click},
    {"mouse_move", UiEventKind::MouseMove},
    {"mouse_down", UiEventKind::MouseDown},
    {"mouse_up", UiEventKind::MouseUp},
    {"mouse_wheel", UiEventKind::MouseWheel},
    {"key_down", UiEventKind::KeyDown},
    {"key_up", UiEventKind::KeyUp},
    {"text_input", UiEventKind::TextInput},
    {"focus_in", UiEventKind::FocusIn},
    {"focus_out", UiEventKind::FocusOut},
    {"composition_start", UiEventKind::CompositionStart},
    {"composition_update", UiEventKind::CompositionUpdate},
    {"composition_end", UiEventKind::CompositionEnd},
};

bool resolve_event_kind(std::string_view name, UiEventKind &kind) {
  const auto found = std::find_if(
      std::begin(kUiEvents), std::end(kUiEvents),
      [name](const auto &entry) { return entry.first == name; });
  if (found == std::end(kUiEvents)) {
    return false;
  }
  kind = found->second;
  return true;
}

std::string_view event_kind_name(UiEventKind kind) {
  const auto found = std::find_if(
      std::begin(kUiEvents), std::end(kUiEvents),
      [kind](const auto &entry) { return entry.second == kind; });
  if (found == std::end(kUiEvents)) {
    throw std::logic_error("compiled UI program contains an invalid event kind");
  }
  return found->first;
}

UiDocumentError lower_event_bindings(const UiNodeDefinition &node,
                                     const UiDocumentLimits &limits,
                                     std::vector<EventBinding> &bindings) {
  std::vector<EventBinding> node_bindings;
  for (const auto &[name, value] : node.properties) {
    if (name.rfind("on.", 0) != 0) {
      continue;
    }

    const auto span_it = node.property_spans.find(name);
    const SourceSpan span =
        span_it == node.property_spans.end() ? SourceSpan{} : span_it->second;
    if (node.id.empty()) {
      return make_error(UiDocumentErrorCode::RequiredElementId,
                        "UI event bindings require an explicit element id",
                        span.line, span.column);
    }
    UiEventKind kind;
    if (!resolve_event_kind(std::string_view(name).substr(3), kind)) {
      return make_error(UiDocumentErrorCode::UnknownEvent,
                        "unknown UI event: " + name.substr(3), span.line,
                        span.column);
    }
    if (bindings.size() + node_bindings.size() >= limits.max_event_bindings) {
      return make_error(UiDocumentErrorCode::EventBindingLimitExceeded,
                        "UI document exceeds maximum event binding count",
                        span.line, span.column);
    }
    node_bindings.push_back(
        EventBinding{kind, node.id, std::get<std::string>(value), span});
  }

  std::sort(node_bindings.begin(), node_bindings.end(),
            [](const EventBinding &left, const EventBinding &right) {
              if (left.source.line != right.source.line) {
                return left.source.line < right.source.line;
              }
              if (left.source.column != right.source.column) {
                return left.source.column < right.source.column;
              }
              return left.handler < right.handler;
            });
  bindings.insert(bindings.end(),
                  std::make_move_iterator(node_bindings.begin()),
                  std::make_move_iterator(node_bindings.end()));

  for (const auto &child : node.children) {
    auto error = lower_event_bindings(child, limits, bindings);
    if (error) {
      return error;
    }
  }
  return {};
}

std::string unwrap_binding_expression(const std::string &value) {
  std::string expression;
  if (value.size() >= 3 && value[0] == '$' &&
      ((value[1] == '{' && value.back() == '}') ||
       (value[1] == '(' && value.back() == ')'))) {
    expression = value.substr(2, value.size() - 3);
  } else if (value.size() >= 2 && value[0] == '$') {
    expression = value.substr(1);
  } else {
    return {};
  }

  const auto first = expression.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = expression.find_last_not_of(" \t\r\n");
  return expression.substr(first, last - first + 1);
}

bool is_binding_identifier(std::string_view value) {
  if (value.empty() ||
      (std::isalpha(static_cast<unsigned char>(value.front())) == 0 &&
       value.front() != '_')) {
    return false;
  }
  return std::all_of(value.begin() + 1, value.end(), [](char character) {
    return std::isalnum(static_cast<unsigned char>(character)) != 0 ||
           character == '_';
  });
}

struct LoweredBinding {
  BindingDefinition definition;
  std::shared_ptr<const flex::MirExpressionProgram> mir_program;
  std::string source_property;
};

UiDocumentError validate_binding_ownership(
    const std::vector<LoweredBinding> &bindings) {
  bool owns_class_list = false;
  std::unordered_set<std::string> owned_class_tokens;
  for (const auto &binding : bindings) {
    const auto target = binding.definition.target;
    const bool owns_whole_class_list =
        target == UiBindingTargetKind::Classes ||
        target == UiBindingTargetKind::Utilities;
    if (owns_whole_class_list) {
      if (owns_class_list || !owned_class_tokens.empty()) {
        return make_error(
            UiDocumentErrorCode::BindingTargetConflict,
            "UI binding target '" + binding.source_property +
                "' on element '" + binding.definition.element_id +
                "' conflicts with existing class-list ownership",
            binding.definition.source_span.line,
            binding.definition.source_span.column);
      }
      owns_class_list = true;
      continue;
    }
    if (target == UiBindingTargetKind::ClassToggle) {
      if (owns_class_list ||
          !owned_class_tokens.insert(binding.definition.target_name).second) {
        return make_error(
            UiDocumentErrorCode::BindingTargetConflict,
            "UI binding target '" + binding.source_property +
                "' on element '" + binding.definition.element_id +
                "' conflicts with existing class-list ownership",
            binding.definition.source_span.line,
            binding.definition.source_span.column);
      }
    }
  }
  return {};
}

UiDocumentError lower_bindings(const UiNodeDefinition &node,
                               const UiDocumentLimits &limits,
                               std::vector<BindingDefinition> &definitions,
                               std::vector<std::shared_ptr<const flex::MirExpressionProgram>> &programs) {
  std::vector<LoweredBinding> node_bindings;
  for (const auto &[name, value] : node.properties) {
    if (name.rfind("bind.", 0) != 0) {
      continue;
    }

    const auto span_it = node.property_spans.find(name);
    const SourceSpan span =
        span_it == node.property_spans.end() ? SourceSpan{} : span_it->second;
    if (definitions.size() + node_bindings.size() >= limits.max_bindings) {
      return make_error(UiDocumentErrorCode::BindingLimitExceeded,
                        "UI document exceeds maximum binding count", span.line,
                        span.column);
    }

    const auto *source = std::get_if<std::string>(&value);
    const std::string expression =
        source ? unwrap_binding_expression(*source) : std::string{};
    if (expression.empty()) {
      return make_error(UiDocumentErrorCode::InvalidBindingExpression,
                        "UI binding requires a $input or ${expression} value",
                        span.line, span.column);
    }

    if (node.id.empty()) {
      return make_error(UiDocumentErrorCode::RequiredElementId,
                        "UI bindings require an explicit element id",
                        span.line, span.column);
    }

    const std::string target_name = name.substr(5);
    LoweredBinding lowered;
    lowered.source_property = name;
    lowered.definition.element_id = node.id;
    lowered.definition.expression = expression;
    lowered.definition.source_span = span;

    if (target_name == "text" || target_name == "classes" ||
        target_name == "utilities") {
      if (!is_binding_identifier(expression)) {
        return make_error(UiDocumentErrorCode::InvalidBindingExpression,
                          "string UI bindings require one input identifier",
                          span.line, span.column);
      }
      if (target_name == "text") {
        lowered.definition.target = UiBindingTargetKind::Text;
      } else if (target_name == "classes") {
        lowered.definition.target = UiBindingTargetKind::Classes;
      } else {
        lowered.definition.target = UiBindingTargetKind::Utilities;
      }
      lowered.definition.source = UiBindingSourceKind::StringInput;
      lowered.definition.dependencies = {expression};
    } else if (target_name.rfind("class_", 0) == 0 &&
               target_name.size() > 6) {
      lowered.definition.target = UiBindingTargetKind::ClassToggle;
      lowered.definition.source = UiBindingSourceKind::BoolExpression;
      lowered.definition.target_name = target_name.substr(6);
      lowered.definition.dependencies =
          flex::MirExpressionProgram::collect_variables(expression);
      auto mir_program = flex::MirExpressionProgram::compile(
          expression, lowered.definition.dependencies);
      if (!mir_program) {
        return make_error(UiDocumentErrorCode::InvalidBindingExpression,
                          "invalid MIR UI binding expression: " + expression,
                          span.line, span.column);
      }
      lowered.mir_program =
          std::shared_ptr<const flex::MirExpressionProgram>(std::move(mir_program));
      lowered.definition.uses_jit = lowered.mir_program->uses_jit();
    } else {
      return make_error(UiDocumentErrorCode::UnknownBindingTarget,
                        "unknown UI binding target: " + target_name, span.line,
                        span.column);
    }
    node_bindings.push_back(std::move(lowered));
  }

  std::sort(node_bindings.begin(), node_bindings.end(),
            [](const LoweredBinding &left, const LoweredBinding &right) {
              if (left.definition.source_span.line !=
                  right.definition.source_span.line) {
                return left.definition.source_span.line <
                       right.definition.source_span.line;
              }
              return left.definition.source_span.column <
                     right.definition.source_span.column;
            });
  auto ownership_error = validate_binding_ownership(node_bindings);
  if (ownership_error) {
    return ownership_error;
  }
  for (auto &binding : node_bindings) {
    definitions.push_back(std::move(binding.definition));
    programs.push_back(std::move(binding.mir_program));
  }

  for (const auto &child : node.children) {
    auto error = lower_bindings(child, limits, definitions, programs);
    if (error) {
      return error;
    }
  }
  return {};
}

std::vector<std::string> split_tokens(const std::string &value) {
  std::istringstream input(value);
  std::vector<std::string> tokens;
  std::string token;
  while (input >> token) {
    tokens.push_back(std::move(token));
  }
  return tokens;
}

std::string value_to_string(const UiDocumentValue &value) {
  if (const auto *text = std::get_if<std::string>(&value)) {
    return *text;
  }
  if (const auto *boolean = std::get_if<bool>(&value)) {
    return *boolean ? "true" : "false";
  }
  std::ostringstream output;
  output << std::setprecision(std::numeric_limits<float>::max_digits10) << std::get<float>(value);
  return output.str();
}

// Slots belong to one component instance, not the Box-global ID namespace.
// Check the entire tree before any factory invocation or Box modification.
UiDocumentError validate_slots(const UiNodeDefinition &root,
                               const UiMountOptions &options) {
  std::unordered_set<std::string> names;
  std::vector<const UiNodeDefinition *> pending{&root};
  while (!pending.empty()) {
    const auto *node = pending.back();
    pending.pop_back();
    const auto it = node->properties.find("slot");
    if (it != node->properties.end()) {
      const auto *slot = std::get_if<std::string>(&it->second);
      if (!slot || slot->empty() || !names.emplace(*slot).second) {
        const auto location = node->property_spans.find("slot");
        const auto span = location == node->property_spans.end()
                              ? node->source : location->second;
        return make_error(UiDocumentErrorCode::DuplicateSlot,
                          "UI slot must be a unique non-empty string",
                          span.line, span.column);
      }
    }
    for (const auto &child : node->children) pending.push_back(&child);
  }
  if (options.required_slots.size() > UiDocumentLimits{}.max_nodes) {
    return make_error(UiDocumentErrorCode::NodeLimitExceeded,
                      "component requires more slots than the node budget");
  }
  for (const auto &required : options.required_slots) {
    if (required.empty() || names.count(required) == 0) {
      return make_error(UiDocumentErrorCode::MissingSlot,
                        "UI component requires missing slot: " + required);
    }
  }
  return {};
}

std::string scoped_id(std::string_view local, const UiMountOptions &options) {
  if (options.id_policy == UiIdPolicy::Scoped) {
    return options.id_scope + "::" + std::string(local);
  }
  return std::string(local);
}

UiDocumentError validate_target(Box &box, const UiDocumentDefinition &definition,
                                const UiMountOptions &options) {
  if (options.mode == UiMountMode::Root && box.root()) {
    return make_error(UiDocumentErrorCode::BoxAlreadyHasRoot,
                      "UI document requires a Box without an installed root");
  }

  std::vector<const UiNodeDefinition *> pending{&definition.root};
  while (!pending.empty()) {
    const UiNodeDefinition *node = pending.back();
    pending.pop_back();
    if (!node->id.empty() && options.id_policy != UiIdPolicy::LocalOnly &&
        box.get_by_id(scoped_id(node->id, options))) {
      return make_error(UiDocumentErrorCode::ElementIdConflict,
                        "Box already contains element id: " +
                            scoped_id(node->id, options));
    }
    for (const auto &[name, value] : node->properties) {
      if (name != "utility" && name != "utilities") {
        continue;
      }
      if (!box.utility_jit_enabled()) {
        return make_error(UiDocumentErrorCode::UtilityJitDisabled,
                          "explicit UI utilities require an enabled utility JIT");
      }
      for (const auto &token : split_tokens(std::get<std::string>(value))) {
        if (!box.is_known_utility(token)) {
          return make_error(UiDocumentErrorCode::UnknownUtility,
                            "unknown explicit utility token: " + token);
        }
      }
    }
    for (const auto &child : node->children) pending.push_back(&child);
  }
  return {};
}

struct DetachedTree {
  Element *root = nullptr;
  std::vector<std::unique_ptr<Element>> elements;
  std::unordered_map<std::string, Element *> elements_by_id;
  std::unordered_map<std::string, Element *> slots_by_name;
  struct DetachedWidget {
    Element *host = nullptr;
    std::unique_ptr<Widget> widget;
  };
  std::vector<DetachedWidget> widgets;
};

void apply_native_presentation(Element &element,
                               const UiNodePresentation &p) {
  auto &style = element.style_;
  if (p.has_bounds) {
    style.width_size.kind = CssSizeKind::Length;
    style.width_size.value = p.width;
    style.width_size.expression.clear();
    style.height_size.kind = CssSizeKind::Length;
    style.height_size.value = p.height;
    style.height_size.expression.clear();
    style.width = p.width;
    style.height = p.height;
    style.width_is_percent = false;
    style.height_is_percent = false;
    element.set_layout_size(p.width, p.height);
  }
  if (p.absolute) {
    style.position = Position::Absolute;
    style.left = p.x;
    style.top = p.y;
    style.right = NAN;
    style.bottom = NAN;
    element.set_position_mode(flex::PositionMode::Absolute);
    element.set_position_offsets(p.y, NAN, NAN, p.x);
    element.set_position(p.x, p.y);
  } else if (p.relative) {
    style.position = Position::Relative;
  }
  if (p.clipped) element.set_clip(true);
  if (p.background_color) style.background_color = *p.background_color;
  if (p.border_color) {
    style.border_color = *p.border_color;
    style.has_border_side_colors = true;
    for (auto &color : style.border_colors) color = *p.border_color;
  }
  if (p.border_width) {
    for (auto &width : style.border_width) width = *p.border_width;
  }
  if (p.border_style) {
    for (auto &border : style.border_style) border = *p.border_style;
  }
  if (p.border_radius) {
    for (auto &radius : style.border_radius) radius = *p.border_radius;
  }
  if (p.opacity) style.opacity = *p.opacity;
  if (p.font_size) style.font_size = *p.font_size;
  if (p.font_family) style.font_family = *p.font_family;
  if (p.text_color) style.text_color = *p.text_color;
  if (p.text_align) style.text_align = *p.text_align;
  if (p.z_index) element.set_z_index(*p.z_index);
}

void apply_properties(Element &element, const UiNodeDefinition &definition,
                      bool include_event_attributes) {
  const auto find_property = [&](const char *first,
                                 const char *second = nullptr) -> const UiDocumentValue * {
    auto it = definition.properties.find(first);
    if (it != definition.properties.end()) {
      return &it->second;
    }
    if (second) {
      it = definition.properties.find(second);
      if (it != definition.properties.end()) {
        return &it->second;
      }
    }
    return nullptr;
  };

  if (const auto *classes = find_property("class", "classes")) {
    element.set_classes(std::get<std::string>(*classes));
  }
  if (const auto *utilities = find_property("utility", "utilities")) {
    element.set_utilities(std::get<std::string>(*utilities));
  }
  if (const auto *text = find_property("text", "content")) {
    element.set_text(std::get<std::string>(*text));
  }
  if (const auto *slot = find_property("slot")) {
    element.set_attribute("data-slot", std::get<std::string>(*slot));
  }
  if (const auto *width = find_property("layout_width")) {
    const auto *height = find_property("layout_height");
    // Both fields were validated during whole-tree preflight. Stage layout
    // before ownership publication, including the prepared native Plot host.
    element.set_layout_size(std::get<float>(*width), std::get<float>(*height));
  }
  if (const auto *focusable = find_property("focusable")) {
    element.focusable = std::get<bool>(*focusable);
  }
  if (const auto *tab_index = find_property("tab_index")) {
    element.tab_index = static_cast<int>(std::get<float>(*tab_index));
  }

  for (const auto &[name, value] : definition.properties) {
    if (name == "class" || name == "classes" || name == "utility" || name == "utilities" ||
        name == "text" || name == "content" || name == "slot" ||
        name == "layout_width" || name == "layout_height" ||
        name == "focusable" || name == "tab_index") {
      continue;
    }
    if (name.rfind("var.", 0) == 0) {
      element.set_custom_property("--" + name.substr(4), value_to_string(value));
    } else if (name.rfind("attr.", 0) == 0) {
      element.set_attribute(name.substr(5), value_to_string(value));
    } else if (include_event_attributes && name.rfind("on.", 0) == 0) {
      element.set_attribute("data-flexui-on-" + name.substr(3), std::get<std::string>(value));
    } else if (name.rfind("on.", 0) == 0) {
      continue;
    } else if (name.rfind("bind.", 0) == 0) {
      continue;
    } else if (const auto *boolean = std::get_if<bool>(&value)) {
      if (*boolean) {
        element.set_attribute(name);
      }
    } else {
      element.set_attribute(name, value_to_string(value));
    }
  }
  if (definition.presentation) {
    apply_native_presentation(element, *definition.presentation);
  }
}

Element *build_node(const UiNodeDefinition &definition, DetachedTree &tree,
                    bool include_event_attributes,
                    const WidgetRegistry *registry,
                    UiDocumentError &build_error) {
  std::unique_ptr<Widget> widget;
  if (registry) {
    auto created = registry->create(definition);
    if (!created) {
      build_error = make_error(
          created.error.code == WidgetRegistryErrorCode::UnknownTag
              ? UiDocumentErrorCode::UnknownElementTag
              : UiDocumentErrorCode::WidgetFactoryFailed,
          created.error.message);
      return nullptr;
    }
    widget = std::move(created.widget);
  }

  auto element = std::make_unique<Element>();
  element->set_tag(definition.tag);
  if (!definition.id.empty()) {
    element->set_element_id(definition.id);
  }
  if (widget) {
    element->widget = widget.get();
    element->focusable = true;
  }
  apply_properties(*element, definition, include_event_attributes);

  Element *raw = element.get();
  if (!definition.id.empty()) {
    tree.elements_by_id.emplace(definition.id, raw);
  }
  tree.elements.push_back(std::move(element));
  if (const auto found = definition.properties.find("slot");
      found != definition.properties.end()) {
    tree.slots_by_name.emplace(std::get<std::string>(found->second), raw);
  }
  if (widget) {
    tree.widgets.push_back({raw, std::move(widget)});
  }

  for (const auto &child_definition : definition.children) {
    Element *child = build_node(child_definition, tree,
                                include_event_attributes, registry,
                                build_error);
    if (build_error) {
      return nullptr;
    }
    if (!raw->append(child)) {
      throw std::logic_error("failed to attach detached UI element");
    }
  }
  return raw;
}

struct ParsedUiSource {
  std::shared_ptr<const UiDocumentDefinition> definition;
  std::vector<UiResourceDefinition> resources;
  UiDocumentError error;

  explicit operator bool() const {
    return definition != nullptr && !static_cast<bool>(error);
  }
};

enum class ResourceLowering { Omit, Include };

ParsedUiSource parse_ui_source(std::string_view source,
                               std::string_view document_name,
                               const UiDocumentLimits &limits,
                               ResourceLowering resource_lowering) {
  ParsedUiSource result;
  if (source.size() > limits.max_source_bytes) {
    result.error = make_error(UiDocumentErrorCode::SourceTooLarge,
                              "UI document source exceeds configured limit");
    return result;
  }
  if (source.find('\0') != std::string_view::npos) {
    result.error = make_error(UiDocumentErrorCode::ParseError,
                              "UI document source must not contain embedded NUL bytes");
    return result;
  }
  if (document_name.size() > limits.max_string_bytes) {
    result.error = make_error(UiDocumentErrorCode::StringLimitExceeded,
                              "requested UI document name exceeds the string limit");
    return result;
  }

  const std::string owned_source(source);
  auto program = flex::parser::parse(owned_source.c_str());
  if (!program) {
    result.error = make_error(UiDocumentErrorCode::ParseError, flex::parser::get_error(),
                              flex::parser::get_error_line(), flex::parser::get_error_column());
    return result;
  }

  std::unordered_set<std::string> document_names;
  for (const auto &document : program->ui_documents) {
    if (!document || document->name.empty()) {
      result.error = make_error(UiDocumentErrorCode::DuplicateDocumentName,
                                "UI document names must be non-empty and unique");
      return result;
    }
    if (document->name.size() > limits.max_string_bytes) {
      result.error = make_error(UiDocumentErrorCode::StringLimitExceeded,
                                "UI document name exceeds the string limit");
      return result;
    }
    if (!document_names.insert(document->name).second) {
      result.error = make_error(UiDocumentErrorCode::DuplicateDocumentName,
                                "UI document names must be non-empty and unique");
      return result;
    }
  }
  if (program->ui_documents.empty()) {
    result.error =
        make_error(UiDocumentErrorCode::MissingDocument, "source does not contain a ui document");
    return result;
  }

  const AstUiDocument *selected = nullptr;
  if (!document_name.empty()) {
    for (const auto &document : program->ui_documents) {
      if (document->name == document_name) {
        selected = document.get();
        break;
      }
    }
    if (!selected) {
      result.error = make_error(UiDocumentErrorCode::MissingDocument,
                                "UI document not found: " + std::string(document_name));
      return result;
    }
  } else if (program->ui_documents.size() == 1) {
    selected = program->ui_documents.front().get();
  } else {
    result.error = make_error(UiDocumentErrorCode::AmbiguousDocument,
                              "multiple UI documents require an explicit document name");
    return result;
  }

  if (selected->children.size() != 1) {
    result.error = make_error(UiDocumentErrorCode::InvalidRootCount,
                              "UI document must contain exactly one root");
    return result;
  }

  std::size_t node_count = 0;
  std::unordered_set<std::string> ids;
  result.error = validate_node(*selected->children.front(), limits, 1, node_count, ids);
  if (result.error) {
    return result;
  }

  if (resource_lowering == ResourceLowering::Include && program->assets) {
    if (program->assets->assets.size() > limits.max_resources) {
      result.error = make_error(
          UiDocumentErrorCode::ResourceLimitExceeded,
          "UI document exceeds maximum declared resource count");
      return result;
    }
    result.resources.reserve(program->assets->assets.size());
    for (const auto &asset : program->assets->assets) {
      result.resources.push_back(
          UiResourceDefinition{asset.type, asset.id, asset.path, asset.options});
    }
  }

  auto definition = std::make_shared<UiDocumentDefinition>();
  definition->name = selected->name;
  definition->root = copy_node(*selected->children.front());
  result.definition = std::move(definition);
  return result;
}

} // namespace

UiDocumentParseResult parse_ui_document(std::string_view source,
                                        std::string_view document_name,
                                        const UiDocumentLimits &limits) {
  auto parsed = parse_ui_source(source, document_name, limits,
                                ResourceLowering::Omit);
  return UiDocumentParseResult{std::move(parsed.definition),
                               std::move(parsed.error)};
}

struct CompiledUiProgram::Impl {
  std::shared_ptr<const UiDocumentDefinition> definition;
  std::vector<EventBinding> event_bindings;
  std::unordered_map<flex::Symbol, std::vector<std::size_t>, flex::SymbolHash>
      event_binding_index;
  std::vector<BindingDefinition> bindings;
  std::vector<std::shared_ptr<const flex::MirExpressionProgram>> binding_programs;
  std::vector<UiResourceDefinition> resources;
};

CompiledUiProgram::CompiledUiProgram(std::unique_ptr<Impl> impl)
    : impl_(std::move(impl)) {}

CompiledUiProgram::~CompiledUiProgram() = default;

const std::string &CompiledUiProgram::name() const noexcept {
  return impl_->definition->name;
}

const UiDocumentDefinition &CompiledUiProgram::definition() const noexcept {
  return *impl_->definition;
}

const std::vector<EventBinding> &
CompiledUiProgram::event_bindings() const noexcept {
  return impl_->event_bindings;
}

const EventBinding *CompiledUiProgram::find_event_binding(
    std::string_view element_id, UiEventKind event) const noexcept {
  const auto found = impl_->event_binding_index.find(flex::Symbol(element_id));
  if (found == impl_->event_binding_index.end()) {
    return nullptr;
  }
  for (const auto index : found->second) {
    const auto &binding = impl_->event_bindings[index];
    if (binding.event == event &&
        std::string_view(binding.element_id) == element_id) {
      return &binding;
    }
  }
  return nullptr;
}

const std::vector<BindingDefinition> &
CompiledUiProgram::bindings() const noexcept {
  return impl_->bindings;
}

const std::vector<UiResourceDefinition> &
CompiledUiProgram::resources() const noexcept {
  return impl_->resources;
}

UiDocumentCompileResult compile_ui_document(std::string_view source,
                                            std::string_view document_name,
                                            const UiDocumentLimits &limits) {
  auto parsed = parse_ui_source(source, document_name, limits,
                                ResourceLowering::Include);
  if (!parsed) {
    UiDocumentCompileResult result;
    result.error = std::move(parsed.error);
    return result;
  }

  return compile_ui_definition(*parsed.definition, std::move(parsed.resources),
                               limits);
}

UiDocumentCompileResult compile_ui_definition(
    UiDocumentDefinition definition,
    std::vector<UiResourceDefinition> resources,
    const UiDocumentLimits &limits) {
  UiDocumentCompileResult result;
  if (definition.name.empty()) {
    result.error = make_error(UiDocumentErrorCode::InvalidNode,
                              "UI document definition requires a name");
    return result;
  }
  if (definition.name.size() > limits.max_string_bytes) {
    result.error = make_error(UiDocumentErrorCode::StringLimitExceeded,
                              "UI document name exceeds the string limit");
    return result;
  }
  std::size_t node_count = 0;
  std::unordered_set<std::string> ids;
  result.error = validate_definition_node(definition.root, limits, 1,
                                          node_count, ids);
  if (result.error) {
    return result;
  }
  if (resources.size() > limits.max_resources) {
    result.error = make_error(
        UiDocumentErrorCode::ResourceLimitExceeded,
        "UI document exceeds maximum declared resource count");
    return result;
  }

  auto impl = std::make_unique<CompiledUiProgram::Impl>();
  impl->definition =
      std::make_shared<const UiDocumentDefinition>(std::move(definition));
  impl->resources = std::move(resources);
  result.error = lower_event_bindings(impl->definition->root, limits,
                                      impl->event_bindings);
  if (result.error) {
    return result;
  }
  impl->event_binding_index.reserve(impl->event_bindings.size());
  for (std::size_t index = 0; index < impl->event_bindings.size(); ++index) {
    const auto &binding = impl->event_bindings[index];
    impl->event_binding_index[flex::Symbol(binding.element_id)].push_back(index);
  }
  result.error = lower_bindings(impl->definition->root, limits,
                                impl->bindings, impl->binding_programs);
  if (result.error) {
    return result;
  }

  result.program = std::shared_ptr<const CompiledUiProgram>(
      new CompiledUiProgram(std::move(impl)));
  return result;
}

UiDocumentCompileResult compile_scoped_ui_components(
    const CompiledUiProgram &application,
    const std::vector<UiScopedComponent> &components,
    const WidgetRegistry &registry, const UiDocumentLimits &limits) {
  UiDocumentCompileResult result;
  if (components.size() > limits.max_nodes) {
    result.error = make_error(UiDocumentErrorCode::NodeLimitExceeded,
                              "too many scoped XML component instances");
    return result;
  }
  // Build a detached compiler input. No Box or live Widget/Controller is
  // touched until ALL component programs are accepted and recompiled into
  // ONE authoritative immutable program.
  UiDocumentDefinition merged = application.definition();
  // A composition of individually bounded XML templates is NOT itself
  // bounded until we charge the total node cost. Reject oversized repeated
  // component graphs BEFORE copying or adding any child subtrees.
  const auto count_nodes = [&limits](const UiNodeDefinition &root) {
    std::size_t total = 0;
    std::vector<const UiNodeDefinition *> pending{&root};
    while (!pending.empty()) {
      const UiNodeDefinition *node = pending.back();
      pending.pop_back();
      if (++total > limits.max_nodes) return total;
      for (const auto &child : node->children) pending.push_back(&child);
    }
    return total;
  };
  std::size_t used_nodes = count_nodes(merged.root);
  if (used_nodes > limits.max_nodes) {
    result.error = make_error(UiDocumentErrorCode::NodeLimitExceeded,
                              "base application exceeds XML node budget");
    return result;
  }
  std::unordered_set<std::string> base_hosts;
  {
    std::vector<const UiNodeDefinition *> pending{&merged.root};
    while (!pending.empty()) {
      const UiNodeDefinition *node = pending.back();
      pending.pop_back();
      if (!node->id.empty()) base_hosts.insert(node->id);
      for (const auto &child : node->children) pending.push_back(&child);
    }
  }
  std::unordered_set<std::string> scopes;
  for (const auto &instance : components) {
    const auto valid_scope = [&] {
      if (instance.scope.empty() ||
          instance.scope.size() > limits.max_string_bytes / 2 ||
          !((instance.scope.front() >= 'A' && instance.scope.front() <= 'Z') ||
            (instance.scope.front() >= 'a' && instance.scope.front() <= 'z') ||
            instance.scope.front() == '_')) {
        return false;
      }
      return std::all_of(instance.scope.begin() + 1, instance.scope.end(),
                         [](char ch) {
                           return (ch >= 'A' && ch <= 'Z') ||
                                  (ch >= 'a' && ch <= 'z') ||
                                  (ch >= '0' && ch <= '9') ||
                                  ch == '_' || ch == '-';
                         });
    };
    if (!valid_scope() || !scopes.emplace(instance.scope).second ||
        !instance.program) {
      result.error = make_error(
          UiDocumentErrorCode::InvalidComponentScope,
          "component requires a unique ASCII scope and a compiled program");
      return result;
    }
    if (base_hosts.count(instance.host_id) == 0) {
      result.error = make_error(
          UiDocumentErrorCode::MissingComponentHost,
          "scoped XML component host must exist in the base application: " +
              instance.host_id);
      return result;
    }
    if (!instance.program->resources().empty()) {
      result.error = make_error(
          UiDocumentErrorCode::UnsupportedComponentResources,
          "component-specific resources require an explicit scoped resource "
          "ownership contract");
      return result;
    }
    const auto added = count_nodes(instance.program->definition().root);
    if (added > limits.max_nodes - used_nodes) {
      result.error = make_error(
          UiDocumentErrorCode::NodeLimitExceeded,
          "combined scoped XML components exceed application node budget");
      return result;
    }
    used_nodes += added;
    // Host lookup occurs on the new private candidate for each insertion.
    // Only base_hosts are allowed so a child component cannot attach to a
    // prior component or depend on insertion order.
    UiNodeDefinition *host = nullptr;
    std::vector<UiNodeDefinition *> pending{&merged.root};
    while (!pending.empty()) {
      UiNodeDefinition *node = pending.back();
      pending.pop_back();
      if (node->id == instance.host_id) {
        host = node;
        break;
      }
      for (auto &child : node->children) pending.push_back(&child);
    }
    if (!host || !registry.descriptor(host->tag) ||
        registry.descriptor(host->tag)->kind != UiNodeKind::Container) {
      result.error = make_error(
          UiDocumentErrorCode::InvalidMountTarget,
          "scoped XML component requires a structural, non-Widget host");
      return result;
    }

    UiNodeDefinition subtree = instance.program->definition().root;
    // Validate local semantic references against this one immutable component
    // before qualifying IDs. Never silently link a label/ARIA target from one
    // instance to another instance or to an application-global element.
    std::unordered_set<std::string> local_ids;
    {
      std::vector<const UiNodeDefinition *> pending{&subtree};
      while (!pending.empty()) {
        const auto *node = pending.back();
        pending.pop_back();
        if (!node->id.empty()) local_ids.insert(node->id);
        for (const auto &child : node->children) pending.push_back(&child);
      }
    }
    std::vector<UiNodeDefinition *> nodes{&subtree};
    while (!nodes.empty()) {
      UiNodeDefinition *node = nodes.back();
      nodes.pop_back();
      if (!node->native_widget_key.empty()) {
        result.error = make_error(
            UiDocumentErrorCode::InvalidComponentScope,
            "native component Widget factory keys need an explicit "
            "per-instance registry contract");
        return result;
      }
      if (!node->id.empty()) {
        if (node->id.find("::") != std::string::npos ||
            node->id.size() + instance.scope.size() + 2 >
                limits.max_string_bytes) {
          result.error = make_error(
              UiDocumentErrorCode::InvalidComponentScope,
              "component element ID cannot be safely qualified");
          return result;
        }
        node->id = instance.scope + "::" + node->id;
      }
      // Semantic IDREF attributes, including multi-token ARIA references,
      // must point to this component's OWN known IDs. External URLs remain
      // ordinary URLs; fragment links (#id) refer to qualified local IDs.
      for (auto &[name, value] : node->properties) {
        const bool namespaced = name.rfind("attr.", 0) == 0;
        const std::string_view key = namespaced
                                         ? std::string_view(name).substr(5)
                                         : std::string_view(name);
        const bool multiple = key == "aria-labelledby" ||
                              key == "aria-describedby" ||
                              key == "aria-controls" ||
                              key == "aria-owns" ||
                              key == "aria-flowto" ||
                              key == "headers";
        const bool single = key == "for" || key == "list" ||
                            key == "form" ||
                            key == "aria-activedescendant" ||
                            key == "aria-details" ||
                            key == "aria-errormessage";
        const bool href = key == "href";
        if (!multiple && !single && !href) continue;
        // The plain and attr:-prefixed spellings map to the same Element
        // attribute. Never allow one to override the scoped IDREF of another.
        const std::string alias = namespaced
                                      ? std::string(key)
                                      : "attr." + name;
        if (node->properties.count(alias) != 0) {
          result.error = make_error(
              UiDocumentErrorCode::InvalidProperty,
              "component IDREF attribute has conflicting XML aliases: " +
                  std::string(key));
          return result;
        }
        const auto *ref = std::get_if<std::string>(&value);
        if (!ref) {
          result.error = make_error(UiDocumentErrorCode::InvalidProperty,
                                    "component IDREF attribute must be a string");
          return result;
        }
        if (href && (ref->empty() || ref->front() != '#')) continue;
        const std::string raw = href ? ref->substr(1) : *ref;
        std::istringstream tokens(raw);
        std::string id;
        std::string qualified;
        std::size_t count = 0;
        while (tokens >> id) {
          if (local_ids.count(id) == 0 ||
              id.size() + instance.scope.size() + 2 >
                  limits.max_string_bytes) {
            result.error = make_error(
                UiDocumentErrorCode::InvalidComponentScope,
                "component IDREF must resolve inside the same scope: " + id);
            return result;
          }
          if (++count > 1) qualified += " ";
          qualified += instance.scope + "::" + id;
        }
        if (count == 0 || (!multiple && count != 1)) {
          result.error = make_error(
              UiDocumentErrorCode::InvalidProperty,
              "component IDREF requires valid local target IDs");
          return result;
        }
        value = href ? "#" + qualified : qualified;
      }
      // The same template can appear more than once in a single program.
      // Its slot names stay available to CSS/semantic matching via
      // data-slot, but must not enter the app-wide UNIQUE slot index.
      if (auto it = node->properties.find("slot");
          it != node->properties.end()) {
        const auto *slot = std::get_if<std::string>(&it->second);
        if (!slot || node->properties.count("attr.data-slot") != 0) {
          result.error = make_error(
              UiDocumentErrorCode::InvalidProperty,
              "component slot cannot shadow data-slot");
          return result;
        }
        std::string value = *slot;
        node->properties.erase(it);
        node->properties.emplace("attr.data-slot", std::move(value));
        node->property_spans.erase("slot");
      }
      for (auto &child : node->children) nodes.push_back(&child);
    }
    host->children.push_back(std::move(subtree));
  }
  result = compile_ui_definition(std::move(merged), application.resources(),
                                 limits);
  if (result) {
    if (auto error = registry.validate(*result.program, limits)) {
      result.program.reset();
      result.error = std::move(error);
    }
  }
  return result;
}

UiDocumentInstantiateResult
UiDocumentInstantiator::instantiate(Box &box, const UiDocumentDefinition &definition) {
  return instantiate_impl(box, definition, nullptr, nullptr, {});
}

UiDocumentInstantiateResult UiDocumentInstantiator::instantiate_impl(
    Box &box, const UiDocumentDefinition &definition,
    const CompiledUiProgram *program, const WidgetRegistry *registry,
    const UiMountOptions &options) {
  UiDocumentInstantiateResult result;
  if (definition.name.empty()) {
    result.error =
        make_error(UiDocumentErrorCode::InvalidNode, "UI document definition requires a name");
    return result;
  }
  if (definition.name.size() > UiDocumentLimits{}.max_string_bytes) {
    result.error = make_error(UiDocumentErrorCode::StringLimitExceeded,
                              "UI document definition name exceeds the string limit");
    return result;
  }

  std::size_t node_count = 0;
  std::unordered_set<std::string> ids;
  result.error = validate_definition_node(definition.root, UiDocumentLimits{}, 1, node_count, ids);
  if (result.error) {
    return result;
  }

  if (registry) {
    result.error = registry->validate(definition);
    if (result.error) {
      return result;
    }
  }

  if (options.mode == UiMountMode::Root) {
    if (options.parent || options.id_policy != UiIdPolicy::Global ||
        !options.id_scope.empty()) {
      result.error = make_error(UiDocumentErrorCode::InvalidMountTarget,
                                "root mount requires global IDs and no parent");
      return result;
    }
  } else if (options.mode == UiMountMode::DetachedFragment) {
    if (options.parent) {
      result.error = make_error(UiDocumentErrorCode::InvalidMountTarget,
                                "detached fragment must not have a parent");
      return result;
    }
  } else if (options.mode == UiMountMode::Child) {
    if (!options.parent || !box.owns_element(options.parent) ||
        options.parent->is_widget_owned() || options.parent->widget != nullptr) {
      result.error = make_error(UiDocumentErrorCode::InvalidMountTarget,
                                "child mount requires a Box-owned application parent");
      return result;
    }
  } else {
    result.error = make_error(UiDocumentErrorCode::InvalidMountTarget,
                              "unknown UI mount mode");
    return result;
  }
  if (options.id_policy != UiIdPolicy::Global &&
      options.id_policy != UiIdPolicy::LocalOnly &&
      options.id_policy != UiIdPolicy::Scoped) {
    result.error = make_error(UiDocumentErrorCode::InvalidIdScope,
                              "unknown UI ID policy");
    return result;
  }
  if (options.id_policy == UiIdPolicy::Scoped) {
    if (options.id_scope.empty() ||
        options.id_scope.size() > UiDocumentLimits{}.max_string_bytes / 2 ||
        options.id_scope.find("::") != std::string::npos) {
      result.error = make_error(UiDocumentErrorCode::InvalidIdScope,
                                "scoped IDs require a bounded nonempty namespace");
      return result;
    }
  } else if (!options.id_scope.empty()) {
    result.error = make_error(UiDocumentErrorCode::InvalidIdScope,
                              "ID namespace supplied without scoped policy");
    return result;
  }
  if (options.mode != UiMountMode::Root && program &&
      (!program->event_bindings().empty() || !program->bindings().empty())) {
    result.error = make_error(UiDocumentErrorCode::UnsupportedFragmentBindings,
                              "compiled fragment controller bindings require scoped resolution");
    return result;
  }

  result.error = validate_slots(definition.root, options);
  if (result.error) return result;
  if (options.id_policy == UiIdPolicy::Scoped) {
    for (const auto &id : ids) {
      if (id.size() + options.id_scope.size() + 2 >
          UiDocumentLimits{}.max_string_bytes) {
        result.error = make_error(UiDocumentErrorCode::InvalidIdScope,
                                  "qualified UI ID exceeds string limit");
        return result;
      }
    }
  }
  result.error = validate_target(box, definition, options);
  if (result.error) return result;

  DetachedTree detached;
  std::optional<UiBindingRuntime::TransactionCheckpoint> binding_transaction;
  std::optional<SourceSpan> active_binding_span;
  const std::size_t original_element_count = box.elements_.size();
  const std::size_t original_widget_count = box.widgets_.size();
  const std::size_t original_active_widget_count = box.active_widgets_.size();
  const std::uint64_t original_element_generation =
      box.next_element_generation_;
  bool index_committed = false;
  // Keep the original ID map as a no-allocation rollback snapshot after
  // publishing a new global/scoped index. This also handles OOM failure in
  // Element::replace_children without constructing new scoped IDs in catch.
  decltype(box.elements_by_id_) next_index;
  bool child_attached = false;
  std::vector<Element *> original_children;
  // Do not open a no-op binding transaction for controller-free fragments:
  // Child publication must be the last potentially failing operation.
  const auto *bindings = program && !program->impl_->bindings.empty()
                             ? &program->impl_->bindings : nullptr;
  try {
    UiDocumentError build_error;
    detached.root = build_node(definition.root, detached, program == nullptr,
                               registry, build_error);
    if (build_error) {
      result.error = std::move(build_error);
      return result;
    }
    if (program) {
      for (const auto &event_binding : program->impl_->event_bindings) {
        const auto element =
            detached.elements_by_id.find(event_binding.element_id);
        if (element == detached.elements_by_id.end()) {
          throw std::logic_error(
              "compiled UI event references an unknown element: " +
              event_binding.element_id);
        }
        element->second->set_attribute(
            "data-flexui-on-" +
                std::string(event_kind_name(event_binding.event)),
            event_binding.handler);
      }
    }
    if (options.mode == UiMountMode::Root) {
      detached.root->set_attribute("data-flexui-theme-root");
      switch (box.theme_mode()) {
    case ThemeMode::System:
      detached.root->remove_attribute("data-theme");
      break;
    case ThemeMode::Light:
      detached.root->set_attribute("data-theme", "light");
      break;
    case ThemeMode::Dark:
      detached.root->set_attribute("data-theme", "dark");
      break;
      }
    }

    // Anonymous/local component instances do not require a potentially
    // O(Box-global IDs) index copy for every chart in a dashboard.
    if (options.id_policy != UiIdPolicy::LocalOnly) {
      next_index = box.elements_by_id_;
    }
    std::uint64_t next_generation = box.next_element_generation_;
    for (const auto &[id, element] : detached.elements_by_id) {
      if (options.id_policy == UiIdPolicy::LocalOnly) continue;
      const auto global_id = scoped_id(id, options);
      if (options.id_policy == UiIdPolicy::Scoped) {
        element->set_element_id(global_id);
      }
      if (next_generation == 0) {
        throw std::overflow_error(
            "FlexUI element handle generation exhausted");
      }
      next_index[global_id] = {element, next_generation++};
    }
    // Stage the Child parent's target list before adopting anything into Box.
    // Element::replace_children does all allocation before its pointer swap.
    std::vector<Element *> next_children;
    if (options.mode == UiMountMode::Child) {
      const auto &children = options.parent->children();
      original_children.reserve(children.size());
      next_children.reserve(children.size() + 1);
      for (auto *child : children) {
        auto *entry = static_cast<Element *>(child);
        original_children.push_back(entry);
        next_children.push_back(entry);
      }
      next_children.push_back(detached.root);
    }
    box.elements_.reserve(box.elements_.size() + detached.elements.size());
    box.widgets_.reserve(box.widgets_.size() + detached.widgets.size());
    box.active_widgets_.reserve(box.active_widgets_.size() +
                                detached.widgets.size());

    if (bindings) {
      binding_transaction.emplace(box.bindings_.begin_transaction());
      for (std::size_t binding_index = 0;
           binding_index < bindings->size(); ++binding_index) {
        const auto &binding = (*bindings)[binding_index];
        active_binding_span = binding.source_span;
        auto element = detached.elements_by_id.find(binding.element_id);
        if (element == detached.elements_by_id.end()) {
          throw std::invalid_argument("UI binding references an unknown element: " +
                                      binding.element_id);
        }
        switch (binding.target) {
        case UiBindingTargetKind::Text:
          box.bindings_.targets().bind_text(*element->second,
                                            binding.expression);
          break;
        case UiBindingTargetKind::Value:
          throw std::invalid_argument(
              "compiled UI documents do not support widget value bindings");
        case UiBindingTargetKind::Classes:
          box.bindings_.targets().bind_classes(*element->second,
                                               binding.expression);
          break;
        case UiBindingTargetKind::Utilities:
          box.bindings_.targets().bind_utilities(*element->second,
                                                 binding.expression);
          break;
        case UiBindingTargetKind::ClassToggle:
          if (binding_index >= program->impl_->binding_programs.size()) {
            throw std::logic_error("compiled UI binding program table is incomplete");
          }
          box.bindings_.bind_compiled_class(
              *element->second, binding.target_name, binding.dependencies,
              program->impl_->binding_programs[binding_index]);
          break;
        }
      }
      active_binding_span.reset();
    }

    for (auto &element : detached.elements) {
      element->owner_box_ = &box;
      box.elements_.push_back(std::move(element));
    }
    for (auto &detached_widget : detached.widgets) {
      Widget *widget = detached_widget.widget.get();
      box.widgets_.push_back(std::move(detached_widget.widget));
      widget->bind_host_element(detached_widget.host);
      box.active_widgets_.push_back(detached_widget.host);
    }
    if (options.id_policy != UiIdPolicy::LocalOnly) {
      box.elements_by_id_.swap(next_index);
      box.next_element_generation_ = next_generation;
      index_committed = true;
    }
    if (options.mode == UiMountMode::Root) {
      box.set_root(detached.root);
    } else if (options.mode == UiMountMode::Child) {
      options.parent->replace_children(next_children);
      child_attached = true;
    }
    if (binding_transaction) {
      box.bindings_.commit_transaction(*binding_transaction);
    }
  } catch (const std::exception &exception) {
    if (child_attached) {
      try { options.parent->replace_children(original_children); }
      catch (...) { /* commit path must be no-throw after child publication */ }
    }
    if (binding_transaction) {
      box.bindings_.rollback_transaction(*binding_transaction);
    }
    if (options.mode == UiMountMode::Root) box.root_ = nullptr;
    box.next_element_generation_ = original_element_generation;
    box.active_widgets_.resize(original_active_widget_count);
    box.widgets_.resize(original_widget_count);
    if (index_committed) {
      // swap is nonallocating for std::map with its default allocator; the
      // previous complete index (including handle generations) is restored.
      box.elements_by_id_.swap(next_index);
    }
    box.elements_.resize(original_element_count);
    const auto code = active_binding_span
                          ? UiDocumentErrorCode::BindingInstallFailed
                          : UiDocumentErrorCode::BuildFailed;
    const char *operation = active_binding_span ? "install UI binding" :
                                                  "build UI document";
    result.error = make_error(
        code, std::string("failed to ") + operation + ": " + exception.what(),
        active_binding_span ? active_binding_span->line : 0,
        active_binding_span ? active_binding_span->column : 0);
    return result;
  }

  result.tree.root = detached.root;
  result.tree.elements_by_id = std::move(detached.elements_by_id);
  result.tree.slots_by_name = std::move(detached.slots_by_name);
  return result;
}

UiDocumentInstantiateResult
UiDocumentInstantiator::instantiate(Box &box, const CompiledUiProgram &program) {
  return instantiate_impl(box, program.definition(), &program, nullptr, {});
}

UiDocumentInstantiateResult UiDocumentInstantiator::instantiate(
    Box &box, const UiDocumentDefinition &definition,
    const WidgetRegistry &registry) {
  return instantiate_impl(box, definition, nullptr, &registry, {});
}

UiDocumentInstantiateResult UiDocumentInstantiator::instantiate(
    Box &box, const CompiledUiProgram &program,
    const WidgetRegistry &registry) {
  return instantiate_impl(box, program.definition(), &program, &registry, {});
}

UiDocumentInstantiateResult UiDocumentInstantiator::instantiate(
    Box &box, const CompiledUiProgram &program, const WidgetRegistry &registry,
    const UiMountOptions &options) {
  return instantiate_impl(box, program.definition(), &program, &registry, options);
}

UiDocumentInstantiateResult UiDocumentInstantiator::instantiate(
    Box &box, const UiDocumentDefinition &definition,
    const WidgetRegistry &registry, const UiMountOptions &options) {
  return instantiate_impl(box, definition, nullptr, &registry, options);
}

} // namespace flexUI
