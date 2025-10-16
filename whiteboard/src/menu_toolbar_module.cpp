#include "whiteboard/menu_toolbar_module.h"

MenuToolbarModule::MenuToolbarModule(Widget *parent) : Widget(parent) {
  m_hovered_item = -1;
  m_pressed_item = -1;
  m_dragging = false;

  // Add default menu items
  add_menu_item("New", FA_FILE, []() {});
  add_menu_item("Open", FA_FOLDER_OPEN, []() {});
  add_menu_item("Save", FA_SAVE, []() {});
  add_separator();
  add_menu_item("Undo", FA_UNDO, []() {});
  add_menu_item("Redo", FA_REDO, []() {});
  add_separator();
  add_menu_item("Export", FA_DOWNLOAD, []() {});
  add_menu_item("Share", FA_SHARE_ALT, []() {});
  add_separator();
  add_menu_item("Settings", FA_COG, []() {});
  add_menu_item("Help", FA_QUESTION_CIRCLE, []() {});

  layout_items();
}

void MenuToolbarModule::add_menu_item(const std::string &label, int icon,
                                      std::function<void()> callback) {
  MenuItem item;
  item.label = label;
  item.icon = icon;
  item.callback = std::move(callback);
  item.hovered = false;
  item.pressed = false;
  item.id = static_cast<int>(m_items.size());
  item.x = 0.f;
  item.y = 0.f;
  item.width = 0.f;
  item.height = 0.f;
  m_items.push_back(item);
}

void MenuToolbarModule::add_separator() {
  m_separator_positions.push_back(static_cast<float>(m_items.size()));
}

void MenuToolbarModule::clear_items() {
  m_items.clear();
  m_separator_positions.clear();
  m_hovered_item = -1;
  m_pressed_item = -1;
}

void MenuToolbarModule::layout_items() {
  const float padding = 10.f;
  const float item_height = 36.f;
  const float icon_size = 18.f;
  const float icon_text_spacing = 8.f;
  const float item_spacing = 4.f;
  const float separator_width = 12.f;

  float current_x = padding;
  size_t separator_index = 0;

  for (size_t i = 0; i < m_items.size(); i++) {
    // Check if we need a separator before this item
    if (separator_index < m_separator_positions.size() &&
        i == static_cast<size_t>(m_separator_positions[separator_index])) {
      current_x += separator_width;
      separator_index++;
    }

    // Estimate text width (will be more accurate in draw, but this is good enough for layout)
    float text_width = m_items[i].label.length() * 7.5f; // Approximate 7.5px per character
    float item_width = padding + icon_size + icon_text_spacing + text_width + padding;

    m_items[i].x = current_x;
    m_items[i].y = padding;
    m_items[i].width = item_width;
    m_items[i].height = item_height;

    current_x += item_width + item_spacing;
  }
}

Vector2i MenuToolbarModule::preferred_size_impl(NVGcontext *) const {
  const float padding = 10.f;
  const float item_height = 36.f;
  const float item_spacing = 4.f;

  // Calculate total width
  float total_width = padding;
  for (const auto &item : m_items) {
    total_width += item.width + item_spacing;
  }
  total_width += m_separator_positions.size() * 12.f;
  total_width += padding;

  return {static_cast<int>(total_width), static_cast<int>(item_height + padding * 2)};
}

int MenuToolbarModule::find_item_at(const Vector2i &pos) {
  Vector2f local_pos = Vector2f(pos.x() - m_pos.x(), pos.y() - m_pos.y());

  for (size_t i = 0; i < m_items.size(); i++) {
    const auto &item = m_items[i];
    if (local_pos.x() >= item.x && local_pos.x() <= item.x + item.width &&
        local_pos.y() >= item.y && local_pos.y() <= item.y + item.height) {
      return static_cast<int>(i);
    }
  }
  return -1;
}

bool MenuToolbarModule::mouse_button_event(const Vector2i &p, int button, bool down,
                                           int modifiers) {
  if (button == GLFW_MOUSE_BUTTON_1) {
    if (down) {
      int item_index = find_item_at(p);
      if (item_index >= 0) {
        m_pressed_item = item_index;
        m_items[item_index].pressed = true;
        screen()->redraw();
        return true;
      } else {
        // Start dragging the toolbar
        m_dragging = true;
        m_drag_start = p;
        return true;
      }
    } else {
      // Mouse up
      if (m_pressed_item >= 0) {
        int item_index = find_item_at(p);
        if (item_index == m_pressed_item && m_items[item_index].callback) {
          m_items[item_index].callback();
        }
        m_items[m_pressed_item].pressed = false;
        m_pressed_item = -1;
        screen()->redraw();
        return true;
      }
      m_dragging = false;
    }
  }
  return Widget::mouse_button_event(p, button, down, modifiers);
}

