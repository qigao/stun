#include "flexui/widgets/spinner.h"

#include <nanovg.h>
#include <nanovg_css.h>
#include "flexui/document.h"
#include "flexui/node.h" // Needed for FlexNode definition

#include <cmath>
#include <algorithm>
#include <cstdio>

namespace flexui {

namespace {
std::string deg(float value) {
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%.2fdeg", value);
  return buffer;
  }
}

void FlexSpinner::registerSpinner(FlexSpinnerBinding binding) {
  if (binding.spinner_id.empty()) {
    return;
  }

  SpinnerState state;
  state.binding = std::move(binding);
  
  // Insert into map and get stable reference
  auto& stored_state = m_spinners[state.binding.spinner_id];
  stored_state = std::move(state);

  if (document()) {
    // Attach custom paint callback
    if (auto* node = document()->findNode(stored_state.binding.spinner_id)) {
      if (auto* elem = node->element()) {
        elem->custom_paint = DrawSpinner;
        elem->user_data = &stored_state;
      }
    }
    
    updateSpinner(stored_state);
  }
}


void FlexSpinner::onDocumentAttached(FlexDocument *document) {
  FlexNode::onDocumentAttached(document); // Call base class method

  // Re-attach custom paint callback and update for all registered spinners
  for (auto &entry : m_spinners) {
    auto &state = entry.second;
    if (auto *node = document->findNode(state.binding.spinner_id)) {
      if (auto *elem = node->element()) {
        elem->custom_paint = DrawSpinner;
        elem->user_data = &state;
      }
    }
    updateSpinner(state);
  }
}

void FlexSpinner::setActive(const std::string &spinner_id, bool active) {
  auto it = m_spinners.find(spinner_id);
  if (it == m_spinners.end()) {
    return;
  }

  it->second.binding.active = active;
  
  if (document()) {
    document()->setClass(spinner_id, "spinner-active", active);
    document()->setClass(spinner_id, "spinner-inactive", !active);
  }
}

bool FlexSpinner::isActive(const std::string &spinner_id) const {
  auto it = m_spinners.find(spinner_id);
  if (it == m_spinners.end()) {
    return false;
  }
  return it->second.binding.active;
}

void FlexSpinner::update(float dt) {
  if (!document()) {
    return;
  }

  for (auto &entry : m_spinners) {
    auto &state = entry.second;
    
    if (!state.binding.active) {
      continue;
    }

    // Update rotation
    state.rotation += state.binding.rotation_speed * dt;
    
    // Keep rotation in 0-360 range
    while (state.rotation >= 360.0f) {
      state.rotation -= 360.0f;
    }

    updateSpinner(state);
  }
}

void FlexSpinner::updateSpinner(SpinnerState &state) {
  if (!document()) {
    return;
  }
  
  // We no longer set CSS transform since we draw manually.
  // However, we might need to force a redraw if the renderer is lazy.
  // But FlexView renders every frame, so updating state.rotation is enough
  // as long as DrawSpinner reads it.
  
  // If we wanted to force layout/style recalc, we would do:
  // document()->setStyle(state.binding.spinner_id, "data-tick", std::to_string(state.rotation));
}

void FlexSpinner::DrawSpinner(NVGcontext* vg, const NVGCSSElement* element, const std::map<std::string, std::string>& attributes) {
  if (!element->user_data) return;
  
  // Cast user_data back to SpinnerState
  // Note: This assumes SpinnerState memory is still valid (owned by FlexSpinner)
  SpinnerState* state = static_cast<SpinnerState*>(element->user_data);
  
  if (!state->binding.active) return;

  // Element dimensions (computed layout)
  float x = element->computed.x;
  float y = element->computed.y;
  float width = element->computed.width;
  float height = element->computed.height;
  
  // Center of the element
  // NanoVG CSS Painter typically sets transform to global but uses computed.x/y for positioning
  // inside that space (unless CSS transform is used).
  // We must translate to the element's computed position.
  float cx = x + width / 2.0f;
  float cy = y + height / 2.0f;
  float radius = std::min(width, height) / 2.0f - 4.0f; // Padding
  if (radius < 1.0f) radius = 1.0f;
  
  float thickness = radius * 0.2f; // Adaptive thickness
  if (thickness < 2.0f) thickness = 2.0f;
  if (thickness > 6.0f) thickness = 6.0f;

  nvgSave(vg);
  
  // Translate to center
  nvgTranslate(vg, cx, cy);
  
  // Rotation is handled by animating the arc angles directly, so no nvgRotate needed.
  
  // Style: Standard Dot Spinner (iOS-style)
  const int numDots = 12;
  const float dotBaseRadius = radius * 0.1f;
  const float spinnerRadius = radius - dotBaseRadius; // Adjust so dots stay within bounds

  // Current rotation angle in radians (0 is Up)
  // state->rotation is in degrees
  float currentAngle = nvgDegToRad(state->rotation);

  for (int i = 0; i < numDots; ++i) {
    // Calculate angle for this dot
    // Start from -PI/2 (Up) and go clockwise
    float dotAngle = -NVG_PI * 0.5f + (float)i * (NVG_PI * 2.0f / numDots);
    
    float cx_dot = std::cos(dotAngle) * spinnerRadius;
    float cy_dot = std::sin(dotAngle) * spinnerRadius;

    // Calculate opacity based on angular distance from current rotation
    // We want the dot at currentAngle to be brightest, and trailing dots to fade
    float angleDiff = currentAngle - (float)i * (NVG_PI * 2.0f / numDots);
    
    // Normalize angleDiff to [0, 2PI)
    while (angleDiff < 0) angleDiff += NVG_PI * 2.0f;
    while (angleDiff >= NVG_PI * 2.0f) angleDiff -= NVG_PI * 2.0f;

    // Opacity decays as angleDiff increases (trail effect)
    // angleDiff=0 -> Opacity=1.0
    // angleDiff=2PI -> Opacity=0.0
    float opacity = 1.0f - (angleDiff / (NVG_PI * 2.0f));
    
    // Ensure minimum opacity if desired, or clamp
    if (opacity < 0.0f) opacity = 0.0f;
    if (opacity > 1.0f) opacity = 1.0f;

    // Optional: Fade lower bound
    // opacity = 0.2f + 0.8f * opacity; 

    nvgBeginPath(vg);
    nvgCircle(vg, cx_dot, cy_dot, dotBaseRadius);
    
    // Use a standard gray or theme color
    // Let's use a dark gray/black for visibility or theme color
    // The previous code used Theme Blue (74, 144, 226)
    // iOS uses gray/black usually, but let's stick to theme color or neutral
    // example_flexui_all_widgets.cpp sets --primary-color: #4a90e2 (74, 144, 226)
    // We can use a fixed color for now, or try to read computed styles if available.
    // Since we don't have easy access to computed color properties here without parsing,
    // let's use a neutral dark gray which works on light backgrounds, 
    // or the theme blue from before. Let's stick to Theme Blue for consistency.
    
    // Modulate alpha
    nvgFillColor(vg, nvgRGBA(74, 144, 226, (unsigned char)(opacity * 255)));
    nvgFill(vg);
  }

  nvgRestore(vg);
}

} // namespace flexui
