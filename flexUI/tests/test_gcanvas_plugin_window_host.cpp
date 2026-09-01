#include <flexUI/gcanvas_plugin_window_host.h>

#include <tinytest.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifndef FLEXUI_TEST_ECHO_PLUGIN
  #error FLEXUI_TEST_ECHO_PLUGIN must name the echo test plugin library
#endif

namespace {

constexpr int kTestWidth = 96;
constexpr int kTestHeight = 64;

struct CompositionProbe {
  std::string operation = "echo";
  std::vector<flexUI::ApplicationServiceCompletion> completions;
};

class CompositionModule final : public flexUI::IScriptModule {
public:
  explicit CompositionModule(std::shared_ptr<CompositionProbe> probe) : probe_(std::move(probe)) {}

  flexUI::ScriptResolveResult resolve_export(std::string_view name,
                                             flexUI::ScriptCallbackKind callback) override {
    if (name == "on_frame" && callback == flexUI::ScriptCallbackKind::Frame) {
      return {flexUI::ScriptExportHandle{1}, {}};
    }
    if (name == "on_service_completion" &&
        callback == flexUI::ScriptCallbackKind::ServiceCompletion) {
      return {flexUI::ScriptExportHandle{2}, {}};
    }
    return {};
  }

  flexUI::ScriptCallResult call(flexUI::ScriptExportHandle handle,
                                const flexUI::ScriptCallContext &context) override {
    flexUI::ScriptCallResult result;
    if (handle.value == 1 && context.callback == flexUI::ScriptCallbackKind::Frame) {
      if (!emitted_) {
        const auto appended =
            result.commands.append({101, "test.echo/1", probe_->operation, "composition-payload"});
        if (!appended) {
          return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded, appended.error.message}};
        }
        emitted_ = true;
      }
      return result;
    }
    if (handle.value == 2 && context.callback == flexUI::ScriptCallbackKind::ServiceCompletion &&
        context.service_completion != nullptr) {
      probe_->completions.push_back(*context.service_completion);
      const auto appended = result.mutations.append(
          flexUI::SetTextMutation{{"surface", 1}, context.service_completion->payload});
      if (!appended) {
        return {{flexUI::ScriptModuleErrorCode::ResourceLimitExceeded, appended.error.message}};
      }
      return result;
    }
    return {{flexUI::ScriptModuleErrorCode::RuntimeFailure,
             "invalid plugin composition test callback"}};
  }

private:
  std::shared_ptr<CompositionProbe> probe_;
  bool emitted_ = false;
};

class RejectingSink final : public flexUI::IApplicationServiceCompletionSink {
public:
  flexUI::ApplicationCompletionPostResult try_post(const flexUI::ApplicationCompletion &) override {
    return {{flexUI::ApplicationCompletionErrorCode::Closed, "test sink is closed"}};
  }
};

flexUI::GCanvasWindowHostConfig window_config() {
  flexUI::GCanvasWindowHostConfig config;
  config.window.title = "FlexUI plugin composition test";
  config.window.width = kTestWidth;
  config.window.height = kTestHeight;
  config.window.visible = false;
  config.window.decorated = false;
  config.window.resizeable = false;
  config.window.vsync = false;
  config.window.backend = gcanvas::Backend::OpenGL;
  return config;
}

flexUI::DesktopApplicationBuilder
application_builder(const std::shared_ptr<CompositionProbe> &probe) {
  flexUI::DesktopApplicationBuilder builder;
  builder.xml_entry("<ui name=\"PluginComposition\"><div id=\"surface\"/></ui>")
      .script("plugin-composition-test", [probe](std::string_view, std::string_view) {
        return flexUI::ScriptModuleFactoryResult{std::make_unique<CompositionModule>(probe), {}};
      });
  return builder;
}

flexUI::PluginHostBuildResult build_echo_host() {
  flexUI::PluginHostBuilder builder;
  const auto loaded = builder.load_plugin(std::filesystem::path(FLEXUI_TEST_ECHO_PLUGIN));
  if (!loaded) {
    flexUI::PluginHostBuildResult failed;
    failed.error = loaded.error;
    return failed;
  }
  return builder.build();
}

flexUI::ApplicationCapabilityManifest echo_manifest() { return {{"test.echo/1"}, {"test.echo/1"}}; }

} // namespace

