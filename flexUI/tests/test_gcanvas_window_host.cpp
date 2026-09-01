#include <flexUI/gcanvas_window_host.h>

#include <tinytest.hpp>

#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <thread>

namespace {

constexpr int kTestWidth = 96;
constexpr int kTestHeight = 64;

struct RunProbe {
  flexUI::GCanvasWindowHost *host = nullptr;
  flexUI::GCanvasWindowHostResult close_result;
  std::size_t frame_calls = 0;
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
