#include "flexui/widgets/slider.h"

#include <nanovg_css.h>
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

void FlexSlider::onDocumentAttached(FlexDocument *document) {
  FlexNode::onDocumentAttached(document); // Call base class implementation
  for (auto &entry : m_sliders) {
    updateStyles(entry.second);
  }
}

void FlexSlider::registerSlider(FlexSliderBinding binding) {
  if (binding.track_id.empty()) {
    return;
  }

  SliderState state;
  state.binding = std::move(binding);
  state.binding.value = std::clamp(state.binding.value, 0.0f, 1.0f);
  m_sliders[state.binding.track_id] = std::move(state);

}

void FlexSlider::handleEvent(const SDL_Event &event) {
  auto doc = this->document();
  if (!doc) {
    return;
  }

  switch (event.type) {
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    auto *state = hitTest(static_cast<float>(event.button.x), static_cast<float>(event.button.y));
    if (state) {
      // Get actual track position from computed layout
      float track_left = state->binding.track_left;
      float track_width = state->binding.track_width;
      auto track_node = doc->findNode(state->binding.track_id);
      if (track_node && track_node->element()) {
        track_left = track_node->element()->computed.x;
        track_width = track_node->element()->computed.width;
      }
      setValue(*state, (static_cast<float>(event.button.x) - track_left) / track_width);
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
        // Get actual track position from computed layout
        float track_left = it->second.binding.track_left;
        float track_width = it->second.binding.track_width;
        auto track_node = doc->findNode(it->second.binding.track_id);
        if (track_node && track_node->element()) {
          track_left = track_node->element()->computed.x;
          track_width = track_node->element()->computed.width;
        }
        setValue(it->second, (static_cast<float>(event.motion.x) - track_left) / track_width);
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
  auto doc = this->document();
  if (!doc) {
    return nullptr;
  }
  for (auto &entry : m_sliders) {
    // Use document hitTest for proper bounds checking
    if (doc->hitTest(entry.second.binding.track_id, x, y) ||
        doc->hitTest(entry.second.binding.thumb_id, x, y)) {
      return &entry.second;
    }
  }
  return nullptr;
}

void FlexSlider::updateStyles(SliderState &state) {
  auto doc = this->document();
  if (!doc) {
    return;
  }

  // Get actual track position from computed layout
  float track_left = state.binding.track_left;
  float track_width = state.binding.track_width;
  auto track_node = doc->findNode(state.binding.track_id);
  if (track_node && track_node->element()) {
    track_left = track_node->element()->computed.x;
    track_width = track_node->element()->computed.width;
  }

  const float fill_width = state.binding.value * track_width;
  doc->setAttribute(state.binding.fill_id, "style", "width: " + px(fill_width) + ";");

  const float thumb_left = track_left + fill_width - state.binding.thumb_size * 0.5f;
  doc->setAttribute(state.binding.thumb_id, "style", "left: " + px(thumb_left) + ";");
}

} // namespace flexui
