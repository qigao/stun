#include "flexui/widgets/checkbox.h"

#include "flexui/document.h"

namespace flexui {

void FlexCheckbox::registerCheckbox(FlexCheckboxBinding binding) {
  if (binding.element_id.empty()) {
    return;
  }

  CheckboxState state;
  state.binding = std::move(binding);
  state.checked = state.binding.initial_checked;
  m_checkboxes[state.binding.element_id] = std::move(state);

  if (document()) {
    document()->setClass(state.binding.element_id, "checkbox-checked",
                         m_checkboxes[state.binding.element_id].checked);
  }
}

void FlexCheckbox::handleEvent(const SDL_Event &event) {
  if (!document()) {
    return;
  }

  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    auto *state = hitTest(static_cast<float>(event.button.x), static_cast<float>(event.button.y));
    if (state) {
      setState(*state, !state->checked);
    }
  }
}

FlexCheckbox::CheckboxState *FlexCheckbox::hitTest(float x, float y) {
  for (auto &entry : m_checkboxes) {
    if (document()->hitTest(entry.first, x, y)) {
      return &entry.second;
    }
  }
  return nullptr;
}

void FlexCheckbox::setState(CheckboxState &state, bool checked) {
  if (!document() || state.checked == checked) {
    return;
  }
  state.checked = checked;
  document()->setClass(state.binding.element_id, "checkbox-checked", state.checked);
  if (state.binding.on_change) {
    state.binding.on_change(state.checked);
  }
}

} // namespace flexui
