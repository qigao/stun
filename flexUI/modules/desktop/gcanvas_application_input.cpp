#include "flexUI/gcanvas_application_input.h"

#include "flexUI/application_host_bridge.h"

#include <utility>

namespace flexUI {
namespace {

GCanvasApplicationInputResult wrong_thread_result() {
  DesktopApplicationError application_error;
  application_error.code = DesktopApplicationErrorCode::WrongThread;
  application_error.stage = DesktopApplicationStage::ControllerEvent;
  application_error.message = "gCanvas application input routing must run on its owner thread";
  return {false,
          {GCanvasApplicationInputErrorCode::ApplicationFailed, {}, std::move(application_error)}};
}

GCanvasApplicationInputResult normalization_error(GCanvasInputError error) {
  return {false, {GCanvasApplicationInputErrorCode::NormalizationFailed, std::move(error), {}}};
}

GCanvasApplicationInputResult application_error(DesktopApplicationError error) {
  return {false, {GCanvasApplicationInputErrorCode::ApplicationFailed, {}, std::move(error)}};
}

} // namespace

GCanvasApplicationInputRouter::GCanvasApplicationInputRouter(
    DesktopApplication &application) noexcept
    : application_(application) {}

bool GCanvasApplicationInputRouter::is_owner_thread() const noexcept {
  return application_.is_owner_thread();
}

GCanvasApplicationInputResult
GCanvasApplicationInputRouter::dispatch(GCanvasInputResult normalized) {
  if (!normalized) {
    return normalization_error(std::move(normalized.error));
  }

  auto dispatched = application_.dispatch_event(*normalized.event);
  if (!dispatched) {
    return application_error(std::move(dispatched.error));
  }
  return {true, {}};
}

GCanvasApplicationInputResult
GCanvasApplicationInputRouter::mouse_move(const gcanvas::mouse_move_event &native) {
  if (!is_owner_thread()) {
    return wrong_thread_result();
  }
  return dispatch(normalizer_.mouse_move(native));
}

GCanvasApplicationInputResult
GCanvasApplicationInputRouter::mouse_button(const gcanvas::mouse_button_event &native) {
  if (!is_owner_thread()) {
    return wrong_thread_result();
  }
  return dispatch(normalizer_.mouse_button(native));
}

GCanvasApplicationInputResult GCanvasApplicationInputRouter::key(const gcanvas::key_event &native) {
  if (!is_owner_thread()) {
    return wrong_thread_result();
  }
  return dispatch(normalizer_.key(native));
}

GCanvasApplicationInputResult
GCanvasApplicationInputRouter::character(const gcanvas::char_event &native) {
  if (!is_owner_thread()) {
    return wrong_thread_result();
  }

  auto normalized = normalizer_.character(native);
  if (!normalized) {
    return normalization_error(std::move(normalized.error));
  }

  auto dispatched = host::dispatch_text_input_if_focused(application_, normalized.event->text);
  if (!dispatched) {
    return application_error(std::move(dispatched.error));
  }
  return {dispatched.dispatched, {}};
}

GCanvasApplicationInputResult
GCanvasApplicationInputRouter::scroll(const gcanvas::scroll_event &native) {
  if (!is_owner_thread()) {
    return wrong_thread_result();
  }
  return dispatch(normalizer_.scroll(native));
}

GCanvasApplicationInputResult
GCanvasApplicationInputRouter::resize(const gcanvas::resize_event &native) {
  if (!is_owner_thread()) {
    return wrong_thread_result();
  }

  auto normalized = normalizer_.resize(native);
  if (!normalized) {
    return normalization_error(std::move(normalized.error));
  }

  application_.box().set_viewport(static_cast<float>(normalized.metrics->width),
                                  static_cast<float>(normalized.metrics->height));
  application_.box().invalidate();
  return {true, {}};
}

} // namespace flexUI
