#include "flexui/widgets/text_input.h"

#include "flexui/document.h"
#include "flexui/node.h"

#include <SDL3/SDL.h>

namespace flexui {

namespace {
SDL_Window *CurrentWindow() {
  SDL_Window *window = SDL_GetKeyboardFocus();
  if (!window) {
    window = SDL_GetMouseFocus();
  }
  return window;
}

void StartTextInputSession(bool &flag) {
  if (flag) {
    return;
  }
  if (SDL_Window *window = CurrentWindow()) {
    SDL_StartTextInput(window);
    flag = true;
  }
}

void StopTextInputSession(bool &flag) {
  if (!flag) {
    return;
  }
  if (SDL_Window *window = CurrentWindow()) {
    SDL_StopTextInput(window);
  }
  flag = false;
}
} // namespace

void FlexTextInput::registerInput(FlexTextInputBinding binding) {
  if (binding.element_id.empty()) {
    return;
  }

  InputState state;
  state.binding = std::move(binding);

  if (document()) {
    if (auto *node = document()->findNode(state.binding.element_id)) {
      state.value = node->text();
    }
  }

  m_inputs[state.binding.element_id] = std::move(state);

  if (document()) {
    StartTextInputSession(m_text_input_active);
  }
}

void FlexTextInput::handleEvent(const SDL_Event &event) {
  if (!document()) {
    return;
  }

  switch (event.type) {
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    focusInputAt(static_cast<float>(event.button.x), static_cast<float>(event.button.y));
    break;
  case SDL_EVENT_TEXT_INPUT:
    insertText(event.text.text);
    break;
  case SDL_EVENT_KEY_DOWN:
    if (!event.key.down) {
      break;
    }
    if (event.key.key == SDLK_BACKSPACE) {
      handleBackspace();
    } else if (event.key.key == SDLK_ESCAPE) {
      setFocusedId({});
    }
    break;
  default:
    break;
  }
}

void FlexTextInput::onDocumentAttached(FlexDocument *doc) {
  Flex::onDocumentAttached(doc);
  if (doc && !m_inputs.empty()) {
    StartTextInputSession(m_text_input_active);
  } else if (!doc) {
    StopTextInputSession(m_text_input_active);
  }
}

void FlexTextInput::focusInputAt(float x, float y) {
  std::string hit_id;
  for (auto &entry : m_inputs) {
    if (document()->hitTest(entry.first, x, y)) {
      hit_id = entry.first;
      break;
    }
  }
  setFocusedId(hit_id);
}

void FlexTextInput::setFocusedId(const std::string &id) {
  if (m_focused_id == id || !document()) {
    return;
  }

  if (!m_focused_id.empty()) {
    document()->setClass(m_focused_id, "text-input-focused", false);
  }

  m_focused_id = id;

  if (!m_focused_id.empty()) {
    document()->setClass(m_focused_id, "text-input-focused", true);
  }
}

FlexTextInput::InputState *FlexTextInput::focusedState() {
  if (m_focused_id.empty()) {
    return nullptr;
  }
  auto it = m_inputs.find(m_focused_id);
  if (it == m_inputs.end()) {
    return nullptr;
  }
  return &it->second;
}

void FlexTextInput::commitValue(InputState &state) {
  if (!document()) {
    return;
  }
  document()->setText(state.binding.element_id, state.value);
  if (state.binding.on_change) {
    state.binding.on_change(state.value);
  }
}

void FlexTextInput::insertText(const char *text) {
  if (!text) {
    return;
  }
  auto *state = focusedState();
  if (!state) {
    return;
  }
  state->value += text;
  commitValue(*state);
}

void FlexTextInput::handleBackspace() {
  auto *state = focusedState();
  if (!state || state->value.empty()) {
    return;
  }
  state->value.pop_back();
  commitValue(*state);
}

} // namespace flexui
