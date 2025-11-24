/**
 * \file context_menu_module.cpp
 * \brief Implementation of ContextMenuModule class.
 */

#include "whiteboard/ui/context_menu_module.h"
#include <nanogui/theme.h>
#include <nanogui/screen.h>
#include <nanovg.h>
#include <algorithm>
#include <cmath>

namespace whiteboard {

ContextMenuModule::ContextMenuModule(nanogui::Widget *parent)
    : nanogui::Widget(parent), m_hovered_item(-1), m_visible(false) {
  set_visible(false);
}

void ContextMenuModule::show_at(const nanogui::Vector2i &pos, const std::vector<MenuItem> &items) {
  m_items = items;
  m_hovered_item = -1;
  m_visible = true;
  
  // Calculate menu size
  NVGcontext *ctx = screen()->nvg_context();
  float width, height;
  calculate_size(ctx, width, height);
  
  // Position menu at cursor, but keep it on screen
  nanogui::Vector2i menu_pos = pos;
  nanogui::Vector2i screen_size = screen()->size();
  
  // Adjust if menu would go off right edge
  if (menu_pos.x() + width > screen_size.x()) {
    menu_pos.x() = screen_size.x() - static_cast<int>(width) - 10;
  }
  
  // Adjust if menu would go off bottom edge
  if (menu_pos.y() + height > screen_size.y()) {
    menu_pos.y() = screen_size.y() - static_cast<int>(height) - 10;
  }
  
  // Ensure menu stays on screen
  menu_pos.x() = std::max(10, menu_pos.x());
  menu_pos.y() = std::max(10, menu_pos.y());
  
  set_position(menu_pos);
  set_size(nanogui::Vector2i(static_cast<int>(width), static_cast<int>(height)));
  set_visible(true);
  request_focus();
}

void ContextMenuModule::hide() {
  m_visible = false;
  set_visible(false);
  m_items.clear();
  m_hovered_item = -1;
}

void ContextMenuModule::draw(NVGcontext *ctx) {
  if (!m_visible || m_items.empty()) {
    return;
  }

  nanogui::Widget::draw(ctx);

  float x = m_pos.x();
  float y = m_pos.y();
  float w = m_size.x();
  float h = m_size.y();

  nvgSave(ctx);

  // Draw shadow
  NVGpaint shadow_paint = nvgBoxGradient(ctx, x, y + 2, w, h, CORNER_RADIUS, SHADOW_SIZE,
                                          nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0));
  nvgBeginPath(ctx);
  nvgRect(ctx, x - SHADOW_SIZE, y - SHADOW_SIZE, w + 2 * SHADOW_SIZE, h + 2 * SHADOW_SIZE);
  nvgRoundedRect(ctx, x, y, w, h, CORNER_RADIUS);
  nvgPathWinding(ctx, NVG_HOLE);
  nvgFillPaint(ctx, shadow_paint);
  nvgFill(ctx);

  // Draw background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, CORNER_RADIUS);
  nvgFillColor(ctx, nvgRGBA(250, 250, 250, 255));
  nvgFill(ctx);

