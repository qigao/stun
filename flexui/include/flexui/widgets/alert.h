#pragma once

#include "flexui/controller.h"

#include <SDL3/SDL.h> // Required for SDL_Event
#include <functional>
#include <string>
#include <unordered_map>

namespace flexui {

enum class FlexAlertType {
  Info,
  Success,
  Warning,
  Error
};

struct FlexAlertBinding {
  std::string alert_id;
  std::string close_button_id;
  FlexAlertType type = FlexAlertType::Info;
  std::string message;
  bool dismissible = true;
  float auto_dismiss_time = 0.0f; // 0 = no auto dismiss
  std::function<void()> on_dismiss;
};

class FlexAlert : public FlexController {
public:
  void registerAlert(FlexAlertBinding binding);

  void showAlert(const std::string &alert_id);
  void dismissAlert(const std::string &alert_id);
  bool isVisible(const std::string &alert_id) const;

  void handleEvent(const SDL_Event &event) override;
  void update(float dt) override;
  void onDocumentAttached(FlexDocument *document) override; // Add onDocumentAttached

private:
  struct AlertState {
    FlexAlertBinding binding;
    bool visible = false;
    float time_visible = 0.0f;
  };

  void updateAlertState(AlertState &state);

  std::unordered_map<std::string, AlertState> m_alerts;
};

} // namespace flexui
