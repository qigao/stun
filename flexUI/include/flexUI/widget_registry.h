#pragma once

#include <flexUI/ui_node_schema.h>
#include <flexUI/widget.h>

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

namespace flexUI {

enum class WidgetRegistryErrorCode {
  None,
  InvalidTag,
  DuplicateTag,
  InvalidFactory,
  UnknownTag,
  FactoryFailed,
  InvalidDescriptor,
  InvalidContent,
};

struct WidgetRegistryError {
  WidgetRegistryErrorCode code = WidgetRegistryErrorCode::None;
  std::string message;

  explicit operator bool() const noexcept { return code != WidgetRegistryErrorCode::None; }
};

struct WidgetCreationResult {
  std::unique_ptr<Widget> widget;
  WidgetRegistryError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

/// Explicit tag-to-factory registry used by typed UI instantiation.
///
/// Registration and instantiation belong to the caller's UI owner thread.
/// An element tag is a recognized structural element with no Widget object;
/// a widget tag owns one factory. Duplicate registration never replaces the
/// existing entry.
class WidgetRegistry final {
public:
  using Factory = std::function<std::unique_ptr<Widget>(const UiNodeDefinition &)>;

  /// Registers a structural tag that creates an Element without a Widget.
  /// @param tag Non-empty canonical XML tag.
  /// @return None on success; InvalidTag or DuplicateTag without changing an
  ///         existing registration.
  WidgetRegistryError register_element(
      std::string tag, UiContentModel content = UiContentModel::Children);

  /// Registers one Widget factory for a canonical XML tag.
  /// @param tag Non-empty canonical XML tag.
  /// @param factory Callable used during owner-thread instantiation.
  /// @return None on success; InvalidTag, InvalidFactory or DuplicateTag on
  ///         rejection. Existing registrations are never replaced.
  /// The default preserves the existing custom-widget contract. Built-ins
  /// declare their content model explicitly; no unknown tag is admitted.
  WidgetRegistryError register_widget(
      std::string tag, Factory factory,
      UiContentModel content = UiContentModel::TextAndChildren);

  /// Returns whether a structural or widget tag is registered.
  bool contains(std::string_view tag) const;

  /// Registry-owned immutable metadata, or nullptr for an unknown tag.
  const UiNodeDescriptor *descriptor(std::string_view tag) const;

  /// Whole-tree preflight, without factories or Box mutation. O(nodes) time,
  /// O(depth) auxiliary storage, bounded by the supplied document limits.
  /// This validates registered tags and content, not all property schemas (#16).
  UiDocumentError validate(const UiDocumentDefinition &definition,
                           const UiDocumentLimits &limits = {}) const;
  UiDocumentError validate(const CompiledUiProgram &program,
                           const UiDocumentLimits &limits = {}) const;

  /// Creates one Widget after checking its own node content. Whole-tree
  /// callers must use validate() before invoking any factory.
  /// @return Success with a null widget for structural tags, success with an
  ///         owned Widget for widget tags, or a structured error. Factory
  ///         exceptions are converted to FactoryFailed.
  WidgetCreationResult create(const UiNodeDefinition &definition) const;

  /// Returns a registry containing FlexUI structural tags and linked widgets.
  ///
  /// @code
  /// auto registry = WidgetRegistry::builtins();
  /// auto result = UiDocumentInstantiator::instantiate(box, program, registry);
  /// @endcode
  static WidgetRegistry builtins();

private:
  struct Entry {
    UiNodeDescriptor descriptor;
    Factory factory;
  };

  std::unordered_map<std::string, Entry> entries_;
};

} // namespace flexUI
