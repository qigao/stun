#pragma once

#include "flexui/controller.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace flexui {

struct FlexAccordionItem {
  std::string header_id;
  std::string content_id;
  std::string title;
  bool initial_expanded = false;
  float header_height = 50.0f;
  float content_height = 80.0f;
};

struct FlexAccordionBinding {
  std::string container_id;
  std::vector<FlexAccordionItem> items;
  bool allow_multiple = false; // Allow multiple panels open at once
  std::function<void(int, bool)> on_toggle; // index, expanded
};

class FlexAccordion : public FlexController {
public:
  void registerAccordion(FlexAccordionBinding binding);

  void expandItem(const std::string &container_id, int index);
  void collapseItem(const std::string &container_id, int index);
  void toggleItem(const std::string &container_id, int index);
  bool isExpanded(const std::string &container_id, int index) const;

  void handleEvent(const SDL_Event &event) override;

private:
  struct AccordionState {
    FlexAccordionBinding binding;
    std::vector<bool> expanded;
  };

  AccordionState *hitTestHeader(float x, float y, int &item_index);
  void updateItemState(AccordionState &state, int index);
  void updateAllItemPositions(AccordionState &state);

  std::unordered_map<std::string, AccordionState> m_accordions;
};

} // namespace flexui
