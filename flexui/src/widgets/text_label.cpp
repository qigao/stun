#include "flexui/widgets/text_label.h"

#include "flexui/document.h"

namespace flexui {

void FlexTextLabel::registerLabel(TextLabelBinding binding) {
  if (binding.element_id.empty()) {
    return;
  }

  std::string element_id = binding.element_id;
  m_labels[element_id].binding = std::move(binding);

  if (document()) {
    document()->setText(element_id, m_labels[element_id].binding.text);
  }
}

void FlexTextLabel::setText(const std::string& element_id, const std::string& text) {
  auto it = m_labels.find(element_id);
  if (it == m_labels.end() || !document()) {
    return;
  }

  it->second.binding.text = text;
  document()->setText(element_id, text);
}

void FlexTextLabel::handleEvent(const SDL_Event& event) {
  if (!document()) {
    return;
  }

  // Future: handle click events for selectable labels
  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    const float x = static_cast<float>(event.button.x);
    const float y = static_cast<float>(event.button.y);

    for (auto& entry : m_labels) {
      if (document()->hitTest(entry.first, x, y)) {
        if (entry.second.binding.on_click) {
          entry.second.binding.on_click(entry.second.binding.text);
        }
        return;
      }
    }
  }
}

} // namespace flexui
