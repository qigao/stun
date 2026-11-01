#pragma once

#include <charts/ui/graph_edit_document.h>
#include <flexUI/widget_registry.h>

#include <memory>

namespace charts::ui {

// One graph-document world→Widget-local transform. It does NOT include the
// host Element's absolute offset, which EventDispatcher/RenderManager own.
struct GraphCanvasViewport {
  double zoom = 1.0;
  double pan_x = 0.0;
  double pan_y = 0.0;

  bool to_local(double world_x, double world_y,
                double &local_x, double &local_y) const noexcept;
  bool to_world(double local_x, double local_y,
                double &world_x, double &world_y) const noexcept;
  bool zoom_at(double local_x, double local_y, double wheel_delta) noexcept;
};

// Register one LINKED native GraphCanvas in the existing FlexUI registry.
// Every mounted Widget has its own view/gesture/capture state; only the
// application owns GraphEditDocument and its committed geometry/history.
// This is not an ImGui bridge, dynamic Widget DLL ABI, or external GPU surface.
flexUI::WidgetRegistryError register_graph_canvas_widget(
    flexUI::WidgetRegistry &registry,
    std::shared_ptr<GraphEditDocument> document);

} // namespace charts::ui
