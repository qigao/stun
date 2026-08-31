#pragma once

#include "flexUI/controller.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace flexUI {

enum class TurboScriptExecutionMode {
  Interpreter,
  Jit,
};

using TurboScriptInterruptCallback = bool (*)(void *user_data);

struct TurboScriptCompileLimits {
  std::size_t max_source_bytes = 0;
  std::size_t max_ast_nodes = 0;
  std::size_t max_imports = 0;
  std::size_t max_exports = 0;
  std::size_t max_string_bytes = 0;
};

struct TurboScriptInstanceLimits {
  std::size_t max_retained_bytes = 0;
  std::size_t max_stack_bytes = 0;
  std::uint32_t max_recursion = 0;
  std::uint32_t max_globals = 0;
  std::uint32_t max_value_depth = 0;
  std::size_t max_value_nodes = 0;
  std::size_t max_result_bytes = 0;
};

struct TurboScriptCallLimits {
  std::uint32_t max_recursion = 0;
  std::uint32_t max_steps = 0;
  std::uint32_t max_loop_iterations = 0;
  std::uint32_t max_host_callbacks = 0;
  std::size_t max_result_bytes = 0;
};

/// Fixed resource policy for one TurboScript controller module.
///
/// Zero-valued limits preserve TurboScript's bounded defaults. The interrupt
/// callback is borrowed for the created module's lifetime and runs
/// synchronously on the module owner thread.
struct TurboScriptControllerOptions {
  TurboScriptExecutionMode execution_mode = TurboScriptExecutionMode::Jit;
  TurboScriptCompileLimits compile_limits;
  TurboScriptInstanceLimits instance_limits;
  TurboScriptCallLimits call_limits;
  TurboScriptInterruptCallback interrupt = nullptr;
  void *interrupt_user_data = nullptr;
};

struct TurboScriptModuleCreateResult {
  std::unique_ptr<IScriptModule> module;
  ScriptModuleError error;

  explicit operator bool() const noexcept { return module != nullptr && !static_cast<bool>(error); }
};

/// Compiles one source-backed TurboScript module and creates its isolated
/// stateful instance. The returned module is owner-thread-affine and must be
/// destroyed on the creating thread.
///
/// Every exported callback must return null or one effect record with optional
/// `mutations` and `commands` arrays. Mutation records use the public
/// UiMutation operation names documented by FlexUI; command records contain
/// `request_id`, `capability`, `operation`, and `payload`. Unknown fields,
/// malformed entries, and bounded-batch overflow fail the whole callback, so
/// no partial effect is returned to ScriptController.
///
/// Native TurboScript plugins are denied at this boundary. Source and module
/// name are copied before this call returns.
TurboScriptModuleCreateResult
create_turboscript_controller_module(std::string_view source,
                                     std::string_view module_name = "flexui-controller",
                                     TurboScriptControllerOptions options = {});

} // namespace flexUI
