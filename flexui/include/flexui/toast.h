#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>
#include <chrono>

namespace flexui {

enum class ToastType {
    Info,
    Success,
    Warning,
    Error
};

struct ToastStyle {
    NVGcolor infoColor = nvgRGBA(33, 150, 243, 230);
    NVGcolor successColor = nvgRGBA(76, 175, 80, 230);
    NVGcolor warningColor = nvgRGBA(255, 152, 0, 230);
    NVGcolor errorColor = nvgRGBA(244, 67, 54, 230);
    NVGcolor textColor = nvgRGBA(255, 255, 255, 255);
    float fontSize = 14.0f;
    float padding = 12.0f;
    float borderRadius = 4.0f;
};

class Toast : public Widget {
public:
    using DismissCallback = std::function<void()>;

    Toast(cssboxRenderer* renderer, const std::string& id, const std::string& message,
          ToastType type = ToastType::Info, const ToastStyle& style = ToastStyle());

    void draw(NVGcontext* vg) override;
    void show(float duration = 2.0f);
    void hide();
    bool isVisible() const { return visible_; }
    void updateTimer();
    void setMessage(const std::string& message) { message_ = message; }
    const std::string& getMessage() const { return message_; }
    void setType(ToastType type);
    void setDismissCallback(DismissCallback cb) { dismiss_callback_ = cb; }

private:
    std::string message_;
    ToastType type_;
    ToastStyle style_;
    bool visible_ = false;
    float duration_ = 2.0f;
    std::chrono::steady_clock::time_point show_time_;
    DismissCallback dismiss_callback_;

    void updateStyleForType();
};

} // namespace flexui
