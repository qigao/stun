#include "flexUI/gcanvas_plugin_window_host.h"

#include <new>
#include <thread>
#include <utility>

namespace flexUI {
namespace {

GCanvasPluginWindowHostError fail(GCanvasPluginWindowHostErrorCode code,
                                  GCanvasPluginWindowHostStage stage, std::string message) {
  return {code, stage, std::move(message), {}, {}};
}

GCanvasPluginWindowHostError window_failure(GCanvasWindowHostError error,
                                            GCanvasPluginWindowHostStage stage,
                                            std::string message) {
  auto result = fail(GCanvasPluginWindowHostErrorCode::WindowHostFailed, stage, std::move(message));
  result.window_error = std::move(error);
  return result;
}

GCanvasPluginWindowHostError plugin_failure(PluginHostError error) {
  auto result = fail(GCanvasPluginWindowHostErrorCode::PluginStopFailed,
                     GCanvasPluginWindowHostStage::Shutdown,
                     "PluginHost stop/join failed during desktop shutdown");
  result.plugin_error = std::move(error);
  return result;
}

} // namespace

struct GCanvasPluginWindowHost::Impl {
  Impl(std::unique_ptr<GCanvasWindowHost> configured_window_host,
       std::unique_ptr<PluginHost> configured_plugin_host,
       std::chrono::milliseconds configured_stop_timeout)
      : owner_thread(std::this_thread::get_id()), window_host(std::move(configured_window_host)),
        plugin_host(std::move(configured_plugin_host)),
        plugin_stop_timeout(configured_stop_timeout) {}

  ~Impl() {
    if (window_host != nullptr && is_owner_thread()) {
      if (window_host->application().state() == DesktopApplicationState::Ready) {
        static_cast<void>(window_host->request_close());
      }
      if (window_host->application().state() == DesktopApplicationState::CloseRequested) {
        static_cast<void>(window_host->pump_once(0.0));
      }
    }
    if (plugin_host != nullptr && plugin_host->state() != PluginHostState::Stopped) {
      static_cast<void>(plugin_host->stop(std::chrono::milliseconds::max()));
    }
  }

  std::thread::id owner_thread;
  // Reverse destruction releases PluginHost before the application mailbox.
  std::unique_ptr<GCanvasWindowHost> window_host;
  std::unique_ptr<PluginHost> plugin_host;
  std::chrono::milliseconds plugin_stop_timeout;

  bool is_owner_thread() const noexcept { return std::this_thread::get_id() == owner_thread; }

  GCanvasPluginWindowHostResult stop_plugins(std::chrono::milliseconds timeout,
                                             GCanvasPluginWindowHostError primary = {}) {
    auto stopped = plugin_host->stop(timeout);
    if (stopped) {
      return {std::move(primary)};
    }
    if (primary) {
      primary.message += "; PluginHost stop/join also failed";
      primary.plugin_error = std::move(stopped.error);
      return {std::move(primary)};
    }
    return {plugin_failure(std::move(stopped.error))};
  }

