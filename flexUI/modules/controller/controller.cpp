#include "flexUI/controller.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace flexUI {
namespace {

constexpr std::string_view kMountExport = "on_mount";
constexpr std::string_view kFrameExport = "on_frame";
constexpr std::string_view kServiceCompletionExport = "on_service_completion";
constexpr std::string_view kUnmountExport = "on_unmount";

ControllerResult fail(ControllerErrorCode code, ControllerStage stage, std::string message) {
  ControllerResult result;
  result.error.code = code;
  result.error.stage = stage;
  result.error.message = std::move(message);
  return result;
}

ControllerResult fail_module(ControllerStage stage, std::string handler, ScriptModuleError error) {
  ControllerResult result;
  result.error.code = ControllerErrorCode::ModuleCallFailed;
  result.error.stage = stage;
  result.error.message = error.message;
  result.error.handler = std::move(handler);
  result.error.module_error = std::move(error);
  return result;
}

ScriptResolveResult safe_resolve(IScriptModule &module, std::string_view name,
                                 ScriptCallbackKind callback) noexcept {
  try {
    return module.resolve_export(name, callback);
  } catch (const std::exception &error) {
    return {{}, {ScriptModuleErrorCode::RuntimeFailure, error.what()}};
  } catch (...) {
    return {{},
            {ScriptModuleErrorCode::RuntimeFailure,
             "script export resolution raised an unknown exception"}};
  }
}

ScriptCallResult safe_call(IScriptModule &module, ScriptExportHandle handle,
                           const ScriptCallContext &context) noexcept {
  try {
    return module.call(handle, context);
  } catch (const std::exception &error) {
    return {{ScriptModuleErrorCode::RuntimeFailure, error.what()}};
  } catch (...) {
    return {{ScriptModuleErrorCode::RuntimeFailure, "script callback raised an unknown exception"}};
  }
}

// Only complete compiler-valid ASCII component scopes may be retired.
bool valid_component_event_scope(std::string_view scope,
                                 std::size_t max_id_bytes) noexcept {
  if (scope.empty() || scope.size() > max_id_bytes ||
      max_id_bytes - scope.size() < 3) return false;
  const char first = scope.front();
  if (!((first >= 'A' && first <= 'Z') ||
        (first >= 'a' && first <= 'z') || first == '_')) return false;
  return std::all_of(scope.begin() + 1, scope.end(), [](char ch) {
    return (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '_' || ch == '-';
  });
}

bool event_id_in_scope(std::string_view element_id,
                       std::string_view scope) noexcept {
  return element_id.size() > scope.size() + 2 &&
         element_id.compare(0, scope.size(), scope) == 0 &&
         element_id.substr(scope.size(), 2) == "::";
}

} // namespace

struct ScriptController::Impl {
  explicit Impl(ControllerLimits configured_limits, ControllerEffects configured_effects = {})
      : limits(configured_limits), owner_thread(std::this_thread::get_id()),
        mutation_engine(configured_effects.mutations), command_engine(configured_effects.commands) {
  }

  ControllerLimits limits;
  std::thread::id owner_thread;
  UiMutationEngine *mutation_engine = nullptr;
  ApplicationCommandEngine *command_engine = nullptr;
  ControllerState state = ControllerState::Empty;
  std::unique_ptr<IScriptModule> module;
  std::shared_ptr<const CompiledUiProgram> program;
  std::optional<ScriptExportHandle> mount_export;
  std::optional<ScriptExportHandle> frame_export;
  std::optional<ScriptExportHandle> service_completion_export;
  std::optional<ScriptExportHandle> unmount_export;
  std::unordered_map<const EventBinding *, ScriptExportHandle> event_exports;
  // One authoritative controller-active event table. The compiler program
  // remains immutable; retired scope markers distinguish intentional missing
  // entries from corrupt controller export resolution.
  std::unordered_set<std::string> retired_event_scopes;

  bool is_retired_event(const EventBinding &binding) const noexcept {
    return std::any_of(retired_event_scopes.begin(), retired_event_scopes.end(),
                       [&binding](const std::string &scope) {
                         return event_id_in_scope(binding.element_id, scope);
                       });
  }

  bool is_owner_thread() const noexcept { return owner_thread == std::this_thread::get_id(); }

