#pragma once

#include "flexUI/application_completion.h"
#include "flexUI/box.h"
#include "flexUI/controller.h"
#include "flexUI/widget_registry.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace flexUI {

class DesktopApplication;

struct ScriptModuleFactoryResult {
  std::unique_ptr<IScriptModule> module;
  ScriptModuleError error;

  explicit operator bool() const noexcept { return module != nullptr && !static_cast<bool>(error); }
};

/// Creates one isolated script module from owned application source.
///
/// The factory is called on the application owner thread. Implementations must
/// copy source and module_name before returning if they retain either value.
using ScriptModuleFactory =
    std::function<ScriptModuleFactoryResult(std::string_view source, std::string_view module_name)>;

struct DesktopApplicationSources {
  std::string ui;
  std::string stylesheet;
  std::string script;
  std::string script_module_name = "flexui-application";
};

struct DesktopApplicationLimits {
  UiDocumentLimits document;
  ControllerLimits controller;
  MutationLimits mutation;
  ApplicationCompletionLimits completion;
};

enum class DesktopApplicationStage {
  None,
  Configuration,
  UiCompile,
  CssLoad,
  UiInstantiation,
  ScriptModuleCreation,
  ControllerLoad,
  ControllerMount,
  ControllerEvent,
  CandidateBuild,
  Lifecycle,
  ControllerUnmount,
  ControllerFrame,
  CompletionMailbox,
};

enum class DesktopApplicationErrorCode {
  None,
  InvalidConfiguration,
  WrongThread,
  UiCompileFailed,
  CssLoadFailed,
  UiInstantiationFailed,
  ScriptRequired,
  ScriptModuleCreationFailed,
  ControllerLoadFailed,
  ControllerMountFailed,
  ControllerDispatchFailed,
  EventDispatchFailed,
  CandidateBuildFailed,
  InvalidState,
  ControllerUnmountFailed,
  InvalidArgument,
  ControllerFrameFailed,
  CompletionMailboxFailed,
  GenerationExhausted,
};

struct DesktopApplicationError {
  DesktopApplicationErrorCode code = DesktopApplicationErrorCode::None;
  DesktopApplicationStage stage = DesktopApplicationStage::None;
  std::string message;
  UiDocumentError ui_error;
  std::vector<CssDiagnostic> css_diagnostics;
  ScriptModuleError script_error;
  ControllerError controller_error;
  ApplicationCompletionError completion_error;

  explicit operator bool() const noexcept { return code != DesktopApplicationErrorCode::None; }
};

struct DesktopApplicationResult {
  DesktopApplicationError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

struct DesktopApplicationBuildResult {
  std::unique_ptr<DesktopApplication> application;
  DesktopApplicationError error;

  explicit operator bool() const noexcept {
    return application != nullptr && !static_cast<bool>(error);
  }
};

/// Monotonic owner-thread lifecycle of one published desktop application.
enum class DesktopApplicationState {
  Ready,
  CloseRequested,
  Shutdown,
};

/// Published owner-thread application state for one UI/CSS/script bundle.
///
/// The application owns Box, compiled program, mutation adapter and optional
/// mounted controller. The renderer passed to the builder is borrowed and must
/// outlive the application. Reload builds a complete detached candidate and
/// publishes it with one no-fail owner swap. All access must remain on the
/// thread that called DesktopApplicationBuilder::build().
class DesktopApplication final {
public:
  ~DesktopApplication();

  DesktopApplication(const DesktopApplication &) = delete;
  DesktopApplication &operator=(const DesktopApplication &) = delete;
  DesktopApplication(DesktopApplication &&) = delete;
  DesktopApplication &operator=(DesktopApplication &&) = delete;

  Box &box() noexcept;
  const Box &box() const noexcept;
  const CompiledUiProgram &program() const noexcept;
  ScriptController *controller() noexcept;
  const ScriptController *controller() const noexcept;
  const DesktopApplicationSources &sources() const noexcept;
  bool uses_legacy_flex_compatibility() const noexcept;
  DesktopApplicationState state() const noexcept;
  /// Current application incarnation used in host-issued service tokens. This
  /// atomic query is safe from producer threads.
  std::uint64_t generation() const noexcept;
  /// The returned object is application-owned. Only try_post() may be called
  /// outside the application owner thread.
  ApplicationCompletionMailbox &completion_mailbox() noexcept;
  const ApplicationCompletionMailbox &completion_mailbox() const noexcept;

  /// Reports whether the caller may access owner-thread application state.
  /// This query does not read the active Box and is safe from any thread.
  bool is_owner_thread() const noexcept;

  /// Stops future event dispatch and reload work without destroying the
  /// published UI state. Repeated owner-thread calls are idempotent.
  DesktopApplicationResult request_close();

  /// Unmounts the optional controller after close was requested. Published
  /// Box, program and source state remain readable until destruction.
  /// Repeated calls after completed cleanup are idempotent.
  DesktopApplicationResult shutdown();

  /// Routes one native event through widgets and the optional script
  /// controller. Script callbacks run only for unconsumed compiled bindings.
  /// This operation must run on the application owner thread.
  DesktopApplicationResult dispatch_event(Event &event);

  /// Calls the optional script on_frame callback with a finite, non-negative
  /// delta. This operation does not advance Box animation time or render; the
  /// desktop host owns those frame stages.
  DesktopApplicationResult frame(double delta_seconds);

  /// Builds and validates a replacement before changing active state. This
  /// operation must run on the application owner thread.
  /// @return Success after atomic publication, or a structured candidate error
  ///         while the current Box, program and controller remain unchanged.
  DesktopApplicationResult reload(DesktopApplicationSources sources);

private:
  struct Impl;
  explicit DesktopApplication(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;

  friend class DesktopApplicationBuilder;
};

/// Fluent candidate builder. XML is the default and recommended entry format.
class DesktopApplicationBuilder final {
public:
  /// @param renderer Borrowed renderer; null is valid for headless operation.
  explicit DesktopApplicationBuilder(flex::Renderer *renderer = nullptr);
  ~DesktopApplicationBuilder();

  DesktopApplicationBuilder(const DesktopApplicationBuilder &) = delete;
  DesktopApplicationBuilder &operator=(const DesktopApplicationBuilder &) = delete;
  DesktopApplicationBuilder(DesktopApplicationBuilder &&) noexcept;
  DesktopApplicationBuilder &operator=(DesktopApplicationBuilder &&) noexcept;

  DesktopApplicationBuilder &xml_entry(std::string source);

  /// Explicit migration-only entry for the retired `.flex` UI syntax.
  DesktopApplicationBuilder &legacy_flex_entry_compatibility(std::string source);

  DesktopApplicationBuilder &stylesheet(std::string source);
  DesktopApplicationBuilder &script(std::string source, ScriptModuleFactory factory,
                                    std::string module_name = "flexui-application");
  /// Replaces the borrowed renderer used by subsequently built candidates.
  DesktopApplicationBuilder &renderer(flex::Renderer *renderer) noexcept;
  DesktopApplicationBuilder &box_options(BoxOptions options);
  DesktopApplicationBuilder &limits(DesktopApplicationLimits limits);
  DesktopApplicationBuilder &widget_registry(WidgetRegistry registry);

  /// Validates and mounts a detached candidate before returning ownership.
  DesktopApplicationBuildResult build() const;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace flexUI
