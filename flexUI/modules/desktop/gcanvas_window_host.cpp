#include "flexUI/gcanvas_window_host.h"

#include "flexUI/host_bridge.h"
#include "flex/render/engines/gcanvas.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <exception>
#include <limits>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

namespace flexUI {
namespace {

constexpr double kMillisecondsPerSecond = 1000.0;
constexpr std::size_t kNativeListenerCount = 8;

void wake_gcanvas_event_loop(void *) noexcept {
  gcanvas::Window::trigger_events();
}

GCanvasWindowHostError fail(GCanvasWindowHostErrorCode code,
                            GCanvasWindowHostStage stage,
                            std::string message) {
  return {code, stage, std::move(message)};
}

GCanvasWindowHostError validate_config(const GCanvasWindowHostConfig &config) {
  if (config.window.title == nullptr) {
    return fail(GCanvasWindowHostErrorCode::InvalidConfiguration,
                GCanvasWindowHostStage::Configuration,
                "gCanvas window title pointer must not be null");
  }
  if (config.window.width <= 0 || config.window.height <= 0) {
    return fail(GCanvasWindowHostErrorCode::InvalidConfiguration,
                GCanvasWindowHostStage::Configuration,
                "gCanvas window dimensions must be positive");
  }
  if (!std::isfinite(config.window.x_scale) ||
      !std::isfinite(config.window.y_scale) || config.window.x_scale <= 0.0F ||
      config.window.y_scale <= 0.0F) {
    return fail(GCanvasWindowHostErrorCode::InvalidConfiguration,
                GCanvasWindowHostStage::Configuration,
                "gCanvas window scales must be finite and positive");
  }
  if (config.window.backend != gcanvas::Backend::OpenGL &&
      config.window.backend != gcanvas::Backend::Vulkan) {
    return fail(GCanvasWindowHostErrorCode::InvalidConfiguration,
                GCanvasWindowHostStage::Configuration,
                "gCanvas window host requires an OpenGL or Vulkan backend");
  }
  if (!std::isfinite(config.continuous_interval_seconds) ||
      config.continuous_interval_seconds <= 0.0 ||
      config.continuous_interval_seconds >
          static_cast<double>((std::numeric_limits<float>::max)())) {
    return fail(GCanvasWindowHostErrorCode::InvalidConfiguration,
                GCanvasWindowHostStage::Configuration,
                "continuous frame interval must be finite, positive, and fit in float");
  }
  constexpr double kMaximumFloatDeltaSeconds =
      static_cast<double>((std::numeric_limits<float>::max)()) /
      kMillisecondsPerSecond;
  if (!std::isfinite(config.max_frame_delta_seconds) ||
      config.max_frame_delta_seconds <= 0.0 ||
      config.max_frame_delta_seconds > kMaximumFloatDeltaSeconds) {
    return fail(GCanvasWindowHostErrorCode::InvalidConfiguration,
                GCanvasWindowHostStage::Configuration,
                "maximum frame delta must be finite, positive, and convertible to milliseconds");
  }
  if (config.max_service_completions_per_pump == 0 ||
      config.max_service_completions_per_pump >
          GCanvasWindowHostConfig::kMaximumServiceCompletionsPerPump) {
    return fail(GCanvasWindowHostErrorCode::InvalidConfiguration,
                GCanvasWindowHostStage::Configuration,
                "service completions per pump must be within the supported range");
  }
  if (config.max_service_requests_per_pump == 0 ||
      config.max_service_requests_per_pump >
          GCanvasWindowHostConfig::kMaximumServiceRequestsPerPump) {
    return fail(GCanvasWindowHostErrorCode::InvalidConfiguration,
                GCanvasWindowHostStage::Configuration,
                "service requests per pump must be within the supported range");
  }
  return {};
}

GCanvasWindowHostError application_failure(GCanvasWindowHostErrorCode code,
                                            GCanvasWindowHostStage stage,
                                            DesktopApplicationError error,
                                            std::string message) {
  auto host_error = fail(code, stage, std::move(message));
  host_error.application_error = std::move(error);
  return host_error;
}

GCanvasWindowHostError input_failure(GCanvasApplicationInputError error) {
  auto host_error = fail(GCanvasWindowHostErrorCode::InputFailed,
                         GCanvasWindowHostStage::NativeEvent,
                         "gCanvas native input routing failed");
  host_error.input_error = std::move(error);
  return host_error;
}

GCanvasWindowHostError dispatcher_failure(
    ApplicationServiceDispatcherError error) {
  auto host_error = fail(GCanvasWindowHostErrorCode::ServiceDispatchFailed,
                         GCanvasWindowHostStage::ServiceDispatch,
                         "desktop application service request dispatch failed");
  host_error.service_dispatcher_error = std::move(error);
  return host_error;
}

} // namespace

struct GCanvasWindowHost::Impl {
  explicit Impl(GCanvasWindowHostConfig value)
      : config(std::move(value)), owner_thread(std::this_thread::get_id()) {}
  ~Impl() { teardown_noexcept(); }

