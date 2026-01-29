/*
 * flexUI Designer - Script Editor
 *
 * Panel for editing widget event handler code.
 * Provides text editing areas for on_click, on_change, on_focus, on_blur handlers.
 */

#pragma once

#include "designer.h"
#include <meta_editor/view/panel.h>
#include <functional>
#include <string>

namespace flexui_designer {

// Handler types supported by ScriptEditor
enum class HandlerType {
    OnClick,
    OnChange,
    OnFocus,
    OnBlur
};

class ScriptEditor : public meta_editor::Panel {
public:
    ScriptEditor();

    void render(flex::Renderer& renderer) override;
    bool handle_click(float x, float y) override;
    bool handle_text_input(const char* text);
    bool handle_key(int key);
    bool handle_scroll(float delta);

    // Set the widget to edit
    void set_widget(DesignWidget* widget);
    DesignWidget* widget() const { return widget_; }

    // Set handler value directly (for testing)
    void set_handler(HandlerType type, const std::string& code);
    std::string get_handler(HandlerType type) const;

    // Change callback - called when handler code is modified
    using ChangeCallback = std::function<void()>;
    void set_change_callback(ChangeCallback cb) { on_change_ = std::move(cb); }

    // Check if a handler type is supported by current widget
    bool is_handler_supported(HandlerType type) const;

    // Get currently editing handler
    HandlerType editing_handler() const { return editing_handler_; }
    bool is_editing() const { return editing_; }

private:
    // Render a single handler section
    void render_handler_section(flex::Renderer& renderer, float& y,
                                 const char* label, HandlerType type,
                                 const std::string& code);

    // Apply current edit buffer to widget
    void apply_edit();

    // Get handler string reference from widget
    std::string* get_handler_ptr(HandlerType type);
    const std::string* get_handler_ptr(HandlerType type) const;

    DesignWidget* widget_ = nullptr;
    HandlerType editing_handler_ = HandlerType::OnClick;
    bool editing_ = false;
    std::string edit_buffer_;
    int cursor_pos_ = 0;
    float scroll_offset_ = 0;
    ChangeCallback on_change_;

    static constexpr float SECTION_HEIGHT = 80.0f;
    static constexpr float HEADER_HEIGHT = 24.0f;
    static constexpr float PADDING = 10.0f;
};

} // namespace flexui_designer
