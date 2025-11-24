#pragma once

#include "flexui/controller.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace flexui {

struct FlexTabItem {
  std::string tab_id;
  std::string panel_id;
  std::string label;
};

struct FlexTabsBinding {
  std::string container_id;
  std::vector<FlexTabItem> tabs;
  int initial_active = 0;
  std::function<void(int)> on_change;
};

class FlexTabs : public FlexController {
public:
  void registerTabs(FlexTabsBinding binding);

  void setActiveTab(const std::string &container_id, int index);
  int getActiveTab(const std::string &container_id) const;

  void handleEvent(const SDL_Event &event) override;

private:
  struct TabsState {
    FlexTabsBinding binding;
    int active_index = 0;
  };

  TabsState *hitTest(float x, float y, int &tab_index);
  void updateTabStates(TabsState &state);

  std::unordered_map<std::string, TabsState> m_tabs;
};

} // namespace flexui
