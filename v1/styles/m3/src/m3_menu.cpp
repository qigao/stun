/*
    src/m3_menu.cpp -- Material Design 3 Menu implementation

    NanoGUI was developed by Wenzel Jakob <wenzel.jakob@epfl.ch>.

    All rights reserved. Use of this source code is governed by a
    BSD-style license that can be found in the LICENSE.txt file.
*/

#include <nanogui/m3_menu.h>
#include <nanogui/opengl.h>
#include <nanogui/screen.h>


NAMESPACE_BEGIN(nanogui)

M3Menu::M3Menu(Widget *parent, Widget *anchor) : Popup(parent) {
  if (anchor)
    set_anchor_pos(anchor->absolute_position());
}

M3Theme *M3Menu::m3_theme() const {
  return dynamic_cast<M3Theme *>(const_cast<Theme *>(m_theme.get()));
}

void M3Menu::add_item(const std::string &label, const std::function<void()> &callback, int icon,
                      bool enabled) {
  MenuItem item;
  item.label = label;
  item.callback = callback;
  item.icon = icon;
  item.enabled = enabled;
  item.divider = false;
  m_items.push_back(item);
}

void M3Menu::add_item_with_trailing(const std::string &label, const std::string &trailing,
                                    const std::function<void()> &callback, int icon, bool enabled) {
  MenuItem item;
  item.label = label;
  item.trailing_text = trailing;
  item.callback = callback;
  item.icon = icon;
  item.enabled = enabled;
  item.divider = false;
  m_items.push_back(item);
}

void M3Menu::add_submenu(const std::string &label, M3Menu *submenu, int icon) {
  MenuItem item;
  item.label = label;
  item.icon = icon;
  item.enabled = true;
  item.divider = false;
  item.submenu = submenu;
  m_items.push_back(item);

  // Store submenu for cleanup
  if (submenu) {
    m_owned_submenus.push_back(submenu);
  }
}

void M3Menu::add_divider() {
  MenuItem item;
  item.divider = true;
  m_items.push_back(item);
}

void M3Menu::show_at(Widget *anchor) {
  Vector2i menu_size = m_size;
  if (screen()) {
    menu_size = preferred_size(screen()->nvg_context());
    set_size(menu_size);
  }

  if (!anchor) {
    show_at_position(Vector2i(0, 0));
    return;
  }

  Vector2i anchor_pos = anchor->absolute_position();
  Vector2i anchor_size = anchor->size();
  Vector2i position;

  switch (m_anchor_position) {
  case AnchorPosition::TOP_START:
    // Align top-left of menu with top-left of anchor
    position = anchor_pos;
    break;

  case AnchorPosition::TOP_END:
    // Align top-right of menu with top-right of anchor
    position = Vector2i(anchor_pos.x() + anchor_size.x() - menu_size.x(), anchor_pos.y());
    break;

  case AnchorPosition::BOTTOM_START:
    // Align top-left of menu with bottom-left of anchor (default)
    position = Vector2i(anchor_pos.x(), anchor_pos.y() + anchor_size.y());
    break;

  case AnchorPosition::BOTTOM_END:
    // Align top-right of menu with bottom-right of anchor
    position = Vector2i(anchor_pos.x() + anchor_size.x() - menu_size.x(),
                        anchor_pos.y() + anchor_size.y());
    break;
  }

  show_at_position(position);
}

void M3Menu::show_at_position(const Vector2i &pos) {
  if (screen()) {
    set_size(preferred_size(screen()->nvg_context()));
  }

  set_position(pos);
  set_anchor_pos(pos);
  reposition_if_needed();

  Popup::set_visible(true);

  // Start show animation
  animate_show();

  if (screen())
    screen()->redraw();

  // Move focus to first enabled item when menu opens
  for (size_t i = 0; i < m_items.size(); ++i) {
    if (!m_items[i].divider && m_items[i].enabled) {
      focus_item(static_cast<int>(i));
      break;
    }
  }
}
void M3Menu::animate_show() {
  M3Theme *theme = m3_theme();

  // Check if animations are disabled (accessibility preference)
  if (theme && !theme->animations_enabled()) {
    // Skip animation - show immediately
    m_opacity = 1.0f;
    m_animating = false;
    return;
  }

  // Start fade-in and scale animation (150ms duration as per requirements 14.3)
  m_animating = true;
  m_fading_in = true;
  m_animation_time = 0.0f;
  m_animation_duration = 0.15f; // 150ms
  m_opacity = 0.0f;
}

