#pragma once

#include "flexui/controller.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace flexui {

enum class FlexPointerEventType { HoverEnter, HoverLeave, Pressed, Released, Clicked };

struct FlexPointerEvent {
  FlexPointerEventType type;
  float x = 0.f;
  float y = 0.f;
  Uint8 button = 0;
};

struct FlexPointerBinding {
  std::string element_id;
  bool apply_hover_pseudo = true;
  bool apply_active_pseudo = true;
  std::function<void(const FlexPointerEvent &)> callback;
};

class FlexPointer : public Flex {
public:
  void addBinding(FlexPointerBinding binding);

  void handleEvent(const SDL_Event &event) override;

private:
  struct BindingState {
    FlexPointerBinding binding;
    bool hovering = false;
    bool active = false;
  };

  void processMotion(float x, float y);
  void processButton(bool pressed, float x, float y, Uint8 button);
  void setHoverState(BindingState &state, bool hovering, float x, float y);
  void setActiveState(BindingState &state, bool active, float x, float y, Uint8 button);

  std::unordered_map<std::string, BindingState> m_bindings;
};

} // namespace flexui
