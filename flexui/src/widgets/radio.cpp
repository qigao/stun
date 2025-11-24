#include "flexui/widgets/radio.h"

#include "flexui/document.h"
#include <fmtlog.h>
#include <nanovg_css.h>

namespace flexui {

void FlexRadio::registerRadio(FlexRadioBinding binding) {
  if (binding.element_id.empty() || binding.group_id.empty()) {
    return;
  }

  std::string element_id = binding.element_id; // Save ID before move
  std::string group_id = binding.group_id; // Save group ID before move
  bool initial_checked = binding.initial_checked; // Save initial state

  RadioState state;
  state.binding = std::move(binding);
  state.checked = initial_checked;
  m_radios[element_id] = std::move(state);
  m_groups[group_id].push_back(element_id);

  if (getDocument()) {
    getDocument()->setClass(element_id, "radio-checked", initial_checked);
  }
}

void FlexRadio::handleEvent(const SDL_Event &event) {
  if (!getDocument()) {
    return;
  }
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
    auto *state = hitTest(static_cast<float>(event.button.x), static_cast<float>(event.button.y));
    if (state && !state->checked) {
      setChecked(state->binding.group_id, state->binding.element_id);
    }
  }
}

void FlexRadio::setChecked(const std::string &group_id, const std::string &element_id) {
  if (!getDocument()) return;

  auto group_it = m_groups.find(group_id);
  if (group_it == m_groups.end()) {
    loge("setChecked: group '{}' not found", group_id);
    return;
  }

  logi("Radio setChecked: group='{}', selecting='{}'", group_id, element_id);

  bool needs_recompute = false;
  for (const auto &radio_id : group_it->second) {
    auto radio_it = m_radios.find(radio_id);
    if (radio_it == m_radios.end()) continue;

    const bool selected = (radio_id == element_id);
    if (radio_it->second.checked != selected) {
        radio_it->second.checked = selected;
        getDocument()->setClass(radio_id, "radio-checked", selected);
        needs_recompute = true;
    }

    if (selected && radio_it->second.binding.on_selected) {
      radio_it->second.binding.on_selected();
    }
  }

  if (needs_recompute) {
    NVGCSSRenderer* renderer = getDocument()->renderer();
    if (renderer) {
        nvgcssComputeLayout(renderer);
    }
  }
}

FlexRadio::RadioState *FlexRadio::hitTest(float x, float y) {
  for (auto &entry : m_radios) {
    if (getDocument()->hitTest(entry.first, x, y)) {
      return &entry.second;
    }
  }
  return nullptr;
}

} // namespace flexui