void M3Menu::animate_hide() {
  M3Theme *theme = m3_theme();

  // Check if animations are disabled (accessibility preference)
  if (theme && !theme->animations_enabled()) {
    // Skip animation - hide immediately
    m_opacity = 0.0f;
    m_animating = false;
    Popup::set_visible(false);
    return;
  }

  // Start fade-out animation (75ms duration as per requirements 14.4)
  m_animating = true;
  m_fading_in = false;
  m_animation_time = 0.0f;
  m_animation_duration = 0.075f; // 75ms
}

void M3Menu::update_animation(float dt) {
  if (!m_animating) {
    return;
  }

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
  } else {
    // Fade out from 1.0 to 0.0
    m_opacity = 1.0f - progress;

    if (progress >= 1.0f) {
      m_opacity = 0.0f;
      m_animating = false;
      Popup::set_visible(false);
    }
  }
}

void M3Menu::set_visible(bool visible) {
  // Close any open submenu when hiding this menu
  if (!visible && m_open_submenu) {
    close_submenu();
  }

  if (!visible && m_visible) {
    // Start hide animation instead of hiding immediately
    animate_hide();
  } else if (visible && !m_visible) {
    Popup::set_visible(visible);
  }
}

void M3Menu::reposition_if_needed() {
  if (!screen())
    return;

  Vector2i screen_size = screen()->size();
  Vector2i menu_pos = m_pos;
  Vector2i menu_size = m_size;

  bool repositioned = false;

  // Check if menu extends beyond screen bottom
  if (menu_pos.y() + menu_size.y() > screen_size.y()) {
    // Move menu up to fit on screen
    menu_pos.y() = screen_size.y() - menu_size.y();
    repositioned = true;
  }

  // Check if menu extends beyond screen right
  if (menu_pos.x() + menu_size.x() > screen_size.x()) {
    // Move menu left to fit on screen
    menu_pos.x() = screen_size.x() - menu_size.x();
    repositioned = true;
  }

  // Check if menu extends beyond screen left
  if (menu_pos.x() < 0) {
    menu_pos.x() = 0;
    repositioned = true;
  }

  // Check if menu extends beyond screen top
  if (menu_pos.y() < 0) {
    menu_pos.y() = 0;
    repositioned = true;
  }

  if (repositioned) {
    set_position(menu_pos);
  }
}

void M3Menu::perform_layout(NVGcontext *ctx) {
  Popup::perform_layout(ctx);
  reposition_if_needed();
}

int M3Menu::item_at_position(const Vector2i &p) const {
  Vector2i local = p - m_pos;

  float y_offset = 8.0f; // Top padding
  int item_height = 48;
  int divider_height = 9;

  for (size_t i = 0; i < m_items.size(); ++i) {
    float item_h =
        m_items[i].divider ? static_cast<float>(divider_height) : static_cast<float>(item_height);

    if (local.y() >= y_offset && local.y() < y_offset + item_h) {
      if (!m_items[i].divider && m_items[i].enabled) {
        return static_cast<int>(i);
      }
    }

    y_offset += item_h;
  }

  return -1;
}

bool M3Menu::mouse_button_event(const Vector2i &p, int button, bool down, int modifiers) {
  Popup::mouse_button_event(p, button, down, modifiers);

  if (button != NANOGUI_MOUSE_BUTTON_LEFT)
    return false;

  if (down) {
    int item = item_at_position(p);
    if (item >= 0) {
      activate_item(item);
      return true;
    }
  }

  return false;
}

bool M3Menu::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
  Popup::mouse_motion_event(p, rel, button, modifiers);

  int prev_hover = m_hover_item;
  m_hover_item = item_at_position(p);

  // Handle submenu hover timing
  if (m_hover_item >= 0 && m_hover_item < static_cast<int>(m_items.size())) {
    const MenuItem &item = m_items[m_hover_item];

    // If hovering over a submenu item
    if (item.submenu && item.enabled) {
      // If this is a new hover, start timing
      if (m_hover_item != prev_hover) {
        m_submenu_hover_time = static_cast<float>(Screen::get_time());
      } else {
        // Check if hover delay has elapsed (200ms)
        float current_time = static_cast<float>(Screen::get_time());
        if (current_time - m_submenu_hover_time >= 0.2f) {
          // Open submenu if not already open
          if (m_open_submenu != item.submenu) {
            open_submenu(m_hover_item);
          }
        }
      }
    } else {
      // Not hovering over a submenu item
      // If we have an open submenu, start leave timer
      if (m_open_submenu && m_submenu_leave_time == 0.0f) {
        m_submenu_leave_time = static_cast<float>(Screen::get_time());
      }
    }
  } else {
    // Not hovering over any item
    // If we have an open submenu, start leave timer
    if (m_open_submenu && m_submenu_leave_time == 0.0f) {
      m_submenu_leave_time = static_cast<float>(Screen::get_time());
    }
  }

  // Check leave timer for closing submenu
  if (m_open_submenu && m_submenu_leave_time > 0.0f) {
    float current_time = static_cast<float>(Screen::get_time());
    if (current_time - m_submenu_leave_time >= 0.3f) {
      close_submenu();
    }
  }

  return false;
}