  GCanvasWindowHostConfig config;
  std::thread::id owner_thread;
  std::unique_ptr<gcanvas::Window> window;
  gcanvas::Context *context = nullptr;
  std::unique_ptr<flex::Renderer> renderer;
  std::unique_ptr<DesktopApplication> application;
  std::unique_ptr<ApplicationServiceDispatcher> service_dispatcher;
  std::unique_ptr<GCanvasApplicationInputRouter> input;
  std::vector<gcanvas::WindowListenerSubscription> subscriptions;
  std::optional<GCanvasWindowHostError> pending_error;
  bool running = false;

  bool is_owner_thread() const noexcept {
    return std::this_thread::get_id() == owner_thread;
  }

  void release_capture_and_focus() noexcept {
    if (application != nullptr) {
      host::clear_focus_and_capture(&application->box());
    }
    if (window != nullptr && window->has_pointer_capture()) {
      try {
        window->set_pointer_capture(false);
      } catch (...) {
      }
    }
  }

  void request_close_after_failure() noexcept {
    release_capture_and_focus();
    if (application != nullptr && application->state() == DesktopApplicationState::Ready) {
      static_cast<void>(application->request_close());
    }
    if (window != nullptr) {
      try {
        window->close();
      } catch (...) {
      }
    }
  }

  void record_failure(GCanvasWindowHostError error) noexcept {
    if (!pending_error.has_value()) {
      pending_error = std::move(error);
    }
    request_close_after_failure();
  }

  GCanvasWindowHostResult sync_pointer_capture() {
    try {
      host::sync_mouse_capture(
          &application->box(), window->has_pointer_capture(),
          [this] { window->set_pointer_capture(true); },
          [this] { window->set_pointer_capture(false); });
      return {};
    } catch (const std::exception &exception) {
      return {fail(GCanvasWindowHostErrorCode::PointerCaptureFailed,
                   GCanvasWindowHostStage::PointerCapture,
                   std::string("gCanvas pointer capture synchronization failed: ") +
                       exception.what())};
    } catch (...) {
      return {fail(GCanvasWindowHostErrorCode::PointerCaptureFailed,
                   GCanvasWindowHostStage::PointerCapture,
                   "gCanvas pointer capture synchronization failed with an unknown exception")};
    }
  }

  void receive_input(GCanvasApplicationInputResult result,
                     bool synchronize_capture = false) noexcept {
    if (!result) {
      record_failure(input_failure(std::move(result.error)));
      return;
    }
    if (synchronize_capture &&
        application->state() == DesktopApplicationState::Ready) {
      auto synchronized = sync_pointer_capture();
      if (!synchronized) {
        record_failure(std::move(synchronized.error));
      }
    }
  }

