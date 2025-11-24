#pragma once

#include "flexui/node.h" // Needed for FlexNode definition
#include "flexui/controller.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace flexui {

struct FlexScrollbarBinding {
  std::string track_id;
  std::string thumb_id;
  std::string content_id;  // The element to scroll
  float viewport_height = 600.f;
  float content_height = 2000.f;
  float thumb_size = 40.f;
  float value = 0.0f;  // 0.0 = top, 1.0 = bottom
  std::function<void(float)> on_change;
};

class FlexScrollbar : public FlexNode {
public:
  void registerScrollbar(FlexScrollbarBinding binding);

  void handleEvent(const SDL_Event &event) override;

private:
  struct ScrollbarState {
    FlexScrollbarBinding binding;
    bool dragging = false;
    float track_top = 0.f;
    float track_height = 0.f;
  };

  void setValue(ScrollbarState &state, float value);
  void updateStyles(ScrollbarState &state);
  void updateContentPosition(ScrollbarState &state);
  ScrollbarState *hitTestThumb(float x, float y);
  ScrollbarState *hitTestTrack(float x, float y);

  std::unordered_map<std::string, ScrollbarState> m_scrollbars;
  std::string m_active_id;
}; // Don't forget the semicolon after class definition

} // namespace flexui
