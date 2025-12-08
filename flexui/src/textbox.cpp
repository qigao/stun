#include <flexui/textbox.h>
#include <flexui/screen.h>
#include <flexui/utf8.h>
#include <cssbox_internal.h>
#include <algorithm>

namespace flexui {

TextBox::TextBox(cssboxRenderer* renderer, const std::string& id,
                 const std::string& placeholder, const TextBoxStyle& style)
    : Widget(renderer, id, "input"), placeholder_(placeholder), style_(style) {

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
                
                // Set text input area for IME
                auto* el = element();
                SDL_Rect rect;
                rect.x = static_cast<int>(el->layout.x);
                rect.y = static_cast<int>(el->layout.y);
                rect.w = static_cast<int>(el->layout.width);
                rect.h = static_cast<int>(el->layout.height);
                SDL_SetTextInputArea(screen_->window(), &rect, 0);
                
                // Start text input
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
    clearSelection();
}

void TextBox::setInputText(const std::string& text) {
    text_ = text;
    cursor_pos_ = charCount();
    clearSelection();
}

// UTF-8 helper: get character count
size_t TextBox::charCount() const {
    return utf8len(reinterpret_cast<const utf8_int8_t*>(text_.c_str()));
}

// UTF-8 helper: convert character index to byte offset
size_t TextBox::byteOffsetForChar(size_t charIndex) const {
    if (charIndex == 0) return 0;
    const utf8_int8_t* str = reinterpret_cast<const utf8_int8_t*>(text_.c_str());
    size_t len = charCount();
    if (charIndex >= len) return text_.size();
    return utf8nsize_lazy(str, charIndex);
}

void TextBox::insertTextAtCursor(const std::string& text) {
    deleteSelection();
    size_t bytePos = byteOffsetForChar(cursor_pos_);
    text_.insert(bytePos, text);
    size_t insertedChars = utf8len(reinterpret_cast<const utf8_int8_t*>(text.c_str()));
    cursor_pos_ += insertedChars;
}

void TextBox::deleteSelection() {
    if (!has_selection_) return;
    
    size_t start = std::min(cursor_pos_, selection_start_);
    size_t end = std::max(cursor_pos_, selection_start_);
    
    size_t startByte = byteOffsetForChar(start);
    size_t endByte = byteOffsetForChar(end);
    
    text_.erase(startByte, endByte - startByte);
    cursor_pos_ = start;
    clearSelection();
}

std::string TextBox::getSelectedText() const {
    if (!has_selection_) return "";
    
    size_t start = std::min(cursor_pos_, selection_start_);
    size_t end = std::max(cursor_pos_, selection_start_);
    
    size_t startByte = byteOffsetForChar(start);
    size_t endByte = byteOffsetForChar(end);
    
    return text_.substr(startByte, endByte - startByte);
}

void TextBox::moveCursor(int delta, bool select) {
    if (!select && has_selection_ && delta != 0) {
        // Move to selection edge
        if (delta < 0) {
            cursor_pos_ = std::min(cursor_pos_, selection_start_);
        } else {
            cursor_pos_ = std::max(cursor_pos_, selection_start_);
        }
        clearSelection();
        return;
    }
    
    if (!select) {
        clearSelection();
    } else if (!has_selection_) {
        selection_start_ = cursor_pos_;
        has_selection_ = true;
    }
    
    int newPos = static_cast<int>(cursor_pos_) + delta;
    cursor_pos_ = std::clamp(newPos, 0, static_cast<int>(charCount()));
    
    if (select && cursor_pos_ == selection_start_) {
        clearSelection();
    }
}

void TextBox::clearSelection() {
    has_selection_ = false;
    selection_start_ = 0;
}

size_t TextBox::positionToCharIndex(float x) const {
    auto* el = element();
    float padding = cssPaddingLeft(style_.padding);
    float textX = el->layout.x + padding;
    
    if (x <= textX) return 0;
    
    // Get display text
    std::string displayText = text_;
    if (style_.passwordMode && !text_.empty()) {
        size_t count = charCount();
        displayText = std::string(count, '*');
    }
    
    // Find closest character position
    NVGcontext* vg = screen_->vg();
    nvgFontSize(vg, cssFontSize(style_.fontSize));
    nvgFontFace(vg, cssFontFamily());
    
    size_t len = charCount();
    float minDist = 1e10f;
    size_t bestIndex = 0;
    
    for (size_t i = 0; i <= len; i++) {
        size_t bytePos = byteOffsetForChar(i);
        std::string substr = displayText.substr(0, bytePos);
        
        float bounds[4];
        nvgTextBounds(vg, textX, 0, substr.c_str(), nullptr, bounds);
        float charX = (substr.empty() ? textX : bounds[2]);
        
        float dist = std::abs(charX - x);
        if (dist < minDist) {
            minDist = dist;
            bestIndex = i;
        }
    }
    
    return bestIndex;
}

bool TextBox::handleMouseDown(float mx, float my) {
    if (!focused_) return false;
    
    // Check for double-click (select all)
    double currentTime = SDL_GetTicks() / 1000.0;
    if (currentTime - last_click_time_ < 0.3) {
        // Double click - select all
        selection_start_ = 0;
        cursor_pos_ = charCount();
        has_selection_ = (cursor_pos_ > 0);
        mouse_dragging_ = false;
    } else {
        // Single click - set cursor position
        cursor_pos_ = positionToCharIndex(mx);
        clearSelection();
        mouse_dragging_ = true;
    }
    
    last_click_time_ = currentTime;
    return true;
}

bool TextBox::handleMouseMove(float mx, float my) {
    if (!focused_ || !mouse_dragging_) return false;
    
    size_t newPos = positionToCharIndex(mx);
    
    if (newPos != cursor_pos_) {
        if (!has_selection_) {
            selection_start_ = cursor_pos_;
            has_selection_ = true;
        }
        cursor_pos_ = newPos;
        
        if (cursor_pos_ == selection_start_) {
            clearSelection();
        }
    }
    
    return true;
}

bool TextBox::handleMouseUp(float mx, float my) {
    if (!focused_) return false;
    mouse_dragging_ = false;
    return true;
}

void TextBox::draw(NVGcontext* vg) {
    auto* el = element();
    float x = el->layout.x;
    float y = el->layout.y;
    float w = el->layout.width;
    float h = el->layout.height;

    NVGcolor bgColor = cssBackground(style_.bgColor);
    NVGcolor fallbackBorder = focused_ ? style_.borderColorFocus : style_.borderColor;
    NVGcolor borderColor = cssBorderColor(fallbackBorder);
    float borderWidth = cssBorderWidth(style_.borderWidth);
    float borderRadius = cssBorderRadius(style_.borderRadius);
    NVGcolor textColor = cssColor(style_.textColor);
    float fontSize = cssFontSize(style_.fontSize);
    float padding = cssPaddingLeft(style_.padding);

    // Background
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, borderRadius);
    nvgFillColor(vg, bgColor);
    nvgFill(vg);