  template <typename Callback>
  void native_callback(Callback &&callback) noexcept {
    if (application->state() != DesktopApplicationState::Ready ||
        pending_error.has_value()) {
      return;
    }
    try {
      std::forward<Callback>(callback)();
    } catch (const std::exception &exception) {
      record_failure(fail(
          GCanvasWindowHostErrorCode::NativeEventFailed,
          GCanvasWindowHostStage::NativeEvent,
          std::string("gCanvas native event callback failed: ") + exception.what()));
    } catch (...) {
      record_failure(fail(
          GCanvasWindowHostErrorCode::NativeEventFailed,
          GCanvasWindowHostStage::NativeEvent,
          "gCanvas native event callback failed with an unknown exception"));
    }
  }

  void register_listeners() {
    subscriptions.reserve(kNativeListenerCount);
    subscriptions.emplace_back(window->subscribe_resize_listener(
        [this](gcanvas::resize_event event) {
          native_callback([&] { receive_input(input->resize(event)); });
        }));
    subscriptions.emplace_back(window->subscribe_mouse_move_listener(
        [this](gcanvas::mouse_move_event event) {
          native_callback(
              [&] { receive_input(input->mouse_move(event), true); });
        }));
    subscriptions.emplace_back(window->subscribe_mouse_click_listener(
        [this](gcanvas::mouse_button_event event) {
          native_callback(
              [&] { receive_input(input->mouse_button(event), true); });
        }));
    subscriptions.emplace_back(window->subscribe_key_listener(
        [this](gcanvas::key_event event) {
          native_callback([&] { receive_input(input->key(event)); });
        }));
    subscriptions.emplace_back(window->subscribe_char_listener(
        [this](gcanvas::char_event event) {
          native_callback([&] { receive_input(input->character(event)); });
        }));
    subscriptions.emplace_back(window->subscribe_scroll_listener(
        [this](gcanvas::scroll_event event) {
          native_callback([&] { receive_input(input->scroll(event)); });
        }));
    subscriptions.emplace_back(window->subscribe_focus_listener(
        [this](gcanvas::focus_event event) {
          native_callback([&] {
            receive_input(input->focus(event));
            if (!event.focused && window->has_pointer_capture()) {
              window->set_pointer_capture(false);
            }
          });
        }));
    subscriptions.emplace_back(window->subscribe_close_listener(
        [this](gcanvas::close_event) {
          native_callback([&] {
            release_capture_and_focus();
            auto closed = application->request_close();
            if (!closed) {
              record_failure(application_failure(
                  GCanvasWindowHostErrorCode::InvalidState,
                  GCanvasWindowHostStage::Shutdown,
                  std::move(closed.error),
                  "desktop application rejected the native close request"));
            }
          });
        }));
  }

  GCanvasWindowHostResult finish_shutdown() {
    if (application->state() == DesktopApplicationState::Shutdown) {
      return {};
    }
    if (application->state() != DesktopApplicationState::CloseRequested) {
      return {fail(GCanvasWindowHostErrorCode::InvalidState,
                   GCanvasWindowHostStage::Shutdown,
                   "gCanvas host shutdown requires a close request")};
    }
    release_capture_and_focus();
    auto shutdown = application->shutdown();
    if (!shutdown) {
      return {application_failure(GCanvasWindowHostErrorCode::ShutdownFailed,
                                  GCanvasWindowHostStage::Shutdown,
                                  std::move(shutdown.error),
                                  "desktop application shutdown failed")};
    }
    return {};
  }

  GCanvasWindowHostResult finish_pending_error() {
    GCanvasWindowHostError primary = std::move(*pending_error);
    pending_error.reset();
    auto shutdown = finish_shutdown();
    if (!shutdown) {
      primary.message += "; shutdown also failed: " + shutdown.error.message;
    }
    return {std::move(primary)};
  }

