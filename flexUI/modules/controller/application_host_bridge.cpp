#include "flexUI/application_host_bridge.h"

#include "flexUI/host_bridge.h"
#include "flexUI/text_util.h"

#include <string>
#include <utility>

namespace flexUI::host {
namespace {

HostApplicationDispatchResult wrong_thread_result() {
  DesktopApplicationError error;
  error.code = DesktopApplicationErrorCode::WrongThread;
  error.stage = DesktopApplicationStage::ControllerEvent;
  error.message = "desktop host input dispatch must run on its owner thread";
  return {false, std::move(error)};
}

HostApplicationDispatchResult invalid_utf8_result(std::size_t offset) {
  DesktopApplicationError error;
  error.code = DesktopApplicationErrorCode::InvalidArgument;
  error.stage = DesktopApplicationStage::ControllerEvent;
  error.message = "desktop host text input contains invalid UTF-8 at byte offset " +
                  std::to_string(offset);
  return {false, std::move(error)};
}

HostApplicationDispatchResult dispatch_utf8_if_focused(
    DesktopApplication &application, std::string_view utf8,
    bool composition_update) {
  if (!application.is_owner_thread()) {
    return wrong_thread_result();
  }
  const auto validation = validate_utf8(utf8);
  if (!validation.valid) {
    return invalid_utf8_result(validation.invalid_offset);
  }
  if ((!composition_update && utf8.empty()) ||
      !box_wants_text_input(&application.box())) {
    return {};
  }
  Event event = composition_update
                    ? Event::composition_update(std::string(utf8))
                    : Event::text_input(std::string(utf8));
  auto result = application.dispatch_event(event);
  if (!result) {
    return {false, std::move(result.error)};
  }
  return {true, {}};
}

HostApplicationDispatchResult dispatch_if_focused(DesktopApplication &application, Event event) {
  if (!application.is_owner_thread()) {
    return wrong_thread_result();
  }
  if ((event.type == EventType::TextInput && event.text.empty()) ||
      !box_wants_text_input(&application.box())) {
    return {};
  }

  auto result = application.dispatch_event(event);
  if (!result) {
    return {false, std::move(result.error)};
  }
  return {true, {}};
}

} // namespace

HostApplicationDispatchResult dispatch_text_input_if_focused(DesktopApplication &application,
                                                             std::string_view utf8) {
  return dispatch_utf8_if_focused(application, utf8, false);
}

HostApplicationDispatchResult
dispatch_composition_start_if_focused(DesktopApplication &application) {
  return dispatch_if_focused(application, Event::composition_start());
}

HostApplicationDispatchResult
dispatch_composition_update_if_focused(DesktopApplication &application, std::string_view text) {
  return dispatch_utf8_if_focused(application, text, true);
}

HostApplicationDispatchResult dispatch_composition_end_if_focused(DesktopApplication &application) {
  return dispatch_if_focused(application, Event::composition_end());
}

} // namespace flexUI::host
