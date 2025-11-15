#include "flexui/widgets/toggle.h"

#include "flexui/document.h"

namespace flexui {

void FlexToggle::registerToggle(FlexToggleBinding binding) {
  if (binding.track_id.empty()) {
    return;
  }

  if (binding.track_id.empty()) {
    return;
  }
  ToggleState state;
  state.binding = std::move(binding);
  state.on = state.binding.initial_on;

  m_toggles[state.binding.track_id] = std::move(state);

  if (document()) {
    auto &stored = m_toggles[state.binding.track_id];
    document()->setClass(stored.binding.track_id, "toggle-on", stored.on);
    const std::string &value = stored.on ? stored.binding.handle_on_value
                                         : stored.binding.handle_off_value;
    if (!stored.binding.handle_id.empty() && !value.empty()) {
      document()->setStyle(stored.binding.handle_id,
                           stored.binding.handle_property, value);
    }
  }
}

void FlexToggle::handleEvent(const SDL_Event &event) {
  if (!document()) {
    return;
  }

  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    auto *state = hitTest(static_cast<float>(event.button.x),
                          static_cast<float>(event.button.y));
    if (state) {
      setState(*state, !state->on);
    }
  }
}

void FlexToggle::setState(ToggleState &state, bool on) {
  if (state.on == on || !document()) {
    return;
  }
  state.on = on;
  document()->setClass(state.binding.track_id, "toggle-on", state.on);
  const std::string &value =
      state.on ? state.binding.handle_on_value : state.binding.handle_off_value;
  if (!state.binding.handle_id.empty() && !value.empty()) {
    document()->setStyle(state.binding.handle_id, state.binding.handle_property,
                         value);
  }
  if (state.binding.on_change) {
    state.binding.on_change(state.on);
  }
}

FlexToggle::ToggleState *
FlexToggle::hitTest(float x, float y) {
  for (auto &entry : m_toggles) {
    if (document()->hitTest(entry.first, x, y)) {
      return &entry.second;
    }
  }
  return nullptr;
}

} // namespace flexui