    // Border
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, borderRadius);
    nvgStrokeColor(vg, borderColor);
    nvgStrokeWidth(vg, borderWidth);
    nvgStroke(vg);

    // Text or placeholder
    nvgFontSize(vg, fontSize);
    nvgFontFace(vg, cssFontFamily());
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    if (text_.empty() && !focused_) {
        nvgFillColor(vg, style_.placeholderColor);
        nvgText(vg, x + padding, y + h / 2, placeholder_.c_str(), nullptr);
    } else {
        // Display text or masked password
        std::string displayText = text_;
        if (style_.passwordMode && !text_.empty()) {
            size_t count = charCount();
            displayText = std::string(count, '*');
        }

        // Draw selection background
        if (focused_ && has_selection_) {
            size_t selStart = std::min(cursor_pos_, selection_start_);
            size_t selEnd = std::max(cursor_pos_, selection_start_);
            
            size_t startByte = byteOffsetForChar(selStart);
            size_t endByte = byteOffsetForChar(selEnd);
            
            std::string beforeSel = displayText.substr(0, startByte);
            std::string selection = displayText.substr(startByte, endByte - startByte);
            
            float bounds[4];
            nvgTextBounds(vg, x + padding, y + h / 2, beforeSel.c_str(), nullptr, bounds);
            float selStartX = bounds[2];
            
            nvgTextBounds(vg, x + padding, y + h / 2, (beforeSel + selection).c_str(), nullptr, bounds);
            float selEndX = bounds[2];
            
            nvgBeginPath(vg);
            nvgRect(vg, selStartX, y + h * 0.2f, selEndX - selStartX, h * 0.6f);
            nvgFillColor(vg, style_.selectionColor);
            nvgFill(vg);
        }

        // Draw text
        nvgFillColor(vg, textColor);
        nvgText(vg, x + padding, y + h / 2, displayText.c_str(), nullptr);

        // Draw cursor
        if (focused_) {
            size_t cursorByte = byteOffsetForChar(cursor_pos_);
            std::string beforeCursor = displayText.substr(0, cursorByte);
            
            float bounds[4];
            nvgTextBounds(vg, x + padding, y + h / 2, beforeCursor.c_str(), nullptr, bounds);
            float cursorX = (beforeCursor.empty() ? x + padding : bounds[2]);
            
            nvgBeginPath(vg);
            nvgMoveTo(vg, cursorX, y + h * 0.25f);
            nvgLineTo(vg, cursorX, y + h * 0.75f);
            nvgStrokeColor(vg, textColor);
            nvgStrokeWidth(vg, 1.5f);
            nvgStroke(vg);
        }
    }
}

