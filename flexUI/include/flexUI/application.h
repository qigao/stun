#pragma once

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
};

struct DesktopApplicationError {
  DesktopApplicationErrorCode code = DesktopApplicationErrorCode::None;
  DesktopApplicationStage stage = DesktopApplicationStage::None;
  std::string message;
  UiDocumentError ui_error;
  std::vector<CssDiagnostic> css_diagnostics;
  ScriptModuleError script_error;
  ControllerError controller_error;

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

  /// Reports whether the caller may access owner-thread application state.
  /// This query does not read the active Box and is safe from any thread.
  bool is_owner_thread() const noexcept;

  /// Routes one native event through widgets and the optional script
  /// controller. Script callbacks run only for unconsumed compiled bindings.
  /// This operation must run on the application owner thread.
  DesktopApplicationResult dispatch_event(Event &event);

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