bool MenuToolbarModule::mouse_motion_event(const Vector2i &p, const Vector2i &rel, int button,
                                           int modifiers) {
  int item_index = find_item_at(p);

  // Update hover states
  bool changed = false;
  for (size_t i = 0; i < m_items.size(); i++) {
    bool should_hover = (static_cast<int>(i) == item_index);
    if (m_items[i].hovered != should_hover) {
      m_items[i].hovered = should_hover;
      changed = true;
    }
  }

  if (changed) {
    m_hovered_item = item_index;
    screen()->redraw();
  }

  return Widget::mouse_motion_event(p, rel, button, modifiers);
}

bool MenuToolbarModule::mouse_drag_event(const Vector2i &p, const Vector2i &rel, int button,
                                         int modifiers) {
  if (m_dragging) {
    Vector2i new_pos = m_pos + rel;
    set_position(new_pos);
    return true;
  }
  return Widget::mouse_drag_event(p, rel, button, modifiers);
}

void MenuToolbarModule::draw(NVGcontext *ctx) {
  Widget::draw(ctx);

  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float pw = static_cast<float>(m_size.x());
  const float ph = static_cast<float>(m_size.y());

  // Draw rounded panel background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 12.f);
  nvgFillColor(ctx, nvgRGBA(250, 250, 252, 255));
  nvgFill(ctx);

  // Draw subtle border
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 12.f);
  nvgStrokeColor(ctx, nvgRGBA(220, 220, 230, 255));
  nvgStrokeWidth(ctx, 1.f);
  nvgStroke(ctx);

  // Draw menu items
  for (const auto &item : m_items) {
    drawMenuItem(ctx, px + item.x, py + item.y, item.width, item.height, item.label.c_str(),
                 item.icon, item.hovered, item.pressed);
  }

  // Draw separators
  size_t separator_index = 0;
  for (size_t i = 0; i < m_items.size(); i++) {
    if (separator_index < m_separator_positions.size() &&
        i == static_cast<size_t>(m_separator_positions[separator_index])) {
      float sep_x = px + m_items[i].x - 8.f;
      drawSeparator(ctx, sep_x, py + 12.f, ph - 24.f);
      separator_index++;
    }
  }
}

void MenuToolbarModule::drawMenuItem(NVGcontext *ctx, float x, float y, float w, float h,
                                     const char *label, int icon, bool hovered, bool pressed) {
  // Draw button background
  if (pressed) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, 6.f);
    nvgFillColor(ctx, nvgRGBA(200, 200, 220, 255));
    nvgFill(ctx);
  } else if (hovered) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, x, y, w, h, 6.f);
    nvgFillColor(ctx, nvgRGBA(235, 235, 245, 255));
    nvgFill(ctx);
  }

  const float padding = 10.f;
  const float icon_size = 18.f;
  const float icon_text_spacing = 8.f;

  float center_y = y + h * 0.5f;
  float current_x = x + padding;

  // Draw icon
  if (icon > 0) {
    nvgFontFace(ctx, "icons");
    nvgFontSize(ctx, icon_size);
    nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
    nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    std::string icon_str = utf8(icon);
    nvgText(ctx, current_x, center_y, icon_str.c_str(), nullptr);

    // Measure actual icon width
    float icon_bounds[4];
    nvgTextBounds(ctx, current_x, center_y, icon_str.c_str(), nullptr, icon_bounds);
    float icon_width = icon_bounds[2] - icon_bounds[0];

    current_x += icon_width + icon_text_spacing;
  }

  // Draw label
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 14.f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(ctx, current_x, center_y, label, nullptr);
}

void MenuToolbarModule::drawSeparator(NVGcontext *ctx, float x, float y, float h) {
  nvgBeginPath(ctx);
  nvgMoveTo(ctx, x, y);
  nvgLineTo(ctx, x, y + h);
  nvgStrokeColor(ctx, nvgRGBA(200, 200, 210, 255));
  nvgStrokeWidth(ctx, 1.f);
  nvgStroke(ctx);
}
