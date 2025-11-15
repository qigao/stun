#include "flexui/widgets/radio.h"

#include "flexui/document.h"

namespace flexui {

void FlexRadio::registerRadio(FlexRadioBinding binding) {
  if (binding.element_id.empty() || binding.group_id.empty()) {
    return;
  }

  RadioState state;
  state.binding = std::move(binding);
  state.checked = state.binding.initial_checked;
  m_radios[state.binding.element_id] = std::move(state);
  m_groups[state.binding.group_id].push_back(state.binding.element_id);

  if (document()) {
    document()->setClass(state.binding.element_id, "radio-checked", state.binding.initial_checked);
  }
}

void FlexRadio::handleEvent(const SDL_Event &event) {
  if (!document()) {
    return;
  }
  if (event.type != SDL_EVENT_MOUSE_BUTTON_DOWN) {
    return;
  }

  auto *state = hitTest(static_cast<float>(event.button.x), static_cast<float>(event.button.y));
  if (state && !state->checked) {
    setChecked(state->binding.group_id, state->binding.element_id);
  }
}

void FlexRadio::setChecked(const std::string &group_id, const std::string &element_id) {
  auto group_it = m_groups.find(group_id);
  if (group_it == m_groups.end() || !document()) {
    return;
  }

  for (const auto &radio_id : group_it->second) {
    auto radio_it = m_radios.find(radio_id);
    if (radio_it == m_radios.end()) {
      continue;
    }
    const bool selected = (radio_id == element_id);
    radio_it->second.checked = selected;
    document()->setClass(radio_id, "radio-checked", selected);
    if (selected && radio_it->second.binding.on_selected) {
      radio_it->second.binding.on_selected();
    }
  }
}

FlexRadio::RadioState *FlexRadio::hitTest(float x, float y) {
  for (auto &entry : m_radios) {
    if (document()->hitTest(entry.first, x, y)) {
      return &entry.second;
    }
  }
  return nullptr;
}

} // namespace flexui
