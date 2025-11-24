#include "flexui/widgets/scrollbar.h"

#include <nanovg_css.h>
#include "flexui/document.h"

#include <algorithm>
#include <cstdio>

namespace flexui {

namespace {
std::string px(float value) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.2fpx", value);
  return buffer;
}
} // namespace

void FlexScrollbar::registerScrollbar(FlexScrollbarBinding binding) {
  if (binding.track_id.empty()) {
    return;
  }

  ScrollbarState state;
  state.binding = std::move(binding);
  state.binding.value = std::clamp(state.binding.value, 0.0f, 1.0f);
  m_scrollbars[state.binding.track_id] = std::move(state);

  if (document()) {
    updateStyles(m_scrollbars[state.binding.track_id]);
    updateContentPosition(m_scrollbars[state.binding.track_id]);
  }
}

void FlexScrollbar::handleEvent(const SDL_Event &event) {
  if (!document()) {
    return;
  }

  switch (event.type) {
  case SDL_EVENT_MOUSE_BUTTON_DOWN: {
    // Try hitting thumb first
    auto *state = hitTestThumb(static_cast<float>(event.button.x), static_cast<float>(event.button.y));
    if (state) {
      state->dragging = true;
      m_active_id = state->binding.track_id;
    } else {
      // Try hitting track (jump to position)
      state = hitTestTrack(static_cast<float>(event.button.x), static_cast<float>(event.button.y));
      if (state) {
        // Get track position
        auto *track_node = document()->findNode(state->binding.track_id);
        if (track_node && track_node->element()) {
          float track_top = track_node->element()->computed.y;
          float track_height = track_node->element()->computed.height;
          float scrollable_height = track_height - state->binding.thumb_size;
          if (scrollable_height > 0) {
            float new_value = (static_cast<float>(event.button.y) - track_top - state->binding.thumb_size / 2) / scrollable_height;
            setValue(*state, new_value);
          }
        }
      }
    }
    break;
  }
  case SDL_EVENT_MOUSE_BUTTON_UP:
    if (!m_active_id.empty()) {
      auto it = m_scrollbars.find(m_active_id);
      if (it != m_scrollbars.end()) {
        it->second.dragging = false;
      }
      m_active_id.clear();
    }
    break;
  case SDL_EVENT_MOUSE_MOTION:
    if (!m_active_id.empty()) {
      auto it = m_scrollbars.find(m_active_id);
      if (it != m_scrollbars.end() && it->second.dragging) {
        // Get track position
        auto *track_node = document()->findNode(it->second.binding.track_id);
        if (track_node && track_node->element()) {
          float track_top = track_node->element()->computed.y;
          float track_height = track_node->element()->computed.height;
          float scrollable_height = track_height - it->second.binding.thumb_size;
          if (scrollable_height > 0) {
            float new_value = (static_cast<float>(event.motion.y) - track_top - it->second.binding.thumb_size / 2) / scrollable_height;
            setValue(it->second, new_value);
          }
        }
      }
    }
    break;
  case SDL_EVENT_MOUSE_WHEEL: {
    // Scroll the first scrollbar (or the one under mouse)
    if (!m_scrollbars.empty()) {
      auto &state = m_scrollbars.begin()->second;
      float delta = event.wheel.y * 0.05f;  // Scroll speed
      setValue(state, state.binding.value - delta);
    }
    break;
  }
  default:
    break;
  }
}

void FlexScrollbar::setValue(ScrollbarState &state, float value) {
  value = std::clamp(value, 0.0f, 1.0f);
  if (state.binding.value == value) {
    return;
  }
  state.binding.value = value;
  updateStyles(state);
  updateContentPosition(state);
  if (state.binding.on_change) {
    state.binding.on_change(state.binding.value);
  }
}

void FlexScrollbar::updateStyles(ScrollbarState &state) {
  if (!document()) {
    return;
  }

  // Update thumb position based on scroll value
  auto *track_node = document()->findNode(state.binding.track_id);
  if (track_node && track_node->element()) {
    float track_height = track_node->element()->computed.height;
    float scrollable_height = track_height - state.binding.thumb_size;
    float thumb_top = state.binding.value * scrollable_height;
    document()->setAttribute(state.binding.thumb_id, "style", "top: " + px(thumb_top) + ";");
  }
}

void FlexScrollbar::updateContentPosition(ScrollbarState &state) {
  if (!document() || state.binding.content_id.empty()) {
    return;
  }

  // Calculate scroll offset
  float max_scroll = state.binding.content_height - state.binding.viewport_height;
  if (max_scroll <= 0) {
    return;  // No scrolling needed
  }

  float scroll_offset = -state.binding.value * max_scroll;

  // Use transform for smooth scrolling
  char transform[64];
  std::snprintf(transform, sizeof(transform), "translateY(%.2fpx)", scroll_offset);
  document()->setAttribute(state.binding.content_id, "style", "transform: " + std::string(transform) + ";");
}

FlexScrollbar::ScrollbarState *FlexScrollbar::hitTestThumb(float x, float y) {
  for (auto &entry : m_scrollbars) {
    auto &state = entry.second;
    auto *thumb_node = document()->findNode(state.binding.thumb_id);
    if (thumb_node && thumb_node->element()) {
      const auto &box = thumb_node->element()->computed;
      if (x >= box.x && x <= box.x + box.width && y >= box.y && y <= box.y + box.height) {
        return &state;
      }
    }
  }
  return nullptr;
}

FlexScrollbar::ScrollbarState *FlexScrollbar::hitTestTrack(float x, float y) {
  for (auto &entry : m_scrollbars) {
    auto &state = entry.second;
    auto *track_node = document()->findNode(state.binding.track_id);
    if (track_node && track_node->element()) {
      const auto &box = track_node->element()->computed;
      if (x >= box.x && x <= box.x + box.width && y >= box.y && y <= box.y + box.height) {
        return &state;
      }
    }
  }
  return nullptr;
}

} // namespace flexui
