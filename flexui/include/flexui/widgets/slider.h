#pragma once

#include "flexui/controller.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace flexui {

struct FlexSliderBinding {
  std::string track_id;
  std::string fill_id;
  std::string thumb_id;
  float track_left = 0.f;
  float track_width = 300.f;
  float thumb_size = 24.f;
  float value = 0.5f;
  std::function<void(float)> on_change;
};

class FlexSlider : public Flex {
public:
  void registerSlider(FlexSliderBinding binding);

  void handleEvent(const SDL_Event &event) override;

private:
  struct SliderState {
    FlexSliderBinding binding;
    bool dragging = false;
  };

  void setValue(SliderState &state, float value);
  SliderState *hitTest(float x, float y);
  void updateStyles(SliderState &state);

  std::unordered_map<std::string, SliderState> m_sliders;
  std::string m_active_id;
};

} // namespace flexui
