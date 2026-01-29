/*
 * Meta Editor - Keyboard Shortcut Overlay Implementation
 */

#include "meta_editor/view/shortcut_overlay.h"
#include <cmath>
#include <cstring>

namespace meta_editor {

ShortcutOverlay::ShortcutOverlay() {}

void ShortcutOverlay::render(flex::Renderer& renderer, float viewport_width, float viewport_height) {
    if (!visible_) return;

    // Semi-transparent overlay background
    flex::Paint overlay_bg = flex::Paint::solid(flex::Color{0, 0, 0, 0.6f});
    renderer.draw_rect(0, 0, viewport_width, viewport_height, 0, overlay_bg, flex::Paint::none(), 0);

    // Panel position (centered)
    float x = (viewport_width - panel_width_) / 2;
    float y = (viewport_height - panel_height_) / 2;

    // Panel background with rounded corners
    flex::Paint panel_bg = flex::Paint::solid(flex::Color{0.15f, 0.15f, 0.15f, 0.98f});
    flex::Paint panel_border = flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.3f, 1});
    renderer.draw_rect(x, y, panel_width_, panel_height_, 12, panel_bg, panel_border, 1);

    // Title
    float ty = y + 30;
    renderer.draw_text("Keyboard Shortcuts", x + panel_width_ / 2 - 80, ty - 12, "Arial", 18, true,
                      flex::Color{1, 1, 1, 1});

    // Close hint
    renderer.draw_text("Press ? or Esc to close", x + panel_width_ / 2 - 75, ty + 10, "Arial", 12, false,
                      flex::Color{0.6f, 0.6f, 0.6f, 1});

    // Divider
    ty += 30;
    std::string divider_path = "M " + std::to_string(x + 20) + " " + std::to_string(ty) +
                               " L " + std::to_string(x + panel_width_ - 20) + " " + std::to_string(ty);
    renderer.stroke_path(divider_path, flex::Paint::solid(flex::Color{0.3f, 0.3f, 0.3f, 1}), 1);

    // Shortcuts data
    struct ShortcutItem {
        const char* key;
        const char* desc;
    };

    struct ShortcutGroup {
        const char* title;
        ShortcutItem items[8];
        int count;
    };

    ShortcutGroup groups[] = {
        {
            "Tools",
            {
                {"V", "Select tool"},
                {"P", "Pen tool"},
                {"R", "Rectangle"},
                {"O", "Circle"},
                {"E", "Ellipse"},
                {"S", "Star"},
                {"T", "Text"},
            },
            7
        },
        {
            "Selection",
            {
                {"Ctrl+A", "Select all"},
                {"Del", "Delete"},
                {"Esc", "Deselect"},
                {"Ctrl+D", "Duplicate"},
                {"Ctrl+C", "Copy"},
                {"Ctrl+V", "Paste"},
            },
            6
        },
        {
            "Grouping",
            {
                {"Ctrl+G", "Group"},
                {"Ctrl+Shift+G", "Ungroup"},
            },
            2
        },
        {
            "File",
            {
                {"Ctrl+S", "Save project"},
                {"Ctrl+O", "Open project"},
                {"Ctrl+Shift+S", "Export SVG"},
                {"Ctrl+Shift+C", "Copy as SVG"},
            },
            4
        },
        {
            "View",
            {
                {"G", "Toggle grid"},
                {"Ctrl+Z", "Undo"},
                {"Ctrl+Y", "Redo"},
            },
            3
        }
    };

    flex::Color title_color{0.4f, 0.7f, 1.0f, 1};
    flex::Color key_color{1, 0.9f, 0.6f, 1};
    flex::Color desc_color{0.85f, 0.85f, 0.85f, 1};
    flex::Paint key_bg = flex::Paint::solid(flex::Color{0.25f, 0.25f, 0.25f, 1});

    float col_x = x + 20;
    float col_width = (panel_width_ - 40) / 2;
    ty += 20;
    float start_y = ty;

    int group_idx = 0;
    for (int gi = 0; gi < 5; gi++) {
        const auto& group = groups[gi];

        if (group_idx == 3) {
            col_x = x + 20 + col_width;
            ty = start_y;
        }

        // Group title
        renderer.draw_text(group.title, col_x, ty - 12, "Arial", 14, true, title_color);
        ty += 22;

        // Shortcuts in group
        for (int i = 0; i < group.count; i++) {
            // Key background
            float key_width = 12.0f + static_cast<float>(strlen(group.items[i].key)) * 7.0f;
            renderer.draw_rect(col_x, ty - 12, key_width, 18, 4, key_bg, flex::Paint::none(), 0);

            // Key text
            renderer.draw_text(group.items[i].key, col_x + 6, ty - 12, "Consolas", 11, false, key_color);

            // Description
            renderer.draw_text(group.items[i].desc, col_x + key_width + 10, ty - 12, "Arial", 12, false, desc_color);

            ty += 22;
        }

        ty += 12;
        group_idx++;
    }
}

bool ShortcutOverlay::handle_click(float x, float y) {
    if (!visible_) return false;
    // Click anywhere to close
    visible_ = false;
    return true;
}

} // namespace meta_editor
