#include <flexUI/gcanvas_window_host.h>

#include <tinytest.hpp>

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

constexpr int kTestWidth = 96;
constexpr int kTestHeight = 64;

struct RunProbe {
  flexUI::GCanvasWindowHost *host = nullptr;
  flexUI::GCanvasWindowHostResult close_result;
  std::size_t frame_calls = 0;
};

struct CompletionProbe {
  std::size_t command_count = 1;
  std::vector<std::uint64_t> submitted_ids;
  std::vector<std::uint64_t> completion_ids;
  bool enable_completion_handler = true;
  bool fail_completion = false;
};

class CompletionEndpoint final : public flexUI::IApplicationServiceEndpoint {
public:
  explicit CompletionEndpoint(std::shared_ptr<CompletionProbe> probe)
      : probe_(std::move(probe)) {}

  flexUI::ApplicationServiceSubmitResult try_submit(
      const flexUI::ApplicationServiceRequest &request,
      flexUI::IApplicationServiceCompletionSink &completion_sink) override {
    probe_->submitted_ids.push_back(request.script_request_id);
    flexUI::ApplicationCompletion completion;
    completion.token = request.token;
    completion.payload =
        "automatic-" + std::to_string(request.script_request_id);
    const auto posted = completion_sink.try_post(completion);
    if (!posted) {
      return {{flexUI::ApplicationServiceSubmitErrorCode::InternalFailure,
               posted.error.message}};
    }
    return {};
  }

private:
  std::shared_ptr<CompletionProbe> probe_;
};

class CompletionModule final : public flexUI::IScriptModule {
public:
  explicit CompletionModule(std::shared_ptr<CompletionProbe> probe)
      : probe_(std::move(probe)) {}

  flexUI::ScriptResolveResult resolve_export(
      std::string_view name, flexUI::ScriptCallbackKind callback) override {
    if (name == "on_frame" && callback == flexUI::ScriptCallbackKind::Frame) {
      return {flexUI::ScriptExportHandle{1}, {}};
    }
    if (probe_->enable_completion_handler &&
        name == "on_service_completion" &&
        callback == flexUI::ScriptCallbackKind::ServiceCompletion) {
      return {flexUI::ScriptExportHandle{2}, {}};
    }
    return {};
  }

  flexUI::ScriptCallResult call(
      flexUI::ScriptExportHandle handle,
      const flexUI::ScriptCallContext &context) override {
    flexUI::ScriptCallResult result;
    if (handle.value == 1 && context.callback == flexUI::ScriptCallbackKind::Frame) {
      if (!commands_emitted_) {
        for (std::size_t index = 0; index < probe_->command_count; ++index) {
          const auto appended = result.commands.append(
              {static_cast<std::uint64_t>(101 + index), "storage/1", "write",
               "document"});
          if (!appended) {
            return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded,
                     appended.error.message}};
          }
        }
        commands_emitted_ = true;
      }
      return result;
    }
    if (handle.value == 2 &&
        context.callback == flexUI::ScriptCallbackKind::ServiceCompletion &&
        context.service_completion != nullptr) {
      if (probe_->fail_completion) {
        return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
                 "injected host completion failure"}};
      }
      probe_->completion_ids.push_back(
          context.service_completion->script_request_id);
      const auto appended = result.mutations.append(
          flexUI::SetTextMutation{{"surface", 1},
                                  context.service_completion->payload});
      if (!appended) {
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded,
                 appended.error.message}};
      }
      return result;
    }
    return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
             "invalid completion host test call"}};
  }

private:
  std::shared_ptr<CompletionProbe> probe_;
  bool commands_emitted_ = false;
};

class CloseOnFrameModule final : public flexUI::IScriptModule {
public:
  explicit CloseOnFrameModule(std::shared_ptr<RunProbe> probe)
      : probe_(std::move(probe)) {}

