#include "flexui/widgets/slider.h"

#include "flexui/document.h"

#include <algorithm>
#include <cstdio>

namespace flexui {

namespace {
std::string px(float value) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.2fpx", value);
  return buffer;
}
} // namespace

void FlexSlider::registerSlider(FlexSliderBinding binding) {
  if (binding.track_id.empty()) {
    return;
  }

  SliderState state;
  state.binding = std::move(binding);
  state.binding.value = std::clamp(state.binding.value, 0.0f, 1.0f);
  m_sliders[state.binding.track_id] = std::move(state);

  if (document()) {
    updateStyles(m_sliders[state.binding.track_id]);
  }
}

void FlexSlider::handleEvent(const SDL_Event &event) {
  if (!document()) {
    return;
  }

  switch (event.type) {
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    auto *state = hitTest(static_cast<float>(event.button.x), static_cast<float>(event.button.y));
    if (state) {
      setValue(*state, (static_cast<float>(event.button.x) - state->binding.track_left) /
                           state->binding.track_width);
      state->dragging = true;
      m_active_id = state->binding.track_id;
    }
    break;
  }
  case SDL_EVENT_MOUSE_BUTTON_UP:
    if (!m_active_id.empty()) {
      auto it = m_sliders.find(m_active_id);
      if (it != m_sliders.end()) {
        it->second.dragging = false;
      }
      m_active_id.clear();
    }
    break;
  case SDL_EVENT_MOUSE_MOTION:
    if (!m_active_id.empty()) {
      auto it = m_sliders.find(m_active_id);
      if (it != m_sliders.end() && it->second.dragging) {
        setValue(it->second, (static_cast<float>(event.motion.x) - it->second.binding.track_left) /
                                 it->second.binding.track_width);
      }
    }
    break;
  default:
    break;
  }
}

void FlexSlider::setValue(SliderState &state, float value) {
  value = std::clamp(value, 0.0f, 1.0f);
  if (state.binding.value == value) {
    return;
  }
  state.binding.value = value;
  updateStyles(state);
  if (state.binding.on_change) {
    state.binding.on_change(state.binding.value);
  }
}

FlexSlider::SliderState *FlexSlider::hitTest(float x, float y) {
  for (auto &entry : m_sliders) {
    const auto &binding = entry.second.binding;
    const float thumb_left =
        binding.track_left + binding.value * binding.track_width - binding.thumb_size * 0.5f;
    const float thumb_right = thumb_left + binding.thumb_size;
    if (x >= thumb_left && x <= thumb_right) {
      return &entry.second;
    }
    if (x >= binding.track_left && x <= binding.track_left + binding.track_width) {
      return &entry.second;
    }
  }
  return nullptr;
}

void FlexSlider::updateStyles(SliderState &state) {
  if (!document()) {
    return;
  }

  const float fill_width = state.binding.value * state.binding.track_width;
  document()->setStyle(state.binding.fill_id, "width", px(fill_width));

  const float thumb_left = state.binding.track_left + fill_width - state.binding.thumb_size * 0.5f;
  document()->setStyle(state.binding.thumb_id, "left", px(thumb_left));
}

} // namespace flexui
