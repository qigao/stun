/*
 * Meta Editor - Keyboard Shortcut Overlay
 *
 * Shows all available keyboard shortcuts when '?' is pressed.
 */

#pragma once

#include <flex.h>

namespace meta_editor {

class ShortcutOverlay {
public:
    ShortcutOverlay();

    void toggle() { visible_ = !visible_; }
    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    bool visible() const { return visible_; }

    void render(flex::Renderer& renderer, float viewport_width, float viewport_height);

    bool handle_click(float x, float y);

private:
    bool visible_ = false;
    float panel_width_ = 400;
    float panel_height_ = 500;
};

} // namespace meta_editor
