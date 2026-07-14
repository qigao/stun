#ifndef FLEXUI_EXAMPLES_HOST_INPUT_BRIDGE_H
#define FLEXUI_EXAMPLES_HOST_INPUT_BRIDGE_H

#include <flexUI/host_bridge.h>

namespace flexui_examples {

using flexUI::host::box_wants_text_input;
using flexUI::host::clear_focus;
using flexUI::host::clear_focus_and_capture;
using flexUI::host::clear_mouse_capture;
using flexUI::host::dispatch_composition_end_if_focused;
using flexUI::host::dispatch_composition_start_if_focused;
using flexUI::host::dispatch_composition_update_if_focused;
using flexUI::host::dispatch_text_input_if_focused;
using flexUI::host::focused_text_input_element;
using flexUI::host::focused_text_input_caret_anchor;
using flexUI::host::sync_mouse_capture;

}  // namespace flexui_examples

#endif // FLEXUI_EXAMPLES_HOST_INPUT_BRIDGE_H
