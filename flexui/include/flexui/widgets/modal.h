#pragma once

#pragma once

#include "flexui/controller.h"
#include "flexui/node.h" // Add this include

#include <functional>
#include <string>
#include <unordered_map>

namespace flexui {

struct FlexModalBinding {
  std::string modal_id;
  std::string overlay_id;
  std::string close_button_id;
  bool initial_open = false;
  bool close_on_overlay_click = true;
  std::function<void()> on_open;
  std::function<void()> on_close;
};

class FlexModal : public FlexNode { // Change Flex to FlexNode
public:
  void registerModal(FlexModalBinding binding);

  void openModal(const std::string &modal_id);
  void closeModal(const std::string &modal_id);
  void toggleModal(const std::string &modal_id);
  bool isOpen(const std::string &modal_id) const;

  void handleEvent(const SDL_Event &event) override;

private:
  struct ModalState {
    FlexModalBinding binding;
    bool open = false;
  };

  void updateModalState(ModalState &state);

  std::unordered_map<std::string, ModalState> m_modals;
};

} // namespace flexui
