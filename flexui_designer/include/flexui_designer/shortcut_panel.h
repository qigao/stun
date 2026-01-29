/*
 * flexUI Designer - Shortcut Help Panel
 */

#pragma once

#include <flex/runtime/renderer.h>

namespace flexui_designer {

class ShortcutPanel {
public:
    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    void toggle() { visible_ = !visible_; }
    bool visible() const { return visible_; }

    void set_position(float x, float y, float w, float h) {
        x_ = x; y_ = y; width_ = w; height_ = h;
    }

    void render(flex::Renderer& renderer);
    bool handle_click(float x, float y);

private:
    bool visible_ = false;
    float x_ = 0, y_ = 0, width_ = 400, height_ = 500;
};

} // namespace flexui_designer
