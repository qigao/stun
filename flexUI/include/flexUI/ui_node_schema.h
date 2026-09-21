#pragma once

#include <flexUI/ui_document.h>

#include <initializer_list>
#include <utility>

namespace flexUI {

/// Kinds currently instantiated by WidgetRegistry. Components and standalone
/// text nodes remain separate #16 work rather than placeholder factory kinds.
enum class UiNodeKind { Container, Widget };

enum class UiContentModel { Empty, Text, SingleChild, Children, TextAndChildren };

/// Registry-owned content metadata. This checks one node's declared content,
/// not tag lookup, arbitrary widget properties, or descendant schemas (#16).
/// A const descriptor returned by WidgetRegistry must not be modified.
struct UiNodeDescriptor {
  UiNodeKind kind = UiNodeKind::Container;
  UiContentModel content = UiContentModel::Children;

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
      return true;
    }
    return false;
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
