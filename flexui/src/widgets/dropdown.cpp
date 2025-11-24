#include "flexui/widgets/dropdown.h"

#include "flexui/document.h"

namespace flexui {

void FlexDropdown::registerDropdown(FlexDropdownBinding binding) {
  if (binding.container_id.empty()) {
    return;
  }

  DropdownState state;
  state.binding = std::move(binding);
  m_dropdowns[state.binding.container_id] = std::move(state);

  if (getDocument()) {
    getDocument()->setText(m_dropdowns[state.binding.container_id].binding.display_id,
                        m_dropdowns[state.binding.container_id].binding.placeholder);
  }
}

void FlexDropdown::handleEvent(const SDL_Event &event) {
  if (!getDocument()) {
    return;
  }

  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    const float x = static_cast<float>(event.button.x);
    const float y = static_cast<float>(event.button.y);

    // Check if clicking on dropdown container/button to toggle
    if (auto *state = hitTest(x, y)) {
      setOpen(*state, !state->open);
      return;
    }

    // Check if clicking on an option (only when dropdown is open)
    for (auto &entry : m_dropdowns) {
      if (!entry.second.open) {
        continue;  // Skip closed dropdowns
      }
      for (const auto &option : entry.second.binding.options) {
        if (getDocument()->hitTest(option.id, x, y)) {
          selectOption(entry.second, option);
          return;
        }
      }
    }

    // Click outside - close all dropdowns
    for (auto &entry : m_dropdowns) {
      setOpen(entry.second, false);
    }
  }
}

void FlexDropdown::setOpen(DropdownState &state, bool open) {
  if (!getDocument() || state.open == open) {
    return;
  }
  state.open = open;

  // Update container class for styling
  getDocument()->setClass(state.binding.container_id, "dropdown-open", state.open);

  // Show/hide menu
  if (!state.binding.menu_id.empty()) {
    getDocument()->setClass(state.binding.menu_id, "open", state.open);
  }
}

void FlexDropdown::selectOption(
    DropdownState &state, const FlexDropdownOptionBinding &option) {
  if (!getDocument()) {
    return;
  }
  state.selected_option_id = option.id;
  getDocument()->setText(state.binding.display_id, option.text);
  setOpen(state, false);
  if (state.binding.on_select) {
    state.binding.on_select(option.value.empty() ? option.text : option.value);
  }
}

FlexDropdown::DropdownState *
FlexDropdown::hitTest(float x, float y) {
  for (auto &entry : m_dropdowns) {
    // Only toggle on button click, or on the whole container if clicking to open
    if (getDocument()->hitTest(entry.second.binding.button_id, x, y)) {
      return &entry.second;
    }
    // Also allow clicking display area or container to open
    if (getDocument()->hitTest(entry.first, x, y) ||
        getDocument()->hitTest(entry.second.binding.display_id, x, y)) {
      return &entry.second;
    }
  }
  return nullptr;
}

} // namespace flexui