  ControllerResult apply_effects(ScriptCallResult &called, ControllerStage stage,
                                 std::string_view handler) {
    std::unique_ptr<IPreparedApplicationCommands> commands;
    if (!called.commands.empty()) {
      if (stage == ControllerStage::Mount) {
        ControllerResult result =
            fail(ControllerErrorCode::CommandFailed, stage,
                 "on_mount cannot publish application commands before activation");
        result.error.handler.assign(handler);
        return result;
      }
      if (command_engine == nullptr) {
        ControllerResult result =
            fail(ControllerErrorCode::CommandFailed, stage,
                 "script emitted application commands without a command engine");
        result.error.handler.assign(handler);
        return result;
      }
      auto reserved = command_engine->reserve(called.commands);
      if (!reserved) {
        ControllerResult result =
            fail(ControllerErrorCode::CommandFailed, stage, reserved.error.message);
        result.error.handler.assign(handler);
        result.error.command_error = std::move(reserved.error);
        return result;
      }
      commands = std::move(reserved.prepared);
    }

    if (!called.mutations.empty()) {
      if (mutation_engine == nullptr) {
        ControllerResult result = fail(ControllerErrorCode::MutationFailed, stage,
                                       "script emitted UI mutations without a mutation engine");
        result.error.handler.assign(handler);
        return result;
      }
      auto applied = mutation_engine->apply(called.mutations);
      if (!applied) {
        ControllerResult result =
            fail(ControllerErrorCode::MutationFailed, stage, applied.error.message);
        result.error.handler.assign(handler);
        result.error.mutation_error = std::move(applied.error);
        return result;
      }
    }
    if (commands) {
      commands->publish();
    }
    return {};
  }

