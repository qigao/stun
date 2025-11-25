#include <flexui/textbox.h>
#include <flexui/screen.h>
#include <flexui/utf8.h>
#include <nanovg_css_internal.h>

namespace flexui {

TextBox::TextBox(NVGCSSRenderer* renderer, const std::string& id,
                 const std::string& placeholder, const TextBoxStyle& style)
    : Widget(renderer, id, "input"), placeholder_(placeholder), style_(style) {

    // Set up click handler to focus
    setClickCallback([this](Widget*) {
        setFocused(true);
        return true;
    });
}

void TextBox::setFocused(bool focused) {
    if (focused_ != focused) {
        focused_ = focused;
        if (screen_) {
            if (focused) {
                screen_->setFocusedTextBox(this);
                SDL_StartTextInput(screen_->window());
            } else {
                screen_->setFocusedTextBox(nullptr);
                SDL_StopTextInput(screen_->window());
            }
        }
    }
}

void TextBox::blur() {
    setFocused(false);
}

void TextBox::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->computed.x;
    float y = el->computed.y;
    float w = el->computed.width;
    float h = el->computed.height;

    NVGcolor borderColor = focused_ ? style_.borderColorFocus : style_.borderColor;

    // Background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, style_.borderRadius);
    nvgFillColor(vg, style_.bgColor);
    nvgFill(vg);

    // Border
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, style_.borderRadius);
    nvgStrokeColor(vg, borderColor);
    nvgStrokeWidth(vg, style_.borderWidth);
    nvgStroke(vg);

    // Text or placeholder
    nvgFontSize(vg, style_.fontSize);
    nvgFontFace(vg, "sans-serif");
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    if (text_.empty() && !focused_) {
        nvgFillColor(vg, style_.placeholderColor);
        nvgText(vg, x + style_.padding, y + h / 2, placeholder_.c_str(), nullptr);
    } else {
        nvgFillColor(vg, style_.textColor);

        // Display text or masked password
        const char* displayText = text_.c_str();
        std::string maskedText;

        if (style_.passwordMode && !text_.empty()) {
            // Count UTF-8 characters
            size_t charCount = utf8len(reinterpret_cast<const utf8_int8_t*>(text_.c_str()));
            // Create masked string with asterisks
            maskedText = std::string(charCount, '*');
            displayText = maskedText.c_str();
        }

        nvgText(vg, x + style_.padding, y + h / 2, displayText, nullptr);

        // Cursor if focused
        if (focused_) {
            float bounds[4];
            nvgTextBounds(vg, x + style_.padding, y + h / 2, displayText, nullptr, bounds);
            float cursorX = bounds[2];
            nvgBeginPath(vg);
            nvgMoveTo(vg, cursorX + 2, y + h * 0.25f);
            nvgLineTo(vg, cursorX + 2, y + h * 0.75f);
            nvgStrokeColor(vg, style_.textColor);
            nvgStrokeWidth(vg, 1);
            nvgStroke(vg);
        }
    }
}

void TextBox::handleTextInput(const std::string& text) {
    if (focused_) {
        text_ += text;
        if (change_callback_) {
            change_callback_(text_);
        }
    }
}

void TextBox::handleKeyPress(int key) {
    if (!focused_) return;

    if (key == SDLK_BACKSPACE && !text_.empty()) {
        // Use UTF-8 to delete the last complete character
        // Find the start of the last UTF-8 character by walking backwards
        size_t pos = text_.size() - 1;

        // Skip continuation bytes (0b10xxxxxx)
        while (pos > 0 && (text_[pos] & 0xC0) == 0x80) {
            pos--;
        }

        // Now pos points to the start of the last character
        text_.erase(pos);

        if (change_callback_) {
            change_callback_(text_);
        }
    }
}

} // namespace flexui
