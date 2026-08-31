#pragma once

#include "flexUI/application.h"
#include "flexUI/gcanvas_application_input.h"

#include <gcanvas/window.hpp>

#include <memory>
#include <string>

namespace flexUI {

enum class GCanvasWindowHostFrameMode {
  EventDriven,
  Continuous,
};

struct GCanvasWindowHostConfig {
  gcanvas::WindowConfig window;
  GCanvasWindowHostFrameMode frame_mode = GCanvasWindowHostFrameMode::EventDriven;
  double continuous_interval_seconds = 1.0 / 60.0;
  double max_frame_delta_seconds = 0.25;
};

enum class GCanvasWindowHostStage {
  None,
  Configuration,
  WindowCreation,
  ContextCreation,
  RendererCreation,
  ApplicationBuild,
  ListenerRegistration,
  NativeEvent,
  Frame,
  PointerCapture,
  Shutdown,
};

enum class GCanvasWindowHostErrorCode {
  None,
  InvalidConfiguration,
  WrongThread,
  InvalidState,
  WindowCreationFailed,
  ContextCreationFailed,
  RendererCreationFailed,
  ApplicationBuildFailed,
  ListenerRegistrationFailed,
  InputFailed,
  FrameFailed,
  PointerCaptureFailed,
  ShutdownFailed,
  NativeEventFailed,
};

struct GCanvasWindowHostError {
  GCanvasWindowHostErrorCode code = GCanvasWindowHostErrorCode::None;
  GCanvasWindowHostStage stage = GCanvasWindowHostStage::None;
  std::string message;
  DesktopApplicationError application_error;
  GCanvasApplicationInputError input_error;

  explicit operator bool() const noexcept {
    return code != GCanvasWindowHostErrorCode::None;
  }
};

struct GCanvasWindowHostResult {
  GCanvasWindowHostError error;

  explicit operator bool() const noexcept {
    return !static_cast<bool>(error);
  }
};

class GCanvasWindowHost;

struct GCanvasWindowHostBuildResult {
  std::unique_ptr<GCanvasWindowHost> host;
  GCanvasWindowHostError error;

  explicit operator bool() const noexcept {
    return host != nullptr && !static_cast<bool>(error);
  }
};

/// Owner-thread desktop adapter for one gCanvas window and one FlexUI app.
///
/// The host owns its native window, GPU context, Flex renderer, application,
/// input router, and listener subscriptions. It must be destroyed on the
/// thread that called create(). Core FlexUI remains independent of GLFW.
class GCanvasWindowHost final {
public:
  static GCanvasWindowHostBuildResult create(GCanvasWindowHostConfig config,
                                              DesktopApplicationBuilder builder);

  ~GCanvasWindowHost();

  GCanvasWindowHost(const GCanvasWindowHost &) = delete;
  GCanvasWindowHost &operator=(const GCanvasWindowHost &) = delete;
  GCanvasWindowHost(GCanvasWindowHost &&) = delete;
  GCanvasWindowHost &operator=(GCanvasWindowHost &&) = delete;

  DesktopApplication &application() noexcept;
  const DesktopApplication &application() const noexcept;
  bool is_owner_thread() const noexcept;
  gcanvas::Backend backend() const noexcept;
  bool has_native_pointer_capture() const noexcept;

  /// Polls pending native events, advances one bounded frame, submits dirty UI,
  /// and presents it. A close request is finalized as application shutdown.
  GCanvasWindowHostResult pump_once(double delta_seconds);

  /// Runs an event-driven or bounded continuous owner-thread loop until close.
  /// Elapsed time is capped by max_frame_delta_seconds before frame dispatch.
  GCanvasWindowHostResult run();

  /// Requests close and closes the native window. pump_once()/run() performs
  /// the subsequent controller shutdown; repeated calls are idempotent.
  GCanvasWindowHostResult request_close();

private:
  struct Impl;
  explicit GCanvasWindowHost(std::unique_ptr<Impl> impl) noexcept;
  std::unique_ptr<Impl> impl_;
};

} // namespace flexUI
