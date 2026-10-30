#pragma once

#include "types.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace flexUI {

class Box;
class Element;
class WidgetRegistry;

using UiDocumentValue = std::variant<float, std::string, bool>;

struct SourceSpan {
  int line = 0;
  int column = 0;
  std::size_t length = 0;
};

/// Prevalidated native layout/paint snapshot used by semantic components
/// whose dynamic geometry is produced by a domain layout engine (e.g. DOT).
/// It is staged on detached Elements BEFORE Box publication. XML-authored
/// classes, variables and normal CSS cascade remain available afterwards.
/// Never store Element pointers or duplicate a runtime UI ownership tree.
struct UiNodePresentation {
  bool has_bounds = false;
  bool absolute = false;
  bool relative = false;
  bool clipped = false;
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;
  std::optional<Color> background_color;
  std::optional<Color> border_color;
  std::optional<Color> text_color;
  std::optional<float> border_width;
  std::optional<BorderStyle> border_style;
  std::optional<float> border_radius;
  std::optional<float> opacity;
  std::optional<float> font_size;
  std::optional<std::string> font_family;
  std::optional<TextAlign> text_align;
  std::optional<int> z_index;
};

/// Parser-independent description of one UI element and its ordered children.
struct UiNodeDefinition {
  std::string tag;
  std::string id;
  std::unordered_map<std::string, UiDocumentValue> properties;
  std::unordered_map<std::string, SourceSpan> property_spans;
  std::vector<UiNodeDefinition> children;
  /// Owning tag location; appended to preserve existing aggregate field order.
  SourceSpan source;
  /// Domain-native prepublication style/geometry; absent for XML-only nodes.
  std::optional<UiNodePresentation> presentation;
  /// Internal lookup key used by registered native widget factories.
  /// Never becomes an Element ID, class, attribute or slot.
  std::string native_widget_key;
};

struct UiDocumentDefinition {
  std::string name;
  UiNodeDefinition root;
};

/// Parser-independent description of one resource declared by an `assets` block.
struct UiResourceDefinition {
  std::string type;
  std::string id;
  std::string path;
  std::unordered_map<std::string, UiDocumentValue> options;
};

struct UiDocumentLimits {
  std::size_t max_source_bytes = 4U * 1024U * 1024U;
  std::size_t max_nodes = 10000;
  std::size_t max_depth = 128;
  std::size_t max_properties_per_node = 128;
  std::size_t max_string_bytes = 64U * 1024U;
  std::size_t max_event_bindings = 10000;
  std::size_t max_bindings = 10000;
  /// Applies when producing a CompiledUiProgram; legacy parse results do not
  /// own resources.
  std::size_t max_resources = 4096;
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
  BuildFailed = 18, // Preserve the pre-CompiledUiProgram public value.
  UnknownEvent,
  EventBindingLimitExceeded,
  UnknownBindingTarget,
  InvalidBindingExpression,
  BindingLimitExceeded,
  BindingInstallFailed,
  DuplicateEventBinding,
  DuplicateBindingTarget,
  BindingTargetConflict,
  ResourceLimitExceeded,
  UnknownElementTag,
  WidgetFactoryFailed,
  InvalidUtf8,
  RequiredElementId,
  InvalidMountTarget,
  InvalidIdScope,
  DuplicateSlot,
  UnsupportedFragmentBindings,
  MissingSlot,
  InvalidComponentScope,
  MissingComponentHost,
  UnsupportedComponentResources,
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

/// Immutable result of structural parsing and event/binding lowering.
/// Registry content schemas are checked separately before typed instantiation.
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
  /// Finds one immutable handler definition without consulting Element attributes.
  ///
  /// The returned pointer remains valid while this compiled program is alive.
  /// Returns nullptr when the element/event pair has no declared handler.
  const EventBinding *find_event_binding(
      std::string_view element_id, UiEventKind event) const noexcept;
  const std::vector<BindingDefinition> &bindings() const noexcept;
  const std::vector<UiResourceDefinition> &resources() const noexcept;

private:
  struct Impl;
  explicit CompiledUiProgram(std::unique_ptr<Impl> impl);

