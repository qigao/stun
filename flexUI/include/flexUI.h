/*
 * flexUI - Main Header
 *
 * Include this single header to use flexUI.
 * For minimal builds, include only flexUI/box.h and specific widgets.
 */

#pragma once

// Public API (users need these)
#include "flexUI/types.h"
#include "flexUI/event.h"
#include "flexUI/element.h"
#include "flexUI/widget.h"
#include "flexUI/box.h"
#include "flexUI/binding_runtime.h"
#include "flexUI/keyed_repeater.h"
#include "flexUI/ui_document.h"
#include "flexUI/text_value_widget.h"
#include "flexUI/renderer.h"
#include "flexUI/render_command.h"
#include "flexUI/render_frame.h"
#include "flexUI/view_pipeline.h"
#include "flexUI/utility_jit.h"

// Scene Graph (optional, for custom rendering)
#include "flexUI/group.h"
#include "flexUI/shapes.h"

// Animation
#include "flexUI/transition.h"

// Widgets
#include "flexUI/widgets/label_widget.h"
#include "flexUI/widgets/button_widget.h"
#include "flexUI/widgets/input_widget.h"
#include "flexUI/widgets/checkbox_widget.h"
#include "flexUI/widgets/radio_widget.h"
#include "flexUI/widgets/switch_widget.h"
#include "flexUI/widgets/slider_widget.h"
#include "flexUI/widgets/progressbar_widget.h"
#include "flexUI/widgets/select_widget.h"
#include "flexUI/widgets/dropdown_widget.h"
#include "flexUI/widgets/tabs_widget.h"
#include "flexUI/widgets/spinner_widget.h"
#include "flexUI/widgets/card_widget.h"
#include "flexUI/widgets/markdown_widget.h"
