/*
 * flexUI Designer - Script Editor Implementation
 */

#include "flexui_designer/script_editor.h"
#include <algorithm>

namespace flexui_designer {

ScriptEditor::ScriptEditor() {
    width_ = 300;
    height_ = 400;
}

void ScriptEditor::set_widget(DesignWidget* widget) {
    // Apply any pending edit before switching
    if (editing_ && widget_ != widget) {
        apply_edit();
    }
    widget_ = widget;
    editing_ = false;
    edit_buffer_.clear();
    cursor_pos_ = 0;
    scroll_offset_ = 0;
}

void ScriptEditor::set_handler(HandlerType type, const std::string& code) {
    if (!widget_) return;
    
    std::string* handler = get_handler_ptr(type);
    if (handler) {
        *handler = code;
        if (on_change_) on_change_();
    }
}

std::string ScriptEditor::get_handler(HandlerType type) const {
    const std::string* handler = get_handler_ptr(type);
    return handler ? *handler : "";
}

std::string* ScriptEditor::get_handler_ptr(HandlerType type) {
    if (!widget_) return nullptr;
    
    switch (type) {
        case HandlerType::OnClick: return &widget_->on_click;
        case HandlerType::OnChange: return &widget_->on_change;
        case HandlerType::OnFocus: return &widget_->on_focus;
        case HandlerType::OnBlur: return &widget_->on_blur;
    }
    return nullptr;
}

const std::string* ScriptEditor::get_handler_ptr(HandlerType type) const {
    if (!widget_) return nullptr;
    
    switch (type) {
        case HandlerType::OnClick: return &widget_->on_click;
        case HandlerType::OnChange: return &widget_->on_change;
        case HandlerType::OnFocus: return &widget_->on_focus;
        case HandlerType::OnBlur: return &widget_->on_blur;
    }
    return nullptr;
}

bool ScriptEditor::is_handler_supported(HandlerType type) const {
    if (!widget_) return false;
    
    switch (type) {
        case HandlerType::OnClick:
            // Button supports on_click
            return widget_->type == WidgetType::Button;
            
        case HandlerType::OnChange:
            // Input, Slider, Checkbox, Switch, Dropdown support on_change
            return widget_->type == WidgetType::Input ||
                   widget_->type == WidgetType::Slider ||
                   widget_->type == WidgetType::Checkbox ||
                   widget_->type == WidgetType::Switch ||
                   widget_->type == WidgetType::Dropdown;
            
        case HandlerType::OnFocus:
        case HandlerType::OnBlur:
            // Only Input supports focus/blur
            return widget_->type == WidgetType::Input;
    }
    return false;
}

void ScriptEditor::render(flex::Renderer& renderer) {
    if (!visible_) return;
    
    render_background(renderer);
    
    flex::Color white{1, 1, 1, 1};
    flex::Color hint{0.5f, 0.5f, 0.5f, 1};
    
    // Header
    renderer.draw_text("Script Editor", x_ + PADDING, y_ + 22, "sans", 14, true, white);
    
    if (!widget_) {
        renderer.draw_text("Select a widget", x_ + PADDING, y_ + 60, "sans", 12, false, hint);
        return;
    }
    
    // Show widget info
    char info[64];
    snprintf(info, sizeof(info), "Widget: %s", widget_->id.c_str());
    renderer.draw_text(info, x_ + PADDING, y_ + 42, "sans", 11, false, hint);
    
    float section_y = y_ + 55 - scroll_offset_;
    
    // Render handler sections based on widget type
    if (is_handler_supported(HandlerType::OnClick)) {
        render_handler_section(renderer, section_y, "on_click", 
                               HandlerType::OnClick, widget_->on_click);
    }
    
    if (is_handler_supported(HandlerType::OnChange)) {
        render_handler_section(renderer, section_y, "on_change",
                               HandlerType::OnChange, widget_->on_change);
    }
    
    if (is_handler_supported(HandlerType::OnFocus)) {
        render_handler_section(renderer, section_y, "on_focus",
                               HandlerType::OnFocus, widget_->on_focus);
    }
    
    if (is_handler_supported(HandlerType::OnBlur)) {
        render_handler_section(renderer, section_y, "on_blur",
                               HandlerType::OnBlur, widget_->on_blur);
    }
    
    // If no handlers supported, show message
    if (!is_handler_supported(HandlerType::OnClick) &&
        !is_handler_supported(HandlerType::OnChange) &&
        !is_handler_supported(HandlerType::OnFocus) &&
        !is_handler_supported(HandlerType::OnBlur)) {
        renderer.draw_text("No event handlers for this widget type", 
                          x_ + PADDING, y_ + 80, "sans", 11, false, hint);
    }
}

void ScriptEditor::render_handler_section(flex::Renderer& renderer, float& y,
                                           const char* label, HandlerType type,
                                           const std::string& code) {
    // Skip if section is outside visible area
    if (y > y_ + height_ || y + SECTION_HEIGHT < y_ + 40) {
        y += SECTION_HEIGHT + 10;
        return;
    }
    
    bool is_editing = editing_ && editing_handler_ == type;
    
    // Handler label
    flex::Color label_color{0.7f, 0.85f, 1.0f, 1.0f};
    renderer.draw_text(label, x_ + PADDING, y + 16, "sans", 11, true, label_color);
    
    // Code edit area background
    float area_y = y + HEADER_HEIGHT;
    float area_h = SECTION_HEIGHT - HEADER_HEIGHT - 5;
    float area_w = width_ - 2 * PADDING;
    
    flex::Color bg = is_editing ? 
        flex::Color{0.15f, 0.15f, 0.18f, 1.0f} : 
        flex::Color{0.12f, 0.12f, 0.14f, 1.0f};
    flex::Color border = is_editing ?
        flex::Color{0.4f, 0.6f, 1.0f, 1.0f} :
        flex::Color{0.25f, 0.25f, 0.28f, 1.0f};
    
    renderer.draw_rect(x_ + PADDING, area_y, area_w, area_h, 4,
                      flex::Paint::solid(bg), flex::Paint::solid(border), 1.0f);
    
    // Code text
    const std::string& display_text = is_editing ? edit_buffer_ : code;
    flex::Color code_color{0.9f, 0.9f, 0.9f, 1.0f};
    
    if (display_text.empty()) {
        flex::Color placeholder{0.4f, 0.4f, 0.45f, 1.0f};
        renderer.draw_text("// Enter handler code...", 
                          x_ + PADDING + 6, area_y + 18, "mono", 11, false, placeholder);
    } else {
        // Simple single-line display for now
        // Truncate if too long
        std::string display = display_text;
        if (display.length() > 35) {
            display = display.substr(0, 32) + "...";
        }
        renderer.draw_text(display.c_str(), 
                          x_ + PADDING + 6, area_y + 18, "mono", 11, false, code_color);
    }
    
    // Cursor when editing
    if (is_editing) {
        float cursor_x = x_ + PADDING + 6 + cursor_pos_ * 7.0f;
        if (cursor_x < x_ + width_ - PADDING) {
            renderer.draw_rect(cursor_x, area_y + 6, 1, 16, 0,
                              flex::Paint::solid(flex::Color{1, 1, 1, 0.8f}),
                              flex::Paint::none(), 0);
        }
    }
    
    y += SECTION_HEIGHT + 10;
}

bool ScriptEditor::handle_click(float screen_x, float screen_y) {
    if (!visible_ || !widget_) return false;
    if (!contains(screen_x, screen_y)) return false;
    
    float local_y = screen_y - y_ + scroll_offset_;
    
    // Check which handler section was clicked
    float section_y = 55;
    
    auto check_section = [&](HandlerType type) -> bool {
        if (!is_handler_supported(type)) return false;
        
        float area_y = section_y + HEADER_HEIGHT;
        float area_h = SECTION_HEIGHT - HEADER_HEIGHT - 5;
        
        if (local_y >= area_y && local_y < area_y + area_h) {
            // Apply previous edit if switching handlers
            if (editing_ && editing_handler_ != type) {
                apply_edit();
            }
            
            editing_ = true;
            editing_handler_ = type;
            
            // Load current handler value into edit buffer
            const std::string* handler = get_handler_ptr(type);
            edit_buffer_ = handler ? *handler : "";
            cursor_pos_ = (int)edit_buffer_.length();
            
            return true;
        }
        
        section_y += SECTION_HEIGHT + 10;
        return false;
    };
    
    if (check_section(HandlerType::OnClick)) return true;
    if (check_section(HandlerType::OnChange)) return true;
    if (check_section(HandlerType::OnFocus)) return true;
    if (check_section(HandlerType::OnBlur)) return true;
    
    // Clicked outside edit areas - apply and stop editing
    if (editing_) {
        apply_edit();
        editing_ = false;
    }
    
    return true;
}

bool ScriptEditor::handle_text_input(const char* text) {
    if (!editing_ || !widget_) return false;
    
    // Insert text at cursor position
    edit_buffer_.insert(cursor_pos_, text);
    cursor_pos_ += (int)strlen(text);
    
    return true;
}

bool ScriptEditor::handle_key(int key) {
    if (!editing_ || !widget_) return false;
    
    // Ctrl+S - save changes
    if (key == 19) {  // Ctrl+S
        apply_edit();
        return true;
    }
    
    // Enter - apply edit (single line mode)
    if (key == '\r' || key == '\n') {
        apply_edit();
        editing_ = false;
        return true;
    }
    
    // Escape - cancel edit
    if (key == 0x1B) {
        editing_ = false;
        edit_buffer_.clear();
        return true;
    }
    
    // Backspace
    if (key == '\b' || key == 0x08) {
        if (cursor_pos_ > 0 && !edit_buffer_.empty()) {
            edit_buffer_.erase(cursor_pos_ - 1, 1);
            cursor_pos_--;
        }
        return true;
    }
    
    // Delete
    if (key == 0x7F || key == 0x2E) {
        if (cursor_pos_ < (int)edit_buffer_.length()) {
            edit_buffer_.erase(cursor_pos_, 1);
        }
        return true;
    }
    
    // Left arrow
    if (key == 0x25) {
        if (cursor_pos_ > 0) cursor_pos_--;
        return true;
    }
    
    // Right arrow
    if (key == 0x27) {
        if (cursor_pos_ < (int)edit_buffer_.length()) cursor_pos_++;
        return true;
    }
    
    // Home
    if (key == 0x24) {
        cursor_pos_ = 0;
        return true;
    }
    
    // End
    if (key == 0x23) {
        cursor_pos_ = (int)edit_buffer_.length();
        return true;
    }
    
    return false;
}

bool ScriptEditor::handle_scroll(float delta) {
    if (!visible_) return false;
    
    scroll_offset_ = std::max(0.0f, scroll_offset_ - delta * 20);
    return true;
}

void ScriptEditor::apply_edit() {
    if (!editing_ || !widget_) return;
    
    std::string* handler = get_handler_ptr(editing_handler_);
    if (handler && *handler != edit_buffer_) {
        *handler = edit_buffer_;
        if (on_change_) on_change_();
    }
}

} // namespace flexui_designer