  GCanvasWindowHostResult drain_service_completions() {
    ScriptController *active_controller = application->controller();
    if (active_controller == nullptr ||
        !active_controller->has_service_completion_handler()) {
      return {};
    }

    std::size_t consumed = 0;
    while (consumed < config.max_service_completions_per_pump) {
      auto dispatched = application->try_dispatch_service_completion();
      if (dispatched.error) {
        return {application_failure(
            GCanvasWindowHostErrorCode::ServiceCompletionFailed,
            GCanvasWindowHostStage::ServiceCompletion,
            std::move(dispatched.error),
            "desktop application service completion dispatch failed")};
      }
      if (dispatched.status == ApplicationServicePollStatus::Empty) {
        return {};
      }
      if (dispatched.status == ApplicationServicePollStatus::Closed) {
        return {fail(GCanvasWindowHostErrorCode::InvalidState,
                     GCanvasWindowHostStage::ServiceCompletion,
                     "ready desktop application exposed a closed completion mailbox")};
      }
      if (!dispatched.completion.has_value() || !dispatched.dispatched) {
        return {fail(GCanvasWindowHostErrorCode::InvalidState,
                     GCanvasWindowHostStage::ServiceCompletion,
                     "scripted completion dispatch violated its ready-state contract")};
      }
      ++consumed;
      if (application->state() != DesktopApplicationState::Ready) {
        return {};
      }
    }

    // The mailbox may still contain records. A redundant empty event is safe
    // and prevents the bounded owner-thread loop from sleeping indefinitely.
    gcanvas::Window::trigger_events();
    return {};
  }

  GCanvasWindowHostResult dispatch_service_requests() {
    auto dispatched = service_dispatcher->pump();
    if (!dispatched) {
      return {dispatcher_failure(std::move(dispatched.error))};
    }
    if (dispatched.status == ApplicationServiceDispatchStatus::Closed &&
        application->state() == DesktopApplicationState::Ready) {
      return {fail(GCanvasWindowHostErrorCode::InvalidState,
                   GCanvasWindowHostStage::ServiceDispatch,
                   "ready desktop application exposed a closed service dispatcher")};
    }
    if (dispatched.status == ApplicationServiceDispatchStatus::Blocked ||
        dispatched.submitted + dispatched.failed >=
            config.max_service_requests_per_pump) {
      // Retry bounded work on the next event-loop turn. An empty event is safe
      // even when a synchronous endpoint already issued a completion wakeup.
      gcanvas::Window::trigger_events();
    }
    return {};
  }

  GCanvasWindowHostResult drive_frame(double delta_seconds) {
    if (pending_error.has_value()) {
      return finish_pending_error();
    }
    if (application->state() == DesktopApplicationState::Shutdown) {
      return {};
    }
    if (application->state() == DesktopApplicationState::CloseRequested) {
      return finish_shutdown();
    }
    if (!window->is_running()) {
      auto closed = application->request_close();
      if (!closed) {
        return {application_failure(GCanvasWindowHostErrorCode::InvalidState,
                                    GCanvasWindowHostStage::Shutdown,
                                    std::move(closed.error),
                                    "desktop application rejected window closure")};
      }
      return finish_shutdown();
    }

    auto drained = drain_service_completions();
    if (!drained) {
      record_failure(std::move(drained.error));
      return finish_pending_error();
    }
    if (application->state() != DesktopApplicationState::Ready) {
      return drive_frame(0.0);
    }

    auto dispatched = dispatch_service_requests();
    if (!dispatched) {
      record_failure(std::move(dispatched.error));
      return finish_pending_error();
    }
    if (application->state() != DesktopApplicationState::Ready) {
      return drive_frame(0.0);
    }

    auto frame = application->frame(delta_seconds);
    if (!frame) {
      auto error = application_failure(
          GCanvasWindowHostErrorCode::FrameFailed,
          GCanvasWindowHostStage::Frame, std::move(frame.error),
          "desktop application frame callback failed");
      record_failure(std::move(error));
      return finish_pending_error();
    }
    if (application->state() != DesktopApplicationState::Ready) {
      return drive_frame(0.0);
    }

    try {
      application->box().update_time(
          static_cast<float>(delta_seconds * kMillisecondsPerSecond));
      application->box().update();
      context->present_frame();
    } catch (const std::exception &exception) {
      record_failure(fail(GCanvasWindowHostErrorCode::FrameFailed,
                          GCanvasWindowHostStage::Frame,
                          std::string("FlexUI GPU frame failed: ") + exception.what()));
      return finish_pending_error();
    } catch (...) {
      record_failure(fail(
          GCanvasWindowHostErrorCode::FrameFailed,
          GCanvasWindowHostStage::Frame,
          "FlexUI GPU frame failed with an unknown exception"));
      return finish_pending_error();
    }

    auto synchronized = sync_pointer_capture();
    if (!synchronized) {
      record_failure(std::move(synchronized.error));
      return finish_pending_error();
    }
    return {};
  }

