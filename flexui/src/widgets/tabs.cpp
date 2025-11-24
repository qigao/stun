#include "flexui/widgets/tabs.h"

#include "flexui/document.h"

#include <algorithm>

namespace flexui {

void FlexTabs::registerTabs(FlexTabsBinding binding) {
  if (binding.container_id.empty() || binding.tabs.empty()) {
    return;
  }

  TabsState state;
  state.binding = std::move(binding);
  state.active_index = std::clamp(state.binding.initial_active, 0,
                                  static_cast<int>(state.binding.tabs.size()) - 1);
  m_tabs[state.binding.container_id] = std::move(state);

  if (getDocument()) {
    updateTabStates(m_tabs[state.binding.container_id]);
  }
}

void FlexTabs::setActiveTab(const std::string &container_id, int index) {
  auto it = m_tabs.find(container_id);
  if (it == m_tabs.end()) {
    return;
  }

  auto &state = it->second;
  index = std::clamp(index, 0, static_cast<int>(state.binding.tabs.size()) - 1);
  
  if (state.active_index == index) {
    return;
  }

  state.active_index = index;
  updateTabStates(state);

  if (state.binding.on_change) {
    state.binding.on_change(state.active_index);
  }
}

int FlexTabs::getActiveTab(const std::string &container_id) const {
  auto it = m_tabs.find(container_id);
  if (it == m_tabs.end()) {
    return -1;
  }
  return it->second.active_index;
}

void FlexTabs::handleEvent(const SDL_Event &event) {
  if (!getDocument()) {
    return;
  }

  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    int tab_index = -1;
    auto *state = hitTest(static_cast<float>(event.button.x), 
                         static_cast<float>(event.button.y), tab_index);
    if (state && tab_index >= 0) {
      setActiveTab(state->binding.container_id, tab_index);
    }
  }
}

FlexTabs::TabsState *FlexTabs::hitTest(float x, float y, int &tab_index) {
  for (auto &entry : m_tabs) {
    auto &state = entry.second;
    for (size_t i = 0; i < state.binding.tabs.size(); ++i) {
      if (getDocument()->hitTest(state.binding.tabs[i].tab_id, x, y)) {
        tab_index = static_cast<int>(i);
        return &state;
      }
    }
  }
  tab_index = -1;
  return nullptr;
}

void FlexTabs::updateTabStates(TabsState &state) {
  if (!getDocument()) {
    return;
  }

  // Update tab and panel visibility/active states
  for (size_t i = 0; i < state.binding.tabs.size(); ++i) {
    const auto &tab = state.binding.tabs[i];
    const bool is_active = (static_cast<int>(i) == state.active_index);

    // Set active class on tab
    getDocument()->setClass(tab.tab_id, "tab-active", is_active);

    // Show/hide panel
    if (!tab.panel_id.empty()) {
      getDocument()->setClass(tab.panel_id, "tab-panel-active", is_active);
      getDocument()->setClass(tab.panel_id, "tab-panel-hidden", !is_active);
    }
  }
}

} // namespace flexui
