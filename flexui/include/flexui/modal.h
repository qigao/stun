#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <string>
#include <functional>

namespace flexui {

struct ModalStyle {
    NVGcolor overlayColor = nvgRGBA(0, 0, 0, 128);
    NVGcolor bgColor = nvgRGB(255, 255, 255);
    NVGcolor titleBg = nvgRGB(245, 245, 245);
    NVGcolor textColor = nvgRGB(51, 51, 51);
    float borderRadius = 8;
    float titleHeight = 50;
    float fontSize = 14;
};

class Modal : public Widget {
public:
    Modal(cssboxRenderer* renderer, const std::string& id,
          const std::string& title, const std::string& content,
          const ModalStyle& style = ModalStyle());

    void draw(NVGcontext* vg) override;
    bool handleMouseDown(float mx, float my) override;

    void show() { visible_ = true; }
    void hide() { visible_ = false; }
    bool isVisible() const { return visible_; }

    void setTitle(const std::string& title) { title_ = title; }
    void setContent(const std::string& content) { content_ = content; }
    void setCloseCallback(std::function<void()> callback) { close_callback_ = callback; }

private:
    std::string title_;
    std::string content_;
    bool visible_ = false;
    ModalStyle style_;
    std::function<void()> close_callback_;
};

} // namespace flexui
