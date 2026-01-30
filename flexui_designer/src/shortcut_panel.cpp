/*
 * flexUI Designer - Shortcut Panel Implementation
 */

#include "flexui_designer/shortcut_panel.h"

namespace flexui_designer {

struct ShortcutEntry {
    const char* key;
    const char* desc;
};

static const ShortcutEntry shortcuts[] = {
    // File
    {"Ctrl+N", "New Project"},
    {"Ctrl+T", "New Tab"},
    {"Ctrl+W", "Close Tab"},
    {"Ctrl+O", "Open Project"},
    {"Ctrl+Shift+S", "Save Project"},
    {"Ctrl+E", "Export C++"},
    // Edit
    {"Ctrl+Z", "Undo"},
    {"Ctrl+Y", "Redo"},
    {"Ctrl+C", "Copy"},
    {"Ctrl+V", "Paste"},
    {"Ctrl+D", "Duplicate"},
    {"Ctrl+A", "Select All"},
    {"Del", "Delete"},
    {"Ctrl+Shift+C", "Copy Style"},
    {"Ctrl+Shift+V", "Paste Style"},
    // View
    {"Ctrl++", "Zoom In"},
    {"Ctrl+-", "Zoom Out"},
    {"Ctrl+0", "Zoom 100%"},
    {"Ctrl+1", "Zoom to Fit"},
    {"G", "Toggle Grid"},
    {"S", "Toggle Snap"},
    {"R", "Toggle Rulers"},
    {"T", "Toggle Widget Tree"},
    {"P", "Preview Mode"},
    // Arrange
    {"Ctrl+L", "Lock Widget"},
    {"Ctrl+Shift+L", "Unlock Widget"},
    {"Ctrl+G", "Group"},
    {"Ctrl+Shift+G", "Ungroup"},
    // Navigation & Edit
    {"Tab", "Next Widget"},
    {"Shift+Tab", "Previous Widget"},
    {"F2/Enter", "Edit Text"},
    {"Double-click", "Edit Text"},
    {"Arrows", "Move Widget"},
    {"Ctrl+Arrows", "Move 1px"},
    {"Middle Drag", "Pan Canvas"},
    {"Scroll", "Zoom"},
    // Other
    {"?/F1", "Show Shortcuts"},
    {"Esc", "Close/Cancel"},
};

void ShortcutPanel::render(flex::Renderer& renderer) {
    if (!visible_) return;

    renderer.save();
    renderer.translate(x_, y_);

    float pw = 340, ph = 520;
    float cx = (width_ - pw) / 2;
    float cy = (height_ - ph) / 2;

    // Backdrop
    renderer.draw_rect(0, 0, width_, height_, 0,
        flex::Paint::solid(flex::Color{0, 0, 0, 0.5f}), flex::Paint::none(), 0);

    // Panel
    flex::Paint bg = flex::Paint::solid(flex::Color{0.14f, 0.14f, 0.16f, 0.98f});
    flex::Paint border = flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.35f, 1});
    renderer.draw_rect(cx, cy, pw, ph, 8, bg, border, 1);

    // Title
    renderer.draw_text("Keyboard Shortcuts", cx + 20, cy + 22, "sans", 16, true, {1, 1, 1, 1});
    renderer.draw_rect(cx + 20, cy + 45, pw - 40, 1, 0,
        flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.35f, 1}), flex::Paint::none(), 0);

    // Shortcuts list
    float ly = cy + 65;
    constexpr size_t count = sizeof(shortcuts) / sizeof(shortcuts[0]);
    for (size_t i = 0; i < count && ly < cy + ph - 30; ++i) {
        renderer.draw_text(shortcuts[i].key, cx + 30, ly + 2, "sans", 11, false, {0.5f, 0.7f, 1.0f, 1});
        renderer.draw_text(shortcuts[i].desc, cx + 130, ly + 2, "sans", 11, false, {0.8f, 0.8f, 0.8f, 1});
        ly += 20;
    }

    // Close hint
    renderer.draw_text("Press Esc or click to close", cx + pw/2 - 80, cy + ph - 20, "sans", 10, false, {0.5f, 0.5f, 0.55f, 1});

    renderer.restore();
}

bool ShortcutPanel::handle_click(float x, float y) {
    if (!visible_) return false;
    visible_ = false;
    return true;
}

} // namespace flexui_designer
