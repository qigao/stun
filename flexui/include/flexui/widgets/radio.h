#pragma once

#include "flexui/controller.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace flexui {

struct FlexRadioBinding {
  std::string element_id;
  std::string group_id;
  bool initial_checked = false;
  std::function<void()> on_selected;
};

class FlexRadio : public FlexController {
public:
  void registerRadio(FlexRadioBinding binding);

  void handleEvent(const SDL_Event &event) override;

private:
  struct RadioState {
    FlexRadioBinding binding;
    bool checked = false;
  };

  void setChecked(const std::string &group_id, const std::string &element_id);
  RadioState *hitTest(float x, float y);

  std::unordered_map<std::string, RadioState> m_radios;
  std::unordered_map<std::string, std::vector<std::string>> m_groups;
}; // Don't forget the semicolon after class definition

} // namespace flexui