Vector2i M3Menu::preferred_size(NVGcontext *ctx) const {
  nvgFontSize(ctx, 14);
  nvgFontFace(ctx, "sans");

  float max_width = 112.0f; // Minimum width

  for (const auto &item : m_items) {
    if (!item.divider) {
      float tw = nvgTextBounds(ctx, 0, 0, item.label.c_str(), nullptr, nullptr);
      float item_width = tw + 24 + (item.icon ? 40 : 0); // Padding + icon space

      // Add trailing text width if present
      if (!item.trailing_text.empty()) {
        float trailing_w = nvgTextBounds(ctx, 0, 0, item.trailing_text.c_str(), nullptr, nullptr);
        item_width += trailing_w + 24; // Add spacing between label and trailing text
      }

      max_width = std::max(max_width, item_width);
    }
  }

  int total_height = 16; // Top + bottom padding
  for (const auto &item : m_items) {
    total_height += item.divider ? 9 : 48;
  }

  return Vector2i(static_cast<int>(max_width), total_height);
}

void M3Menu::focus_item(int index) {
  if (index < 0 || index >= static_cast<int>(m_items.size())) {
    m_focused_item = -1;
    return;
  }

  // Skip dividers and disabled items
  if (m_items[index].divider || !m_items[index].enabled) {
    return;
  }

  m_focused_item = index;
}

void M3Menu::activate_item(int index) {
  // Validate index
  if (index < 0 || index >= static_cast<int>(m_items.size())) {
    return;
  }

  const MenuItem &item = m_items[index];

  // Handle disabled items - no-op
  if (!item.enabled || item.divider) {
    return;
  }

  // If item has a submenu, open it instead of activating
  if (item.submenu) {
    open_submenu(index);
    return;
  }

  // Invoke callback if present
  if (item.callback) {
    item.callback();
  }

  // Close menu after activation
  set_visible(false);
}

void M3Menu::open_submenu(int index) {
  // Validate index
  if (index < 0 || index >= static_cast<int>(m_items.size())) {
    return;
  }

  const MenuItem &item = m_items[index];

  // Check if item has a submenu
  if (!item.submenu || !item.enabled) {
    return;
  }

  // Close any currently open submenu
  if (m_open_submenu && m_open_submenu != item.submenu) {
    close_submenu();
  }

  // Store reference to open submenu
  m_open_submenu = item.submenu;
  m_submenu_item_index = index;

  // Calculate submenu position
  // Position submenu to the right of parent menu
  Vector2i submenu_pos;

  // Calculate the Y position of the menu item
  float y_offset = 8.0f; // Top padding
  int item_height = 48;
  int divider_height = 9;

  for (int i = 0; i < index; ++i) {
    y_offset += m_items[i].divider ? divider_height : item_height;
  }

  // Position submenu to the right of parent menu, aligned with the item
  submenu_pos = Vector2i(m_pos.x() + m_size.x(), m_pos.y() + static_cast<int>(y_offset));

  // Get submenu size
  Vector2i submenu_size = m_open_submenu->preferred_size(screen()->nvg_context());

  // Check if submenu extends beyond screen bounds
  if (!screen()) {
    m_open_submenu->show_at_position(submenu_pos);
    return;
  }

  Vector2i screen_size = screen()->size();

  // Check if submenu extends beyond screen right
  if (submenu_pos.x() + submenu_size.x() > screen_size.x()) {
    // Position to the left of parent menu instead
    submenu_pos.x() = m_pos.x() - submenu_size.x();
  }

  // Check if submenu extends beyond screen bottom
  if (submenu_pos.y() + submenu_size.y() > screen_size.y()) {
    // Adjust Y position to fit on screen
    submenu_pos.y() = screen_size.y() - submenu_size.y();
  }

  // Check if submenu extends beyond screen top
  if (submenu_pos.y() < 0) {
    submenu_pos.y() = 0;
  }

  // Show submenu at calculated position
  m_open_submenu->show_at_position(submenu_pos);

  // Reset leave timer
  m_submenu_leave_time = 0.0f;
}

