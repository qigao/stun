#include "flexUI/controller.h"

#include <cmath>
#include <exception>
#include <thread>
#include <unordered_map>
#include <utility>

namespace flexUI {
namespace {

constexpr std::string_view kMountExport = "on_mount";
constexpr std::string_view kFrameExport = "on_frame";
constexpr std::string_view kUnmountExport = "on_unmount";

ControllerResult fail(ControllerErrorCode code, ControllerStage stage,
                      std::string message) {
  ControllerResult result;
  result.error.code = code;
  result.error.stage = stage;
  result.error.message = std::move(message);
  return result;
}

ControllerResult fail_module(ControllerStage stage, std::string handler,
                             ScriptModuleError error) {
  ControllerResult result;
  result.error.code = ControllerErrorCode::ModuleCallFailed;
  result.error.stage = stage;
  result.error.message = error.message;
  result.error.handler = std::move(handler);
  result.error.module_error = std::move(error);
  return result;
}

ScriptResolveResult safe_resolve(IScriptModule &module,
                                 std::string_view name) noexcept {
  try {
    return module.resolve_export(name);
  } catch (const std::exception &error) {
    return {{}, {ScriptModuleErrorCode::RuntimeFailure, error.what()}};
  } catch (...) {
    return {{}, {ScriptModuleErrorCode::RuntimeFailure,
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
    return {{ScriptModuleErrorCode::RuntimeFailure,
             "script callback raised an unknown exception"}};
  }
}

} // namespace

struct ScriptController::Impl {
  explicit Impl(ControllerLimits configured_limits,
                UiMutationEngine *configured_mutation_engine = nullptr)
      : limits(configured_limits), owner_thread(std::this_thread::get_id()),
        mutation_engine(configured_mutation_engine) {}

  ControllerLimits limits;
  std::thread::id owner_thread;
  UiMutationEngine *mutation_engine = nullptr;
  ControllerState state = ControllerState::Empty;
  std::unique_ptr<IScriptModule> module;
  std::shared_ptr<const CompiledUiProgram> program;
  std::optional<ScriptExportHandle> mount_export;
  std::optional<ScriptExportHandle> frame_export;
  std::optional<ScriptExportHandle> unmount_export;
  std::unordered_map<const EventBinding *, ScriptExportHandle> event_exports;

  bool is_owner_thread() const noexcept {
    return owner_thread == std::this_thread::get_id();
  }

  ControllerResult apply_mutations(ScriptCallResult &called,
                                   ControllerStage stage,
                                   std::string_view handler) {
    if (called.mutations.empty()) {
      return {};
    }
    if (mutation_engine == nullptr) {
      ControllerResult result =
          fail(ControllerErrorCode::MutationFailed, stage,
               "script emitted UI mutations without a mutation engine");
      result.error.handler.assign(handler);
      return result;
    }
    auto applied = mutation_engine->apply(called.mutations);
    if (!applied) {
      ControllerResult result =
          fail(ControllerErrorCode::MutationFailed, stage,
               applied.error.message);
      result.error.handler.assign(handler);
      result.error.mutation_error = std::move(applied.error);
      return result;
    }
    return {};
  }

  void clear() noexcept {
    event_exports.clear();
    mount_export.reset();
    frame_export.reset();
    unmount_export.reset();
    program.reset();
    module.reset();
    state = ControllerState::Empty;
  }
};

ScriptController::ScriptController(ControllerLimits limits)
    : impl_(std::make_unique<Impl>(limits)) {}

ScriptController::ScriptController(UiMutationEngine &mutation_engine,
                                   ControllerLimits limits)
    : impl_(std::make_unique<Impl>(limits, &mutation_engine)) {}

ScriptController::~ScriptController() {
  if (impl_ != nullptr && impl_->is_owner_thread()) {
    static_cast<void>(unmount());
  }
}

ControllerResult ScriptController::load(
    std::unique_ptr<IScriptModule> module,
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
  std::optional<ScriptExportHandle> unmount_export;
  std::unordered_map<const EventBinding *, ScriptExportHandle> event_exports;
  std::unordered_map<std::string, ScriptExportHandle> resolved_handlers;

  const auto resolve_optional = [&](std::string_view name,
                                    std::optional<ScriptExportHandle> &output)
      -> ControllerResult {
    auto resolved = safe_resolve(*module, name);
    if (resolved.error) {
      ControllerResult result =
          fail(ControllerErrorCode::ModuleResolutionFailed,
               ControllerStage::Load, resolved.error.message);
      result.error.handler.assign(name);
      result.error.module_error = std::move(resolved.error);
      return result;
    }
    if (resolved.handle.has_value() && !static_cast<bool>(*resolved.handle)) {
      ControllerResult result =
          fail(ControllerErrorCode::ModuleResolutionFailed,
               ControllerStage::Load,
               "script module returned an invalid export handle");
      result.error.handler.assign(name);
      result.error.module_error = {
          ScriptModuleErrorCode::InvalidExport,
          "resolved script export handle must not be zero"};
      return result;
    }
    output = resolved.handle;
    return {};
  };

  if (auto result = resolve_optional(kMountExport, mount_export); !result) {
    return result;
  }
  if (auto result = resolve_optional(kFrameExport, frame_export); !result) {
    return result;
  }
  if (auto result = resolve_optional(kUnmountExport, unmount_export); !result) {
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
      auto resolved = safe_resolve(*module, binding.handler);
      if (resolved.error) {
        ControllerResult result =
            fail(ControllerErrorCode::ModuleResolutionFailed,
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
            fail(ControllerErrorCode::MissingHandlerExport,
                 ControllerStage::Load,
                 "compiled event handler is not exported by the script module");
        result.error.element_id = binding.element_id;
        result.error.handler = binding.handler;
        result.error.event = binding.event;
        result.error.source = binding.source;
        return result;
      }
      if (!static_cast<bool>(*resolved.handle)) {
        ControllerResult result =
            fail(ControllerErrorCode::ModuleResolutionFailed,
                 ControllerStage::Load,
                 "script module returned an invalid export handle");
        result.error.element_id = binding.element_id;
        result.error.handler = binding.handler;
        result.error.event = binding.event;
        result.error.source = binding.source;
        result.error.module_error = {
            ScriptModuleErrorCode::InvalidExport,
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
    if (auto applied =
            impl_->apply_mutations(called, ControllerStage::Mount,
                                   kMountExport);
        !applied) {
      impl_->state = ControllerState::Faulted;
      return applied;
    }
  }
  impl_->state = ControllerState::Mounted;
  return {};
}

ControllerResult
ScriptController::dispatch(const ScriptEventSnapshot &event) {
  if (!impl_->is_owner_thread()) {
    return fail(ControllerErrorCode::WrongThread, ControllerStage::Event,
                "controller dispatch must run on its owner thread");
  }
  if (impl_->state == ControllerState::Faulted) {
    return fail(ControllerErrorCode::ControllerFaulted,
                ControllerStage::Event,
                "faulted controller rejects callbacks until explicit reload");
  }
  if (impl_->state != ControllerState::Mounted) {
    return fail(ControllerErrorCode::InvalidState, ControllerStage::Event,
                "controller must be mounted before event dispatch");
  }
  if (!event.target ||
      event.target.id.size() > impl_->limits.max_element_id_bytes ||
      event.text.size() > impl_->limits.max_event_text_bytes ||
      event.composition_text.size() >
          impl_->limits.max_composition_text_bytes) {
    return fail(ControllerErrorCode::EventLimitExceeded,
                ControllerStage::Event,
                "script event snapshot violates configured bounds");
  }

  const auto *binding =
      impl_->program->find_event_binding(event.target.id, event.event);
  if (binding == nullptr) {
    return {};
  }
  const auto resolved = impl_->event_exports.find(binding);
  if (resolved == impl_->event_exports.end()) {
    impl_->state = ControllerState::Faulted;
    return fail(ControllerErrorCode::InternalInvariant,
                ControllerStage::Event,
                "resolved handler table does not match compiled UI program");
  }

  ScriptCallContext context;
  context.callback = ScriptCallbackKind::Event;
  context.event = &event;
  impl_->state = ControllerState::Dispatching;
  auto called = safe_call(*impl_->module, resolved->second, context);
  if (!called) {
    impl_->state = ControllerState::Faulted;
    auto result = fail_module(ControllerStage::Event, binding->handler,
                              std::move(called.error));
    result.error.element_id = binding->element_id;
    result.error.event = binding->event;
    result.error.source = binding->source;
    return result;
  }
  if (auto applied = impl_->apply_mutations(
          called, ControllerStage::Event, binding->handler);
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

ControllerResult ScriptController::frame(double delta_seconds) {
  if (!impl_->is_owner_thread()) {
    return fail(ControllerErrorCode::WrongThread, ControllerStage::Frame,
                "controller frame must run on its owner thread");
  }
  if (impl_->state == ControllerState::Faulted) {
    return fail(ControllerErrorCode::ControllerFaulted,
                ControllerStage::Frame,
                "faulted controller rejects callbacks until explicit reload");
  }
  if (impl_->state != ControllerState::Mounted) {
    return fail(ControllerErrorCode::InvalidState, ControllerStage::Frame,
                "controller must be mounted before frame dispatch");
  }
  if (!std::isfinite(delta_seconds) || delta_seconds < 0.0) {
    return fail(ControllerErrorCode::InvalidArgument,
                ControllerStage::Frame,
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
    return fail_module(ControllerStage::Frame, std::string(kFrameExport),
                       std::move(called.error));
  }
  if (auto applied =
          impl_->apply_mutations(called, ControllerStage::Frame,
                                 kFrameExport);
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
  if (impl_->state == ControllerState::Dispatching ||
      impl_->state == ControllerState::Unmounting) {
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
      result = fail_module(ControllerStage::Unmount,
                           std::string(kUnmountExport),
                           std::move(called.error));
    } else if (!called.mutations.empty()) {
      result = fail(ControllerErrorCode::MutationFailed,
                    ControllerStage::Unmount,
                    "on_unmount must not emit UI mutations");
      result.error.handler = std::string(kUnmountExport);
    }
  }
  impl_->clear();
  return result;
}

ControllerState ScriptController::state() const noexcept {
  return impl_->state;
}

std::shared_ptr<const CompiledUiProgram>
ScriptController::program() const noexcept {
  return impl_->program;
}

} // namespace flexUI