  flexUI::ScriptResolveResult
  resolve_export(std::string_view name,
                 flexUI::ScriptCallbackKind callback) override {
    if (name == "on_frame" && callback == flexUI::ScriptCallbackKind::Frame) {
      return {flexUI::ScriptExportHandle{1}, {}};
    }
    return {};
  }

  flexUI::ScriptCallResult
  call(flexUI::ScriptExportHandle handle,
       const flexUI::ScriptCallContext &context) override {
    if (handle.value != 1 || context.callback != flexUI::ScriptCallbackKind::Frame ||
        probe_->host == nullptr) {
      return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
               "invalid close-on-frame test call"}};
    }
    ++probe_->frame_calls;
    probe_->close_result = probe_->host->request_close();
    return {};
  }

private:
  std::shared_ptr<RunProbe> probe_;
};

flexUI::GCanvasWindowHostConfig host_config(gcanvas::Backend backend,
                                             bool native_pixel_size = true) {
  flexUI::GCanvasWindowHostConfig config;
  config.window.title = "FlexUI gCanvas host test";
  config.window.width = kTestWidth;
  config.window.height = kTestHeight;
  config.window.visible = false;
  config.window.decorated = false;
  config.window.resizeable = false;
  config.window.vsync = false;
  config.window.native_pixel_size = native_pixel_size;
  config.window.backend = backend;
  return config;
}

flexUI::DesktopApplicationBuilder application_builder() {
  flexUI::DesktopApplicationBuilder builder;
  builder
      .xml_entry("<ui name=\"HostTest\"><div id=\"surface\"/></ui>")
      .stylesheet("#surface { width: 48px; height: 32px; background: #336699; }");
  return builder;
}

flexUI::DesktopApplicationBuilder completion_application_builder(
    const std::shared_ptr<CompletionProbe> &probe) {
  auto builder = application_builder();
  auto endpoint = std::make_shared<CompletionEndpoint>(probe);
  flexUI::ApplicationServiceRegistryBuilder registry_builder;
  check(registry_builder.register_service(
      {"storage/1", {{"write", 1024}}}, endpoint));
  auto registry = registry_builder.build();
  check(registry);
  builder.services(std::move(registry.registry),
                   {{"storage/1"}, {"storage/1"}})
      .script("completion-host-test",
              [probe](std::string_view, std::string_view) {
                return flexUI::ScriptModuleFactoryResult{
                    std::make_unique<CompletionModule>(probe), {}};
              });
  return builder;
}

flexUI::ApplicationCompletionPostResult post_completion_from_worker(
    flexUI::DesktopApplication &application,
    const flexUI::ApplicationServiceRequest &request, std::string payload) {
  flexUI::ApplicationCompletion completion;
  completion.token = request.token;
  completion.payload = std::move(payload);
  flexUI::ApplicationCompletionPostResult posted;
  std::thread worker([&] {
    posted = application.completion_mailbox().try_post(completion);
  });
  worker.join();
  return posted;
}

void exercise_backend(gcanvas::Backend backend) {
  auto built = flexUI::GCanvasWindowHost::create(
      host_config(backend), application_builder());
  check(static_cast<bool>(built));
  check_not_null(built.host.get());
  if (!built) {
    return;
  }

  check(built.host->backend() == backend);
  check_within(built.host->application().box().viewport_width(),
               static_cast<float>(kTestWidth), 0.001F);
  check_within(built.host->application().box().viewport_height(),
               static_cast<float>(kTestHeight), 0.001F);
  check(built.host->pump_once(0.0));

#ifdef _WIN32
  auto *surface = built.host->application().box().get_by_id("surface");
  check_not_null(surface);
  built.host->application().box().set_mouse_capture(surface);
  check(built.host->pump_once(0.0));
  check_true(built.host->has_native_pointer_capture());
  built.host->application().box().release_mouse_capture(surface);
  check(built.host->pump_once(0.0));
  check_false(built.host->has_native_pointer_capture());
#endif

  check(built.host->request_close());
  check(built.host->application().state() ==
        flexUI::DesktopApplicationState::CloseRequested);
  check(built.host->pump_once(0.0));
  check(built.host->application().state() ==
        flexUI::DesktopApplicationState::Shutdown);
}

} // namespace

