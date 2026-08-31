#pragma once

#include "flexUI/application.h"
#include "flexUI/gcanvas_input.h"

namespace flexUI {

enum class GCanvasApplicationInputErrorCode {
  None,
  NormalizationFailed,
  ApplicationFailed,
};

struct GCanvasApplicationInputError {
  GCanvasApplicationInputErrorCode code = GCanvasApplicationInputErrorCode::None;
  GCanvasInputError input_error;
  DesktopApplicationError application_error;

  explicit operator bool() const noexcept { return code != GCanvasApplicationInputErrorCode::None; }
};

struct GCanvasApplicationInputResult {
  /// True only when a normalized event or viewport update reached the application.
  bool processed = false;
  GCanvasApplicationInputError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

/// Routes validated gCanvas callback values into one DesktopApplication.
///
/// The application is borrowed and must outlive this router. Every operation
/// must run on the thread that built the application. A wrong-thread call is
/// rejected before native input normalization, so it cannot change cached
/// pointer state. Successful character input may report `processed == false`
/// when no editable widget owns focus.
class GCanvasApplicationInputRouter final {
public:
  /// @param application Borrowed application and owner-thread authority.
  explicit GCanvasApplicationInputRouter(DesktopApplication &application) noexcept;

  GCanvasApplicationInputRouter(const GCanvasApplicationInputRouter &) = delete;
  GCanvasApplicationInputRouter &operator=(const GCanvasApplicationInputRouter &) = delete;
  GCanvasApplicationInputRouter(GCanvasApplicationInputRouter &&) = delete;
  GCanvasApplicationInputRouter &operator=(GCanvasApplicationInputRouter &&) = delete;

  /// @return Application dispatch status, or a thread/normalization error.
  GCanvasApplicationInputResult mouse_move(const gcanvas::mouse_move_event &native);

  /// @return Application dispatch status, or a thread/normalization error.
  GCanvasApplicationInputResult mouse_button(const gcanvas::mouse_button_event &native);

  /// @return Application dispatch status, or a thread/normalization error.
  GCanvasApplicationInputResult key(const gcanvas::key_event &native);

  /// @return Success with `processed == false` when focused text input is absent.
  GCanvasApplicationInputResult character(const gcanvas::char_event &native);

  /// @return Application dispatch status, or a thread/normalization error.
  GCanvasApplicationInputResult scroll(const gcanvas::scroll_event &native);

  /// Applies positive logical dimensions and invalidates the application Box.
  /// @return Viewport update status, or a thread/normalization error.
  GCanvasApplicationInputResult resize(const gcanvas::resize_event &native);

private:
  GCanvasApplicationInputResult dispatch(GCanvasInputResult normalized);
  bool is_owner_thread() const noexcept;

  DesktopApplication &application_;
  GCanvasInputNormalizer normalizer_;
};

} // namespace flexUI
