#pragma once

#include "flexui/controller.h"

#include <SDL3/SDL.h> // Required for SDL_Event
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace flexui {

struct FlexDropdownOptionBinding {
  std::string id;
  std::string text;
  std::string value;
};

struct FlexDropdownBinding {
  std::string container_id;
  std::string display_id;
  std::string button_id;
  std::string menu_id;
  std::vector<FlexDropdownOptionBinding> options;
  std::string placeholder = "Select";
  std::function<void(const std::string &value)> on_select;
};

class FlexDropdown : public FlexController {
public:
  void registerDropdown(FlexDropdownBinding binding);

  void handleEvent(const SDL_Event &event) override;

private:
  struct DropdownState {
    FlexDropdownBinding binding;
    bool open = false;
    std::string selected_option_id;
  };

  void setOpen(DropdownState &state, bool open);
  void selectOption(DropdownState &state, const FlexDropdownOptionBinding &option);
  DropdownState *hitTest(float x, float y);
  DropdownState *findByOptionId(const std::string &id);

  std::unordered_map<std::string, DropdownState> m_dropdowns;
};

} // namespace flexui
