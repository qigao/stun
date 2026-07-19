#include <flexUI/ui_document.h>

#include <flexUI/box.h>
#include <flexUI/element.h>

#include <flex/dsl/flex_parser.h>

#include <cmath>
#include <iomanip>
#include <limits>
#include <memory>
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
         name == "text" || name == "content" || name.rfind("on.", 0) == 0;
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
  target.children.reserve(source.children.size());
  for (const auto &child : source.children) {
    target.children.push_back(copy_node(*child));
  }
  return target;
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

UiDocumentInstantiateResult
UiDocumentInstantiator::instantiate(Box &box, const UiDocumentDefinition &definition) {
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

    for (auto &element : detached.elements) {
      element->owner_box_ = &box;
      box.elements_.push_back(std::move(element));
    }
    box.elements_by_id_.swap(next_index);
    box.set_root(detached.root);
  } catch (const std::exception &exception) {
    result.error = make_error(UiDocumentErrorCode::BuildFailed,
                              std::string("failed to build UI document: ") + exception.what());
    return result;
  }

  result.tree.root = detached.root;
  result.tree.elements_by_id = std::move(detached.elements_by_id);
  return result;
}

} // namespace flexUI
