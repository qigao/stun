#pragma once

#include <flexui/widget.h>
#include <nanovg.h>

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
    NVGcolor selectionColor = nvgRGBA(33, 150, 243, 100);
    float borderWidth = 2;
    float fontSize = 16;
    float padding = 12;
    bool passwordMode = false;
};

class TextBox : public Widget {
public:
    using ChangeCallback = std::function<void(const std::string&)>;

    TextBox(cssboxRenderer* renderer, const std::string& id,
            const std::string& placeholder = "", const TextBoxStyle& style = TextBoxStyle());

    void draw(NVGcontext* vg) override;

    void handleTextInput(const std::string& text);
    virtual void handleKeyPress(int key, bool shift, bool ctrl);
    void blur();

    void setInputText(const std::string& text);
    std::string getInputText() const { return text_; }

    void setPlaceholder(const std::string& placeholder) { placeholder_ = placeholder; }

    void setChangeCallback(ChangeCallback cb) { change_callback_ = cb; }
    void setTextBoxStyle(const TextBoxStyle& style) { style_ = style; }

    bool isFocused() const { return focused_; }
    void setFocused(bool focused);

    void setPasswordMode(bool password) { style_.passwordMode = password; }
    bool isPasswordMode() const { return style_.passwordMode; }

    void setScreen(class Screen* screen) { screen_ = screen; }
    
    // Mouse interaction
    bool handleMouseDown(float mx, float my) override;
    bool handleMouseMove(float mx, float my) override;
    bool handleMouseUp(float mx, float my) override;

private:
    // UTF-8 helper functions
    size_t charCount() const;
    size_t byteOffsetForChar(size_t charIndex) const;
    void insertTextAtCursor(const std::string& text);
    void deleteSelection();
    std::string getSelectedText() const;
    void moveCursor(int delta, bool select);
    void clearSelection();
    
    // Mouse helper
    size_t positionToCharIndex(float x) const;

    std::string text_;
    std::string placeholder_;
    TextBoxStyle style_;
    bool focused_ = false;
    size_t cursor_pos_ = 0;        // Character index, not byte index
    size_t selection_start_ = 0;   // Character index for selection
    bool has_selection_ = false;
    bool mouse_dragging_ = false;
    double last_click_time_ = 0.0;
    ChangeCallback change_callback_;
    class Screen* screen_ = nullptr;
};

} // namespace flexui
