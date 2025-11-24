#include "flexui/widgets/tooltip.h"

#include "flexui/document.h"

#include <cstdio>

namespace flexui {

namespace {
std::string px(float value) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.2fpx", value);
  return buffer;
}
} // namespace

void FlexTooltip::registerTooltip(FlexTooltipBinding binding) {
  if (binding.target_id.empty() || binding.tooltip_id.empty()) {
    return;
  }

  TooltipState state;
  state.binding = std::move(binding);
  m_tooltips[state.binding.target_id] = std::move(state);

  if (getDocument()) {
    // Initially hide tooltip
    hideTooltip(m_tooltips[state.binding.target_id]);
  }
}

void FlexTooltip::handleEvent(const SDL_Event &event) {
  if (!getDocument()) {
    return;
  }

  switch (event.type) {
  case SDL_EVENT_MOUSE_MOTION: {
    const float x = static_cast<float>(event.motion.x);
    const float y = static_cast<float>(event.motion.y);

    for (auto &entry : m_tooltips) {
      auto &state = entry.second;
      const bool hit = getDocument()->hitTest(state.binding.target_id, x, y);

      if (hit && !state.hovering) {
        // Started hovering
        state.hovering = true;
        state.hover_time = 0.0f;
        state.mouse_x = x;
        state.mouse_y = y;
      } else if (!hit && state.hovering) {
        // Stopped hovering
        state.hovering = false;
        state.hover_time = 0.0f;
        if (state.visible) {
          hideTooltip(state);
        }
      } else if (hit && state.hovering) {
        // Update mouse position while hovering
        state.mouse_x = x;
        state.mouse_y = y;
        if (state.visible) {
          updateTooltipPosition(state);
        }
      }
    }
    break;
  }
  default:
    break;
  }
}

void FlexTooltip::update(float dt) {
  if (!getDocument()) {
    return;
  }

  for (auto &entry : m_tooltips) {
    auto &state = entry.second;
    
    if (state.hovering && !state.visible) {
      state.hover_time += dt;
      if (state.hover_time >= state.binding.delay) {
        showTooltip(state);
      }
    }
  }
}

void FlexTooltip::showTooltip(TooltipState &state) {
  if (!getDocument() || state.visible) {
    return;
  }

  state.visible = true;
  
  // Set tooltip text
  getDocument()->setText(state.binding.tooltip_id, state.binding.text);
  
  // Position tooltip near mouse
  updateTooltipPosition(state);
  
  // Show tooltip
  getDocument()->setClass(state.binding.tooltip_id, "tooltip-visible", true);
  getDocument()->setClass(state.binding.tooltip_id, "tooltip-hidden", false);
}

void FlexTooltip::hideTooltip(TooltipState &state) {
  if (!getDocument()) {
    return;
  }

  state.visible = false;
  getDocument()->setClass(state.binding.tooltip_id, "tooltip-visible", false);
  getDocument()->setClass(state.binding.tooltip_id, "tooltip-hidden", true);
}

void FlexTooltip::updateTooltipPosition(TooltipState &state) {
  if (!getDocument()) {
    return;
  }

  // Position tooltip slightly offset from mouse cursor
  const float offset_x = 10.0f;
  const float offset_y = 20.0f;

  char style[128];
  std::snprintf(style, sizeof(style), "left: %fpx; top: %fpx;", state.mouse_x + offset_x, state.mouse_y + offset_y);

  getDocument()->setAttribute(state.binding.tooltip_id, "style", style);
}

} // namespace flexui