  // Draw border
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, x, y, w, h, CORNER_RADIUS);
  nvgStrokeColor(ctx, nvgRGBA(200, 200, 200, 255));
  nvgStrokeWidth(ctx, 1.0f);
  nvgStroke(ctx);

  // Draw items
  float item_y = y + PADDING;
  
  for (size_t i = 0; i < m_items.size(); ++i) {
    const MenuItem &item = m_items[i];
    
    if (item.separator) {
      // Draw separator
      float sep_y = item_y + SEPARATOR_HEIGHT / 2.0f;
      nvgBeginPath(ctx);
      nvgMoveTo(ctx, x + PADDING, sep_y);
      nvgLineTo(ctx, x + w - PADDING, sep_y);
      nvgStrokeColor(ctx, nvgRGBA(220, 220, 220, 255));
      nvgStrokeWidth(ctx, 1.0f);
      nvgStroke(ctx);
      
      item_y += SEPARATOR_HEIGHT;
      continue;
    }
    
    // Draw hover background
    if (static_cast<int>(i) == m_hovered_item && item.enabled) {
      nvgBeginPath(ctx);
      nvgRect(ctx, x + 2, item_y, w - 4, ITEM_HEIGHT);
      nvgFillColor(ctx, nvgRGBA(230, 240, 255, 255));
      nvgFill(ctx);
    }
    
    // Set text color based on enabled state
    NVGcolor text_color = item.enabled ? nvgRGBA(50, 50, 50, 255) : nvgRGBA(150, 150, 150, 255);
    
    float text_x = x + PADDING;
    
    // Draw icon if present
    if (item.icon != 0) {
      nvgFontFace(ctx, "icons");
      nvgFontSize(ctx, ICON_SIZE);
      nvgFillColor(ctx, text_color);
      nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
      
      // Convert icon code to UTF-8 string
      char icon_str[8];
      int icon_len = 0;
      unsigned int codepoint = static_cast<unsigned int>(item.icon);
      
      if (codepoint < 0x80) {
        icon_str[icon_len++] = static_cast<char>(codepoint);
      } else if (codepoint < 0x800) {
        icon_str[icon_len++] = static_cast<char>(0xC0 | (codepoint >> 6));
        icon_str[icon_len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
      } else if (codepoint < 0x10000) {
        icon_str[icon_len++] = static_cast<char>(0xE0 | (codepoint >> 12));
        icon_str[icon_len++] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        icon_str[icon_len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
      } else {
        icon_str[icon_len++] = static_cast<char>(0xF0 | (codepoint >> 18));
        icon_str[icon_len++] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
        icon_str[icon_len++] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
        icon_str[icon_len++] = static_cast<char>(0x80 | (codepoint & 0x3F));
      }
      icon_str[icon_len] = '\0';
      
      nvgText(ctx, text_x, item_y + ITEM_HEIGHT / 2.0f, icon_str, nullptr);
      text_x += ICON_SIZE + ICON_SPACING;
    }
    
    // Draw label
    nvgFontFace(ctx, "sans");
    nvgFontSize(ctx, 14.0f);
    nvgFillColor(ctx, text_color);
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(ctx, text_x, item_y + ITEM_HEIGHT / 2.0f, item.label.c_str(), nullptr);
    
    // Draw shortcut if present
    if (!item.shortcut.empty()) {
      nvgFontSize(ctx, 12.0f);
      nvgFillColor(ctx, nvgRGBA(120, 120, 120, 255));
      nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
      nvgText(ctx, x + w - PADDING, item_y + ITEM_HEIGHT / 2.0f, item.shortcut.c_str(), nullptr);
    }
    
    item_y += ITEM_HEIGHT;
  }

  nvgRestore(ctx);
}

bool ContextMenuModule::mouse_button_event(const nanogui::Vector2i &p, int button, bool down,
                                             int modifiers) {
  if (!m_visible) {
    return false;
  }

  if (button == 0 && down) { // Left click
    // Convert to local coordinates
    nanogui::Vector2i local_p = p - m_pos;
    
    // Check if clicking inside menu
    if (local_p.x() >= 0 && local_p.x() < m_size.x() &&
        local_p.y() >= 0 && local_p.y() < m_size.y()) {
      
      int item_index = item_at_position(local_p);
      if (item_index >= 0) {
        execute_item(item_index);
        return true;
      }
    } else {
      // Clicked outside menu - close it
      hide();
      return true;
    }
  }

  return false;
}

bool ContextMenuModule::mouse_motion_event(const nanogui::Vector2i &p,
                                             const nanogui::Vector2i &rel, int button,
                                             int modifiers) {
  if (!m_visible) {
    return false;
  }

  // Convert to local coordinates
  nanogui::Vector2i local_p = p - m_pos;
  
  // Update hovered item
  int old_hovered = m_hovered_item;
  m_hovered_item = item_at_position(local_p);
  
  // Request redraw if hover changed
  if (old_hovered != m_hovered_item) {
    // Trigger redraw
  }

  return true;
}

bool ContextMenuModule::keyboard_event(int key, int scancode, int action, int modifiers) {
  if (!m_visible) {
    return false;
  }

  // Close on ESC
  if (key == 256 && action == 1) { // ESC key, press action
    hide();
    return true;
  }

  return false;
}

void ContextMenuModule::calculate_size(NVGcontext *ctx, float &width, float &height) const {
  width = MIN_WIDTH;
  height = PADDING * 2;

  nvgSave(ctx);
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 14.0f);

  for (const auto &item : m_items) {
    if (item.separator) {
      height += SEPARATOR_HEIGHT;
      continue;
    }

    // Calculate text width
    float text_width = 0.0f;
    float bounds[4];
    
    // Icon width
    if (item.icon != 0) {
      text_width += ICON_SIZE + ICON_SPACING;
    }
    
    // Label width
    nvgTextBounds(ctx, 0, 0, item.label.c_str(), nullptr, bounds);
    text_width += bounds[2] - bounds[0];
    
    // Shortcut width
    if (!item.shortcut.empty()) {
      nvgFontSize(ctx, 12.0f);
      nvgTextBounds(ctx, 0, 0, item.shortcut.c_str(), nullptr, bounds);
      text_width += SHORTCUT_SPACING + (bounds[2] - bounds[0]);
      nvgFontSize(ctx, 14.0f);
    }
    
    width = std::max(width, text_width + PADDING * 2);
    height += ITEM_HEIGHT;
  }

  nvgRestore(ctx);
}

int ContextMenuModule::item_at_position(const nanogui::Vector2i &p) const {
  if (p.x() < 0 || p.x() >= m_size.x() || p.y() < 0 || p.y() >= m_size.y()) {
    return -1;
  }

  float item_y = PADDING;
  
  for (size_t i = 0; i < m_items.size(); ++i) {
    const MenuItem &item = m_items[i];
    
    if (item.separator) {
      item_y += SEPARATOR_HEIGHT;
      continue;
    }
    
    if (p.y() >= item_y && p.y() < item_y + ITEM_HEIGHT) {
      return item.enabled ? static_cast<int>(i) : -1;
    }
    
    item_y += ITEM_HEIGHT;
  }

  return -1;
}

void ContextMenuModule::execute_item(int index) {
  if (index < 0 || index >= static_cast<int>(m_items.size())) {
    return;
  }

  const MenuItem &item = m_items[index];
  
  if (!item.enabled || item.separator || !item.callback) {
    return;
  }

  // Execute callback
  item.callback();
  
  // Close menu
  hide();
}

} // namespace whiteboard
