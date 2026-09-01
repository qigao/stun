#include "flexUI/application.h"

#include "application_service_requests.hpp"
#include "flexUI/box_mutation_host.h"
#include "flexUI/ui_xml.h"

#include <cmath>
#include <exception>
#include <limits>
#include <new>
#include <optional>
#include <thread>
#include <utility>

namespace flexUI {
namespace {

enum class UiEntryFormat {
  Xml,
  LegacyFlexCompatibility,
};

struct ApplicationConfig {
  flex::Renderer *renderer = nullptr;
  BoxOptions box_options;
  DesktopApplicationLimits limits;
  WidgetRegistry registry;
  ScriptModuleFactory script_factory;
  UiEntryFormat entry_format = UiEntryFormat::Xml;
  bool script_enabled = false;
};

std::optional<UiEventKind> to_ui_event_kind(EventType type) noexcept {
  switch (type) {
  case EventType::Click:
    return UiEventKind::Click;
  case EventType::MouseMove:
    return UiEventKind::MouseMove;
  case EventType::MouseDown:
    return UiEventKind::MouseDown;
  case EventType::MouseUp:
    return UiEventKind::MouseUp;
  case EventType::MouseWheel:
    return UiEventKind::MouseWheel;
  case EventType::KeyDown:
    return UiEventKind::KeyDown;
  case EventType::KeyUp:
    return UiEventKind::KeyUp;
  case EventType::TextInput:
    return UiEventKind::TextInput;
  case EventType::FocusIn:
    return UiEventKind::FocusIn;
  case EventType::FocusOut:
    return UiEventKind::FocusOut;
  case EventType::CompositionStart:
    return UiEventKind::CompositionStart;
  case EventType::CompositionUpdate:
    return UiEventKind::CompositionUpdate;
  case EventType::CompositionEnd:
    return UiEventKind::CompositionEnd;
  case EventType::MouseEnter:
  case EventType::MouseLeave:
  case EventType::TouchStart:
  case EventType::TouchMove:
  case EventType::TouchEnd:
    return std::nullopt;
  }
  return std::nullopt;
}

ControllerResult bridge_failure(ControllerErrorCode code, std::string message) {
  ControllerResult result;
  result.error.code = code;
  result.error.stage = ControllerStage::Event;
  result.error.message = std::move(message);
  return result;
}

class ControllerEventBridge final {
public:
  ControllerEventBridge(Box &box, ScriptController &controller)
      : box_(box), controller_(controller) {
    box_.set_framework_event_callback(
        [this](Element &current_target, const Event &event) { observe(current_target, event); });
  }

  ~ControllerEventBridge() { box_.set_framework_event_callback({}); }

  ControllerEventBridge(const ControllerEventBridge &) = delete;
  ControllerEventBridge &operator=(const ControllerEventBridge &) = delete;

  ControllerResult dispatch(Event &event) {
    if (dispatching_) {
      return bridge_failure(ControllerErrorCode::InvalidState,
                            "desktop application event dispatch cannot be re-entered");
    }

    struct DispatchScope final {
      explicit DispatchScope(bool &dispatching) : dispatching_(dispatching) { dispatching_ = true; }
      ~DispatchScope() { dispatching_ = false; }
      bool &dispatching_;
    } scope(dispatching_);

    error_ = {};
    box_.dispatch_event(event);
    if (error_) {
      return {std::move(error_)};
    }
    return {};
  }

private:
  void observe(Element &current_element, const Event &event) {
    if (!dispatching_ || error_) {
      return;
    }

    const auto kind = to_ui_event_kind(event.type);
    if (!kind.has_value()) {
      return;
    }
    const UiHandle current_target = box_.handle_for(current_element);
    const auto program = controller_.program();
    if (!current_target || program == nullptr ||
        program->find_event_binding(current_target.id, *kind) == nullptr) {
      return;
    }

    ScriptEventSnapshot snapshot;
    snapshot.event = *kind;
    snapshot.current_target = current_target;
    snapshot.target = event.target != nullptr ? box_.handle_for(*event.target) : UiHandle{};
    snapshot.x = event.x;
    snapshot.y = event.y;
    snapshot.delta_x = event.delta_x;
    snapshot.delta_y = event.delta_y;
    snapshot.button = static_cast<std::int32_t>(event.button);
    snapshot.key = static_cast<std::int32_t>(event.key);
    snapshot.modifiers = event.mods;
    snapshot.text = event.text;
    snapshot.composition_text = event.composition_text;
    snapshot.timestamp_ms = event.timestamp_ms;
    snapshot.handled = event.handled;
    snapshot.propagate = event.propagate;

    auto dispatched = controller_.dispatch(snapshot);
    if (!dispatched) {
      error_ = std::move(dispatched.error);
    }
  }