spec("FlexUI gCanvas window host owns the GPU desktop lifecycle") {
  it("drives a hidden OpenGL application") {
    exercise_backend(gcanvas::Backend::OpenGL);
  }

  it("drives a hidden Vulkan application") {
    exercise_backend(gcanvas::Backend::Vulkan);
  }

  it("keeps a DPI-scaled OpenGL viewport in logical coordinates") {
    auto built = flexUI::GCanvasWindowHost::create(
        host_config(gcanvas::Backend::OpenGL, false), application_builder());
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check_within(built.host->application().box().viewport_width(),
                 static_cast<float>(kTestWidth), 0.001F);
    check_within(built.host->application().box().viewport_height(),
                 static_cast<float>(kTestHeight), 0.001F);
    check(built.host->pump_once(0.0));
    check(built.host->request_close());
    check(built.host->pump_once(0.0));
  }

  it("dispatches a worker completion before the next frame") {
    auto probe = std::make_shared<CompletionProbe>();
    auto built = flexUI::GCanvasWindowHost::create(
        host_config(gcanvas::Backend::OpenGL),
        completion_application_builder(probe));
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.host->pump_once(0.0));
    auto request = built.host->application().try_receive_service_request();
    check(request);
    check(post_completion_from_worker(
        built.host->application(), *request.request, "completion-one"));
    check(built.host->pump_once(0.0));

    check_equal(probe->completion_ids.size(), std::size_t{1});
    check_equal(probe->completion_ids.front(), std::uint64_t{101});
    check_equal(built.host->application().box().get_by_id("surface")->text(),
                std::string("completion-one"));
    check_equal(
        built.host->application().completion_mailbox().statistics().consumed,
        std::uint64_t{1});
    check(built.host->request_close());
    check(built.host->pump_once(0.0));
  }

  it("automatically routes script service requests through the registry") {
    auto probe = std::make_shared<CompletionProbe>();
    auto built = flexUI::GCanvasWindowHost::create(
        host_config(gcanvas::Backend::OpenGL),
        completion_application_builder(probe));
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.host->pump_once(0.0));
    check_equal(probe->submitted_ids.size(), std::size_t{0});
    check(built.host->pump_once(0.0));
    check_equal(probe->submitted_ids.size(), std::size_t{1});
    check_equal(probe->completion_ids.size(), std::size_t{0});
    check(built.host->pump_once(0.0));
    check_equal(probe->completion_ids.size(), std::size_t{1});
    check_equal(probe->completion_ids.front(), std::uint64_t{101});
    check_equal(built.host->application().box().get_by_id("surface")->text(),
                std::string("automatic-101"));
    check(built.host->request_close());
    check(built.host->pump_once(0.0));
  }

  it("bounds automatic request dispatch per pump") {
    auto probe = std::make_shared<CompletionProbe>();
    probe->command_count = 2;
    auto config = host_config(gcanvas::Backend::OpenGL);
    config.max_service_requests_per_pump = 1;
    auto built = flexUI::GCanvasWindowHost::create(
        config, completion_application_builder(probe));
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.host->pump_once(0.0));
    check(built.host->pump_once(0.0));
    check_equal(probe->submitted_ids.size(), std::size_t{1});
    check(built.host->pump_once(0.0));
    check_equal(probe->submitted_ids.size(), std::size_t{2});
    check_equal(probe->completion_ids.size(), std::size_t{1});
    check(built.host->pump_once(0.0));
    check_equal(probe->completion_ids.size(), std::size_t{2});
    check(built.host->request_close());
    check(built.host->pump_once(0.0));
  }

  it("bounds scripted completion dispatch per pump") {
    auto probe = std::make_shared<CompletionProbe>();
    probe->command_count = 2;
    auto config = host_config(gcanvas::Backend::OpenGL);
    config.max_service_completions_per_pump = 1;
    auto built = flexUI::GCanvasWindowHost::create(
        config, completion_application_builder(probe));
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.host->pump_once(0.0));
    auto first = built.host->application().try_receive_service_request();
    auto second = built.host->application().try_receive_service_request();
    check(first);
    check(second);
    check(post_completion_from_worker(
        built.host->application(), *first.request, "completion-one"));
    check(post_completion_from_worker(
        built.host->application(), *second.request, "completion-two"));

    check(built.host->pump_once(0.0));
    check_equal(probe->completion_ids.size(), std::size_t{1});
    check_equal(
        built.host->application().completion_mailbox().statistics().consumed,
        std::uint64_t{1});
    check(built.host->pump_once(0.0));
    check_equal(probe->completion_ids.size(), std::size_t{2});
    check_equal(built.host->application().box().get_by_id("surface")->text(),
                std::string("completion-two"));
    check_equal(
        built.host->application().completion_mailbox().statistics().consumed,
        std::uint64_t{2});
    check(built.host->request_close());
    check(built.host->pump_once(0.0));
  }

  it("leaves completions for raw polling when no script handler exists") {
    auto probe = std::make_shared<CompletionProbe>();
    probe->enable_completion_handler = false;
    auto built = flexUI::GCanvasWindowHost::create(
        host_config(gcanvas::Backend::OpenGL),
        completion_application_builder(probe));
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.host->pump_once(0.0));
    auto request = built.host->application().try_receive_service_request();
    check(request);
    check(post_completion_from_worker(
        built.host->application(), *request.request, "raw-completion"));
    check(built.host->pump_once(0.0));
    check_equal(
        built.host->application().completion_mailbox().statistics().consumed,
        std::uint64_t{0});

    auto completion =
        built.host->application().try_receive_service_completion();
    check(completion);
    check_equal(completion.completion->payload, std::string("raw-completion"));
    check(built.host->request_close());
    check(built.host->pump_once(0.0));
  }

  it("maps script completion failure and shuts down") {
    auto probe = std::make_shared<CompletionProbe>();
    probe->fail_completion = true;
    auto built = flexUI::GCanvasWindowHost::create(
        host_config(gcanvas::Backend::OpenGL),
        completion_application_builder(probe));
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.host->pump_once(0.0));
    auto request = built.host->application().try_receive_service_request();
    check(request);
    check(post_completion_from_worker(
        built.host->application(), *request.request, "completion-failure"));
    const auto pumped = built.host->pump_once(0.0);

    check_false(static_cast<bool>(pumped));
    check(pumped.error.code ==
          flexUI::GCanvasWindowHostErrorCode::ServiceCompletionFailed);
    check(pumped.error.stage ==
          flexUI::GCanvasWindowHostStage::ServiceCompletion);
    check(pumped.error.application_error.code ==
          flexUI::DesktopApplicationErrorCode::ControllerServiceCompletionFailed);
    check(built.host->application().state() ==
          flexUI::DesktopApplicationState::Shutdown);
  }

  it("rejects invalid configuration before native window creation") {
    auto config = host_config(gcanvas::Backend::OpenGL);
    config.window.width = 0;
    auto built = flexUI::GCanvasWindowHost::create(
        config, application_builder());

    check_false(static_cast<bool>(built));
    check_null(built.host.get());
    check(built.error.code ==
          flexUI::GCanvasWindowHostErrorCode::InvalidConfiguration);
    check(built.error.stage ==
          flexUI::GCanvasWindowHostStage::Configuration);

    config = host_config(gcanvas::Backend::OpenGL);
    config.max_service_completions_per_pump = 0;
    built = flexUI::GCanvasWindowHost::create(config, application_builder());
    check_false(static_cast<bool>(built));
    check(built.error.code ==
          flexUI::GCanvasWindowHostErrorCode::InvalidConfiguration);

    config.max_service_completions_per_pump =
        flexUI::GCanvasWindowHostConfig::kMaximumServiceCompletionsPerPump + 1;
    built = flexUI::GCanvasWindowHost::create(config, application_builder());
    check_false(static_cast<bool>(built));
    check(built.error.code ==
          flexUI::GCanvasWindowHostErrorCode::InvalidConfiguration);

    config = host_config(gcanvas::Backend::OpenGL);
    config.max_service_requests_per_pump = 0;
    built = flexUI::GCanvasWindowHost::create(config, application_builder());
    check_false(static_cast<bool>(built));
    check(built.error.code ==
          flexUI::GCanvasWindowHostErrorCode::InvalidConfiguration);

    config.max_service_requests_per_pump =
        flexUI::GCanvasWindowHostConfig::kMaximumServiceRequestsPerPump + 1;
    built = flexUI::GCanvasWindowHost::create(config, application_builder());
    check_false(static_cast<bool>(built));
    check(built.error.code ==
          flexUI::GCanvasWindowHostErrorCode::InvalidConfiguration);
  }

  it("finalizes a close requested from on_frame before run returns") {
    auto probe = std::make_shared<RunProbe>();
    auto builder = application_builder();
    builder.script(
        "close-on-frame",
        [probe](std::string_view, std::string_view) {
          return flexUI::ScriptModuleFactoryResult{
              std::make_unique<CloseOnFrameModule>(probe), {}};
        });
    auto built = flexUI::GCanvasWindowHost::create(
        host_config(gcanvas::Backend::OpenGL), std::move(builder));
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }
    probe->host = built.host.get();

    const auto run = built.host->run();
    check(static_cast<bool>(run));
    check(static_cast<bool>(probe->close_result));
    check_equal(probe->frame_calls, std::size_t{1});
    check(built.host->application().state() ==
          flexUI::DesktopApplicationState::Shutdown);
  }

  it("rejects pump and close calls from a non-owner thread") {
    auto built = flexUI::GCanvasWindowHost::create(
        host_config(gcanvas::Backend::OpenGL), application_builder());
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    flexUI::GCanvasWindowHostResult pump_result;
    flexUI::GCanvasWindowHostResult close_result;
    std::thread worker([&] {
      pump_result = built.host->pump_once(0.0);
      close_result = built.host->request_close();
    });
    worker.join();

    check_false(static_cast<bool>(pump_result));
    check(pump_result.error.code ==
          flexUI::GCanvasWindowHostErrorCode::WrongThread);
    check_false(static_cast<bool>(close_result));
    check(close_result.error.code ==
          flexUI::GCanvasWindowHostErrorCode::WrongThread);
    check(built.host->application().state() ==
          flexUI::DesktopApplicationState::Ready);

    check(built.host->request_close());
    check(built.host->pump_once(0.0));
  }

  it("rejects non-finite and oversized explicit frame deltas") {
    auto built = flexUI::GCanvasWindowHost::create(
        host_config(gcanvas::Backend::OpenGL), application_builder());
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    auto invalid = built.host->pump_once(
        std::numeric_limits<double>::infinity());
    auto oversized = built.host->pump_once(1.0);
    check_false(static_cast<bool>(invalid));
    check(invalid.error.code ==
          flexUI::GCanvasWindowHostErrorCode::InvalidConfiguration);
    check_false(static_cast<bool>(oversized));
    check(oversized.error.code ==
          flexUI::GCanvasWindowHostErrorCode::InvalidConfiguration);
    check(built.host->application().state() ==
          flexUI::DesktopApplicationState::Ready);

    check(built.host->request_close());
    check(built.host->pump_once(0.0));
  }
}
