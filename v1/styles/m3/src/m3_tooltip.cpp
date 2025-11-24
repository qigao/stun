/*
    src/m3_tooltip.cpp -- Material Design 3 Tooltip implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <algorithm>
#include <iostream>
#include <nanogui/m3_tooltip.h>
#include <nanogui/m3_tooltip_manager.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>


NAMESPACE_BEGIN(nanogui)

M3Tooltip::M3Tooltip(Widget *parent, const std::string &text) : Widget(parent), m_text(text) {
  // Regular tooltips are not persistent by default (requirement 12.6)
  set_persistent(false);
}

M3Tooltip::~M3Tooltip() {
  // Ensure proper cleanup when tooltip is destroyed
  // Remove this tooltip from the manager if it's the current one
  if (M3TooltipManager::instance().current_tooltip() == this) {
    M3TooltipManager::instance().hide_tooltip(this);
  }
}

M3Theme *M3Tooltip::m3_theme() const {
  return dynamic_cast<M3Theme *>(const_cast<Theme *>(m_theme.get()));
}

Vector2i M3Tooltip::preferred_size(NVGcontext *ctx) const {
  nvgFontSize(ctx, 12);
  nvgFontFace(ctx, "sans");

  float tw = nvgTextBounds(ctx, 0, 0, m_text.c_str(), nullptr, nullptr);

  return Vector2i(static_cast<int>(tw) + 16, 24);
}

void M3Tooltip::draw(NVGcontext *ctx) {
  // Update tooltip manager timer (needs to run every frame)
  M3TooltipManager::instance().update(1.0f / 60.0f); // Approximate 60fps

  M3Theme *theme = m3_theme();
  if (!theme) {
    Widget::draw(ctx);
    return;
  }

  // Update animation state
  if (m_animating || m_pending_show) {
    // Calculate delta time (approximate - using 60fps as baseline)
    float dt = 1.0f / 60.0f;
    update_animation(dt);
  }

  // Don't draw if fully transparent, but still update manager
  if (m_opacity <= 0.0f) {
    return;
  }

  float x = m_pos.x();
  float y = m_pos.y();
  float w = m_size.x();
  float h = m_size.y();
  float corner_radius = theme->corner_radius(M3Theme::ShapeFamily::Small);

  nvgSave(ctx);

  // Apply global opacity
  nvgGlobalAlpha(ctx, m_opacity);

  // Draw shadow
  NVGpaint shadow = nvgBoxGradient(ctx, x, y + 2, w, h, corner_radius, 4.0f,
                                   nvgRGBAf(0, 0, 0, 0.2f), nvgRGBAf(0, 0, 0, 0));
  nvgBeginPath(ctx);
  nvgRect(ctx, x - 4, y - 4, w + 8, h + 10);
  nvgRoundedRect(ctx, x, y, w, h, corner_radius);
  nvgPathWinding(ctx, NVG_HOLE);
  nvgFillPaint(ctx, shadow);
  nvgFill(ctx);

  // Draw background (inverse surface)
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, corner_radius);
  nvgFillColor(ctx, theme->inverse_surface());
  nvgFill(ctx);

  // Draw text
  nvgFontSize(ctx, 12);
  nvgFontFace(ctx, "sans");
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgFillColor(ctx, theme->inverse_on_surface());
  nvgText(ctx, x + w * 0.5f, y + h * 0.5f, m_text.c_str(), nullptr);

  nvgRestore(ctx);
}

void M3Tooltip::position_relative_to_target() {
  if (!m_target || !screen())
    return;

  // Get target position and size
  Vector2i target_pos = m_target->absolute_position();
  Vector2i target_size = m_target->size();

  // Get tooltip size and set it
  Vector2i tooltip_size = preferred_size(screen()->nvg_context());
  set_size(tooltip_size);

  // Get screen size for bounds checking
  Vector2i screen_size = screen()->size();

  // 4dp spacing (assuming 1dp = 1px for simplicity)
  const int spacing = 4;

  Vector2i position;
  Position effective_position = m_position;

  // If AUTO, default to BOTTOM
  if (effective_position == Position::AUTO) {
    effective_position = Position::BOTTOM;
  }

  // Calculate initial position based on requested position
  switch (effective_position) {
  case Position::BOTTOM:
    // Position below target, centered horizontally
    position.x() = target_pos.x() + (target_size.x() - tooltip_size.x()) / 2;
    position.y() = target_pos.y() + target_size.y() + spacing;
    break;

  case Position::TOP:
    // Position above target, centered horizontally
    position.x() = target_pos.x() + (target_size.x() - tooltip_size.x()) / 2;
    position.y() = target_pos.y() - tooltip_size.y() - spacing;
    break;

  case Position::LEFT:
    // Position to the left of target, centered vertically
    position.x() = target_pos.x() - tooltip_size.x() - spacing;
    position.y() = target_pos.y() + (target_size.y() - tooltip_size.y()) / 2;
    break;

  case Position::RIGHT:
    // Position to the right of target, centered vertically
    position.x() = target_pos.x() + target_size.x() + spacing;
    position.y() = target_pos.y() + (target_size.y() - tooltip_size.y()) / 2;
    break;

  case Position::AUTO:
    // Already handled above
    break;
  }

  // Check screen bounds and reposition if needed

  // Check if tooltip extends beyond screen bottom
  if (position.y() + tooltip_size.y() > screen_size.y()) {
    if (effective_position == Position::BOTTOM) {
      // Flip to top
      position.y() = target_pos.y() - tooltip_size.y() - spacing;
    } else {
      // Move up to fit
      position.y() = screen_size.y() - tooltip_size.y();
    }
  }

  // Check if tooltip extends beyond screen top
  if (position.y() < 0) {
    if (effective_position == Position::TOP) {
      // Flip to bottom
      position.y() = target_pos.y() + target_size.y() + spacing;
    } else {
      // Move down to fit
      position.y() = 0;
    }
  }

  // Check if tooltip extends beyond screen right
  if (position.x() + tooltip_size.x() > screen_size.x()) {
    // Align to right edge
    position.x() = screen_size.x() - tooltip_size.x();
  }

  // Check if tooltip extends beyond screen left
  if (position.x() < 0) {
    // Align to left edge
    position.x() = 0;
  }

  // Set the calculated position using Widget's set_position
  Widget::set_position(position);
}

void M3Tooltip::animate_show() {
  M3Theme *theme = m3_theme();

  // Check if animations are disabled (accessibility preference)
  if (theme && !theme->animations_enabled()) {
    // Skip animation - show immediately
    m_opacity = 1.0f;
    m_animating = false;
    set_visible(true);
    position_relative_to_target();
    return;
  }

  // Start fade-in animation (150ms duration as per requirements 14.5)
  m_animating = true;
  m_fading_in = true;
  m_fading_out = false;
  m_animation_time = 0.0f;
  m_animation_duration = 0.15f; // 150ms
  m_opacity = 0.0f;
  set_visible(true);

  // Position tooltip before showing
  position_relative_to_target();
}

void M3Tooltip::animate_hide() {
  M3Theme *theme = m3_theme();

  // Check if animations are disabled (accessibility preference)
  if (theme && !theme->animations_enabled()) {
    // Skip animation - hide immediately
    m_opacity = 0.0f;
    m_animating = false;
    set_visible(false);
    return;
  }

  // Start fade-out animation (75ms duration as per requirements 14.6)
  m_animating = true;
  m_fading_out = true;
  m_fading_in = false;
  m_animation_time = 0.0f;
  m_animation_duration = 0.075f; // 75ms
}

void M3Tooltip::update_animation(float dt) {
  // Handle pending show timer
  if (m_pending_show) {
    m_show_timer += dt;
    if (m_show_timer >= m_show_delay / 1000.0f) {
      m_pending_show = false;
      m_show_timer = 0.0f;
      animate_show();
    }
  }

  // Handle fade animations
  if (m_animating) {
    m_animation_time += dt;
    float progress = std::min(1.0f, m_animation_time / m_animation_duration);

    if (m_fading_in) {
      // Fade in from 0.0 to 1.0
      m_opacity = progress;

      if (progress >= 1.0f) {
        m_opacity = 1.0f;
        m_animating = false;
        m_fading_in = false;
      }
    } else if (m_fading_out) {
      // Fade out from 1.0 to 0.0
      m_opacity = 1.0f - progress;

      if (progress >= 1.0f) {
        m_opacity = 0.0f;
        m_animating = false;
        m_fading_out = false;
        set_visible(false);
      }
    }
  }
}

void M3Tooltip::show_delayed(int delay_ms) {
  // Use the manager to handle delayed show
  // This ensures only one tooltip is visible at a time (requirement 10.7)
  M3TooltipManager::instance().show_tooltip(this, delay_ms);

  // Keep local state for backward compatibility
  m_show_delay = delay_ms;
  m_show_timer = 0.0f;
  m_pending_show = true;
  m_opacity = 0.0f;

  if (screen()) {
    set_visible(true);
    position_relative_to_target();
    screen()->redraw();
  }
}

void M3Tooltip::show_immediate() {
  // Show tooltip immediately without delay
  m_pending_show = false;
  m_show_timer = 0.0f;
  animate_show();

  // Bring to front (ensure tooltip appears above other UI elements)
  auto p = parent();
  if (p) {
    p->remove_child(this);
    p->add_child(this);
  }

  if (screen())
    screen()->redraw();

  // Announce to screen readers for accessibility
  announce_to_screen_reader();
}

void M3Tooltip::hide_immediate(bool notify_manager) {
  if (notify_manager)
    M3TooltipManager::instance().hide_tooltip(this);

  // Hide tooltip immediately without animation
  m_pending_show = false;
  m_show_timer = 0.0f;
  m_animating = false;
  m_fading_in = false;
  m_fading_out = false;
  m_opacity = 0.0f;
  set_visible(false);

  if (screen())
    screen()->redraw();
}

// ============================================================================
// M3RichTooltip Implementation
// ============================================================================

M3RichTooltip::M3RichTooltip(Widget *parent) : M3Tooltip(parent, "") {
  // Rich tooltips are persistent by default (requirement 12.6)
  // They remain visible until explicitly dismissed or an action is clicked
  set_persistent(true);
}

void M3RichTooltip::add_action(const std::string &label, const std::function<void()> &callback) {
  m_actions.push_back({label, callback});
}

void M3RichTooltip::clear_actions() { m_actions.clear(); }

Vector2i M3RichTooltip::preferred_size(NVGcontext *ctx) const {
  M3Theme *theme = m3_theme();
  if (!theme) {
    return M3Tooltip::preferred_size(ctx);
  }

  // Rich tooltips have more complex layout
  // Padding: 16dp horizontal, 12dp vertical
  const int h_padding = 16;
  const int v_padding = 12;
  const int icon_size = 24;
  const int icon_margin = 12;
  const int spacing = 8;        // spacing between elements
  const int action_height = 32; // height for action buttons

  int width = 0;
  int height = v_padding;

  // Calculate content width (max 280dp as per M3 spec for rich tooltips)
  int content_width = 280 - 2 * h_padding;

  // Account for icon if present
  int text_width = content_width;
  if (m_icon != 0) {
    text_width -= (icon_size + icon_margin);
  }

  // Calculate title height
  if (!m_title.empty()) {
    height += 14; // title-small line height
    height += spacing;
  }

  // Calculate supporting text height (estimate 2-3 lines)
  if (!m_supporting_text.empty()) {
    height += 36; // Approximate height for 2-3 lines of body-small
    height += spacing;
  }

  // Add action buttons height if present
  if (!m_actions.empty()) {
    height += action_height;
    height += spacing;
  }

  height += v_padding;

  // Width is fixed at 280dp for rich tooltips
  width = 280;

  return Vector2i(width, height);
}

void M3RichTooltip::draw(NVGcontext *ctx) {
  M3Theme *theme = m3_theme();
  if (!theme) {
    M3Tooltip::draw(ctx);
    return;
  }

  // Update animation state
  if (m_animating || m_pending_show) {
    float dt = 1.0f / 60.0f;
    update_animation(dt);
  }

  // Don't draw if fully transparent
  if (m_opacity <= 0.0f) {
    return;
  }

  float x = m_pos.x();
  float y = m_pos.y();
  float w = m_size.x();
  float h = m_size.y();
  float corner_radius = theme->corner_radius(M3Theme::ShapeFamily::Small);

  nvgSave(ctx);

  // Apply global opacity
  nvgGlobalAlpha(ctx, m_opacity);

  // Draw shadow (elevation level 2 for rich tooltips)
  NVGpaint shadow = nvgBoxGradient(ctx, x, y + 2, w, h, corner_radius, 6.0f,
                                   nvgRGBAf(0, 0, 0, 0.25f), nvgRGBAf(0, 0, 0, 0));
  nvgBeginPath(ctx);
  nvgRect(ctx, x - 6, y - 6, w + 12, h + 14);
  nvgRoundedRect(ctx, x, y, w, h, corner_radius);
  nvgPathWinding(ctx, NVG_HOLE);
  nvgFillPaint(ctx, shadow);
  nvgFill(ctx);

  // Draw background (inverse surface)
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, corner_radius);
  nvgFillColor(ctx, theme->inverse_surface());
  nvgFill(ctx);

  // Layout constants
  const int h_padding = 16;
  const int v_padding = 12;
  const int icon_size = 24;
  const int icon_margin = 12;
  const int spacing = 8;

  float current_y = y + v_padding;
  float content_x = x + h_padding;
  float content_width = w - 2 * h_padding;

  // Draw icon if present
  if (m_icon != 0) {
    nvgFontSize(ctx, icon_size);
    nvgFontFace(ctx, "icons");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(ctx, theme->inverse_on_surface());

    nvgText(ctx, content_x, current_y + icon_size, utf8(m_icon).data(), nullptr);

    content_x += icon_size + icon_margin;
    content_width -= (icon_size + icon_margin);
  }

  // Draw title if present
  if (!m_title.empty()) {
    nvgFontSize(ctx, 14); // title-small
    nvgFontFace(ctx, "sans-bold");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(ctx, theme->inverse_on_surface());

    // Simple single-line title rendering
    nvgText(ctx, content_x, current_y + 14, m_title.c_str(), nullptr);
    current_y += 14 + spacing;
  }

  // Draw supporting text if present
  if (!m_supporting_text.empty()) {
    nvgFontSize(ctx, 12); // body-small
    nvgFontFace(ctx, "sans");
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgFillColor(ctx, theme->inverse_on_surface());

    // Simple multi-line text rendering (estimate 2-3 lines)
    nvgText(ctx, content_x, current_y + 12, m_supporting_text.c_str(), nullptr);
    current_y += 36 + spacing; // Approximate height for 2-3 lines
  }

  // Draw action buttons if present
  if (!m_actions.empty()) {
    const int action_height = 32;
    const int action_padding = 12;
    const int action_spacing = 8;

    // Reset content_x to account for icon offset
    content_x = x + h_padding;
    content_width = w - 2 * h_padding;

    // Position actions at the right side
    float action_x = x + w - h_padding;

    for (int i = static_cast<int>(m_actions.size()) - 1; i >= 0; --i) {
      const Action &action = m_actions[i];

      // Calculate button width
      nvgFontSize(ctx, 14); // label-large
      nvgFontFace(ctx, "sans-bold");
      float bounds[4];
      float text_width = nvgTextBounds(ctx, 0, 0, action.label.c_str(), nullptr, bounds);
      float button_width = text_width + 2 * action_padding;

      action_x -= button_width;

      // Draw button background if hovered
      if (i == m_hovered_action) {
        nvgBeginPath(ctx);
        nvgRoundedRect(ctx, action_x, current_y, button_width, action_height,
                       theme->corner_radius(M3Theme::ShapeFamily::Small));
        // State layer with 8% opacity
        Color state_color = theme->inverse_on_surface();
        nvgFillColor(ctx, nvgRGBAf(state_color.r(), state_color.g(), state_color.b(), 0.08f));
        nvgFill(ctx);
      }

      // Draw button text
      nvgFontSize(ctx, 14); // label-large
      nvgFontFace(ctx, "sans-bold");
      nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
      nvgFillColor(ctx, theme->inverse_primary());
      nvgText(ctx, action_x + button_width / 2, current_y + action_height / 2, action.label.c_str(),
              nullptr);

      action_x -= action_spacing;
    }
  }

  nvgRestore(ctx);
}

bool M3RichTooltip::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
  if (!down || button != GLFW_MOUSE_BUTTON_1) {
    return Widget::mouse_button_event(p, button, down, modifiers);
  }

  // Check if click is on an action button
  if (!m_actions.empty()) {
    M3Theme *theme = m3_theme();
    if (!theme) {
      return Widget::mouse_button_event(p, button, down, modifiers);
    }

    const int h_padding = 16;
    const int v_padding = 12;
    const int icon_size = 24;
    const int icon_margin = 12;
    const int spacing = 8;
    const int action_height = 32;
    const int action_padding = 12;
    const int action_spacing = 8;

    // Calculate action button area
    float current_y = m_pos.y() + v_padding;

    // Skip title
    if (!m_title.empty()) {
      current_y += 14 + spacing; // title-small line height
    }

    // Skip supporting text
    if (!m_supporting_text.empty()) {
      current_y += 36 + spacing; // Approximate height for 2-3 lines
    }

    // Check if click is in action button area
    if (p.y() >= current_y && p.y() <= current_y + action_height) {
      NVGcontext *ctx = screen() ? screen()->nvg_context() : nullptr;
      if (ctx) {
        float action_x = m_pos.x() + m_size.x() - h_padding;

        for (int i = static_cast<int>(m_actions.size()) - 1; i >= 0; --i) {
          const Action &action = m_actions[i];

          // Calculate button width
          nvgFontSize(ctx, 14);
          nvgFontFace(ctx, "sans-bold");
          float bounds[4];
          float text_width = nvgTextBounds(ctx, 0, 0, action.label.c_str(), nullptr, bounds);
          float button_width = text_width + 2 * action_padding;

          action_x -= button_width;

          // Check if click is on this button
          if (p.x() >= action_x && p.x() <= action_x + button_width) {
            // Invoke callback
            if (action.callback) {
              action.callback();
            }

            // Hide tooltip after action click (requirement 12.5)
            hide_immediate();
            return true;
          }

          action_x -= action_spacing;
        }
      }
    }
  }

  return Widget::mouse_button_event(p, button, down, modifiers);
}

bool M3RichTooltip::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button,
                                       int modifiers) {
  // Track which action button is being hovered
  m_hovered_action = -1;

  if (!m_actions.empty()) {
    M3Theme *theme = m3_theme();
    if (!theme) {
      return Widget::mouse_motion_event(p, rel, button, modifiers);
    }

    const int h_padding = 16;
    const int v_padding = 12;
    const int icon_size = 24;
    const int icon_margin = 12;
    const int spacing = 8;
    const int action_height = 32;
    const int action_padding = 12;
    const int action_spacing = 8;

    // Calculate action button area
    float current_y = m_pos.y() + v_padding;

    // Skip title
    if (!m_title.empty()) {
      current_y += 14 + spacing; // title-small line height
    }

    // Skip supporting text
    if (!m_supporting_text.empty()) {
      current_y += 36 + spacing; // Approximate height for 2-3 lines
    }

    // Check if mouse is in action button area
    if (p.y() >= current_y && p.y() <= current_y + action_height) {
      NVGcontext *ctx = screen() ? screen()->nvg_context() : nullptr;
      if (ctx) {
        float action_x = m_pos.x() + m_size.x() - h_padding;

        for (int i = static_cast<int>(m_actions.size()) - 1; i >= 0; --i) {
          const Action &action = m_actions[i];

          // Calculate button width
          nvgFontSize(ctx, 14);
          nvgFontFace(ctx, "sans-bold");
          float bounds[4];
          float text_width = nvgTextBounds(ctx, 0, 0, action.label.c_str(), nullptr, bounds);
          float button_width = text_width + 2 * action_padding;

          action_x -= button_width;

          // Check if mouse is on this button
          if (p.x() >= action_x && p.x() <= action_x + button_width) {
            m_hovered_action = i;
            return true;
          }

          action_x -= action_spacing;
        }
      }
    }
  }

  return Widget::mouse_motion_event(p, rel, button, modifiers);
}

void M3Tooltip::announce_to_screen_reader() const {
  // Platform-specific screen reader announcement would go here
  // On Windows: Use UI Automation or MSAA
  // On macOS: Use NSAccessibility
  // On Linux: Use AT-SPI

  // For now, this is a placeholder that documents the intent
  // A full implementation would require platform-specific code

  // The tooltip text is available via accessibility_description()
  // and the role is available via accessibility_role()

  // Example pseudo-code for Windows:
  // if (UiaRaiseNotificationEvent) {
  //     UiaRaiseNotificationEvent(element, NotificationKind_Other,
  //                               NotificationProcessing_All,
  //                               m_text.c_str(), L"");
  // }
}

NAMESPACE_END(nanogui)