  Box &box_;
  ScriptController &controller_;
  ControllerError error_;
  bool dispatching_ = false;
};

struct PublishedApplicationState {
  DesktopApplicationSources sources;
  std::shared_ptr<const CompiledUiProgram> program;
  std::unique_ptr<Box> box;
  std::unique_ptr<BoxMutationHost> mutation_host;
  std::unique_ptr<UiMutationEngine> mutation_engine;
  std::unique_ptr<ScriptController> controller;
  std::unique_ptr<ControllerEventBridge> event_bridge;
};

struct CandidateResult {
  std::unique_ptr<PublishedApplicationState> state;
  DesktopApplicationError error;

  explicit operator bool() const noexcept { return state != nullptr && !static_cast<bool>(error); }
};

DesktopApplicationError fail(DesktopApplicationErrorCode code, DesktopApplicationStage stage,
                             std::string message) {
  return {code, stage, std::move(message)};
}

CandidateResult build_candidate(const ApplicationConfig &config, DesktopApplicationSources sources,
                                ApplicationCommandEngine *command_engine) {
  if (sources.ui.empty()) {
    return {{},
            fail(DesktopApplicationErrorCode::InvalidConfiguration,
                 DesktopApplicationStage::Configuration,
                 "desktop application UI source must not be empty")};
  }
  if (config.script_enabled && !config.script_factory) {
    return {{},
            fail(DesktopApplicationErrorCode::InvalidConfiguration,
                 DesktopApplicationStage::Configuration,
                 "configured desktop script requires a module factory")};
  }
  if (!config.script_enabled && !sources.script.empty()) {
    return {{},
            fail(DesktopApplicationErrorCode::InvalidConfiguration,
                 DesktopApplicationStage::Configuration,
                 "reload script source requires a configured module factory")};
  }
  if (config.script_enabled && sources.script_module_name.empty()) {
    return {{},
            fail(DesktopApplicationErrorCode::InvalidConfiguration,
                 DesktopApplicationStage::Configuration,
                 "desktop script module name must not be empty")};
  }

  try {
    UiDocumentCompileResult compiled =
        config.entry_format == UiEntryFormat::Xml
            ? compile_ui_xml(sources.ui, config.limits.document)
            : compile_ui_document(sources.ui, {}, config.limits.document);
    if (!compiled) {
      auto error = fail(DesktopApplicationErrorCode::UiCompileFailed,
                        DesktopApplicationStage::UiCompile, compiled.error.message);
      error.ui_error = std::move(compiled.error);
      return {{}, std::move(error)};
    }

    if (!config.script_enabled && !compiled.program->event_bindings().empty()) {
      auto error =
          fail(DesktopApplicationErrorCode::ScriptRequired, DesktopApplicationStage::ControllerLoad,
               "UI event handlers require an explicitly configured script module factory");
      const auto &binding = compiled.program->event_bindings().front();
      error.controller_error = {ControllerErrorCode::MissingHandlerExport,
                                ControllerStage::Load,
                                "no script module is configured for the compiled event handler",
                                binding.element_id,
                                binding.handler,
                                binding.event,
                                binding.source};
      return {{}, std::move(error)};
    }

    auto state = std::make_unique<PublishedApplicationState>();
    state->sources = std::move(sources);
    state->program = std::move(compiled.program);
    state->box = std::make_unique<Box>(config.renderer, config.box_options);

    const CssLoadResult css = state->box->load_stylesheet(
        state->sources.stylesheet, CssLoadOptions{"<desktop-stylesheet>", true});
    if (!css.applied) {
      auto error =
          fail(DesktopApplicationErrorCode::CssLoadFailed, DesktopApplicationStage::CssLoad,
               "desktop stylesheet was rejected in strict mode");
      error.css_diagnostics = css.diagnostics;
      return {{}, std::move(error)};
    }

    const auto instantiated =
        UiDocumentInstantiator::instantiate(*state->box, *state->program, config.registry);
    if (!instantiated) {
      auto error = fail(DesktopApplicationErrorCode::UiInstantiationFailed,
                        DesktopApplicationStage::UiInstantiation, instantiated.error.message);
      error.ui_error = instantiated.error;
      return {{}, std::move(error)};
    }

    if (config.script_enabled) {
      ScriptModuleFactoryResult created;
      try {
        created = config.script_factory(state->sources.script, state->sources.script_module_name);
      } catch (const std::exception &exception) {
        auto error = fail(DesktopApplicationErrorCode::ScriptModuleCreationFailed,
                          DesktopApplicationStage::ScriptModuleCreation,
                          std::string("script module factory failed: ") + exception.what());
        error.script_error = {ScriptModuleErrorCode::RuntimeFailure, error.message};
        return {{}, std::move(error)};
      } catch (...) {
        auto error = fail(DesktopApplicationErrorCode::ScriptModuleCreationFailed,
                          DesktopApplicationStage::ScriptModuleCreation,
                          "script module factory failed with an unknown exception");
        error.script_error = {ScriptModuleErrorCode::RuntimeFailure, error.message};
        return {{}, std::move(error)};
      }
      if (!created) {
        auto error =
            fail(DesktopApplicationErrorCode::ScriptModuleCreationFailed,
                 DesktopApplicationStage::ScriptModuleCreation,
                 created.error.message.empty() ? "script module factory returned no usable module"
                                               : created.error.message);
        error.script_error = std::move(created.error);
        return {{}, std::move(error)};
      }

      state->mutation_host = std::make_unique<BoxMutationHost>(*state->box);
      state->mutation_engine =
          std::make_unique<UiMutationEngine>(*state->mutation_host, config.limits.mutation);
      state->controller = std::make_unique<ScriptController>(
          ControllerEffects{state->mutation_engine.get(), command_engine},
          config.limits.controller);

      auto loaded = state->controller->load(std::move(created.module), state->program);
      if (!loaded) {
        auto error = fail(DesktopApplicationErrorCode::ControllerLoadFailed,
                          DesktopApplicationStage::ControllerLoad, loaded.error.message);
        error.controller_error = std::move(loaded.error);
        return {{}, std::move(error)};
      }

      auto mounted = state->controller->mount();
      if (!mounted) {
        auto error = fail(DesktopApplicationErrorCode::ControllerMountFailed,
                          DesktopApplicationStage::ControllerMount, mounted.error.message);
        error.controller_error = std::move(mounted.error);
        return {{}, std::move(error)};
      }
      state->event_bridge =
          std::make_unique<ControllerEventBridge>(*state->box, *state->controller);
    }

    return {std::move(state), {}};
  } catch (const std::exception &exception) {
    return {{},
            fail(DesktopApplicationErrorCode::CandidateBuildFailed,
                 DesktopApplicationStage::CandidateBuild,
                 std::string("desktop candidate build failed: ") + exception.what())};
  } catch (...) {
    return {{},
            fail(DesktopApplicationErrorCode::CandidateBuildFailed,
                 DesktopApplicationStage::CandidateBuild,
                 "desktop candidate build failed with an unknown exception")};
  }
}

} // namespace

struct DesktopApplication::Impl {
  ApplicationConfig config;
  std::unique_ptr<ApplicationCompletionMailbox> completion_mailbox;
  std::unique_ptr<ApplicationServiceRequestQueue> service_requests;
  std::unique_ptr<ApplicationCommandEngine> command_engine;
  std::unique_ptr<PublishedApplicationState> active;
  std::thread::id owner_thread;
  DesktopApplicationState state = DesktopApplicationState::Ready;
};

struct DesktopApplicationBuilder::Impl {
  explicit Impl(flex::Renderer *renderer) {
    config.renderer = renderer;
    config.registry = WidgetRegistry::builtins();
  }

