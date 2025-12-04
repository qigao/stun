#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>
#include <functional>

namespace flexui {

struct ChipStyle {
    float padding = 12;
    float borderRadius = 16;
    NVGcolor bgColor = nvgRGB(224, 224, 224);
    NVGcolor textColor = nvgRGB(51, 51, 51);
    float fontSize = 13;
    bool closeable = false;
};

class Chip : public Widget {
public:
    using CloseCallback = std::function<void()>;

    Chip(cssboxRenderer* renderer, const std::string& id, const std::string& text,
         const ChipStyle& style = ChipStyle());

    void draw(NVGcontext* vg) override;
    void setChipText(const std::string& text) { text_ = text; }
    const std::string& getChipText() const { return text_; }
    void setCloseCallback(CloseCallback cb) { closeCallback_ = cb; }
    void setChipStyle(const ChipStyle& style) { style_ = style; }
    void setCloseable(bool closeable) { style_.closeable = closeable; }
    bool isCloseable() const { return style_.closeable; }

protected:
    bool onClicked() override;

private:
    std::string text_;
    ChipStyle style_;
    CloseCallback closeCallback_;
};

} // namespace flexui
