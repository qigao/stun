#pragma once

#include "flexui/controller.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace flexui {

struct FlexToggleBinding {
  std::string track_id;
  std::string handle_id;
  std::string handle_property = "left";
  std::string handle_on_value;
  std::string handle_off_value;
  bool initial_on = true;
  std::function<void(bool)> on_change;
};

class FlexToggle : public Flex {
public:
  void registerToggle(FlexToggleBinding binding);

  void handleEvent(const SDL_Event &event) override;

private:
  struct ToggleState {
    FlexToggleBinding binding;
    bool on = true;
  };

  void setState(ToggleState &state, bool on);
  ToggleState *hitTest(float x, float y);

  std::unordered_map<std::string, ToggleState> m_toggles;
};

} // namespace flexui