  void clear() noexcept {
    event_exports.clear();
    retired_event_scopes.clear();
    mount_export.reset();
    frame_export.reset();
    service_completion_export.reset();
    unmount_export.reset();
    program.reset();
    module.reset();
    state = ControllerState::Empty;
  }
};

ScriptController::ScriptController(ControllerLimits limits)
    : impl_(std::make_unique<Impl>(limits)) {}

ScriptController::ScriptController(UiMutationEngine &mutation_engine, ControllerLimits limits)
    : ScriptController(ControllerEffects{&mutation_engine, nullptr}, limits) {}

ScriptController::ScriptController(ControllerEffects effects, ControllerLimits limits)
    : impl_(std::make_unique<Impl>(limits, effects)) {}

ScriptController::~ScriptController() {
  if (impl_ != nullptr && impl_->is_owner_thread()) {
    static_cast<void>(unmount());
  }
}

ControllerResult ScriptController::load(std::unique_ptr<IScriptModule> module,
                                        std::shared_ptr<const CompiledUiProgram> program) {
  if (!impl_->is_owner_thread()) {
    return fail(ControllerErrorCode::WrongThread, ControllerStage::Load,
                "controller load must run on its owner thread");
  }
  if (impl_->state != ControllerState::Empty) {
    return fail(ControllerErrorCode::InvalidState, ControllerStage::Load,
                "controller must be empty before loading a module");
  }
  if (module == nullptr || program == nullptr) {
    return fail(ControllerErrorCode::InvalidArgument, ControllerStage::Load,
                "module and compiled UI program are required");
  }

  std::optional<ScriptExportHandle> mount_export;
  std::optional<ScriptExportHandle> frame_export;
  std::optional<ScriptExportHandle> service_completion_export;
  std::optional<ScriptExportHandle> unmount_export;
  std::unordered_map<const EventBinding *, ScriptExportHandle> event_exports;
  std::unordered_map<std::string, ScriptExportHandle> resolved_handlers;

  const auto resolve_optional = [&](std::string_view name, ScriptCallbackKind callback,
                                    std::optional<ScriptExportHandle> &output) -> ControllerResult {
    auto resolved = safe_resolve(*module, name, callback);
    if (resolved.error) {
      ControllerResult result = fail(ControllerErrorCode::ModuleResolutionFailed,
                                     ControllerStage::Load, resolved.error.message);
      result.error.handler.assign(name);
      result.error.module_error = std::move(resolved.error);
      return result;
    }
    if (resolved.handle.has_value() && !static_cast<bool>(*resolved.handle)) {
      ControllerResult result =
          fail(ControllerErrorCode::ModuleResolutionFailed, ControllerStage::Load,
               "script module returned an invalid export handle");
      result.error.handler.assign(name);
      result.error.module_error = {ScriptModuleErrorCode::InvalidExport,
                                   "resolved script export handle must not be zero"};
      return result;
    }
    output = resolved.handle;
    return {};
  };

  if (auto result = resolve_optional(kMountExport, ScriptCallbackKind::Mount, mount_export);
      !result) {
    return result;
  }
  if (auto result = resolve_optional(kFrameExport, ScriptCallbackKind::Frame, frame_export);
      !result) {
    return result;
  }
  if (auto result = resolve_optional(kServiceCompletionExport,
                                     ScriptCallbackKind::ServiceCompletion,
                                     service_completion_export);
      !result) {
    return result;
  }
  if (auto result = resolve_optional(kUnmountExport, ScriptCallbackKind::Unmount, unmount_export);
      !result) {
    return result;
  }

  event_exports.reserve(program->event_bindings().size());
  resolved_handlers.reserve(program->event_bindings().size());
  for (const auto &binding : program->event_bindings()) {
    ScriptExportHandle handle;
    const auto cached = resolved_handlers.find(binding.handler);
    if (cached != resolved_handlers.end()) {
      handle = cached->second;
    } else {
      auto resolved = safe_resolve(*module, binding.handler, ScriptCallbackKind::Event);
      if (resolved.error) {
        ControllerResult result = fail(ControllerErrorCode::ModuleResolutionFailed,
                                       ControllerStage::Load, resolved.error.message);
        result.error.element_id = binding.element_id;
        result.error.handler = binding.handler;
        result.error.event = binding.event;
        result.error.source = binding.source;
        result.error.module_error = std::move(resolved.error);
        return result;
      }
      if (!resolved.handle.has_value()) {
        ControllerResult result =
            fail(ControllerErrorCode::MissingHandlerExport, ControllerStage::Load,
                 "compiled event handler is not exported by the script module");
        result.error.element_id = binding.element_id;
        result.error.handler = binding.handler;
        result.error.event = binding.event;
        result.error.source = binding.source;
        return result;
      }
      if (!static_cast<bool>(*resolved.handle)) {
        ControllerResult result =
            fail(ControllerErrorCode::ModuleResolutionFailed, ControllerStage::Load,
                 "script module returned an invalid export handle");
        result.error.element_id = binding.element_id;
        result.error.handler = binding.handler;
        result.error.event = binding.event;
        result.error.source = binding.source;
        result.error.module_error = {ScriptModuleErrorCode::InvalidExport,
                                     "resolved script export handle must not be zero"};
        return result;
      }
      handle = *resolved.handle;
      resolved_handlers.emplace(binding.handler, handle);
    }
    event_exports.emplace(&binding, handle);
  }

  impl_->module = std::move(module);
  impl_->program = std::move(program);
  impl_->mount_export = mount_export;
  impl_->frame_export = frame_export;
  impl_->service_completion_export = service_completion_export;
  impl_->unmount_export = unmount_export;
  impl_->event_exports = std::move(event_exports);
  impl_->state = ControllerState::Compiled;
  return {};
}

ControllerResult ScriptController::mount() {
  if (!impl_->is_owner_thread()) {
    return fail(ControllerErrorCode::WrongThread, ControllerStage::Mount,
                "controller mount must run on its owner thread");
  }
  if (impl_->state != ControllerState::Compiled) {
    return fail(ControllerErrorCode::InvalidState, ControllerStage::Mount,
                "controller must be compiled before mount");
  }

  if (impl_->mount_export.has_value()) {
    ScriptCallContext context;
    context.callback = ScriptCallbackKind::Mount;
    auto called = safe_call(*impl_->module, *impl_->mount_export, context);
    if (!called) {
      impl_->state = ControllerState::Faulted;
      return fail_module(ControllerStage::Mount, std::string(kMountExport),
                         std::move(called.error));
    }
    if (auto applied = impl_->apply_effects(called, ControllerStage::Mount, kMountExport);
        !applied) {
      impl_->state = ControllerState::Faulted;
      return applied;
    }
  }
  impl_->state = ControllerState::Mounted;
  return {};
}

ControllerResult ScriptController::dispatch(const ScriptEventSnapshot &event) {
  if (!impl_->is_owner_thread()) {
    return fail(ControllerErrorCode::WrongThread, ControllerStage::Event,
                "controller dispatch must run on its owner thread");
  }
  if (impl_->state == ControllerState::Faulted) {
    return fail(ControllerErrorCode::ControllerFaulted, ControllerStage::Event,
                "faulted controller rejects callbacks until explicit reload");
  }
  if (impl_->state != ControllerState::Mounted) {
    return fail(ControllerErrorCode::InvalidState, ControllerStage::Event,
                "controller must be mounted before event dispatch");
  }
  const UiHandle &handler_target = event.current_target ? event.current_target : event.target;
  if (!handler_target || event.target.id.size() > impl_->limits.max_element_id_bytes ||
      handler_target.id.size() > impl_->limits.max_element_id_bytes ||
      event.text.size() > impl_->limits.max_event_text_bytes ||
      event.composition_text.size() > impl_->limits.max_composition_text_bytes) {
    return fail(ControllerErrorCode::EventLimitExceeded, ControllerStage::Event,
                "script event snapshot violates configured bounds");
  }

  const auto *binding = impl_->program->find_event_binding(handler_target.id, event.event);
  if (binding == nullptr) {
    return {};
  }
  const auto resolved = impl_->event_exports.find(binding);
  if (resolved == impl_->event_exports.end()) {
    if (impl_->is_retired_event(*binding)) return {};
    impl_->state = ControllerState::Faulted;
    return fail(ControllerErrorCode::InternalInvariant, ControllerStage::Event,
                "resolved handler table does not match compiled UI program");
  }

  ScriptCallContext context;
  context.callback = ScriptCallbackKind::Event;
  context.event = &event;
  impl_->state = ControllerState::Dispatching;
  auto called = safe_call(*impl_->module, resolved->second, context);
  if (!called) {
    impl_->state = ControllerState::Faulted;
    auto result = fail_module(ControllerStage::Event, binding->handler, std::move(called.error));
    result.error.element_id = binding->element_id;
    result.error.event = binding->event;
    result.error.source = binding->source;
    return result;
  }
  // Attribute commands to the COMPILED handler, not an untrusted payload
  // or the original event target (which may have bubbled to this handler).
  // The borrowed scope remains alive in the immutable program throughout
  // the synchronous reservation transaction.
  if (!called.commands.empty()) {
    const auto separator = binding->element_id.find("::");
    if (separator != std::string::npos && separator != 0)
      called.commands.origin_scope_ =
          std::string_view(binding->element_id).substr(0, separator);
  }
  if (auto applied = impl_->apply_effects(called, ControllerStage::Event, binding->handler);
      !applied) {
    impl_->state = ControllerState::Faulted;
    applied.error.element_id = binding->element_id;
    applied.error.event = binding->event;
    applied.error.source = binding->source;
    return applied;
  }
  impl_->state = ControllerState::Mounted;
  return {};
}

bool ScriptController::has_active_event_binding(
    std::string_view element_id, UiEventKind event) const noexcept {
  // Route existence and execution permission are different facts.
  // A Faulted controller still holds the old compiled table and must have
  // its ControllerFaulted diagnostic propagated by native EventBridge,
  // rather than silently swallowing a real native event.
  if (!impl_->is_owner_thread() || impl_->program == nullptr) return false;
  const auto *binding = impl_->program->find_event_binding(element_id, event);
  return binding != nullptr &&
         impl_->event_exports.find(binding) != impl_->event_exports.end();
}

bool ScriptController::is_scoped_event_route_retired(
    std::string_view scope) const noexcept {
  if (!impl_->is_owner_thread() || impl_->state != ControllerState::Mounted)
    return false;
  return std::any_of(impl_->retired_event_scopes.begin(),
                     impl_->retired_event_scopes.end(),
                     [scope](const std::string &value) {
                       return std::string_view(value) == scope;
                     });
}

ControllerResult ScriptController::retire_scoped_event_handlers(
    std::string_view scope) {
  if (!impl_->is_owner_thread()) {
    return fail(ControllerErrorCode::WrongThread,
                ControllerStage::EventRetirement,
                "scoped event retirement requires the controller owner thread");
  }
  if (impl_->state != ControllerState::Mounted || !impl_->program) {
    return fail(ControllerErrorCode::InvalidState,
                ControllerStage::EventRetirement,
                "scoped event retirement requires a mounted idle controller");
  }
  if (!valid_component_event_scope(scope, impl_->limits.max_element_id_bytes)) {
    return fail(ControllerErrorCode::InvalidArgument,
                ControllerStage::EventRetirement,
                "scoped event retirement requires a bounded ASCII component scope");
  }
  const auto already = std::find_if(
      impl_->retired_event_scopes.begin(), impl_->retired_event_scopes.end(),
      [scope](const std::string &value) {
        return std::string_view(value) == scope;
      });
  if (already != impl_->retired_event_scopes.end()) return {};

  std::size_t matches = 0;
  for (const auto &binding : impl_->program->event_bindings()) {
    if (!event_id_in_scope(binding.element_id, scope)) continue;
    if (impl_->event_exports.find(&binding) == impl_->event_exports.end()) {
      return fail(ControllerErrorCode::InternalInvariant,
                  ControllerStage::EventRetirement,
                  "active scoped event table mismatches the compiled definition");
    }
    ++matches;
  }
  if (matches == 0) {
    return fail(ControllerErrorCode::InvalidArgument,
                ControllerStage::EventRetirement,
                "the requested scope has no compiled script event handlers");
  }

  try {
    // Stage every allocation privately. Publish all old-handler removals
    // with a pair of no-throw standard-container swaps, never a partial erase.
    auto next_routes = impl_->event_exports;
    auto next_retired = impl_->retired_event_scopes;
    next_retired.emplace(scope);
    for (const auto &binding : impl_->program->event_bindings()) {
      if (event_id_in_scope(binding.element_id, scope))
        next_routes.erase(&binding);
    }
    impl_->event_exports.swap(next_routes);
    impl_->retired_event_scopes.swap(next_retired);
  } catch (const std::exception &) {
    return fail(ControllerErrorCode::EventRetirementFailed,
                ControllerStage::EventRetirement,
                "failed to stage scoped Controller event retirement");
  } catch (...) {
    return fail(ControllerErrorCode::EventRetirementFailed,
                ControllerStage::EventRetirement,
                "unknown scoped Controller event retirement error");
  }
  return {};
}

ControllerResult ScriptController::frame(double delta_seconds) {
  if (!impl_->is_owner_thread()) {
    return fail(ControllerErrorCode::WrongThread, ControllerStage::Frame,
                "controller frame must run on its owner thread");
  }
  if (impl_->state == ControllerState::Faulted) {
    return fail(ControllerErrorCode::ControllerFaulted, ControllerStage::Frame,
                "faulted controller rejects callbacks until explicit reload");
  }
  if (impl_->state != ControllerState::Mounted) {
    return fail(ControllerErrorCode::InvalidState, ControllerStage::Frame,
                "controller must be mounted before frame dispatch");
  }
  if (!std::isfinite(delta_seconds) || delta_seconds < 0.0) {
    return fail(ControllerErrorCode::InvalidArgument, ControllerStage::Frame,
                "frame delta must be finite and non-negative");
  }
  if (!impl_->frame_export.has_value()) {
    return {};
  }

  ScriptCallContext context;
  context.callback = ScriptCallbackKind::Frame;
  context.delta_seconds = delta_seconds;
  impl_->state = ControllerState::Dispatching;
  auto called = safe_call(*impl_->module, *impl_->frame_export, context);
  if (!called) {
    impl_->state = ControllerState::Faulted;
    return fail_module(ControllerStage::Frame, std::string(kFrameExport), std::move(called.error));
  }
  if (auto applied = impl_->apply_effects(called, ControllerStage::Frame, kFrameExport); !applied) {
    impl_->state = ControllerState::Faulted;
    return applied;
  }
  impl_->state = ControllerState::Mounted;
  return {};
}

ControllerResult ScriptController::dispatch_service_completion(
    const ApplicationServiceCompletion &completion) {
  if (!impl_->is_owner_thread()) {
    return fail(ControllerErrorCode::WrongThread, ControllerStage::ServiceCompletion,
                "service completion dispatch must run on the controller owner thread");
  }
  if (impl_->state == ControllerState::Faulted) {
    return fail(ControllerErrorCode::ControllerFaulted, ControllerStage::ServiceCompletion,
                "faulted controller rejects callbacks until explicit reload");
  }
  if (impl_->state != ControllerState::Mounted) {
    return fail(ControllerErrorCode::InvalidState, ControllerStage::ServiceCompletion,
                "controller must be mounted before service completion dispatch");
  }
  if (completion.script_request_id == 0) {
    return fail(ControllerErrorCode::InvalidArgument, ControllerStage::ServiceCompletion,
                "service completion request ID must be non-zero");
  }
  switch (completion.status) {
  case ApplicationCompletionStatus::Succeeded:
    if (!completion.error_code.empty() || !completion.error_message.empty()) {
      return fail(ControllerErrorCode::InvalidArgument, ControllerStage::ServiceCompletion,
                  "successful service completion cannot carry error fields");
    }
    break;
  case ApplicationCompletionStatus::Failed:
    if (completion.error_code.empty()) {
      return fail(ControllerErrorCode::InvalidArgument, ControllerStage::ServiceCompletion,
                  "failed service completion requires an error code");
    }
    break;
  case ApplicationCompletionStatus::Cancelled:
    if (!completion.payload.empty() || !completion.error_code.empty()) {
      return fail(ControllerErrorCode::InvalidArgument, ControllerStage::ServiceCompletion,
                  "cancelled service completion cannot carry payload or an error code");
    }
    break;
  default:
    return fail(ControllerErrorCode::InvalidArgument, ControllerStage::ServiceCompletion,
                "service completion status is invalid");
  }

  const auto &limits = impl_->limits;
  if (completion.payload.size() > limits.max_service_payload_bytes ||
      completion.error_code.size() > limits.max_service_error_code_bytes ||
      completion.error_message.size() > limits.max_service_error_message_bytes) {
    return fail(ControllerErrorCode::CompletionLimitExceeded,
                ControllerStage::ServiceCompletion,
                "service completion field exceeds configured bounds");
  }
  std::size_t total = completion.payload.size();
  if (completion.error_code.size() > limits.max_service_total_string_bytes ||
      total > limits.max_service_total_string_bytes - completion.error_code.size()) {
    return fail(ControllerErrorCode::CompletionLimitExceeded,
                ControllerStage::ServiceCompletion,
                "service completion strings exceed the configured total bound");
  }
  total += completion.error_code.size();
  if (completion.error_message.size() > limits.max_service_total_string_bytes ||
      total > limits.max_service_total_string_bytes - completion.error_message.size()) {
    return fail(ControllerErrorCode::CompletionLimitExceeded,
                ControllerStage::ServiceCompletion,
                "service completion strings exceed the configured total bound");
  }
  if (!impl_->service_completion_export.has_value()) {
    return {};
  }

  ScriptCallContext context;
  context.callback = ScriptCallbackKind::ServiceCompletion;
  context.service_completion = &completion;
  impl_->state = ControllerState::Dispatching;
  auto called = safe_call(*impl_->module, *impl_->service_completion_export, context);
  if (!called) {
    impl_->state = ControllerState::Faulted;
    return fail_module(ControllerStage::ServiceCompletion,
                       std::string(kServiceCompletionExport), std::move(called.error));
  }
  if (auto applied = impl_->apply_effects(called, ControllerStage::ServiceCompletion,
                                          kServiceCompletionExport);
      !applied) {
    impl_->state = ControllerState::Faulted;
    return applied;
  }
  impl_->state = ControllerState::Mounted;
  return {};
}

ControllerResult ScriptController::unmount() {
  if (!impl_->is_owner_thread()) {
    return fail(ControllerErrorCode::WrongThread, ControllerStage::Unmount,
                "controller unmount must run on its owner thread");
  }
  if (impl_->state == ControllerState::Empty) {
    return {};
  }
  if (impl_->state == ControllerState::Dispatching || impl_->state == ControllerState::Unmounting) {
    return fail(ControllerErrorCode::InvalidState, ControllerStage::Unmount,
                "controller cannot unmount during a callback");
  }
  if (impl_->state == ControllerState::Compiled) {
    impl_->clear();
    return {};
  }

  impl_->state = ControllerState::Unmounting;
  ControllerResult result;
  if (impl_->unmount_export.has_value()) {
    ScriptCallContext context;
    context.callback = ScriptCallbackKind::Unmount;
    auto called = safe_call(*impl_->module, *impl_->unmount_export, context);
    if (!called) {
      result = fail_module(ControllerStage::Unmount, std::string(kUnmountExport),
                           std::move(called.error));
    } else if (!called.mutations.empty()) {
      result = fail(ControllerErrorCode::MutationFailed, ControllerStage::Unmount,
                    "on_unmount must not emit UI mutations");
      result.error.handler = std::string(kUnmountExport);
    } else if (!called.commands.empty()) {
      result = fail(ControllerErrorCode::CommandFailed, ControllerStage::Unmount,
                    "on_unmount must not emit application commands");
      result.error.handler = std::string(kUnmountExport);
    }
  }
  impl_->clear();
  return result;
}

ControllerState ScriptController::state() const noexcept { return impl_->state; }

std::shared_ptr<const CompiledUiProgram> ScriptController::program() const noexcept {
  return impl_->program;
}

bool ScriptController::has_service_completion_handler() const noexcept {
  return impl_->service_completion_export.has_value();
}

} // namespace flexUI
