/*
 * Meta Editor - Shortcut Panel Implementation
 */
#include "meta_editor/view/shortcut_panel.h"

namespace meta_editor {

ShortcutPanel::ShortcutPanel() {
  set_size(400, 500);

  shortcuts_ = {
      // Tools
      {"V", "Select tool", "Tools"},
      {"P", "Pen tool", "Tools"},
      {"L", "Line tool", "Tools"},
      {"C", "Connector tool", "Tools"},
      {"D", "Freehand/Draw tool", "Tools"},
      {"N", "Sticky Note tool", "Tools"},
      {"H", "Hand/Pan tool", "Tools"},
      {"X", "Eraser tool", "Tools"},
      {"F", "Frame tool", "Tools"},
      {"I", "Image tool", "Tools"},
      {"Z", "Laser pointer", "Tools"},
      {"R", "Rectangle tool", "Tools"},
      {"O", "Circle tool", "Tools"},
      {"E", "Ellipse tool", "Tools"},
      {"T", "Text tool", "Tools"},
      {"S", "Star tool", "Tools"},

      // Tool modifiers
      {"A", "Cycle arrow style (Line tool)", "Modifiers"},
      {"1-5", "Change note color (Sticky Note)", "Modifiers"},
      {"Space", "Temporary hand tool", "Modifiers"},

      // View
      {"+/-", "Zoom in/out", "View"},
      {"Ctrl+0", "Reset view", "View"},
      {"G", "Toggle grid snap", "View"},
      {"?", "Show shortcuts", "View"},

      // Edit
      {"Ctrl+Z", "Undo", "Edit"},
      {"Ctrl+Y", "Redo", "Edit"},
      {"Ctrl+C", "Copy", "Edit"},
      {"Ctrl+V", "Paste", "Edit"},
      {"Ctrl+D", "Duplicate", "Edit"},
      {"Ctrl+A", "Select all", "Edit"},
      {"Ctrl+G", "Group", "Edit"},
      {"Ctrl+Shift+G", "Ungroup", "Edit"},
      {"Delete", "Delete selection", "Edit"},
      {"Escape", "Clear selection", "Edit"},
  };
}

void ShortcutPanel::render(flex::Renderer &renderer) {
  if (!visible_)
    return;

  // Semi-transparent overlay background
  flex::Paint overlay = flex::Paint::solid(flex::Color{0.0f, 0.0f, 0.0f, 0.85f});
  flex::Paint border = flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.3f, 1.0f});
  renderer.draw_rect(x_, y_, width_, height_, 12.0f, overlay, border, 1.0f);

  // Title
  renderer.draw_text("Keyboard Shortcuts", x_ + width_ / 2 - 80, y_ + 30, "Arial", 16.0f, true,
                     flex::Color{1.0f, 1.0f, 1.0f, 1.0f});

  // Close hint
  renderer.draw_text("Press ? or Esc to close", x_ + width_ / 2 - 70, y_ + 50, "Arial", 10.0f,
                     false, flex::Color{0.6f, 0.6f, 0.6f, 1.0f});

  float row_y = y_ + 80;
  std::string current_category;

  for (const auto &s : shortcuts_) {
    // Category header
    if (s.category != current_category) {
      current_category = s.category;
      renderer.draw_text(current_category.c_str(), x_ + 20, row_y, "Arial", 12.0f, true,
                         flex::Color{0.5f, 0.7f, 0.9f, 1.0f});
      row_y += 22;
    }

    // Key
    flex::Paint key_bg = flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.25f, 1.0f});
    float key_width = std::max(30.0f, static_cast<float>(s.key.length()) * 8.0f + 12.0f);
    renderer.draw_rect(x_ + 30, row_y - 12, key_width, 18, 4.0f, key_bg, flex::Paint::none(), 0);
    renderer.draw_text(s.key.c_str(), x_ + 36, row_y, "Arial", 10.0f, true,
                       flex::Color{0.9f, 0.9f, 0.9f, 1.0f});

    // Description
    renderer.draw_text(s.description.c_str(), x_ + 40 + key_width + 10, row_y, "Arial", 11.0f,
                       false, flex::Color{0.8f, 0.8f, 0.8f, 1.0f});

    row_y += 20;

    if (row_y > y_ + height_ - 30) {
      break;
    }
  }
}

bool ShortcutPanel::handle_click(float px, float py) {
  // Click anywhere inside closes the panel
  set_visible(false);
  return true;
}

} // namespace meta_editor
