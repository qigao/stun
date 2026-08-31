#pragma once

#include "flexUI/ui_document.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

namespace flexUI {

enum class ControllerState {
  Empty,
  Compiled,
  Mounted,
  Dispatching,
  Faulted,
  Unmounting,
};

struct ScriptExportHandle {
  std::uint64_t value = 0;

  explicit operator bool() const noexcept { return value != 0; }
};

enum class ScriptModuleErrorCode {
  None,
  InvalidExport,
  CompileFailure,
  RuntimeFailure,
  Timeout,
  Interrupted,
  ResourceLimitExceeded,
};

struct ScriptModuleError {
  ScriptModuleErrorCode code = ScriptModuleErrorCode::None;
  std::string message;

  explicit operator bool() const noexcept {
    return code != ScriptModuleErrorCode::None;
  }
};

struct ScriptResolveResult {
  std::optional<ScriptExportHandle> handle;
  ScriptModuleError error;

  explicit operator bool() const noexcept {
    return !static_cast<bool>(error);
  }
};

struct UiHandle {
  std::string id;
  std::uint64_t generation = 0;

  explicit operator bool() const noexcept {
    return !id.empty() && generation != 0;
  }
};

/// Pointer-free, value-semantic event passed across the script boundary.
struct ScriptEventSnapshot {
  UiEventKind event = UiEventKind::Click;
  UiHandle target;
  double x = 0.0;
  double y = 0.0;
  double delta_x = 0.0;
  double delta_y = 0.0;
  std::int32_t button = 0;
  std::int32_t key = 0;
  std::int32_t modifiers = 0;
  std::string text;
  std::string composition_text;
  double timestamp_ms = 0.0;
  bool handled = false;
  bool propagate = true;
};

enum class ScriptCallbackKind {
  Mount,
  Event,
  Frame,
  Unmount,
};

struct ScriptCallContext {
  ScriptCallbackKind callback = ScriptCallbackKind::Event;
  /// Borrowed only for the duration of IScriptModule::call().
  const ScriptEventSnapshot *event = nullptr;
  double delta_seconds = 0.0;
};

struct ScriptCallResult {
  ScriptModuleError error;

  explicit operator bool() const noexcept {
    return !static_cast<bool>(error);
  }
};

/// Script runtime strategy used by ScriptController.
///
/// Implementations own their runtime/module context. Both methods are called
/// only on the controller's owner thread. A missing export is represented by a
/// successful ScriptResolveResult with an empty handle.
class IScriptModule {
public:
  virtual ~IScriptModule() = default;

  virtual ScriptResolveResult resolve_export(std::string_view name) = 0;
  virtual ScriptCallResult call(ScriptExportHandle handle,
                                const ScriptCallContext &context) = 0;
};

struct ControllerLimits {
  std::size_t max_element_id_bytes = 256;
  std::size_t max_event_text_bytes = 4096;
  std::size_t max_composition_text_bytes = 4096;
};

enum class ControllerErrorCode {
  None,
  InvalidState,
  InvalidArgument,
  WrongThread,
  ModuleResolutionFailed,
  MissingHandlerExport,
  ModuleCallFailed,
  ControllerFaulted,
  EventLimitExceeded,
  InternalInvariant,
};

enum class ControllerStage {
  None,
  Load,
  Mount,
  Event,
  Frame,
  Unmount,
};

struct ControllerError {
  ControllerErrorCode code = ControllerErrorCode::None;
  ControllerStage stage = ControllerStage::None;
  std::string message;
  std::string element_id;
  std::string handler;
  UiEventKind event = UiEventKind::Click;
  SourceSpan source;
  ScriptModuleError module_error;

  explicit operator bool() const noexcept {
    return code != ControllerErrorCode::None;
  }
};

struct ControllerResult {
  ControllerError error;

  explicit operator bool() const noexcept {
    return !static_cast<bool>(error);
  }
};

/// UI-thread-owned lifecycle coordinator for one compiled script module.
class ScriptController final {
public:
  /// Creates an empty controller owned by the calling thread.
  ///
  /// @param limits Hard byte limits checked before script dispatch.
  explicit ScriptController(ControllerLimits limits = {});
  ~ScriptController();

  ScriptController(const ScriptController &) = delete;
  ScriptController &operator=(const ScriptController &) = delete;
  ScriptController(ScriptController &&) = delete;
  ScriptController &operator=(ScriptController &&) = delete;

  /// Resolves lifecycle and compiled event exports into a detached candidate.
  ///
  /// @param module Runtime module whose ownership is transferred to this call.
  /// @param program Immutable handler table retained on success.
  /// @return Success in Compiled state, or a source-located load error while
  ///         leaving the controller Empty. A failed call destroys module.
  ControllerResult
  load(std::unique_ptr<IScriptModule> module,
       std::shared_ptr<const CompiledUiProgram> program);
  /// Calls optional on_mount and transitions Compiled to Mounted.
  /// @return ModuleCallFailed and Faulted when the callback fails.
  ControllerResult mount();
  /// Dispatches one bounded snapshot to its pre-resolved event export.
  /// @return Success for an unbound event; otherwise a structured state,
  ///         limit, or module error. A module error transitions to Faulted.
  ControllerResult dispatch(const ScriptEventSnapshot &event);
  /// Calls optional on_frame with a finite, non-negative delta.
  /// @return Success without a module call when on_frame is absent.
  ControllerResult frame(double delta_seconds);
  /// Stops callbacks, calls optional on_unmount, and releases owned state.
  /// @return A callback error if on_unmount fails; cleanup still completes.
  ControllerResult unmount();

  /// Returns the current lifecycle state; call only on the owner thread.
  ControllerState state() const noexcept;
  /// Retains the immutable program, or returns null while Empty.
  std::shared_ptr<const CompiledUiProgram> program() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace flexUI