  GCanvasPluginWindowHostResult finish_window_result(GCanvasWindowHostResult window_result,
                                                     std::chrono::milliseconds timeout,
                                                     GCanvasPluginWindowHostStage stage) {
    GCanvasPluginWindowHostError primary;
    if (!window_result) {
      primary = window_failure(std::move(window_result.error), stage,
                               "gCanvas window/application operation failed");
    }
    if (window_host->application().state() == DesktopApplicationState::Shutdown) {
      return stop_plugins(timeout, std::move(primary));
    }
    return {std::move(primary)};
  }
};

GCanvasPluginWindowHost::GCanvasPluginWindowHost(std::unique_ptr<Impl> impl) noexcept
    : impl_(std::move(impl)) {}

GCanvasPluginWindowHost::~GCanvasPluginWindowHost() = default;

GCanvasPluginWindowHostBuildResult GCanvasPluginWindowHost::create(
    GCanvasWindowHostConfig window_config, DesktopApplicationBuilder application_builder,
    PluginHostBuildResult plugins, ApplicationCapabilityManifest capability_manifest,
    std::chrono::milliseconds plugin_stop_timeout) {
  if (plugin_stop_timeout.count() < 0) {
    return {{},
            fail(GCanvasPluginWindowHostErrorCode::InvalidConfiguration,
                 GCanvasPluginWindowHostStage::Configuration,
                 "automatic PluginHost stop timeout must not be negative")};
  }
  if (!plugins.host || !plugins.registry || plugins.error) {
    auto error = fail(GCanvasPluginWindowHostErrorCode::InvalidPluginHost,
                      GCanvasPluginWindowHostStage::PluginHost,
                      "composition requires one complete PluginHost build result");
    error.plugin_error = std::move(plugins.error);
    return {{}, std::move(error)};
  }
  if (!plugins.host->is_owner_thread()) {
    return {{},
            fail(GCanvasPluginWindowHostErrorCode::WrongThread,
                 GCanvasPluginWindowHostStage::PluginHost,
                 "PluginHost and desktop composition must share one owner thread")};
  }
  if (plugins.host->state() != PluginHostState::Started) {
    return {{},
            fail(GCanvasPluginWindowHostErrorCode::InvalidPluginHost,
                 GCanvasPluginWindowHostStage::PluginHost,
                 "composition requires a Started PluginHost")};
  }
  if (plugins.host->registry() != plugins.registry) {
    return {{},
            fail(GCanvasPluginWindowHostErrorCode::InvalidPluginHost,
                 GCanvasPluginWindowHostStage::PluginHost,
                 "PluginHost build result contains a foreign registry snapshot")};
  }

  application_builder.services(plugins.registry, std::move(capability_manifest));
  auto window = GCanvasWindowHost::create(std::move(window_config), std::move(application_builder));
  if (!window) {
    return {{},
            window_failure(std::move(window.error), GCanvasPluginWindowHostStage::WindowHost,
                           "gCanvas plugin window construction failed")};
  }

  try {
    auto impl = std::make_unique<Impl>(std::move(window.host), std::move(plugins.host),
                                       plugin_stop_timeout);
    return {std::unique_ptr<GCanvasPluginWindowHost>(new GCanvasPluginWindowHost(std::move(impl))),
            {}};
  } catch (const std::bad_alloc &) {
    return {{},
            fail(GCanvasPluginWindowHostErrorCode::AllocationFailed,
                 GCanvasPluginWindowHostStage::WindowHost,
                 "gCanvas plugin composition allocation failed")};
  } catch (...) {
    return {{},
            fail(GCanvasPluginWindowHostErrorCode::InternalInvariant,
                 GCanvasPluginWindowHostStage::WindowHost,
                 "gCanvas plugin composition creation failed unexpectedly")};
  }
}

DesktopApplication &GCanvasPluginWindowHost::application() noexcept {
  return impl_->window_host->application();
}

const DesktopApplication &GCanvasPluginWindowHost::application() const noexcept {
  return impl_->window_host->application();
}

PluginHost &GCanvasPluginWindowHost::plugins() noexcept { return *impl_->plugin_host; }

const PluginHost &GCanvasPluginWindowHost::plugins() const noexcept { return *impl_->plugin_host; }

GCanvasWindowHost &GCanvasPluginWindowHost::window_host() noexcept { return *impl_->window_host; }

const GCanvasWindowHost &GCanvasPluginWindowHost::window_host() const noexcept {
  return *impl_->window_host;
}

bool GCanvasPluginWindowHost::is_owner_thread() const noexcept { return impl_->is_owner_thread(); }

GCanvasPluginWindowHostResult GCanvasPluginWindowHost::pump_once(double delta_seconds) {
  if (!is_owner_thread()) {
    return {fail(GCanvasPluginWindowHostErrorCode::WrongThread,
                 GCanvasPluginWindowHostStage::Runtime,
                 "gCanvas plugin composition pump must run on its owner thread")};
  }
  return impl_->finish_window_result(impl_->window_host->pump_once(delta_seconds),
                                     impl_->plugin_stop_timeout,
                                     GCanvasPluginWindowHostStage::Runtime);
}

GCanvasPluginWindowHostResult GCanvasPluginWindowHost::run() {
  if (!is_owner_thread()) {
    return {fail(GCanvasPluginWindowHostErrorCode::WrongThread,
                 GCanvasPluginWindowHostStage::Runtime,
                 "gCanvas plugin composition loop must run on its owner thread")};
  }
  return impl_->finish_window_result(impl_->window_host->run(), impl_->plugin_stop_timeout,
                                     GCanvasPluginWindowHostStage::Runtime);
}

GCanvasPluginWindowHostResult GCanvasPluginWindowHost::request_close() {
  if (!is_owner_thread()) {
    return {fail(GCanvasPluginWindowHostErrorCode::WrongThread,
                 GCanvasPluginWindowHostStage::Shutdown,
                 "gCanvas plugin composition close must run on its owner thread")};
  }
  auto closed = impl_->window_host->request_close();
  if (!closed) {
    return {window_failure(std::move(closed.error), GCanvasPluginWindowHostStage::Shutdown,
                           "gCanvas plugin composition close request failed")};
  }
  return {};
}

GCanvasPluginWindowHostResult GCanvasPluginWindowHost::shutdown(std::chrono::milliseconds timeout) {
  if (!is_owner_thread()) {
    return {fail(GCanvasPluginWindowHostErrorCode::WrongThread,
                 GCanvasPluginWindowHostStage::Shutdown,
                 "gCanvas plugin composition shutdown must run on its owner thread")};
  }
  if (timeout.count() < 0) {
    return {fail(GCanvasPluginWindowHostErrorCode::InvalidConfiguration,
                 GCanvasPluginWindowHostStage::Configuration,
                 "PluginHost shutdown timeout must not be negative")};
  }

  GCanvasWindowHostResult window_result;
  if (application().state() == DesktopApplicationState::Ready) {
    window_result = impl_->window_host->request_close();
  }
  if (window_result && application().state() == DesktopApplicationState::CloseRequested) {
    window_result = impl_->window_host->pump_once(0.0);
  }
  return impl_->finish_window_result(std::move(window_result), timeout,
                                     GCanvasPluginWindowHostStage::Shutdown);
}

} // namespace flexUI
