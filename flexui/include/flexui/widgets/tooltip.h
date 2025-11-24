#pragma once

#include "flexui/controller.h"

#include <string>
#include <unordered_map>

namespace flexui {

struct FlexTooltipBinding {
  std::string target_id;
  std::string tooltip_id;
  std::string text;
  float delay = 0.5f; // Delay before showing tooltip (seconds)
};

class FlexTooltip : public FlexController {
public:
  void registerTooltip(FlexTooltipBinding binding);

  void handleEvent(const SDL_Event &event) override;
  void update(float dt) override;

private:
  struct TooltipState {
    FlexTooltipBinding binding;
    bool hovering = false;
    bool visible = false;
    float hover_time = 0.0f;
    float mouse_x = 0.0f;
    float mouse_y = 0.0f;
  };

  void showTooltip(TooltipState &state);
  void hideTooltip(TooltipState &state);
  void updateTooltipPosition(TooltipState &state);

  std::unordered_map<std::string, TooltipState> m_tooltips;
};

} // namespace flexui
