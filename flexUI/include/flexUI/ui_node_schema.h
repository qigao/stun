#pragma once

#include <flexUI/ui_document.h>

#include <cmath>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace flexUI {

/// Kinds currently instantiated by WidgetRegistry. Components and standalone
/// text nodes remain separate #16 work rather than placeholder factory kinds.
enum class UiNodeKind { Container, Widget };

enum class UiContentModel { Empty, Text, SingleChild, Children, TextAndChildren };

// Typed, compile-time native XML properties live in the ONE WidgetRegistry
// descriptor. This does not introduce a text template or a new UI runtime.
enum class UiPropertyKind { String, Boolean, Number, Integer };

struct UiPropertySpec {
  std::string name;
  UiPropertyKind kind = UiPropertyKind::String;
  bool required = false;
};

/// Registry-owned content metadata. This checks one node's declared content,
/// not tag lookup, arbitrary widget properties, or descendant schemas (#16).
/// A const descriptor returned by WidgetRegistry must not be modified.
struct UiNodeDescriptor {
  UiNodeKind kind = UiNodeKind::Container;
  UiContentModel content = UiContentModel::Children;
  // Empty retains the historical open-property behavior. Explicit schemas
  // can opt into closed native Props without changing shared XML parsing.
  std::vector<UiPropertySpec> properties;
  bool reject_unknown_properties = false;

  const UiPropertySpec *property(std::string_view name) const noexcept {
    for (const auto &spec : properties) {
      if (std::string_view(spec.name) == name) return &spec;
    }
    return nullptr;
  }

  bool valid() const noexcept {
    if (kind != UiNodeKind::Container && kind != UiNodeKind::Widget) {
      return false;
    }
    switch (content) {
    case UiContentModel::Empty:
    case UiContentModel::Text:
    case UiContentModel::SingleChild:
    case UiContentModel::Children:
    case UiContentModel::TextAndChildren:
      break;
    default:
      return false;
    }
    for (std::size_t i = 0; i < properties.size(); ++i) {
      const auto &spec = properties[i];
      if (spec.name.empty() || spec.name == "id" || spec.name == "attr.id") return false;
      switch (spec.kind) {
      case UiPropertyKind::String:
      case UiPropertyKind::Boolean:
      case UiPropertyKind::Number:
      case UiPropertyKind::Integer:
        break;
      default:
        return false;
      }
      for (std::size_t j = 0; j < i; ++j) {
        if (properties[j].name == spec.name) return false;
      }
    }
    return true;
  }

  // No Box/Widget factory is called during this check. An explicit closed
  // Props schema rejects unknown attributes rather than silently coercing
  // them into native widget state. Shared properties must be declared by an
  // author opting into a closed schema.
  UiDocumentError validate_properties(const UiNodeDefinition &node) const {
    const auto fail = [&node](std::string message, const std::string *name = nullptr) {
      SourceSpan source = node.source;
      if (name) {
        const auto at = node.property_spans.find(*name);
        if (at != node.property_spans.end()) source = at->second;
      }
      return UiDocumentError{UiDocumentErrorCode::InvalidProperty,
                             std::move(message), source.line, source.column};
    };
    for (const auto &spec : properties) {
      const auto found = node.properties.find(spec.name);
      if (found == node.properties.end()) {
        if (spec.required)
          return fail("UI tag '" + node.tag + "' requires property '" + spec.name + "'");
        continue;
      }
      bool correct = false;
      switch (spec.kind) {
      case UiPropertyKind::String:
        correct = std::holds_alternative<std::string>(found->second);
        break;
      case UiPropertyKind::Boolean:
        correct = std::holds_alternative<bool>(found->second);
        break;
      case UiPropertyKind::Number:
        if (const auto *value = std::get_if<float>(&found->second))
          correct = std::isfinite(*value);
        break;
      case UiPropertyKind::Integer:
        if (const auto *value = std::get_if<float>(&found->second)) {
          const double numeric = static_cast<double>(*value);
          correct = std::isfinite(numeric) && std::floor(numeric) == numeric &&
                    numeric >= static_cast<double>(std::numeric_limits<int>::min()) &&
                    numeric <= static_cast<double>(std::numeric_limits<int>::max());
        }
        break;
      }
      if (!correct)
        return fail("UI tag '" + node.tag + "' property '" + spec.name +
                    "' has the wrong declared type", &spec.name);
    }
    if (reject_unknown_properties) {
      for (const auto &entry : node.properties) {
        if (!property(entry.first))
          return fail("UI tag '" + node.tag + "' has unknown property '" +
                      entry.first + "'", &entry.first);
      }
    }
    return {};
  }

  /// Rejects disallowed children/text before any widget factory runs.
  /// Empty text declarations still count as text; bindings to text cannot
  /// bypass the content model. Attribute locations take precedence when known.
  /// This operation never mutates the definition or invokes application code.
  UiDocumentError validate_content(const UiNodeDefinition &node) const {
    const auto fail = [&node](UiDocumentErrorCode code, std::string message,
                              const char *property = nullptr) {
      SourceSpan location = node.source;
      if (property) {
        const auto found = node.property_spans.find(property);
        if (found != node.property_spans.end()) {
          location = found->second;
        }
      }
      return UiDocumentError{code, std::move(message), location.line, location.column};
    };
    if (!valid()) {
      return fail(UiDocumentErrorCode::InvalidNode, "invalid UI node descriptor");
    }
    if ((content == UiContentModel::Empty || content == UiContentModel::Text) &&
        !node.children.empty()) {
      return fail(UiDocumentErrorCode::InvalidNode,
                  "UI tag '" + node.tag + "' does not accept child elements");
    }
    if (content == UiContentModel::SingleChild && node.children.size() != 1) {
      return fail(UiDocumentErrorCode::InvalidNode,
                  "UI tag '" + node.tag + "' requires exactly one child element");
    }
    const bool accepts_text = content == UiContentModel::Text ||
                              content == UiContentModel::TextAndChildren;
    for (const char *property : {"text", "content", "bind.text"}) {
      const auto found = node.properties.find(property);
      if (found == node.properties.end()) {
        continue;
      }
      if (!accepts_text) {
        return fail(UiDocumentErrorCode::InvalidProperty,
                    "UI tag '" + node.tag + "' does not accept '" + property + "'",
                    property);
      }
      if (!std::holds_alternative<std::string>(found->second)) {
        return fail(UiDocumentErrorCode::InvalidProperty,
                    "UI text property '" + std::string(property) + "' requires a string",
                    property);
      }
    }
    return {};
  }
};

} // namespace flexUI