  ApplicationConfig config;
  DesktopApplicationSources sources;
  std::shared_ptr<const ApplicationServiceRegistry> service_registry;
  ApplicationCapabilityManifest capability_manifest;
};

DesktopApplication::DesktopApplication(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}

DesktopApplication::~DesktopApplication() = default;

Box &DesktopApplication::box() noexcept { return *impl_->active->box; }

const Box &DesktopApplication::box() const noexcept { return *impl_->active->box; }

const CompiledUiProgram &DesktopApplication::program() const noexcept {
  return *impl_->active->program;
}

ScriptController *DesktopApplication::controller() noexcept {
  return impl_->active->controller.get();
}

const ScriptController *DesktopApplication::controller() const noexcept {
  return impl_->active->controller.get();
}

const DesktopApplicationSources &DesktopApplication::sources() const noexcept {
  return impl_->active->sources;
}

bool DesktopApplication::uses_legacy_flex_compatibility() const noexcept {
  return impl_->config.entry_format == UiEntryFormat::LegacyFlexCompatibility;
}

DesktopApplicationState DesktopApplication::state() const noexcept { return impl_->state; }

std::uint64_t DesktopApplication::generation() const noexcept {
  return impl_->completion_mailbox->generation();
}

ApplicationCompletionMailbox &DesktopApplication::completion_mailbox() noexcept {
  return *impl_->completion_mailbox;
}

const ApplicationCompletionMailbox &DesktopApplication::completion_mailbox() const noexcept {
  return *impl_->completion_mailbox;
}

ApplicationServiceRequestResult DesktopApplication::try_receive_service_request() {
  if (!is_owner_thread()) {
    return {ApplicationServicePollStatus::Empty,
            {},
            {ApplicationServiceErrorCode::WrongThread,
             "service request receive must run on the application owner thread",
             {}}};
  }
  if (impl_->state != DesktopApplicationState::Ready) {
    return {ApplicationServicePollStatus::Closed, {}, {}};
  }
  return impl_->service_requests->try_receive_request();
}

ApplicationServiceResolveResult DesktopApplication::resolve_service_request(
    const ApplicationServiceRequest &request) const {
  if (!is_owner_thread()) {
    return {{},
            {ApplicationServiceRegistryErrorCode::WrongThread,
             request.capability, request.operation,
             "service resolution must run on the application owner thread"}};
  }
  if (impl_->state != DesktopApplicationState::Ready) {
    return {{},
            {ApplicationServiceRegistryErrorCode::InvalidState,
             request.capability, request.operation,
             "service resolution requires a ready application"}};
  }
  return impl_->service_requests->resolve_service(request);
}

ApplicationServiceCompletionResult DesktopApplication::try_receive_service_completion() {
  if (!is_owner_thread()) {
    return {ApplicationServicePollStatus::Empty,
            {},
            {ApplicationServiceErrorCode::WrongThread,
             "service completion receive must run on the application owner thread",
             {}}};
  }
  if (impl_->state != DesktopApplicationState::Ready) {
    return {ApplicationServicePollStatus::Closed, {}, {}};
  }

  auto received = impl_->completion_mailbox->try_receive();
  if (received.error) {
    ApplicationServiceError error{ApplicationServiceErrorCode::CompletionMailboxFailed,
                                  received.error.message, std::move(received.error)};
    return {ApplicationServicePollStatus::Empty, {}, std::move(error)};
  }
  if (received.status == ApplicationCompletionReceiveStatus::Empty) {
    return {};
  }
  if (received.status == ApplicationCompletionReceiveStatus::Closed) {
    return {ApplicationServicePollStatus::Closed, {}, {}};
  }
  if (!received.completion.has_value()) {
    return {ApplicationServicePollStatus::Empty,
            {},
            {ApplicationServiceErrorCode::InternalInvariant,
             "completion mailbox returned Ready without a completion",
             {}}};
  }
  return impl_->service_requests->resolve_completion(std::move(*received.completion));
}

ApplicationServiceStatistics DesktopApplication::service_statistics() const noexcept {
  return impl_->service_requests->statistics();
}

bool DesktopApplication::is_owner_thread() const noexcept {
  return std::this_thread::get_id() == impl_->owner_thread;
}

DesktopApplicationResult DesktopApplication::request_close() {
  if (!is_owner_thread()) {
    return {fail(DesktopApplicationErrorCode::WrongThread, DesktopApplicationStage::Lifecycle,
                 "desktop application close request must run on its owner thread")};
  }
  if (impl_->state == DesktopApplicationState::Ready) {
    auto service_closed = impl_->service_requests->close();
    if (!service_closed) {
      auto error = fail(DesktopApplicationErrorCode::ServiceRequestFailed,
                        DesktopApplicationStage::ServiceRequests, service_closed.error.message);
      error.service_error = std::move(service_closed.error);
      return {std::move(error)};
    }
    auto closed = impl_->completion_mailbox->close();
    if (!closed) {
      auto error = fail(DesktopApplicationErrorCode::CompletionMailboxFailed,
                        DesktopApplicationStage::CompletionMailbox, closed.error.message);
      error.completion_error = std::move(closed.error);
      return {std::move(error)};
    }
    impl_->state = DesktopApplicationState::CloseRequested;
  }
  return {};
}

DesktopApplicationResult DesktopApplication::shutdown() {
  if (!is_owner_thread()) {
    return {fail(DesktopApplicationErrorCode::WrongThread, DesktopApplicationStage::Lifecycle,
                 "desktop application shutdown must run on its owner thread")};
  }
  if (impl_->state == DesktopApplicationState::Shutdown) {
    return {};
  }
  if (impl_->state != DesktopApplicationState::CloseRequested) {
    return {fail(DesktopApplicationErrorCode::InvalidState, DesktopApplicationStage::Lifecycle,
                 "desktop application shutdown requires a close request")};
  }

  ScriptController *controller = impl_->active->controller.get();
  if (controller == nullptr) {
    impl_->state = DesktopApplicationState::Shutdown;
    return {};
  }

  auto unmounted = controller->unmount();
  if (unmounted) {
    impl_->state = DesktopApplicationState::Shutdown;
    return {};
  }

  const bool cleanup_completed = controller->state() == ControllerState::Empty;
  if (cleanup_completed) {
    impl_->state = DesktopApplicationState::Shutdown;
  }
  auto error = fail(cleanup_completed ? DesktopApplicationErrorCode::ControllerUnmountFailed
                                      : DesktopApplicationErrorCode::InvalidState,
                    DesktopApplicationStage::ControllerUnmount, unmounted.error.message);
  error.controller_error = std::move(unmounted.error);
  return {std::move(error)};
}

DesktopApplicationResult DesktopApplication::dispatch_event(Event &event) {
  if (!is_owner_thread()) {
    return {fail(DesktopApplicationErrorCode::WrongThread, DesktopApplicationStage::ControllerEvent,
                 "desktop application event dispatch must run on its owner thread")};
  }
  if (impl_->state != DesktopApplicationState::Ready) {
    return {fail(DesktopApplicationErrorCode::InvalidState,
                 DesktopApplicationStage::ControllerEvent,
                 "desktop application event dispatch requires the ready state")};
  }

  try {
    if (impl_->active->event_bridge == nullptr) {
      impl_->active->box->dispatch_event(event);
      return {};
    }
    auto dispatched = impl_->active->event_bridge->dispatch(event);
    if (!dispatched) {
      auto error = fail(DesktopApplicationErrorCode::ControllerDispatchFailed,
                        DesktopApplicationStage::ControllerEvent, dispatched.error.message);
      error.controller_error = std::move(dispatched.error);
      return {std::move(error)};
    }
    return {};
  } catch (const std::exception &exception) {
    return {fail(DesktopApplicationErrorCode::EventDispatchFailed,
                 DesktopApplicationStage::ControllerEvent,
                 std::string("desktop event dispatch failed: ") + exception.what())};
  } catch (...) {
    return {fail(DesktopApplicationErrorCode::EventDispatchFailed,
                 DesktopApplicationStage::ControllerEvent,
                 "desktop event dispatch failed with an unknown exception")};
  }
}

DesktopApplicationResult DesktopApplication::frame(double delta_seconds) {
  if (!is_owner_thread()) {
    return {fail(DesktopApplicationErrorCode::WrongThread, DesktopApplicationStage::ControllerFrame,
                 "desktop application frame must run on its owner thread")};
  }
  if (impl_->state != DesktopApplicationState::Ready) {
    return {fail(DesktopApplicationErrorCode::InvalidState,
                 DesktopApplicationStage::ControllerFrame,
                 "desktop application frame requires the ready state")};
  }
  if (!std::isfinite(delta_seconds) || delta_seconds < 0.0) {
    return {fail(DesktopApplicationErrorCode::InvalidArgument,
                 DesktopApplicationStage::ControllerFrame,
                 "desktop application frame delta must be finite and non-negative")};
  }
  if (impl_->active->controller == nullptr) {
    return {};
  }

  auto framed = impl_->active->controller->frame(delta_seconds);
  if (framed) {
    return {};
  }
  auto error = fail(DesktopApplicationErrorCode::ControllerFrameFailed,
                    DesktopApplicationStage::ControllerFrame, framed.error.message);
  error.controller_error = std::move(framed.error);
  return {std::move(error)};
}

DesktopApplicationResult DesktopApplication::reload(DesktopApplicationSources sources) {
  if (!is_owner_thread()) {
    return {fail(DesktopApplicationErrorCode::WrongThread, DesktopApplicationStage::Configuration,
                 "desktop application reload must run on its owner thread")};
  }
  if (impl_->state != DesktopApplicationState::Ready) {
    return {fail(DesktopApplicationErrorCode::InvalidState, DesktopApplicationStage::Configuration,
                 "desktop application reload requires the ready state")};
  }

  const auto current_generation = generation();
  if (current_generation == std::numeric_limits<std::uint64_t>::max()) {
    return {fail(DesktopApplicationErrorCode::GenerationExhausted,
                 DesktopApplicationStage::CompletionMailbox,
                 "desktop application completion generation exhausted")};
  }

  auto candidate = build_candidate(impl_->config, std::move(sources), impl_->command_engine.get());
  if (!candidate) {
    return {std::move(candidate.error)};
  }
  auto service_reset = impl_->service_requests->validate_generation_reset(current_generation + 1);
  if (!service_reset) {
    auto error = fail(DesktopApplicationErrorCode::ServiceRequestFailed,
                      DesktopApplicationStage::ServiceRequests, service_reset.error.message);
    error.service_error = std::move(service_reset.error);
    return {std::move(error)};
  }
  auto advanced = impl_->completion_mailbox->advance_generation(current_generation + 1);
  if (!advanced) {
    auto error = fail(DesktopApplicationErrorCode::CompletionMailboxFailed,
                      DesktopApplicationStage::CompletionMailbox, advanced.error.message);
    error.completion_error = std::move(advanced.error);
    return {std::move(error)};
  }
  impl_->service_requests->reset_generation(current_generation + 1);
  impl_->active.swap(candidate.state);
  return {};
}

DesktopApplicationBuilder::DesktopApplicationBuilder(flex::Renderer *renderer)
    : impl_(std::make_unique<Impl>(renderer)) {}

DesktopApplicationBuilder::~DesktopApplicationBuilder() = default;

DesktopApplicationBuilder::DesktopApplicationBuilder(DesktopApplicationBuilder &&) noexcept =
    default;

DesktopApplicationBuilder &
DesktopApplicationBuilder::operator=(DesktopApplicationBuilder &&) noexcept = default;

DesktopApplicationBuilder &DesktopApplicationBuilder::xml_entry(std::string source) {
  impl_->config.entry_format = UiEntryFormat::Xml;
  impl_->sources.ui = std::move(source);
  return *this;
}

DesktopApplicationBuilder &
DesktopApplicationBuilder::legacy_flex_entry_compatibility(std::string source) {
  impl_->config.entry_format = UiEntryFormat::LegacyFlexCompatibility;
  impl_->sources.ui = std::move(source);
  return *this;
}

DesktopApplicationBuilder &DesktopApplicationBuilder::stylesheet(std::string source) {
  impl_->sources.stylesheet = std::move(source);
  return *this;
}

DesktopApplicationBuilder &DesktopApplicationBuilder::script(std::string source,
                                                             ScriptModuleFactory factory,
                                                             std::string module_name) {
  impl_->config.script_enabled = true;
  impl_->config.script_factory = std::move(factory);
  impl_->sources.script = std::move(source);
  impl_->sources.script_module_name = std::move(module_name);
  return *this;
}

DesktopApplicationBuilder &DesktopApplicationBuilder::renderer(flex::Renderer *renderer) noexcept {
  if (impl_) {
    impl_->config.renderer = renderer;
  }
  return *this;
}

DesktopApplicationBuilder &DesktopApplicationBuilder::box_options(BoxOptions options) {
  impl_->config.box_options = std::move(options);
  return *this;
}

DesktopApplicationBuilder &DesktopApplicationBuilder::limits(DesktopApplicationLimits limits) {
  impl_->config.limits = std::move(limits);
  return *this;
}

DesktopApplicationBuilder &DesktopApplicationBuilder::widget_registry(WidgetRegistry registry) {
  impl_->config.registry = std::move(registry);
  return *this;
}

DesktopApplicationBuilder &DesktopApplicationBuilder::services(
    std::shared_ptr<const ApplicationServiceRegistry> registry,
    ApplicationCapabilityManifest manifest) {
  impl_->service_registry = std::move(registry);
  impl_->capability_manifest = std::move(manifest);
  return *this;
}

DesktopApplicationBuildResult DesktopApplicationBuilder::build() const {
  if (!impl_) {
    return {{},
            fail(DesktopApplicationErrorCode::InvalidConfiguration,
                 DesktopApplicationStage::Configuration,
                 "moved-from desktop application builder cannot build")};
  }

  std::shared_ptr<const ApplicationServiceRegistry> service_registry =
      impl_->service_registry;
  if (!service_registry) {
    try {
      ApplicationServiceRegistryBuilder registry_builder;
      auto empty_registry = registry_builder.build();
      if (!empty_registry) {
        auto error = fail(DesktopApplicationErrorCode::ServiceRegistryFailed,
                          DesktopApplicationStage::ServiceRegistry,
                          empty_registry.error.message);
        error.registry_error = std::move(empty_registry.error);
        return {{}, std::move(error)};
      }
      service_registry = std::move(empty_registry.registry);
    } catch (const std::bad_alloc &) {
      auto error = fail(DesktopApplicationErrorCode::ServiceRegistryFailed,
                        DesktopApplicationStage::ServiceRegistry,
                        "empty service registry allocation failed");
      error.registry_error = {
          ApplicationServiceRegistryErrorCode::AllocationFailed, {}, {},
          error.message};
      return {{}, std::move(error)};
    } catch (...) {
      auto error = fail(DesktopApplicationErrorCode::ServiceRegistryFailed,
                        DesktopApplicationStage::ServiceRegistry,
                        "empty service registry construction failed");
      error.registry_error = {
          ApplicationServiceRegistryErrorCode::InternalInvariant, {}, {},
          error.message};
      return {{}, std::move(error)};
    }
  }
  auto manifest_validation =
      service_registry->validate_manifest(impl_->capability_manifest);
  if (!manifest_validation) {
    auto error = fail(DesktopApplicationErrorCode::ServiceRegistryFailed,
                      DesktopApplicationStage::ServiceRegistry,
                      manifest_validation.error.message);
    error.registry_error = std::move(manifest_validation.error);
    return {{}, std::move(error)};
  }

  auto completion_mailbox =
      ApplicationCompletionMailbox::create(1, impl_->config.limits.completion);
  if (!completion_mailbox) {
    auto error = fail(DesktopApplicationErrorCode::CompletionMailboxFailed,
                      DesktopApplicationStage::CompletionMailbox, completion_mailbox.error.message);
    error.completion_error = std::move(completion_mailbox.error);
    return {{}, std::move(error)};
  }

  auto service_requests =
      ApplicationServiceRequestQueue::create(
          1, impl_->config.limits.service_requests, std::move(service_registry),
          impl_->capability_manifest);
  if (!service_requests) {
    auto error = fail(DesktopApplicationErrorCode::ServiceRequestFailed,
                      DesktopApplicationStage::ServiceRequests, service_requests.error.message);
    error.service_error = std::move(service_requests.error);
    return {{}, std::move(error)};
  }

  std::unique_ptr<ApplicationCommandEngine> command_engine;
  try {
    command_engine = std::make_unique<ApplicationCommandEngine>(*service_requests.queue);
  } catch (const std::bad_alloc &) {
    auto error =
        fail(DesktopApplicationErrorCode::ServiceRequestFailed,
             DesktopApplicationStage::ServiceRequests, "service command engine allocation failed");
    error.service_error = {ApplicationServiceErrorCode::AllocationFailed, error.message, {}};
    return {{}, std::move(error)};
  }

  auto candidate = build_candidate(impl_->config, impl_->sources, command_engine.get());
  if (!candidate) {
    return {{}, std::move(candidate.error)};
  }

  try {
    auto application_impl = std::make_unique<DesktopApplication::Impl>();
    application_impl->config = impl_->config;
    application_impl->completion_mailbox = std::move(completion_mailbox.mailbox);
    application_impl->service_requests = std::move(service_requests.queue);
    application_impl->command_engine = std::move(command_engine);
    application_impl->active = std::move(candidate.state);
    application_impl->owner_thread = std::this_thread::get_id();
    return {
        std::unique_ptr<DesktopApplication>(new DesktopApplication(std::move(application_impl))),
        {}};
  } catch (const std::exception &exception) {
    return {{},
            fail(DesktopApplicationErrorCode::CandidateBuildFailed,
                 DesktopApplicationStage::CandidateBuild,
                 std::string("desktop application publication failed: ") + exception.what())};
  } catch (...) {
    return {{},
            fail(DesktopApplicationErrorCode::CandidateBuildFailed,
                 DesktopApplicationStage::CandidateBuild,
                 "desktop application publication failed with an unknown exception")};
  }
}

} // namespace flexUI
