#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>

namespace flexui {

enum class AlertType {
    Info,
    Success,
    Warning,
    Error
};

struct AlertStyle {
    float padding = 12;
    float borderRadius = 4;
    float borderWidth = 1;
    NVGcolor bgColor = nvgRGB(229, 246, 253);
    NVGcolor textColor = nvgRGB(1, 67, 97);
    NVGcolor borderColor = nvgRGB(144, 202, 249);
    float fontSize = 14;
};

class Alert : public Widget {
public:
    Alert(cssboxRenderer* renderer, const std::string& id, const std::string& message,
          AlertType type = AlertType::Info);

    void draw(NVGcontext* vg) override;
    void setMessage(const std::string& message) { message_ = message; }
    void setType(AlertType type);
    const std::string& getMessage() const { return message_; }

private:
    std::string message_;
    AlertType type_;
    AlertStyle style_;

    void updateStyleForType();
};

} // namespace flexui