void TextBox::handleTextInput(const std::string& text) {
    if (!focused_) return;
    
    insertTextAtCursor(text);
    
    if (change_callback_) {
        change_callback_(text_);
    }
}

void TextBox::handleKeyPress(int key, bool shift, bool ctrl) {
    if (!focused_) return;

    switch (key) {
        case SDLK_LEFT:
            moveCursor(-1, shift);
            break;
            
        case SDLK_RIGHT:
            moveCursor(1, shift);
            break;
            
        case SDLK_HOME:
            if (!shift) clearSelection();
            else if (!has_selection_) {
                selection_start_ = cursor_pos_;
                has_selection_ = true;
            }
            cursor_pos_ = 0;
            if (shift && cursor_pos_ == selection_start_) clearSelection();
            break;
            
        case SDLK_END:
            if (!shift) clearSelection();
            else if (!has_selection_) {
                selection_start_ = cursor_pos_;
                has_selection_ = true;
            }
            cursor_pos_ = charCount();
            if (shift && cursor_pos_ == selection_start_) clearSelection();
            break;
            
        case SDLK_BACKSPACE:
            if (has_selection_) {
                deleteSelection();
            } else if (cursor_pos_ > 0) {
                cursor_pos_--;
                size_t bytePos = byteOffsetForChar(cursor_pos_);
                size_t nextBytePos = byteOffsetForChar(cursor_pos_ + 1);
                text_.erase(bytePos, nextBytePos - bytePos);
            }
            if (change_callback_) change_callback_(text_);
            break;
            
        case SDLK_DELETE:
            if (has_selection_) {
                deleteSelection();
            } else if (cursor_pos_ < charCount()) {
                size_t bytePos = byteOffsetForChar(cursor_pos_);
                size_t nextBytePos = byteOffsetForChar(cursor_pos_ + 1);
                text_.erase(bytePos, nextBytePos - bytePos);
            }
            if (change_callback_) change_callback_(text_);
            break;
            
        case SDLK_A:
            if (ctrl) {
                selection_start_ = 0;
                cursor_pos_ = charCount();
                has_selection_ = (cursor_pos_ > 0);
            }
            break;
            
        case SDLK_C:
            if (ctrl && has_selection_) {
                std::string selected = getSelectedText();
                SDL_SetClipboardText(selected.c_str());
            }
            break;
            
        case SDLK_X:
            if (ctrl && has_selection_) {
                std::string selected = getSelectedText();
                SDL_SetClipboardText(selected.c_str());
                deleteSelection();
                if (change_callback_) change_callback_(text_);
            }
            break;
            
        case SDLK_V:
            if (ctrl) {
                char* clipboard = SDL_GetClipboardText();
                if (clipboard) {
                    insertTextAtCursor(clipboard);
                    SDL_free(clipboard);
                    if (change_callback_) change_callback_(text_);
                }
            }
            break;
    }
}

} // namespace flexui
