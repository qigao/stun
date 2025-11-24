#include "flexui/widgets/alert.h"

#include "flexui/document.h"

namespace flexui {

void FlexAlert::registerAlert(FlexAlertBinding binding) {
  if (binding.alert_id.empty()) {
    return;
  }

  AlertState state;
  state.binding = std::move(binding);
  state.visible = false;
  m_alerts[state.binding.alert_id] = std::move(state);

  if (getDocument()) {
    updateAlertState(m_alerts[state.binding.alert_id]);
  }
}

void FlexAlert::showAlert(const std::string &alert_id) {
  auto it = m_alerts.find(alert_id);
  if (it == m_alerts.end()) {
    return;
  }

  it->second.visible = true;
  it->second.time_visible = 0.0f;
  updateAlertState(it->second);
}

void FlexAlert::dismissAlert(const std::string &alert_id) {
  auto it = m_alerts.find(alert_id);
  if (it == m_alerts.end() || !it->second.visible) {
    return;
  }

  it->second.visible = false;
  it->second.time_visible = 0.0f;
  updateAlertState(it->second);

  if (it->second.binding.on_dismiss) {
    it->second.binding.on_dismiss();
  }
}

bool FlexAlert::isVisible(const std::string &alert_id) const {
  auto it = m_alerts.find(alert_id);
  if (it == m_alerts.end()) {
    return false;
  }
  return it->second.visible;
}

void FlexAlert::handleEvent(const SDL_Event &event) {
  if (!getDocument()) {
    return;
  }

  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    const float x = static_cast<float>(event.button.x);
    const float y = static_cast<float>(event.button.y);

    for (auto &entry : m_alerts) {
      auto &state = entry.second;
      if (!state.visible || !state.binding.dismissible) {
        continue;
      }

      // Check close button click
      if (!state.binding.close_button_id.empty() &&
          getDocument()->hitTest(state.binding.close_button_id, x, y)) {
        dismissAlert(entry.first);
        return;
      }
    }
  }
}

void FlexAlert::onDocumentAttached(FlexDocument *doc) {
  FlexController::onDocumentAttached(doc);
  if (doc) {
    for (auto &entry : m_alerts) {
      updateAlertState(entry.second);
    }
  }
}

void FlexAlert::update(float dt) {
  if (!getDocument()) {
    return;
  }

  for (auto &entry : m_alerts) {
    auto &state = entry.second;
    if (!state.visible) {
      continue;
    }

    // Check auto-dismiss
    if (state.binding.auto_dismiss_time > 0.0f) {
      state.time_visible += dt;
      if (state.time_visible >= state.binding.auto_dismiss_time) {
        dismissAlert(entry.first);
      }
    }
  }
}

void FlexAlert::updateAlertState(AlertState &state) {
  if (!getDocument()) {
    return;
  }

  // Update visibility
  getDocument()->setClass(state.binding.alert_id, "alert-visible", state.visible);
  getDocument()->setClass(state.binding.alert_id, "alert-hidden", !state.visible);

  // Update type classes
  const char *type_class = nullptr;
  switch (state.binding.type) {
  case FlexAlertType::Info:
    type_class = "alert-info";
    break;
  case FlexAlertType::Success:
    type_class = "alert-success";
    break;
  case FlexAlertType::Warning:
    type_class = "alert-warning";
    break;
  case FlexAlertType::Error:
    type_class = "alert-error";
    break;
  }

  if (type_class) {
    getDocument()->setClass(state.binding.alert_id, type_class, true);
  }

  // Update message
  if (!state.binding.message.empty()) {
    getDocument()->setText(state.binding.alert_id, state.binding.message);
  }
}

} // namespace flexui
