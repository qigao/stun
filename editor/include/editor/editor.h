/*
 * Flex Editor Framework
 *
 * MVVM-based architecture for building vector graphics editors.
 * Supports multiple scenarios: SVG designer, CAD, UI designer, etc.
 */

#pragma once

// Core
#include "core/types.h"
#include "core/observable.h"

// Model
#include "model/node.h"
#include "model/layer.h"
#include "model/document.h"

// Command
#include "command/command.h"
#include "command/transform_commands.h"
#include "command/boolean_commands.h"

// ViewModel
#include "viewmodel/selection.h"
#include "viewmodel/tool.h"
#include "viewmodel/editor_vm.h"

// Input
#include "input/shortcut.h"

// Tools
#include "tool/transform_gizmo.h"
#include "tool/select_tool.h"
#include "tool/draw_tool.h"
#include "tool/pen_tool.h"
#include "tool/text_tool.h"
#include "tool/path_editor.h"

// View
#include "view/panel.h"
#include "view/color_picker.h"
#include "view/gradient_picker.h"
#include "view/layer_panel.h"
#include "view/ruler_guide.h"
#include "view/effects_panel.h"
#include "view/toolbar.h"

#include "view/context_menu.h"
#include "view/history_panel.h"
#include "view/align_panel.h"
#include "view/zoom_panel.h"
#include "view/navigator_panel.h"
#include "view/grid.h"

// I/O
#include "io/serializer.h"
#include "io/exporter.h"
