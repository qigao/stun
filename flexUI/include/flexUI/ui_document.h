#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace flexUI {

class Box;
class Element;

using UiDocumentValue = std::variant<float, std::string, bool>;

/// Parser-independent description of one UI element and its ordered children.
struct UiNodeDefinition {
  std::string tag;
  std::string id;
  std::unordered_map<std::string, UiDocumentValue> properties;
  std::vector<UiNodeDefinition> children;
};

struct UiDocumentDefinition {
  std::string name;
  UiNodeDefinition root;
};

struct UiDocumentLimits {
  std::size_t max_source_bytes = 4U * 1024U * 1024U;
  std::size_t max_nodes = 10000;
  std::size_t max_depth = 128;
  std::size_t max_properties_per_node = 128;
  std::size_t max_string_bytes = 64U * 1024U;
};

enum class UiDocumentErrorCode {
  None,
  SourceTooLarge,
  ParseError,
  MissingDocument,
  AmbiguousDocument,
  DuplicateDocumentName,
  InvalidRootCount,
  NodeLimitExceeded,
  DepthLimitExceeded,
  PropertyLimitExceeded,
  StringLimitExceeded,
  InvalidNode,
  DuplicateElementId,
  InvalidProperty,
  BoxAlreadyHasRoot,
  ElementIdConflict,
  UtilityJitDisabled,
  UnknownUtility,
  BuildFailed,
};

struct UiDocumentError {
  UiDocumentErrorCode code = UiDocumentErrorCode::None;
  std::string message;
  int line = 0;
  int column = 0;

  explicit operator bool() const { return code != UiDocumentErrorCode::None; }
};

struct UiDocumentParseResult {
  std::shared_ptr<const UiDocumentDefinition> definition;
  UiDocumentError error;

  explicit operator bool() const { return definition != nullptr && !static_cast<bool>(error); }
};

struct UiDocumentTree {
  Element *root = nullptr;
  std::unordered_map<std::string, Element *> elements_by_id;
};

struct UiDocumentInstantiateResult {
  UiDocumentTree tree;
  UiDocumentError error;

  explicit operator bool() const { return tree.root != nullptr && !static_cast<bool>(error); }
};

/// Parses and semantically validates one named `ui` block.
///
/// @param source Flex DSL source. Properties inside UI nodes must be comma-separated.
/// @param document_name Document to select; may be empty only when source has exactly one UI block.
/// @param limits Resource limits applied before and during semantic validation.
/// @return A const definition on success, otherwise a structured error with parser location when
/// available.
UiDocumentParseResult parse_ui_document(std::string_view source,
                                        std::string_view document_name = {},
                                        const UiDocumentLimits &limits = {});

/// Builds a validated definition as a detached tree and commits ownership to `box`.
///
/// `box` must not already have a root. On success, `box` owns every returned Element pointer.
/// On failure, `error` describes the rejected definition or target and `box` remains unchanged.
class UiDocumentInstantiator {
public:
  /// @param box Target ownership and ID-index boundary.
  /// @param definition Parsed or manually constructed UI definition.
  /// @return The installed root/index view, or a structured validation/build error.
  static UiDocumentInstantiateResult instantiate(Box &box, const UiDocumentDefinition &definition);
};

} // namespace flexUI
