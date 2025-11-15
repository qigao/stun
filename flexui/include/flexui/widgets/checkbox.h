#pragma once

#include "flexui/controller.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace flexui {

struct FlexCheckboxBinding {
  std::string element_id;
  bool initial_checked = false;
  std::function<void(bool)> on_change;
};

class FlexCheckbox : public Flex {
public:
  void registerCheckbox(FlexCheckboxBinding binding);

  void handleEvent(const SDL_Event &event) override;

private:
  struct CheckboxState {
    FlexCheckboxBinding binding;
    bool checked = false;
  };

  CheckboxState *hitTest(float x, float y);
  void setState(CheckboxState &state, bool checked);

  std::unordered_map<std::string, CheckboxState> m_checkboxes;
};

} // namespace flexui
