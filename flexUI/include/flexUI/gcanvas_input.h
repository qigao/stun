#pragma once

#include "flexUI/event.h"

#include <gcanvas/event.hpp>

#include <optional>
#include <string>

namespace flexUI {

enum class GCanvasInputErrorCode {
  None,
  NonFiniteScalar,
  UnsupportedMouseButton,
  UnsupportedAction,
  InvalidModifiers,
  NullText,
  InvalidCodepoint,
  InvalidText,
  MissingPointerPosition,
  InvalidViewportSize,
};

struct GCanvasInputError {
  GCanvasInputErrorCode code = GCanvasInputErrorCode::None;
  std::string message;

  explicit operator bool() const noexcept { return code != GCanvasInputErrorCode::None; }
};

struct GCanvasInputResult {
  std::optional<Event> event;
  GCanvasInputError error;

  explicit operator bool() const noexcept { return event.has_value() && !static_cast<bool>(error); }
};

struct GCanvasViewportMetrics {
  int width = 0;
  int height = 0;
};

struct GCanvasViewportResult {
  std::optional<GCanvasViewportMetrics> metrics;
  GCanvasInputError error;

  explicit operator bool() const noexcept {
    return metrics.has_value() && !static_cast<bool>(error);
  }
};

/// Strictly converts gCanvas callback values into backend-neutral FlexUI input.
///
/// The instance retains only the last valid logical pointer position so a
/// gCanvas scroll callback, which has no coordinates, can create MouseWheel.
/// Failed conversions never change that position. One window/owner thread must
/// own each instance; concurrent calls are not supported.
///
/// @code
/// GCanvasInputNormalizer input;
/// auto move = input.mouse_move({10.0, 20.0});
/// if (move) application.dispatch_event(*move.event);
/// @endcode
class GCanvasInputNormalizer final {
public:
  /// @param native Logical-pixel pointer coordinates from gCanvas.
  /// @return MouseMove, or NonFiniteScalar when conversion to float is unsafe.
  GCanvasInputResult mouse_move(const gcanvas::mouse_move_event &native);

  /// @param native Logical coordinates, button, action and modifier state.
  /// @return MouseDown/MouseUp, or a scalar, button, action or modifier error.
  GCanvasInputResult mouse_button(const gcanvas::mouse_button_event &native);

  /// @param native Native key action and modifier state; repeat maps to KeyDown.
  /// @return KeyDown/KeyUp, or UnsupportedAction/InvalidModifiers.
  GCanvasInputResult key(const gcanvas::key_event &native) const;

  /// @param native Unicode scalar and matching callback-owned UTF-8 pointer.
  /// @return A copied TextInput event, or a null/codepoint/text error.
  GCanvasInputResult character(const gcanvas::char_event &native) const;

  /// @param native Finite logical scroll offsets.
  /// @return MouseWheel at the last pointer position, or a scalar/state error.
  GCanvasInputResult scroll(const gcanvas::scroll_event &native) const;

  /// @param native Logical window dimensions from gCanvas.
  /// @return Positive viewport metrics, or InvalidViewportSize.
  GCanvasViewportResult resize(const gcanvas::resize_event &native) const;

private:
  std::optional<float> pointer_x_;
  std::optional<float> pointer_y_;
};

} // namespace flexUI
