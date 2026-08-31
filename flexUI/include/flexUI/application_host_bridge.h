#pragma once

#include "flexUI/application.h"

#include <string_view>

namespace flexUI::host {

/// Result of conditionally routing one host text or IME event.
///
/// A successful result with `dispatched == false` means the event was ignored
/// because no eligible text widget was focused, or text input was empty. An
/// error preserves the DesktopApplication owner-thread or dispatch failure.
struct HostApplicationDispatchResult {
  bool dispatched = false;
  DesktopApplicationError error;

  explicit operator bool() const noexcept { return !static_cast<bool>(error); }
};

/// Routes UTF-8 text when an editable, visible text widget owns focus.
/// @param application Application whose owner thread must call this function.
/// @param utf8 UTF-8 text copied into the event; empty input is ignored.
/// @return Whether routing succeeded and whether an event was dispatched.
HostApplicationDispatchResult dispatch_text_input_if_focused(DesktopApplication &application,
                                                             std::string_view utf8);

/// Starts IME composition when an editable, visible text widget owns focus.
/// @return WrongThread or an application dispatch error on failure.
HostApplicationDispatchResult
dispatch_composition_start_if_focused(DesktopApplication &application);

/// Updates IME composition when an editable, visible text widget owns focus.
/// @param text UTF-8 composition text copied into the event.
/// @return WrongThread or an application dispatch error on failure.
HostApplicationDispatchResult
dispatch_composition_update_if_focused(DesktopApplication &application, std::string_view text);

/// Ends IME composition when an editable, visible text widget owns focus.
/// @return WrongThread or an application dispatch error on failure.
HostApplicationDispatchResult dispatch_composition_end_if_focused(DesktopApplication &application);

} // namespace flexUI::host
