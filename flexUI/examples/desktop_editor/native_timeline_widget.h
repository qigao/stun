#pragma once

#include <flexUI/widget_registry.h>

#include <array>
#include <memory>

namespace flexui_demo {

struct TimelineClip {
  int start_frame = 0;
  int length_frames = 1;
};

/// Application-owned source of truth; per-widget pointer gesture state is
/// separate and is never serialized into the XML Element tree.
struct TimelineState {
  std::array<TimelineClip, 2> clips{{{8, 10}, {24, 10}}};
};

/// Register a linked native widget into the one application WidgetRegistry.
flexUI::WidgetRegistryError register_timeline_widget(
    flexUI::WidgetRegistry &registry, std::shared_ptr<TimelineState> model);

} // namespace flexui_demo
