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

struct SourceSpan {
  int line = 0;
  int column = 0;
  std::size_t length = 0;
};

/// Parser-independent description of one UI element and its ordered children.
struct UiNodeDefinition {
  std::string tag;
  std::string id;
  std::unordered_map<std::string, UiDocumentValue> properties;
  std::unordered_map<std::string, SourceSpan> property_spans;
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
  std::size_t max_event_bindings = 10000;
  std::size_t max_bindings = 10000;
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
  UnknownEvent,
  EventBindingLimitExceeded,
  UnknownBindingTarget,
  InvalidBindingExpression,
  BindingLimitExceeded,
  BuildFailed,
  BindingInstallFailed,
  DuplicateEventBinding,
  DuplicateBindingTarget,
  BindingTargetConflict,
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

struct UiDocumentCompileResult;

enum class UiEventKind {
  Click,
  MouseMove,
  MouseDown,
  MouseUp,
  MouseWheel,
  KeyDown,
  KeyUp,
  TextInput,
  FocusIn,
  FocusOut,
  CompositionStart,
  CompositionUpdate,
  CompositionEnd,
};

struct EventBinding {
  UiEventKind event = UiEventKind::Click;
  std::string element_id;
  std::string handler;
  SourceSpan source;
};

enum class UiBindingTargetKind {
  Text,
  Value,
  Classes,
  Utilities,
  ClassToggle,
};

enum class UiBindingSourceKind {
  StringInput,
  BoolExpression,
};

struct BindingDefinition {
  UiBindingTargetKind target = UiBindingTargetKind::Text;
  UiBindingSourceKind source = UiBindingSourceKind::StringInput;
  std::string element_id;
  std::string target_name;
  std::string expression;
  std::vector<std::string> dependencies;
  bool uses_jit = false;
  SourceSpan source_span;
};

/// Immutable result of UI parsing and semantic lowering.
///
/// A const program may be shared across threads. Instantiation still belongs to
/// the target Box's owner thread, and the Box remains the sole owner of created
/// Element objects.
class CompiledUiProgram final {
public:
  ~CompiledUiProgram();

  CompiledUiProgram(const CompiledUiProgram &) = delete;
  CompiledUiProgram &operator=(const CompiledUiProgram &) = delete;

  const std::string &name() const noexcept;
  const UiDocumentDefinition &definition() const noexcept;
  const std::vector<EventBinding> &event_bindings() const noexcept;
  const std::vector<BindingDefinition> &bindings() const noexcept;

private:
  struct Impl;
  explicit CompiledUiProgram(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;

  friend class UiDocumentInstantiator;
  friend UiDocumentCompileResult compile_ui_document(std::string_view source,
                                                      std::string_view document_name,
                                                      const UiDocumentLimits &limits);
};

struct UiDocumentCompileResult {
  std::shared_ptr<const CompiledUiProgram> program;
  UiDocumentError error;

  explicit operator bool() const { return program != nullptr && !static_cast<bool>(error); }
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

/// Parses and semantically lowers one named `ui` block into an immutable program.
///
/// The legacy parse result remains available for callers that only need the
/// parser-independent definition.
UiDocumentCompileResult compile_ui_document(std::string_view source,
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

  /// Instantiates the validated definition owned by an immutable compiled program.
  static UiDocumentInstantiateResult instantiate(Box &box, const CompiledUiProgram &program);

private:
  static UiDocumentInstantiateResult instantiate_impl(
      Box &box, const UiDocumentDefinition &definition,
      const CompiledUiProgram *program);
};

} // namespace flexUI
