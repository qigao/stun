#include "flexui/widgets/progress_bar.h"
#include "flexui/document.h" // Needed for FlexDocument

#include <algorithm>
#include <cmath>
#include <utility> // For std::move

namespace flexui {

void FlexProgressBar::registerProgressBar(FlexProgressBarBinding binding) {
  if (binding.track_id.empty() || binding.fill_id.empty()) {
    return;
  }

  FlexProgressBar::ProgressBarState state;
  state.binding = std::move(binding);
  state.value = std::clamp(state.binding.initial_value, 0.0f, 1.0f);
  state.target_value = state.value;
  std::string track_id_key = state.binding.track_id; // Capture key before move
  m_progress_bars[track_id_key] = std::move(state);

  if (getDocument()) {
    updateProgressBar(m_progress_bars[track_id_key]);
  }
}

void FlexProgressBar::setValue(const std::string &track_id, float value) {
  auto it = m_progress_bars.find(track_id);
  if (it == m_progress_bars.end()) {
    return;
  }

  value = std::clamp(value, 0.0f, 1.0f);
  it->second.target_value = value;
  it->second.animating = std::abs(it->second.value - value) > 0.001f;
}

float FlexProgressBar::getValue(const std::string &track_id) const {
  auto it = m_progress_bars.find(track_id);
  if (it == m_progress_bars.end()) {
    return 0.0f;
  }
  return it->second.value;
}

void FlexProgressBar::onDocumentAttached(FlexDocument *document) {
  FlexController::onDocumentAttached(document); // Call base class method
  for (auto &entry : m_progress_bars) {
    updateProgressBar(entry.second);
  }
}

void FlexProgressBar::update(float dt) {
  if (!getDocument()) {
    return;
  }

  for (auto &entry : m_progress_bars) {
    auto &state = entry.second;
    if (!state.animating) {
      continue;
    }

    // Smooth animation towards target
    const float speed = 2.0f; // Animation speed
    const float diff = state.target_value - state.value;
    const float delta = diff * speed * dt;

    if (std::abs(diff) < 0.001f) {
      state.value = state.target_value;
      state.animating = false;
    } else {
      state.value += delta;
    }

    updateProgressBar(state);

    if (state.binding.on_change) {
      state.binding.on_change(state.value);
    }
  }
}

void FlexProgressBar::updateProgressBar(FlexProgressBar::ProgressBarState &state) {
  std::string width_percent = std::to_string(state.value * 100.0f) + "%";
  if (getDocument()) {
      getDocument()->setAttribute(state.binding.fill_id, "style", "width: " + width_percent + ";");
  }
}

} // namespace flexui
