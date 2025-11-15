#pragma once

#include "flexui/controller.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace flexui {

struct FlexTextInputBinding {
  std::string element_id;
  std::function<void(const std::string &)> on_change;
};

class FlexTextInput : public Flex {
public:
  void registerInput(FlexTextInputBinding binding);

  void handleEvent(const SDL_Event &event) override;
  void onDocumentAttached(FlexDocument *document) override;

private:
  struct InputState {
    FlexTextInputBinding binding;
    std::string value;
  };

  void focusInputAt(float x, float y);
  void setFocusedId(const std::string &id);
  InputState *focusedState();
  void commitValue(InputState &state);
  void insertText(const char *text);
  void handleBackspace();

  std::unordered_map<std::string, InputState> m_inputs;
  std::string m_focused_id;
  bool m_text_input_active = false;
};

} // namespace flexui