void M3Menu::close_submenu() {
  if (m_open_submenu) {
    m_open_submenu->set_visible(false);
    m_open_submenu = nullptr;
    m_submenu_item_index = -1;
  }

  // Reset timing variables
  m_submenu_hover_time = 0.0f;
  m_submenu_leave_time = 0.0f;
}

bool M3Menu::keyboard_event(int key, int scancode, int action, int modifiers) {
  if (Popup::keyboard_event(key, scancode, action, modifiers))
    return true;

  if (action != GLFW_PRESS && action != GLFW_REPEAT)
    return false;

  // Handle Escape key - close menu
  if (key == GLFW_KEY_ESCAPE) {
    set_visible(false);
    return true;
  }

  // Handle Enter key - activate focused item
  if (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER) {
    if (m_focused_item >= 0) {
      activate_item(m_focused_item);
      return true;
    }
    return false;
  }

  // Handle Arrow Down - move to next enabled item
  if (key == GLFW_KEY_DOWN) {
    int next_item = m_focused_item + 1;

    // Find next enabled, non-divider item
    while (next_item < static_cast<int>(m_items.size())) {
      if (!m_items[next_item].divider && m_items[next_item].enabled) {
        focus_item(next_item);
        return true;
      }
      next_item++;
    }

    // Wrap around to first item
    next_item = 0;
    while (next_item <= m_focused_item && next_item < static_cast<int>(m_items.size())) {
      if (!m_items[next_item].divider && m_items[next_item].enabled) {
        focus_item(next_item);
        return true;
      }
      next_item++;
    }

    return true;
  }

  // Handle Arrow Up - move to previous enabled item
  if (key == GLFW_KEY_UP) {
    int prev_item = m_focused_item - 1;

    // Find previous enabled, non-divider item
    while (prev_item >= 0) {
      if (!m_items[prev_item].divider && m_items[prev_item].enabled) {
        focus_item(prev_item);
        return true;
      }
      prev_item--;
    }

    // Wrap around to last item
    prev_item = static_cast<int>(m_items.size()) - 1;
    while (prev_item >= m_focused_item && prev_item >= 0) {
      if (!m_items[prev_item].divider && m_items[prev_item].enabled) {
        focus_item(prev_item);
        return true;
      }
      prev_item--;
    }

    return true;
  }

  // Handle Arrow Right - open submenu if focused item has one
  if (key == GLFW_KEY_RIGHT) {
    if (m_focused_item >= 0 && m_focused_item < static_cast<int>(m_items.size())) {
      const MenuItem &item = m_items[m_focused_item];
      if (item.submenu && item.enabled) {
        open_submenu(m_focused_item);
        return true;
      }
    }
    return false;
  }

  // Handle Arrow Left - close this menu (useful for nested submenus)
  if (key == GLFW_KEY_LEFT) {
    set_visible(false);
    return true;
  }

  return false;
}

