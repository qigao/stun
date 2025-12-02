#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <functional>

namespace flexui {

struct SwitchStyle {
    float trackWidth = 44;
    float trackHeight = 24;
    float thumbSize = 20;
    float borderRadius = 12;
    NVGcolor trackColorOff = nvgRGB(204, 204, 204);
    NVGcolor trackColorOn = nvgRGB(33, 150, 243);
    NVGcolor thumbColor = nvgRGB(255, 255, 255);
    float thumbShadowBlur = 4;
};

class Switch : public Widget {
public:
    using ChangeCallback = std::function<void(bool)>;

    Switch(NVGCSSRenderer* renderer, const std::string& id, bool initialState = false,
           const SwitchStyle& style = SwitchStyle());

    void draw(NVGcontext* vg) override;

    void setOn(bool on) { on_ = on; }
    bool isOn() const { return on_; }

    void setChangeCallback(ChangeCallback cb) { change_callback_ = cb; }
    void setSwitchStyle(const SwitchStyle& style) { style_ = style; }

protected:
    bool onClicked() override;

private:
    bool on_;
    SwitchStyle style_;
    ChangeCallback change_callback_;
};

} // namespace flexui
