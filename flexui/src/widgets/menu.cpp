#include "flexui/widgets/menu.h"
#include "flexui/document.h"

namespace flexui {

void FlexMenu::registerMenu(FlexMenuBinding binding) {
  if (binding.menu_id.empty()) {
    return;
  }

  MenuState state;
  state.binding = std::move(binding);
  state.open = state.binding.open;
  m_menus[state.binding.menu_id] = std::move(state);

  if (document()) {
    updateMenuState(m_menus[state.binding.menu_id]);
  }
}

void FlexMenu::openMenu(const std::string &menu_id) {
  auto it = m_menus.find(menu_id);
  if (it == m_menus.end()) {
    return;
  }

  // Close all other menus
  for (auto &entry : m_menus) {
    if (entry.first != menu_id && entry.second.open) {
      entry.second.open = false;
      updateMenuState(entry.second);
    }
  }

  it->second.open = true;
  updateMenuState(it->second);
}

void FlexMenu::closeMenu(const std::string &menu_id) {
  auto it = m_menus.find(menu_id);
  if (it == m_menus.end()) {
    return;
  }

  it->second.open = false;
  updateMenuState(it->second);
}

void FlexMenu::toggleMenu(const std::string &menu_id) {
  auto it = m_menus.find(menu_id);
  if (it == m_menus.end()) {
    return;
  }

  if (it->second.open) {
    closeMenu(menu_id);
  } else {
    openMenu(menu_id);
  }
}

bool FlexMenu::isOpen(const std::string &menu_id) const {
  auto it = m_menus.find(menu_id);
  if (it == m_menus.end()) {
    return false;
  }
  return it->second.open;
}

void FlexMenu::handleEvent(const SDL_Event &event) {
  if (!document()) {
    return;
  }

  switch (event.type) {
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    const float x = static_cast<float>(event.button.x);
    const float y = static_cast<float>(event.button.y);

    // Check if clicking on a trigger
    auto *trigger_state = hitTestTrigger(x, y);
    if (trigger_state) {
      toggleMenu(trigger_state->binding.menu_id);
      return;
    }

    // Check if clicking on a menu item
    int item_index = -1;
    auto *menu_state = hitTestMenuItem(x, y, item_index);
    if (menu_state && item_index >= 0) {
      const auto &item = menu_state->binding.items[item_index];
      if (!item.disabled && !item.separator && item.on_click) {
        item.on_click();
      }
      closeMenu(menu_state->binding.menu_id);
      return;
    }

    // Click outside - close all menus
    for (auto &entry : m_menus) {
      if (entry.second.open) {
        closeMenu(entry.first);
      }
    }
    break;
  }
  default:
    break;
  }
}

void FlexMenu::onDocumentAttached(FlexDocument *document) {
  FlexNode::onDocumentAttached(document);
  // Re-attach existing menus to the new document
  for (auto &entry : m_menus) {
    updateMenuState(entry.second);
  }
}

FlexMenu::MenuState *FlexMenu::hitTestTrigger(float x, float y) {
  for (auto &entry : m_menus) {
    if (!entry.second.binding.trigger_id.empty() &&
        document()->hitTest(entry.second.binding.trigger_id, x, y)) {
      return &entry.second;
    }
  }
  return nullptr;
}

FlexMenu::MenuState *FlexMenu::hitTestMenuItem(float x, float y,
                                                int &item_index) {
  for (auto &entry : m_menus) {
    if (!entry.second.open) {
      continue;
    }

    for (size_t i = 0; i < entry.second.binding.items.size(); ++i) {
      const auto &item = entry.second.binding.items[i];
      if (document()->hitTest(item.id, x, y)) {
        item_index = static_cast<int>(i);
        return &entry.second;
      }
    }
  }
  item_index = -1;
  return nullptr;
}

void FlexMenu::updateMenuState(MenuState &state) {
  if (!document()) {
    return;
  }

  // Update menu visibility
  document()->setClass(state.binding.menu_id, "menu-open", state.open);
  document()->setClass(state.binding.menu_id, "menu-closed", !state.open);

  // Update trigger state
  if (!state.binding.trigger_id.empty()) {
    document()->setClass(state.binding.trigger_id, "menu-trigger-active",
                        state.open);
  }
}

} // namespace flexui
