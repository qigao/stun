#include "flexui/widgets/interaction.h"

#include <utility>

namespace flexui {

void FlexPointer::addBinding(FlexPointerBinding binding) {
  if (binding.element_id.empty()) {
    return;
  }
  m_bindings[binding.element_id] = BindingState{std::move(binding), false,
                                                false};
}

void FlexPointer::handleEvent(const SDL_Event &event) {
  switch (event.type) {
  case SDL_EVENT_MOUSE_MOTION:
    processMotion(static_cast<float>(event.motion.x),
                  static_cast<float>(event.motion.y));
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    processButton(true, static_cast<float>(event.button.x),
                  static_cast<float>(event.button.y), event.button.button);
    break;
  case SDL_EVENT_MOUSE_BUTTON_UP:
    processButton(false, static_cast<float>(event.button.x),
                  static_cast<float>(event.button.y), event.button.button);
    break;
  default:
    break;
  }
}

void FlexPointer::processMotion(float x, float y) {
  auto *doc = document();
  if (!doc) {
    return;
  }

  for (auto &entry : m_bindings) {
    bool inside = doc->hitTest(entry.first, x, y);
    setHoverState(entry.second, inside, x, y);
  }
}

void FlexPointer::processButton(bool pressed, float x, float y,
                                          Uint8 button) {
  auto *doc = document();
  if (!doc) {
    return;
  }

  for (auto &entry : m_bindings) {
    bool inside = doc->hitTest(entry.first, x, y);
    auto &state = entry.second;

    if (pressed) {
      if (inside) {
        setActiveState(state, true, x, y, button);
      }
    } else {
      if (state.active) {
        setActiveState(state, false, x, y, button);
        if (inside && state.binding.callback) {
          FlexPointerEvent evt;
          evt.type = FlexPointerEventType::Clicked;
          evt.x = x;
          evt.y = y;
          evt.button = button;
          state.binding.callback(evt);
        }
      }
    }
  }
}

void FlexPointer::setHoverState(BindingState &state, bool hovering,
                                          float x, float y) {
  if (state.hovering == hovering) {
    return;
  }
  state.hovering = hovering;

  if (state.binding.apply_hover_pseudo && document()) {
    document()->setPseudoState(state.binding.element_id, "hover", hovering);
  }

  if (state.binding.callback) {
    FlexPointerEvent evt;
    evt.type = hovering ? FlexPointerEventType::HoverEnter
                        : FlexPointerEventType::HoverLeave;
    evt.x = x;
    evt.y = y;
    state.binding.callback(evt);
  }
}

void FlexPointer::setActiveState(BindingState &state, bool active,
                                           float x, float y, Uint8 button) {
  if (state.active == active) {
    return;
  }
  state.active = active;

  if (state.binding.apply_active_pseudo && document()) {
    document()->setPseudoState(state.binding.element_id, "active", active);
  }

  if (state.binding.callback) {
    FlexPointerEvent evt;
    evt.type =
        active ? FlexPointerEventType::Pressed : FlexPointerEventType::Released;
    evt.x = x;
    evt.y = y;
    evt.button = button;
    state.binding.callback(evt);
  }
}

} // namespace flexui
