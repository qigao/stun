#include <flexUI/ui_document.h>

#include <flexUI/box.h>
#include <flexUI/element.h>

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
         name == "text" || name == "content" || name.rfind("on.", 0) == 0 ||
         name.rfind("bind.", 0) == 0;
}

bool has_alias_conflict(const flex::parser::AstProps &properties, const char *first,
                        const char *second) {
  return properties.count(first) != 0 && properties.count(second) != 0;
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
  if (node.type.empty() || node.id.empty()) {
    return make_error(UiDocumentErrorCode::InvalidNode, "UI nodes require a non-empty tag and id");
  }
  if (node.type.size() > limits.max_string_bytes || node.id.size() > limits.max_string_bytes) {
    return make_error(UiDocumentErrorCode::StringLimitExceeded,
                      "UI node tag or id exceeds the string limit");
  }
  if (!ids.insert(node.id).second) {
    return make_error(UiDocumentErrorCode::DuplicateElementId,
                      "duplicate UI element id: " + node.id);
  }
  if (node.properties.size() > limits.max_properties_per_node) {
    return make_error(UiDocumentErrorCode::PropertyLimitExceeded,
                      "UI node exceeds maximum property count: " + node.id);
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
  if (has_alias_conflict(node.properties, "class", "classes") ||
      has_alias_conflict(node.properties, "utility", "utilities") ||
      has_alias_conflict(node.properties, "text", "content")) {
    return make_error(UiDocumentErrorCode::InvalidProperty,
                      "UI node contains conflicting property aliases: " + node.id);
  }

  for (const auto &[name, value] : node.properties) {
    if (name.empty() || name == "id") {
      return make_error(UiDocumentErrorCode::InvalidProperty,
                        "invalid or reserved UI property on node: " + node.id);
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
    if (name.rfind("attr.", 0) == 0 && name.size() == 5) {
      return make_error(UiDocumentErrorCode::InvalidProperty, "attr. requires an attribute name");
    }
    if (name.rfind("on.", 0) == 0 && name.size() == 3) {
      return make_error(UiDocumentErrorCode::InvalidProperty, "on. requires an event name");
    }
  }

  for (const auto &child : node.children) {
    if (!child) {
      return make_error(UiDocumentErrorCode::InvalidNode, "UI document contains a null child");
    }
    auto error = validate_node(*child, limits, depth + 1, node_count, ids);
    if (error) {
      return error;
    }
  }
  return {};
}

UiDocumentError validate_definition_node(const UiNodeDefinition &node,
                                         const UiDocumentLimits &limits, std::size_t depth,
                                         std::size_t &node_count,
                                         std::unordered_set<std::string> &ids) {
  AstNode facade(node.tag, node.id);
  facade.properties = node.properties;
  auto error = validate_node(facade, limits, depth, node_count, ids);
  if (error) {
    return error;
  }
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

bool resolve_event_kind(std::string_view name, UiEventKind &kind) {
  static constexpr std::pair<std::string_view, UiEventKind> events[] = {
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
  const auto found = std::find_if(
      std::begin(events), std::end(events),
      [name](const auto &entry) { return entry.first == name; });
  if (found == std::end(events)) {
    return false;
  }
  kind = found->second;
  return true;
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

UiDocumentError validate_target(Box &box, const UiDocumentDefinition &definition) {
  if (box.root()) {
    return make_error(UiDocumentErrorCode::BoxAlreadyHasRoot,
                      "UI document requires a Box without an installed root");
  }

  std::vector<const UiNodeDefinition *> pending{&definition.root};
  while (!pending.empty()) {
    const UiNodeDefinition *node = pending.back();
    pending.pop_back();
    if (box.get_by_id(node->id)) {
      return make_error(UiDocumentErrorCode::ElementIdConflict,
                        "Box already contains element id: " + node->id);
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
    for (const auto &child : node->children) {
      pending.push_back(&child);
    }
  }
  return {};
}

struct DetachedTree {
  Element *root = nullptr;
  std::vector<std::unique_ptr<Element>> elements;
  std::unordered_map<std::string, Element *> elements_by_id;
};

void apply_properties(Element &element, const UiNodeDefinition &definition) {
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
  if (const auto *focusable = find_property("focusable")) {
    element.focusable = std::get<bool>(*focusable);
  }
  if (const auto *tab_index = find_property("tab_index")) {
    element.tab_index = static_cast<int>(std::get<float>(*tab_index));
  }

  for (const auto &[name, value] : definition.properties) {
    if (name == "class" || name == "classes" || name == "utility" || name == "utilities" ||
        name == "text" || name == "content" || name == "focusable" || name == "tab_index") {
      continue;
    }
    if (name.rfind("attr.", 0) == 0) {
      element.set_attribute(name.substr(5), value_to_string(value));
    } else if (name.rfind("on.", 0) == 0) {
      element.set_attribute("data-flexui-on-" + name.substr(3), std::get<std::string>(value));
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
}

Element *build_node(const UiNodeDefinition &definition, DetachedTree &tree) {
  auto element = std::make_unique<Element>();
  element->set_tag(definition.tag);
  element->set_element_id(definition.id);
  apply_properties(*element, definition);

  Element *raw = element.get();
  tree.elements_by_id.emplace(definition.id, raw);
  tree.elements.push_back(std::move(element));

  for (const auto &child_definition : definition.children) {
    Element *child = build_node(child_definition, tree);
    if (!raw->append(child)) {
      throw std::logic_error("failed to attach detached UI element");
    }
  }
  return raw;
}

} // namespace

UiDocumentParseResult parse_ui_document(std::string_view source, std::string_view document_name,
                                        const UiDocumentLimits &limits) {
  UiDocumentParseResult result;
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

  auto definition = std::make_shared<UiDocumentDefinition>();
  definition->name = selected->name;
  definition->root = copy_node(*selected->children.front());
  result.definition = std::move(definition);
  return result;
}

struct CompiledUiProgram::Impl {
  std::shared_ptr<const UiDocumentDefinition> definition;
  std::vector<EventBinding> event_bindings;
  std::vector<BindingDefinition> bindings;
  std::vector<std::shared_ptr<const flex::MirExpressionProgram>> binding_programs;
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

const std::vector<BindingDefinition> &
CompiledUiProgram::bindings() const noexcept {
  return impl_->bindings;
}

UiDocumentCompileResult compile_ui_document(std::string_view source,
                                            std::string_view document_name,
                                            const UiDocumentLimits &limits) {
  UiDocumentCompileResult result;
  auto parsed = parse_ui_document(source, document_name, limits);
  if (!parsed) {
    result.error = std::move(parsed.error);
    return result;
  }

  auto impl = std::make_unique<CompiledUiProgram::Impl>();
  impl->definition = std::move(parsed.definition);
  result.error = lower_event_bindings(impl->definition->root, limits,
                                      impl->event_bindings);
  if (result.error) {
    return result;
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

UiDocumentInstantiateResult
UiDocumentInstantiator::instantiate(Box &box, const UiDocumentDefinition &definition) {
  return instantiate_impl(box, definition, nullptr);
}

UiDocumentInstantiateResult UiDocumentInstantiator::instantiate_impl(
    Box &box, const UiDocumentDefinition &definition,
    const CompiledUiProgram *program) {
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

  result.error = validate_target(box, definition);
  if (result.error) {
    return result;
  }

  DetachedTree detached;
  std::optional<UiBindingRuntime::TransactionCheckpoint> binding_transaction;
  std::optional<SourceSpan> active_binding_span;
  const std::size_t original_element_count = box.elements_.size();
  bool index_committed = false;
  const auto *bindings = program ? &program->impl_->bindings : nullptr;
  try {
    detached.root = build_node(definition.root, detached);
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

    auto next_index = box.elements_by_id_;
    for (const auto &[id, element] : detached.elements_by_id) {
      next_index[id] = element;
    }
    box.elements_.reserve(box.elements_.size() + detached.elements.size());

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
    box.elements_by_id_.swap(next_index);
    index_committed = true;
    box.set_root(detached.root);
    if (binding_transaction) {
      box.bindings_.commit_transaction(*binding_transaction);
    }
  } catch (const std::exception &exception) {
    if (binding_transaction) {
      box.bindings_.rollback_transaction(*binding_transaction);
    }
    box.root_ = nullptr;
    if (index_committed) {
      for (const auto &[id, element] : detached.elements_by_id) {
        auto found = box.elements_by_id_.find(id);
        if (found != box.elements_by_id_.end() && found->second == element) {
          box.elements_by_id_.erase(found);
        }
      }
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
  return result;
}

UiDocumentInstantiateResult
UiDocumentInstantiator::instantiate(Box &box, const CompiledUiProgram &program) {
  return instantiate_impl(box, program.definition(), &program);
}

} // namespace flexUI
