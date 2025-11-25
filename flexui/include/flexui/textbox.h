#pragma once

#include <flexui/widget.h>
#include <nanovg.h>
#include <SDL3/SDL.h>
#include <functional>
#include <string>

namespace flexui {

struct TextBoxStyle {
    float borderRadius = 4;
    NVGcolor bgColor = nvgRGB(255, 255, 255);
    NVGcolor borderColor = nvgRGB(204, 204, 204);
    NVGcolor borderColorFocus = nvgRGB(33, 150, 243);
    NVGcolor textColor = nvgRGB(51, 51, 51);
    NVGcolor placeholderColor = nvgRGB(158, 158, 158);
    float borderWidth = 2;
    float fontSize = 16;
    float padding = 12;
    bool passwordMode = false;
};

class TextBox : public Widget {
public:
    using ChangeCallback = std::function<void(const std::string&)>;

    TextBox(NVGCSSRenderer* renderer, const std::string& id,
            const std::string& placeholder = "", const TextBoxStyle& style = TextBoxStyle());

    void draw(NVGcontext* vg) override;

    void handleTextInput(const std::string& text);
    void handleKeyPress(int key);
    void blur();

    void setInputText(const std::string& text) { text_ = text; }
    std::string getInputText() const { return text_; }

    void setPlaceholder(const std::string& placeholder) { placeholder_ = placeholder; }

    void setChangeCallback(ChangeCallback cb) { change_callback_ = cb; }
    void setTextBoxStyle(const TextBoxStyle& style) { style_ = style; }

    bool isFocused() const { return focused_; }
    void setFocused(bool focused);

    void setPasswordMode(bool password) { style_.passwordMode = password; }
    bool isPasswordMode() const { return style_.passwordMode; }

    void setScreen(class Screen* screen) { screen_ = screen; }

private:
    std::string text_;
    std::string placeholder_;
    TextBoxStyle style_;
    bool focused_ = false;
    ChangeCallback change_callback_;
    class Screen* screen_ = nullptr;
};

} // namespace flexui
