#include "flexui/widgets/modal.h"

#include "flexui/document.h"

namespace flexui {

void FlexModal::registerModal(FlexModalBinding binding) {
  if (binding.modal_id.empty()) {
    return;
  }

  ModalState state;
  state.binding = std::move(binding);
  state.open = state.binding.initial_open;
  m_modals[state.binding.modal_id] = std::move(state);

  if (document()) {
    updateModalState(m_modals[state.binding.modal_id]);
  }
}

void FlexModal::openModal(const std::string &modal_id) {
  auto it = m_modals.find(modal_id);
  if (it == m_modals.end() || it->second.open) {
    return;
  }

  it->second.open = true;
  updateModalState(it->second);

  if (it->second.binding.on_open) {
    it->second.binding.on_open();
  }
}

void FlexModal::closeModal(const std::string &modal_id) {
  auto it = m_modals.find(modal_id);
  if (it == m_modals.end() || !it->second.open) {
    return;
  }

  it->second.open = false;
  updateModalState(it->second);

  if (it->second.binding.on_close) {
    it->second.binding.on_close();
  }
}

void FlexModal::toggleModal(const std::string &modal_id) {
  auto it = m_modals.find(modal_id);
  if (it == m_modals.end()) {
    return;
  }

  if (it->second.open) {
    closeModal(modal_id);
  } else {
    openModal(modal_id);
  }
}

bool FlexModal::isOpen(const std::string &modal_id) const {
  auto it = m_modals.find(modal_id);
  if (it == m_modals.end()) {
    return false;
  }
  return it->second.open;
}

void FlexModal::handleEvent(const SDL_Event &event) {
  if (!document()) {
    return;
  }

  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    const float x = static_cast<float>(event.button.x);
    const float y = static_cast<float>(event.button.y);

    for (auto &entry : m_modals) {
      auto &state = entry.second;
      if (!state.open) {
        continue;
      }

      // Check close button click
      if (!state.binding.close_button_id.empty() &&
          document()->hitTest(state.binding.close_button_id, x, y)) {
        closeModal(entry.first);
        return;
      }

      // Check overlay click
      if (state.binding.close_on_overlay_click &&
          !state.binding.overlay_id.empty() &&
          document()->hitTest(state.binding.overlay_id, x, y)) {
        // Make sure we're not clicking on the modal itself
        if (!document()->hitTest(state.binding.modal_id, x, y)) {
          closeModal(entry.first);
          return;
        }
      }
    }
  }
}

void FlexModal::updateModalState(ModalState &state) {
  if (!document()) {
    return;
  }

  // Update modal visibility
  document()->setClass(state.binding.modal_id, "modal-open", state.open);
  document()->setClass(state.binding.modal_id, "modal-closed", !state.open);

  // Update overlay visibility
  if (!state.binding.overlay_id.empty()) {
    document()->setClass(state.binding.overlay_id, "overlay-visible",
                        state.open);
    document()->setClass(state.binding.overlay_id, "overlay-hidden",
                        !state.open);
  }
}

} // namespace flexui