  std::unique_ptr<Impl> impl_;

  friend class UiDocumentInstantiator;
  friend UiDocumentCompileResult compile_ui_definition(
      UiDocumentDefinition definition,
      std::vector<UiResourceDefinition> resources,
      const UiDocumentLimits &limits);
  friend UiDocumentCompileResult compile_ui_document(std::string_view source,
                                                      std::string_view document_name,
                                                      const UiDocumentLimits &limits);
};

/// Compiler-produced identity of one node within an assembled scoped
/// component. parent_index refers to an earlier node in the SAME pre-order
/// ledger; the root uses kNoParent and its child_index is in the base host.
/// Every node is represented, INCLUDING anonymous and Widget nodes without
/// public Element IDs; no raw pointer or mutable runtime state is persisted.
struct UiScopedOwnedNode {
  static constexpr std::size_t kNoParent = static_cast<std::size_t>(-1);
  std::size_t parent_index = kNoParent;
  std::size_t child_index = 0;
  std::string qualified_id; // May be empty for an anonymous Element/Widget.
  std::string tag;
  bool widget = false;
  bool has_compiled_events = false;
  bool has_compiled_bindings = false;
};

/// One immutable compile-candidate provenance ledger. This is not an
/// Element/Widget resource lease or approval to physically unmount a scope.
/// The application may MOVE it into the one published generation owner.
struct UiScopedComponentOwnership {
  std::string scope;
  std::string host_id;
  std::size_t root_child_index = 0;
  std::vector<UiScopedOwnedNode> nodes;
};

struct UiDocumentCompileResult {
  std::shared_ptr<const CompiledUiProgram> program;
  UiDocumentError error;
  /// Populated ONLY by compile_scoped_ui_components after the merged program
  /// has passed complete registry/semantic acceptance. Non-scoped callers and
  /// all failed compositions return an empty ledger. Not a second UI runtime.
  std::vector<UiScopedComponentOwnership> scoped_ownership;

  explicit operator bool() const { return program != nullptr && !static_cast<bool>(error); }
};

/// Root installation, unattached Box-owned fragment or atomic child insertion.
enum class UiMountMode { Root, DetachedFragment, Child };

/// Public element IDs may be globally indexed, local-only, or prefixed by an
/// explicit, nonempty scope. Slot names remain local in all three policies.
enum class UiIdPolicy { Global, LocalOnly, Scoped };

struct UiMountOptions {
  UiMountMode mode = UiMountMode::Root;
  UiIdPolicy id_policy = UiIdPolicy::Global;
  Element *parent = nullptr; // Required only for Child; must belong to Box.
  std::string id_scope;       // Required only for Scoped.
  std::vector<std::string> required_slots; // Checked before any factory or Box mutation.
};