void M3Menu::draw(NVGcontext *ctx) {
  M3Theme *theme = m3_theme();
  if (!theme) {
    Popup::draw(ctx);
    return;
  }

  // Update animation state
  if (m_animating) {
    // Calculate delta time (approximate - using 60fps as baseline)
    float dt = 1.0f / 60.0f;
    update_animation(dt);
  }

  // Don't draw if fully transparent
  if (m_opacity <= 0.0f && !m_animating) {
    return;
  }

  float x = m_pos.x();
  float y = m_pos.y();
  float w = m_size.x();
  float h = m_size.y();
  float corner_radius = theme->corner_radius(M3Theme::ShapeFamily::Small);

  nvgSave(ctx);

  // Apply scale-from-top and fade animation
  // Scale from 0.8 to 1.0, anchored at top
  float scale = 0.8f + (m_opacity * 0.2f);

  // Apply fade (opacity)
  nvgGlobalAlpha(ctx, m_opacity);

  // Calculate top-center point for scaling
  float center_x = x + w * 0.5f;
  float top_y = y;

  // Apply scale transform around top-center
  nvgTranslate(ctx, center_x, top_y);
  nvgScale(ctx, scale, scale);
  nvgTranslate(ctx, -center_x, -top_y);

  // Draw shadow
  NVGpaint shadow = nvgBoxGradient(ctx, x, y + 2, w, h, corner_radius, 6.0f,
                                   nvgRGBAf(0, 0, 0, 0.2f), nvgRGBAf(0, 0, 0, 0));
  nvgBeginPath(ctx);
  nvgRect(ctx, x - 6, y - 6, w + 12, h + 14);
  nvgRoundedRect(ctx, x, y, w, h, corner_radius);
  nvgPathWinding(ctx, NVG_HOLE);
  nvgFillPaint(ctx, shadow);
  nvgFill(ctx);

  // Draw background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, corner_radius);
  nvgFillColor(ctx, theme->surface());
  nvgFill(ctx);

  // Draw elevation tint
  Color tint = theme->elevation_tint(M3Theme::Elevation::Level2);
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, corner_radius);
  nvgFillColor(ctx, tint);
  nvgFill(ctx);

  // Draw items
  float y_offset = y + 8;
  int item_height = 48;
  int divider_height = 9;

  for (size_t i = 0; i < m_items.size(); ++i) {
    const auto &item = m_items[i];

    if (item.divider) {
      // Draw divider
      nvgBeginPath(ctx);
      nvgRect(ctx, x + 12, y_offset + 4, w - 24, 1);
      nvgFillColor(ctx, theme->outline_variant());
      nvgFill(ctx);
      y_offset += divider_height;
    } else {
      bool hovered = (static_cast<int>(i) == m_hover_item);
      bool focused = (static_cast<int>(i) == m_focused_item);

      // Draw hover or focus state
      if ((hovered || focused) && item.enabled) {
        Color state_color = theme->state_layer(theme->on_surface(), 0.08f);
        nvgBeginPath(ctx);
        nvgRect(ctx, x, y_offset, w, item_height);
        nvgFillColor(ctx, state_color);
        nvgFill(ctx);
      }

      // Draw icon
      float text_x = x + 12;
      if (item.icon) {
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, item.enabled ? theme->on_surface_variant()
                                       : Color(theme->on_surface().r(), theme->on_surface().g(),
                                               theme->on_surface().b(), 0.38f));
        nvgText(ctx, text_x, y_offset + item_height * 0.5f, utf8(item.icon).data(), nullptr);
        text_x += 40;
      }

      // Draw text
      nvgFontSize(ctx, 14);
      nvgFontFace(ctx, "sans");
      nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      nvgFillColor(ctx, item.enabled ? theme->on_surface()
                                     : Color(theme->on_surface().r(), theme->on_surface().g(),
                                             theme->on_surface().b(), 0.38f));
      nvgText(ctx, text_x, y_offset + item_height * 0.5f, item.label.c_str(), nullptr);

      // Draw trailing text (e.g., keyboard shortcut)
      if (!item.trailing_text.empty()) {
        nvgFontSize(ctx, 14); // label-large typography
        nvgFontFace(ctx, "sans");
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, item.enabled ? theme->on_surface_variant()
                                       : Color(theme->on_surface().r(), theme->on_surface().g(),
                                               theme->on_surface().b(), 0.38f));
        nvgText(ctx, x + w - 12, y_offset + item_height * 0.5f, item.trailing_text.c_str(),
                nullptr);
      }

      // Draw arrow icon for submenu items
      if (item.submenu) {
        nvgFontSize(ctx, 24);
        nvgFontFace(ctx, "icons");
        nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgFillColor(ctx, item.enabled ? theme->on_surface_variant()
                                       : Color(theme->on_surface().r(), theme->on_surface().g(),
                                               theme->on_surface().b(), 0.38f));
        // Unicode for right arrow (chevron_right icon)
        nvgText(ctx, x + w - 12, y_offset + item_height * 0.5f, utf8(0xE5CC).data(), nullptr);
      }

      y_offset += item_height;
    }
  }

  nvgRestore(ctx);
}

const char *M3Menu::item_accessibility_role(int index) const {
  if (index < 0 || index >= (int)m_items.size()) {
    return "menuitem";
  }

  const MenuItem &item = m_items[index];

  // Dividers don't have a role
  if (item.divider) {
    return "separator";
  }

  // Items with submenus have a special role
  if (item.submenu) {
    return "menuitem"; // Still menuitem, but with aria-haspopup
  }

  return "menuitem";
}

bool M3Menu::item_has_submenu(int index) const {
  if (index < 0 || index >= (int)m_items.size()) {
    return false;
  }

  return m_items[index].submenu != nullptr;
}

bool M3Menu::submenu_is_expanded(int index) const {
  if (index < 0 || index >= (int)m_items.size()) {
    return false;
  }

  // Check if this item's submenu is currently open
  return m_open_submenu != nullptr && m_submenu_item_index == index;
}

NAMESPACE_END(nanogui)
