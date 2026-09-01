#pragma once

#include "flexUI/gcanvas_window_host.h"
#include "flexUI/plugin_host.h"

#include <chrono>
#include <memory>
#include <string>

namespace flexUI {

enum class GCanvasPluginWindowHostStage {
  None,
  Configuration,
  PluginHost,
  WindowHost,
  Runtime,
  Shutdown,
};

enum class GCanvasPluginWindowHostErrorCode {
  None,
  InvalidConfiguration,
  InvalidPluginHost,
  WrongThread,
  WindowHostFailed,
  PluginStopFailed,
  AllocationFailed,
  InternalInvariant,
};

struct GCanvasPluginWindowHostError {
  GCanvasPluginWindowHostErrorCode code = GCanvasPluginWindowHostErrorCode::None;
  GCanvasPluginWindowHostStage stage = GCanvasPluginWindowHostStage::None;
  std::string message;
  GCanvasWindowHostError window_error;
  PluginHostError plugin_error;

  explicit operator bool() const noexcept { return code != GCanvasPluginWindowHostErrorCode::None; }
};

struct GCanvasPluginWindowHostResult {
  GCanvasPluginWindowHostError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

class GCanvasPluginWindowHost;

struct GCanvasPluginWindowHostBuildResult {
  std::unique_ptr<GCanvasPluginWindowHost> host;
  GCanvasPluginWindowHostError error;

  explicit operator bool() const noexcept { return host != nullptr && !static_cast<bool>(error); }
};

/// Optional desktop composition that owns PluginHost and gCanvas lifetimes.
///
/// PluginHost is stopped and joined while the application completion mailbox
/// still exists. A bounded shutdown timeout retains both owners so the caller
/// can retry; destruction uses the PluginHost infinite-join safety net.
class GCanvasPluginWindowHost final {
public:
  /// Consumes one complete Started PluginHost build and injects its exact
  /// registry snapshot into the application builder before window creation.
  /// @param plugin_stop_timeout Non-negative timeout used by automatic stop
  ///        after pump_once()/run() observes application Shutdown.
  static GCanvasPluginWindowHostBuildResult
  create(GCanvasWindowHostConfig window_config, DesktopApplicationBuilder application_builder,
         PluginHostBuildResult plugins, ApplicationCapabilityManifest capability_manifest,
         std::chrono::milliseconds plugin_stop_timeout);

  ~GCanvasPluginWindowHost();

  GCanvasPluginWindowHost(const GCanvasPluginWindowHost &) = delete;
  GCanvasPluginWindowHost &operator=(const GCanvasPluginWindowHost &) = delete;
  GCanvasPluginWindowHost(GCanvasPluginWindowHost &&) = delete;
  GCanvasPluginWindowHost &operator=(GCanvasPluginWindowHost &&) = delete;

  DesktopApplication &application() noexcept;
  const DesktopApplication &application() const noexcept;
  PluginHost &plugins() noexcept;
  const PluginHost &plugins() const noexcept;
  GCanvasWindowHost &window_host() noexcept;
  const GCanvasWindowHost &window_host() const noexcept;
  bool is_owner_thread() const noexcept;

  /// Advances the window/application and automatically stops plugins only
  /// after application Shutdown. A stop timeout is returned and remains
  /// retryable through shutdown(timeout).
  GCanvasPluginWindowHostResult pump_once(double delta_seconds);
  GCanvasPluginWindowHostResult run();

  /// Requests application/window close without blocking for plugin workers.
  GCanvasPluginWindowHostResult request_close();

  /// Finishes controller shutdown, then stops/joins plugins with this call's
  /// non-negative timeout. Repeated calls after success are idempotent.
  GCanvasPluginWindowHostResult shutdown(std::chrono::milliseconds timeout);

private:
  struct Impl;
  explicit GCanvasPluginWindowHost(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;
};

} // namespace flexUI
