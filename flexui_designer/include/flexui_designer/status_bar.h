/*
 * flexUI Designer - Status Bar
 */

#pragma once

#include <flex/runtime/renderer.h>
#include <string>

namespace flexui_designer {

class Designer;

class StatusBar {
public:
    static constexpr float HEIGHT = 24.0f;

    void set_designer(Designer* d) { designer_ = d; }
    void set_position(float x, float y) { x_ = x; y_ = y; }
    void set_size(float w, float h) { width_ = w; height_ = h; }

    void render(flex::Renderer& renderer);

    void set_message(const std::string& msg) { message_ = msg; message_time_ = 3.0f; }
    void update(float dt) { if (message_time_ > 0) message_time_ -= dt; }

private:
    Designer* designer_ = nullptr;
    float x_ = 0, y_ = 0, width_ = 0, height_ = HEIGHT;
    std::string message_;
    float message_time_ = 0;
};

} // namespace flexui_designer
