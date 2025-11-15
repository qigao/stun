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

  if (document()) {
    document()->setText(m_dropdowns[state.binding.container_id].binding.display_id,
                        m_dropdowns[state.binding.container_id].binding.placeholder);
  }
}

void FlexDropdown::handleEvent(const SDL_Event &event) {
  if (!document()) {
    return;
  }

  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    const float x = static_cast<float>(event.button.x);
    const float y = static_cast<float>(event.button.y);
    if (auto *state = hitTest(x, y)) {
      setOpen(*state, !state->open);
      return;
    }

    for (auto &entry : m_dropdowns) {
      for (const auto &option : entry.second.binding.options) {
        if (document()->hitTest(option.id, x, y)) {
          selectOption(entry.second, option);
          return;
        }
      }
    }

    for (auto &entry : m_dropdowns) {
      setOpen(entry.second, false);
    }
  }
}

void FlexDropdown::setOpen(DropdownState &state, bool open) {
  if (!document() || state.open == open) {
    return;
  }
  state.open = open;
  document()->setClass(state.binding.container_id, "dropdown-open", state.open);
}

void FlexDropdown::selectOption(
    DropdownState &state, const FlexDropdownOptionBinding &option) {
  if (!document()) {
    return;
  }
  state.selected_option_id = option.id;
  document()->setText(state.binding.display_id, option.text);
  setOpen(state, false);
  if (state.binding.on_select) {
    state.binding.on_select(option.value.empty() ? option.text : option.value);
  }
}

FlexDropdown::DropdownState *
FlexDropdown::hitTest(float x, float y) {
  for (auto &entry : m_dropdowns) {
    if (document()->hitTest(entry.first, x, y) ||
        document()->hitTest(entry.second.binding.display_id, x, y) ||
        document()->hitTest(entry.second.binding.button_id, x, y)) {
      return &entry.second;
    }
  }
  return nullptr;
}

} // namespace flexui

