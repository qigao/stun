#pragma once

#include "flexui/controller.h"
#include "flexui/node.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace flexui {

/**
 * @brief Text label widget with typography support
 *
 * Features:
 * - Font family, size, weight, style
 * - Text alignment and transform
 * - Color and decoration
 * - Multiline support
 * - Selectable text (future)
 */

struct TextLabelBinding {
  std::string element_id;
  std::string text;
  std::function<void(const std::string&)> on_click;
};

class FlexTextLabel : public FlexNode {
public:
  FlexTextLabel() = default;

  void registerLabel(TextLabelBinding binding);

  void setText(const std::string& element_id, const std::string& text);

  void handleEvent(const SDL_Event& event) override;

private:
  struct LabelState {
    TextLabelBinding binding;
  };

  std::unordered_map<std::string, LabelState> m_labels;
};

} // namespace flexui