spec("FlexUI gCanvas PluginHost composition owns service lifetime") {
  it("routes a plugin request and stops plugins before releasing the application") {
    auto plugins = build_echo_host();
    check(static_cast<bool>(plugins));
    if (!plugins) {
      return;
    }
    const flexUI::ApplicationServiceRequest cached_request{
        {900, 1}, 900, "test.echo/1", "echo", "after-stop"};
    auto resolved = plugins.registry->resolve(cached_request, echo_manifest());
    check(static_cast<bool>(resolved));

    auto probe = std::make_shared<CompositionProbe>();
    auto built = flexUI::GCanvasPluginWindowHost::create(
        window_config(), application_builder(probe), std::move(plugins), echo_manifest(),
        std::chrono::seconds(1));
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.host->pump_once(0.0));
    check(built.host->pump_once(0.0));
    check(built.host->pump_once(0.0));
    check_equal(probe->completions.size(), std::size_t{1});
    check_equal(probe->completions.front().payload, std::string("composition-payload"));
    check_equal(built.host->application().box().get_by_id("surface")->text(),
                std::string("composition-payload"));

    check(built.host->request_close());
    check(built.host->pump_once(0.0));
    check(built.host->application().state() == flexUI::DesktopApplicationState::Shutdown);
    check(built.host->plugins().state() == flexUI::PluginHostState::Stopped);
    check(built.host->shutdown(std::chrono::seconds(1)));
    RejectingSink sink;
    const auto rejected = resolved.endpoint->try_submit(cached_request, sink);
    check_false(static_cast<bool>(rejected));
    check(rejected.error.code == flexUI::ApplicationServiceSubmitErrorCode::Closed);
  }

  it("rejects an empty plugin build result before creating a window") {
    auto probe = std::make_shared<CompositionProbe>();
    auto built = flexUI::GCanvasPluginWindowHost::create(
        window_config(), application_builder(probe), {}, echo_manifest(), std::chrono::seconds(1));
    check_false(static_cast<bool>(built));
    check(built.error.code == flexUI::GCanvasPluginWindowHostErrorCode::InvalidPluginHost);
  }

  it("rejects a registry that does not belong to the plugin host") {
    auto plugins = build_echo_host();
    check(static_cast<bool>(plugins));
    if (!plugins) {
      return;
    }
    flexUI::ApplicationServiceRegistryBuilder registry_builder;
    auto unrelated = registry_builder.build();
    check(static_cast<bool>(unrelated));
    plugins.registry = std::move(unrelated.registry);

    auto probe = std::make_shared<CompositionProbe>();
    auto built = flexUI::GCanvasPluginWindowHost::create(
        window_config(), application_builder(probe), std::move(plugins), echo_manifest(),
        std::chrono::seconds(1));
    check_false(static_cast<bool>(built));
    check(built.error.code == flexUI::GCanvasPluginWindowHostErrorCode::InvalidPluginHost);
  }

  it("rejects a negative automatic plugin stop timeout") {
    auto plugins = build_echo_host();
    check(static_cast<bool>(plugins));
    if (!plugins) {
      return;
    }
    auto probe = std::make_shared<CompositionProbe>();
    auto built = flexUI::GCanvasPluginWindowHost::create(
        window_config(), application_builder(probe), std::move(plugins), echo_manifest(),
        std::chrono::milliseconds(-1));
    check_false(static_cast<bool>(built));
    check(built.error.code == flexUI::GCanvasPluginWindowHostErrorCode::InvalidConfiguration);
  }

  it("rejects a PluginHost owned by another thread") {
    flexUI::PluginHostBuildResult plugins;
    std::thread worker([&] { plugins = build_echo_host(); });
    worker.join();
    check(static_cast<bool>(plugins));
    if (!plugins) {
      return;
    }

    auto probe = std::make_shared<CompositionProbe>();
    auto built = flexUI::GCanvasPluginWindowHost::create(
        window_config(), application_builder(probe), std::move(plugins), echo_manifest(),
        std::chrono::seconds(1));
    check_false(static_cast<bool>(built));
    check(built.error.code == flexUI::GCanvasPluginWindowHostErrorCode::WrongThread);
  }

  it("retains both owners after a plugin join timeout and permits retry") {
    auto plugins = build_echo_host();
    check(static_cast<bool>(plugins));
    if (!plugins) {
      return;
    }
    auto probe = std::make_shared<CompositionProbe>();
    probe->operation = "block";
    auto built = flexUI::GCanvasPluginWindowHost::create(
        window_config(), application_builder(probe), std::move(plugins), echo_manifest(),
        std::chrono::milliseconds(1));
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    check(built.host->pump_once(0.0));
    check(built.host->pump_once(0.0));
    const auto timed_out = built.host->shutdown(std::chrono::milliseconds(1));
    check_false(static_cast<bool>(timed_out));
    check(timed_out.error.code == flexUI::GCanvasPluginWindowHostErrorCode::PluginStopFailed);
    check(timed_out.error.plugin_error.code == flexUI::PluginHostErrorCode::JoinTimedOut);
    check(built.host->application().state() == flexUI::DesktopApplicationState::Shutdown);
    check(built.host->plugins().state() == flexUI::PluginHostState::Stopping);

    check(built.host->shutdown(std::chrono::seconds(1)));
    check(built.host->plugins().state() == flexUI::PluginHostState::Stopped);
  }

  it("rejects owner operations from another thread") {
    auto plugins = build_echo_host();
    check(static_cast<bool>(plugins));
    if (!plugins) {
      return;
    }
    auto probe = std::make_shared<CompositionProbe>();
    auto built = flexUI::GCanvasPluginWindowHost::create(
        window_config(), application_builder(probe), std::move(plugins), echo_manifest(),
        std::chrono::seconds(1));
    check(static_cast<bool>(built));
    if (!built) {
      return;
    }

    flexUI::GCanvasPluginWindowHostResult foreign;
    std::thread worker([&] { foreign = built.host->pump_once(0.0); });
    worker.join();
    check_false(static_cast<bool>(foreign));
    check(foreign.error.code == flexUI::GCanvasPluginWindowHostErrorCode::WrongThread);
    check(built.host->shutdown(std::chrono::seconds(1)));
  }
}
