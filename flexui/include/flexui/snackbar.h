#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>
#include <functional>

namespace flexui {

struct SnackbarStyle {
    NVGcolor backgroundColor = nvgRGBA(50, 50, 50, 230);
    NVGcolor textColor = nvgRGBA(255, 255, 255, 255);
    NVGcolor actionColor = nvgRGBA(100, 200, 255, 255);
    float fontSize = 14.0f;
    float padding = 16.0f;
    float borderRadius = 4.0f;
    float minWidth = 300.0f;
    float maxWidth = 600.0f;
};

class Snackbar : public Widget {
public:
    Snackbar(NVGCSSRenderer* renderer, const std::string& id,
             const std::string& message, const SnackbarStyle& style = SnackbarStyle());

    void draw(NVGcontext* vg) override;
    bool handleMouseDown(float mx, float my) override;

    void setMessage(const std::string& msg) { message_ = msg; }
    void setAction(const std::string& text, std::function<void()> callback) {
        action_text_ = text;
        action_callback_ = callback;
    }
    void show(float duration = 3.0f);
    void hide() { visible_ = false; }
    bool isVisible() const { return visible_; }
    void update(float dt);

private:
    std::string message_;
    std::string action_text_;
    std::function<void()> action_callback_;
    SnackbarStyle style_;
    bool visible_ = false;
    float duration_ = 3.0f;
    float elapsed_ = 0.0f;
};

} // namespace flexui