  void teardown_noexcept() noexcept {
    subscriptions.clear();
    if (!is_owner_thread()) {
      return;
    }
    release_capture_and_focus();
    if (application != nullptr &&
        application->state() == DesktopApplicationState::Ready) {
      static_cast<void>(application->request_close());
    }
    if (application != nullptr &&
        application->state() == DesktopApplicationState::CloseRequested) {
      static_cast<void>(application->shutdown());
    }
  }
};

GCanvasWindowHost::GCanvasWindowHost(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

GCanvasWindowHost::~GCanvasWindowHost() = default;

GCanvasWindowHostBuildResult
GCanvasWindowHost::create(GCanvasWindowHostConfig config,
                          DesktopApplicationBuilder builder) {
  if (auto error = validate_config(config); error) {
    return {{}, std::move(error)};
  }

  std::unique_ptr<Impl> impl;
  try {
    impl = std::make_unique<Impl>(std::move(config));
    impl->window = gcanvas::Window::create(impl->config.window);
    if (impl->window == nullptr) {
      return {{}, fail(GCanvasWindowHostErrorCode::WindowCreationFailed,
                       GCanvasWindowHostStage::WindowCreation,
                       "gCanvas returned a null window")};
    }
  } catch (const std::exception &exception) {
    return {{}, fail(GCanvasWindowHostErrorCode::WindowCreationFailed,
                     GCanvasWindowHostStage::WindowCreation,
                     std::string("gCanvas window creation failed: ") +
                         exception.what())};
  } catch (...) {
    return {{}, fail(GCanvasWindowHostErrorCode::WindowCreationFailed,
                     GCanvasWindowHostStage::WindowCreation,
                     "gCanvas window creation failed with an unknown exception")};
  }

  try {
    impl->context = &impl->window->create_context();
  } catch (const std::exception &exception) {
    return {{}, fail(GCanvasWindowHostErrorCode::ContextCreationFailed,
                     GCanvasWindowHostStage::ContextCreation,
                     std::string("gCanvas context creation failed: ") +
                         exception.what())};
  } catch (...) {
    return {{}, fail(GCanvasWindowHostErrorCode::ContextCreationFailed,
                     GCanvasWindowHostStage::ContextCreation,
                     "gCanvas context creation failed with an unknown exception")};
  }

  try {
    impl->renderer =
        flex::render::engines::gcanvas::create_renderer(*impl->context);
    if (impl->renderer == nullptr) {
      return {{}, fail(GCanvasWindowHostErrorCode::RendererCreationFailed,
                       GCanvasWindowHostStage::RendererCreation,
                       "Flex gCanvas adapter returned a null renderer")};
    }
  } catch (const std::exception &exception) {
    return {{}, fail(GCanvasWindowHostErrorCode::RendererCreationFailed,
                     GCanvasWindowHostStage::RendererCreation,
                     std::string("Flex gCanvas renderer creation failed: ") +
                         exception.what())};
  } catch (...) {
    return {{}, fail(GCanvasWindowHostErrorCode::RendererCreationFailed,
                     GCanvasWindowHostStage::RendererCreation,
                     "Flex gCanvas renderer creation failed with an unknown exception")};
  }

  auto built = builder.renderer(impl->renderer.get())
                   .completion_wakeup(
                       {&wake_gcanvas_event_loop, nullptr})
                   .build();
  if (!built) {
    return {{}, application_failure(
                    GCanvasWindowHostErrorCode::ApplicationBuildFailed,
                    GCanvasWindowHostStage::ApplicationBuild,
                    std::move(built.error),
                    "FlexUI desktop application build failed")};
  }
  impl->application = std::move(built.application);

  auto dispatcher = ApplicationServiceDispatcher::create(
      *impl->application,
      {impl->config.max_service_requests_per_pump});
  if (!dispatcher) {
    return {{}, dispatcher_failure(std::move(dispatcher.error))};
  }
  impl->service_dispatcher = std::move(dispatcher.dispatcher);

  try {
    impl->input = std::make_unique<GCanvasApplicationInputRouter>(
        *impl->application);
    auto resized = impl->input->resize(
        {impl->window->get_width(), impl->window->get_height()});
    if (!resized) {
      return {{}, input_failure(std::move(resized.error))};
    }
    impl->register_listeners();
  } catch (const std::exception &exception) {
    return {{}, fail(GCanvasWindowHostErrorCode::ListenerRegistrationFailed,
                     GCanvasWindowHostStage::ListenerRegistration,
                     std::string("gCanvas listener registration failed: ") +
                         exception.what())};
  } catch (...) {
    return {{}, fail(GCanvasWindowHostErrorCode::ListenerRegistrationFailed,
                     GCanvasWindowHostStage::ListenerRegistration,
                     "gCanvas listener registration failed with an unknown exception")};
  }

  try {
    return {std::unique_ptr<GCanvasWindowHost>(
                new GCanvasWindowHost(std::move(impl))),
            {}};
  } catch (const std::exception &exception) {
    return {{}, fail(GCanvasWindowHostErrorCode::ApplicationBuildFailed,
                     GCanvasWindowHostStage::ApplicationBuild,
                     std::string("gCanvas window host allocation failed: ") +
                         exception.what())};
  }
}

DesktopApplication &GCanvasWindowHost::application() noexcept {
  return *impl_->application;
}

const DesktopApplication &GCanvasWindowHost::application() const noexcept {
  return *impl_->application;
}

bool GCanvasWindowHost::is_owner_thread() const noexcept {
  return impl_->is_owner_thread();
}

gcanvas::Backend GCanvasWindowHost::backend() const noexcept {
  return impl_->window->get_backend();
}

bool GCanvasWindowHost::has_native_pointer_capture() const noexcept {
  return impl_->window->has_pointer_capture();
}

GCanvasWindowHostResult GCanvasWindowHost::pump_once(double delta_seconds) {
  if (!is_owner_thread()) {
    return {fail(GCanvasWindowHostErrorCode::WrongThread,
                 GCanvasWindowHostStage::Frame,
                 "gCanvas window host pump must run on its owner thread")};
  }
  if (!std::isfinite(delta_seconds) || delta_seconds < 0.0 ||
      delta_seconds > impl_->config.max_frame_delta_seconds) {
    return {fail(GCanvasWindowHostErrorCode::InvalidConfiguration,
                 GCanvasWindowHostStage::Frame,
                 "frame delta must be finite, non-negative, and within the configured maximum")};
  }
  if (impl_->application->state() != DesktopApplicationState::Ready) {
    return impl_->drive_frame(delta_seconds);
  }

  try {
    impl_->window->poll_events();
  } catch (const std::exception &exception) {
    impl_->record_failure(fail(
        GCanvasWindowHostErrorCode::NativeEventFailed,
        GCanvasWindowHostStage::NativeEvent,
        std::string("gCanvas event polling failed: ") + exception.what()));
  } catch (...) {
    impl_->record_failure(fail(
        GCanvasWindowHostErrorCode::NativeEventFailed,
        GCanvasWindowHostStage::NativeEvent,
        "gCanvas event polling failed with an unknown exception"));
  }
  return impl_->drive_frame(delta_seconds);
}

GCanvasWindowHostResult GCanvasWindowHost::run() {
  if (!is_owner_thread()) {
    return {fail(GCanvasWindowHostErrorCode::WrongThread,
                 GCanvasWindowHostStage::Frame,
                 "gCanvas window host loop must run on its owner thread")};
  }
  if (impl_->running) {
    return {fail(GCanvasWindowHostErrorCode::InvalidState,
                 GCanvasWindowHostStage::Frame,
                 "gCanvas window host loop cannot be re-entered")};
  }
  if (impl_->application->state() != DesktopApplicationState::Ready) {
    return impl_->drive_frame(0.0);
  }

  struct RunningScope final {
    explicit RunningScope(bool &running) : running_(running) { running_ = true; }
    ~RunningScope() { running_ = false; }
    bool &running_;
  } running_scope(impl_->running);

  auto initial = pump_once(0.0);
  if (!initial || impl_->application->state() != DesktopApplicationState::Ready) {
    return initial;
  }

  auto previous = std::chrono::steady_clock::now();
  while (impl_->application->state() == DesktopApplicationState::Ready &&
         impl_->window->is_running()) {
    try {
      if (impl_->config.frame_mode == GCanvasWindowHostFrameMode::EventDriven) {
        impl_->window->wait_events();
      } else {
        impl_->window->wait_events(
            static_cast<float>(impl_->config.continuous_interval_seconds));
      }
    } catch (const std::exception &exception) {
      impl_->record_failure(fail(
          GCanvasWindowHostErrorCode::NativeEventFailed,
          GCanvasWindowHostStage::NativeEvent,
          std::string("gCanvas event wait failed: ") + exception.what()));
    } catch (...) {
      impl_->record_failure(fail(
          GCanvasWindowHostErrorCode::NativeEventFailed,
          GCanvasWindowHostStage::NativeEvent,
          "gCanvas event wait failed with an unknown exception"));
    }

    const auto now = std::chrono::steady_clock::now();
    const double elapsed = std::chrono::duration<double>(now - previous).count();
    previous = now;
    auto framed = impl_->drive_frame(
        (std::min)(elapsed, impl_->config.max_frame_delta_seconds));
    if (!framed) {
      return framed;
    }
  }

  if (impl_->application->state() == DesktopApplicationState::Ready) {
    auto closed = request_close();
    if (!closed) {
      return closed;
    }
  }
  return impl_->drive_frame(0.0);
}

GCanvasWindowHostResult GCanvasWindowHost::request_close() {
  if (!is_owner_thread()) {
    return {fail(GCanvasWindowHostErrorCode::WrongThread,
                 GCanvasWindowHostStage::Shutdown,
                 "gCanvas window host close request must run on its owner thread")};
  }
  if (impl_->application->state() == DesktopApplicationState::Shutdown) {
    return {};
  }

  impl_->release_capture_and_focus();
  auto closed = impl_->application->request_close();
  if (!closed) {
    return {application_failure(GCanvasWindowHostErrorCode::InvalidState,
                                GCanvasWindowHostStage::Shutdown,
                                std::move(closed.error),
                                "desktop application rejected the close request")};
  }
  try {
    impl_->window->close();
  } catch (const std::exception &exception) {
    return {fail(GCanvasWindowHostErrorCode::NativeEventFailed,
                 GCanvasWindowHostStage::Shutdown,
                 std::string("gCanvas window close failed: ") + exception.what())};
  } catch (...) {
    return {fail(GCanvasWindowHostErrorCode::NativeEventFailed,
                 GCanvasWindowHostStage::Shutdown,
                 "gCanvas window close failed with an unknown exception")};
  }
  return {};
}

} // namespace flexUI