struct UiDocumentTree {
  Element *root = nullptr;
  std::unordered_map<std::string, Element *> elements_by_id;
  /// Instance-local slot lookup; slot is never a global Element ID.
  std::unordered_map<std::string, Element *> slots_by_name;
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

/// Semantically lowers a parser-independent definition into an immutable program.
///
/// The definition and resources are accepted by value so the resulting program
/// cannot be changed through aliases retained by the caller. This is the shared
/// semantic boundary for source-format adapters such as XML and the temporary
/// legacy Flex DSL frontend.
UiDocumentCompileResult compile_ui_definition(
    UiDocumentDefinition definition,
    std::vector<UiResourceDefinition> resources = {},
    const UiDocumentLimits &limits = {});

/// One statically registered XML component compiled into an application
/// before mount. The local component Element IDs are qualified as
/// `scope::local_id`. Component events and bindings use the SAME immutable
/// app program, ScriptController and UI/Command effect engines. This is
/// pre-mount composition; dynamic fragment handler registration is NOT opened.
/// Compile-time source redirection into the one application UiDataContext.
/// The host must declare the typed application input; no second data owner.
struct UiScopedInputAlias {
  std::string local_name;
  std::string application_name;
};

/// One compile-time caller fill for a named <slot name="..."/> outlet inside
/// a precompiled component source. This is a single-root, static M2 slice:
/// children keep caller-local data/event references, and no Slot Widget or
/// runtime template engine exists. Multiple-root/default/optional slots and
/// imported XML modules remain separately gated in #16.
struct UiScopedSlotFill {
  std::string name;
  std::shared_ptr<const CompiledUiProgram> program;
};

struct UiScopedComponent {
  std::string scope;
  /// ID of an existing structural host in the application's BASE document.
  /// Components cannot target a widget host or another component.
  std::string host_id;
  std::shared_ptr<const CompiledUiProgram> program;
  /// Optional static aliases; existing three-field aggregate callers keep
  /// shared global input source semantics without an extra owner.
  std::vector<UiScopedInputAlias> input_aliases = {};
  /// Optional named compile-time slot fills. The template must declare each
  /// outlet exactly once. Every outlet requires one caller-provided program.
  /// IDs and binding/event inputs in fills retain caller lexical meaning;
  /// component-private nodes continue using scope::id and input aliases.
  std::vector<UiScopedSlotFill> slot_fills = {};
};

/// Build a single immutable UI program from an XML application and statically
/// declared component instances. All IDs/event handlers and binding targets
/// are qualified before the normal compile, registry preflight or Box mount.
/// Failed composition cannot mutate an application or its Box.
UiDocumentCompileResult compile_scoped_ui_components(
    const CompiledUiProgram &application,
    const std::vector<UiScopedComponent> &components,
    const WidgetRegistry &registry, const UiDocumentLimits &limits = {});

/// Builds a validated definition as a detached tree and commits ownership to `box`.
///
/// `box` must not already have a root. On success, `box` owns every returned Element pointer.
/// On failure, `error` describes the rejected definition or target and `box` remains unchanged.
class UiDocumentInstantiator {
public:
  /// Structural-only overload creates generic Elements. Registry schema
  /// acceptance requires the overload taking a WidgetRegistry (#16/#33).
  /// @param box Target ownership and ID-index boundary.
  /// @param definition Parsed or manually constructed UI definition.
  /// @return The installed root/index view, or a structured validation/build error.
  static UiDocumentInstantiateResult instantiate(Box &box, const UiDocumentDefinition &definition);

  /// Instantiates the validated definition owned by an immutable compiled program.
  static UiDocumentInstantiateResult instantiate(Box &box, const CompiledUiProgram &program);

  /// Instantiates a definition using an explicit strict tag registry.
  static UiDocumentInstantiateResult instantiate(
      Box &box, const UiDocumentDefinition &definition,
      const WidgetRegistry &registry);

  /// Instantiates a compiled program using an explicit strict tag registry.
  static UiDocumentInstantiateResult instantiate(
      Box &box, const CompiledUiProgram &program,
      const WidgetRegistry &registry);

  /// Instantiates a registered UI component into an existing Box without a
  /// second runtime tree. DetachedFragment publishes ownership but not a view
  /// parent; Child appends atomically after preflight and staging.
  ///
  /// Fragment mode currently rejects compiled event/binding exports: scoped
  /// local-ID lookup must not masquerade as a global controller binding.
  /// All errors before publication preserve the previous Box state.
  static UiDocumentInstantiateResult instantiate(
      Box &box, const CompiledUiProgram &program,
      const WidgetRegistry &registry, const UiMountOptions &options);

  /// For chart/compiler prepared definitions without controller exports.
  static UiDocumentInstantiateResult instantiate(
      Box &box, const UiDocumentDefinition &definition,
      const WidgetRegistry &registry, const UiMountOptions &options);

private:
  static UiDocumentInstantiateResult instantiate_impl(
      Box &box, const UiDocumentDefinition &definition,
      const CompiledUiProgram *program, const WidgetRegistry *registry,
      const UiMountOptions &options);
};

} // namespace flexUI
