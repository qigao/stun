#include "flexui/widgets/accordion.h"

#include "flexui/document.h"

#include <algorithm>

namespace flexui {

void FlexAccordion::registerAccordion(FlexAccordionBinding binding) {
  if (binding.container_id.empty() || binding.items.empty()) {
    return;
  }

  AccordionState state;
  state.binding = std::move(binding);
  state.expanded.resize(state.binding.items.size());

  // Initialize expanded states
  for (size_t i = 0; i < state.binding.items.size(); ++i) {
    state.expanded[i] = state.binding.items[i].initial_expanded;
  }

  m_accordions[state.binding.container_id] = std::move(state);

  if (getDocument()) {
    auto &accordion = m_accordions[state.binding.container_id];

    for (size_t i = 0; i < accordion.expanded.size(); ++i) {
      updateItemState(accordion, static_cast<int>(i));
    }
  }
}

void FlexAccordion::expandItem(const std::string &container_id, int index) {
  auto it = m_accordions.find(container_id);
  if (it == m_accordions.end() ||
      index < 0 || index >= static_cast<int>(it->second.expanded.size())) {
    return;
  }

  auto &state = it->second;

  // If not allowing multiple, collapse all others
  if (!state.binding.allow_multiple) {
    for (size_t i = 0; i < state.expanded.size(); ++i) {
      if (static_cast<int>(i) != index && state.expanded[i]) {
        state.expanded[i] = false;
        updateItemState(state, static_cast<int>(i));
        if (state.binding.on_toggle) {
          state.binding.on_toggle(static_cast<int>(i), false);
        }
      }
    }
  }

  if (!state.expanded[index]) {
    state.expanded[index] = true;
    updateItemState(state, index);
    if (state.binding.on_toggle) {
      state.binding.on_toggle(index, true);
    }
  }
}

void FlexAccordion::collapseItem(const std::string &container_id, int index) {
  auto it = m_accordions.find(container_id);
  if (it == m_accordions.end() ||
      index < 0 || index >= static_cast<int>(it->second.expanded.size())) {
    return;
  }

  auto &state = it->second;
  if (state.expanded[index]) {
    state.expanded[index] = false;
    updateItemState(state, index);
    if (state.binding.on_toggle) {
      state.binding.on_toggle(index, false);
    }
  }
}

void FlexAccordion::toggleItem(const std::string &container_id, int index) {
  auto it = m_accordions.find(container_id);
  if (it == m_accordions.end() ||
      index < 0 || index >= static_cast<int>(it->second.expanded.size())) {
    return;
  }

  if (it->second.expanded[index]) {
    collapseItem(container_id, index);
  } else {
    expandItem(container_id, index);
  }
}

bool FlexAccordion::isExpanded(const std::string &container_id,
                               int index) const {
  auto it = m_accordions.find(container_id);
  if (it == m_accordions.end() ||
      index < 0 || index >= static_cast<int>(it->second.expanded.size())) {
    return false;
  }
  return it->second.expanded[index];
}

void FlexAccordion::handleEvent(const SDL_Event &event) {
  if (!getDocument()) {
    return;
  }

  if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
    const float x = static_cast<float>(event.button.x);
    const float y = static_cast<float>(event.button.y);

    int item_index = -1;
    auto *state = hitTestHeader(x, y, item_index);
    if (state && item_index >= 0) {
      toggleItem(state->binding.container_id, item_index);
    }
  }
}

FlexAccordion::AccordionState *FlexAccordion::hitTestHeader(float x, float y,
                                                             int &item_index) {
  for (auto &entry : m_accordions) {
    auto &state = entry.second;
    for (size_t i = 0; i < state.binding.items.size(); ++i) {
      const auto &item = state.binding.items[i];
      if (getDocument()->hitTest(item.header_id, x, y)) {
        item_index = static_cast<int>(i);
        return &state;
      }
    }
  }
  item_index = -1;
  return nullptr;
}

void FlexAccordion::updateItemState(AccordionState &state, int index) {
  if (!getDocument() || index < 0 ||
      index >= static_cast<int>(state.binding.items.size())) {
    return;
  }

  const auto &item = state.binding.items[index];
  const bool expanded = state.expanded[index];

  // Update header state
  getDocument()->setClass(item.header_id, "accordion-header-expanded", expanded);
  getDocument()->setClass(item.header_id, "accordion-header-collapsed",
                      !expanded);

  // Update content visibility
  getDocument()->setClass(item.content_id, "accordion-content-expanded",
                      expanded);
  getDocument()->setClass(item.content_id, "accordion-content-collapsed",
                      !expanded);

  // Update all item positions to reflect the new expanded/collapsed state
  updateAllItemPositions(state);
}

void FlexAccordion::updateAllItemPositions(AccordionState &state) {
  if (!getDocument()) {
    return;
  }

  float current_y = 0.0f;

  for (size_t i = 0; i < state.binding.items.size(); ++i) {
    const auto &item = state.binding.items[i];
    const bool expanded = state.expanded[i];

    // Position header
    getDocument()->setAttribute(item.header_id, "style", "top: " +
                        std::to_string(static_cast<int>(current_y)) + "px;");
    current_y += item.header_height;

    // Position content
    getDocument()->setAttribute(item.content_id, "style", "top: " +
                        std::to_string(static_cast<int>(current_y)) + "px;");

    // Only add content height if expanded
    if (expanded) {
      current_y += item.content_height;
    }
  }
}

} // namespace flexui
