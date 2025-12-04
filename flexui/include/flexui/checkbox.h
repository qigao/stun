#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <functional>

namespace flexui {

struct CheckboxStyle {
    float borderRadius = 4;
    float borderWidth = 2;
    NVGcolor bgColorUnchecked = nvgRGB(255, 255, 255);
    NVGcolor bgColorChecked = nvgRGB(33, 150, 243);
    NVGcolor borderColorUnchecked = nvgRGB(204, 204, 204);
    NVGcolor borderColorChecked = nvgRGB(33, 150, 243);
    NVGcolor checkmarkColor = nvgRGB(255, 255, 255);
    float checkmarkWidth = 2;
};

class Checkbox : public Widget {
public:
    using ChangeCallback = std::function<void(bool)>;

    Checkbox(cssboxRenderer* renderer, const std::string& id, bool initialState = false,
             const CheckboxStyle& style = CheckboxStyle());

    void draw(NVGcontext* vg) override;

    void setChecked(bool checked) { checked_ = checked; }
    bool isChecked() const { return checked_; }

    void setChangeCallback(ChangeCallback cb) { change_callback_ = cb; }
    void setCheckboxStyle(const CheckboxStyle& style) { style_ = style; }

protected:
    bool onClicked() override;

private:
    bool checked_;
    CheckboxStyle style_;
    ChangeCallback change_callback_;
};

} // namespace flexui
